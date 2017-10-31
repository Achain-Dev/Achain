// TODO: async apis

#include "glua/lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/algorithm/string.hpp>

#include "glua/lua.h"

#include "glua/lauxlib.h"
#include "glua/lualib.h"
#include "glua/thinkyoung_lua_api.h"
#include "glua/thinkyoung_lua_lib.h"
#include "glua/lnetlib.h"
#include "glua/lapi.h"
#include <boost/bind/bind.hpp>

using thinkyoung::lua::api::global_glua_chain_api;

#undef LUA_HTTP_SERVERNAME
#define LUA_HTTP_SERVERNAME "glua_http_server";

typedef boost::asio::io_service IoService;
typedef boost::asio::ip::tcp TCP;


struct HttpRequest
{
	std::unordered_map<std::string, std::string> headers;
	std::vector<char> body;
	std::string body_string;
	std::string http_method;
	std::string path;
	std::string http_protocol;
	inline std::string get_header(std::string key)
	{
		auto found = headers.find(key);
		if (found != headers.end())
			return found->second;
		else
			return "";
	}
	inline int get_int_header(std::string key)
	{
		auto value = get_header(key);
		if (value.length() < 1)
			return 0;
		return std::stoi(value);
	}
};

struct HttpResponse
{
	int status_code;
	std::string http_version;
	std::string status_message;
	std::unordered_map<std::string, std::string> headers;
	std::vector<char> body;
	inline HttpResponse()
	{
		headers["Content-Length"] = "0";
		status_code = 200;
		status_message = "OK";
		http_version = "HTTP/1.1";
	}
	inline std::string get_header(std::string key)
	{
		auto found = headers.find(key);
		if (found != headers.end())
			return found->second;
		else
			return "";
	}
	inline int get_int_header(std::string key)
	{
		auto value = get_header(key);
		if (value.length() < 1)
			return 0;
		return std::stoi(value);
	}
};

struct HttpContext
{
	TCP::socket *socket;
	HttpRequest *req;
	HttpResponse *res;
	inline HttpContext()
	{
		req = nullptr;
		res = nullptr;
	}
	~HttpContext()
	{
		if (req)
			delete req;
		if (res)
			delete res;
	}
};

static int lualib_http_listen(lua_State *L)
{
	return lualib_net_listen(L);
}

static int lualib_http_connect(lua_State *L)
{
	return lualib_net_connect(L);
}

static void easy_tolower(std::string &data)
{
	std::transform(data.begin(), data.end(), data.begin(), ::tolower);
}

static void easy_toupper(std::string &data)
{
	std::transform(data.begin(), data.end(), data.begin(), ::toupper);
}

static bool response_headers_traverser(lua_State *L, void *ud, size_t len, std::list<const void *> &jsons, size_t recur_depth)
{
	std::stringstream *ss = (std::stringstream*)ud;
	if (lua_gettop(L) < 2)
		return false;
	if (!lua_isstring(L, -2) && !lua_isinteger(L, -1))
		return false;
	std::string key;
	if (lua_isstring(L, -2))
		key = std::string(lua_tostring(L, -2));
	else if (lua_isinteger(L, -2))
		key = std::to_string(lua_tointeger(L, -2));
	else
		return false;
	std::string value(lua_tostring(L, -1));
	*ss << key << ":" << value << "\r\n";
	return true;
}

// 发起http请求, http.request(method, url, body, headers)
static int lualib_http_request(lua_State *L)
{
	auto method = luaL_checkstring(L, 1);
	auto url = luaL_checkstring(L, 2);
	std::string method_str(method);
	easy_toupper(method_str);
	size_t body_len = 0;
	auto body = luaL_tolstring(L, 3, &body_len);
	/*
	auto headers_table_value = lua_type_to_storage_value_type(L, 4);
	if(headers_table_value.type != GluaStorageValueType::LVALUE_TABLE)
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "headers must be table");
		return 0;
	}
	*/
	// auto headers_table = headers_table_value.value.table_value;
	std::string url_str(url);
	std::wstring url_wstr(url_str.begin(), url_str.end());
	auto uri = glua::net::Uri::parse(url_wstr);
	int result_count = lualib_net_connect_impl(L, uri.host_str(), uri.port_int());
	auto *socket = (TcpSocket*)lua_touserdata(L, -1);
	lua_pop(L, result_count);
	std::vector<char> buf;
	std::stringstream ss;
	ss << method_str << " " << uri.path_str();
	if (uri.query_string.length() > 0)
		ss << "?" << uri.query_string_str();
	ss << " HTTP/1.1\r\n";
	/*
	for(const auto &p : *headers_table)
	{
		if (p.second.type == GluaStorageValueType::LVALUE_STRING)
			ss << p.first << ":" << p.second.value.string_value << "\r\n";
		else if(p.second.type == GluaStorageValueType::LVALUE_INTEGER)
			ss << p.first << ":" << p.second.value.int_value << "\r\n";
		else if (p.second.type == GluaStorageValueType::LVALUE_NUMBER)
			ss << p.first << ":" << p.second.value.number_value << "\r\n";
		else if (p.second.type == GluaStorageValueType::LVALUE_BOOLEAN)
			ss << p.first << ":" << p.second.value.bool_value << "\r\n";
	}
	*/
	std::list<const void*> ignored_nested;
	ss << "Host:" << uri.host_str() << "\r\n";
	ss << "Connection:keep-alive" << "\r\n";
	luaL_traverse_table_with_nested(L, 4, response_headers_traverser, &ss, ignored_nested, 0);
	ss << "\r\n";
	std::string data_str = ss.str();
	for (size_t i = 0; i < data_str.length(); ++i)
		buf.push_back(data_str[i]);
	for (size_t i = 0; i < body_len; ++i)
		buf.push_back(body[i]);
	result_count = lualib_net_write_impl(L, socket, buf);
	if (result_count > 0)
		lua_pop(L, result_count);
	// read response
	auto *ctx = new HttpContext();
	ctx->socket = socket;
	ctx->res = new HttpResponse();
	size_t body_len_from_header = 0; // 从header中读取到的body长度
	do
	{
		result_count = lualib_net_read_until_string_impl(L, socket, "\r\n");
		if (result_count < 2)
			break;
		auto len = lua_tointeger(L, -1);
		size_t buf_len = 0;
		auto buf = lua_tolstring(L, -2, &buf_len);
		if (buf_len <= 2)
			break; // end read headers
		lua_pop(L, result_count);
		std::string header_name;
		std::string header_value;
		bool is_name = true;
		for (auto i = 0; i<len; ++i)
		{
			char c = buf[i];
			if (c == ':')
			{
				is_name = false;
				continue;
			}
			if (is_name)
				header_name += c;
			else
				header_value += c;
		}

		if (ctx->res->status_message.length()<1)
		{
			std::vector<std::string> splited;
			boost::algorithm::split(splited, header_name, boost::is_any_of(" "));
			if (splited.size() > 0)
			{
				std::string http_version(splited[0]);
				easy_toupper(http_version);
				ctx->res->http_version = http_version;
			}
			if (splited.size() > 1)
				ctx->res->status_code = std::stoi(splited[1]);
			if (splited.size() > 2)
			{
				std::string status_message(splited[2]);
				boost::algorithm::trim(status_message);
				ctx->res->status_message = status_message;
			}
			continue;
		}
		boost::algorithm::trim(header_name);
		boost::algorithm::trim(header_value);
		ctx->res->headers[header_name] = header_value;
		if (header_name == "Content-Length")
			body_len_from_header = std::stoi(header_value);
	} while (true);
	if (body_len_from_header>0)
	{
		int result_len = lualib_net_read_impl(L, socket, body_len_from_header);
		size_t buf_len = 0;
		auto len = lua_tointeger(L, -1);
		auto buf = lua_tolstring(L, -2, &buf_len);
		for (auto i = 0; i<len; ++i)
		{
			char c = buf[i];
			ctx->res->body.push_back(c);
		}
		lua_pop(L, result_len);
	}
	else if(ctx->res->get_header("Transfer-Encoding") == "chunked")
	{
		// read chunked body
		std::stringstream ss;
		while(true)
		{
			result_count = lualib_net_read_until_string_impl(L, socket, "\r\n");
			size_t lenstrlen = 0;
			auto lenstr = lua_tolstring(L, -2, &lenstrlen);
			lua_pop(L, result_count);
			int chunk_size = 0;
			int pos_mul = 1;
			if (lenstrlen < 1)
				break;
			for(size_t i= lenstrlen - 3;i>=0;--i)
			{
				chunk_size += (lenstr[i]-'0') * pos_mul;
				pos_mul *= 16;
			}
			if (chunk_size <= 0)
				break;
			result_count = lualib_net_read_impl(L, socket, chunk_size);
			size_t chunk_len = 0;
			auto chunkchars = lua_tolstring(L, -2, &chunk_len);
			lua_pop(L, result_count);
			for(size_t i=0;i<chunk_size;++i)
			{
				ctx->res->body.push_back(chunkchars[i]);
				ss << chunkchars[i];
			}
			result_count = lualib_net_read_impl(L, socket, 2);
			lua_pop(L, result_count);
		}
		result_count = lualib_net_read_impl(L, socket, 2);
		lua_pop(L, result_count);
	}
	lua_pushlightuserdata(L, ctx);
	return 1;
}

static int lualib_http_close(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, -1);
	delete ctx;
	return 0;
}

static void process_http_request_async_read_data(lua_State *L, TcpSocket *socket, const boost::system::error_code& error)
{
	// TODO	
}

// http.on_request_data(socket)
static int lualib_http_on_request_data(lua_State *L)
{
	// TODO
	if (lua_gettop(L) < 2 || !lua_isuserdata(L, 1) && !lua_islightuserdata(L, 2))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "http.on_request_data need arguments (socket: TcpSocket, handler: Function)");
		return 0;
	}
	auto *socket = (TcpSocket*) lua_touserdata(L, 1);
	lua_pushvalue(L, 2);
	lua_setglobal(L, "http_on_request_data");
	std::vector<char> buf(4096); // TODO: 把buffer放入TcpSocket或者HttpRequest里
	boost::system::error_code ignored_error;
	boost::asio::async_read(*socket, boost::asio::buffer(buf),
		boost::bind(&process_http_request_async_read_data, L, socket, ignored_error));
	return 0;
}

// http.on_request_end(socket)
static int lualib_http_on_request_end(lua_State *L)
{
	// TODO
	auto *socket = (TcpSocket*) lua_touserdata(L, -1);
	return 0;
}

static int lualib_http_accept(lua_State *L)
{
	lualib_net_accept(L);
	auto *socket = (TcpSocket*) lua_touserdata(L, -1);
	auto *ctx = new HttpContext();
	ctx->socket = socket;
	ctx->req = new HttpRequest();
	ctx->res = new HttpResponse();
	size_t body_len_from_header = 0; // 从header中读取到的body长度
	do
	{
		int result_count = lualib_net_read_until_string_impl(L, socket, "\r\n");
		if (result_count < 2)
			break;
		auto len = lua_tointeger(L, -1);
		size_t buf_len = 0;
		auto buf = lua_tolstring(L, -2, &buf_len);
		if (buf_len <= 2)
			break; // end read headers
		lua_pop(L, result_count);
		std::string header_name;
		std::string header_value;
		bool is_name = true;
		for(lua_Integer i=0;i<len;++i)
		{
			char c = buf[i];
			if(c == ':')
			{
				is_name = false;
				continue;
			}
			if (is_name)
				header_name += c;
			else
				header_value += c;
		}

		if (ctx->req->http_method.length()<1)
		{
			std::vector<std::string> splited;
			boost::algorithm::split(splited, header_name, boost::is_any_of(" "));
			if (splited.size() > 0)
			{
				std::string http_method(splited[0]);
				easy_toupper(http_method);
				ctx->req->http_method = http_method;
			}
			else
				ctx->req->http_method = "GET";
			if (splited.size() > 1)
				ctx->req->path = splited[1];
			if (splited.size() > 2)
			{
				std::string http_protocol(splited[2]);
				boost::algorithm::trim(http_protocol);
				ctx->req->http_protocol = http_protocol;
			}
			continue;
		}
		boost::algorithm::trim(header_name);
		// easy_tolower(header_name);
		boost::algorithm::trim(header_value);
		ctx->req->headers[header_name] = header_value;
		if (header_name == "Content-Length")
			body_len_from_header = std::stoi(header_value);
	} while (true);
	// TODO: 处理stream的情况
	if(body_len_from_header)
	{
		int result_len = lualib_net_read_impl(L, socket, body_len_from_header);
		size_t buf_len = 0;
		auto len = lua_tointeger(L, -1);
		auto buf = lua_tolstring(L, -2, &buf_len);
		for(auto i=0;i<len;++i)
		{
			char c = buf[i];
			ctx->req->body.push_back(c);
			ctx->req->body_string += c;
		}
		lua_pop(L, result_len);
	}
	lua_pushlightuserdata(L, ctx);
	return 1;
}

static int lualib_http_accept_async(lua_State *L)
{
	if (lua_gettop(L) < 2 || !lua_islightuserdata(L, 1) || !lua_isfunction(L, 2))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
			"http.accept_async need arguments (server: HttpServer, handler: Function)");
		return 0;
	}
	auto server = lua_touserdata(L, 1);
	lua_pushvalue(L, 2);
	lua_setglobal(L, "net_async_handler");
	return lualib_net_accept_async_impl(L, server);
}

static int lualib_http_start_io_loop(lua_State *L)
{
	if (lua_gettop(L) < 1 || !lua_islightuserdata(L, 1))
	{
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
			"http.start_io_loop need arguments (server: HttpServer)");
		return 0;
	}
	auto server = lua_touserdata(L, 1);
	return lualib_net_start_io_loop_impl(L, server);
}

static int lualib_http_get_req_header(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	auto key = luaL_checkstring(L, 2);
	auto value = ctx->req->get_header(key);
	lua_pushstring(L, value.c_str());
	return 1;
}

static int lualib_http_get_req_body(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	auto body = ctx->req->body;
	auto *buf = new char[body.size()];
	for(size_t i=0;i<body.size();++i)
	{
		buf[i] = body.at(i);
	}
	lua_pushlstring(L, buf, body.size());
	delete[] buf;
	return 1;
}

static int lualib_http_get_req_http_method(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	lua_pushstring(L, ctx->req->http_method.c_str());
	return 1;
}

static int lualib_http_get_req_path(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	lua_pushstring(L, ctx->req->path.c_str());
	return 1;
}

static int lualib_http_get_req_http_protocol(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	lua_pushstring(L, ctx->req->http_protocol.c_str());
	return 1;
}

static int lualib_http_set_res_header(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	auto key = luaL_checkstring(L, 2);
	auto value = luaL_checkstring(L, 3);
	if(strlen(key)>0)
		ctx->res->headers[key] = value;
	return 0;
}

static int lualib_http_get_res_header(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	auto key = luaL_checkstring(L, 2);
	auto value = ctx->res->get_header(key);
	lua_pushstring(L, value.c_str());
	return 1;
}

static int lualib_http_write_res_body(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	size_t buf_len = 0;
	auto buf = lua_tolstring(L, 2, &buf_len);
	for(size_t i=0;i<buf_len;++i)
	{
		ctx->res->body.push_back(buf[i]);
	}
	ctx->res->headers["Content-Length"] = std::to_string(std::stoi(ctx->res->headers["Content-Length"]) + buf_len);
	return 0;
}

static int lualib_http_get_res_body(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	char *buf = new char[ctx->res->body.size() + 1];
	memset(buf, 0x0, sizeof(char) * (ctx->res->body.size() + 1));
	for (size_t i = 0; i < ctx->res->body.size(); ++i)
		buf[i] = ctx->res->body[i];
	lua_pushlstring(L, buf, ctx->res->body.size());
	return 1;
}

static int lualib_http_set_status(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	auto code = luaL_checkinteger(L, 2);
	std::string status_message("OK");
	if (lua_gettop(L) >= 3)
		status_message = luaL_checkstring(L, 3);
	ctx->res->status_code = static_cast<int>(code);
	ctx->res->status_message = status_message;
	return 0;
}

static int lualib_http_get_status(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	lua_pushinteger(L, ctx->res->status_code);
	return 1;
}

static int lualib_http_get_status_message(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	lua_pushstring(L, ctx->res->status_message.c_str());
	return 1;
}

// send response and finish this request and response
static int lualib_http_finish_res(lua_State *L)
{
	auto *ctx = (HttpContext*)lua_touserdata(L, 1);
	auto *socket = ctx->socket;
	lualib_net_write_string_impl(L, socket, ctx->req->http_protocol+" "+ std::to_string(ctx->res->status_code) + " " + ctx->res->status_message + "\r\n");
	for(const auto & header : ctx->res->headers)
	{
		lualib_net_write_string_impl(L, socket, header.first + ":" + header.second + "\r\n");
	}
	lualib_net_write_string_impl(L, socket, "\r\n");
	// TODO: 如果是Transfer-Encoding是chunked，则用chunked的方式返回
	lualib_net_write_impl(L, socket, ctx->res->body);
	lualib_net_close_socket_impl(L, socket);
	return 0;
}


static const luaL_Reg httplib[] = {
	{ "listen", lualib_http_listen },
	{ "connect", lualib_http_connect },
	{ "request", lualib_http_request },
	{ "close", lualib_http_close },
	{ "accept", lualib_http_accept },
	{ "accept_async", lualib_http_accept_async },
	{ "start_io_loop", lualib_http_start_io_loop },
	{ "get_req_header", lualib_http_get_req_header },
	{ "get_res_header", lualib_http_get_res_header },
	{ "get_req_http_method", lualib_http_get_req_http_method },
	{ "get_req_path", lualib_http_get_req_path },
	{ "get_req_http_protocol", lualib_http_get_req_http_protocol },
	{ "get_req_body", lualib_http_get_req_body },
	{ "on_request_data", lualib_http_on_request_data },
	{ "on_request_end", lualib_http_on_request_end },
	{ "set_res_header", lualib_http_set_res_header },
	{ "write_res_body", lualib_http_write_res_body },
	{ "set_status", lualib_http_set_status },
	{ "get_status", lualib_http_get_status },
	{ "get_status_message", lualib_http_get_status_message },
	{ "get_res_body", lualib_http_get_res_body },
	{ "finish_res", lualib_http_finish_res },
	{ nullptr, nullptr }
};


LUAMOD_API int luaopen_http(lua_State *L) {
	luaL_newlib(L, httplib);
	return 1;
}
