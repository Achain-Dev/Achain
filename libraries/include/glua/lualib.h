/*
** $Id: lualib.h,v 1.44 2014/02/06 17:32:33 roberto Exp $
** Lua standard libraries
** See Copyright Notice in lua.h
*/


#ifndef lualib_h
#define lualib_h

#include "glua/lua.h"



LUAMOD_API int (luaopen_base)(lua_State *L);

#define LUA_COLIBNAME	"coroutine"
LUAMOD_API int (luaopen_coroutine)(lua_State *L);

#define LUA_TABLIBNAME	"table"
LUAMOD_API int (luaopen_table)(lua_State *L);

#define LUA_IOLIBNAME	"io"
LUAMOD_API int (luaopen_io)(lua_State *L);

#define LUA_OSLIBNAME	"os"
LUAMOD_API int (luaopen_os)(lua_State *L);

#define LUA_TIMELIBNAME "time"
LUAMOD_API int (luaopen_time)(lua_State *L);

#define LUA_STRLIBNAME	"string"
LUAMOD_API int (luaopen_string)(lua_State *L);

#define LUA_UTF8LIBNAME	"utf8"
LUAMOD_API int (luaopen_utf8)(lua_State *L);

#define LUA_NETLIBNAME	"net"
LUAMOD_API int (luaopen_net)(lua_State *L);

#define LUA_HTTPLIBNAME "http"
LUAMOD_API int (luaopen_http)(lua_State *L);

#define LUA_JSONRPCLIBNAME "jsonrpc"
LUAMOD_API int (luaopen_jsonrpc)(lua_State *L);

#define LUA_BITLIBNAME	"bit32"
LUAMOD_API int (luaopen_bit32)(lua_State *L);

#define LUA_MATHLIBNAME	"math"
LUAMOD_API int (luaopen_math)(lua_State *L);

#define LUA_THINKYOUNGLIBNAME "thinkyoung"
LUAMOD_API int(luaopen_thinkyoung)(lua_State *L);

#define LUA_DBLIBNAME	"debug"
LUAMOD_API int (luaopen_debug)(lua_State *L);

#define LUA_JSONLIBNAME	"json"
LUAMOD_API int (luaopen_json)(lua_State *L);

#define LUA_LOADLIBNAME	"package"
LUAMOD_API int (luaopen_package)(lua_State *L);


/* open all previous libraries */
LUALIB_API void (luaL_openlibs)(lua_State *L);



#if !defined(lua_assert)
#define lua_assert(x)	((void)0)
#endif


#endif
