/**
* some lib for thinkyoung lua
* @author 
*/

#ifndef thinkyoung_lua_lib_h
#define thinkyoung_lua_lib_h

#include <glua/lprefix.h>


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <vector>
#include <stack>
#include <string>
#include <unordered_map>
#include <set>

#include <glua/lua.h>
#include <glua/lhashmap.h>
#include <glua/lstate.h>
#include <glua/llimits.h>
#include <glua/lobject.h>
#include <glua/thinkyoung_lua_api.h>

#define BOOL_VAL(val) ((val)>0?true:false)

#define ADDRESS_CONTRACT_PREFIX "@pointer_"

#define STREAM_CONTRACT_PREFIX "@stream_"

/**
 * wrapper contract name, so it wan't conflict with lua inner module names
 */
#define CONTRACT_NAME_WRAPPER_PREFIX "@g_"

#define CURRENT_CONTRACT_NAME	"@self"

#define THINKYOUNG_CONTRACT_INITING "thinkyoung_contract_ininting"

#define STARTING_CONTRACT_ADDRESS "starting_contract_address"

#define LUA_STATE_DEBUGGER_INFO	"lua_state_debugger_info"


/**
* in lua_State scope, share some values, after close lua_State, you must release these shared values
* all keys and values need copied to shared pool
*/
enum GluaStateValueType {
    LUA_STATE_VALUE_INT = 1,
    LUA_STATE_VALUE_INT_POINTER = 2,
    LUA_STATE_VALUE_STRING = 3,
    LUA_STATE_VALUE_POINTER = 4,
    LUA_STATE_VALUE_nullptr = 0
};

typedef union _GluaStateValue {
    int int_value;
    int *int_pointer_value;
    const char *string_value;
    void *pointer_value;
} GluaStateValue;

typedef struct _GluaStateValueNode {
    enum GluaStateValueType type;
    GluaStateValue value;
} GluaStateValueNode;


namespace thinkyoung
{
    namespace lua
    {
        namespace lib
        {

			// 合约的特殊API名称列表
			extern std::vector<std::string> contract_special_api_names;
			// 合约的参数是整数的特殊API名称列表
			extern std::vector<std::string> contract_int_argument_special_api_names;

            class GluaStateScope
            {
            private:
                lua_State *_L;
                bool _use_contract;
            public:
                GluaStateScope(bool use_contract = true);
                GluaStateScope(const GluaStateScope &other);
                ~GluaStateScope();

                inline lua_State *L() const { return this->_L; }

                /************************************************************************/
                /* use other to replace stdin                                           */
                /************************************************************************/
                void change_in_file(FILE *in);
                /************************************************************************/
                /* use other to replace stdout                                          */
                /************************************************************************/
                void change_out_file(FILE *out);
                /************************************************************************/
                /* use other to replace stderr                                          */
                /************************************************************************/
                void change_err_file(FILE *err);

                /************************************************************************/
                /* whether in sandbox(need every api check)                             */
                /************************************************************************/
                bool in_sandbox() const;
                void enter_sandbox();
                void exit_sandbox();

                /************************************************************************/
                /* commit all storage changes happened in the lua stack                 */
                /************************************************************************/
                bool commit_storage_changes();

                /************************************************************************/
                /* every lua stack has a key=>value map                                 */
                /************************************************************************/
                GluaStateValueNode get_value_node(const char *key);
                /************************************************************************/
                /* every lua stack has a key=>value map                                 */
                /************************************************************************/
                GluaStateValue get_value(const char *key);
                /************************************************************************/
                /* every lua stack has a key=>value map                                 */
                /************************************************************************/
                void set_value(const char *key, GluaStateValue value, enum GluaStateValueType type);

                /************************************************************************/
                /* set how many lua vm instructions can run in the lua stack            */
                /************************************************************************/
                void set_instructions_limit(int limit);
                /************************************************************************/
                /* the the max limit instructions count in the lua stack                */
                /************************************************************************/
                int get_instructions_limit();
                /************************************************************************/
                /* get how many lua vm instructions ran now in the lua stack            */
                /************************************************************************/
                int get_instructions_executed_count();
                /************************************************************************/
                /* check whether the thinkyoung apis over limit(maybe thinkyoung limit api called count) */
                /************************************************************************/
                int check_thinkyoung_contract_api_instructions_over_limit();

                /************************************************************************/
                /* notify the lua stack to stop                                         */
                /************************************************************************/
                void notify_stop();
                /************************************************************************/
                /* whether the lua stack waiting to stop                                */
                /************************************************************************/
                bool check_notified_stop();
                /************************************************************************/
                /* cancel stopping of the lua stack                                     */
                /************************************************************************/
                void resume_running();

				// 添加合约中没有的更多的系统库，比如网络库，IO库，操作系统库等
				void add_system_extra_libs();

                bool run_compiledfile(const char *filename);
                bool run_compiled_bytestream(void *stream_addr);

                /************************************************************************/
                /* bind C function to Lua global variable                               */
                /************************************************************************/
                void add_global_c_function(const char *name, lua_CFunction func);
                void add_global_string_variable(const char *name, const char *str);
                void add_global_int_variable(const char *name, lua_Integer num);
                void add_global_bool_variable(const char *name, bool value);
                void add_global_number_variable(const char *name, lua_Number num);
                /************************************************************************/
                /* bind C module to Lua, then user can use require '<module_name>' to load the module */
                /************************************************************************/
                void register_module(const char *name, lua_CFunction openmodule_func);

                /************************************************************************/
                /* execute contract's api by contract name and api name                 */
                /************************************************************************/
                int execute_contract_api(const char *contract_name, const char *api_name, const char *arg1, std::string *result_json_string);
                /************************************************************************/
                /* execute contract's api by contract address and api name              */
                /************************************************************************/
                int execute_contract_api_by_address(const char *address, const char *api_name, const char *arg1, std::string *result_json_string);

                /************************************************************************/
                /* execute contract's init function by byte stream.
                   if you use this, notice the life cycle of the stream */
                /************************************************************************/
                bool execute_contract_init(GluaModuleByteStreamP stream, const char *arg1, std::string *result_json_string);
                /************************************************************************/
                /* execute contract's start function by byte stream,
                   if you use this, notice the life cycle of the stream */
                /************************************************************************/
                bool execute_contract_start(GluaModuleByteStreamP stream, const char *arg1, std::string *result_json_string);
                /************************************************************************/
                /* execute contract's init function by contract address                 */
                /************************************************************************/
                bool execute_contract_init_by_address(const char *contract_address, const char *arg1, std::string *result_json_string);
                /************************************************************************/
                /* execute contract's start function by contract address                */
                /************************************************************************/
                bool execute_contract_start_by_address(const char *contract_address, const char *arg1, std::string *result_json_string);

                /************************************************************************/
                /* whether contract's bytecode stream right                             */
                /************************************************************************/
                bool check_contract_bytecode_stream(GluaModuleByteStreamP stream);

                bool start_repl();
                bool stop_repl();
                bool start_repl_async();
                bool check_repl_running();
                int *get_repl_state();
            };

			// glua中的字节流类型，可以在运行时使用
			class GluaByteStream
			{
			private:
				size_t _pos;
				std::vector<char> _buff;
			public:
				inline GluaByteStream(): _pos(0) {}
				inline virtual ~GluaByteStream(){}

				inline bool eof() const
				{
					return _pos >= _buff.size();
				}

				inline char current() const
				{
					return eof() ? -1 : _buff[_pos];
				}

				inline bool next()
				{
					if (eof())
						return false;
					++_pos;
					return true;
				}

				inline size_t pos() const
				{
					return _pos;
				}

				inline void reset_pos()
				{
					_pos = 0; 
				}

				inline size_t size() const
				{
					return _buff.size();
				}

				inline std::vector<char>::const_iterator begin() const
				{
					return _buff.begin();
				}

				inline std::vector<char>::const_iterator end() const
				{
					return _buff.end();
				}

				inline void push(char c)
				{
					_buff.push_back(c);
				}

				inline void push_some(std::vector<char> const &data)
				{
					_buff.resize(_buff.size() + data.size());
					memcpy(_buff.data() + _buff.size(), data.data(), sizeof(char) * data.size());
				}

				inline bool equals(GluaByteStream *other)
				{
                    if (!other)
                        return false;
					if (this == other)
						return true;
					if (this->size() != other->size())
						return false;
					return this->_buff == other->_buff;
				}

			};

            lua_State *create_lua_state(bool use_contract = true);

            bool commit_storage_changes(lua_State *L);

            void close_lua_state(lua_State *L);

            /**
            * share some values in L
            */
            void close_all_lua_state_values();
            void close_lua_state_values(lua_State *L);

            GluaStateValueNode get_lua_state_value_node(lua_State *L, const char *key);
            GluaStateValue get_lua_state_value(lua_State *L, const char *key);
            void set_lua_state_instructions_limit(lua_State *L, int limit);

            int get_lua_state_instructions_limit(lua_State *L);

            int get_lua_state_instructions_executed_count(lua_State *L);

            void enter_lua_sandbox(lua_State *L);

            void exit_lua_sandbox(lua_State *L);

            bool check_in_lua_sandbox(lua_State *L);

            /**
             * notify lvm to stop running the lua stack
             */
            void notify_lua_state_stop(lua_State *L);

            /**
             * check whether the lua state notified stop before
             */
            bool check_lua_state_notified_stop(lua_State *L);

            /**
             * resume lua_State to be available running again
             */
            void resume_lua_state_running(lua_State *L);

            void set_lua_state_value(lua_State *L, const char *key, GluaStateValue value, enum GluaStateValueType type);

            GluaTableMapP create_managed_lua_table_map(lua_State *L);

            char *malloc_managed_string(lua_State *L, size_t size, const char *init_data=nullptr);

			char *malloc_and_copy_string(lua_State *L, const char *init_data);

            GluaModuleByteStream *malloc_managed_byte_stream(lua_State *L);


            /**
             * compile/run apis
             */
            bool compilefile_to_file(const char *filename, const char *out_filename, char *error, bool use_type_check = false);
            bool compilefile_to_stream(lua_State *L, const char *filename, GluaModuleByteStreamP stream, char *error, bool use_type_check = false, bool is_contract=false);

            bool compile_contract_to_stream(const char *filename, GluaModuleByteStreamP stream, char *error,
                GluaStatePreProcessorFunction *preprocessor = nullptr, bool use_type_check = false);

            bool run_compiledfile(lua_State *L, const char *filename);
            bool run_compiled_bytestream(lua_State *L, void *stream_addr);

            void add_global_c_function(lua_State *L, const char *name, lua_CFunction func);
            void add_global_string_variable(lua_State *L, const char *name, const char *str);
            void add_global_int_variable(lua_State *L, const char *name, lua_Integer num);
            void add_global_number_variable(lua_State *L, const char *name, lua_Number num);
            void add_global_bool_variable(lua_State *L, const char *name, bool value);

			std::string get_global_string_variable(lua_State *L, const char *name);
			
			void remove_global_variable(lua_State *L, const char *name);
            void register_module(lua_State *L, const char *name, lua_CFunction openmodule_func);
			// 添加更多系统库，包括网络，操作系统，http等合约中不适合存在的库
			void add_system_extra_libs(lua_State *L);

			// 重置lvm执行的指令步数
			void reset_lvm_instructions_executed_count(lua_State *L);

            // 给lvm执行的指令步数增加add_count条
            void increment_lvm_instructions_executed_count(lua_State *L, int add_count);

            int execute_contract_api(lua_State *L, const char *contract_name, const char *api_name, const char *arg1, std::string *result_json_string);

			int execute_contract_api_by_stream(lua_State *L, GluaModuleByteStreamP stream, const char *api_name, const char *arg1, std::string *result_json_string);

            const char *get_contract_id_in_api(lua_State *L);

            GluaStorageValue lthinkyoung_get_storage(lua_State *L, const char *contract_id, const char *name);
            
			bool lthinkyoung_set_storage(lua_State *L, const char *contract_id, const char *name, GluaStorageValue value);

            void free_bytecode_stream(GluaModuleByteStreamP stream);

            /**
            * diff from execute_contract_api is the contract bytestream is loaded by pointer and thinkyoung
            */
            LUA_API int execute_contract_api_by_address(lua_State *L, const char *contract_address,
				const char *api_name, const char *arg1, std::string *result_json_string);

            bool execute_contract_init(lua_State *L, const char *name, GluaModuleByteStreamP stream, const char *arg1, std::string *result_json_string);
            bool execute_contract_start(lua_State *L, const char *name, GluaModuleByteStreamP stream, const char *arg1, std::string *result_json_string);

            bool execute_contract_init_by_address(lua_State *L, const char *contract_address, const char *arg1, std::string *result_json_string);
            bool execute_contract_start_by_address(lua_State *L, const char *contract_address, const char *arg1, std::string *result_json_string);

			// 此次调用合约是否是从init API开始调用的
			bool is_calling_contract_init_api(lua_State *L);

			// 获取此次调用合约一开始调用的合约地址
			std::string get_starting_contract_address(lua_State *L);

            const std::map<std::string, std::string> *get_globalvar_type_infos();

			// 获取当前合约调用API栈中栈中各函数代码定义所在的合约ID
			std::stack<std::string> *get_using_contract_id_stack(lua_State *L, bool init_if_not_exist=true);

			// 获取当前栈中最上层的合约API（也就当前所处的合约API）代码定义所在的合约ID
			std::string get_current_using_contract_id(lua_State *L);

            /**
            * load one chunk from lua bytecode file
            */
            LClosure* luaU_undump_from_file(lua_State *L, const char *binary_filename, const char* name);

            /**
             * load one chunk from lua bytecode stream
             */
            LClosure *luaU_undump_from_stream(lua_State *L, GluaModuleByteStreamP stream, const char *name);

			/**
			 * 把字节码流按比较对人可读的文本undump到文件中
			 */
			bool undump_from_bytecode_stream_to_file(lua_State *L, GluaModuleByteStreamP stream, FILE *out);

			/**
			 * 把字节码文件按比较对人可读的文本undump到文件中
			 */
			bool undump_from_bytecode_file_to_file(lua_State *L, const char *bytecode_filename, FILE *out);

            /**
             * secure apis
             */

            /**
             * check contract lua bytecode file(whether safe)
             */
            bool check_contract_bytecode_file(lua_State *L, const char *binary_filename);

            /**
             * check contract bytecode(whether safe)
             */
            bool check_contract_bytecode_stream(lua_State *L, GluaModuleByteStreamP stream, char *error = nullptr);

            /**
             * check contract lua bytecode proto is right(whether safe)
             */
            bool check_contract_proto(lua_State *L, Proto *proto, char *error = nullptr, std::list<Proto*> *parents = nullptr);

			/**
			 * 反编译字节码到可读源码
			 */
			std::string decompile_bytecode_to_source_code(lua_State *L, GluaModuleByteStreamP stream,
				std::string module_name, char *error = nullptr);

			/**
			 * 反编译字节码文件到可读源码
			 */
			std::string decompile_bytecode_file_to_source_code(lua_State *L, std::string bytecode_filepath,
				std::string module_name, char *error = nullptr);

			/**
			 * 反编译字节码到可读的汇编码
			 */
			std::string disassemble_bytecode(lua_State *L, GluaModuleByteStreamP stream, std::string module_name, char *error = nullptr);
			
			/**
			 * 反编译字节码文件到可读的汇编码
			 */
			std::string disassemble_bytecode_file(lua_State *L, std::string bytecode_filepath, std::string module_name, char *error = nullptr);

            /************************************************************************/
            /* pre modify lua source
               eg. change import_contract 'constant_contract_name' to import_contract_from_address 'contract_id'
               save the new content to new tmp file
               @return new source file path
               */
            /************************************************************************/
            // std::string pre_modify_lua_source(lua_State *L, std::string source_filepath, char *error, bool use_type_check = false);

			std::string pre_modify_lua_source_code_string(lua_State *L, std::string source_filepath, 
				bool create_debug_file, std::string &source_code, char *error, bool *changed = nullptr, 
				bool use_type_check = false, bool include_typed_lua_lib=true, GluaModuleByteStreamP stream=nullptr,
				bool throw_exception=true, bool in_repl = false, bool is_contract = false);

			/* pre modify lua source
			eg. change import_contract 'constant_contract_name' to import_contract_from_address 'contract_id'
			@return new source content
			*/
			std::string pre_modify_lua_source_code(lua_State *L, std::string source_filepath, char *error,
				bool *changed=nullptr, bool use_type_check = false, GluaModuleByteStreamP stream=nullptr, bool is_contract=false);

            /**
             * start REPL
             */
            bool start_repl(lua_State *L);

            /**
             * stop REPL
             */
            bool stop_repl(lua_State *L);

            /**
             * start REPL in a detached thread
             */
            bool start_repl_async(lua_State *L);

            /**
             * check whether the REPL is running
             */
            bool check_repl_running(lua_State *L);

            /**
             * get REPL state stored in L
             */
            int *get_repl_state(lua_State *L);

            std::string wrap_contract_name(const char *contract_name);

            std::string unwrap_any_contract_name(const char *contract_name);

            std::string unwrap_contract_name(const char *wrappered_contract_name);

            const std::string get_typed_lua_lib_code();

#define lerror_set(L, error, error_format, ...) do {			 \
     if (nullptr != error && strlen(error) < 1)					\
     {						\
        char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH+1];		 \
        memset(error_msg, 0x0, sizeof(error_msg));		     \
        snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH, error_format, ##__VA_ARGS__);				\
        error_msg[LUA_COMPILE_ERROR_MAX_LENGTH-1] = '\-';       \
        memcpy(error, error_msg, sizeof(char)*(1 + strlen(error_msg)));								\
     }												\
     global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_format, ##__VA_ARGS__);		\
} while(0)

#define lcompile_error_set(L, error, error_format, ...) do {	   \
    lerror_set(L, error, error_format, ##__VA_ARGS__);		   \
    if (strlen(L->compile_error) < 1)						  \
    {														   \
        snprintf(L->compile_error, LUA_COMPILE_ERROR_MAX_LENGTH-1, error_format, ##__VA_ARGS__);			\
    }															\
} while(0)

#define lcompile_error_get(L, error) {					   \
    if (error && strlen(error)<1 && strlen(L->compile_error) > 0)			\
    {															   \
        memcpy(error, L->compile_error, sizeof(char) * (strlen(L->compile_error) + 1));			\
    } else if(nullptr != error && strlen(error)<1 && strlen(L->runerror) > 0)			   \
	{			\
		 memcpy(error, L->runerror, sizeof(char) * (strlen(L->runerror) + 1));			\
			}		\
}

#define lmalloc_error(L) do { \
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "malloc memory error"); \
	} while(0)

#define GLUA_TYPE_NAMESPACE_PREFIX "$type$"
#define GLUA_TYPE_NAMESPACE_PREFIX_WRAP(name) "$type$" #name ""

        }
    }
}


#endif