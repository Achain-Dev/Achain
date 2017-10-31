#ifndef lnetlib_h
#define lnetlib_h

#include "glua/lprefix.h"

#include <stdlib.h>
#include <string.h>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "glua/lua.h"

#include "glua/lauxlib.h"
#include "glua/lualib.h"
#include "glua/thinkyoung_lua_api.h"

typedef boost::asio::ip::tcp::socket TcpSocket;

namespace glua {
	namespace net {
		struct Uri
		{
		public:
			std::wstring query_string, path, protocol, host, port;

			static Uri parse(const std::wstring &uri);

			inline std::string host_str() const
			{
				return std::string(host.begin(), host.end());
			}
			inline int port_int() const
			{
				std::string s(port.begin(), port.end());
				if (s.length() < 1)
					return 80;
				return std::stoi(s);
			}
			inline std::string path_str() const
			{
				return std::string(path.begin(), path.end());
			}
			inline std::string query_string_str() const
			{
				return std::string(query_string.begin(), query_string.end());
			}
		};  // uri

	} // end namespace glua::net
}

int lualib_net_listen(lua_State *L);

int lualib_net_shutdown(lua_State *L);

// client connect server
int lualib_net_connect_impl(lua_State *L, std::string host, int port);

int lualib_net_connect(lua_State *L);

// ctx = net.accept(server), server accept client connection
int lualib_net_accept(lua_State *L);

int lualib_net_start_io_loop_impl(lua_State *L, void *server);

// server start async io loop
int lualib_net_start_io_loop(lua_State *L);

int lualib_net_accept_async_impl(lua_State *L, void *server);

// net.accept_async(server, handlerFunction), server accept client connection async
int lualib_net_accept_async(lua_State *L);

int lualib_net_write_string_impl(lua_State *L, TcpSocket *socket, std::string message);

int lualib_net_write_impl(lua_State *L, TcpSocket *socket, std::vector<char> &buf);

int lualib_net_write(lua_State *L);

int lualib_net_read_impl(lua_State *L, TcpSocket *socket, size_t count);

int lualib_net_read(lua_State *L);

int lualib_net_read_until_string_impl(lua_State *L, TcpSocket *socket, const char *end);

int lualib_net_read_until_impl(lua_State *L, TcpSocket *socket, std::vector<char> &end);

int lualib_net_read_until(lua_State *L);

int lualib_net_close_server(lua_State *L);

int lualib_net_close_socket_impl(lua_State *L, TcpSocket *socket);

int lualib_net_close_socket(lua_State *L);



#endif