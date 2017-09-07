#include <glua/lprefix.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <utility>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>

#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>

#include <glua/glua_lutil.h>
#include <glua/lobject.h>
#include <glua/lzio.h>
#include <glua/lundump.h>
#include <glua/lvm.h>
#include <glua/lapi.h>
#include <glua/lopcodes.h>
#include <glua/lparser.h>
#include <glua/lstate.h>
#include <glua/ldo.h>
#include <glua/ldebug.h>
#include <glua/lauxlib.h>
#include <glua/lualib.h>

using thinkyoung::lua::api::global_glua_chain_api;

namespace thinkyoung
{
    namespace lua
    {
        namespace lib
        {
            GluaStateScope::GluaStateScope(bool use_contract)
                :_use_contract(use_contract) {
                this->_L = create_lua_state(use_contract);
            }
            GluaStateScope::GluaStateScope(const GluaStateScope &other) : _L(other._L) {}
            GluaStateScope::~GluaStateScope() {
                if (nullptr != _L)
                    close_lua_state(_L);
            }

            void GluaStateScope::change_in_file(FILE *in)
            {
                if (nullptr == in)
                    in = stdin;
                _L->in = in;
            }

            void GluaStateScope::change_out_file(FILE *out)
            {
                if (nullptr == out)
                    out = stdout;
                _L->out = out;
            }

            void GluaStateScope::change_err_file(FILE *err)
            {
                if (nullptr == err)
                    err = stderr;
                _L->err = err;
            }

            bool GluaStateScope::in_sandbox() const {
                return check_in_lua_sandbox(_L);
            }

            void GluaStateScope::enter_sandbox() {
                enter_lua_sandbox(_L);
            }

            void GluaStateScope::exit_sandbox() {
                exit_lua_sandbox(_L);
            }

            bool GluaStateScope::commit_storage_changes()
            {
                return thinkyoung::lua::lib::commit_storage_changes(_L);
            }

            GluaStateValueNode GluaStateScope::get_value_node(const char *key)
            {
                return get_lua_state_value_node(_L, key);
            }
            GluaStateValue GluaStateScope::get_value(const char *key)
            {
                return get_lua_state_value(_L, key);
            }

            void GluaStateScope::set_value(const char *key, GluaStateValue value, enum GluaStateValueType type)
            {
                set_lua_state_value(_L, key, value, type);
            }

            void GluaStateScope::set_instructions_limit(int limit)
            {
                return set_lua_state_instructions_limit(_L, limit);
            }
            int GluaStateScope::get_instructions_limit()
            {
                return get_lua_state_instructions_limit(_L);
            }
            int GluaStateScope::get_instructions_executed_count()
            {
                return get_lua_state_instructions_executed_count(_L);
            }
            int GluaStateScope::check_thinkyoung_contract_api_instructions_over_limit()
            {
                return global_glua_chain_api->check_contract_api_instructions_over_limit(_L);
            }

            void GluaStateScope::notify_stop()
            {
                notify_lua_state_stop(_L);
            }
            bool GluaStateScope::check_notified_stop()
            {
                return check_lua_state_notified_stop(_L);
            }
            void GluaStateScope::resume_running()
            {
                resume_lua_state_running(_L);
            }

            bool GluaStateScope::run_compiledfile(const char *filename)
            {
                return thinkyoung::lua::lib::run_compiledfile(_L, filename);
            }
            bool GluaStateScope::run_compiled_bytestream(void *stream_addr)
            {
                return thinkyoung::lua::lib::run_compiled_bytestream(_L, stream_addr);
            }
            void GluaStateScope::add_global_c_function(const char *name, lua_CFunction func)
            {
                thinkyoung::lua::lib::add_global_c_function(_L, name, func);
            }
            void GluaStateScope::add_global_string_variable(const char *name, const char *str)
            {
                thinkyoung::lua::lib::add_global_string_variable(_L, name, str);
            }
            void GluaStateScope::add_global_int_variable(const char *name, lua_Integer num)
            {
                thinkyoung::lua::lib::add_global_int_variable(_L, name, num);
            }
            void GluaStateScope::add_global_number_variable(const char *name, lua_Number num)
            {
                thinkyoung::lua::lib::add_global_number_variable(_L, name, num);
            }
            void GluaStateScope::add_global_bool_variable(const char *name, bool value)
            {
                thinkyoung::lua::lib::add_global_bool_variable(_L, name, value);
            }
            void GluaStateScope::register_module(const char *name, lua_CFunction openmodule_func)
            {
                thinkyoung::lua::lib::register_module(_L, name, openmodule_func);
            }
            int GluaStateScope::execute_contract_api(const char *contract_name, const char *api_name, const char *arg1, std::string *result_json_string)
            {
                return thinkyoung::lua::lib::execute_contract_api(_L, contract_name, api_name, arg1, result_json_string);
            }

            int GluaStateScope::execute_contract_api_by_address(const char *address, const char *api_name, const char *arg1, std::string *result_json_string)
            {
                return thinkyoung::lua::lib::execute_contract_api_by_address(_L, address, api_name, arg1, result_json_string);
            }

            bool GluaStateScope::execute_contract_init_by_address(const char *contract_address, const char *arg1, std::string *result_json_string)
            {
                return thinkyoung::lua::lib::execute_contract_init_by_address(_L, contract_address, arg1, result_json_string);
            }
            bool GluaStateScope::execute_contract_start_by_address(const char *contract_address, const char *arg1, std::string *result_json_string)
            {
                return thinkyoung::lua::lib::execute_contract_start_by_address(_L, contract_address, arg1, result_json_string);
            }

            bool GluaStateScope::execute_contract_init(GluaModuleByteStream *stream, const char *arg1, std::string *result_json_string)
            {
                return thinkyoung::lua::lib::execute_contract_init(_L, "tmp", stream, arg1, result_json_string);
            }
            bool GluaStateScope::execute_contract_start(GluaModuleByteStream *stream, const char *arg1, std::string *result_json_string)
            {
                return thinkyoung::lua::lib::execute_contract_start(_L, "tmp", stream, arg1, result_json_string);
            }

            bool GluaStateScope::check_contract_bytecode_stream(GluaModuleByteStream *stream)
            {
                return thinkyoung::lua::lib::check_contract_bytecode_stream(_L, stream);
            }

            bool GluaStateScope::start_repl()
            {
                return thinkyoung::lua::lib::start_repl(_L);
            }
            bool GluaStateScope::stop_repl()
            {
                return thinkyoung::lua::lib::stop_repl(_L);
            }
            bool GluaStateScope::start_repl_async()
            {
                return thinkyoung::lua::lib::start_repl_async(_L);
            }
            bool GluaStateScope::check_repl_running()
            {
                return thinkyoung::lua::lib::check_repl_running(_L);
            }
            int *GluaStateScope::get_repl_state()
            {
                return thinkyoung::lua::lib::get_repl_state(_L);
            }

			void GluaStateScope::add_system_extra_libs()
            {
				thinkyoung::lua::lib::add_system_extra_libs(_L);
            }

        }
    }
}
