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
            
            template<typename StorageBaseType, typename StorageContainerType>
            void apply_storage_change(const ContractStorageChangeItem& change_storage, StorageDataType& storage)const {
                //都做反序列化
                std::map<std::string, StorageBaseType> storage_table = storage.as<StorageContainerType>().raw_storage_map;
                std::map<std::string, StorageBaseType> storage_before_table = change_storage.before.as<StorageContainerType>().raw_storage_map;
                std::map<std::string, StorageBaseType> storage_after_table = change_storage.after.as<StorageContainerType>().raw_storage_map;
                //删除所有before map中的
                auto iter_before = storage_before_table.begin();
                
                for (; iter_before != storage_before_table.end(); ++iter_before)
                    storage_table.erase(iter_before->first);
                    
                //增加所有after map中的
                auto iter_after = storage_after_table.begin();
                
                for (; iter_after != storage_after_table.end(); ++iter_after)
                    storage_table[iter_after->first] = iter_after->second;
                    
                //做序列化
                StorageContainerType new_storage;
                new_storage.raw_storage_map = storage_table;
                storage = StorageDataType(new_storage);
            }
            
            void update_contract_storages(const std::string storage_name, const ContractStorageChangeItem& change_storage,
                                          std::map<std::string, StorageDataType>& contract_storages)const {
                if (change_storage.before.storage_type == StorageValueTypes::storage_value_null &&
                        change_storage.after.storage_type != StorageValueTypes::storage_value_null) {
                    StorageDataType storage_data;
                    storage_data.storage_type = change_storage.after.storage_type;
                    storage_data.storage_data = change_storage.after.storage_data;
                    contract_storages.erase(storage_name);
                    contract_storages.emplace(make_pair(storage_name, storage_data));
                    
                } else if (change_storage.before.storage_type != StorageValueTypes::storage_value_null &&
                           change_storage.after.storage_type == StorageValueTypes::storage_value_null) {
                    contract_storages.erase(storage_name);
                    
                } else if (change_storage.before.storage_type != StorageValueTypes::storage_value_null &&
                           change_storage.after.storage_type != StorageValueTypes::storage_value_null) {
                    //基本类型则直接替换
                    //map类型的话先做反序列化, 再做更新, 然后再做序列化后替换
                    auto iter = contract_storages.find(storage_name);
                    FC_ASSERT(iter != contract_storages.end(), "can not get storage_name");
                    auto storage_type = iter->second.storage_type;
                    
                    if (is_any_base_storage_value_type(storage_type))
                        iter->second.storage_data = change_storage.after.storage_data;
                        
                    else if (storage_type == StorageValueTypes::storage_value_int_table)
                        apply_storage_change<LUA_INTEGER, StorageIntTableType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_number_table)
                        apply_storage_change<double, StorageNumberTableType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_bool_table)
                        apply_storage_change<bool, StorageBoolTableType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_string_table)
                        apply_storage_change<std::string, StorageStringTableType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_int_array)
                        apply_storage_change<LUA_INTEGER, StorageIntArrayType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_number_array)
                        apply_storage_change<double, StorageNumberArrayType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_bool_array)
                        apply_storage_change<bool, StorageBoolArrayType>(change_storage, iter->second);
                        
                    else if (storage_type == StorageValueTypes::storage_value_string_array)
                        apply_storage_change<std::string, StorageStringArrayType>(change_storage, iter->second);
                        
                } else {
                    contract_storages.erase(storage_name);
                }
            }
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
