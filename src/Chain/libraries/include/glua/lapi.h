/*
** $Id: lapi.h,v 2.9 2015/03/06 19:49:50 roberto Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h

#include <stdio.h>
#include <string.h>
#include <string>
#include <list>

#include "glua/llimits.h"
#include "glua/lstate.h"
#include "glua/thinkyoung_lua_api.h"

#define api_incr_top(L)   {L->top++; api_check(L, L->top <= L->ci->top, \
				"stack overflow");}

#define adjustresults(L,nres) \
        { if ((nres) == LUA_MULTRET && L->ci->top < L->top) L->ci->top = L->top; }

#define api_checknelems(L,n)	api_check(L, (n) < (L->top - L->ci->func), \
				  "not enough elements in the stack")

LUA_API int (lua_compilex)(lua_State *L, lua_Reader reader, void *dt,
    const char *chunkname, const char *mode, lua_Writer writer, const char *out_filename);

LUA_API int (lua_compile)(lua_State *L, lua_Reader reader, void *dt,
    const char *chunkname, const char *mode, lua_Writer writer, FILE *out_file);

LUA_API int (lua_compile_to_stream)(lua_State *L, lua_Reader reader, void *dt,
    const char *chunkname, const char *mode, lua_Writer writer, GluaModuleByteStreamP stream);

/**
 * compile lua code file to bytecode file
 * @return TRUE(1 or not 0) if success, FALSE(0) if failed
 */
LUA_API int(luaL_compilefile)(const char *filename, const char *out_filename, char *error,
	bool use_type_check = false, bool is_contract=false);

LUA_API int(luaL_compilefilex)(const char *filename, FILE *out_file, char *error, bool is_contract=false);

LUA_API int(luaL_compilefile_to_stream)(lua_State *L, const char *filename, GluaModuleByteStreamP stream,
	char *error, bool use_type_check = false, bool is_contract=false);

/**
 * index -2 is key, index -1 is value
 */
typedef bool (lua_table_traverser)(lua_State *L, void *ud);

typedef bool (lua_table_traverser_with_nested)(lua_State *L, void *ud, size_t len, std::list<const void *> &jsons, size_t recur_depth);

/**
 * traverse visit lua table
 */
size_t luaL_traverse_table(lua_State *L, int index, lua_table_traverser traverser, void *ud);
size_t luaL_traverse_table_with_nested(lua_State *L, int index, lua_table_traverser_with_nested traverser, void *ud, std::list<const void*> &jsons, size_t recur_depth);

/**
 * count size of _G(global variables table)
 */
size_t luaL_count_global_variables(lua_State *L);

/**
 * get global var names of L, store them in param list
 */
void luaL_get_global_variables(lua_State *L, std::list<std::string> *list);


#endif
