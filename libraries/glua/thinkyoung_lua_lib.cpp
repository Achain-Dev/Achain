#include <glua/lprefix.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <fstream>

#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>
#include <glua/glua_tokenparser.h>
#include <glua/lparsercombinator.h>
#include <glua/ltypechecker.h>
#include <glua/glua_lutil.h>
#include <glua/lobject.h>
#include <glua/lzio.h>
#include <glua/lundump.h>
#include <glua/lapi.h>
#include <glua/lopcodes.h>
#include <glua/lstate.h>
#include <glua/ldebug.h>
#include <glua/lauxlib.h>
#include <glua/lualib.h>
#include <glua/lrepl.h>
#include <glua/lfunc.h>
#include <glua/lcompile.h>
#include <glua/lthinkyounglib.h>
#include <glua/glua_decompile.h>
#include <glua/glua_disassemble.h>

namespace thinkyoung
{
    namespace lua
    {
      namespace api
      {
        IGluaChainApi *global_glua_chain_api = nullptr;
      }

      using thinkyoung::lua::api::global_glua_chain_api;

		namespace lib
		{

			std::vector<std::string> contract_special_api_names = { "init", "on_deposit", "on_destroy", "on_upgrade" };
			std::vector<std::string> contract_int_argument_special_api_names = { "on_deposit" };

#define LUA_REPL_RUNNING_STATE_KEY "lua_repl_running"
#define LUA_IN_SANDBOX_STATE_KEY "lua_in_sandbox"
            // 一次操作中可能改动了storage的contract id集合的state key
#define LUA_MAYBE_CHANGE_STORAGE_CONTRACT_IDS_STATE_KEY "maybe_change_storage_contract_ids_state"

            static const char *globalvar_whitelist[] = {
                "print", "pprint", "table", "string", "time", "math", "json", "type", "require", "Array", "Stream",
                "import_contract_from_address", "import_contract", "emit",
                "thinkyoung", "storage", "repl", "exit", "exit_repl", "self", "debugger", "exit_debugger",
                "caller", "caller_address",
                "contract_transfer", "contract_transfer_to", "transfer_from_contract_to_address",
				"transfer_from_contract_to_public_account",
                "get_chain_random", "get_transaction_fee",
                "get_transaction_id", "get_header_block_num", "wait_for_future_random", "get_waited",
                "get_contract_balance_amount", "get_chain_now", "get_current_contract_address",
                "pairs", "ipairs", "pairsByKeys", "collectgarbage", "error", "getmetatable", "_VERSION",
                "tostring", "tojsonstring", "tonumber", "tointeger", "todouble", "totable",
                "next", "rawequal", "rawlen", "rawget", "rawset", "select",
                "setmetatable"
            };

            // 这里用ordered_map而不是unordered_map是为了保持顺序，比如Stream type要在Stream构造函数前面
			static const std::map<std::string, std::string> globalvar_type_infos =
			{
				// inner types
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(string), "string" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(int), "int" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(number), "number" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(bool), "bool" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(table), "table" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(Array), "Array" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(Map), "Map" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(Nil), "nil" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(Function), "function" },
				{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(object), "object" },

				// bin operations
			{ "(+)", "(number, number) => number" }, // bin operations functions are '(' + op + ')'
			{ "(-)", "(number, number) => number" },
			{ "(*)", "(number, number) => number" },

			// (函数名/操作符名$参数类型$参数类型...)表示具体的重载函数签名，比如(+$int#int)，找不到特化重载函数签名，用非特化版本函数签名
			// 如果是非中缀函数/操作符，则重载函数签名的名称是函数名/操作符名$参数类型$参数类型...，比如 func$int$int$int
			{ "(+$int$int)", "(int, int) => int" },
			{ "(-$int$int)", "(int, int) => int" },
			{ "(*$int$int)", "(int, int) => int" },
			{ "(^$int$int)", "(int, int) => int" },

			{ "(/)", "(number, number) => number" },
			{ "(//)", "(number, number) => int" },
			{ "(^)", "(number, number) => number" },
			{ "(%)", "(number, number) => number" },
			{ "(&)", "(int, int) => int" },
			{ "(~)", "(int, int) => int" },
			{ "(|)", "(number, number) => int" },
			{ "(>>)", "(number, number) => number" },
			{ "(<<)", "(number, number) => number" },
			{ "(<)", "(number, number) => bool" },
			{ "(<=)", "(number, number) => bool" },
			{ "(>)", "(number, number) => bool" },
			{ "(>=)", "(number, number) => bool" },
			{ "(==)", "(object, object) => bool" },
			{ "(~=)", "(object, object) => bool" },
			{ "(and)", "(object, object) => object" },
			{ "(or)", "(object, object) => object" },
			{ "(..)", "(string, string) => string" },
			// infix operations
		{ "-", "(number) => number" },
		{ "not", "(object) => bool" },
		{ "#", "(object) => int" },
		{ "~", "(int) => int" },
		// global functions
	{ "print", "(...) => void" },
	{ "pprint", "(...) => void" },
	{ "table", R"END(record {
concat:(table,string, ...)=>string;
insert:(table, ...)=>void;
append: (table, object) => void;
length: (table) => int;
remove:(table, ...) => object;
sort:(table, ...) => void
})END" },
{ "string", R"END(record {
split: (string, string) => table;
byte: (string) => int;
char: (...) => string;
find: (string, string, ...) => string;
format: (string, ...) => string;
gmatch: (string, string) => function;
gsub: (string, string, string, ...) => string;
len: (string) => int;
match: (string, string, ...) => string;
rep: (string, int, ...) => string;
reverse: (string) => string;
sub: (string, int, ...) => string;
upper: (string) => string
})END" },
{ "Array", "(object) => table" },
{ "time", R"END(record {
add: (int, string, int) => int;
tostr: (int) => string;
difftime: (int, int) => int
})END" },
{ "math", R"END(record {
abs$int: (int) => int;
abs: (number) => number;
ceil: (number) => int;
floor: (number) => int;
max: (...) => number;
maxinteger: int;
min: (...) => number;
mininteger: int;
pi: number;
tointeger: (number) => int;
type: (number) => string
})END" },
{ "json", R"END(record {
dumps: (object) => string;
loads: (string) => object
})END" },
{ "utf8", R"END(record {
char: (...) => string;
charpattern: string;
codes: (string) => function;
codepoint: (string, int, int) => int;
len: (striing, int, int) => int;
offset: (string, int, int) => int
})END" },
{ "os", R"END(record {
clock: () => int;
date: (string, int) => string;
difftime: (int, int) => int;
execute: (string) => void;
exit: (int, bool) => void;
getenv: (string) => string;
remove: (string) => void;
rename: (string, string) => void;
setlocale: (string, string) => void;
time: (...) => int;
tmpname: () => string
})END" },
{ "io", R"END(record {
close: (...) => void;
flush: () => void;
input: (...) => void;
lines: (string) => function;
open: (string, string) => void;
read: (string) => string;
seek: (...) => int;
write: (string) => void
})END" },
{ "net", R"END(record {
listen: (string, port) => object;
connect: (string, port) => object;
accept: (object) => object;
accept_async: (object, function) => void;
start_io_loop: (object) => void;
read: (object, int) => string;
read_until: (object, string) => string;
write: (object, object) => void;
close_socket: (object) => void;
close_server: (object) => void;
shutdown: () => void
})END" },
{ "http", R"END(record {
listen: (string, port) => object;
connect: (string, port) => object;
request: (string, string, string, table) => object;
close: (object) => void;
accept: (object) => object;
accept_async: (object, function) => void;
start_io_loop: (object) => void;
get_req_header: (object, string) => string;
get_res_header: (object, string) => string;
get_req_http_method: (object) => string;
get_req_path: (object) => string;
get_req_http_protocol: (object) => string;
get_req_body: (object) => string;
set_res_header: (object, string, string) => void;
write_res_body: (object, string) => void;
set_status: (object, int, string) => void;
get_status: (object) => int;
get_status_message: (object) => string;
get_res_body: (object) => string;
finish_res: (object) => void
})END" },
// FIXME: 下面这个Stream record的成员函数，第一个参数应该是self
{ GLUA_TYPE_NAMESPACE_PREFIX_WRAP(Stream), R"END(record {
size: (table) => int;
pos: (table) => int;
reset_pos: (table) => void;
current: (table) => int;
eof: (table) => bool;
push: (table, int) => void;
push_string: (table, string) => void;
next: (table) => bool
})END"},
{"Stream", "() => Stream"},
				{ "jsonrpc", R"END(record {})END" },
                { "type", "(object) => string" },
                { "require", "(string) => object" },
                { "import_contract_from_address", "(string) => table" },
                { "import_contract", "(string) => table" },
                // { "emit", "(string, string) => void" },
                { "thinkyoung", "table" },
                { "storage", "table" },
                { "repl", "() => void" },
                { "exit", "(object) => object" },
				{ "exit_repl", "object (object)" },
                { "debugger", "(...) => void" },
                { "exit_debugger", "() => void" },
                { "caller", "string" },
                { "caller_address", "string" },
				// 脚本模式下的全局变量
				{ "param", "string" },
				{ "truncated", "bool" },
				{ "contract_id", "string" },
				{ "event_type", "string" },

                { "contract_transfer", "(...) => object" },
                { "contract_transfer_to", "(...) => object" },
                { "transfer_from_contract_to_address", "(string, string, int) => int" },
				{ "transfer_from_contract_to_public_account", "(string, string, int) => int"},
                { "get_chain_random", "() => number" },
                { "get_transaction_fee", "() => int" },
                { "get_transaction_id", "() => string" },
                { "get_header_block_num", "() => int" },
                { "wait_for_future_random", "(int) => int" },
                { "get_waited", "(int) => int" },
                { "get_contract_balance_amount", "(string, string) => int" },
                { "get_chain_now", "() => int" },
                { "get_current_contract_address", "() => string" },
                { "pairs", "(table) => object" },
                { "ipairs", "(table) => object" },
				{ "pairsByKeys", "(table) => object" },
                { "collectgarbage", "object (...)" },
                { "error", "(...) => object" },
                { "getmetatable", "(table) => table" },
                { "_VERSION", "string" },
				{ "_ENV", "table" },
				{ "_G", "table" },
                { "tostring", "(object) => string" },
                { "tojsonstring", "(object) => string" },
                { "tonumber", "(object) => number" },
                { "tointeger", "(object) => int" },
                { "todouble", "(object) => number" },
                { "totable", "(object) => table" },
				{ "toboolean", "(object) => bool" },
                { "next", "(...) => object" },
                { "rawequal", "(object, object) => bool" },
                { "rawlen", "(object) => int" },
                { "rawget", "(object, object) => object" },
                { "rawset", "(object, object, object) => void" },
                { "select", "(...) => object" },
                { "setmetatable", "(table, table) => void" }
            };

            const std::map<std::string, std::string> *get_globalvar_type_infos()
            {
                return &globalvar_type_infos;
            }

            typedef lua_State* L_Key1;

            typedef std::unordered_map<std::string, GluaStateValueNode> L_VM1;

            typedef std::shared_ptr<L_VM1> L_V1;

            typedef std::unordered_map<L_Key1, L_V1> LStatesMap;

            static LStatesMap states_map;

            static LStatesMap *get_lua_states_value_hashmap()
            {
                return &states_map;
            }

            static std::mutex states_map_mutex;

            static L_V1 create_value_map_for_lua_state(lua_State *L)
            {
                LStatesMap *states_map = get_lua_states_value_hashmap();
                auto it = states_map->find(L);
                if (it == states_map->end())
                {
                    states_map_mutex.lock();
                    L_V1 map = std::make_shared<L_VM1>();
                    states_map->insert(std::make_pair(L, map));
                    states_map_mutex.unlock();
                    return map;
                }
                else
                    return it->second;
            }

			// 从当前合约总转账到
			static int transfer_from_contract_to_public_account(lua_State *L)
            {
				if (lua_gettop(L) < 3)
				{
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "transfer_from_contract_to_public_account need 3 arguments");
					return 0;
				}
				const char *contract_id = get_contract_id_in_api(L);
				if (nullptr == contract_id)
				{
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract transfer must be called in contract api");
					return 0;
				}
				const char *to_account_name = luaL_checkstring(L, 1);
				const char *asset_type = luaL_checkstring(L, 2);
				auto amount_str = luaL_checkinteger(L, 3);
				if (amount_str <= 0)
				{
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "amount must be positive");
					return 0;
				}
				lua_Integer transfer_result = thinkyoung::lua::api::global_glua_chain_api->transfer_from_contract_to_public_account(L, contract_id, to_account_name, asset_type, amount_str);
				lua_pushinteger(L, transfer_result);
				return 1;
            }

            /************************************************************************/
            /* transfer from contract to address                                    */
            /************************************************************************/
            static int transfer_from_contract_to_address(lua_State *L)
            {
                if (lua_gettop(L) < 3)
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "transfer_from_contract_to_address need 3 arguments");
                    return 0;
                }
                const char *contract_id = get_contract_id_in_api(L);
                if (!contract_id)
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract transfer must be called in contract api");
                    return 0;
                }
                const char *to_address = luaL_checkstring(L, 1);
                const char *asset_type = luaL_checkstring(L, 2);
                auto amount_str = luaL_checkinteger(L, 3);
                if (amount_str <= 0)
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "amount must be positive");
                    return 0;
                }
                lua_Integer transfer_result = thinkyoung::lua::api::global_glua_chain_api->transfer_from_contract_to_address(L, contract_id, to_address, asset_type, amount_str);
                lua_pushinteger(L, transfer_result);
                return 1;
            }

            static int get_contract_address_lua_api(lua_State *L)
            {
                const char *cur_contract_id = get_contract_id_in_api(L);
                if (!cur_contract_id)
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "can't get current contract address");
                    return 0;
                }
                lua_pushstring(L, cur_contract_id);
                return 1;
            }

            static int get_contract_balance_amount(lua_State *L)
            {
                if (lua_gettop(L) > 0 && !lua_isstring(L, 1))
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
                        "get_contract_balance_amount need 1 string argument of contract address");
                    return 0;
                }

				auto contract_address = luaL_checkstring(L, 1);
                if (strlen(contract_address) < 1)
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
                        "contract address can't be empty");
                    return 0;
                }

                if (lua_gettop(L) < 2 || !lua_isstring(L, 2))
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "get balance amount need asset symbol");
                    return 0;
                }

                auto assert_symbol = luaL_checkstring(L, 2);
                
                auto result = thinkyoung::lua::api::global_glua_chain_api->get_contract_balance_amount(L, contract_address, assert_symbol);
                lua_pushinteger(L, result);
                return 1;
            }

            // pair: (value_string, is_upvalue)
            typedef std::unordered_map<std::string, std::pair<std::string, bool>> LuaDebuggerInfoList;

            static LuaDebuggerInfoList g_debug_infos;
            static bool g_debug_running = false;

            static LuaDebuggerInfoList *get_lua_debugger_info_list_in_state(lua_State *L)
            {
                return &g_debug_infos;
            }

            static int enter_lua_debugger(lua_State *L)
            {
                LuaDebuggerInfoList *debugger_info = get_lua_debugger_info_list_in_state(L);
                if (!debugger_info)
                    return 0;
                if (lua_gettop(L) > 0 && lua_isstring(L, 1))
                {
                    // if has string parameter, use this function to get the value of the name in running debugger;
                    auto found = debugger_info->find(luaL_checkstring(L, 1));
                    if (found == debugger_info->end())
                        return 0;
                    lua_pushstring(L, found->second.first.c_str());
                    lua_pushboolean(L, found->second.second ? 1 : 0);
                    return 2;
                }

                debugger_info->clear();
                g_debug_running = true;

                // get all info snapshot in current lua_State stack
                // and then pause the thread(but other thread can use this lua_State
                // maybe you want to start a REPL before real enter debugger

                // first need back to caller func frame

                CallInfo *ci = L->ci;
                CallInfo *oci = ci->previous;
                Proto *np = getproto(ci->func);
                Proto *p = getproto(oci->func);
                LClosure *ocl = clLvalue(oci->func);
                int real_localvars_count = (int)(ci->func - oci->func - 1);
                int linedefined = p->linedefined;
                // TODO: maybe can pre-modify the debugger() in source code into (function() _debugger())() and then get the source code
                // fprintf(L->out, "debugging into line %d\n", linedefined); // FIXME: logging the debugging code line
                L->ci = oci;
                int top = lua_gettop(L);

                // capture locals vars and values(need get localvar name from whereelse)
                for (int i = 0; i < top; ++i)
                {
                    luaL_tojsonstring(L, i + 1, nullptr);
                    const char *value_str = luaL_checkstring(L, -1);
                    lua_pop(L, 1);
                    // if the debugger position is before the localvar position, ignore the next localvars
                    if (i >= real_localvars_count) // the localvars is after the debugger() call
                        break;
                    // get local var name
                    if (i < p->sizelocvars)
                    {
                        LocVar localvar = p->locvars[i];
                        const char *varname = getstr(localvar.varname);
						if(L->out)
							fprintf(L->out, "[debugger]%s=%s\n", varname, value_str);
                        debugger_info->insert(std::make_pair(varname, std::make_pair(value_str, false)));
                    }
                }
                // capture upvalues vars and values(need get upvalue name from whereelse)
                L->ci = ci;
                for (int i = 0; i < p->sizeupvalues; ++i)
                {
                    Upvaldesc upval = p->upvalues[i];
                    const char *upval_name = getstr(upval.name);
                    if (std::string(upval_name) == "_ENV")
                        continue;
                    UpVal *upv = ocl->upvals[upval.idx];
                    lua_pushinteger(L, 1);
                    setobj2s(L, ci->func + 1, upv->v);
                    auto value_string1 = luaL_tojsonstring(L, -1, nullptr);
                    lua_pop(L, 2);
					if(L->out)
						fprintf(L->out, "[debugger-upvalue]%s=%s\n", upval_name, value_string1);
                    debugger_info->insert(std::make_pair(upval_name, std::make_pair(value_string1, true)));
                }
                L->ci = ci;
                if (L->debugger_pausing)
                {
                    do
                    {
                        std::this_thread::yield();
                    } while (g_debug_running); // while not exit lua debugger
                }
                return 0;
            }

            static int exit_lua_debugger(lua_State *L)
            {
                g_debug_infos.clear();
                g_debug_running = false;
                return 0;
            }

            static int panic_message(lua_State *L)
            {
                auto msg = luaL_checkstring(L, -1);
                if (nullptr != msg)
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, msg);
                return 0;
            }

            static int get_chain_now(lua_State *L)
            {
                auto time = thinkyoung::lua::api::global_glua_chain_api->get_chain_now(L);
                lua_pushinteger(L, time);
                return 1;
            }

            static int get_chain_random(lua_State *L)
            {
                auto rand = thinkyoung::lua::api::global_glua_chain_api->get_chain_random(L);
                lua_pushinteger(L, rand);
                return 1;
            }
            static int get_transaction_id(lua_State *L)
            {
                std::string tid = thinkyoung::lua::api::global_glua_chain_api->get_transaction_id(L);
                lua_pushstring(L, tid.c_str());
                return 1;
            }
            static int get_transaction_fee(lua_State *L)
            {
                int64_t res = thinkyoung::lua::api::global_glua_chain_api->get_transaction_fee(L);
                lua_pushinteger(L, res);
                return 1;
            }
            static int get_header_block_num(lua_State *L)
            {
                auto result = thinkyoung::lua::api::global_glua_chain_api->get_header_block_num(L);
                lua_pushinteger(L, result);
                return 1;
            }

            static int wait_for_future_random(lua_State *L)
            {
                if (lua_gettop(L) < 1 || !lua_isinteger(L, 1))
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "wait_for_future_random need a integer param");
                    return 0;
                }
                auto next = luaL_checkinteger(L, 1);
                if (next <= 0)
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "wait_for_future_random first param must be positive number");
                    return 0;
                }
                auto result = thinkyoung::lua::api::global_glua_chain_api->wait_for_future_random(L, (int)next);
                lua_pushinteger(L, result);
                return 1;
            }

            /************************************************************************/
            /* 获取某个块（可以是未来块，也可能是过去块）上某个哈希数据产生的伪随机数               */
            /************************************************************************/
            static int get_waited_block_random(lua_State *L)
            {
                if (lua_gettop(L) < 1 || !lua_isinteger(L, 1))
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "get_waited need a integer param");
                    return 0;
                }
                auto num = luaL_checkinteger(L, 1);
                auto result = thinkyoung::lua::api::global_glua_chain_api->get_waited(L, (uint32_t)num);
                lua_pushinteger(L, result);
                return 1;
            }

            static int emit_thinkyoung_event(lua_State *L)
            {
                if (lua_gettop(L) < 2 && (!lua_isstring(L, 1) || !lua_isstring(L, 2)))
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "emit need 2 string params");
                    return 0;
                }
                const char *contract_id = get_contract_id_in_api(L);
                const char *event_name = luaL_checkstring(L, 1);
                const char *event_param = luaL_checkstring(L, 2);
				if (!contract_id || strlen(contract_id) < 1)
					return 0;
                thinkyoung::lua::api::global_glua_chain_api->emit(L, contract_id, event_name, event_param);
                return 0;
            }

			static int glua_core_lib_Stream_size(lua_State *L)
            {
				auto stream = (GluaByteStream*) luaL_checkudata(L, 1, "GluaByteStream_metatable");
				if(thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t) stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					auto stream_size = stream->size();
					lua_pushinteger(L, stream_size);
					return 1;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
            }

			static int glua_core_lib_Stream_eof(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					lua_pushboolean(L, stream->eof());
					return 1;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			static int glua_core_lib_Stream_current(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					lua_pushinteger(L, stream->current());
					return 1;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			static int glua_core_lib_Stream_next(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					lua_pushboolean(L, stream->next());
					return 1;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			static int glua_core_lib_Stream_pos(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					lua_pushinteger(L, stream->pos());
					return 1;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			static int glua_core_lib_Stream_reset_pos(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					stream->reset_pos();
					return 0;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			static int glua_core_lib_Stream_push(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				auto c = luaL_checkinteger(L, 2);
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					stream->push((char)c);
					return 0;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			static int glua_core_lib_Stream_push_string(lua_State *L)
			{
				auto stream = (GluaByteStream*)luaL_checkudata(L, 1, "GluaByteStream_metatable");
				auto argstr = luaL_checkstring(L, 2);
				if (thinkyoung::lua::api::global_glua_chain_api->is_object_in_pool(L, (intptr_t)stream,
					GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE)>0)
				{
					for(size_t i=0;i<strlen(argstr);++i)
					{
						stream->push(argstr[i]);
					}
					return 0;
				}
				else
				{
					luaL_argerror(L, 1, "Stream expected");
					return 0;
				}
			}

			// Stream类型的构造函数,为了避免tostring到处出问题，使用lightuserdata
			// 不用userdata来托管内存到glua gc中是考虑到指针可能是new class出来的
			// 调用函数的时候
			static int glua_core_lib_Stream(lua_State *L)
            {
				auto stream = new GluaByteStream();
				thinkyoung::lua::api::global_glua_chain_api->register_object_in_pool(L, (intptr_t) stream, GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE);
				lua_pushlightuserdata(L, (void*) stream);
				luaL_getmetatable(L, "GluaByteStream_metatable");
				lua_setmetatable(L, -2);
				return 1;
            }

			// function Array(props) return props or {}; end
			static int glua_core_lib_Array(lua_State *L)
            {
	            if(lua_gettop(L)<1 || lua_isnil(L, 1) || !lua_istable(L, 1))
	            {
					lua_createtable(L, 0, 0);
					return 1;
	            }
				else
				{
					lua_pushvalue(L, 1);
					return 1;
				}
            }

			static int glua_core_lib_Hashmap(lua_State *L)
            {
	            if(lua_gettop(L)<1 || !lua_istable(L, 1))
	            {
					lua_createtable(L, 0, 0);
					return 1;
	            }
				else
				{
					lua_pushvalue(L, 1);
					return 1;
				}
            }

			// contract::__index: function (t, k) return t._data[k]; end
			static int glua_core_lib_contract_metatable_index(lua_State *L)
            {
				// top:2: table, key
				lua_pushstring(L, "id");
				lua_rawget(L, 1);
				auto contract_id_in_contract = lua_tostring(L, 3);
				lua_pop(L, 1);

				auto key = luaL_checkstring(L, 2);
				lua_getfield(L, 1, "_data");

				lua_pushstring(L, "id");
				lua_rawget(L, 3);
				auto contract_id_in_data_prop = lua_tostring(L, 4);
				lua_pop(L, 1);

				auto contract_id = contract_id_in_contract ? contract_id_in_contract : contract_id_in_data_prop;

				lua_getfield(L, 3, key);
				
				auto tmp_global_key = "_glua_core_lib_contract_metatable_index_tmp_value";
				lua_setglobal(L, tmp_global_key);
				lua_pop(L, 1);
				lua_getglobal(L, tmp_global_key);
				lua_pushnil(L);
				lua_setglobal(L, tmp_global_key);

				return 1;
            }

			/*
			contract::__newindex:
			function(t, k, v)
				if not t._data then
					print("empty _data\n")
				end
				if k == 'id' or k == 'name' or k == 'storage' then
					if not t._data[k] then
						t._data[k] = v
						return
					end
					error("attempt to update a read-only table!")
					return
				else
					t._data[k] = v
				end
			end
			*/
			static int glua_core_lib_contract_metatable_newindex(lua_State *L)
			{
				lua_getfield(L, 1, "_data"); // stack: t, k, v, _data
				if(lua_isnil(L, 4))
				{
					if (L->out)
						fprintf(L->out, "empty _data\n");
					lua_pop(L, 1);
					return 0;
				}
				auto key = luaL_checkstring(L, 2);
				std::string key_str(key);
				if(key == "id" || key == "name" || key == "storage")
				{
					lua_getfield(L, 4, key); // stack: t, k, v, _data, _data[k]
					if(lua_isnil(L, 5))
					{
						lua_pop(L, 1); // stack: t, k, v, _data
						lua_pushvalue(L, 3); // stack: t, k, v, _data, v
						lua_setfield(L, 4, key); // stack: t, k, v, _data
						lua_pop(L, 1);
						return 0;
					}
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "attempt to update a read-only table!");
					lua_pop(L, 2); // stack: t, k, v
					return 0;
				}
				else
				{
					lua_pushvalue(L, 3); // stack: t, k, v, _data, v
					lua_setfield(L, 4, key); // stack: t, k, v, _data
					lua_pop(L, 1); // stack: t, k, v
					return 0;
				}
			}

            // 对storage的访问操作会访问这个方法
			// storage::__index: function(s, key)
			//    if type(key) ~= 'string' then
			//    thinkyoung.error('only string can be storage key')
			//	  return nil
			//	  end
			//	  return thinkyoung.get_storage(s.contract, key)
			//	  end
			static int glua_core_lib_storage_metatable_index(lua_State *L)
            {
				if(!lua_isstring(L, 2))
				{
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "only string can be storage key");
					L->force_stopping = true;
					lua_pushnil(L);
					return 1;
				}
				auto key = luaL_checkstring(L, 2); // top=2
				lua_getfield(L, 1, "contract"); // top=3
				lua_getfield(L, 3, "id");
				auto contract_id = luaL_checkstring(L, -1);
				lua_pop(L, 1);
				lua_pushvalue(L, 2); // top=4
				auto ret_count = glua::lib::thinkyounglib_get_storage_impl(L, contract_id, key); // top=ret_count + 4
				if(ret_count>0)
				{
					auto tmp_global_key = "_glua_core_lib_thinkyoung_get_storage_tmp_value";
					lua_setglobal(L, tmp_global_key); // top=ret_count + 3
					lua_pop(L, ret_count + 1); // top=2
					lua_getglobal(L, tmp_global_key); // top=3
					lua_pushnil(L); // top=4
					lua_setglobal(L, tmp_global_key); // top=3
					return 1;
				} else
				{
					lua_pop(L, 2);
					return 0;
				}
            }

            // 对storage的写入操作会调用此API
			// storage::__newindex: function(s, key, val)
			// if type(key) ~= 'string' then
			//	thinkyoung.error('only string can be storage key')
			//	return nil
			//	end
			//	return thinkyoung.set_storage(s.contract, key, val)
			//	end
			static int glua_core_lib_storage_metatable_new_index(lua_State *L)
            {
				if (!lua_isstring(L, 2))
				{
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "only string can be storage key");
					L->force_stopping = true;
					lua_pushnil(L);
					return 1;
				}

                auto key = luaL_checkstring(L, 2); // top=3, self, key, value
                lua_getfield(L, 1, "contract"); // top=4, self, key, value, contract
                lua_getfield(L, 4, "id"); // top=5, self, key, value, contract, contract_id
                auto contract_id = luaL_checkstring(L, -1);
                lua_pop(L, 2); // top=3, self, key, value
                return glua::lib::thinkyounglib_set_storage_impl(L, contract_id, key, 3);

                /*
				int old_top = lua_gettop(L); // 3
				lua_getfield(L, 1, "contract"); // top=4
				lua_pushcfunction(L, glua::lib::thinkyounglib_set_storage); // storage, key, val, contract, set_storage
				lua_pushvalue(L, 4); // storage, key, val, contract, set_storage, contract
				lua_pushvalue(L, 2);
				lua_pushvalue(L, 3); // storage, key, val, contract, set_storage, contract, key, val
				lua_call(L, 3, 0);
				lua_pop(L, lua_gettop(L) - old_top);
                */

				// return 0;
            }

			static int glua_core_lib_pairs_by_keys_func_loader(lua_State *L)
            {
				lua_getglobal(L, "__real_pairs_by_keys_func");
				bool exist = !lua_isnil(L, -1);
				lua_pop(L, 1);
				if (exist)
					return 0; 
				// TODO: 修改基础库本身的pairs实现, 或者把内容放入另一个函数，那个函数根据需要dostring产生一个新函数（cached)去执行
				// pairsByKeys的排序方式是先数字key部分遍历，然后哈希表字符串key部分按key字符串长度和key字符序从小到大遍历
				const char *code = R"END(
function __real_pairs_by_keys_func(t)
	local hashes = {}  
	local n = nil
	local int_key_size = 0;
	local int_hashes = {}
    for n in __old_pairs(t) do
		if type(n) == 'number' then
			int_key_size = int_key_size + 1
			int_hashes[#int_hashes+1] = n
		else
			hashes[#hashes+1] = tostring(n)
		end
    end  
	table.sort(int_hashes)
    table.sort(hashes)  
	local k = 0
	while k < #hashes do
		k = k + 1
	end
    local i = 0  
    return function()  
        i = i + 1  
		if i <= int_key_size then
			return int_hashes[i], t[int_hashes[i]]
		end
        return hashes[i-int_key_size], t[hashes[i-int_key_size]] 
    end  
end
)END";
				luaL_dostring(L, code);
				return 0;
            }

			/*
			function pairsByKeys(t)
				glua_core_lib_pairs_by_keys_func_loader()
				return __real_pairs_by_keys_func(t)
			end 
			*/
			static int glua_core_lib_pairs_by_keys(lua_State *L)
            {
				lua_getglobal(L, "glua_core_lib_pairs_by_keys_func_loader");
				lua_call(L, 0, 0);
				lua_getglobal(L, "__real_pairs_by_keys_func");
				lua_pushvalue(L, 1);
				lua_call(L, 1, 1);
				return 1;
            }

            lua_State *create_lua_state(bool use_contract)
            {
                lua_State *L = luaL_newstate();
                luaL_openlibs(L);
                // run init lua code here, eg. init storage api, load some modules
				add_global_c_function(L, "debugger", &enter_lua_debugger);
				add_global_c_function(L, "exit_debugger", &exit_lua_debugger);
				lua_createtable(L, 0, 0);
				lua_setglobal(L, "last_return"); // 函数的最后返回值记录到这个全局变量
				add_global_c_function(L, "Array", &glua_core_lib_Array);
				add_global_c_function(L, "Map", &glua_core_lib_Hashmap);

				luaL_newmetatable(L, "GluaByteStream_metatable");
				lua_pushcfunction(L, &glua_core_lib_Stream_size);
				lua_setfield(L, -2, "size");
				lua_pushcfunction(L, &glua_core_lib_Stream_eof);
				lua_setfield(L, -2, "eof");
				lua_pushcfunction(L, &glua_core_lib_Stream_current);
				lua_setfield(L, -2, "current");
				lua_pushcfunction(L, &glua_core_lib_Stream_pos);
				lua_setfield(L, -2, "pos");
				lua_pushcfunction(L, &glua_core_lib_Stream_next);
				lua_setfield(L, -2, "next");
				lua_pushcfunction(L, &glua_core_lib_Stream_push);
				lua_setfield(L, -2, "push");
				lua_pushcfunction(L, &glua_core_lib_Stream_push_string);
				lua_setfield(L, -2, "push_string");
				lua_pushcfunction(L, &glua_core_lib_Stream_reset_pos);
				lua_setfield(L, -2, "reset_pos"); 

				lua_pushvalue(L, -1);
				lua_setfield(L, -2, "__index");
				lua_pop(L, 1);

				add_global_c_function(L, "Stream", &glua_core_lib_Stream);

				/*
				add_global_c_function(L, "glua_core_lib_contract_metatable_index", &glua_core_lib_contract_metatable_index);
				add_global_c_function(L, "glua_core_lib_storage_metatable_index", &glua_core_lib_storage_metatable_index);
				add_global_c_function(L, "glua_core_lib_storage_metatable_new_index", &glua_core_lib_storage_metatable_new_index);
				add_global_c_function(L, "glua_core_lib_contract_metatable_newindex", &glua_core_lib_contract_metatable_newindex);
				*/

				add_global_c_function(L, "glua_core_lib_pairs_by_keys_func_loader", &glua_core_lib_pairs_by_keys_func_loader);
				
				/*
				thinkyoung.storage_mt = {
						__index = glua_core_lib_storage_metatable_index,
						__newindex = glua_core_lib_storage_metatable_new_index
					}
					contract_mt = {	  
						__index = glua_core_lib_contract_metatable_index,
						__newindex = glua_core_lib_contract_metatable_newindex
					}
				*/
				lua_createtable(L, 0, 0);
				lua_pushcfunction(L, &glua_core_lib_storage_metatable_index);
				lua_setfield(L, -2, "__index");
				lua_pushcfunction(L, &glua_core_lib_storage_metatable_new_index);
				lua_setfield(L, -2, "__newindex");
				lua_getglobal(L, "thinkyoung");
				lua_pushvalue(L, -2);
				lua_setfield(L, -2, "storage_mt");
				lua_pop(L, 2);

				/*
				contract_mt = {	  
						__index = glua_core_lib_contract_metatable_index,
						__newindex = glua_core_lib_contract_metatable_newindex
					}
				*/
				lua_createtable(L, 0, 0);
				lua_pushcfunction(L, &glua_core_lib_contract_metatable_index);
				lua_setfield(L, -2, "__index");
				lua_pushcfunction(L, &glua_core_lib_contract_metatable_newindex);
				lua_setfield(L, -2, "__newindex");
				lua_setglobal(L, "contract_mt");

				// add_global_c_function(L, "pairsByKeys", &glua_core_lib_pairs_by_keys);

				/*
				__old_pairs = pairs
				pairs = pairsByKeys
				*/
				lua_getglobal(L, "pairs");
				lua_setglobal(L, "__old_pairs");
				lua_pushcfunction(L, &glua_core_lib_pairs_by_keys);
				lua_setglobal(L, "pairs");
				
				// TODO: 用lightuserdata重构合约的storage
				/*
				const char *init_code = R"END(

				)END";
                luaL_dostring(L, init_code);
				*/

				/*
				lua_pushnil(L);
				lua_setglobal(L, "glua_core_lib_contract_metatable_index");
				lua_pushnil(L);
				lua_setglobal(L, "glua_core_lib_storage_metatable_index");
				lua_pushnil(L);
				lua_setglobal(L, "glua_core_lib_storage_metatable_new_index");
				lua_pushnil(L);
				lua_setglobal(L, "glua_core_lib_contract_metatable_newindex");
				*/
				reset_lvm_instructions_executed_count(L);
                lua_atpanic(L, panic_message);
                if (use_contract)
                {
                    add_global_c_function(L, "transfer_from_contract_to_address", transfer_from_contract_to_address);
					add_global_c_function(L, "transfer_from_contract_to_public_account", transfer_from_contract_to_public_account);
                    add_global_c_function(L, "get_contract_balance_amount", get_contract_balance_amount);
                    add_global_c_function(L, "get_chain_now", get_chain_now);
                    add_global_c_function(L, "get_chain_random", get_chain_random);
                    add_global_c_function(L, "get_current_contract_address", get_contract_address_lua_api);
                    add_global_c_function(L, "get_transaction_id", get_transaction_id);
                    add_global_c_function(L, "get_header_block_num", get_header_block_num);
                    add_global_c_function(L, "wait_for_future_random", wait_for_future_random);
                    add_global_c_function(L, "get_waited", get_waited_block_random);
                    add_global_c_function(L, "get_transaction_fee", get_transaction_fee);
                    add_global_c_function(L, "emit", emit_thinkyoung_event);
                }
                return L;
            }

            bool commit_storage_changes(lua_State *L)
            {
                if (!thinkyoung::lua::api::global_glua_chain_api->has_exception(L))
                {
                    return luaL_commit_storage_changes(L);
                }
                return false;
            }

            void close_lua_state(lua_State *L)
            {
                luaL_commit_storage_changes(L);
				thinkyoung::lua::api::global_glua_chain_api->release_objects_in_pool(L);
                LStatesMap *states_map = get_lua_states_value_hashmap();
                if (nullptr != states_map)
                {
                    auto lua_table_map_list_p = get_lua_state_value(L, LUA_TABLE_MAP_LIST_STATE_MAP_KEY).pointer_value;
                    if (nullptr != lua_table_map_list_p)
                    {
                        auto list_p = (std::list<GluaTableMapP>*) lua_table_map_list_p;
                        for (auto it = list_p->begin(); it != list_p->end(); ++it)
                        {
                            GluaTableMapP lua_table_map = *it;
                            // lua_table_map->~GluaTableMap();
                            // lua_free(L, lua_table_map);
                            delete lua_table_map;
                        }
                        delete list_p;
                    }

                    L_V1 map = create_value_map_for_lua_state(L);
                    for (auto it = map->begin(); it != map->end(); ++it)
                    {
                        if (it->second.type == LUA_STATE_VALUE_INT_POINTER)
                        {
                            lua_free(L, it->second.value.int_pointer_value);
                            it->second.value.int_pointer_value = nullptr;
                        }
                    }
                    // close values in state values(some pointers need free), eg. storage infos, contract infos
                    GluaStateValueNode storage_changelist_node = get_lua_state_value_node(L, LUA_STORAGE_CHANGELIST_KEY);
                    if (storage_changelist_node.type == LUA_STATE_VALUE_POINTER && nullptr != storage_changelist_node.value.pointer_value)
                    {
                        GluaStorageChangeList *list = (GluaStorageChangeList*)storage_changelist_node.value.pointer_value;
                        for (auto it = list->begin(); it != list->end(); ++it)
                        {
                            GluaStorageValue before = it->before;
                            GluaStorageValue after = it->after;
                            if (lua_storage_is_table(before.type))
                            {
                                // free_lua_table_map(L, before.value.table_value);
                            }
                            if (lua_storage_is_table(after.type))
                            {
                                // free_lua_table_map(L, after.value.table_value);
                            }
                        }
                        list->~GluaStorageChangeList();
                        lua_free(L, list);
                    }

                    GluaStateValueNode storage_table_read_list_node = get_lua_state_value_node(L, LUA_STORAGE_READ_TABLES_KEY);
                    if (storage_table_read_list_node.type == LUA_STATE_VALUE_POINTER && nullptr != storage_table_read_list_node.value.pointer_value)
                    {
                        GluaStorageTableReadList *list = (GluaStorageTableReadList*)storage_table_read_list_node.value.pointer_value;
                        list->~GluaStorageTableReadList();
                        lua_free(L, list);
                    }

                    GluaStateValueNode repl_state_node = get_lua_state_value_node(L, LUA_REPL_RUNNING_STATE_KEY);
                    if (repl_state_node.type == LUA_STATE_VALUE_INT_POINTER)
                    {
                        lua_free(L, repl_state_node.value.int_pointer_value);
                    }
                    int *insts_executed_count = get_lua_state_value(L, INSTRUCTIONS_EXECUTED_COUNT_LUA_STATE_MAP_KEY).int_pointer_value;
                    if (nullptr != insts_executed_count)
                    {
                        lua_free(L, insts_executed_count);
                    }
                    int *stopped_pointer = thinkyoung::lua::lib::get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
                    if (nullptr != stopped_pointer)
                    {
                        lua_free(L, stopped_pointer);
                    }
                    auto repl_running_node = get_lua_state_value_node(L, LUA_REPL_RUNNING_STATE_KEY);
                    if (repl_running_node.type == LUA_STATE_VALUE_INT_POINTER && nullptr != repl_running_node.value.int_pointer_value)
                    {
                        lua_free(L, repl_running_node.value.int_pointer_value);
                    }

                    states_map->erase(L);
                }

                lua_close(L);
            }

            /**
            * share some values in L
            */
            void close_all_lua_state_values()
            {
                LStatesMap *states_map = get_lua_states_value_hashmap();
                states_map->clear();
            }
            void close_lua_state_values(lua_State *L)
            {
                LStatesMap *states_map = get_lua_states_value_hashmap();
                states_map->erase(L);
            }

            GluaStateValueNode get_lua_state_value_node(lua_State *L, const char *key)
            {
                GluaStateValue nil_value = { 0 };
                GluaStateValueNode nil_value_node;
                nil_value_node.type = LUA_STATE_VALUE_nullptr;
                nil_value_node.value = nil_value;
                if (nullptr == L || nullptr == key || strlen(key) < 1)
                {
                    return nil_value_node;
                }

                LStatesMap *states_map = get_lua_states_value_hashmap();
                L_V1 map = create_value_map_for_lua_state(L);
                std::string key_str(key);
                auto it = map->find(key_str);
                if (it == map->end())
                    return nil_value_node;
                else
                    return map->at(key_str);
            }

            GluaStateValue get_lua_state_value(lua_State *L, const char *key)
            {
                return get_lua_state_value_node(L, key).value;
            }
            void set_lua_state_instructions_limit(lua_State *L, int limit)
            {
                GluaStateValue value = { limit };
                set_lua_state_value(L, INSTRUCTIONS_LIMIT_LUA_STATE_MAP_KEY, value, LUA_STATE_VALUE_INT);
            }

            int get_lua_state_instructions_limit(lua_State *L)
            {
                return get_lua_state_value(L, INSTRUCTIONS_LIMIT_LUA_STATE_MAP_KEY).int_value;
            }

            int get_lua_state_instructions_executed_count(lua_State *L)
            {
                int *insts_executed_count = get_lua_state_value(L, INSTRUCTIONS_EXECUTED_COUNT_LUA_STATE_MAP_KEY).int_pointer_value;
                if (nullptr == insts_executed_count)
                {
                    return 0;
                }
                if (*insts_executed_count < 0)
                    return 0;
                else
                    return *insts_executed_count;
            }

            void enter_lua_sandbox(lua_State *L)
            {
                GluaStateValue value;
                value.int_value = 1;
                set_lua_state_value(L, LUA_IN_SANDBOX_STATE_KEY, value, LUA_STATE_VALUE_INT);
            }

            void exit_lua_sandbox(lua_State *L)
            {
                GluaStateValue value;
                value.int_value = 0;
                set_lua_state_value(L, LUA_IN_SANDBOX_STATE_KEY, value, LUA_STATE_VALUE_INT);
            }

            bool check_in_lua_sandbox(lua_State *L)
            {
                return get_lua_state_value_node(L, LUA_IN_SANDBOX_STATE_KEY).value.int_value > 0;
            }

            /**
            * notify lvm to stop running the lua stack
            */
            void notify_lua_state_stop(lua_State *L)
            {
                int *pointer = get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
                if (nullptr == pointer)
                {
                    pointer = (int*)lua_malloc(L, sizeof(int));
                    *pointer = 1;
                    GluaStateValue value;
                    value.int_pointer_value = pointer;
                    set_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY, value, LUA_STATE_VALUE_INT_POINTER);
                }
                else
                {
                    *pointer = 1;
                }
            }

            /**
            * check whether the lua state notified stop before
            */
            bool check_lua_state_notified_stop(lua_State *L)
            {
                int *pointer = get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
                if (nullptr == pointer)
                    return false;
                return (*pointer) > 0;
            }

            /**
            * resume lua_State to be available running again
            */
            void resume_lua_state_running(lua_State *L)
            {
                int *pointer = get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
                if (nullptr != pointer)
                {
                    *pointer = 0;
                }
            }

            void set_lua_state_value(lua_State *L, const char *key, GluaStateValue value, enum GluaStateValueType type)
            {
                if (nullptr == L || nullptr == key || strlen(key) < 1)
                {
                    return;
                }

                L_V1 map = create_value_map_for_lua_state(L);
                GluaStateValueNode node_v;
                node_v.type = type;
                node_v.value = value;
                if (node_v.type == LUA_STATE_VALUE_STRING)
                    node_v.value.string_value = thinkyoung::lua::lib::malloc_and_copy_string(L, value.string_value);
                std::string key_str(key);
                map->erase(key_str);
                map->insert(std::make_pair(key_str, node_v));
            }

            static const char* reader_of_stream(lua_State *L, void *ud, size_t *size)
            {
                UNUSED(L);
                GluaModuleByteStream *stream = static_cast<GluaModuleByteStream*>(ud);
                if (!stream)
                    return nullptr;
				if (size)
					*size = stream->buff.size();
                return stream->buff.data();
            }

            LClosure* luaU_undump_from_file(lua_State *L, const char *binary_filename, const char* name)
            {
                ZIO z;
                FILE *f = fopen(binary_filename, "rb");
                if (nullptr == f)
                    return nullptr;
				auto stream = std::make_shared<GluaModuleByteStream>();
                if (nullptr == stream)
                    return nullptr;
				auto f_cur = ftell(f);
				fseek(f, 0, SEEK_END);
				auto f_size = ftell(f);
				fseek(f, f_cur, 0);
				stream->buff.resize(stream->buff.size() + f_size);
                fread(stream->buff.data(), f_size, 1, f);
                fseek(f, 0, SEEK_END); // seek to end of file
                stream->is_bytes = true;
                luaZ_init(L, &z, reader_of_stream, (void*)stream.get());
                luaZ_fill(&z);
                LClosure *closure = luaU_undump(L, &z, name);
                fclose(f);
                return closure;
            }

            void free_bytecode_stream(GluaModuleByteStreamP stream)
            {
				/*
                if (nullptr != stream)
                {
					stream->contract_apis.clear();
					stream->contract_apis_count = 0;
					stream->offline_apis.clear();
					stream->offline_apis_count = 0;
					stream->contract_emit_events.clear();
					stream->contract_emit_events_count = 0;
                    free(stream);
                }
				*/
				delete stream;
            }

            LClosure *luaU_undump_from_stream(lua_State *L, GluaModuleByteStream *stream, const char *name)
            {
                ZIO z;
                luaZ_init(L, &z, reader_of_stream, (void*) stream);
                luaZ_fill(&z);
                auto cl = luaU_undump(L, &z, name);
				return cl;
            }

			bool undump_from_bytecode_stream_to_file(lua_State *L, GluaModuleByteStream *stream, FILE *out)
            {
				LClosure *closure = luaU_undump_from_stream(L, stream, "undump_tmp");
				if (!closure)
					return false;
				luaL_PrintFunctionToFile(out, closure->p, 1);
				return true;
            }

			bool undump_from_bytecode_file_to_file(lua_State *L, const char *bytecode_filename, FILE *out)
            {
				LClosure *closure = luaU_undump_from_file(L, bytecode_filename, "undump_tmp");
				if (!closure)
					return false;
				luaL_PrintFunctionToFile(out, closure->p, 1);
				return true;
            }


#define UPVALNAME_OF_PROTO(proto, x) (((proto)->upvalues[x].name) ? getstr((proto)->upvalues[x].name) : "-")
#define MYK(x)		(-1-(x))

            static const size_t globalvar_whitelist_count = sizeof(globalvar_whitelist) / sizeof(globalvar_whitelist[0]);


            static const std::string TYPED_LUA_LIB_CODE = R"END(type Contract<S> = {
	id: string,
	name: string,
	storage: S
}
)END";

            const std::string get_typed_lua_lib_code()
            {
                return TYPED_LUA_LIB_CODE;
            }

			std::string pre_modify_lua_source_code_string(lua_State *L, std::string source_filepath,
				bool create_debug_file, std::string &source_code, char *error, bool *changed,
				bool use_type_check, bool include_typed_lua_lib, GluaModuleByteStream *stream,
				bool throw_exception, bool in_repl, bool is_contract)
            {
				std::string code = source_code;
				auto origin_code = code;

				if (use_type_check)
				{
					using namespace glua::parser;
					ParserContext ctx;
					auto chunk_p = ctx.generate_glua_parser();
					auto token_parser = std::make_shared<GluaTokenParser>(L);
					if (include_typed_lua_lib)
					{
						ctx.set_inner_lib_code_lines(7); // lib code lines
						code = get_typed_lua_lib_code() + code;
					}
					try
					{
						token_parser->parse(code);
					}
					catch (std::exception e) 
					{
						auto parse_tokens_error = e.what();
						if (strlen(parse_tokens_error) > 0)
						{
							std::string error_str = std::string("error in line ")
								+ std::to_string(token_parser->current_parsing_linenumber() - ctx.inner_lib_code_lines() + 2) + ", " + parse_tokens_error;
							if (throw_exception)
								lcompile_error_set(L, error, "syntax parser error %s", error_str.c_str());
							if (changed)
								*changed = false;
							return origin_code;
						}
					}
                    auto parser_input=token_parser.get();
                    auto vparser=glua::parser::Input(parser_input);
					auto pr = ctx.parse_syntax(vparser);
					if (pr.state() != glua::parser::ParserState::SUCCESS)
					{
						if (throw_exception) {
							std::string error_in_ctx = ctx.get_error() + "\n" + ctx.simple_token_error();
							lcompile_error_set(L, error, "syntax parser error %s", error_in_ctx.c_str());
						}
						if (changed)
							*changed = false;
						return origin_code;
					}
					GluaTypeChecker type_checker(&ctx);
					GluaTypeInfoP contract_storage_type = nullptr;
					GluaTypeInfoP contract_type = nullptr;
					if (in_repl)
						type_checker.set_in_repl(in_repl);
					bool check_syntax_has_errors = is_contract ? type_checker.check_contract_syntax_tree_type(pr.result(), &contract_type, &contract_storage_type) : type_checker.check_syntax_tree_type(pr.result());

					const char *error_str = "syntax tree type check error: ";
					std::stringstream ss;
					ss << error_str << "\n"; 

					bool has_contract_import_error = false;
					if(is_contract)
					{
						for(const auto &p : type_checker.get_imported_contracts())
						{
							if(!thinkyoung::lua::api::global_glua_chain_api->check_contract_exist(L, p.first.c_str()))
							{
								has_contract_import_error = true;
								ss << "import_contract error in line " << p.second << ", contract " << p.first << " not found\n";
							}
						}
					}
					if (!check_syntax_has_errors || type_checker.has_error() || has_contract_import_error)
					{
						for (const auto & item : type_checker.errors())
							ss << item.second << "\n";
						if(throw_exception)
							thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, ss.str().c_str());
						if (error && throw_exception)
						{
							lcompile_error_set(L, error, ss.str().c_str());
						}
						if (changed)
							*changed = false;
						return origin_code;
					}
					code = type_checker.dump();
					if (create_debug_file)
					{
						std::string ldf_filename = source_filepath + ".ldf";
						FILE *ldf_file = fopen(ldf_filename.c_str(), "w");
						if (ldf_file)
						{
							type_checker.dump_ldf_to_file(ldf_file);
							fclose(ldf_file);
						}
					}

					if(stream)
					{
						auto emit_events = type_checker.get_emit_event_types();
						stream->contract_emit_events.clear();
						for(const auto &event_name : emit_events)
						{
							stream->contract_emit_events.push_back(event_name);
						}
						if(is_contract && contract_storage_type)
						{
							try
							{
								contract_storage_type->put_contract_storage_type_to_module_stream(stream);
								contract_storage_type->put_contract_apis_info_to_module_stream(stream);
							} catch(glua::core::GluaException const &e)
							{
								if (throw_exception)
									thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, e.what());
								if (error && throw_exception)
								{
									lcompile_error_set(L, error, e.what());
								}
							}
						}
					}
				}
				else
				{
					// TODO: 如果用lua语法的话，也要抽取出emit(EventName, EventArg)中的eventName列表
				}


				auto token_parser = std::make_shared<glua::parser::GluaTokenParser>(L);
				bool code_changed = false;
				try
				{
					token_parser->parse(code);
					for (auto it = token_parser->begin(); it != token_parser->end(); ++it)
					{
						if (it->type == glua::parser::LTK_NAME && it->token.length() > LUA_MAX_LOCALVARNAME_LENGTH)
						{
							const char *error_format = "too long local variable name %s, limit length %d";
							if(throw_exception)
								lcompile_error_set(L, error, error_format, it->token.c_str(), LUA_MAX_LOCALVARNAME_LENGTH);
							if (changed)
								*changed = false;
							return origin_code;
						}
						if (it->type == glua::parser::LTK_NAME && (it->token == "import_contract" || it->token == "import_contract_from_address"))
						{
							// TODO: check next is string	or '(' + string + ')'
							++it;
							if (it == token_parser->end())
							{
								if(throw_exception)
									lcompile_error_set(L, error, "unfinished import_contract statement");
								if (changed)
									*changed = false;
								return origin_code;
							}
							bool hasXkh = it->type == '(';
							if (hasXkh)
								++it;
							if (it == token_parser->end() || it->type != glua::parser::LTK_STRING || it->token.length() < 1)
							{
								if(throw_exception)
									lcompile_error_set(L, error, "import_contract only accept literal not empty string, but accept %s", it->token.c_str());
								if (changed)
									*changed = false;
								return origin_code;
							}
						}
					}
					code = token_parser->dump();
					token_parser->reset_position();
					// find offline keyword and use M.locals = M.locals or {}; M.locals[#M.locals+1]='<api_name>';
					std::vector<glua::parser::GluaParserToken> new_tokens_after_offline;
					auto offline_it = token_parser->begin();
					size_t line_added = 0;

					while (offline_it != token_parser->end())
					{
						offline_it->linenumber += line_added;
						if (offline_it->type == glua::parser::LTK_OFFLINE)
						{
							if (token_parser->has_n_tokens_after(4))
							{
								// replace code tokens
								++offline_it;
								auto func_token = *offline_it;
								func_token.linenumber += line_added;
								++offline_it;
								auto module_var_token = *offline_it;
								module_var_token.linenumber += line_added;
								++offline_it;
								auto splitter_token = *offline_it;
								++offline_it;
								auto api_name_token = *offline_it;
								api_name_token.linenumber += line_added;
								++offline_it;
								glua::parser::GluaParserToken	dot_token;
								dot_token.token = ".";
								dot_token.linenumber = func_token.linenumber;
								dot_token.position = func_token.position;
								dot_token.source_token = ".";
								dot_token.type = (enum glua::parser::TOKEN_RESERVED)'.';
								glua::parser::GluaParserToken locals_token;
								locals_token.token = "locals";
								locals_token.linenumber = func_token.linenumber;
								locals_token.position = func_token.position;
								locals_token.source_token = "locals";
								locals_token.type = glua::parser::TOKEN_RESERVED::LTK_NAME;
								new_tokens_after_offline.push_back(module_var_token);
								new_tokens_after_offline.push_back(dot_token);
								new_tokens_after_offline.push_back(locals_token);
								glua::parser::GluaParserToken assign_token;
								assign_token.linenumber = func_token.linenumber;
								assign_token.position = func_token.position;
								assign_token.token = "=";
								assign_token.source_token = "=";
								assign_token.type = (enum glua::parser::TOKEN_RESERVED)'=';
								new_tokens_after_offline.push_back(assign_token);
								new_tokens_after_offline.push_back(module_var_token);
								new_tokens_after_offline.push_back(dot_token);
								new_tokens_after_offline.push_back(locals_token);
								glua::parser::GluaParserToken or_token;
								or_token.token = "or";
								or_token.linenumber = func_token.linenumber;
								or_token.position = func_token.position;
								or_token.source_token = "or";
								or_token.type = glua::parser::TOKEN_RESERVED::LTK_OR;
								new_tokens_after_offline.push_back(or_token);
								glua::parser::GluaParserToken ldk_token;
								ldk_token.linenumber = func_token.linenumber;
								ldk_token.position = func_token.position;
								ldk_token.token = "{";
								ldk_token.source_token = "{";
								ldk_token.type = (enum glua::parser::TOKEN_RESERVED)'{';
								glua::parser::GluaParserToken rdk_token;
								rdk_token.linenumber = func_token.linenumber;
								rdk_token.position = func_token.position;
								rdk_token.token = "}";
								rdk_token.source_token = "}";
								rdk_token.type = (enum glua::parser::TOKEN_RESERVED)'}';
								new_tokens_after_offline.push_back(ldk_token);
								new_tokens_after_offline.push_back(rdk_token);
								++line_added;
								func_token.linenumber += 1;
								module_var_token.linenumber += 1;
								splitter_token.linenumber += 1;
								dot_token.linenumber += 1;
								api_name_token.linenumber += 1;
								locals_token.linenumber += 1;
								assign_token.linenumber += 1;
								new_tokens_after_offline.push_back(module_var_token);
								new_tokens_after_offline.push_back(dot_token);
								new_tokens_after_offline.push_back(locals_token);
								glua::parser::GluaParserToken lzk_token;
								lzk_token.linenumber = module_var_token.linenumber;
								lzk_token.position = module_var_token.position;
								lzk_token.token = "[";
								lzk_token.source_token = "[";
								lzk_token.type = (enum glua::parser::TOKEN_RESERVED)'[';
								glua::parser::GluaParserToken rzk_token;
								rzk_token.linenumber = module_var_token.linenumber;
								rzk_token.position = module_var_token.position;
								rzk_token.token = "]";
								rzk_token.source_token = "]";
								rzk_token.type = (enum glua::parser::TOKEN_RESERVED)']';
								glua::parser::GluaParserToken len_token;
								len_token.linenumber = module_var_token.linenumber;
								len_token.position = module_var_token.position;
								len_token.token = "#";
								len_token.source_token = "#";
								len_token.type = (enum glua::parser::TOKEN_RESERVED)'#';
								glua::parser::GluaParserToken add_token;
								add_token.linenumber = module_var_token.linenumber;
								add_token.position = module_var_token.position;
								add_token.token = "+";
								add_token.source_token = "+";
								add_token.type = (enum glua::parser::TOKEN_RESERVED)'+';
								glua::parser::GluaParserToken n1_token;
								n1_token.linenumber = module_var_token.linenumber;
								n1_token.position = module_var_token.position;
								n1_token.token = "1";
								n1_token.source_token = "1";
								n1_token.type = glua::parser::LTK_INT;

								new_tokens_after_offline.push_back(lzk_token);
								new_tokens_after_offline.push_back(len_token);
								new_tokens_after_offline.push_back(module_var_token);
								new_tokens_after_offline.push_back(dot_token);
								new_tokens_after_offline.push_back(locals_token);
								new_tokens_after_offline.push_back(add_token);
								new_tokens_after_offline.push_back(n1_token);
								new_tokens_after_offline.push_back(rzk_token);
								new_tokens_after_offline.push_back(assign_token);
								glua::parser::GluaParserToken api_name_str_token;
								api_name_str_token.linenumber = module_var_token.linenumber;
								api_name_str_token.position = module_var_token.position;
								api_name_str_token.source_token = std::string("'") + api_name_token.token + "'";
								api_name_str_token.token = api_name_str_token.source_token;
								api_name_str_token.type = glua::parser::LTK_STRING;
								new_tokens_after_offline.push_back(api_name_str_token);
								++line_added;
								func_token.linenumber += 1;
								module_var_token.linenumber += 1;
								splitter_token.linenumber += 1;
								api_name_token.linenumber += 1;
								new_tokens_after_offline.push_back(func_token);
								new_tokens_after_offline.push_back(module_var_token);
								new_tokens_after_offline.push_back(splitter_token);
								new_tokens_after_offline.push_back(api_name_token);
								code_changed = true;
								continue;
							}
							else
							{
								if(throw_exception)
									lcompile_error_set(L, error, "offline function <M>[.:]<api_name> syntax error");
								if (changed)
									*changed = false;
								return origin_code;
							}
						}
						else
							new_tokens_after_offline.push_back(*offline_it);
						++offline_it;
					}
					if (code_changed)
					{
						token_parser->replace_all_tokens(new_tokens_after_offline);
						code = token_parser->dump();
					}
				}
				catch (std::exception e)
				{
					thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_PARSER_ERROR, e.what());
					if (changed)
						*changed = false;
					return origin_code;
				}

				if (changed)
					*changed = true;
				return code;
            }

			std::string pre_modify_lua_source_code(lua_State *L, std::string source_filepath, char *error,
				bool *changed, bool use_type_check, GluaModuleByteStreamP stream, bool is_contract)
			{
				std::ifstream in(source_filepath, std::ios::in);
				if (!in.is_open())
				{
					if (changed)
						*changed = false;
					return "";
				}
				std::string code((std::istreambuf_iterator<char>(in)),
					(std::istreambuf_iterator<char>()));
				in.close();
				return pre_modify_lua_source_code_string(L, source_filepath, true, code, error, changed, use_type_check, true, stream, true, false, is_contract);
			}

            static bool upval_defined_in_parent(lua_State *L, Proto *parent, std::list<Proto*> *all_protos, Upvaldesc upvaldesc)
            {
                // whether the upval defined in parent proto
                if (!L || !parent)
                    return false;
                if (upvaldesc.instack > 0)
                {
                    if (upvaldesc.idx < parent->sizelocvars)
                    {
                        return true;
                    }
                    else
                        return false;
                }
                else
                {
                    if (!all_protos || all_protos->size() < 1)
                        return false;
                    if (upvaldesc.idx < parent->sizeupvalues)
                    {
                        Upvaldesc parent_upvaldesc = parent->upvalues[upvaldesc.idx];
                        for (const auto &pp : *all_protos)
                        {
                            std::list<Proto*> remaining;
                            for (const auto &pp2 : *all_protos)
                            {
                                if (remaining.size() >= all_protos->size() - 1)
                                    break;
                                remaining.push_back(pp2);
                            }
                            return upval_defined_in_parent(L, pp, &remaining, parent_upvaldesc);
                        }
                        return false;
                    }
                    else
                        return false;
                }
            }

			std::string decompile_bytecode_to_source_code(lua_State *L, GluaModuleByteStream *stream,
				std::string module_name, char *error)
            {
				LClosure *closure = luaU_undump_from_stream(L, stream, module_name.c_str());
				if (!closure)
				{
					if (error)
					{
						const char *msg = "load bytecode error";
						memcpy(error, msg, sizeof(char) * (strlen(msg) + 1));
					}
					return "load bytecode error";
				}
				auto decompile_ctx = std::make_shared<glua::decompile::GluaDecompileContext>();
				return glua::decompile::luaU_decompile(decompile_ctx, closure->p, 0);
            }

			std::string decompile_bytecode_file_to_source_code(lua_State *L, std::string bytecode_filepath,
				std::string module_name, char *error)
            {
				LClosure *closure = luaU_undump_from_file(L, bytecode_filepath.c_str(), module_name.c_str());
				if (!closure)
				{
					if (error)
					{
						const char *msg = "load bytecode error";
						memcpy(error, msg, sizeof(char) * (strlen(msg) + 1));
					}
					return "load bytecode error";
				}
                auto decompile_ctx = std::make_shared<glua::decompile::GluaDecompileContext>();
				return glua::decompile::luaU_decompile(decompile_ctx, closure->p, 0);
            }

			std::string disassemble_bytecode(lua_State *L, GluaModuleByteStream *stream, std::string module_name, char *error)
            {
				LClosure *closure = luaU_undump_from_stream(L, stream, module_name.c_str());
				if (!closure)
				{
					if (error)
					{
						const char *msg = "load bytecode error";
						memcpy(error, msg, sizeof(char) * (strlen(msg) + 1));
					}
					return "load bytecode error";
				}
                auto decompile_ctx = std::make_shared<glua::decompile::GluaDecompileContext>();
				return glua::decompile::luadec_disassemble(decompile_ctx, closure->p, 1, "tmp");
            }

			std::string disassemble_bytecode_file(lua_State *L, std::string bytecode_filepath, std::string module_name, char *error)
            {
				LClosure *closure = luaU_undump_from_file(L, bytecode_filepath.c_str(), module_name.c_str());
				if (!closure)
				{
					if (error)
					{
						const char *msg = "load bytecode error";
						memcpy(error, msg, sizeof(char) * (strlen(msg) + 1));
					}
					return "load bytecode error";
				}
                auto decompile_ctx = std::make_shared<glua::decompile::GluaDecompileContext>();
				return glua::decompile::luadec_disassemble(decompile_ctx, closure->p, 1, "tmp");
            }

            bool check_contract_proto(lua_State *L, Proto *proto, char *error, std::list<Proto*> *parents)
            {
                // for all sub function in proto, check whether the contract bytecode meet our provision
                int i, proto_size = proto->sizep;
                // int const_size = proto->sizek;
                int localvar_size = proto->sizelocvars;
                int upvalue_size = proto->sizeupvalues;

                if (localvar_size > LUA_FUNCTION_MAX_LOCALVARS_COUNT)
                {
                    lcompile_error_set(L, error, "too many local vars in function, limit is %d", LUA_FUNCTION_MAX_LOCALVARS_COUNT);
                    return false;
                }

                std::list<std::string> localvars;
                for (i = 0; i < localvar_size; i++)
                {
                    // int startpc = proto->locvars[i].startpc;
                    // int endpc = proto->locvars[i].endpc;
                    std::string localvarname(getstr(proto->locvars[i].varname));
                }

                std::list<std::tuple<std::string, int, int>> upvalues;
                for (i = 0; i < upvalue_size; i++)
                {
                    std::string upvalue_name(UPVALNAME_OF_PROTO(proto, i));
                    int instack = proto->upvalues[i].instack;
                    int idx = proto->upvalues[i].idx;
                    upvalues.push_back(std::make_tuple(upvalue_name, instack, idx));
                }


                const Instruction* code = proto->code;
                int pc;
                int code_size = proto->sizecode;

				bool is_importing_contract = false;
				bool is_importing_contract_address = false;

                for (pc = 0; pc < code_size; pc++)
                {
                    Instruction i = code[pc];
                    OpCode o = GET_OPCODE(i);
                    int a = GETARG_A(i);
                    int b = GETARG_B(i);
                    int c = GETARG_C(i);
                    int ax = GETARG_Ax(i);
                    int bx = GETARG_Bx(i);
                    int sbx = GETARG_sBx(i);
                    int line = getfuncline(proto, pc);

					if(is_importing_contract)
					{
						is_importing_contract = false;
						// 检查接下来是否是LOADK常量字符串且这个合约名存在
						if(getOpMode(o) == OP_LOADK)
						{
							int idx = MYK(INDEXK(bx));
							int idx_in_kst = -idx - 1;
							if (idx_in_kst >= 0 && idx_in_kst < proto->sizek)
							{
								const char *contract_name = getstr(tsvalue(&proto->k[idx_in_kst]));
								if (contract_name && !thinkyoung::lua::api::global_glua_chain_api->check_contract_exist(L, contract_name))
								{
									lcompile_error_set(L, error, "Can't find contract %s", contract_name);
									return false;
								}
							}
						}
					}
					else if(is_importing_contract_address)
					{
						is_importing_contract_address = false;
						// 检查接下来是否是LOADK常量字符串且这个合约地址存在
						if (getOpMode(o) == OP_LOADK)
						{
							int idx = MYK(INDEXK(bx));
							int idx_in_kst = -idx - 1;
							if (idx_in_kst >= 0 && idx_in_kst < proto->sizek)
							{
								const char *contract_address = getstr(tsvalue(&proto->k[idx_in_kst]));
								if (contract_address && !thinkyoung::lua::api::global_glua_chain_api->check_contract_exist_by_address(L, contract_address))
								{
									lcompile_error_set(L, error, "Can't find contract address %s", contract_address);
									return false;
								}
							}
						}
					}

                    switch (getOpMode(o))
                    {
                    case iABC:
                        break;
                    case iABx:
                        break;
                    case iAsBx:
                        break;
                    case iAx:
                        break;
                    }
                    switch (o)
                    {
                    case OP_GETUPVAL:
                    {
                        // break; // FIXME: this has BUG
                        // FIXME: when instack=1, find in parent localvars, when instack=0, find in parent upval pool
                        if (a == 0)
                            break;
                        // const char *upvalue_name = UPVALNAME_OF_PROTO(proto, a);
                        const char *upvalue_name = UPVALNAME_OF_PROTO(proto, b);
                        int cidx = MYK(INDEXK(b));
                        if (nullptr == proto->k)
                            break;
                        // const char *cname = getstr(tsvalue(&proto->k[-cidx-1]));
                        const char *cname = upvalue_name;
                        bool in_whitelist = false;
                        for (size_t i = 0; i < globalvar_whitelist_count; ++i)
                        {
                            if (strcmp(cname, globalvar_whitelist[i]) == 0)
                            {
                                in_whitelist = true;
                                break;
                            }
                        }
                        if (strcmp(upvalue_name, "_ENV") == 0)
                        {
                            in_whitelist = true; // TODO: whether this can do? maybe need to get what property are fetching
                        }
                        if (!in_whitelist)
                        {
                            Upvaldesc upvaldesc = proto->upvalues[c];
                            // check in parent proto, whether defined in parent proto
                            bool upval_defined = (parents && parents->size() > 0) ? upval_defined_in_parent(L, *parents->rbegin(), parents, upvaldesc) : false;
                            if (!upval_defined)
                            {
                                lcompile_error_set(L, error, "use global variable %s not in whitelist", cname);
                                return false;
                            }
                        }
                    }	 break;
                    case OP_SETUPVAL:
                    {
                        const char *upvalue_name = UPVALNAME_OF_PROTO(proto, b);
                        // not support change _ENV or _G
                        if (strcmp("_ENV", upvalue_name) == 0
                            || strcmp("_G", upvalue_name) == 0)
                        {
                            lcompile_error_set(L, error, "_ENV or _G set %s is forbidden", upvalue_name);
                            return false;
                        }
                        break;
                    }
                    case OP_GETTABUP:
                    {
                        // FIXME
                        const char *upvalue_name = UPVALNAME_OF_PROTO(proto, b);
                        if (ISK(c)){
                            int cidx = MYK(INDEXK(c));
                            const char *cname = getstr(tsvalue(&proto->k[-cidx - 1]));
                            bool in_whitelist = false;
                            for (size_t i = 0; i < globalvar_whitelist_count; ++i)
                            {
                                if (strcmp(cname, globalvar_whitelist[i]) == 0)
                                {
                                    in_whitelist = true;
                                    break;
                                }
                            }
                            if (!in_whitelist && (strcmp(upvalue_name, "_ENV") == 0 || strcmp(upvalue_name, "_G") == 0))
                            {
                                lcompile_error_set(L, error, "use global variable %s not in whitelist", cname);
                                return false;
                            }
							// TODO: 把字节码反编译再检查
							if(strcmp(cname, "import_contract")==0)
								is_importing_contract = true;
							else if (strcmp(cname, "import_contract_address") == 0)
								is_importing_contract_address = true;
                        }
                        else
                        {

                        }
                    }
                    break;
                    case OP_SETTABUP:
                    {
                        const char *upvalue_name = UPVALNAME_OF_PROTO(proto, a);
                        // not support change _ENV or _G
                        if (strcmp("_ENV", upvalue_name) == 0
                            || strcmp("_G", upvalue_name) == 0)
                        {
                            if (ISK(b)){
                                int bidx = MYK(INDEXK(b));
                                const char *bname = getstr(tsvalue(&proto->k[-bidx - 1]));
                                lcompile_error_set(L, error, "_ENV or _G set %s is forbidden", bname);
                                return false;

                            }
                            else
                            {
                                lcompile_error_set(L, error, "_ENV or _G set %s is forbidden", upvalue_name);
                                return false;
                            }
                        }
                        // not support change _ENV or _G
                        /*
                        if (strcmp("_ENV", upvalue_name) == 0
                        || strcmp("_G", upvalue_name) == 0)
                        {
                        lcompile_error_set(L, error, "_ENV or _G set %s is forbidden", upvalue_name);
                        return false;
                        }
                        */
                    }
                    break;
                    default:
                        break;
                    }
                }

                // check sub protos
                for (i = 0; i < proto_size; i++)
                {
                    std::list<Proto*> sub_parents;
                    if (parents)
                    {
                        for (auto it = parents->begin(); it != parents->end(); ++it)
                        {
                            sub_parents.push_back(*it);
                        }
                    }
                    sub_parents.push_back(proto);
                    if (!check_contract_proto(L, proto->p[i], error, &sub_parents)) {
                        return false;
                    }
                }

                return true;
            }

            bool check_contract_bytecode_file(lua_State *L, const char *binary_filename)
            {
                LClosure *closure = luaU_undump_from_file(L, binary_filename, "check_contract");
                if (!closure)
                    return false;
                return check_contract_proto(L, closure->p);
            }

            bool check_contract_bytecode_stream(lua_State *L, GluaModuleByteStream *stream, char *error)
            {
                LClosure *closure = luaU_undump_from_stream(L, stream, "check_contract");
                if (!closure)
                    return false;
                return check_contract_proto(L, closure->p, error);
            }


            bool compilefile_to_file(const char *filename, const char *out_filename, char *error, bool use_type_check)
            {
                return luaL_compilefile(filename, out_filename, error, use_type_check) == 0;
            }
            bool compilefile_to_stream(lua_State *L, const char *filename, GluaModuleByteStreamP stream, char *error, bool use_type_check, bool is_contract)
            {
                return luaL_compilefile_to_stream(L, filename, stream, error, use_type_check, is_contract) == 0;
            }

            bool compile_contract_to_stream(const char *filename, GluaModuleByteStreamP stream,
                char *error, GluaStatePreProcessorFunction *preprocessor, bool use_type_check)
            {
                // check contract requirements
                // must have init function and start function
                GluaStateScope scope;
                lua_State *L = scope.L();
                if (nullptr != preprocessor)
                    (*preprocessor)(L);

                if (thinkyoung::lua::api::global_glua_chain_api->has_exception(L))
                    thinkyoung::lua::api::global_glua_chain_api->clear_exceptions(L);

				struct _saved_out_release_t
				{
					lua_State *_L;
					FILE *_saved_out;
					FILE *_saved_err;
					inline _saved_out_release_t(lua_State *L)
					{
						_L = L;
						_saved_out = L->out;
						L->out = nullptr;
						_saved_err = L->err;
						L->err = nullptr;
					}
					inline ~_saved_out_release_t()
					{
						_L->out = _saved_out;
						_L->err = _saved_err;
					}
				} _saved_out_release(L);

                if (!compilefile_to_stream(L, filename, stream, error, use_type_check, true))
                {
                    if (error && strlen(error) > 0)
                    {
						lua_set_compile_error(L, error);
                        thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, error);
                    }
                    else
                        thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, "compile error");
                    return false;
                }
                if (thinkyoung::lua::api::global_glua_chain_api->has_exception(L))
                    thinkyoung::lua::api::global_glua_chain_api->clear_exceptions(L);
                if (!luaL_get_contract_apis(L, stream, error))
                {
					lcompile_error_get(L, error);
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, "compile error");
                    return false;
                }
                lcompile_error_get(L, error);
				for(size_t i=0;i<stream->contract_apis.size();++i)
				{
					// FIXME: 把id/name/storage这些属性统一用个函数管理起来，不要到处都重复写
					if(stream->contract_apis[i] == "id"
						|| stream->contract_apis[i] == "name"
						|| stream->contract_apis[i] == "storage")
					{
						lua_set_compile_error(L, "contract's id/name/storage property can't be api name");
						lcompile_error_get(L, error);
						thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, error);
						return false;
					}
				}

                run_compiled_bytestream(L, stream);
				if(strlen(L->runerror)>0)
				{
					lcompile_error_get(L, error);
				}
                if (thinkyoung::lua::api::global_glua_chain_api->has_exception(L))
                    return false;

                // check contract bytecode
                if (!check_contract_bytecode_stream(L, stream, error))
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, "compile error when check contract bytecode");
                    return false;
                }
                lua_getglobal(L, "last_return");
                if (!lua_istable(L, -1))
                {
                    lua_pop(L, 1); // pop last_return
                    const char *error_str = "contract code must end with return a module/table";
                    if (error)
                        strcpy(error, error_str);
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_str);
                    return false;
                }
                else
                {
                    // check init and start exist and are functions
                    lua_getfield(L, -1, "init");
                    if (!lua_isfunction(L, -1))
                    {
                        lua_pop(L, 2);// pop last_return, init
                        const char *error_str = "contract must have init function";
                        if (error)
                            strcpy(error, error_str);
                        thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_str);
                        return false;
                    }
                    lua_pop(L, 1); // pop init
                    lua_pop(L, 1); // pop last_return
                    return true;
                }
            }

			std::stack<std::string> *get_using_contract_id_stack(lua_State *L, bool init_if_not_exist)
            {
				std::stack<std::string> *contract_id_stack = nullptr;
				auto contract_id_stack_value_in_state_map = thinkyoung::lua::lib::get_lua_state_value(L, GLUA_CONTRACT_API_CALL_STACK_STATE_MAP_KEY);
				if (!contract_id_stack_value_in_state_map.pointer_value)
				{
					if (!init_if_not_exist)
						return nullptr;
					contract_id_stack = new std::stack<std::string>();
					if (!contract_id_stack)
					{
						lua_set_run_error(L, "allocate contract id stack memory error");
						return nullptr;
					}
					contract_id_stack_value_in_state_map.pointer_value = (void*)contract_id_stack;
					thinkyoung::lua::lib::set_lua_state_value(L, GLUA_CONTRACT_API_CALL_STACK_STATE_MAP_KEY, contract_id_stack_value_in_state_map, GluaStateValueType::LUA_STATE_VALUE_POINTER);
				}
				else
					contract_id_stack = (std::stack<std::string>*) (contract_id_stack_value_in_state_map.pointer_value);
				return contract_id_stack;
            }

			std::string get_current_using_contract_id(lua_State *L)
            {
				auto contract_id_stack = get_using_contract_id_stack(L, true);
				if (!contract_id_stack || contract_id_stack->size()<1)
					return "";
				return contract_id_stack->top();
            }

            GluaTableMapP create_managed_lua_table_map(lua_State *L)
            {
                return luaL_create_lua_table_map_in_memory_pool(L);
            }

            char *malloc_managed_string(lua_State *L, size_t size, const char *init_data)
            {
                char *data = (char *)lua_malloc(L, size);
				memset(data, 0x0, size);
				if(nullptr != init_data)
					memcpy(data, init_data, strlen(init_data));
				return data;
            }

			char *malloc_and_copy_string(lua_State *L, const char *init_data)
            {
				return malloc_managed_string(L, sizeof(char) * (strlen(init_data) + 1), init_data);
            }

            GluaModuleByteStream *malloc_managed_byte_stream(lua_State *L)
            {
                return (GluaModuleByteStream*)lua_calloc(L, 1, sizeof(GluaModuleByteStream));
            }

            bool run_compiledfile(lua_State *L, const char *filename)
            {
                return lua_docompiledfile(L, filename) == 0;
            }
            bool run_compiled_bytestream(lua_State *L, void *stream_addr)
            {
                return lua_docompiled_bytestream(L, stream_addr) == 0;
            }
            void add_global_c_function(lua_State *L, const char *name, lua_CFunction func)
            {
                lua_pushcfunction(L, func);
                lua_setglobal(L, name);
            }
            void add_global_string_variable(lua_State *L, const char *name, const char *str)
            {
                if (!name || !str)
                    return;
                lua_pushstring(L, str);
                lua_setglobal(L, name);
            }
            void add_global_int_variable(lua_State *L, const char *name, lua_Integer num)
            {
                if (!name)
                    return;
                lua_pushinteger(L, num);
                lua_setglobal(L, name);
            }
            void add_global_number_variable(lua_State *L, const char *name, lua_Number num)
            {
                if (!name)
                    return;
                lua_pushnumber(L, num);
                lua_setglobal(L, name);
            }

            void add_global_bool_variable(lua_State *L, const char *name, bool value)
            {
                if (!name)
                    return;
                lua_pushboolean(L, value ? 1 : 0);
                lua_setglobal(L, name);
            }

			std::string get_global_string_variable(lua_State *L, const char *name)
            {
				if (!name)
					return "";
				lua_getglobal(L, name);
				if(lua_isnil(L, -1))
				{
					lua_pop(L, 1);
					return "";
				}
				std::string value(luaL_checkstring(L, -1));
				lua_pop(L, 1);
				return value;
            }

			void remove_global_variable(lua_State *L, const char *name)
            {
				if (!name)
					return;
				lua_pushnil(L);
				lua_setglobal(L, name);
            }

            void register_module(lua_State *L, const char *name, lua_CFunction openmodule_func)
            {
                luaL_requiref(L, name, openmodule_func, 0);
            }

			void add_system_extra_libs(lua_State *L)
            {
				register_module(L, "os", luaopen_os);
				register_module(L, "io", luaopen_io);
				register_module(L, "net", luaopen_net);
				register_module(L, "http", luaopen_http);
				register_module(L, "jsonrpc", luaopen_jsonrpc);
            }

			void reset_lvm_instructions_executed_count(lua_State *L)
            {
				int *insts_executed_count = get_lua_state_value(L, INSTRUCTIONS_EXECUTED_COUNT_LUA_STATE_MAP_KEY).int_pointer_value;
				if (insts_executed_count)
				{
					*insts_executed_count = 0;
				}
            }

            void increment_lvm_instructions_executed_count(lua_State *L, int add_count)
            {
              int *insts_executed_count = get_lua_state_value(L, INSTRUCTIONS_EXECUTED_COUNT_LUA_STATE_MAP_KEY).int_pointer_value;
              if (insts_executed_count)
              {
                *insts_executed_count = *insts_executed_count + add_count;
              }
            }

            int execute_contract_api(lua_State *L, const char *contract_name,
				const char *api_name, const char *arg1, std::string *result_json_string)
            {
                auto contract_address = malloc_managed_string(L, CONTRACT_ID_MAX_LENGTH + 1);
                memset(contract_address, 0x0, CONTRACT_ID_MAX_LENGTH + 1);
                size_t address_size = 0;
                thinkyoung::lua::api::global_glua_chain_api->get_contract_address_by_name(L, contract_name, contract_address, &address_size);
                if (address_size > 0)
                {
                    GluaStateValue value;
                    value.string_value = contract_address;
                    set_lua_state_value(L, STARTING_CONTRACT_ADDRESS, value, LUA_STATE_VALUE_STRING);
                }
                return lua_execute_contract_api(L, contract_name, api_name, arg1, result_json_string);
            }

            LUA_API int execute_contract_api_by_address(lua_State *L, const char *contract_address,
				const char *api_name, const char *arg1, std::string *result_json_string)
            {
                GluaStateValue value;
                auto str = malloc_managed_string(L, strlen(contract_address) + 1);
                memset(str, 0x0, strlen(contract_address) + 1);
                strncpy(str, contract_address, strlen(contract_address));
                value.string_value = str;
                set_lua_state_value(L, STARTING_CONTRACT_ADDRESS, value, LUA_STATE_VALUE_STRING);
                return lua_execute_contract_api_by_address(L, contract_address, api_name, arg1, result_json_string);
            }

            int execute_contract_api_by_stream(lua_State *L, GluaModuleByteStream *stream,
                const char *api_name, const char *arg1, std::string *result_json_string)
            {
                return lua_execute_contract_api_by_stream(L, stream, api_name, arg1, result_json_string);
            }

			bool is_calling_contract_init_api(lua_State *L)
            {
				const auto &state_node = get_lua_state_value_node(L, THINKYOUNG_CONTRACT_INITING);
				return state_node.type == LUA_STATE_VALUE_INT && state_node.value.int_value > 0;
            }

			std::string get_starting_contract_address(lua_State *L)
            {
				auto starting_contract_address_node = thinkyoung::lua::lib::get_lua_state_value_node(L, STARTING_CONTRACT_ADDRESS);
				if (starting_contract_address_node.type == GluaStateValueType::LUA_STATE_VALUE_STRING)
				{
					return starting_contract_address_node.value.string_value;
				}
				else
					return "";
            }

            bool execute_contract_init_by_address(lua_State *L, const char *contract_address, const char *arg1, std::string *result_json_string)
            {
                GluaStateValue state_value;
                state_value.int_value = 1;
                set_lua_state_value(L, THINKYOUNG_CONTRACT_INITING, state_value, LUA_STATE_VALUE_INT);
                int status = execute_contract_api_by_address(L, contract_address, "init", arg1, result_json_string);
                state_value.int_value = 0;
                set_lua_state_value(L, THINKYOUNG_CONTRACT_INITING, state_value, LUA_STATE_VALUE_INT);
                return status == 0;
            }
            bool execute_contract_start_by_address(lua_State *L, const char *contract_address, const char *arg1, std::string *result_json_string)
            {
                return execute_contract_api_by_address(L, contract_address, "start", arg1, result_json_string) == LUA_OK;
            }

            bool execute_contract_init(lua_State *L, const char *name, GluaModuleByteStreamP stream, const char *arg1, std::string *result_json_string)
            {
                GluaStateValue state_value;
                state_value.int_value = 1;
                set_lua_state_value(L, THINKYOUNG_CONTRACT_INITING, state_value, LUA_STATE_VALUE_INT);
                int status = execute_contract_api_by_stream(L, stream, "init", arg1, result_json_string);
                state_value.int_value = 0;
                set_lua_state_value(L, THINKYOUNG_CONTRACT_INITING, state_value, LUA_STATE_VALUE_INT);
                return status == 0;
            }
            bool execute_contract_start(lua_State *L, const char *name, GluaModuleByteStreamP stream, const char *arg1, std::string *result_json_string)
            {
                return execute_contract_api_by_stream(L, stream, "start", arg1, result_json_string) == LUA_OK;
            }

            bool start_repl(lua_State *L)
            {
				// TODO: 把这里的REPL换成main.cpp中的新REPL
                luaL_doREPL(L);
                return true;
            }

            int *get_repl_state(lua_State *L)
            {
                auto node = get_lua_state_value_node(L, LUA_REPL_RUNNING_STATE_KEY);
                if (node.type == LUA_STATE_VALUE_INT_POINTER && nullptr != node.value.int_pointer_value)
                {
                    return node.value.int_pointer_value;
                }
                GluaStateValue value;
                value.int_pointer_value = (int*)lua_malloc(L, sizeof(int));
                *value.int_pointer_value = 0;
                set_lua_state_value(L, LUA_REPL_RUNNING_STATE_KEY, value, LUA_STATE_VALUE_INT_POINTER);
                return value.int_pointer_value;
            }

            bool stop_repl(lua_State *L)
            {
                int * state = get_repl_state(L);
                *state = 0;
                return true;
            }

            bool start_repl_async(lua_State *L)
            {
                int * state = get_repl_state(L);
                if (nullptr == state)
                    return false;
                *state = 1;
                std::thread t(start_repl, L);
                t.detach();
                return true;
            }

            bool check_repl_running(lua_State *L)
            {
                int * state = get_repl_state(L);
                return nullptr != state && *state > 0;
            }

            std::string wrap_contract_name(const char *contract_name)
            {
                if (glua::util::starts_with(contract_name, STREAM_CONTRACT_PREFIX) || glua::util::starts_with(contract_name, ADDRESS_CONTRACT_PREFIX))
                    return std::string(contract_name);
                return std::string(CONTRACT_NAME_WRAPPER_PREFIX) + std::string(contract_name);
            }

            std::string unwrap_any_contract_name(const char *contract_name)
            {
                std::string wrappered_name_str(contract_name);
                if (wrappered_name_str.find(CONTRACT_NAME_WRAPPER_PREFIX) != std::string::npos)
                {
                    return wrappered_name_str.substr(strlen(CONTRACT_NAME_WRAPPER_PREFIX));
                }
                else if (wrappered_name_str.find(ADDRESS_CONTRACT_PREFIX) != std::string::npos)
                    return wrappered_name_str.substr(strlen(ADDRESS_CONTRACT_PREFIX));
                else if (wrappered_name_str.find(STREAM_CONTRACT_PREFIX) != std::string::npos)
                    return wrappered_name_str.substr(strlen(STREAM_CONTRACT_PREFIX));
                else
                    return wrappered_name_str;
            }

            std::string unwrap_contract_name(const char *wrappered_contract_name)
            {
                std::string wrappered_name_str(wrappered_contract_name);
                if (wrappered_name_str.find(CONTRACT_NAME_WRAPPER_PREFIX) != std::string::npos)
                {
                    return wrappered_name_str.substr(strlen(CONTRACT_NAME_WRAPPER_PREFIX));
                }
                else
                    return wrappered_name_str;
            }

            /**
            * only used in contract api directly
            */
            const char *get_contract_id_in_api(lua_State *L)
            {
				const auto &contract_id = get_current_using_contract_id(L);
				auto contract_id_str = malloc_and_copy_string(L, contract_id.c_str());
				return contract_id_str;
				/*
                lua_Debug ar;
                lua_getstack(L, 1, &ar);
                lua_getinfo(L, "Slnu", &ar);
                const char *localname = lua_getlocal(L, &ar, 1);
                if (strcmp("self", localname) == 0 && lua_istable(L, -1))
                {
                    lua_getfield(L, -1, "id");
                    const char *id = lua_tostring(L, -1);
                    lua_pop(L, 2);
                    return id;
                }
                else
                {
                    thinkyoung::lua::api::global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "the api must have first local self, or need be defined as <M>:<api_name>");
                }
                lua_pop(L, 1);
                return nullptr;
				*/
            }

        }
    }
}