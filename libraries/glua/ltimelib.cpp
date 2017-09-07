#define ltimelib_cpp

#include "glua/lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "glua/lua.h"

#include "glua/lauxlib.h"
#include "glua/lualib.h"

using thinkyoung::lua::api::global_glua_chain_api;


#define ARGS_ERROR()   { global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "arguments wrong"); return 0; }

static int time_difftime(lua_State *L) {
    if (lua_gettop(L) < 2 || !lua_isinteger(L, 1) || !lua_isinteger(L, 2)) {
        ARGS_ERROR();
    }
    lua_Integer t1 = luaL_checkinteger(L, 1);
    lua_Integer t2 = luaL_checkinteger(L, 2);
    lua_pushinteger(L, t1 - t2);
    return 1;
}

/************************************************************************/
/* time.add(timestamp, 'year'/'month'/'day'/'hour'/'minute'/'second', +100/-100)  */
/************************************************************************/
static int time_add(lua_State *L) {
    if (lua_gettop(L) < 3 || !lua_isinteger(L, 1) || !lua_isstring(L, 2) || !lua_isinteger(L, 3)) {
        ARGS_ERROR();
    }
    lua_Integer timestamp = luaL_checkinteger(L, 1);
    const char *field = luaL_checkstring(L, 2);
    auto offset = (int)luaL_checkinteger(L, 3);

    const time_t rawtime = (const time_t)timestamp;
    struct tm * dt;
    dt = localtime(&rawtime);
    if (!dt)
    {
      ARGS_ERROR();
    }
    struct tm cdt = *dt;
    if (strcmp(field, "year") == 0)
        cdt.tm_year += offset;
    else if (strcmp(field, "month") == 0)
        cdt.tm_mon += offset;
    else if (strcmp(field, "day") == 0)
        cdt.tm_yday += offset;
    else if (strcmp(field, "hour") == 0)
        cdt.tm_hour += offset;
    else if (strcmp(field, "minute") == 0)
        cdt.tm_hour += offset;
    else if (strcmp(field, "second") == 0)
        cdt.tm_sec += offset;
    else
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
            "time.add second argument need be 'year'/'month'/'day'/'hour'/'minute'/'second'");
        return 0;
    }
    time_t result_time = mktime(&cdt);
    lua_pushinteger(L, (lua_Integer)result_time);
    return 1;
}

static int time_tostr(lua_State *L)
{
    if (lua_gettop(L) < 1 || !lua_isinteger(L, 1)) {
        ARGS_ERROR();
    }
    lua_Integer timestamp = luaL_checkinteger(L, 1);
    const time_t rawtime = (const time_t)timestamp;
    struct tm * dt;
    char buffer[30];
    dt = localtime(&rawtime);
    if (!dt)
    {
      ARGS_ERROR();
    }
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", dt);
    lua_pushstring(L, buffer);
    return 1;
}

static const luaL_Reg timelib[] = {
    { "add", time_add },
    { "difftime", time_difftime },
    { "tostr", time_tostr },
    { nullptr, nullptr }
};

LUAMOD_API int luaopen_time(lua_State *L) {
    luaL_newlib(L, timelib);
    return 1;
}