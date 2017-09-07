#define lnetlib_cpp

#include <glua/lprefix.h>


#include <stdlib.h>
#include <string.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <glua/lua.h>

#include <glua/lauxlib.h>
#include <glua/lualib.h>
#include <glua/thinkyoung_lua_api.h>
#include <glua/lnetlib.h>

using thinkyoung::lua::api::global_glua_chain_api;


namespace glua
{
	namespace net
	{
		Uri Uri::parse(const std::wstring &uri)
		{
			Uri result;

			typedef std::wstring::const_iterator iterator_t;

			if (uri.length() == 0)
				return result;

			auto uriEnd = uri.end();

			// get query start
			auto queryStart = std::find(uri.begin(), uriEnd, L'?');

			// protocol
			auto protocolStart = uri.begin();
			auto protocolEnd = std::find(protocolStart, uriEnd, L':');            //"://");

			if (protocolEnd != uriEnd)
			{
				std::wstring prot = &*(protocolEnd);
				if ((prot.length() > 3) && (prot.substr(0, 3) == L"://"))
				{
					result.protocol = std::wstring(protocolStart, protocolEnd);
					protocolEnd += 3;   //      ://
				}
				else
					protocolEnd = uri.begin();  // no protocol
			}
			else
				protocolEnd = uri.begin();  // no protocol
			auto hostStart = protocolEnd;
			auto pathStart = std::find(hostStart, uriEnd, L'/');  // get pathStart

			auto hostEnd = std::find(protocolEnd,
				(pathStart != uriEnd) ? pathStart : queryStart,
				L':');  // check for port

			result.host = std::wstring(hostStart, hostEnd);

			if ((hostEnd != uriEnd) && ((&*(hostEnd))[0] == L':'))  // we have a port
			{
				hostEnd++;
				auto portEnd = (pathStart != uriEnd) ? pathStart : queryStart;
				result.port = std::wstring(hostEnd, portEnd);
			}
			if (pathStart != uriEnd)
				result.path = std::wstring(pathStart, queryStart);
			if (queryStart != uriEnd)
				result.query_string = std::wstring(queryStart, uri.end());

			return result;
		}
	}
}


typedef boost::asio::io_service IoService;
typedef boost::asio::ip::tcp TCP;

struct NetServerInfo
{
	IoService* io_service;
	std::shared_ptr<TCP::acceptor> acceptor;
};

static IoService* io_service;

static std::mutex io_service_mutex;

static IoService* get_singleton_io_service()
{
	if (io_service)
		return io_service;
	io_service_mutex.lock();
	if (!io_service)
		io_service = new IoService();
	io_service_mutex.unlock();
	return io_service;
}

// let server = net.listen(host, port)
int lualib_net_listen(lua_State *L)
{
	if(lua_gettop(L)<2 || !lua_isstring(L, 1) || !lua_isnumber(L, 2))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.listen need arguments (host: string, port: integer)");
		return 0;
	}
	auto host = luaL_checkstring(L, 1);
	auto port = luaL_checkinteger(L, 2);
	auto *server = new NetServerInfo();
	server->io_service = get_singleton_io_service();
	boost::asio::ip::tcp::resolver resolver(*server->io_service);
	boost::asio::ip::tcp::resolver::query query(std::string(host), std::to_string(port));
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
	server->acceptor = std::make_shared<TCP::acceptor>(*server->io_service, endpoint);
	lua_pushlightuserdata(L, server);
	return 1;
}

int lualib_net_shutdown(lua_State *L)
{
	io_service_mutex.lock();
	if(io_service)
	{
		io_service->stop();
		delete io_service;
		io_service = nullptr;
	}
	io_service_mutex.unlock();
	return 0;
}

int lualib_net_connect_impl(lua_State *L, std::string host, int port)
{
	boost::system::error_code ignored_error;
	auto *io_service = get_singleton_io_service();
	auto *socket = new TCP::socket(*io_service);
	boost::asio::ip::tcp::resolver resolver(*io_service);
	boost::asio::ip::tcp::resolver::query query(std::string(host), std::to_string(port));
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
	socket->connect(endpoint, ignored_error);
	lua_pushlightuserdata(L, socket);
	return 1;
}

int lualib_net_connect(lua_State *L)
{
	if (lua_gettop(L)<2 || !lua_isstring(L, 1) || !lua_isnumber(L, 2))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.connect need arguments (host: string, port: integer)");
		return 0;
	}
	auto host = luaL_checkstring(L, 1);
	auto port = luaL_checkinteger(L, 2);
	return lualib_net_connect_impl(L, host, static_cast<int>(port));
}

int lualib_net_accept(lua_State *L)
{
	if(lua_gettop(L) < 1 || !lua_islightuserdata(L, 1))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.accept need arguments (server: TcpSocketServer)");
		return 0;
	}
	NetServerInfo *server = (NetServerInfo*) lua_touserdata(L, 1);
	auto *socket = new TCP::socket(*server->io_service);
	server->acceptor->accept(*socket);
	lua_pushlightuserdata(L, socket);
	return 1;
}

static void process_async_connection(lua_State *L, NetServerInfo *server, TcpSocket *socket)
{
	// FIXME: 这里用全局变量暂存的方式，对于闭包处理得不好，如果callback函数里使用了非全局变量的upval，可能会出问题
	lua_getglobal(L, "net_async_handler");
	if(!lua_isfunction(L, -1))
	{
		lua_pop(L, 1);
		return;
	}
	lua_pushlightuserdata(L, socket);
	lua_pcall(L, 1, 0, 0);
	lua_pop(L, 2);
}

static void handle_accept_async(lua_State *L, NetServerInfo *server, TcpSocket *socket,
	const boost::system::error_code& error)
{
	process_async_connection(L, server, socket);
	//if (!_running)
	//{
	//	_loop_running = false;
	//	return;
	///}
	auto new_socket = new TCP::socket(*server->io_service);
	boost::system::error_code ignored_error;
	server->acceptor->async_accept(*new_socket,
		std::bind(&handle_accept_async, L, server, new_socket,
			ignored_error));
}

int lualib_net_accept_async_impl(lua_State *L, void *server)
{
	auto *net_server = (NetServerInfo*)server;
	auto new_socket = new TCP::socket(*net_server->io_service);
	boost::system::error_code ignored_error;
	net_server->acceptor->async_accept(*new_socket,
		std::bind(&handle_accept_async, L, net_server, new_socket,
			ignored_error));
	return 0;
}

int lualib_net_accept_async(lua_State *L)
{
	if (lua_gettop(L) < 2 || !lua_islightuserdata(L, 1) || !lua_isfunction(L, 2))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
			"net.accept_async need arguments (server: TcpSocketServer, handler: Function)");
		return 0;
	}
	NetServerInfo *server = (NetServerInfo*) lua_touserdata(L, 1);
	lua_pushvalue(L, 2);
	lua_setglobal(L, "net_async_handler");
	return lualib_net_accept_async_impl(L, (void*) server);
}

int lualib_net_start_io_loop(lua_State *L)
{
	if (lua_gettop(L) < 1 || !lua_islightuserdata(L, 1))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
			"net.start_io_loop need arguments (server: TcpSocketServer)");
		return 0;
	}
	NetServerInfo *server = (NetServerInfo*)lua_touserdata(L, 1);
	return lualib_net_start_io_loop_impl(L, (void *)server);
}

int lualib_net_start_io_loop_impl(lua_State *L, void *server)
{
	auto *net_server = (NetServerInfo *)server;
	net_server->io_service->run();
	return 0;
}

int lualib_net_write_impl(lua_State *L, TcpSocket *socket, std::vector<char>& buf)
{
	boost::system::error_code ignored_error;
	boost::asio::write(*socket,
		boost::asio::buffer(buf),
		boost::asio::transfer_all(),
		ignored_error);
	lua_pushstring(L, ignored_error.message().c_str());
	return 1;
}

int lualib_net_write_string_impl(lua_State *L, TcpSocket *socket, std::string message)
{
	std::vector<char> buf;
	for(size_t i=0;i<message.length();++i)
	{
		char c = message[i];
		if(c!='\0')
			buf.push_back(message[i]);
	}
	return lualib_net_write_impl(L, socket, buf);
}

int lualib_net_write(lua_State *L)
{
	if (lua_gettop(L)<2)
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.write need arguments (socket: TcpSocket, data: string)");
		return 0;
	}
	TcpSocket *socket = (TcpSocket*)lua_touserdata(L, 1);
	size_t buf_len = 0;
	auto buf = lua_tolstring(L, 2, &buf_len);
	std::vector<char> buffer;
	for(size_t i=0;i<buf_len;++i)
	{
		buffer.push_back(buf[i]);
	}
	return lualib_net_write_impl(L, socket, buffer);
}

int lualib_net_read_impl(lua_State *L, TcpSocket *socket, size_t count)
{
	boost::system::error_code ignored_error;
	std::vector<char> buf(count);
	auto len = boost::asio::read(*socket, boost::asio::buffer(buf), boost::asio::transfer_exactly(count), ignored_error);
	lua_pushlstring(L, buf.data(), len);
	lua_pushinteger(L, len);
	return 2;
}

int lualib_net_read(lua_State *L)
{
	if (lua_gettop(L)<2)
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.read need arguments (socket: TcpSocket, count: integer)");
		return 0;
	}
	TcpSocket *socket = (TcpSocket*)lua_touserdata(L, 1);
	auto count = luaL_checkinteger(L, 2);
	return lualib_net_read_impl(L, socket, count);
}

int lualib_net_read_until_string_impl(lua_State *L, TcpSocket *socket, const char *end)
{
	std::vector<char> end_vector;
	for (size_t i = 0; i < strlen(end); ++i)
		end_vector.push_back(end[i]);
	return lualib_net_read_until_impl(L, socket, end_vector);
}

int lualib_net_read_until_impl(lua_State *L, TcpSocket *socket, std::vector<char> &end)
{
	if (end.size() < 1)
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.read_until second argument can't be empty string");
		return 0;
	}
	boost::system::error_code ignored_error;
	std::vector<char> all_buf;
	while (true)
	{
		std::vector<char> buf(1);
		auto len = boost::asio::read(*socket,
			boost::asio::buffer(buf), boost::asio::transfer_exactly(1), ignored_error);
		if(len<1)
		{
			break;
		}
		else
		{
			for(size_t i=0;i<len;++i)
			{
				all_buf.push_back(buf[i]);
			}
			if(all_buf.size()>=end.size())
			{
				bool ended = true;
				for(size_t i=0;i<end.size();++i)
				{
					if(end[i] != all_buf[all_buf.size()+i-end.size()])
					{
						ended = false;
						break;
					}
				}
				if (ended)
					break;
			}
		}
	}
	lua_pushlstring(L, all_buf.data(), all_buf.size());
	lua_pushinteger(L, all_buf.size());
	return 2;
}

int lualib_net_read_until(lua_State *L)
{
	if (lua_gettop(L) < 2 || !lua_isstring(L, 2))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.read_until need arguments (socket: TcpSocket, end: string)");
		return 0;
	}
	TCP::socket *socket = (TCP::socket*)lua_touserdata(L, 1);
	auto end = luaL_checkstring(L, 2);
	return lualib_net_read_until_string_impl(L, socket, end);
}

int lualib_net_close_server(lua_State *L)
{
	if (lua_gettop(L)<1)
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.close_server need arguments (server: TcpSocketServer)");
		return 0;
	}
	NetServerInfo *server = (NetServerInfo*)lua_touserdata(L, 1);
	delete server;
	return 0;
}

int lualib_net_close_socket_impl(lua_State *L, TcpSocket *socket)
{
	socket->close();
	delete socket;
	return 0;
}

int lualib_net_close_socket(lua_State *L)
{
	if(lua_gettop(L)<1)
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "net.close_socket need arguments (socket: TcpSocket)");
		return 0;
	}
	TcpSocket *socket = (TcpSocket*)lua_touserdata(L, 1);
	return lualib_net_close_socket_impl(L, socket);
}

// TODO: async APIs

static const luaL_Reg netlib[] = {
	{ "listen", lualib_net_listen },
	{ "connect", lualib_net_connect },
	{ "accept", lualib_net_accept },
	{ "accept_async", lualib_net_accept_async },
	{ "start_io_loop", lualib_net_start_io_loop },
	{ "read", lualib_net_read },
	{ "read_until", lualib_net_read_until },
	{ "write", lualib_net_write },
	{ "close_socket", lualib_net_close_socket },
	{ "close_server", lualib_net_close_server },
	{ "shutdown", lualib_net_shutdown },
	{ nullptr, nullptr }
};


LUAMOD_API int luaopen_net(lua_State *L) {
	luaL_newlib(L, netlib);
	return 1;
}
