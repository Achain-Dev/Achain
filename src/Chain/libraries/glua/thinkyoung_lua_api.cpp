/**
 * lua module injector header in thinkyoung
 */

#include "glua/lprefix.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <sstream>
#include <utility>
#include <list>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "glua/thinkyoung_lua_api.h"
#include "glua/thinkyoung_lua_lib.h"
#include "glua/glua_lutil.h"
#include "glua/lstate.h"
#include "glua/lobject.h"

#include <blockchain/GluaChainApi.hpp>
#include <blockchain/Address.hpp>
#include <blockchain/ChainInterface.hpp>
#include <blockchain/StorageOperations.hpp>
#include <blockchain/TransactionEvaluationState.hpp>
#include <blockchain/Exceptions.hpp>
#include "wallet/Wallet.hpp"
#include "blockchain/BalanceOperations.hpp"
#include "blockchain/ContractOperations.hpp"
#include "blockchain/Types.hpp"


namespace thinkyoung {
    namespace lua {
        namespace api {

            // TODO: all these apis need TODO

            static int has_error = 0;


            static std::string get_file_name_str_from_contract_module_name(std::string name)
            {
                std::stringstream ss;
                ss << "thinkyoung_contract_" << name;
                return ss.str();
            }

            /**
            * whether exception happen in L
            */
            bool GluaChainApi::has_exception(lua_State *L)
            {
                return has_error ? true : false;
            }

            /**
            * clear exception marked
            */
            void GluaChainApi::clear_exceptions(lua_State *L)
            {
                has_error = 0;
            }

            /**
            * when exception happened, use this api to tell thinkyoung
            * @param L the lua stack
            * @param code error code, 0 is OK, other is different error
            * @param error_format error info string, will be released by lua
            * @param ... error arguments
            */
            void GluaChainApi::throw_exception(lua_State *L, int code, const char *error_format, ...)
            {
                has_error = 1;
                char *msg = (char*)lua_malloc(L, LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH);
                memset(msg, 0x0, LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH);

                va_list vap;
                va_start(vap, error_format);
                // printf(error_format, vap);
                // const char *msg = luaO_pushfstring(L, error_format, vap);
                vsnprintf(msg, LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH, error_format, vap);
                va_end(vap);
                if (strlen(msg) > LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH - 1)
                {
                    msg[LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH - 1] = 0;
                }
                //perror(msg);
                //printf("\n");
                // luaL_error(L, error_format); // notify lua error
                //FC_THROW(msg);

                lua_set_compile_error(L, msg);

                //如果上次的exception code为THINKYOUNG_API_LVM_LIMIT_OVER_ERROR, 不能被其他异常覆盖
                //只有调用clear清理后，才能继续记录异常
                int last_code = lua::lib::get_lua_state_value(L, "exception_code").int_value;
                if (last_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR
                    && code != THINKYOUNG_API_LVM_LIMIT_OVER_ERROR)
                {
                    return;
                }

                GluaStateValue val_code;
                val_code.int_value = code;

                GluaStateValue val_msg;
                val_msg.string_value = msg;

                lua::lib::set_lua_state_value(L, "exception_code", val_code, GluaStateValueType::LUA_STATE_VALUE_INT);
                lua::lib::set_lua_state_value(L, "exception_msg", val_msg, GluaStateValueType::LUA_STATE_VALUE_STRING);
            }

            /**
            * check whether the contract apis limit over, in this lua_State
            * @param L the lua stack
            * @return TRUE(1 or not 0) if over limit(will break the vm), FALSE(0) if not over limit
            */
            int GluaChainApi::check_contract_api_instructions_over_limit(lua_State *L)
            {
                return 0; // FIXME: need fill by thinkyoung api
            }

            int GluaChainApi::get_stored_contract_info(lua_State *L, const char *name, std::shared_ptr<GluaContractInfo> contract_info_ret)
            {
                blockchain::TransactionEvaluationState* pevaluate_state = (blockchain::TransactionEvaluationState*)lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value;
                // name = lua::lib::unwrap_any_contract_name(name).c_str();
                blockchain::oContractEntry entry = pevaluate_state->_current_state->get_contract_entry(name);
                if (!entry.valid())
                    return 0;

                std::string addr_str = entry->id.AddressToString(AddressType::contract_address);

                return get_stored_contract_info_by_address(L, addr_str.c_str(), contract_info_ret);
            }

            int GluaChainApi::get_stored_contract_info_by_address(lua_State *L, const char *address, std::shared_ptr<GluaContractInfo> contract_info_ret)
            {
                blockchain::TransactionEvaluationState* pevaluate_state = (blockchain::TransactionEvaluationState*)lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value;
                blockchain::oContractEntry entry = pevaluate_state->_current_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(address), AddressType::contract_address));

                if (!entry.valid())
                    return 0;

                blockchain::Code& code = entry->code;
                contract_info_ret->contract_apis.clear();

                std::copy(code.abi.begin(), code.abi.end(), std::back_inserter(contract_info_ret->contract_apis));
                std::copy(code.offline_abi.begin(), code.offline_abi.end(), std::back_inserter(contract_info_ret->contract_apis));

                return 1;
            }

            void GluaChainApi::get_contract_address_by_name(lua_State *L, const char *name, char *address, size_t *address_size)
            {
                /*
                std::string addr_str = std::string("id_") + std::string(name);
                strcpy(address, addr_str.c_str());
                *address_size = addr_str.size() + 1;
                */
                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                {
                    return;
                }

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractEntry entry = cur_state->get_contract_entry(std::string(name));

                if (entry.valid())
                {
                    string address_str = entry->id.AddressToString(AddressType::contract_address);
                    *address_size = address_str.length();
                    strncpy(address, address_str.c_str(), CONTRACT_ID_MAX_LENGTH - 1);
                    address[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
                }
            }

            bool GluaChainApi::check_contract_exist_by_address(lua_State *L, const char *address)
            {
                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return NULL;

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractEntry entry = cur_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(address), AddressType::contract_address));

                return entry.valid();
            }

            bool GluaChainApi::check_contract_exist(lua_State *L, const char *name)
            {
                /*
                char *filename = lutil_concat_str4("thinkyoung_lua_modules", file_separator_str(), "thinkyoung_contract_", name);
                FILE *f = fopen(filename, "rb");
                bool exist = false;
                if (NULL != f)
                {
                exist = true;
                fclose(f);
                }
                free(filename);
                return exist;
                */

                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return NULL;

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractEntry entry = cur_state->get_contract_entry(std::string(name));

                return entry.valid();
            }

            std::shared_ptr<GluaModuleByteStream> GluaChainApi::get_bytestream_from_code(lua_State *L, const Code& code)
            {
                if (code.byte_code.size() > LUA_MODULE_BYTE_STREAM_BUF_SIZE)
                    return NULL;
                auto p_luamodule = std::make_shared<GluaModuleByteStream>();
                p_luamodule->is_bytes = true;
                p_luamodule->buff.resize(code.byte_code.size());
                memcpy(p_luamodule->buff.data(), code.byte_code.data(), code.byte_code.size());
                p_luamodule->contract_name = "";

                p_luamodule->contract_apis.clear();
                std::copy(code.abi.begin(), code.abi.end(), std::back_inserter(p_luamodule->contract_apis));

                p_luamodule->contract_emit_events.clear();
                std::copy(code.offline_abi.begin(), code.offline_abi.end(), std::back_inserter(p_luamodule->offline_apis));

                p_luamodule->contract_emit_events.clear();
                std::copy(code.events.begin(), code.events.end(), std::back_inserter(p_luamodule->contract_emit_events));

                p_luamodule->contract_storage_properties.clear();
                std::copy(code.storage_properties.begin(), code.storage_properties.end(), std::inserter(p_luamodule->contract_storage_properties, p_luamodule->contract_storage_properties.begin()));

                return p_luamodule;
            }
            /**
            * load contract lua byte stream from thinkyoung api
            */
            std::shared_ptr<GluaModuleByteStream> GluaChainApi::open_contract(lua_State *L, const char *name)
            {
                // FXIME
                /*
                bool is_bytes = true;
                char *filename = lutil_concat_str4("thinkyoung_lua_modules", file_separator_str(), "thinkyoung_contract_", name);
                FILE *f = fopen(filename, "rb");
                if (NULL == f)
                {
                filename = lutil_concat_str(filename, ".lua");
                f = fopen(filename, "rb");
                if (NULL == f)
                {
                return NULL;
                }
                is_bytes = false;
                }
                GluaModuleByteStream *stream = (GluaModuleByteStream*)malloc(sizeof(GluaModuleByteStream));
                stream->len = fread(stream->buff, 1024 * 1024, 1, f);
                fseek(f, 0, SEEK_END); // seek to end of file
                stream->len = ftell(f); // get current file pointer
                stream->is_bytes = is_bytes;
                fclose(f);
                if (!is_bytes)
                stream->buff[stream->len] = '\0';
                free(filename);
                return stream;
                */
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);

                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return NULL;

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractEntry entry = cur_state->get_contract_entry(std::string(name));
                if (entry.valid() && (entry->code.byte_code.size() <= LUA_MODULE_BYTE_STREAM_BUF_SIZE))
                {
                    return get_bytestream_from_code(L, entry->code);
                }

                return NULL;
            }

            std::shared_ptr<GluaModuleByteStream> GluaChainApi::open_contract_by_address(lua_State *L, const char *address)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return NULL;

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractEntry entry = cur_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(address), AddressType::contract_address));
                if (entry.valid() && (entry->code.byte_code.size() <= LUA_MODULE_BYTE_STREAM_BUF_SIZE))
                {
                    return get_bytestream_from_code(L, entry->code);
                }

                return NULL;
            }

            /**
            * store contract lua module byte stream to thinkyoung api
            */
            //need to delete
            /*
            int save_contract(const char *name, GluaModuleByteStream *stream)
            {
            return 0;
            }
            */

            /*
            void free_contract_storage(lua_State *L, GluaStorageValue* storage)
            {
            if (storage)
            {
            if (storage->type == GluaStorageValueType::LVALUE_INTEGER ||
            storage->type == GluaStorageValueType::LVALUE_NUMBER ||
            storage->type == GluaStorageValueType::LVALUE_BOOLEAN)
            {
            ;
            }
            else if (storage->type == GluaStorageValueType::LVALUE_STRING)
            {
            if (storage->value.string_value)
            free(storage->value.string_value);
            }
            else if (storage->type == GluaStorageValueType::LVALUE_TABLE)
            {
            GluaTableMap* p_lua_table = storage->value.table_value;

            if (p_lua_table && !p_lua_table->empty())
            {
            GluaStorageValue first_base_storage = p_lua_table->begin()->second;
            if (first_base_storage.type == GluaStorageValueType::LVALUE_INTEGER ||
            first_base_storage.type == GluaStorageValueType::LVALUE_NUMBER ||
            first_base_storage.type == GluaStorageValueType::LVALUE_BOOLEAN)
            {
            ;
            }
            else if (first_base_storage.type == GluaStorageValueType::LVALUE_STRING)
            {
            for (const auto& item: *p_lua_table)
            {
            if (item.second.value.string_value)
            free(item.second.value.string_value);

            }
            }
            delete p_lua_table;
            }

            }
            free(storage);
            }
            }
            */

            GluaStorageValue GluaChainApi::get_storage_value_from_thinkyoung(lua_State *L, const char *contract_name, std::string name)
            {
                GluaStorageValue null_storage;
                null_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;

                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return null_storage;

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractEntry entry = cur_state->get_contract_entry(std::string(contract_name));
                if (!entry.valid())
                {
                    return null_storage;
                }
                return get_storage_value_from_thinkyoung_by_address(L, entry->id.AddressToString(AddressType::contract_address).c_str(), name);
            }

            GluaStorageValue GluaChainApi::get_storage_value_from_thinkyoung_by_address(lua_State *L, const char *contract_address, std::string name)
            {
                GluaStorageValue null_storage;
                null_storage.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;

                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return null_storage;

                thinkyoung::blockchain::ChainInterface* cur_state = eval_state_ptr->_current_state;

                oContractStorage entry = cur_state->get_contractstorage_entry(Address(std::string(contract_address), AddressType::contract_address));
                if (NOT entry.valid())
                    return null_storage;

                auto iter = entry->contract_storages.find(std::string(name));
                if (iter == entry->contract_storages.end())
                    return null_storage;

                thinkyoung::blockchain::StorageDataType storage_data = iter->second;

                return thinkyoung::blockchain::StorageDataType::create_lua_storage_from_storage_data(L, storage_data);
            }

            bool GluaChainApi::commit_storage_changes_to_thinkyoung(lua_State *L, AllContractsChangesMap &changes)
            {
                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                    return false;

                for (auto all_con_chg_iter = changes.begin(); all_con_chg_iter != changes.end(); ++all_con_chg_iter)
                {
                    StorageOperation storage_op;
                    std::string contract_id = all_con_chg_iter->first;
                    ContractChangesMap contract_change = *(all_con_chg_iter->second);

                    storage_op.contract_id = Address(contract_id, AddressType::contract_address);

                    for (auto con_chg_iter = contract_change.begin(); con_chg_iter != contract_change.end(); ++con_chg_iter)
                    {
                        std::string contract_name = con_chg_iter->first;

                        StorageDataChangeType storage_change;
                        storage_change.storage_before = StorageDataType::get_storage_data_from_lua_storage(con_chg_iter->second.before);
                        storage_change.storage_after = StorageDataType::get_storage_data_from_lua_storage(con_chg_iter->second.after);

                        storage_op.contract_change_storages.insert(make_pair(contract_name, storage_change));
                    }

                    eval_state_ptr->p_result_trx.push_storage_operation(storage_op);
                }

                return true;
            }

            //not use
            bool GluaChainApi::register_storage(lua_State *L, const char *contract_name, const char *name)
            {
                // TODO
                printf("registered storage %s[%s] to thinkyoung\n", contract_name, name);
                return true;
            }

            intptr_t GluaChainApi::register_object_in_pool(lua_State *L, intptr_t object_addr, GluaOutsideObjectTypes type)
			{
				auto node = thinkyoung::lua::lib::get_lua_state_value_node(L, GLUA_OUTSIDE_OBJECT_POOLS_KEY);
				// Map<type, Map<object_key, object_addr>>
				std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>> *object_pools = nullptr;
				if (node.type == GluaStateValueType::LUA_STATE_VALUE_nullptr)
				{
					node.type = GluaStateValueType::LUA_STATE_VALUE_POINTER;
					object_pools = new std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>>();
					node.value.pointer_value = (void*)object_pools;
					thinkyoung::lua::lib::set_lua_state_value(L, GLUA_OUTSIDE_OBJECT_POOLS_KEY, node.value, node.type);
				}
				else
				{
					object_pools = (std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>> *) node.value.pointer_value;
				}
				if (object_pools->find(type) == object_pools->end())
				{
					object_pools->emplace(std::make_pair(type, std::make_shared<std::map<intptr_t, intptr_t>>()));
				}
				auto pool = (*object_pools)[type];
				auto object_key = object_addr;
				(*pool)[object_key] = object_addr;
				return object_key;
			}

            intptr_t GluaChainApi::is_object_in_pool(lua_State *L, intptr_t object_key, GluaOutsideObjectTypes type)
			{
				auto node = thinkyoung::lua::lib::get_lua_state_value_node(L, GLUA_OUTSIDE_OBJECT_POOLS_KEY);
				// Map<type, Map<object_key, object_addr>>
				std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>> *object_pools = nullptr;
				if (node.type == GluaStateValueType::LUA_STATE_VALUE_nullptr)
				{
					return 0;
				}
				else
				{
					object_pools = (std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>> *) node.value.pointer_value;
				}
				if (object_pools->find(type) == object_pools->end())
				{
					object_pools->emplace(std::make_pair(type, std::make_shared<std::map<intptr_t, intptr_t>>()));
				}
				auto pool = (*object_pools)[type];
				return (*pool)[object_key];
			}

            void GluaChainApi::release_objects_in_pool(lua_State *L)
			{
				auto node = thinkyoung::lua::lib::get_lua_state_value_node(L, GLUA_OUTSIDE_OBJECT_POOLS_KEY);
				// Map<type, Map<object_key, object_addr>>
				std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>> *object_pools = nullptr;
				if (node.type == GluaStateValueType::LUA_STATE_VALUE_nullptr)
				{
					return;
				}
				object_pools = (std::map<GluaOutsideObjectTypes, std::shared_ptr<std::map<intptr_t, intptr_t>>> *) node.value.pointer_value;
				// TODO: 对于object_pools中不同类型的对象，分别释放
				for (const auto &p : *object_pools)
				{
					auto type = p.first;
					auto pool = p.second;
					for (const auto &object_item : *pool)
					{
						auto object_key = object_item.first;
						auto object_addr = object_item.second;
						if (object_addr == 0)
							continue;
						switch (type)
						{
						case GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE:
						{
							auto stream = (thinkyoung::lua::lib::GluaByteStream*) object_addr;
							delete stream;
						} break;
						default: {
							continue;
						}
						}
					}
				}
				delete object_pools;
				GluaStateValue null_state_value;
				null_state_value.int_value = 0;
				thinkyoung::lua::lib::set_lua_state_value(L, GLUA_OUTSIDE_OBJECT_POOLS_KEY, null_state_value, GluaStateValueType::LUA_STATE_VALUE_nullptr);
			}

            lua_Integer GluaChainApi::transfer_from_contract_to_address(lua_State *L, const char *contract_address, const char *to_address,
                const char *asset_type, int64_t amount)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                //printf("contract transfer from %s to %s, asset[%s] amount %ld\n", contract_address, to_address, asset_type, amount_str);
                //return true;

                if (amount <= 0)
                    return -6;
                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return -1;
                }


                string to_addr;
                string to_sub;
                wallet::Wallet::accountsplit(to_address, to_addr, to_sub);

                try
                {
                    if (!Address::is_valid(contract_address, CONTRACT_ADDRESS_PREFIX))
                        return -3;
                    if (!Address::is_valid(to_addr, ALP_ADDRESS_PREFIX))
                        return -4;

                    eval_state_ptr->transfer_asset_from_contract(amount, asset_type,
                        Address(contract_address, AddressType::contract_address), Address(to_addr, AddressType::alp_address));

					eval_state_ptr->_contract_balance_remain -= amount;

                }
                catch (const fc::exception& err)
                {
                    switch (err.code())
                    {
                    case 31302:
                        return -2;
                    case 31003: //unknown balance entry
                        return -5;
                    case 31004:
                        return -5;
                    default:
                        L->force_stopping = true;
                        L->exit_code = LUA_API_INTERNAL_ERROR;
						return -1;
                    }
                }
                return 0;

            }

            lua_Integer GluaChainApi::transfer_from_contract_to_public_account(lua_State *L, const char *contract_address, const char *to_account_name,
                const char *asset_type, int64_t amount)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                    (thinkyoung::blockchain::TransactionEvaluationState*)
                    (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                if (!eval_state_ptr || !eval_state_ptr->_current_state)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return -1;
                }
                if (!eval_state_ptr->_current_state->is_valid_account_name(to_account_name))
                    return -7;
                auto acc_entry = eval_state_ptr->_current_state->get_account_entry(to_account_name);
                if (!acc_entry.valid())
                    return -7;
                return transfer_from_contract_to_address(L, contract_address, acc_entry->owner_address().AddressToString().c_str(), asset_type, amount);
            }

            int64_t GluaChainApi::get_contract_balance_amount(lua_State *L, const char *contract_address, const char* asset_symbol)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);
                    thinkyoung::blockchain::ChainInterface* cur_state;
                    if (!eval_state_ptr || (cur_state = eval_state_ptr->_current_state) == NULL)
                    {
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    }

                    const auto asset_rec = cur_state->get_asset_entry(asset_symbol);
                    if (!asset_rec.valid() || asset_rec->id != 0)
                    {
                        FC_CAPTURE_AND_THROW(unknown_asset_id, ("Only ACT Allowed"));
                    }

                    BalanceIdType contract_balance_id = cur_state->get_balanceid(Address(contract_address, AddressType::contract_address), WithdrawBalanceTypes::withdraw_contract_type);
                    oBalanceEntry balance_entry = cur_state->get_balance_entry(contract_balance_id);

                    //if (!balance_entry.valid())
                    //    FC_CAPTURE_AND_THROW(unknown_balance_entry, ("Get balance entry failed"));

                    if (!balance_entry.valid())
                        return 0;

                    oAssetEntry asset_entry = cur_state->get_asset_entry(balance_entry->asset_id());
                    if (!asset_entry.valid() || asset_entry->id != 0)
                        FC_CAPTURE_AND_THROW(unknown_asset_id, ("asset error"));

                    Asset asset = balance_entry->get_spendable_balance(cur_state->now());

                    return asset.amount;
                }
                catch (const fc::exception& e)
                {
                    switch (e.code())
                    {
                    case 30028://invalid_address
                        return -2;
                    //case 31003://unknown_balance_entry
                    //    return -3;
                    case 31303:
                        return -1;
                    default:
                        L->force_stopping = true;
                        L->exit_code = LUA_API_INTERNAL_ERROR;
                        return -4;
                        break;
                    }
                }
            }

            int64_t GluaChainApi::get_transaction_fee(lua_State *L)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);
                    ChainInterface*  db_interface = NULL;
                    if (!eval_state_ptr || !(db_interface = eval_state_ptr->_current_state))
                    {
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    }

                    Asset  fee = eval_state_ptr->_current_state->get_transaction_fee();
                    oAssetEntry ass_res = db_interface->get_asset_entry(fee.asset_id);
                    if (!ass_res.valid() || ass_res->precision == 0)
                        return -1;
                    return fee.amount;

                }
                catch (fc::exception e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return -2;
                }
            }

            uint32_t GluaChainApi::get_chain_now(lua_State *L)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);
                    thinkyoung::blockchain::ChainInterface* cur_state;
                    if (!eval_state_ptr || !(cur_state = eval_state_ptr->_current_state))
                    {
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    }
                    fc::time_point_sec time_stamp = cur_state->get_head_block_timestamp();
                    return time_stamp.sec_since_epoch();
                }
                catch (fc::exception e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return 0;
                }
            }
            uint32_t GluaChainApi::get_chain_random(lua_State *L)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);
                    thinkyoung::blockchain::ChainInterface* cur_state;
                    if (!eval_state_ptr || !(cur_state = eval_state_ptr->_current_state))
                    {
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    }

                    return eval_state_ptr->p_result_trx.id().hash(cur_state->get_current_random_seed())._hash[2];

                }
                catch (fc::exception e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return 0;
                }
            }

            std::string GluaChainApi::get_transaction_id(lua_State *L)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                    if (!eval_state_ptr)
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    return eval_state_ptr->trx.id().str();
                }
                catch (fc::exception e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return "";
                }
            }


            uint32_t GluaChainApi::get_header_block_num(lua_State *L)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                    if (!eval_state_ptr || !eval_state_ptr->_current_state)
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    return eval_state_ptr->_current_state->get_head_block_num();
                }
                catch (fc::exception e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return 0;
                }
            }

            uint32_t GluaChainApi::wait_for_future_random(lua_State *L, int next)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                    if (!eval_state_ptr || !eval_state_ptr->_current_state)
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));

                    uint32_t target = eval_state_ptr->_current_state->get_head_block_num() + next;
                    if (target < next)
                        return 0;
                    return target;
                }
                catch (fc::exception e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return 0;
                }
            }
            //获取指定块与之前50块的pre_secret hash出的结果，该值在指定块被产出的上一轮出块时就已经确定，而无人可知，无法操控
            //如果希望使用该值作为随机值，以随机值作为其他数据的选取依据时，需要在目标块被产出前确定要被筛选的数据
            //如投注彩票，只允许在目标块被产出前投注
            int32_t GluaChainApi::get_waited(lua_State *L, uint32_t num)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try{
                    if (num <= 1)
                        return -2;
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);
                    thinkyoung::blockchain::ChainInterface* cur_state;
                    if (!eval_state_ptr || !(cur_state = eval_state_ptr->_current_state))
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    if (cur_state->get_head_block_num() < num)
                        return -1;
                    BlockIdType id = cur_state->get_block_id(num);
                    BlockHeader _header = cur_state->get_block_header(id);
                    SecretHashType _hash = _header.previous_secret;
                    auto default_id = BlockIdType();
                    for (int i = 0; i < 50; i++)
                    {
                        if ((id = _header.previous) == default_id)
                            break;
                        _header = cur_state->get_block_header(id);
                        _hash = _hash.hash(_header.previous_secret);
                    }
                    return _hash._hash[3] % (1 << 31 - 1);
                }
                catch (const fc::exception& e)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return -1;
                }
                //string get_wait
            }



            void GluaChainApi::emit(lua_State *L, const char* contract_id, const char* event_name, const char* event_param)
            {
              thinkyoung::lua::lib::increment_lvm_instructions_executed_count(L, CHAIN_GLUA_API_EACH_INSTRUCTIONS_COUNT - 1);
                try {
                    thinkyoung::blockchain::TransactionEvaluationState* eval_state_ptr =
                        (thinkyoung::blockchain::TransactionEvaluationState*)
                        (thinkyoung::lua::lib::get_lua_state_value(L, "evaluate_state").pointer_value);

                    if (eval_state_ptr == NULL)
                        FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));

                    EventOperation event_op(Address(contract_id, AddressType::contract_address), std::string(event_name), std::string(event_param));
                    eval_state_ptr->p_result_trx.push_event_operation(event_op);
                }
                catch (const fc::exception&)
                {
                    L->force_stopping = true;
                    L->exit_code = LUA_API_INTERNAL_ERROR;
                    return;
                }
            }

        }
    }
}