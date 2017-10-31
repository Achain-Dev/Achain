/*
** $Id: linit.c,v 1.38 2015/01/05 13:48:33 roberto Exp $
** Initialization of libraries for lua.c and other clients
** See Copyright Notice in lua.h
*/


#define linit_cpp

/*
** If you embed Lua in your program and need to open the standard
** libraries, call luaL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  luaL_getsubtable(L, LUA_REGISTRYINDEX, "_PRELOAD");
**  lua_pushcfunction(L, luaopen_modname);
**  lua_setfield(L, -2, modname);
**  lua_pop(L, 1);  // remove _PRELOAD table
*/

#include "glua/lprefix.h"


#include <stddef.h>

#include "glua/lua.h"

#include "glua/lualib.h"
#include "glua/lauxlib.h"


/*
** these libs are loaded by lua.c and are readily available to any Lua
** program
*/
static const luaL_Reg loadedlibs[] = {
    { "_G", luaopen_base },
    { LUA_LOADLIBNAME, luaopen_package }, // READ, whether this module can load outside libs?
    { LUA_COLIBNAME, luaopen_coroutine },
    { LUA_TABLIBNAME, luaopen_table },
    // {LUA_IOLIBNAME, luaopen_io},
    // {LUA_OSLIBNAME, luaopen_os},
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_TIMELIBNAME, luaopen_time },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_THINKYOUNGLIBNAME, luaopen_thinkyoung },
    { LUA_JSONLIBNAME, luaopen_json },
    { LUA_UTF8LIBNAME, luaopen_utf8 },
	// { LUA_NETLIBNAME, luaopen_net },
	// { LUA_HTTPLIBNAME, luaopen_http },
	// { LUA_JSONRPCLIBNAME, luaopen_jsonrpc },
    // {LUA_DBLIBNAME, luaopen_debug},
#if defined(LUA_COMPAT_BITLIB)
    { LUA_BITLIBNAME, luaopen_bit32 },
#endif
    { nullptr, nullptr }
};


LUALIB_API void luaL_openlibs(lua_State *L) {
    const luaL_Reg *lib;
    /* "require" functions from 'loadedlibs' and set results to global table */
    for (lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);  /* remove lib */
    }
}

