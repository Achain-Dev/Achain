#include <blockchain/StorageTypes.hpp>
#include <glua/thinkyoung_lua_api.h>


namespace thinkyoung {
    namespace blockchain {

        std::map <StorageValueTypes, std::string> storage_type_map = \
        {
            make_pair(storage_value_null, std::string("nil")),
                make_pair(storage_value_int, std::string("int")),
                make_pair(storage_value_number, std::string("number")),
                make_pair(storage_value_bool, std::string("bool")),
                make_pair(storage_value_string, std::string("string")),
                make_pair(storage_value_unknown_table, std::string("Map<unknown type>")),
                make_pair(storage_value_int_table, std::string("Map<int>")),
                make_pair(storage_value_number_table, std::string("Map<number>")),
                make_pair(storage_value_bool_table, std::string("Map<bool>")),
                make_pair(storage_value_string_table, std::string("Map<string>")),
                make_pair(storage_value_unknown_array, std::string("Array<unknown type>")),
                make_pair(storage_value_int_array, std::string("Array<int>")),
                make_pair(storage_value_number_array, std::string("Array<number>")),
                make_pair(storage_value_bool_array, std::string("Array<bool>")),
                make_pair(storage_value_string_array, std::string("Array<string>"))
        };

        const uint8_t StorageNullType::type = storage_value_null;
        const uint8_t StorageIntType::type = storage_value_int;
        const uint8_t StorageNumberType::type = storage_value_number;
        const uint8_t StorageBoolType::type = storage_value_bool;
        const uint8_t StorageStringType::type = storage_value_string;

        const uint8_t StorageIntTableType::type = storage_value_int_table;
        const uint8_t StorageNumberTableType::type = storage_value_number_table;
        const uint8_t StorageBoolTableType::type = storage_value_bool_table;
        const uint8_t StorageStringTableType::type = storage_value_string_table;

        const uint8_t StorageIntArrayType::type = storage_value_int_array;
        const uint8_t StorageNumberArrayType::type = storage_value_number_array;
        const uint8_t StorageBoolArrayType::type = storage_value_bool_array;
        const uint8_t StorageStringArrayType::type = storage_value_string_array;


        StorageDataType StorageDataType::get_storage_data_from_lua_storage(const GluaStorageValue& lua_storage)
        {
            StorageDataType storage_data;

            if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_null)
                storage_data = StorageDataType(StorageNullType());
            else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_int)
                storage_data = StorageDataType(StorageIntType(lua_storage.value.int_value));
            else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_number)
                storage_data = StorageDataType(StorageNumberType(lua_storage.value.number_value));
            else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_bool)
                storage_data = StorageDataType(StorageBoolType(lua_storage.value.bool_value));
            else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_string)
                storage_data = StorageDataType(StorageStringType(string(lua_storage.value.string_value)));
            else if (thinkyoung::blockchain::is_any_array_storage_value_type(lua_storage.type)
                || thinkyoung::blockchain::is_any_table_storage_value_type(lua_storage.type))
            {
                if (get_storage_base_type(lua_storage.type) == thinkyoung::blockchain::StorageValueTypes::storage_value_int)
                {
                    std::map<std::string, LUA_INTEGER> int_map;
                    std::for_each(lua_storage.value.table_value->begin(), lua_storage.value.table_value->end(),
                        [&](const std::pair<std::string, struct GluaStorageValue>& item)
                    {
                        int_map.insert(std::make_pair(item.first, item.second.value.int_value));
                    });

                    if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_int_table)
                        storage_data = StorageDataType(StorageIntTableType(int_map));
                    else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_int_array)
                        storage_data = StorageDataType(StorageIntArrayType(int_map));
                }
                else if (get_storage_base_type(lua_storage.type) == thinkyoung::blockchain::StorageValueTypes::storage_value_number)
                {
                    std::map<std::string, double> number_map;
                    std::for_each(lua_storage.value.table_value->begin(), lua_storage.value.table_value->end(),
                        [&](const std::pair<std::string, struct GluaStorageValue>& item)
                    {
                        number_map.insert(std::make_pair(item.first, item.second.value.number_value));
                    });

                    if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_number_table)
                        storage_data = StorageDataType(StorageNumberTableType(number_map));
                    else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_number_array)
                        storage_data = StorageDataType(StorageNumberArrayType(number_map));
                }
                else if (get_storage_base_type(lua_storage.type) == thinkyoung::blockchain::StorageValueTypes::storage_value_bool)
                {
                    std::map<std::string, bool> bool_map;
                    std::for_each(lua_storage.value.table_value->begin(), lua_storage.value.table_value->end(),
                        [&](const std::pair<std::string, struct GluaStorageValue>& item)
                    {
                        bool_map.insert(std::make_pair(item.first, item.second.value.bool_value));
                    });

                    if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_bool_table)
                        storage_data = StorageDataType(StorageBoolTableType(bool_map));
                    else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_bool_array)
                        storage_data = StorageDataType(StorageBoolArrayType(bool_map));
                }
                else if (get_storage_base_type(lua_storage.type) == thinkyoung::blockchain::StorageValueTypes::storage_value_string)
                {
                    std::map<std::string, string> string_map;
                    std::for_each(lua_storage.value.table_value->begin(), lua_storage.value.table_value->end(),
                        [&](const std::pair<std::string, struct GluaStorageValue>& item)
                    {
                        string_map.insert(std::make_pair(item.first, item.second.value.string_value));
                    });

                    if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_string_table)
                        storage_data = StorageDataType(StorageStringTableType(string_map));
                    else if (lua_storage.type == thinkyoung::blockchain::StorageValueTypes::storage_value_string_array)
                        storage_data = StorageDataType(StorageStringArrayType(string_map));
                }
            }

            return storage_data;
        }

        GluaStorageValue StorageDataType::create_lua_storage_from_storage_data(lua_State *L, const StorageDataType& storage)
        {
            GluaStorageValue lua_storage;
            GluaStorageValue* p_lua_storage = &lua_storage;

            GluaStorageValue null_storage;
            null_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;

            if (storage.storage_type == StorageValueTypes::storage_value_null)
            {
                p_lua_storage->type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
            }
            else if (storage.storage_type == StorageValueTypes::storage_value_int)
            {
                p_lua_storage->type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
                p_lua_storage->value.int_value = storage.as<StorageIntType>().raw_storage;
            }
            else if (storage.storage_type == StorageValueTypes::storage_value_number)
            {
                p_lua_storage->type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
                p_lua_storage->value.number_value = storage.as<StorageNumberType>().raw_storage;
            }
            else if (storage.storage_type == StorageValueTypes::storage_value_bool)
            {
                p_lua_storage->type = thinkyoung::blockchain::StorageValueTypes::storage_value_bool;
                p_lua_storage->value.bool_value = storage.as<StorageBoolType>().raw_storage;
            }
            else if (storage.storage_type == StorageValueTypes::storage_value_string)
            {
                p_lua_storage->type = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
                std::string storage_string = storage.as<StorageStringType>().raw_storage;
                size_t string_len = storage_string.length();

                p_lua_storage->value.string_value = thinkyoung::lua::lib::malloc_managed_string(L, string_len + 1);
                if (!p_lua_storage->value.string_value)
                    return null_storage;

                strncpy(p_lua_storage->value.string_value, storage_string.c_str(), string_len);
                p_lua_storage->value.string_value[string_len] = '\0';
            }
            else if (
                thinkyoung::blockchain::is_any_array_storage_value_type(storage.storage_type)
                || thinkyoung::blockchain::is_any_table_storage_value_type(storage.storage_type)
                )
            {
                p_lua_storage->type = storage.storage_type;
                p_lua_storage->value.table_value = thinkyoung::lua::lib::create_managed_lua_table_map(L);
                if (!p_lua_storage->value.table_value)
                {
                    return null_storage;
                }

                if (storage.storage_type == StorageValueTypes::storage_value_int_table ||
                    storage.storage_type == StorageValueTypes::storage_value_int_array)
                {
                    std::map<std::string, LUA_INTEGER> int_table;
                    if (storage.storage_type == StorageValueTypes::storage_value_int_table)
                        int_table = storage.as<StorageIntTableType>().raw_storage_map;
                    else if (storage.storage_type == StorageValueTypes::storage_value_int_array)
                        int_table = storage.as<StorageIntArrayType>().raw_storage_map;

                    for (const auto& item : int_table)
                    {
                        GluaStorageValue base_storage;
                        base_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
                        base_storage.value.int_value = item.second;
                        p_lua_storage->value.table_value->insert(make_pair(item.first, base_storage));
                    }
                }
                else if (storage.storage_type == StorageValueTypes::storage_value_number_table ||
                    storage.storage_type == StorageValueTypes::storage_value_number_array)
                {
                    std::map<std::string, double> double_table;
                    if (storage.storage_type == StorageValueTypes::storage_value_number_table)
                        double_table = storage.as<StorageNumberTableType>().raw_storage_map;
                    else if (storage.storage_type == StorageValueTypes::storage_value_number_array)
                        double_table = storage.as<StorageNumberArrayType>().raw_storage_map;

                    for (const auto& item : double_table)
                    {
                        GluaStorageValue base_storage;
                        base_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
                        base_storage.value.number_value = item.second;
                        p_lua_storage->value.table_value->insert(make_pair(item.first, base_storage));
                    }
                }
                else if (storage.storage_type == StorageValueTypes::storage_value_bool_table ||
                    storage.storage_type == StorageValueTypes::storage_value_bool_array)
                {
                    std::map<std::string, bool> bool_table;
                    if (storage.storage_type == StorageValueTypes::storage_value_bool_table)
                        bool_table = storage.as<StorageBoolTableType>().raw_storage_map;
                    else if (storage.storage_type == StorageValueTypes::storage_value_bool_array)
                        bool_table = storage.as<StorageBoolArrayType>().raw_storage_map;

                    for (const auto& item : bool_table)
                    {
                        GluaStorageValue base_storage;
                        base_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_bool;
                        base_storage.value.bool_value = item.second;
                        p_lua_storage->value.table_value->insert(make_pair(item.first, base_storage));
                    }
                }
                else if (storage.storage_type == StorageValueTypes::storage_value_string_table ||
                    storage.storage_type == StorageValueTypes::storage_value_string_array)
                {
                    std::map<std::string, string> string_table;
                    if (storage.storage_type == StorageValueTypes::storage_value_string_table)
                        string_table = storage.as<StorageStringTableType>().raw_storage_map;
                    else if (storage.storage_type == StorageValueTypes::storage_value_string_array)
                        string_table = storage.as<StorageStringArrayType>().raw_storage_map;

                    for (const auto& item : string_table)
                    {
                        GluaStorageValue base_storage;
                        base_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
                        size_t string_len = item.second.length();

                        base_storage.value.string_value = thinkyoung::lua::lib::malloc_managed_string(L, string_len + 1);
                        if (!base_storage.value.string_value)
                            return null_storage;

                        strncpy(base_storage.value.string_value, item.second.c_str(), string_len);
                        base_storage.value.string_value[string_len] = '\0';
                        p_lua_storage->value.table_value->insert(make_pair(item.first, base_storage));
                    }
                }
            }

            return lua_storage;
        }

    }
}