/**
 * lua module injector header in thinkyoung
 * @author zoowii@thinkyoung
 */

#ifndef thinkyoung_lua_api_h
#define thinkyoung_lua_api_h

#include <glua/lprefix.h>

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>

#include <glua/lua.h>

#define LOG_INFO(...)  fprintf(stderr, "[INFO] " ##__VA_ARGS__)

#define ERROR_INFO(...) fprintf(stderr, "[ERROR] " ##__VA_ARGS__)

#define GLUA_SOURCE_FILE_EXTENTION_NAME ".glua"

#define LUA_MODULE_BYTE_STREAM_BUF_SIZE 1024*1024

#define LUA_VM_EXCEPTION_STRNG_MAX_LENGTH 256

#define LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH 4096

#define CONTRACT_ID_MAX_LENGTH 50

#define CONTRACT_NAME_MAX_LENGTH 50

#define CONTRACT_MAX_OFFLINE_API_COUNT	10

#define LUA_FUNCTION_MAX_LOCALVARS_COUNT 128

#define LUA_MAX_LOCALVARNAME_LENGTH	128

// emit抛出的不同eventTypeName最大数量
#define EMIT_EVENT_NAME_MAX_COUNT 50

//级别 临时 永久
#define CONTRACT_LEVEL_TEMP 1
#define CONTRACT_LEVEL_FOREVER 2

//状态 有效 删除
#define CONTRACT_STATE_VALID 1
#define CONTRACT_STATE_DELETED 2

// 在blockchain中是否使用新语法
#define USE_TYPE_CHECK true 

// 合约的storage的record类型最多可以包含多少个属性
#define CONTRACT_STORAGE_PROPERTIES_MAX_COUNT 256

// 链给glua提供的全局API每次API调用增加指令累计执行数的数量
#define CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT 50

// 外部管理的对象类型枚举，用在register_object_in_pool中
enum GluaOutsideObjectTypes
{
	OUTSIDE_STREAM_STORAGE_TYPE = 0
};

typedef enum GluaStorageValueType
{
    LVALUE_INTEGER = 0, LVALUE_NUMBER = 1, LVALUE_TABLE = 2, LVALUE_STRING = 3,
    LVALUE_BOOLEAN = 4, LVALUE_USERDATA = 5, LVALUE_NIL = 6, LVALUE_ARRAY = 7
    ,LVALUE_STREAM = 9

	, LVALUE_NOT_SUPPORT = 100
} GluaStorageValueType;

namespace thinkyoung {
    namespace blockchain {

        enum StorageValueTypes
        {
            storage_value_null = 0,
            storage_value_int = 1,
            storage_value_number = 2,
            storage_value_bool = 3,
            storage_value_string = 4,
			storage_value_stream = 5,

            storage_value_unknown_table = 50,
            storage_value_int_table = 51,
            storage_value_number_table = 52,
            storage_value_bool_table = 53,
            storage_value_string_table = 54,
			storage_value_stream_table = 55,

            storage_value_unknown_array = 100,
            storage_value_int_array = 101,
            storage_value_number_array = 102,
            storage_value_bool_array = 103,
            storage_value_string_array = 104,
			storage_value_stream_array = 105,

            storage_value_userdata = 201,
            storage_value_not_support = 202
        };


        inline bool is_any_base_storage_value_type(StorageValueTypes type)
        {
            return ((type >= StorageValueTypes::storage_value_int)
                && (type <= StorageValueTypes::storage_value_stream));
        }

        inline bool is_any_table_storage_value_type(StorageValueTypes type)
        {
            return (type >= StorageValueTypes::storage_value_unknown_table)
                && (type <= StorageValueTypes::storage_value_stream_table);
        }

        inline bool is_any_array_storage_value_type(StorageValueTypes type)
        {
            return (type >= StorageValueTypes::storage_value_unknown_array)
                && (type <= StorageValueTypes::storage_value_stream_array);
        }

        inline StorageValueTypes get_storage_base_type(StorageValueTypes type)
        {
            switch (type)
            {
            case StorageValueTypes::storage_value_bool:
            case StorageValueTypes::storage_value_bool_array:
            case StorageValueTypes::storage_value_bool_table:
                return StorageValueTypes::storage_value_bool;
            case StorageValueTypes::storage_value_int:
            case StorageValueTypes::storage_value_int_array:
            case StorageValueTypes::storage_value_int_table:
                return StorageValueTypes::storage_value_int;
            case StorageValueTypes::storage_value_number:
            case StorageValueTypes::storage_value_number_array:
            case StorageValueTypes::storage_value_number_table:
                return StorageValueTypes::storage_value_number;
            case StorageValueTypes::storage_value_string:
            case StorageValueTypes::storage_value_string_array:
            case StorageValueTypes::storage_value_string_table:
                return StorageValueTypes::storage_value_string;
			case StorageValueTypes::storage_value_stream:
			case StorageValueTypes::storage_value_stream_array:
			case StorageValueTypes::storage_value_stream_table:
				return StorageValueTypes::storage_value_stream;
            default:
                return StorageValueTypes::storage_value_null;
            }
        }

        inline StorageValueTypes get_item_type_in_table_or_array(StorageValueTypes type)
        {
            switch (type)
            {
            case StorageValueTypes::storage_value_int_table:
            case StorageValueTypes::storage_value_int_array:
                return StorageValueTypes::storage_value_int;
            case StorageValueTypes::storage_value_bool_table:
            case StorageValueTypes::storage_value_bool_array:
                return StorageValueTypes::storage_value_bool;
            case StorageValueTypes::storage_value_number_table:
            case StorageValueTypes::storage_value_number_array:
                return StorageValueTypes::storage_value_number;
            case StorageValueTypes::storage_value_string_table:
            case StorageValueTypes::storage_value_string_array:
                return StorageValueTypes::storage_value_string;
			case StorageValueTypes::storage_value_stream_table:
			case StorageValueTypes::storage_value_stream_array:
				return StorageValueTypes::storage_value_stream;
            case StorageValueTypes::storage_value_unknown_table:
            case StorageValueTypes::storage_value_unknown_array:
                return StorageValueTypes::storage_value_null;
            default:
                return StorageValueTypes::storage_value_null;
            }
        }

    }
}

enum GluaTypeInfoEnum
{
	LTI_OBJECT = 0,
	LTI_NIL = 1,
	LTI_STRING = 2,
	LTI_INT = 3,
	LTI_NUMBER = 4,
	LTI_BOOL = 5,
	LTI_TABLE = 6,
	LTI_FUNCTION = 7, // coroutine as function type
	LTI_UNION = 8,
	LTI_RECORD = 9, // 新语法, type <RecordName> = { <name> : <type> , ... }
	LTI_GENERIC = 10, // 新语法，泛型类型
	LTI_ARRAY = 11, // 新语法，列表类型
	LTI_MAP = 12, // 新语法，单纯的哈希表，并且key类型只能是字符串
	LTI_LITERIAL_TYPE = 13, // 新语法，literal type 字符串/数字/布尔值的字面值的union的类型，比如: "Male" | "Female"
	LTI_STREAM = 14, // Stream类型，没有直接的字面量
	LTI_UNDEFINED = 100 // 未定义值，类似undefined
};

class GluaModuleByteStream {
public:
    bool is_bytes;
    std::vector<char> buff;
    std::vector<std::string> contract_apis;
    std::vector<std::string> offline_apis;
    std::vector<std::string> contract_emit_events;
    std::string contract_id;
    std::string contract_name;
    int  contract_level;
    int  contract_state;
    // 合约中storage的类型
    std::map<std::string, thinkyoung::blockchain::StorageValueTypes> contract_storage_properties;

	// 合约中各API的API名称的参数类型列表
	std::map<std::string, std::vector<GluaTypeInfoEnum>> contract_api_arg_types;

public:
    GluaModuleByteStream();
    virtual ~GluaModuleByteStream();
};

typedef GluaModuleByteStream *GluaModuleByteStreamP;

class GluaContractInfo
{
public:
    std::vector<std::string> contract_apis;
};

#define THINKYOUNG_API_NO_ERROR 0
#define THINKYOUNG_API_SIMPLE_ERROR 1
#define THINKYOUNG_API_MEMORY_ERROR 2
#define THINKYOUNG_API_LVM_ERROR 3
#define THINKYOUNG_API_PARSER_ERROR 4
#define THINKYOUNG_API_COMPILE_ERROR 5
#define THINKYOUNG_API_LVM_LIMIT_OVER_ERROR 6
#define THINKYOUNG_API_THROW_ERROR 7   // glua代码中主动抛出的错误


// storage structs
#define LUA_STORAGE_CHANGELIST_KEY "__lua_storage_changelist__"
#define LUA_STORAGE_READ_TABLES_KEY "__lua_storage_read_tables__"

#define GLUA_OUTSIDE_OBJECT_POOLS_KEY "__glua_outside_object_pools__"

#define lua_storage_is_table(t) (thinkyoung::blockchain::is_any_table_storage_value_type(t)||thinkyoung::blockchain::is_any_array_storage_value_type(t))
#define lua_storage_is_array(t) (thinkyoung::blockchain::is_any_array_storage_value_type(t))
#define lua_storage_is_hashtable(t) (thinkyoung::blockchain::is_any_table_storage_value_type(t))

struct GluaStorageValue;

struct lua_table_binary_function
{	// base class for binary functions
    typedef std::string first_argument_type;
    typedef std::string second_argument_type;
    typedef bool result_type;
};

struct lua_table_less
    : public lua_table_binary_function
{	// functor for operator<
    bool operator()(const std::string& _Left, const std::string& _Right) const
    {	// apply operator< to operands
        if (_Left.length() != _Right.length())
            return _Left.length() < _Right.length();
        return _Left < _Right;
    }
};

typedef std::map<std::string, struct GluaStorageValue, struct lua_table_less> GluaTableMap;

typedef GluaTableMap* GluaTableMapP;

struct GluaStorageChangeItem;


typedef union GluaStorageValueUnion
{
    lua_Integer int_value;
    lua_Number number_value;
    bool bool_value;
    char *string_value;
    GluaTableMapP table_value;
    void *userdata_value;
    void *pointer_value;
} GluaStorageValueUnion;

typedef struct GluaStorageValue
{
    thinkyoung::blockchain::StorageValueTypes type;
    union GluaStorageValueUnion value;
    inline GluaStorageValue()
    {
        type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
    }
    inline static GluaStorageValue from_int(int val)
    {
        GluaStorageValue sv;
        sv.type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
        sv.value.int_value = val;
        return sv;
    }
    inline static GluaStorageValue from_string(char *val)
    {
        GluaStorageValue sv;
        sv.type = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
        sv.value.string_value = val;
        return sv;
    }

	// 尝试类型转换
	inline void try_parse_type(thinkyoung::blockchain::StorageValueTypes new_type)
    {
	    switch(new_type)
	    {
		case thinkyoung::blockchain::StorageValueTypes::storage_value_int:
			try_parse_to_int_type();
			break;
		case thinkyoung::blockchain::StorageValueTypes::storage_value_number:
			try_parse_to_number_type();
			break;
		default:
			break;
	    }
    }

	// 尝试当成int处理
	inline void try_parse_to_int_type()
    {
	    if(type == thinkyoung::blockchain::StorageValueTypes::storage_value_number)
	    {
			value.int_value = (lua_Integer)value.number_value;
			type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
	    }
    }

	// 尝试当成number处理
	inline void try_parse_to_number_type()
    {
		if (type == thinkyoung::blockchain::StorageValueTypes::storage_value_int)
		{
			value.number_value = (lua_Number)value.int_value;
			type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
		}
    }

    inline static bool is_same_base_type_with_type_parse(thinkyoung::blockchain::StorageValueTypes type1, thinkyoung::blockchain::StorageValueTypes type2)
    {
        return type1 == type2;
    }
    inline bool equals(GluaStorageValue &other)
    {
        if (type != other.type)
            return false;
        switch (type)
        {
        case thinkyoung::blockchain::StorageValueTypes::storage_value_string:
            return strcmp(value.string_value, other.value.string_value) == 0;
        case thinkyoung::blockchain::StorageValueTypes::storage_value_int:
            return value.int_value == other.value.int_value;
        case thinkyoung::blockchain::StorageValueTypes::storage_value_number:
            return value.number_value == other.value.number_value;
        case thinkyoung::blockchain::StorageValueTypes::storage_value_bool:
            return value.bool_value == other.value.bool_value;
            return value.userdata_value == other.value.userdata_value;
        case thinkyoung::blockchain::StorageValueTypes::storage_value_null:
            return true;
        default: {
            if (thinkyoung::blockchain::is_any_table_storage_value_type(type)
                || thinkyoung::blockchain::is_any_array_storage_value_type(type))
            {
                if (value.table_value->size() != other.value.table_value->size())
                    return false;

                for (const auto &p1 : *(value.table_value))
                {
                    auto found = other.value.table_value->find(p1.first);
                    if (found == other.value.table_value->end())
                        return false;
                    auto this_item = p1.second;
                    auto other_item = found->second;
                    if (!this_item.equals(other_item))
                        return false;
                }
                return true;
            }
            return false;
        }
        }
    }
} GluaStorageValue;

typedef struct GluaStorageChangeItem
{
    std::string contract_id;
    std::string key;
    struct GluaStorageValue before;
    struct GluaStorageValue after;
} GluaStorageChangeItem;



typedef std::list<GluaStorageChangeItem> GluaStorageChangeList;

typedef std::list<GluaStorageChangeItem> GluaStorageTableReadList;

struct GluaStorageValue lua_type_to_storage_value_type(lua_State *L, int index);

bool luaL_commit_storage_changes(lua_State *L);

bool lua_push_storage_value(lua_State *L, const GluaStorageValue &value);

typedef std::unordered_map<std::string, GluaStorageChangeItem> ContractChangesMap;

typedef std::shared_ptr<ContractChangesMap> ContractChangesMapP;

typedef std::unordered_map<std::string, ContractChangesMapP> AllContractsChangesMap;


namespace thinkyoung {
    namespace blockchain{
        struct Code;
    }

    namespace lua {
        namespace api {

          class IGluaChainApi
          {
          public:
            /**
            * check whether the contract apis limit over, in this lua_State
            * @param L the lua stack
            * @return TRUE(1 or not 0) if over limit(will break the vm), FALSE(0) if not over limit
            */
            virtual int check_contract_api_instructions_over_limit(lua_State *L) = 0;

            /**
             * whether exception happen in L
             */
            virtual bool has_exception(lua_State *L) = 0;

            /**
            * clear exception marked
            */
            virtual void clear_exceptions(lua_State *L) = 0;


            /**
            * when exception happened, use this api to tell thinkyoung
            * @param L the lua stack
            * @param code error code, 0 is OK, other is different error
            * @param error_format error info string, will be released by lua
            * @param ... error arguments
            */
            virtual void throw_exception(lua_State *L, int code, const char *error_format, ...) = 0;

            /**
            * get contract info stored before from thinkyoung api
            * @param name contract name
            * @param contract_info_ret this api save the contract's api name array here if found, this var will be free by this api
            * @return TRUE(1 or not 0) if success, FALSE(0) if failed
            */
            virtual int get_stored_contract_info(lua_State *L, const char *name, std::shared_ptr<GluaContractInfo> contract_info_ret) = 0;

            virtual int get_stored_contract_info_by_address(lua_State *L, const char *address, std::shared_ptr<GluaContractInfo> contract_info_ret) = 0;

            virtual std::shared_ptr<GluaModuleByteStream> get_bytestream_from_code(lua_State *L, const thinkyoung::blockchain::Code& code) = 0;
            /**
            * load contract lua byte stream from thinkyoung api
            */
            virtual std::shared_ptr<GluaModuleByteStream> open_contract(lua_State *L, const char *name) = 0;

            virtual std::shared_ptr<GluaModuleByteStream> open_contract_by_address(lua_State *L, const char *address) = 0;

            /**
             * get contract address/id from thinkyoung by contract name
             */
            virtual void get_contract_address_by_name(lua_State *L, const char *name, char *address, size_t *address_size) = 0;

            /*
             * check whether the contract exist
             */
            virtual bool check_contract_exist(lua_State *L, const char *name) = 0;

            /**
             * check contract exist by ID string address
             */
            virtual bool check_contract_exist_by_address(lua_State *L, const char *address) = 0;

            /**
             * register new storage name of contract to thinkyoung
             */
            virtual bool register_storage(lua_State *L, const char *contract_name, const char *name) = 0;

            virtual GluaStorageValue get_storage_value_from_thinkyoung(lua_State *L, const char *contract_name, std::string name) = 0;

            virtual GluaStorageValue get_storage_value_from_thinkyoung_by_address(lua_State *L, const char *contract_address, std::string name) = 0;

            /**
             * after lua merge storage changes in lua_State, use the function to store the merged changes of storage to thinkyoung
             */
            virtual bool commit_storage_changes_to_thinkyoung(lua_State *L, AllContractsChangesMap &changes) = 0;

            /**
             * 注册对象地址到关联lua_State的外部对象池（在外部创建的各种类型的对象）中，不同类型的对象需要一个单独的对象池
             * lua_State被释放的时候需要释放所有对象池中所有对象
             * 返回值是glua中用来操作这个对象的句柄/指针，目前只能是对象地址本身，以lightuserdata的形式存在于glua中
             */
            virtual intptr_t register_object_in_pool(lua_State *L, intptr_t object_addr, GluaOutsideObjectTypes type) = 0;

            /**
             * 判断某个对象(register_object_in_pool的返回对象，一般实现为对象的内存地址)是否是关联lua_State中某个类型的对象池中的对象（从而可以判断是否可以强制转换)
             * 如果找到，返回对象地址，否则返回0
             */
            virtual intptr_t is_object_in_pool(lua_State *L, intptr_t object_key, GluaOutsideObjectTypes type) = 0;

            /**
             * 释放lua_State关联的所有外部对象池中的对象已经对象池本身
             */
            virtual void release_objects_in_pool(lua_State *L) = 0;

            /************************************************************************/
            /* transfer asset from contract by account address                      */
            /************************************************************************/
            virtual lua_Integer transfer_from_contract_to_address(lua_State *L, const char *contract_address, const char *to_address,
              const char *asset_type, int64_t amount) = 0;

            /************************************************************************/
            /* transfer asset from contract by account name on chain                */
            /************************************************************************/
            virtual lua_Integer transfer_from_contract_to_public_account(lua_State *L, const char *contract_address, const char *to_account_name,
              const char *asset_type, int64_t amount) = 0;

            virtual int64_t get_contract_balance_amount(lua_State *L, const char *contract_address, const char* asset_symbol) = 0;
            virtual int64_t get_transaction_fee(lua_State *L) = 0;
            virtual uint32_t get_chain_now(lua_State *L) = 0;
            virtual uint32_t get_chain_random(lua_State *L) = 0;
            virtual std::string get_transaction_id(lua_State *L) = 0;
            virtual uint32_t get_header_block_num(lua_State *L) = 0;
            virtual uint32_t wait_for_future_random(lua_State *L, int next) = 0;

            virtual int32_t get_waited(lua_State *L, uint32_t num) = 0;

            virtual void emit(lua_State *L, const char* contract_id, const char* event_name, const char* event_param) = 0;
          };


          extern IGluaChainApi *global_glua_chain_api;

        }
    }
}


#endif