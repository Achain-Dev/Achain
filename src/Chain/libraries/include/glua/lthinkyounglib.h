#ifndef lthinkyounglib_h
#define lthinkyounglib_h

#include <glua/lprefix.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <list>
#include <set>
#include <unordered_map>
#include <memory>

#include <glua/lua.h>
#include <glua/lauxlib.h>
#include <glua/lualib.h>
#include <glua/lapi.h>

#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>

namespace glua
{
	namespace lib
	{
		int thinkyounglib_get_storage(lua_State *L);
		// thinkyoung.get_storage具体的实现
		int thinkyounglib_get_storage_impl(lua_State *L,
			const char *contract_id, const char *name);

		int thinkyounglib_set_storage(lua_State *L);

        int thinkyounglib_set_storage_impl(lua_State *L,
          const char *contract_id, const char *name, int value_index);
	}
}

#endif