/*
author: gzl
date: 2017.11.20
lvm interface in glua runtime
*/
#include "lvm/lvm_interface.h"
namespace lvm {
    namespace api {

        LvmInterface::LvmInterface(thinkyoung::blockchain::TransactionEvaluationState* ptr = nullptr)
            :_evaluate_state(ptr), err_num(0) {
        }


        LvmInterface::~LvmInterface() {
        }

        /*
        contract abi
        contract offline_abi
        */
        void LvmInterface::get_stored_contract_info_by_address(const std::string& address) {
            try {
                if ((nullptr == _evaluate_state->_current_state) || (_evaluate_state == nullptr)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                _entry = _evaluate_state->_current_state->get_contract_entry(thinkyoung::blockchain::Address(address, AddressType::contract_address));

                if (!_entry.valid()) {
                    push_result(fc::raw::pack(false));
                    return;
                }

                thinkyoung::blockchain::Code& code = _entry->code;
                push_result(fc::raw::pack(true));
                push_result(fc::raw::pack(code.abi));
                push_result(fc::raw::pack(code.offline_abi));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }

        void LvmInterface::get_contract_address_by_name(const std::string& contract_name) {
            try {
                if ((_evaluate_state == nullptr) || (nullptr == _evaluate_state->_current_state)) {
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
                err_num = e.code();
            }
        }

        void LvmInterface::check_contract_exist_by_address(const std::string& contract_address) {
            try {
                if ((nullptr == _evaluate_state->_current_state) || (_evaluate_state == nullptr)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                _entry = _evaluate_state->_current_state->get_contract_entry(
                             thinkyoung::blockchain::Address(contract_address, AddressType::contract_address));
                push_result(fc::raw::pack(_entry.valid()));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }

        void LvmInterface::check_contract_exist(const std::string& contract_name) {
            try {
                if ((nullptr == _evaluate_state->_current_state) || (_evaluate_state == nullptr)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                _entry = _evaluate_state->_current_state->get_contract_entry(contract_name);
                push_result(fc::raw::pack(_entry.valid()));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }

        void LvmInterface::open_contract(const std::string& contract_name) {
            try {
                if ((nullptr == _evaluate_state->_current_state) || (_evaluate_state == nullptr)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                _entry = _evaluate_state->_current_state->get_contract_entry(contract_name);

                if (!_entry.valid()) {
                    push_result(fc::raw::pack(false));
                    return;
                }

                if (_entry.valid() && (_entry->code.byte_code.size() <= LUA_MODULE_BYTE_STREAM_BUF_SIZE)) {
                    push_result(fc::raw::pack(true));
                    push_result(fc::raw::pack<Code>(_entry->code));
                    return;
                }

                push_result(fc::raw::pack(false));

            } catch (const fc::exception& e) {
                err_num = e.code();
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
                    push_result(fc::raw::pack(false));
                    return;
                }

                if (_entry.valid() && (_entry->code.byte_code.size() <= LUA_MODULE_BYTE_STREAM_BUF_SIZE)) {
                    push_result(fc::raw::pack(true));
                    push_result(fc::raw::pack<Code>(_entry->code));
                    return;

                } else {
                    push_result(fc::raw::pack(false));
                }

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }

        void LvmInterface::get_storage_value_from_thinkyoung_by_address(const std::string& contract_address, const std::string& storage_name) {
            try {
                StorageDataType null_storage;

                if ((nullptr == _evaluate_state->_current_state) || (_evaluate_state == nullptr)) {
                    push_result(fc::raw::pack<StorageDataType>(null_storage));
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                    return;
                }
                
                thinkyoung::blockchain::ChainInterface* cur_state = _evaluate_state->_current_state;
                thinkyoung::blockchain::oContractStorage entry = cur_state->get_contractstorage_entry(
                            thinkyoung::blockchain::Address(contract_address, AddressType::contract_address));

                if (!entry.valid()) {
                    push_result(fc::raw::pack<StorageDataType>(null_storage));
                    return;
                }

                auto iter = entry->contract_storages.find(std::string(storage_name));

                if (iter == entry->contract_storages.end()) {
                    push_result(fc::raw::pack<StorageDataType>(null_storage));
                    return;
                }

                thinkyoung::blockchain::StorageDataType storage_data = iter->second;
                push_result(fc::raw::pack<thinkyoung::blockchain::StorageDataType>(storage_data));

            } catch (const fc::exception& e) {
                err_num = e.code();
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

                if (!balance_entry.valid()) {
                    push_result(fc::raw::pack(0));
                    return ;
                }

                oAssetEntry asset_entry = _chain_interface->get_asset_entry(balance_entry->asset_id());

                if (!asset_entry.valid() || asset_entry->id != 0) {
                    FC_CAPTURE_AND_THROW(unknown_asset_id, ("asset error"));
                }

                Asset asset = balance_entry->get_spendable_balance(_chain_interface->now());
                push_result(fc::raw::pack(asset.amount));

            } catch (const fc::exception& e) {
                err_num = e.code();
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

                push_result(fc::raw::pack(1));
                push_result(fc::raw::pack(fee.amount));

            } catch (const fc::exception& e) {
                err_num = e.code();
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
                err_num = e.code();
            }
        }

        void LvmInterface::get_chain_random() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                uint32_t ret = _evaluate_state->p_result_trx.id().hash(_chain_interface->get_current_random_seed())._hash[2];
                push_result(fc::raw::pack(ret));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }
        void LvmInterface::get_transaction_id() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                push_result(fc::raw::pack<std::string>(_evaluate_state->trx.id().str()));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }
        void LvmInterface::get_header_block_num() {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                uint32_t ret = _chain_interface->get_head_block_num();
                push_result(fc::raw::pack(ret));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }
        void LvmInterface::wait_for_future_random(const int next) {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                uint32_t target = _chain_interface->get_head_block_num() + next;

                if (target < next) {
                    target = 0;
                }

                push_result(fc::raw::pack(target));

            } catch (fc::exception& e) {
                err_num = e.code();
                return ;
            }
        }
        void LvmInterface::get_waited(uint32_t num) {
            try {
                int32_t ret;

                if (num <= 1) {
                    ret = -2;
                    push_result(fc::raw::pack(ret));
                    return;
                }

                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                if (_chain_interface->get_head_block_num() < num) {
                    ret = -1;
                    push_result(fc::raw::pack(ret));
                    return ;
                }

                BlockIdType id = _chain_interface->get_block_id(num);
                BlockHeader _header = _chain_interface->get_block_header(id);
                SecretHashType _hash = _header.previous_secret;
                auto default_id = BlockIdType();

                for (int i = 0; i < 50; i++) {
                    if ((id = _header.previous) == default_id) {
                        break;
                    }

                    _header = _chain_interface->get_block_header(id);
                    _hash = _hash.hash(_header.previous_secret);
                }

                ret = 1;
                push_result(fc::raw::pack(ret));
                ret = _hash._hash[3] % (1 << 31 - 1);
                push_result(fc::raw::pack(ret));

            } catch (const fc::exception& e) {
                err_num = e.code();
            }
        }

        void LvmInterface::commit_storage_changes_to_thinkyoung(AllStorageDataChange& change_items) {
            try {
                if (!_evaluate_state || !(_chain_interface = _evaluate_state->_current_state)) {
                    FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                }

                for (auto all_con_chg_iter = change_items.begin(); all_con_chg_iter != change_items.end(); ++all_con_chg_iter) {
                    StorageOperation storage_op;
                    std::string contract_id = all_con_chg_iter->first;
                    storage_op.contract_change_storages=all_con_chg_iter->second;
                    storage_op.contract_id = Address(contract_id, AddressType::contract_address);
                    _evaluate_state->p_result_trx.push_storage_operation(storage_op);
                }

            } catch (const fc::exception& e) {
                err_num = e.code();
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
                    return;
                }

                if (!Address::is_valid(to_addr, ALP_ADDRESS_PREFIX)) {
                    push_result(fc::raw::pack(-4));
                    return;
                }

                _evaluate_state->transfer_asset_from_contract(amount, asset_type, Address(contract_address,
                        AddressType::contract_address), Address(to_addr, AddressType::alp_address));
                _evaluate_state->_contract_balance_remain -= amount;
                push_result(fc::raw::pack(0));

            } catch (const fc::exception& e) {
                err_num = e.code();
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
                    return;
                }

                auto acc_entry = _evaluate_state->_current_state->get_account_entry(to_account_name);

                if (!acc_entry.valid()) {
                    push_result(fc::raw::pack(-7));
                    return;
                }

                transfer_from_contract_to_address(contract_address, acc_entry->owner_address().AddressToString(), asset_type, amount);

            } catch (const fc::exception& e) {
                err_num = e.code();
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
                err_num = e.code();
                return;
            }
        }
        void LvmInterface::push_result(std::vector<char>& value) {
            result.push_back(value);
        }
    }
}

