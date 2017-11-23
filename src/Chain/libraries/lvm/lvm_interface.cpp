/*
author: gzl
date: 2017.11.20
lvm interface in glua runtime
*/
#include "lvm/lvm_interface.h"
#include "fc/io/raw.hpp"
namespace lvm {
    namespace api {
    
        LvmInterface::LvmInterface(thinkyoung::blockchain::TransactionEvaluationState* ptr = nullptr)
            :_evaluate_state(ptr) {
            _evaluate_state = ptr;
        }
        
        
        LvmInterface::~LvmInterface() {
        }
        
        void LvmInterface::get_stored_contract_info(const std::string& name) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                if (!_entry.valid()) {
                    _ret = fc::raw::pack(0);
                    return;
                }
                
                std::string addr_str = _entry->id.AddressToString(AddressType::contract_address);
                get_stored_contract_info_by_address(addr_str);
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        /*
        contract abi
        contract offline_abi
        */
        void LvmInterface::get_stored_contract_info_by_address(const std::string& address) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(thinkyoung::blockchain::Address(address, AddressType::contract_address));
                
                if (!_entry.valid()) {
                    _ret = fc::raw::pack(0);
                    return;
                }
                
                thinkyoung::blockchain::Code& code = _entry->code;
                push_result(fc::raw::pack(code.abi));
                push_result(fc::raw::pack(code.offline_abi));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::get_contract_address_by_name(const std::string& contract_name) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(contract_name);
                
                if (!_entry.valid()) {
                    return;
                }
                
                if (_entry.valid()) {
                    std::string address_str = _entry->id.AddressToString(AddressType::contract_address);
                    push_result(fc::raw::pack(address_str));
                }
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::check_contract_exist_by_address(const std::string& contract_address) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(
                             thinkyoung::blockchain::Address(contract_address, AddressType::contract_address));
                push_result(fc::raw::pack(_entry.valid()));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::check_contract_exist(const std::string& contract_name) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(contract_name);
                push_result(fc::raw::pack(_entry.valid()));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::open_contract(const std::string& contract_name) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(contract_name);
                
                if (!_entry.valid()) {
                    return;
                }
                
                if (_entry.valid() && (_entry->code.byte_code.size() <= LUA_MODULE_BYTE_STREAM_BUF_SIZE)) {
                    push_result(fc::raw::pack<Code>(_entry->code));
                }
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::open_contract_by_address(const std::string& contract_address) {
            try {
                if (_evaluate_state == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(
                             thinkyoung::blockchain::Address(contract_address, AddressType::contract_address));
                             
                if (!_entry.valid()) {
                    return;
                }
                
                if (_entry.valid() && (_entry->code.byte_code.size() <= LUA_MODULE_BYTE_STREAM_BUF_SIZE)) {
                    push_result(fc::raw::pack<Code>(_entry->code));
                }
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::get_storage_value_from_thinkyoung(const std::string& contract_name, const std::string& storage_name) {
            try {
                GluaStorageValue null_storage;
                
                if (_evaluate_state == nullptr) {
                    push_result(fc::raw::pack<GluaStorageValue>(null_storage));
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    return;
                }
                
                _entry = _evaluate_state->_current_state->get_contract_entry(contract_name);
                
                if (!_entry.valid()) {
                    push_result(fc::raw::pack<GluaStorageValue>(null_storage));
                    return;
                }
                
                get_storage_value_from_thinkyoung_by_address(_entry->id.AddressToString(AddressType::contract_address), storage_name);
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::get_storage_value_from_thinkyoung_by_address(const std::string& contract_address, const std::string& storage_name) {
            try {
                GluaStorageValue null_storage;
                
                if (_evaluate_state == nullptr) {
                    push_result(fc::raw::pack<GluaStorageValue>(null_storage));
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    return;
                }
                
                thinkyoung::blockchain::oContractStorage entry = _evaluate_state->_current_state->get_contractstorage_entry(
                            thinkyoung::blockchain::Address(contract_address, AddressType::contract_address));
                            
                if (!entry.valid()) {
                    push_result(fc::raw::pack<GluaStorageValue>(null_storage));
                    return;
                }
                
                auto iter = entry->contract_storages.find(std::string(storage_name));
                
                if (iter == entry->contract_storages.end()) {
                    push_result(fc::raw::pack<GluaStorageValue>(null_storage));
                    return;
                }
                
                thinkyoung::blockchain::StorageDataType storage_data = iter->second;
                push_result(fc::raw::pack<thinkyoung::blockchain::StorageDataType>(storage_data));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::get_contract_balance_amount(const std::string& contract_address, const std::string& asset_symbol) {
            try {
                if (!_evaluate_state || (_chain_interface = _evaluate_state->_current_state) == nullptr) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                const auto asset_rec = _chain_interface->get_asset_entry(asset_symbol);
                
                if (!asset_rec.valid() || asset_rec->id != 0) {
                    FC_CAPTURE_AND_THROW(unknown_asset_id, ("Only ACT Allowed"));
                }
                
                BalanceIdType contract_balance_id = _chain_interface->get_balanceid(thinkyoung::blockchain::Address(contract_address, AddressType::contract_address), WithdrawBalanceTypes::withdraw_contract_type);
                oBalanceEntry balance_entry = _chain_interface->get_balance_entry(contract_balance_id);
                
                //if (!balance_entry.valid())
                //    FC_CAPTURE_AND_THROW(unknown_balance_entry, ("Get balance entry failed"));
                
                if (!balance_entry.valid())
                    return ;
                    
                oAssetEntry asset_entry = _chain_interface->get_asset_entry(balance_entry->asset_id());
                
                if (!asset_entry.valid() || asset_entry->id != 0)
                    FC_CAPTURE_AND_THROW(unknown_asset_id, ("asset error"));
                    
                Asset asset = balance_entry->get_spendable_balance(_chain_interface->now());
                push_result(fc::raw::pack(asset.amount));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::get_transaction_fee() {
            try {
                ChainInterface*  db_interface = NULL;
                
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                Asset  fee = _evaluate_state->_current_state->get_transaction_fee();
                oAssetEntry ass_res = db_interface->get_asset_entry(fee.asset_id);
                
                if (!ass_res.valid() || ass_res->precision == 0) {
                    push_result(fc::raw::pack(-1));
                    return;
                }
                
                push_result(fc::raw::pack(fee.amount));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::get_chain_now() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                fc::time_point_sec time_stamp = _chain_interface->get_head_block_timestamp();
                push_result(fc::raw::pack(time_stamp.sec_since_epoch()));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::get_chain_random() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                push_result(fc::raw::pack(_evaluate_state->p_result_trx.id().hash(_chain_interface->get_current_random_seed())._hash[2]));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::get_transaction_id() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                push_result(fc::raw::pack<std::string>(_evaluate_state->trx.id().str()));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::get_header_block_num() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                push_result(fc::raw::pack(_chain_interface->get_head_block_num()));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::wait_for_future_random(const int next) {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                int target = _chain_interface->get_head_block_num() + next;
                
                if (target < next) {
                    push_result(fc::raw::pack(0));
                    
                } else {
                    push_result(fc::raw::pack(target));
                }
                
            } catch (fc::exception& e) {
                _err_num = e.code();
                return ;
            }
        }
        void LvmInterface::get_waited(uint32_t num) {
            try {
                if (num <= 1) {
                    push_result(fc::raw::pack(-2));
                    return;
                }
                
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                if (_chain_interface->get_head_block_num() < num) {
                    push_result(fc::raw::pack(-1));
                    return ;
                }
                
                BlockIdType id = _chain_interface->get_block_id(num);
                BlockHeader _header = _chain_interface->get_block_header(id);
                SecretHashType _hash = _header.previous_secret;
                auto default_id = BlockIdType();
                
                for (int i = 0; i < 50; i++) {
                    if ((id = _header.previous) == default_id)
                        break;
                        
                    _header = _chain_interface->get_block_header(id);
                    _hash = _hash.hash(_header.previous_secret);
                }
                
                push_result(fc::raw::pack(_hash._hash[3] % (1 << 31 - 1)));
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::commit_storage_changes_to_thinkyoung(AllContractsChangesMapRPC& change_items) {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                for (auto all_con_chg_iter = change_items.begin(); all_con_chg_iter != change_items.end(); ++all_con_chg_iter) {
                    StorageOperation storage_op;
                    std::string contract_id = all_con_chg_iter->first;
                    ContractChangesMap contract_change = all_con_chg_iter->second;
                    storage_op.contract_id = Address(contract_id, AddressType::contract_address);
                    
                    for (auto con_chg_iter = contract_change.begin(); con_chg_iter != contract_change.end(); ++con_chg_iter) {
                        std::string contract_name = con_chg_iter->first;
                        StorageDataChangeType storage_change;
                        storage_change.storage_before = StorageDataType::get_storage_data_from_lua_storage(con_chg_iter->second.before);
                        storage_change.storage_after = StorageDataType::get_storage_data_from_lua_storage(con_chg_iter->second.after);
                        storage_op.contract_change_storages.insert(make_pair(contract_name, storage_change));
                    }
                    
                    _evaluate_state->p_result_trx.push_storage_operation(storage_op);
                }
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        
        void LvmInterface::transfer_from_contract_to_address(const std::string& contract_address, const std::string& to_address,
                const std::string& asset_type, int64_t amount) {
            try {
                if (amount <= 0) {
                    push_result(fc::raw::pack(-6));
                    return;
                }
                
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                std::string to_addr;
                std::string to_sub;
                thinkyoung::wallet::Wallet::accountsplit(to_address, to_addr, to_sub);
                
                if (!Address::is_valid(contract_address, CONTRACT_ADDRESS_PREFIX)) {
                    push_result(fc::raw::pack(-3));
                }
                
                if (!Address::is_valid(to_addr, ALP_ADDRESS_PREFIX)) {
                    push_result(fc::raw::pack(-4));
                }
                
                _evaluate_state->transfer_asset_from_contract(amount, asset_type, Address(contract_address,
                        AddressType::contract_address), Address(to_addr, AddressType::alp_address));
                _evaluate_state->_contract_balance_remain -= amount;
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::transfer_from_contract_to_public_account(const std::string& contract_address, const std::string& to_account_name,
                const std::string& asset_type, int64_t amount) {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                if (!_evaluate_state->_current_state->is_valid_account_name(to_account_name)) {
                    push_result(fc::raw::pack(-7));
                }
                
                auto acc_entry = _evaluate_state->_current_state->get_account_entry(to_account_name);
                
                if (!acc_entry.valid()) {
                    push_result(fc::raw::pack(-7));
                }
                
                transfer_from_contract_to_address(contract_address, acc_entry->owner_address().AddressToString(), asset_type, amount);
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
            }
        }
        void LvmInterface::emit(const std::string& contract_id, const std::string& event_name, const std::string& event_param) {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }
                
                EventOperation event_op(Address(contract_id, AddressType::contract_address), std::string(event_name), std::string(event_param));
                _evaluate_state->p_result_trx.push_event_operation(event_op);
                
            } catch (const fc::exception& e) {
                _err_num = e.code();
                return;
            }
        }
        void LvmInterface::push_result(std::vector<char>& value) {
            _result.push_back(value);
        }
        
        void LvmInterface::set_ret(std::vector<char>& value) {
            _ret = value;
        }
        
    }
}

