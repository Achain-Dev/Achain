#pragma once

#include <blockchain/Asset.hpp>
#include <blockchain/Config.hpp>
#include <blockchain/Types.hpp>

#include <fc/io/enum_type.hpp>
#include <fc/io/raw.hpp>

#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>

#include <glua/luaconf.h>
#include <glua/thinkyoung_lua_api.h>

namespace thinkyoung {
    namespace blockchain {
    
        extern std::map <StorageValueTypes, std::string> storage_type_map;
        struct StorageDataBase;
        struct StorageDataType {
            fc::enum_type<uint8_t, StorageValueTypes>  storage_type;
            std::vector<char>                           storage_data;
            
            StorageDataType() :storage_type(storage_value_null) {}
            
            template<typename StorageType>
            StorageDataType(const StorageType& t) {
                storage_type = StorageType::type;
                storage_data = fc::raw::pack(t);
            }
            
            template<typename StorageType>
            StorageType as()const {
                FC_ASSERT(storage_type == StorageType::type, "", ("type", storage_type)("StorageType", StorageType::type));
                return fc::raw::unpack<StorageType>(storage_data);
            }
            
            static StorageDataType get_storage_data_from_lua_storage(const GluaStorageValue& lua_storage);
            static GluaStorageValue create_lua_storage_from_storage_data(lua_State *L, const StorageDataType& storage_data);
            
            inline static bool is_table_type(StorageValueTypes type) {
                return (type >= StorageValueTypes::storage_value_unknown_table && type <= StorageValueTypes::storage_value_string_table);
            }
            
            inline static bool is_array_type(StorageValueTypes type) {
                return (type >= StorageValueTypes::storage_value_unknown_array && type <= StorageValueTypes::storage_value_string_array);
            }
            
            //   static std::shared_ptr<StorageDataBase> create_storage_value(const StorageDataType& storage);
            
            bool equals(const StorageDataType& other) const {
                return (other.storage_type == storage_type && storage_data == other.storage_data);
            }
        };
        
        struct ContractStorageChangeItem {
            std::string contract_id;
            std::string key;
            struct StorageDataType before;
            struct StorageDataType after;
            static ContractStorageChangeItem get_entry_change(const StorageDataType& before, const StorageDataType& after);
            static StorageDataType apply_entry_change(const StorageDataType& before, ContractStorageChangeItem& item);
        };
        
        struct StorageDataBase {
            virtual ~StorageDataBase() {}
        };
        
        struct StorageNullType : public StorageDataBase {
            StorageNullType() : raw_storage(0) {}
            static const uint8_t    type;
            
            LUA_INTEGER raw_storage;
        };
        
        
        struct StorageIntType :public StorageDataBase {
            StorageIntType() {}
            StorageIntType(LUA_INTEGER value) :raw_storage(value) {}
            static const uint8_t    type;
            
            LUA_INTEGER raw_storage;
        };
        
        
        struct StorageNumberType :public StorageDataBase {
            StorageNumberType() {}
            StorageNumberType(double value) :raw_storage(value) {}
            
            static const uint8_t    type;
            double raw_storage;
        };
        
        
        struct StorageBoolType :public StorageDataBase {
            StorageBoolType() {}
            StorageBoolType(bool value) :raw_storage(value) {}
            
            static const uint8_t    type;
            bool raw_storage;
        };
        
        
        struct StorageStringType :public StorageDataBase {
            StorageStringType() {}
            StorageStringType(std::string value) :raw_storage(value) {}
            
            static const uint8_t    type;
            std::string raw_storage;
        };
        
        //table
        struct StorageIntTableType :public StorageDataBase {
            StorageIntTableType() {}
            StorageIntTableType(const std::map<std::string, LUA_INTEGER>& value_map) :raw_storage_map(value_map) {}
            
            static const uint8_t    type;
            std::map<std::string, LUA_INTEGER> raw_storage_map;
        };
        
        
        struct StorageNumberTableType :public StorageDataBase {
            StorageNumberTableType() {}
            StorageNumberTableType(const std::map<std::string, double>& value_map) :raw_storage_map(value_map) {}
            static const uint8_t    type;
            
            std::map<std::string, double> raw_storage_map;
        };
        
        
        struct StorageBoolTableType :public StorageDataBase {
            StorageBoolTableType() {}
            StorageBoolTableType(const std::map<std::string, bool>& value_map) :raw_storage_map(value_map) {}
            
            static const uint8_t    type;
            std::map<std::string, bool> raw_storage_map;
        };
        
        struct StorageStringTableType :public StorageDataBase {
            StorageStringTableType() {}
            StorageStringTableType(const std::map<std::string, string>& value_map) :raw_storage_map(value_map) {}
            
            static const uint8_t    type;
            std::map<std::string, string> raw_storage_map;
        };
        
        //array
        struct StorageIntArrayType :public StorageDataBase {
            StorageIntArrayType() {}
            StorageIntArrayType(const std::map<std::string, LUA_INTEGER>& value_map) :raw_storage_map(value_map) {}
            static const uint8_t    type;
            
            std::map<std::string, LUA_INTEGER> raw_storage_map;
        };
        
        
        struct StorageNumberArrayType :public StorageDataBase {
            StorageNumberArrayType() {}
            StorageNumberArrayType(const std::map<std::string, double>& value_map) :raw_storage_map(value_map) {}
            static const uint8_t    type;
            
            std::map<std::string, double> raw_storage_map;
        };
        
        
        struct StorageBoolArrayType :public StorageDataBase {
            StorageBoolArrayType() {}
            StorageBoolArrayType(const std::map<std::string, bool>& value_map) :raw_storage_map(value_map) {}
            
            static const uint8_t    type;
            std::map<std::string, bool> raw_storage_map;
        };
        
        struct StorageStringArrayType :public StorageDataBase {
            StorageStringArrayType() {}
            StorageStringArrayType(const std::map<std::string, string>& value_map) :raw_storage_map(value_map) {}
            
            static const uint8_t    type;
            std::map<std::string, string> raw_storage_map;
        };
        
    }
} // thinkyoung::blockchain


FC_REFLECT_ENUM(thinkyoung::blockchain::StorageValueTypes,
                (storage_value_null)
                (storage_value_int)
                (storage_value_number)
                (storage_value_bool)
                (storage_value_string)
                (storage_value_unknown_table)
                (storage_value_int_table)
                (storage_value_number_table)
                (storage_value_bool_table)
                (storage_value_string_table)
                (storage_value_unknown_array)
                (storage_value_int_array)
                (storage_value_number_array)
                (storage_value_bool_array)
                (storage_value_string_array)
               )
FC_REFLECT(thinkyoung::blockchain::ContractStorageChangeItem, (contract_id)
           (key)
           (before)
           (after))
FC_REFLECT(thinkyoung::blockchain::StorageDataType,
           (storage_type)
           (storage_data)
          )

FC_REFLECT(thinkyoung::blockchain::StorageNullType,
           (raw_storage)
          )

FC_REFLECT(thinkyoung::blockchain::StorageIntType,
           (raw_storage)
          )

FC_REFLECT(thinkyoung::blockchain::StorageBoolType,
           (raw_storage)
          )

FC_REFLECT(thinkyoung::blockchain::StorageNumberType,
           (raw_storage)
          )

FC_REFLECT(thinkyoung::blockchain::StorageStringType,
           (raw_storage)
          )

FC_REFLECT(thinkyoung::blockchain::StorageIntTableType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageBoolTableType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageNumberTableType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageStringTableType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageIntArrayType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageBoolArrayType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageNumberArrayType,
           (raw_storage_map)
          )

FC_REFLECT(thinkyoung::blockchain::StorageStringArrayType,
           (raw_storage_map)
          )
