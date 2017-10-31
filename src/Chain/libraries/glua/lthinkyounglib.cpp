/**
* thinkyoung core api module for glua code
*/

#include <glua/lprefix.h>

#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <glua/lthinkyounglib.h>

using thinkyoung::lua::api::global_glua_chain_api;

#undef VERSION
#define VERSION	(l_mathop(3.0))


static int unit_test_check_equal_number(lua_State *L)
{
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    if (abs(x - y) < 0.00001)
    {
        return 1;
    }
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "check equal error happen");
    return 0;
}

static int unit_test_check(lua_State *L)
{
    int r = (int)luaL_checkinteger(L, 1);
    if (r)
    {
        return 1;
    }
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "check condition error happen");
    return 0;
}

static int stop_vm(lua_State *L)
{
    thinkyoung::lua::lib::notify_lua_state_stop(L);
    return 0;
}

static int throw_error(lua_State *L)
{
    if (lua_gettop(L) < 1 || !lua_isstring(L, -1))
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_THROW_ERROR, "empty error");
        // thinkyoung::lua::lib::notify_lua_state_stop(L);
        return 0;
    }
    const char *msg = luaL_checkstring(L, -1);
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_THROW_ERROR, msg);
    L->force_stopping = true;
    // thinkyoung::lua::lib::notify_lua_state_stop(L);
    return 0;
}

static const char *empty_string = "";

/**
 * arg1 is contract, arg2 is storage property name
 */
static int register_storage(lua_State *L)
{
    if (!lua_istable(L, 1))
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "first argument of register_storage must be contract");
        return 0;
    }
    lua_getfield(L, 1, "name");
    auto contract_name = (const char*)luaL_checkstring(L, -1);
    if (!contract_name)
        contract_name = empty_string;
    lua_pop(L, 1);
    char *name = (char*)luaL_checkstring(L, 2);
    if (thinkyoung::lua::lib::check_in_lua_sandbox(L))
    {
        printf("register storage in sandbox %s:%s\n", contract_name, name);
        return 0;
    }
    printf("register storage %s:%s\n", contract_name, name);
    global_glua_chain_api->register_storage(L, contract_name, name);
    return 0;
}

static GluaStorageTableReadList *get_or_init_storage_table_read_list(lua_State *L)
{
    GluaStateValueNode state_value_node = thinkyoung::lua::lib::get_lua_state_value_node(L, LUA_STORAGE_READ_TABLES_KEY);
    GluaStorageTableReadList *list = nullptr;;
    if (state_value_node.type != LUA_STATE_VALUE_POINTER || nullptr == state_value_node.value.pointer_value)
    {
        list = (GluaStorageTableReadList*)malloc(sizeof(GluaStorageTableReadList));
        new (list)GluaStorageTableReadList();
        GluaStateValue value_to_store;
        value_to_store.pointer_value = list;
        thinkyoung::lua::lib::set_lua_state_value(L, LUA_STORAGE_READ_TABLES_KEY, value_to_store, LUA_STATE_VALUE_POINTER);
    }
    else
    {
        list = (GluaStorageTableReadList*)state_value_node.value.pointer_value;
    }
    return list;
}

static struct GluaStorageValue get_last_storage_changed_value(lua_State *L, const char *contract_id,
  GluaStorageChangeList *list, const std::string &key)
{
    struct GluaStorageValue nil_value;
    nil_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
    auto post_when_read_table = [&](GluaStorageValue value) {
        if (lua_storage_is_table(value.type))
        {
            // when read a table, snapshot it and record it, when commit, merge to the changes
            GluaStorageTableReadList *table_read_list = get_or_init_storage_table_read_list(L);
            if (table_read_list)
            {
                for (auto it = table_read_list->begin(); it != table_read_list->end(); ++it)
                {
                    if (it->contract_id == std::string(contract_id) && it->key == key)
                    {
                        return;
                    }
                }
                GluaStorageChangeItem change_item;
                change_item.contract_id = contract_id;
                change_item.key = key;
                change_item.before = value;
                table_read_list->push_back(change_item);
            }
        }
    };
    if (!list || list->size() < 1)
    {
        auto value = global_glua_chain_api->get_storage_value_from_thinkyoung_by_address(L, contract_id, key);
        post_when_read_table(value);
        // 如果是第一次读取，要把这个读取结果缓存住，避免重复从区块链上读取数据
        
        if (!list) {
          list = (GluaStorageChangeList*)malloc(sizeof(GluaStorageChangeList));
          new (list)GluaStorageChangeList();
          GluaStateValue value_to_store;
          value_to_store.pointer_value = list;
          thinkyoung::lua::lib::set_lua_state_value(L, LUA_STORAGE_CHANGELIST_KEY, value_to_store, LUA_STATE_VALUE_POINTER);
        }
        GluaStorageChangeItem change_item;
        change_item.before = value;
        change_item.after = value;
        change_item.contract_id = contract_id;
        change_item.key = key;
        list->push_back(change_item);
        
        return value;
    }
    for (auto it = list->rbegin(); it != list->rend(); ++it)
    {
        if (it->contract_id == std::string(contract_id) && it->key == key)
            return it->after;
    }
    auto value = global_glua_chain_api->get_storage_value_from_thinkyoung_by_address(L, contract_id, key);
    post_when_read_table(value);
    return value;
}

static std::string global_key_for_storage_prop(std::string contract_id, std::string key)
{
    return "gk_" + contract_id + "__" + key;
}

bool lua_push_storage_value(lua_State *L, const GluaStorageValue &value);

#define max_support_array_size 10000000  // 目前最大支持的array size
static bool lua_push_storage_table_value(lua_State *L, GluaTableMap *map, int type)
{
    if (nullptr == L || nullptr == map)
        return false;
    lua_createtable(L, 0, 0);
    // when is array, push as array
    if (lua_storage_is_array((thinkyoung::blockchain::StorageValueTypes) type))
    {
        // sort the unordered_map, then push items keys, 1,2,3,... one by one into the new table array
        auto count = 0;

        while (count < max_support_array_size)
        {
            ++count;
            std::string key = std::to_string(count);
            if (map->find(key) == map->end())
                break;
            auto value = map->at(key);
            lua_push_storage_value(L, value);
            lua_seti(L, -2, count);
        }
    }
    else
    {
        for (auto it = map->begin(); it != map->end(); ++it)
        {
            const auto &key = it->first;
            GluaStorageValue value = it->second;
            lua_push_storage_value(L, value);
            lua_setfield(L, -2, key.c_str());
        }
    }
    return true;
}

bool lua_push_storage_value(lua_State *L, const GluaStorageValue &value)
{
    if (nullptr == L)
        return false;
    switch (value.type)
    {
	case thinkyoung::blockchain::StorageValueTypes::storage_value_int: lua_pushinteger(L, value.value.int_value); break;
	case thinkyoung::blockchain::StorageValueTypes::storage_value_bool: lua_pushboolean(L, value.value.bool_value); break;
	case thinkyoung::blockchain::StorageValueTypes::storage_value_number: lua_pushnumber(L, value.value.number_value); break;
	case thinkyoung::blockchain::StorageValueTypes::storage_value_null: lua_pushnil(L); break;
	case thinkyoung::blockchain::StorageValueTypes::storage_value_string: lua_pushstring(L, value.value.string_value); break;
	case thinkyoung::blockchain::StorageValueTypes::storage_value_stream: {
		auto stream = (thinkyoung::lua::lib::GluaByteStream*) value.value.userdata_value;
		lua_pushlightuserdata(L, (void*) stream);
		luaL_getmetatable(L, "GluaByteStream_metatable");
		lua_setmetatable(L, -2);
	} break;
	default: {
		if(thinkyoung::blockchain::is_any_table_storage_value_type(value.type)
			|| thinkyoung::blockchain::is_any_array_storage_value_type(value.type))
		{
			lua_push_storage_table_value(L, value.value.table_value, value.type);
			break;
		}
		lua_pushnil(L); 
	}
    }
    return true;
}

static GluaStorageChangeItem diff_storage_change_if_is_table(lua_State *L, GluaStorageChangeItem change_item)
{
    if (!lua_storage_is_table(change_item.after.type))
        return change_item;

    if (!lua_storage_is_table(change_item.before.type) || !lua_storage_is_table(change_item.after.type))
        return change_item;
	// FIXME: 考虑这里用malloc再手动placement new是否有问题
    auto new_before = (GluaTableMapP) malloc(sizeof(GluaTableMap));
    new (new_before)GluaTableMap();
    auto new_after = (GluaTableMapP) malloc(sizeof(GluaTableMap));
    new (new_after)GluaTableMap();
    for (auto it1 = change_item.before.value.table_value->begin(); it1 != change_item.before.value.table_value->end(); ++it1)
    {
        auto found = change_item.after.value.table_value->find(it1->first);
        if (found == change_item.after.value.table_value->end())
        {
            new_before->insert(new_before->end(), std::make_pair(it1->first, it1->second));
            continue;
        }
        if (found->second.equals(it1->second))
        {
            continue;
        }
        if (it1->second.type == thinkyoung::blockchain::StorageValueTypes::storage_value_null)
            continue;
        new_before->insert(new_before->end(), std::make_pair(it1->first, it1->second));
        new_after->insert(new_after->end(), std::make_pair(found->first, found->second));
    }
    for (auto it1 = change_item.after.value.table_value->begin(); it1 != change_item.after.value.table_value->end(); ++it1)
    {
        auto found = change_item.before.value.table_value->find(it1->first);
        if (found == change_item.before.value.table_value->end())
        {
            new_after->insert(new_after->end(), std::make_pair(it1->first, it1->second));
        }
    }
    change_item.before.value.table_value = new_before;
    change_item.after.value.table_value = new_after;
    return change_item;
}


static bool has_property_changed_in_changelist(GluaStorageChangeList *list, std::string contract_id, std::string name)
{
    if (nullptr == list)
        return false;
    for (auto it = list->begin(); it != list->end(); ++it)
    {
        auto item = *it;
        if (item.contract_id == contract_id && item.key == name)
            return true;
    }
    return false;
}

// FIXME: use light userdata or userdata to reconstructure storegae

bool luaL_commit_storage_changes(lua_State *L)
{
  // TODO: 新storage操作方式，commit的时候再统一比较storage变化
  /*
	auto maybe_storage_changed_contract_ids = thinkyoung::lua::lib::get_maybe_storage_changed_contract_ids_vector(L, false);
  if (maybe_storage_changed_contract_ids && maybe_storage_changed_contract_ids->size()>0)
  {
    // TODO: 如何获取到这些合约的内存中的对象
    // printf("");
  }*/

    GluaStateValueNode storage_changelist_node = thinkyoung::lua::lib::get_lua_state_value_node(L, LUA_STORAGE_CHANGELIST_KEY);
    if (global_glua_chain_api->has_exception(L))
    {
        if (storage_changelist_node.type == LUA_STATE_VALUE_POINTER && nullptr != storage_changelist_node.value.pointer_value)
        {
            GluaStorageChangeList *list = (GluaStorageChangeList*)storage_changelist_node.value.pointer_value;
            list->clear();
        }
        return false;
    }
    // merge changes

    std::unordered_map<std::string, std::shared_ptr<std::unordered_map<std::string, GluaStorageChangeItem>>> changes;
    GluaStorageTableReadList *table_read_list = get_or_init_storage_table_read_list(L);
    if (storage_changelist_node.type != LUA_STATE_VALUE_POINTER && table_read_list)
    {
        storage_changelist_node.type = LUA_STATE_VALUE_POINTER;
        auto *list = (GluaStorageChangeList*)malloc(sizeof(GluaStorageChangeList));
        new (list)GluaStorageChangeList();
        GluaStateValue value_to_store;
        value_to_store.pointer_value = list;
        thinkyoung::lua::lib::set_lua_state_value(L, LUA_STORAGE_CHANGELIST_KEY, value_to_store, LUA_STATE_VALUE_POINTER);
        storage_changelist_node.value.pointer_value = list;
    }
    if (storage_changelist_node.type == LUA_STATE_VALUE_POINTER && nullptr != storage_changelist_node.value.pointer_value)
    {
        GluaStorageChangeList *list = (GluaStorageChangeList*)storage_changelist_node.value.pointer_value;
        // merge initial tables here
        if (table_read_list)
        {
            for (auto it = table_read_list->begin(); it != table_read_list->end(); ++it)
            {
                auto change_item = *it;
                std::string global_skey = global_key_for_storage_prop(change_item.contract_id, change_item.key);
                lua_getglobal(L, global_skey.c_str());
                if (lua_istable(L, -1))
                {
                    auto after_value = lua_type_to_storage_value_type(L, -1, 0);
                    // 检查changelist是否有这个属性的改变项，有的话不用readvalue
                    change_item.after = after_value;

					// FIXME: a= {}, storage.a = a, a['name'] = 123, storage.a = {}的情况下怎么处理? 考虑把storage还是做成一个table来处理
                    //if (!has_property_changed_in_changelist(list, change_item.contract_id, change_item.key))
                    // {
						if(!change_item.before.equals(change_item.after))
						{
							list->push_back(change_item);
						}
                    // }
                }
                lua_pop(L, 1);
            }
            table_read_list->clear();
        }

        for (auto it = list->begin(); it != list->end(); ++it)
        {
            GluaStorageChangeItem change_item = *it;
            auto found = changes.find(change_item.contract_id);
            if (found != changes.end())
            {
                auto contract_changes = found->second;
                auto found_key = contract_changes->find(change_item.key);
                if (found_key != contract_changes->end())
                {
                    found_key->second.after = change_item.after;
                }
                else
                {
                    contract_changes->insert(std::make_pair(change_item.key, change_item));
                }
            }
            else
            {
                auto contract_changes = std::make_shared<std::unordered_map<std::string, GluaStorageChangeItem>>();
                contract_changes->insert(contract_changes->end(), std::make_pair(change_item.key, change_item));
                changes.insert(changes.end(), std::make_pair(change_item.contract_id, contract_changes));
            }
        }
    }
    else
    {
        return false;
    }

    // 如果是调用合约初始化init函数，并且storage不为空，而changes为空，报错
    if (thinkyoung::lua::lib::is_calling_contract_init_api(L)
      && changes.size()==0)
    {
      auto starting_contract_address = thinkyoung::lua::lib::get_starting_contract_address(L);
      auto stream = global_glua_chain_api->open_contract_by_address(L, starting_contract_address.c_str());
      if (stream && stream->contract_storage_properties.size() > 0)
      {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "some storage of this contract not init");
        return false;
      }
    }

    for (auto it = changes.begin(); it != changes.end(); ++it)
    {
		auto stream = global_glua_chain_api->open_contract_by_address(L, it->first.c_str());
		if (!stream)
		{
			global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "Can't get contract info by contract address %s", it->first.c_str());
			return false;
		}
		// 如果是调用init API，并且storage所处的合约地址和初始调用的合约地址一样，则要检查storage的after类型是否能和编译期时的storage类型匹配
		bool is_in_starting_contract_init = false;
		if (thinkyoung::lua::lib::is_calling_contract_init_api(L))
		{
			auto starting_contract_address = thinkyoung::lua::lib::get_starting_contract_address(L);
			if (it->first == starting_contract_address)
			{
				// 检查storage的after类型是否能和编译期时的storage类型匹配
				is_in_starting_contract_init = true;
				const auto &storage_properties_in_chain = stream->contract_storage_properties;
				if(it->second->size()!=storage_properties_in_chain.size())
				{
					global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "some storage of this contract not init");
					return false;
				}
				for(auto &p1 : *(it->second))
				{
					//if (p1.second.before.type == thinkyoung::blockchain::StorageValueTypes::storage_value_null)
					//	continue;
					if(storage_properties_in_chain.find(p1.first) == storage_properties_in_chain.end())
					{
						global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "Can't find storage %s", p1.first.c_str());
						return false;
					}
					auto storage_info_in_chain = storage_properties_in_chain.at(p1.first);
					if(thinkyoung::blockchain::is_any_table_storage_value_type(p1.second.after.type)
						|| thinkyoung::blockchain::is_any_array_storage_value_type(p1.second.after.type))
					{
						// 运行时[]也会变现为{}
						if(!thinkyoung::blockchain::is_any_table_storage_value_type(storage_info_in_chain)
							&& !thinkyoung::blockchain::is_any_array_storage_value_type(storage_info_in_chain))
						{
							global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage %s type not matched in chain", p1.first.c_str());
							return false;
						}
						if(p1.second.after.value.table_value->size()>0)
						{
							for(auto &item_in_table : *(p1.second.after.value.table_value))
							{
								item_in_table.second.try_parse_type(thinkyoung::blockchain::get_storage_base_type(storage_info_in_chain));
							}
							auto item_after = p1.second.after.value.table_value->begin()->second;
							if(item_after.type != thinkyoung::blockchain::get_item_type_in_table_or_array(storage_info_in_chain))
							{
								global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage %s type not matched in chain", p1.first.c_str());
								return false;
							}
						}

						// 检查after值类型和链上值类型是否匹配
						if(p1.second.after.type == thinkyoung::blockchain::storage_value_unknown_table
							|| p1.second.after.type == thinkyoung::blockchain::storage_value_unknown_array)
						{
							p1.second.after.type = storage_info_in_chain;
						}
					} else if(p1.second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_number
						&& storage_info_in_chain==thinkyoung::blockchain::StorageValueTypes::storage_value_int)
					{
						p1.second.after.type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
						p1.second.after.value.int_value = (lua_Integer)p1.second.after.value.number_value;
					}
					else if (p1.second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_int
						&& storage_info_in_chain == thinkyoung::blockchain::StorageValueTypes::storage_value_number)
					{
						p1.second.after.type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
						p1.second.after.value.number_value = (lua_Number)p1.second.after.value.int_value;
					}
				}
			}
		}

        auto it2 = it->second->begin();
        while (it2 != it->second->end())
        {
            if (lua_storage_is_table(it2->second.after.type))
            {
				// 如果before是空table，after是array时
				// 如果before是array, after是空table时
				if (lua_storage_is_array(it2->second.before.type) && it2->second.after.value.table_value->size() == 0)
					it2->second.after.type = it2->second.before.type;
                else if (lua_storage_is_table(it2->second.before.type) && it2->second.before.value.table_value->size()>0)
                    it2->second.after.type = it2->second.before.type;

                // just save table diff
                it2->second = diff_storage_change_if_is_table(L, it2->second);
            }

			// storage的变化要检查对应合约的编译期类型，并适当修改commit的类型
			if(!is_in_starting_contract_init)
			{
				const auto &storage_properties_in_chain = stream->contract_storage_properties;

				if (it2->second.before.type != thinkyoung::blockchain::StorageValueTypes::storage_value_null
					&& storage_properties_in_chain.find(it2->first) != storage_properties_in_chain.end())
				{
					auto storage_info_in_chain = storage_properties_in_chain.at(it2->first);
					if (thinkyoung::blockchain::is_any_table_storage_value_type(it2->second.after.type)
						|| thinkyoung::blockchain::is_any_array_storage_value_type(it2->second.after.type))
					{
						// 运行时[]也会变现为{}
						if (!thinkyoung::blockchain::is_any_table_storage_value_type(storage_info_in_chain)
							&& !thinkyoung::blockchain::is_any_array_storage_value_type(storage_info_in_chain))
						{
							global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage %s type not matched in chain", it2->first.c_str());
							return false;
						}
						if (it2->second.after.value.table_value->size() > 0)
						{
							for(auto &item_in_table : *(it2->second.after.value.table_value))
							{
								item_in_table.second.try_parse_type(thinkyoung::blockchain::get_storage_base_type(storage_info_in_chain));
							}
							auto item_after = it2->second.after.value.table_value->begin()->second;
							if (item_after.type != thinkyoung::blockchain::get_item_type_in_table_or_array(storage_info_in_chain))
							{
								global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage %s type not matched in chain", it2->first.c_str());
								return false;
							}
						}
						// 检查after值类型和链上值类型是否匹配
						if (it2->second.after.type == thinkyoung::blockchain::storage_value_unknown_table
							|| it2->second.after.type == thinkyoung::blockchain::storage_value_unknown_array)
						{
							it2->second.after.type = storage_info_in_chain;
						}
					}
					else if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_number
						&& storage_info_in_chain == thinkyoung::blockchain::StorageValueTypes::storage_value_int)
					{
						it2->second.after.type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
						it2->second.after.value.int_value = (lua_Integer)it2->second.after.value.number_value;
					}
					else if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_int
						&& storage_info_in_chain == thinkyoung::blockchain::StorageValueTypes::storage_value_number)
					{
						it2->second.after.type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
						it2->second.after.value.number_value = (lua_Number)it2->second.after.value.int_value;
					}
				}
			}

			// map/array的值类型要是一致的并且是基本类型
			if(lua_storage_is_table(it2->second.after.type))
			{
				thinkyoung::blockchain::StorageValueTypes item_value_type;
				bool is_first = true;
				for(const auto &p : *it2->second.after.value.table_value)
				{
					if(is_first)
					{
						item_value_type = p.second.type;
						continue;
					}
					is_first = true;
					if(!GluaStorageValue::is_same_base_type_with_type_parse(p.second.type, item_value_type))
					{
						global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR,
							"array/map's value type must be same in contract storage");
						return false;
					}
				}
			}

            if (it2->second.after.type == it2->second.before.type)
            {
                if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_int
					&& it2->second.after.value.int_value == it2->second.before.value.int_value)
                {
                    it2 = it->second->erase(it2);
                    continue;
                }
                else if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_bool
					&& it2->second.after.value.bool_value == it2->second.before.value.bool_value)
                {
                    it2 = it->second->erase(it2);
                    continue;
                }
                else if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_number
					&& abs(it2->second.after.value.number_value - it2->second.before.value.number_value) < 0.0000001)
                {
                    it2 = it->second->erase(it2);
                    continue;
                }
                else if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_string
					&& strcmp(it2->second.after.value.string_value, it2->second.before.value.string_value) == 0)
                {
                    it2 = it->second->erase(it2);
                    continue;
                }
				else if (it2->second.after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_stream
					&& ((thinkyoung::lua::lib::GluaByteStream*) it2->second.after.value.userdata_value)->equals((thinkyoung::lua::lib::GluaByteStream*) it2->second.before.value.userdata_value))
				{
					it2 = it->second->erase(it2);
					continue;
				}
                else if (!lua_storage_is_table(it2->second.after.type)
					&& it2->second.after.type != thinkyoung::blockchain::StorageValueTypes::storage_value_string
					&& memcmp(&it2->second.after.value, &it2->second.before.value, sizeof(GluaStorageValueUnion)) == 0)
                {
                    it2 = it->second->erase(it2);
                    continue;
                }
                else if (lua_storage_is_table(it2->second.after.type))
                {
                    if ((it2->second.after.value.table_value->size() == 0
                        && it2->second.before.value.table_value->size() == 0))
                    {
                        it2 = it->second->erase(it2);
                        continue;
                    }
                }
            }
            ++it2;
        }
    }
    // commit changes to thinkyoung

    if (thinkyoung::lua::lib::check_in_lua_sandbox(L))
    {
        printf("commit storage changes in sandbox\n");
        return false;
    }
    auto result = global_glua_chain_api->commit_storage_changes_to_thinkyoung(L, changes);
    if (storage_changelist_node.type == LUA_STATE_VALUE_POINTER && nullptr != storage_changelist_node.value.pointer_value)
    {
        GluaStorageChangeList *list = (GluaStorageChangeList*)storage_changelist_node.value.pointer_value;
        list->clear();
    }
    return result;
}

/************************************************************************/
/*    获取操作合约代码的storage的代码直接出现在哪个合约，合约只能操作本身的storage，不能操作其他合约的storage */
/************************************************************************/
static const char *get_contract_id_in_storage_operation(lua_State *L)
{
	const auto &contract_id = thinkyoung::lua::lib::get_current_using_contract_id(L);
	auto contract_id_str = thinkyoung::lua::lib::malloc_and_copy_string(L, contract_id.c_str());
	return contract_id_str;
}

static std::string get_contract_id_string_in_storage_operation(lua_State *L)
{
  return thinkyoung::lua::lib::get_current_using_contract_id(L);
}


namespace glua {
	namespace lib {

		int thinkyounglib_get_storage_impl(lua_State *L,
			const char *contract_id, const char *name)
		{
            thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
			
			const auto &code_contract_id = get_contract_id_string_in_storage_operation(L);
			if (code_contract_id != contract_id)
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract can only access its own storage directly");
				thinkyoung::lua::lib::notify_lua_state_stop(L);
				L->force_stopping = true;
				return 0;
			}
			const auto &global_key = global_key_for_storage_prop(contract_id, name);
			lua_getglobal(L, global_key.c_str());
			if (lua_istable(L, -1))
			{
				// auto value = lua_type_to_storage_value_type(L, -1, 0);
				return 1;
			}
			lua_pop(L, 1);
			// printf("get storage %s:%s\n", contract_name, name);
			const auto &state_value_node = thinkyoung::lua::lib::get_lua_state_value_node(L, LUA_STORAGE_CHANGELIST_KEY);
			int result;
			if (state_value_node.type != LUA_STATE_VALUE_POINTER || !state_value_node.value.pointer_value)
			{
				const auto &value = get_last_storage_changed_value(L, contract_id, nullptr, std::string(name));
				lua_push_storage_value(L, value);
				if (lua_storage_is_table(value.type))
				{
					lua_pushvalue(L, -1);
					lua_setglobal(L, global_key.c_str());
                    // thinkyoung::lua::lib::add_maybe_storage_changed_contract_id(L, contract_id);
				}
				result = 1;
			}
			else if (thinkyoung::lua::lib::check_in_lua_sandbox(L))
			{
				printf("get storage in sandbox\n");
				lua_pushnil(L);
				result = 1;
			}
			else
			{
				GluaStorageChangeList *list = (GluaStorageChangeList*)state_value_node.value.pointer_value;
				const auto &value = get_last_storage_changed_value(L, contract_id, list, std::string(name));
				lua_push_storage_value(L, value);
				if (lua_storage_is_table(value.type))
				{
					lua_pushvalue(L, -1);
					lua_setglobal(L, global_key_for_storage_prop(contract_id, name).c_str());
				}
				result = 1;
			}
			return result;
		}

        // TODO: 读写storage的时候记录本次调用一共涉及了哪些合约的storage读table/写任何类型数据
        // 然后在commit的时候取出最新数据来比较

		/**
		* arg1 is contract, arg2 is storage property name
		*/
		int thinkyounglib_get_storage(lua_State *L)
		{
            thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
			if (lua_gettop(L) < 2)
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "get_storage need 2 arguments");
				thinkyoung::lua::lib::notify_lua_state_stop(L);
				return 0;
			}
			if (!lua_istable(L, 1))
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "first argument of get_storage must be contract");
				return 0;
			}
			lua_getfield(L, 1, "id");
			const char *contract_id = luaL_checkstring(L, -1);
			if (!contract_id)
				contract_id = empty_string;
			if (!lua_isstring(L, 2) && !lua_isnumber(L, 2))
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "second argument of set_storage must be string or integer");
				thinkyoung::lua::lib::notify_lua_state_stop(L);
				L->force_stopping = true;
				return 0;
			}
			const char *name = luaL_checkstring(L, 2);
            return thinkyounglib_get_storage_impl(L, contract_id, name);
		}

        int thinkyounglib_set_storage_impl(lua_State *L,
          const char *contract_id, const char *name, int value_index)
        {
          const auto &code_contract_id = get_contract_id_string_in_storage_operation(L);
          if (code_contract_id != contract_id && code_contract_id != contract_id)
          {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract can only access its own storage directly");
            thinkyoung::lua::lib::notify_lua_state_stop(L);
            L->force_stopping = true;
            return 0;
          }
          if (!name || strlen(name) < 1)
          {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "second argument of set_storage must be name");
            return 0;
          }

          // thinkyoung::lua::lib::add_maybe_storage_changed_contract_id(L, contract_id);

		  // FIXME: 这里如果是table，每次创建新对象，占用内存太大了，而且读取也太慢了
          // FIXME: 考虑commit的时候再去读取storage的变化，不要每次都改
          const auto &arg2 = lua_type_to_storage_value_type(L, value_index, 0);

          if (lua_istable(L, value_index))
          {
            // 如果是table，要加入read_list，因为可能直接修改它
            lua_pushvalue(L, value_index);
            lua_setglobal(L, global_key_for_storage_prop(contract_id, name).c_str());
            auto *table_read_list = get_or_init_storage_table_read_list(L);
            if (table_read_list)
            {
              bool found = false;
              for (auto it = table_read_list->begin(); it != table_read_list->end(); ++it)
              {
                if (it->contract_id == std::string(contract_id) && it->key == name)
                {
                  found = true;
                  break;
                }
              }
              if (!found)
              {
                GluaStorageChangeItem change_item;
                change_item.contract_id = contract_id;
                change_item.key = name;
                change_item.before = arg2;
                change_item.after = arg2;
                // TODO: 为了避免arg2太多占用内存，合并历史，释放多余对象
                table_read_list->push_back(change_item);
              }
            }
          }
          /*
          if (arg2.type >= LVALUE_NOT_SUPPORT)
          {
          global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "third argument of set_storage must be value");
          return 0;
          }
          */

          // log the value before and the new value
          GluaStateValueNode state_value_node = thinkyoung::lua::lib::get_lua_state_value_node(L, LUA_STORAGE_CHANGELIST_KEY);
          GluaStorageChangeList *list;
          if (state_value_node.type != LUA_STATE_VALUE_POINTER || nullptr == state_value_node.value.pointer_value)
          {
            list = (GluaStorageChangeList*)malloc(sizeof(GluaStorageChangeList));
            new (list)GluaStorageChangeList();
            GluaStateValue value_to_store;
            value_to_store.pointer_value = list;
            thinkyoung::lua::lib::set_lua_state_value(L, LUA_STORAGE_CHANGELIST_KEY, value_to_store, LUA_STATE_VALUE_POINTER);
          }
          else
          {
            list = (GluaStorageChangeList*)state_value_node.value.pointer_value;
          }
          if (thinkyoung::lua::lib::check_in_lua_sandbox(L))
          {
            printf("set storage in sandbox\n");
            return 0;
          }
          std::string name_str(name);
          auto before = get_last_storage_changed_value(L, contract_id, list, name_str);
          auto after = arg2;
          if (after.type == thinkyoung::blockchain::StorageValueTypes::storage_value_null)
          {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, (name_str + "storage can't change to nil").c_str());
            thinkyoung::lua::lib::notify_lua_state_stop(L);
            return 0;
          }
          if (before.type != thinkyoung::blockchain::StorageValueTypes::storage_value_null
            && (before.type != after.type && !lua_storage_is_table(before.type)))
          {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, (std::string(name) + "storage can't change type").c_str());
            thinkyoung::lua::lib::notify_lua_state_stop(L);
            return 0;
          }

          // can't create new storage index outside init
          if (!thinkyoung::lua::lib::is_calling_contract_init_api(L)
            && before.type == thinkyoung::blockchain::StorageValueTypes::storage_value_null)
          {
            // when not in init api
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, (std::string(name) + "storage can't register storage after inited").c_str());
            thinkyoung::lua::lib::notify_lua_state_stop(L);
            return 0;
          }

          // disable nested map
          if (lua_storage_is_table(after.type))
          {
            auto inner_table = after.value.table_value;
            if (nullptr != inner_table)
            {
              for (auto it = inner_table->begin(); it != inner_table->end(); ++it)
              {
                if (lua_storage_is_table(it->second.type))
                {
                  global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage not support nested map");
                  thinkyoung::lua::lib::notify_lua_state_stop(L);
                  return 0;
                }
              }
            }
          }

          if (lua_storage_is_table(after.type))
          {
            // table properties can't change type except change to nil(in whole history in this lua_State)
            // value type must be same in table in storage
            int table_value_type = -1;
            if (lua_storage_is_table(before.type))
            {
              for (auto it = before.value.table_value->begin(); it != before.value.table_value->end(); ++it)
              {
                auto found = after.value.table_value->find(it->first);
                if (it->second.type != thinkyoung::blockchain::StorageValueTypes::storage_value_null)
                {
                  if (table_value_type < 0)
                  {
                    table_value_type = it->second.type;
                  }
                  else
                  {
                    if (table_value_type != it->second.type)
                    {
                      global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage table type must be same");
                      thinkyoung::lua::lib::notify_lua_state_stop(L);
                      return 0;
                    }
                  }
                }
              }
            }
            for (auto it = after.value.table_value->begin(); it != after.value.table_value->end(); ++it)
            {
              if (it->second.type != thinkyoung::blockchain::StorageValueTypes::storage_value_null)
              {
                if (table_value_type < 0)
                {
                  table_value_type = it->second.type;
                }
                else
                {
                  if (table_value_type != it->second.type)
                  {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage table type must be same");
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                  }
                }
              }
            }
            if ((!lua_storage_is_table(before.type) || before.value.table_value->size() < 1) && after.value.table_value->size() > 0)
            {
              // if before table is empty and after table not empty, search type before
              for (auto it = list->begin(); it != list->end(); ++it)
              {
                if (it->contract_id == std::string(contract_id) && it->key == std::string(name))
                {
                  if (lua_storage_is_table(it->after.type) && it->after.value.table_value->size() > 0)
                  {
                    for (auto it2 = it->after.value.table_value->begin(); it2 != it->after.value.table_value->end(); ++it2)
                    {
                      if (it2->second.type != thinkyoung::blockchain::StorageValueTypes::storage_value_null)
                      {
                        if (it2->second.type != table_value_type)
                        {
                          global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "storage table type must be same");
                          thinkyoung::lua::lib::notify_lua_state_stop(L);
                          return 0;
                        }
                      }
                    }
                  }
                }
              }
            }
          }


          GluaStorageChangeItem change_item;
          change_item.key = name;
          change_item.contract_id = contract_id;
          change_item.after = after;
          change_item.before = before;
          // TODO: 为了避免arg2太多占用内存，合并历史，释放多余对象
          list->push_back(change_item);

          return 0;
        }

		/**
		* storage get/set should cache in lua first,
		* when lua_State finish(close successfully without error), commit all changes of all contracts' storages to thinkyoung
		* when lua_State finished/closed with error, just not commit he changes. so get/set storage api will not call thinkyoung until commit.
		* the rollback_storage api need to be able to get the initial storage of contract in this lua_State
		* arg1 is contract, arg2 is storage property name, arg3 is storage property value
		*/
		int thinkyounglib_set_storage(lua_State *L)
		{
            thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
			if (lua_gettop(L) < 3)
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "set_storage need 2 arguments");
				thinkyoung::lua::lib::notify_lua_state_stop(L);
				return 0;
			}
			if (!lua_istable(L, 1))
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "first argument of set_storage must be contract");
				return 0;
			}
			lua_getfield(L, 1, "id");
			const char *contract_id = (const char*)luaL_checkstring(L, -1);
			if (!contract_id)
				contract_id = empty_string;
			if (!lua_isstring(L, 2) && !lua_isnumber(L, 2))
			{
				global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "second argument of set_storage must be string or integer");
				thinkyoung::lua::lib::notify_lua_state_stop(L);
				L->force_stopping = true;
				return 0;
			}
			char *name = (char*)luaL_checkstring(L, 2);
            return thinkyounglib_set_storage_impl(L, contract_id, name, 3);
		}

	}
}

namespace thinkyoung {
    namespace lua {
        namespace lib {

            GluaStorageValue lthinkyoung_get_storage(lua_State *L, const char *contract_id, const char *name)
            {
                lua_pushstring(L, contract_id); // FIXME: 这里是用contract table还是contract id?
                lua_pushstring(L, name);
                int result_count = glua::lib::thinkyounglib_get_storage(L);
                if (result_count > 0)
                {
                    GluaStorageValue value = lua_type_to_storage_value_type(L, -1, 0);
                    lua_pop(L, 1);
                    return value;
                }
                else
                {
                    GluaStorageValue value;
                    value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
                    value.value.int_value = 0;
                    return value;
                }
            }

            bool lthinkyoung_set_storage(lua_State *L, const char *contract_id, const char *name, GluaStorageValue value)
            {
                lua_pushstring(L, contract_id);
                lua_pushstring(L, name);
                lua_push_storage_value(L, value);
                glua::lib::thinkyounglib_set_storage(L);
                return !global_glua_chain_api->has_exception(L);
            }

        }
    }
} // end namespace

/**
* rollback storage, and store the init storage in thinkyoung
* arg1 is contract
*/
static int rollback_storage(lua_State *L)
{
    if (lua_gettop(L) < 1)
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "rollback_storage need 1 arguments");
        thinkyoung::lua::lib::notify_lua_state_stop(L);
        return 0;
    }
    if (!lua_istable(L, 1))
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "first argument of rollback_storage must be contract");
        return 0;
    }
    lua_getfield(L, 1, "name");
    auto contract_name = (const char*)luaL_checkstring(L, -1);
    if (!contract_name)
        contract_name = empty_string;
    printf("rollback storage\n");
    return 0;
}

static const luaL_Reg thinkyounglib[] = {
    { "check", unit_test_check },
    { "check_equal", unit_test_check_equal_number },
    { "stop_vm", stop_vm },
    { "error", throw_error },
    { "set_storage", glua::lib::thinkyounglib_set_storage },
    { "get_storage", glua::lib::thinkyounglib_get_storage },
    { "register_storage", register_storage },
    { "rollback_storage", rollback_storage },
    { nullptr, nullptr }
};


/*
** Open thinkyoung library
*/
LUAMOD_API int luaopen_thinkyoung(lua_State *L) {
    luaL_newlib(L, thinkyounglib);
    lua_pushnumber(L, VERSION);
    lua_setfield(L, -2, "version");
    return 1;
}