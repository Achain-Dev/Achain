#include "blockchain/ContractOperations.hpp"
#include "blockchain/TransactionEvaluationState.hpp"
#include "blockchain/ChainInterface.hpp"
#include "blockchain/Exceptions.hpp"
#include "fc/crypto/ripemd160.hpp"
#include "fc/crypto/sha512.hpp"
#include <glua/thinkyoung_lua_lib.h>
#include "blockchain/BalanceOperations.hpp"
#include <sstream>
#include <blockchain/TransactionOperations.hpp>
#include <contract/task.hpp>

#define  CLOSE_REGISTER_CONTRACT 0
#define  ENABLELVM 0
namespace thinkyoung {
    namespace blockchain {
        bool is_contract_has_method(const string& method_name, const std::set<std::string>& abi_set) {
            class finder {
              public:
                finder(const std::string& _cmp_str) : cmp_str(_cmp_str) {}
                bool operator()(const std::string& str) {
                    return str == cmp_str;
                }
              private:
                std::string cmp_str;
            };
            auto iter = abi_set.begin();
            
            if ((iter = std::find_if(abi_set.begin(), abi_set.end(), finder(method_name))) != abi_set.end())
                return true;
                
            return false;
        }
        
        ShareType get_amount_sum(ShareType amount_l, ShareType amount_r) {
            ShareType amount_sum = amount_l + amount_r;
            
            if (amount_sum < amount_l || amount_sum < amount_r)
                FC_CAPTURE_AND_THROW(addition_overflow, ("Addition overflow"));
                
            return amount_sum;
        }
        
        ShareType check_balances(TransactionEvaluationState& eval_state, const std::map<BalanceIdType, ShareType>& balances_check, const Address& address_check) {
            std::map<BalanceIdType, ShareType>::const_iterator it = balances_check.begin();
            ShareType all_amount = 0;
            ShareType tmp_amount = 0;
            
            while (it != balances_check.end()) {
                if (it->second <= 0)
                    FC_CAPTURE_AND_THROW(negative_withdraw, (it->second));
                    
                oBalanceEntry current_balance_entry = eval_state._current_state->get_balance_entry(it->first);
                
                if (!current_balance_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_balance_entry, (it->first));
                    
                FC_ASSERT(current_balance_entry->asset_id() == 0, "Asset must use ACT");
                
                if (it->second > current_balance_entry->get_spendable_balance(eval_state._current_state->now()).amount)
                    FC_CAPTURE_AND_THROW(insufficient_funds, (current_balance_entry)(it->second));
                    
                auto asset_rec = eval_state._current_state->get_asset_entry(current_balance_entry->condition.asset_id);
                FC_ASSERT(asset_rec.valid(), "Invalid asset entry");
                bool issuer_override = asset_rec->is_retractable() && eval_state.verify_authority(asset_rec->authority);
                
                if (!issuer_override) {
                    FC_ASSERT(!asset_rec->is_balance_frozen(), "Balance frozen");
                    
                    switch ((WithdrawConditionTypes)current_balance_entry->condition.type) {
                        case withdraw_signature_type: {
                            const WithdrawWithSignature condition = current_balance_entry->condition.as<WithdrawWithSignature>();
                            const Address owner = condition.owner;
                            
                            if (!eval_state.check_signature(owner))
                                FC_CAPTURE_AND_THROW(missing_signature, (owner));
                                
                            if (owner != address_check)
                                FC_CAPTURE_AND_THROW(operator_and_owner_not_the_same, ("signature not match"));
                                
                            break;
                        }
                        
                        default:
                            FC_CAPTURE_AND_THROW(invalid_withdraw_condition, (current_balance_entry->condition));
                    }
                }
                
                all_amount = get_amount_sum(tmp_amount, it->second);
                tmp_amount = all_amount;
                ++it;
            }
            
            return all_amount;
        }
        
        void withdraw_enough_balances(const std::map<BalanceIdType, ShareType>& balances_from, ShareType amount_required, std::map<BalanceIdType, ShareType>& balances_to) {
            balances_to.clear();
            std::map<BalanceIdType, ShareType>::const_iterator it = balances_from.begin();
            
            while (it != balances_from.end()) {
                if (amount_required <= it->second) {
                    balances_to.insert(std::pair<BalanceIdType, ShareType>(it->first, amount_required));
                    break;
                    
                } else {
                    balances_to.insert(std::pair<BalanceIdType, ShareType>(it->first, it->second));
                    amount_required = amount_required - it->second;
                }
                
                ++it;
            }
            
            if (it == balances_from.end())
                FC_CAPTURE_AND_THROW(insufficient_funds, (amount_required));
        }
        
        bool CallContractOperation::is_function_not_allow_call(const string& method) {
            return method == CON_INIT_INTERFACE || method == CON_ON_DEPOSIT_INTERFACE
                   || method == CON_ON_UPGRADE_INTERFACE || method == CON_ON_DESTROY_INTERFACE;
        }
        
        void UpgradeContractOperation::evaluate(TransactionEvaluationState& eval_state) const {
            try {
                FC_ASSERT(transaction_fee.amount >= 0, "transaction should not be negtive num");
                
                if (eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(in_result_of_execute, ("UpgradeContractOperation can only in origin transaction"));
                    
                oContractEntry entry = eval_state._current_state->get_contract_entry(id);
                
                if (!entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (id));
                    
                if (NOT eval_state._current_state->is_temporary_contract(entry->level))
                    FC_CAPTURE_AND_THROW(contract_upgraded, (id));
                    
                if (eval_state._current_state->is_destroyed_contract(entry->state))
                    FC_CAPTURE_AND_THROW(contract_destroyed, (id));
                    
                if (NOT eval_state._current_state->is_valid_contract_name(name))
                    FC_CAPTURE_AND_THROW(contract_name_illegal, (name));
                    
                if (NOT eval_state._current_state->is_valid_contract_description(desc))
                    FC_CAPTURE_AND_THROW(contract_desc_illegal, (desc));
                    
                if (eval_state._current_state->get_contract_entry(name).valid())
                    FC_CAPTURE_AND_THROW(contract_name_in_use, (name));
                    
                //����Ƿ������Լ�����ߵ�ǩ��
                if (!eval_state.check_signature(entry->owner))
                    FC_CAPTURE_AND_THROW(missing_signature, ("Upgrading contract need owner's signature"));
                    
                eval_state.contract_operator = entry->owner;
                // check contract margin
                BalanceIdType margin_balance_id = eval_state._current_state->get_balanceid(entry->id, WithdrawBalanceTypes::withdraw_margin_type);
                oBalanceEntry margin_balance_entry = eval_state._current_state->get_balance_entry(margin_balance_id);
                FC_ASSERT(margin_balance_entry.valid(), "invalid margin balance id");
                FC_ASSERT(margin_balance_entry->asset_id() == 0, "invalid margin balance asset type");
                
                if (margin_balance_entry->balance != ALP_DEFAULT_CONTRACT_MARGIN)
                    FC_CAPTURE_AND_THROW(invalid_margin_amount, (margin_balance_entry->balance));
                    
                bool has_on_upgrade = is_contract_has_method(CON_ON_UPGRADE_INTERFACE, entry->code.abi);
                ShareType all_amount = 0;
                
                if (!eval_state.evaluate_contract_testing) {
                    all_amount = check_balances(eval_state, balances, Address(entry->owner));
                }
                
                if (!has_on_upgrade) {
                    FC_ASSERT(exec_limit.amount >= 0, "execute limit should enqual or bigger than zero");
                    
                    if (!eval_state.evaluate_contract_testing) {
                        eval_state.p_result_trx.operations.resize(0);
                        eval_state.p_result_trx.operations.push_back(TransactionOperation(eval_state.trx));
                        ShareType required = transaction_fee.amount;
                        
                        if (all_amount < required)
                            FC_CAPTURE_AND_THROW(insufficient_funds, ("no enough balance"));
                            
                        map<BalanceIdType, ShareType> withdraw_map;
                        withdraw_enough_balances(balances, required, withdraw_map);
                        eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                        //operate contract balances
                        ShareType deposit_amount = margin_balance_entry->balance;
                        eval_state.p_result_trx.operations.push_back(WithdrawContractOperation(margin_balance_entry->id(), margin_balance_entry->balance));
                        eval_state.p_result_trx.operations.push_back(OnUpgradeOperation(id, name, desc));
                        eval_state.required_fees = transaction_fee + Asset(ALP_DEFAULT_CONTRACT_MARGIN, 0);
                    }
                    
                } else { // has on upgrade function
                    //judge execute limit
                    FC_ASSERT(exec_limit.amount > 0, "execute limit should bigger than zero");
                    
                    if (!eval_state.skipexec) {
                        lua::lib::GluaStateScope scope;
                        int exception_code = 0;
                        string exception_msg;
                        
                        try {
                            FC_ASSERT(eval_state.p_result_trx.operations.size() == 0);
                            eval_state.p_result_trx.push_transaction(eval_state.trx);
                            GluaStateValue statevalue;
                            statevalue.pointer_value = &eval_state;
                            auto contract_entry = eval_state._current_state->get_contract_entry(id);
                            lua::lib::add_global_string_variable(scope.L(), "caller", ((string)(contract_entry->owner)).c_str());
                            lua::lib::add_global_string_variable(scope.L(), "caller_address", ((string)(Address(contract_entry->owner))).c_str());
                            lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
                            lua::api::global_glua_chain_api->clear_exceptions(scope.L());
                            int limit = eval_state._current_state->get_limit(0, exec_limit.amount);
                            
                            if (limit <= 0)
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                
                            scope.set_instructions_limit(limit);
                            //the arg of on_destroy is empty
                            string default_arg = "";
                            scope.execute_contract_api_by_address(id.AddressToString(AddressType::contract_address).c_str(), CON_ON_UPGRADE_INTERFACE, default_arg.c_str(), nullptr);
                            
                            if (scope.L()->force_stopping == true && scope.L()->exit_code == LUA_API_INTERNAL_ERROR)
                                FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                                
                            exception_code = lua::lib::get_lua_state_value(scope.L(), "exception_code").int_value;
                            
                            if (exception_code > 0) {
                                exception_msg = (char*)lua::lib::get_lua_state_value(scope.L(), "exception_msg").string_value;
                                
                                if (exception_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR) {
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                    
                                } else {
                                    thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
                                    throw con_err;
                                }
                            }
                            
                            if (ENABLELVM) {
                                //TODO
                                // this part neeed to tune while debuging
                                // some code need to be added/del
                                UpgradeTask _upgradetask;
                                _upgradetask.num_limit = limit;
                                _upgradetask.statevalue = reinterpret_cast<intptr_t>(&eval_state);
                                _upgradetask.str_caller = ((string)(contract_entry->owner)).c_str();
                                _upgradetask.str_caller_address = ((string)(Address(contract_entry->owner))).c_str();
                                _upgradetask.str_contract_address = this->id.AddressToString(AddressType::contract_address).c_str();
                                thinkyoung::blockchain::ChainInterface* cur_state = eval_state._current_state;
                                oContractEntry entry = cur_state->get_contract_entry(_upgradetask.str_contract_address);
                                
                                if (entry.valid()) {
                                    string address_str = entry->id.AddressToString(AddressType::contract_address);
                                    _upgradetask.str_contract_id = address_str.c_str();
                                    //*address_size = address_str.length();
                                    //strncpy(address, address_str.c_str(), CONTRACT_ID_MAX_LENGTH - 1);
                                    //address[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
                                }
                                
                                blockchain::oContractEntry entry1 = cur_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(this->id), AddressType::contract_address));
                                
                                if (!entry1.valid()) {}
                                
                                Code& _code = entry1->code;
                                _upgradetask.contract_code = _code;
                            }
                            
                            ShareType exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count()).amount;
                            eval_state.exec_cost = Asset(exec_cost, 0);
                            FC_ASSERT(exec_cost <= exec_limit.amount && exec_cost > 0, "costs of execution can be only between 0 and costlimit");
                            ShareType required = get_amount_sum(exec_cost, transaction_fee.amount);
                            
                            if (!eval_state.evaluate_contract_testing) {
                                map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, required, withdraw_map);
                                // withdraw contract margin
                                ShareType deposit_amount = margin_balance_entry->balance;
                                eval_state.p_result_trx.operations.push_back(WithdrawContractOperation(margin_balance_entry->id(), margin_balance_entry->balance));
                                // withdraw balance
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                eval_state.p_result_trx.operations.push_back(OnUpgradeOperation(id, name, desc));
                                eval_state.required_fees = transaction_fee + Asset(ALP_DEFAULT_CONTRACT_MARGIN, 0);
                            }
                            
                        } catch (contract_run_out_of_money& e) {
                            if (!eval_state.evaluate_contract_testing) {
                                if (eval_state.throw_exec_exception)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                    
                                ShareType required = get_amount_sum(exec_limit.amount, transaction_fee.amount);
                                map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, required, withdraw_map);
                                eval_state.p_result_trx.operations.resize(0);
                                eval_state.p_result_trx.push_transaction(eval_state.trx);
                                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                                eval_state.required_fees = transaction_fee;
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                
                            } else {
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_testing_money);
                            }
                        }//end-catch
                        
                        catch (contract_error&e) {
                            if (!eval_state.evaluate_contract_testing) {
                                if (eval_state.throw_exec_exception)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error, (exception_msg));
                                    
                                Asset exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count());
                                std::map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, (exec_cost + transaction_fee).amount, withdraw_map);
                                eval_state.p_result_trx.operations.resize(1);
                                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                
                            } else {
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error_in_testing, (exception_msg));
                            }
                        }
                    }//end-if
                }//end-else
                
                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
        
        void OnUpgradeOperation::evaluate(TransactionEvaluationState& eval_state)const {
            try {
                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(not_be_result_of_execute, ("OnUpgradeOperation should be in result transaction"));
                    
                oContractEntry entry = eval_state._current_state->get_contract_entry(id);
                
                if (!entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (id));
                    
                entry->level = ContractLevel::forever;
                entry->contract_name = name;
                entry->description = desc;
                eval_state._current_state->store_contract_entry(*entry);
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
        
        void DestroyContractOperation::evaluate(TransactionEvaluationState& eval_state)const {
            try {
                FC_ASSERT(transaction_fee.amount >= 0, "transaction should not be negtive num");
                
                if (eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(in_result_of_execute, ("DestroyContractOperation can only in origin transaction"));
                    
                oContractEntry entry = eval_state._current_state->get_contract_entry(id);
                
                if (!entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (id));
                    
                if (eval_state._current_state->is_destroyed_contract(entry->state))
                    FC_CAPTURE_AND_THROW(contract_destroyed, (id));
                    
                if (NOT eval_state._current_state->is_temporary_contract(entry->level))
                    FC_CAPTURE_AND_THROW(permanent_contract, (id));
                    
                //����Ƿ������Լ�����ߵ�ǩ��
                if (!eval_state.check_signature(entry->owner))
                    FC_CAPTURE_AND_THROW(missing_signature, ("destroy contract need owner's signature"));
                    
                eval_state.contract_operator = entry->owner;
                // always deposit contract  margin to contract owner(contract destroyer)
                BalanceIdType margin_balance_id = eval_state._current_state->get_balanceid(entry->id, WithdrawBalanceTypes::withdraw_margin_type);
                oBalanceEntry margin_balance_entry = eval_state._current_state->get_balance_entry(margin_balance_id);
                FC_ASSERT(margin_balance_entry.valid(), "invalid margin balance id");
                FC_ASSERT(margin_balance_entry->asset_id() == 0, "invalid margin balance asset type");
                
                if (margin_balance_entry->balance != ALP_DEFAULT_CONTRACT_MARGIN)
                    FC_CAPTURE_AND_THROW(invalid_margin_amount, (margin_balance_entry->balance));
                    
                BalanceIdType contract_balance_id = eval_state._current_state->get_balanceid(entry->id, WithdrawBalanceTypes::withdraw_contract_type);
                oBalanceEntry contract_balance_entry = eval_state._current_state->get_balance_entry(contract_balance_id);
                bool has_on_destroy = is_contract_has_method(CON_ON_DESTROY_INTERFACE, entry->code.abi);
                ShareType all_amount = 0;
                
                if (!eval_state.evaluate_contract_testing) {
                    all_amount = check_balances(eval_state, balances, Address(entry->owner));
                }
                
                // according to the on_destroy function to deposit related balance
                if (!has_on_destroy) {
                    //judge execute limit
                    FC_ASSERT(exec_limit.amount >= 0, "execute limit should bigger than zero");
                    
                    if (!eval_state.evaluate_contract_testing) {
                        eval_state.p_result_trx.operations.resize(0);
                        eval_state.p_result_trx.operations.push_back(TransactionOperation(eval_state.trx));
                        ShareType required = transaction_fee.amount;
                        
                        if (all_amount < required)
                            FC_CAPTURE_AND_THROW(insufficient_funds, ("no enough balance"));
                            
                        map<BalanceIdType, ShareType> withdraw_map;
                        withdraw_enough_balances(balances, required, withdraw_map);
                        eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                        //operate contract balances
                        ShareType deposit_amount = margin_balance_entry->balance;
                        eval_state.p_result_trx.operations.push_back(WithdrawContractOperation(margin_balance_entry->id(), margin_balance_entry->balance));
                        eval_state.p_result_trx.operations.push_back(DepositOperation(Address(entry->owner), Asset(deposit_amount, 0)));
                        
                        if (contract_balance_entry.valid())
                            eval_state.p_result_trx.operations.push_back(OnDestroyOperation(id, Asset(contract_balance_entry->balance, 0)));
                            
                        else
                            eval_state.p_result_trx.operations.push_back(OnDestroyOperation(id, Asset(0, 0)));
                            
                        eval_state.required_fees = transaction_fee;
                    }
                    
                } else { // has on destroy function
                    //judge execute limit
                    FC_ASSERT(exec_limit.amount > 0, "execute limit should bigger than zero");
                    
                    if (!eval_state.skipexec) {
                        int exception_code = 0;
                        string exception_msg;
                        lua::lib::GluaStateScope scope;
                        
                        try {
                            FC_ASSERT(eval_state.p_result_trx.operations.size() == 0);
                            eval_state.p_result_trx.push_transaction(eval_state.trx);
                            GluaStateValue statevalue;
                            statevalue.pointer_value = &eval_state;
                            
                            if (contract_balance_entry.valid()) {
                                // deposit contract account balance to destroyer account
                                FC_ASSERT(contract_balance_entry->asset_id() == 0, "invalid contract balance asset type");
                                eval_state._contract_balance_remain = contract_balance_entry->get_spendable_balance(eval_state._current_state->now()).amount;
                                
                            } else
                                eval_state._contract_balance_remain = 0;
                                
                            lua::lib::add_global_string_variable(scope.L(), "caller", ((string)(entry->owner)).c_str());
                            lua::lib::add_global_string_variable(scope.L(), "caller_address", ((string)(Address(entry->owner))).c_str());
                            lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
                            lua::api::global_glua_chain_api->clear_exceptions(scope.L());
                            int limit = eval_state._current_state->get_limit(0, exec_limit.amount);
                            
                            if (limit <= 0)
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                
                            scope.set_instructions_limit(limit);
                            //the arg of on_destroy is empty
                            string default_arg = "";
                            scope.execute_contract_api_by_address(id.AddressToString(AddressType::contract_address).c_str(), CON_ON_DESTROY_INTERFACE, default_arg.c_str(), nullptr);
                            
                            if (scope.L()->force_stopping == true && scope.L()->exit_code == LUA_API_INTERNAL_ERROR)
                                FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                                
                            exception_code = lua::lib::get_lua_state_value(scope.L(), "exception_code").int_value;
                            
                            if (exception_code > 0) {
                                exception_msg = (char*)lua::lib::get_lua_state_value(scope.L(), "exception_msg").string_value;
                                
                                if (exception_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                    
                                else {
                                    thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
                                    throw con_err;
                                }
                            }
                            
                            if (ENABLELVM) {
                                //TODO
                                // this part neeed to tune while debuging
                                // some code need to be added/del
                                DestroyTask _destroytask;
                                _destroytask.num_limit = limit;
                                _destroytask.statevalue = reinterpret_cast<intptr_t>(&eval_state);
                                _destroytask.str_caller = ((string)(entry->owner)).c_str();
                                _destroytask.str_caller_address = ((string)(Address(entry->owner))).c_str();
                                _destroytask.str_contract_address = id.AddressToString(AddressType::contract_address).c_str();
                                thinkyoung::blockchain::ChainInterface* cur_state = eval_state._current_state;
                                oContractEntry entry = cur_state->get_contract_entry(_destroytask.str_contract_address);
                                
                                if (entry.valid()) {
                                    string address_str = entry->id.AddressToString(AddressType::contract_address);
                                    _destroytask.str_contract_id = address_str.c_str();
                                    //*address_size = address_str.length();
                                    //strncpy(address, address_str.c_str(), CONTRACT_ID_MAX_LENGTH - 1);
                                    //address[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
                                }
                                
                                blockchain::oContractEntry entry1 = cur_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(id), AddressType::contract_address));
                                
                                if (!entry1.valid()) {}
                                
                                Code& _code = entry1->code;
                                _destroytask.contract_code = _code;
                            }
                            
                            ShareType exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count()).amount;
                            eval_state.exec_cost = Asset(exec_cost, 0);
                            FC_ASSERT(exec_cost <= exec_limit.amount && exec_cost > 0, "costs of execution can be only between 0 and costlimit");
                            ShareType required = get_amount_sum(exec_cost, transaction_fee.amount);
                            
                            if (!eval_state.evaluate_contract_testing) {
                                map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, required, withdraw_map);
                                eval_state.p_result_trx.operations.push_back(OnDestroyOperation(id, Asset(eval_state._contract_balance_remain, 0)));
                                ShareType deposit_amount = margin_balance_entry->balance;
                                eval_state.p_result_trx.operations.push_back(WithdrawContractOperation(margin_balance_entry->id(), margin_balance_entry->balance));
                                eval_state.p_result_trx.operations.push_back(DepositOperation(Address(entry->owner), Asset(deposit_amount, 0)));
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                eval_state.required_fees = transaction_fee;
                            }
                            
                        } catch (contract_run_out_of_money& e) {
                            if (!eval_state.evaluate_contract_testing) {
                                if (eval_state.throw_exec_exception)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                    
                                ShareType required = get_amount_sum(exec_limit.amount, transaction_fee.amount);
                                map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, required, withdraw_map);
                                eval_state.required_fees = transaction_fee;
                                eval_state.p_result_trx.operations.resize(0);
                                eval_state.p_result_trx.push_transaction(eval_state.trx);
                                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                
                            } else
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_testing_money);
                        }//end-catch
                        
                        catch (contract_error&e) {
                            if (!eval_state.evaluate_contract_testing) {
                                if (eval_state.throw_exec_exception)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error, (exception_msg));
                                    
                                Asset exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count());
                                std::map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, (exec_cost + transaction_fee).amount, withdraw_map);
                                eval_state.p_result_trx.operations.resize(1);
                                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                
                            } else
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error_in_testing, (exception_msg));
                        }
                    }//end-if
                }//end-else
                
                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
        
        void OnDestroyOperation::evaluate(TransactionEvaluationState& eval_state)const {
            try {
                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(not_be_result_of_execute, ("OnDestroyOperation should be in result transaction"));
                    
                oContractEntry entry = eval_state._current_state->get_contract_entry(id);
                
                if (!entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (id));
                    
                BalanceIdType contract_balance_id = eval_state._current_state->get_balanceid(entry->id, WithdrawBalanceTypes::withdraw_contract_type);
                oBalanceEntry contract_balance_entry = eval_state._current_state->get_balance_entry(contract_balance_id);
                
                if (contract_balance_entry.valid()) {
                    FC_ASSERT(contract_balance_entry->asset_id() == 0, "invalid contract balance asset type");
                    ShareType deposit_amount = contract_balance_entry->get_spendable_balance(eval_state._current_state->now()).amount;
                    FC_ASSERT(deposit_amount == amount.amount, "the contract balance remain is not equal to the amount in on destroy op");
                    eval_state.add_balance(Asset(deposit_amount, 0));
                    //reset contract account balance
                    contract_balance_entry->balance = 0;
                    eval_state._current_state->store_balance_entry(*contract_balance_entry);
                    //����Լ�е�������Լ�����ߵ��˻���
                    WithdrawWithSignature withdraw = WithdrawWithSignature(Address(entry->owner));
                    WithdrawCondition cond = WithdrawCondition(withdraw, 0, 0, WithdrawBalanceTypes::withdraw_common_type);
                    BalanceIdType destroyer_balance_id = cond.get_address();
                    oBalanceEntry destroyer_balance_entry = eval_state._current_state->get_balance_entry(destroyer_balance_id);
                    
                    if (!destroyer_balance_entry.valid()) {
                        destroyer_balance_entry = BalanceEntry(cond);
                        
                        if (cond.type == withdraw_escrow_type)
                            destroyer_balance_entry->meta_data = variant_object("creating_transaction_id", eval_state.trx.id());
                    }
                    
                    if (destroyer_balance_entry->balance == 0)
                        destroyer_balance_entry->deposit_date = eval_state._current_state->now();
                        
                    else {
                        fc::uint128 old_sec_since_epoch(destroyer_balance_entry->deposit_date.sec_since_epoch());
                        fc::uint128 new_sec_since_epoch(eval_state._current_state->now().sec_since_epoch());
                        fc::uint128 avg = (old_sec_since_epoch * destroyer_balance_entry->balance) + (new_sec_since_epoch * deposit_amount);
                        avg /= (destroyer_balance_entry->balance + deposit_amount);
                        destroyer_balance_entry->deposit_date = time_point_sec(avg.to_integer());
                    }
                    
                    destroyer_balance_entry->balance += deposit_amount;
                    eval_state.sub_balance(destroyer_balance_id, Asset(deposit_amount, destroyer_balance_entry->condition.asset_id));
                    destroyer_balance_entry->last_update = eval_state._current_state->now();
                    const oAssetEntry asset_rec = eval_state._current_state->get_asset_entry(destroyer_balance_entry->condition.asset_id);
                    FC_ASSERT(asset_rec.valid(), "Invalid asset entry");
                    eval_state._current_state->store_balance_entry(*destroyer_balance_entry);
                }
                
                entry->state = ContractState::deleted;
                eval_state._current_state->store_contract_entry(*entry);
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
        
        ContractIdType RegisterContractOperation::get_contract_id() const {
            ContractIdType id;
            fc::sha512::encoder enc;
            fc::raw::pack(enc, *this);
            id.addr = fc::ripemd160::hash(enc.result());
            return id;
        }
        
        void RegisterContractOperation::evaluate(TransactionEvaluationState& eval_state) const {
#if CLOSE_REGISTER_CONTRACT
            return;
#endif
            FC_ASSERT(initcost.amount > 0, "initcost should be greater than 0");
            FC_ASSERT(transaction_fee.amount >= 0, "transaction should not be negative num");
            
            if (eval_state.evaluate_contract_result)
                FC_CAPTURE_AND_THROW(in_result_of_execute, ("RegisterContractOperation can only in origin transaction"));
                
            //���contract_id�Ƿ����ظ�
            if (eval_state._current_state->get_contract_entry(get_contract_id()).valid())
                FC_CAPTURE_AND_THROW(contract_address_in_use, ("contract address already in use"));
                
            //����Ƿ������Լ�����ߵ�ǩ��
            if (!eval_state.check_signature(owner))
                FC_CAPTURE_AND_THROW(missing_signature, ("Registering contract need owner's signature"));
                
            //������
            ShareType all_amount = 0;
            ShareType required = 0;
            ShareType register_fee = 0;
            
            if (!eval_state.evaluate_contract_testing) {
                all_amount = check_balances(eval_state, balances, Address(this->owner));
                ShareType margin = eval_state._current_state->get_default_margin().amount;
                required = 0;
                register_fee = eval_state._current_state->get_contract_register_fee(this->contract_code).amount;
                FC_ASSERT(margin > 0, "margin can only bigger than 0");
                FC_ASSERT(register_fee > 0, "register_fee can must bigger than 0");
                FC_ASSERT(initcost.amount > 0, "init costs can must bigger than 0");
                FC_ASSERT(transaction_fee.amount > 0, "transaction_fee can must bigger than 0");
                required = get_amount_sum(margin, register_fee);
                required = get_amount_sum(required, initcost.amount);
                required = get_amount_sum(required, transaction_fee.amount);
                
                if (all_amount < required)
                    FC_CAPTURE_AND_THROW(insufficient_funds, ("no enough balance"));
            }
            
            ContractEntry entry;
            entry.owner = owner;
            entry.code = contract_code;
            entry.level = ContractLevel::temp;
            entry.state = ContractState::valid;
            entry.id = get_contract_id();
            entry.trx_id = eval_state.trx.id();
            
            if (this->contract_code.code_hash != this->contract_code.GetHash())
                FC_CAPTURE_AND_THROW(code_hash_error, ("code hash not match"));
                
            eval_state._current_state->store_contract_entry(entry);
            //��¼��Լע����
            eval_state.contract_operator = owner;
            eval_state.required_fees = transaction_fee + eval_state._current_state->get_contract_register_fee(this->contract_code);
            
            if (!eval_state.skipexec) {
                lua::lib::GluaStateScope scope;
                int exception_code = 0;
                string exception_msg;
                
                try {
                    FC_ASSERT(eval_state.p_result_trx.operations.size() == 0);
                    GluaStateValue statevalue;
                    statevalue.pointer_value = &eval_state;
                    lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
                    lua::lib::add_global_string_variable(scope.L(), "caller", ((string)(this->owner)).c_str());
                    lua::lib::add_global_string_variable(scope.L(), "caller_address", ((string)(Address(this->owner))).c_str());
                    lua::api::global_glua_chain_api->clear_exceptions(scope.L());
                    int limit = eval_state._current_state->get_limit(0, initcost.amount);
                    
                    if (limit <= 0)
                        FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                        
                    scope.set_instructions_limit(limit);
                    eval_state.p_result_trx.operations.resize(0);
                    eval_state.p_result_trx.push_transaction(eval_state.trx);
                    eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                    eval_state.p_result_trx.operations.push_back(ContractInfoOperation(get_contract_id(), owner, contract_code, register_time));
                    scope.execute_contract_init_by_address(get_contract_id().AddressToString(AddressType::contract_address).c_str(), nullptr, nullptr);
                    exception_code = lua::lib::get_lua_state_value(scope.L(), "exception_code").int_value;
                    
                    if (exception_code > 0) {
                        exception_msg = ((char*)lua::lib::get_lua_state_value(scope.L(), "exception_msg").string_value);
                        
                        if (exception_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR)
                            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                            
                        else {
                            thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
                            throw con_err;
                        }
                    }
                    
                    ShareType exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count()).amount;
                    eval_state.exec_cost = Asset(exec_cost, 0);
                    FC_ASSERT(exec_cost <= initcost.amount&&exec_cost > 0, "costs of execution can be only between 0 and initcost");
                    
                    if (!eval_state.evaluate_contract_testing) {
                        ShareType required = get_amount_sum(exec_cost, eval_state._current_state->get_default_margin().amount);
                        required = get_amount_sum(required, register_fee);
                        required = get_amount_sum(required, transaction_fee.amount);
                        map<BalanceIdType, ShareType> withdraw_map;
                        withdraw_enough_balances(balances, required, withdraw_map);
                        eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                        eval_state.p_result_trx.operations.push_back(DepositContractOperation(get_contract_id(), eval_state._current_state->get_default_margin(), deposit_contract_margin));//todo ��֤�����
                    }
                    
                } catch (contract_run_out_of_money& e) {
                    if (!eval_state.evaluate_contract_testing) {
                        if (eval_state.throw_exec_exception)
                            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                            
                        eval_state.p_result_trx.operations.resize(0);
                        eval_state.p_result_trx.push_transaction(eval_state.trx);
                        eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                        map<BalanceIdType, ShareType> withdraw_map;
                        required = get_amount_sum(register_fee, transaction_fee.amount);
                        required = get_amount_sum(required, initcost.amount);
                        withdraw_enough_balances(balances, required, withdraw_map);
                        eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                        
                    } else
                        FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_testing_money);
                        
                } catch (const contract_error& e) {
                    if (!eval_state.evaluate_contract_testing) {
                        if (eval_state.throw_exec_exception)
                            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error, (exception_msg));
                            
                        Asset exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count());
                        std::map<BalanceIdType, ShareType> withdraw_map;
                        withdraw_enough_balances(balances, (exec_cost + eval_state.required_fees).amount, withdraw_map);
                        eval_state.p_result_trx.operations.resize(1);
                        eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                        eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                        
                    } else
                        FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error_in_testing, (exception_msg));
                }
            }
            
            eval_state._current_state->remove_contract_entry(get_contract_id());
        }
        
        void CallContractOperation::evaluate(TransactionEvaluationState& eval_state) const {
            try {
                FC_ASSERT(costlimit.amount > 0, "costlimit should be greater than 0");
                FC_ASSERT(transaction_fee.amount >= 0, "transaction should not be negtive num");
                
                if (eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(in_result_of_execute, ("CallContractOperation can only in origin transaction"));
                    
                oContractEntry entry = eval_state._current_state->get_contract_entry(contract);
                
                if (!entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (contract));
                    
                if (entry->state == thinkyoung::blockchain::ContractState::deleted)
                    FC_CAPTURE_AND_THROW(contract_destroyed, (contract));
                    
                if (CallContractOperation::is_function_not_allow_call(method))
                    FC_CAPTURE_AND_THROW(method_can_not_be_called_explicitly, (method)("method can't be called explicitly !"));
                    
                if (args.length() > CONTRACT_PARAM_MAX_LEN)
                    FC_CAPTURE_AND_THROW(contract_parameter_length_over_limit, ("the parameter length of contract function is over limit"));
                    
                bool has_call_method = is_contract_has_method(method, entry->code.abi);
                
                if (NOT has_call_method)
                    FC_CAPTURE_AND_THROW(method_not_exist, (method));
                    
                //��֤��Լ�����߱����Ƿ����Լ���ý��׵���
                Address caller_address = Address(caller);
                
                if (!eval_state.check_signature(caller_address))
                    FC_CAPTURE_AND_THROW(missing_signature, (caller_address));
                    
                //��¼��Լ������
                eval_state.contract_operator = caller;
                //��֤balance���
                ShareType all_amount = 0;
                ShareType required = 0;
                
                if (!eval_state.evaluate_contract_testing) {
                    all_amount = check_balances(eval_state, balances, Address(this->caller));
                    //ʵ�ʿ۷�
                    required = get_amount_sum(costlimit.amount, transaction_fee.amount);
                    
                    if (all_amount < required)
                        FC_CAPTURE_AND_THROW(insufficient_funds, ("no enough balance"));
                }
                
                eval_state.required_fees = transaction_fee;
                
                if (!eval_state.skipexec) {
                    lua::lib::GluaStateScope scope;
                    int exception_code = 0;
                    string exception_msg;
                    
                    try {
                        /*
                        �����ɽ�����ף���ִ�д��룬ʹ��ÿ������ִ�еĺ�Լ���붼������һ���������
                        */
                        FC_ASSERT(eval_state.p_result_trx.operations.size() == 0);//һ����Լ���ý���ֻ����һ����Լ����op,����ڴ�op֮ǰһ�������н������
                        eval_state.p_result_trx.push_transaction(eval_state.trx);
                        //���ȷ���һ����ʾ��Լ���óɹ���OP
                        eval_state.p_result_trx.operations.emplace_back(Operation(OnCallSuccessOperation()));
                        eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                        GluaStateValue statevalue;
                        statevalue.pointer_value = &eval_state;
                        lua::lib::add_global_string_variable(scope.L(), "caller", ((string)(this->caller)).c_str());
                        lua::lib::add_global_string_variable(scope.L(), "caller_address", ((string)(Address(this->caller))).c_str());
                        lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
                        
                        if (!eval_state.evaluate_contract_testing)
                            FC_ASSERT(all_amount >= transaction_fee.amount, "call limit amount not enough!");
                            
                        lua::api::global_glua_chain_api->clear_exceptions(scope.L());
                        int limit = 0;
                        limit = eval_state._current_state->get_limit(0, costlimit.amount);
                        
                        if (limit <= 0) {
                            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                        }
                        
                        //add to replace the previous code
                        if (ENABLELVM) {
                            //TODO
                            // this part neeed to tune while debuging
                            // some code need to be added/del
                            CallTask _calltask;
                            _calltask.num_limit = limit;
                            _calltask.statevalue = reinterpret_cast<intptr_t>(&eval_state);
                            _calltask.str_caller = ((string)(this->caller)).c_str();
                            _calltask.str_caller_address = ((string)(Address(this->caller))).c_str();
                            _calltask.str_contract_address = this->contract.AddressToString(AddressType::contract_address).c_str();
                            _calltask.str_method = method.c_str();
                            _calltask.str_args = this->args.c_str();
                            thinkyoung::blockchain::ChainInterface* cur_state = eval_state._current_state;
                            oContractEntry entry = cur_state->get_contract_entry(_calltask.str_contract_address);
                            
                            if (entry.valid()) {
                                string address_str = entry->id.AddressToString(AddressType::contract_address);
                                _calltask.str_contract_id = address_str.c_str();
                                //*address_size = address_str.length();
                                //strncpy(address, address_str.c_str(), CONTRACT_ID_MAX_LENGTH - 1);
                                //address[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
                            }
                            
                            blockchain::oContractEntry entry1 = cur_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(this->caller), AddressType::contract_address));
                            
                            if (!entry1.valid()) {}
                            
                            Code& _code = entry1->code;
                            _calltask.contract_code = _code;
                        }
                        
                        scope.set_instructions_limit(limit);
                        scope.execute_contract_api_by_address(this->contract.AddressToString(AddressType::contract_address).c_str(), method.c_str(), this->args.c_str(), nullptr);
                        
                        if (scope.L()->force_stopping == true && scope.L()->exit_code == LUA_API_INTERNAL_ERROR)
                            FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                            
                        exception_code = lua::lib::get_lua_state_value(scope.L(), "exception_code").int_value;
                        
                        if (exception_code > 0) {
                            exception_msg = (char*)lua::lib::get_lua_state_value(scope.L(), "exception_msg").string_value;
                            
                            if (exception_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR)
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                
                            else {
                                thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
                                throw con_err;
                            }
                        }
                        
                        int left = limit - scope.get_instructions_executed_count();
                        eval_state.exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count());
                        
                        if (left > 0) {
                            //��Լ���ó�ʼ������û�л���
                            //ʵ�ʿ۷ѵ���
                            auto refund = eval_state._current_state->get_amount(left);
                            required = required - refund.amount;
                        }
                        
                    } catch (contract_run_out_of_money& e) {
                        //�����쳣���ͽ��д������Ƿ��úľ�������Ϊ���׳ɹ���׷��һ�����úľ��Ŀ۷�
                        //�����쳣����Ϊ�ǵ�ǰ������֤ʧ��
                        //��gas�ľ�ʱ����entryɾ�����ɣ�����������op,������������
                        //����ԭʼ����OP��������н������
                        if (!eval_state.evaluate_contract_testing) {
                            if (eval_state.throw_exec_exception)
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                
                            eval_state.p_result_trx.operations.resize(0);
                            eval_state.p_result_trx.push_transaction(eval_state.trx);
                            eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                            
                        } else
                            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_testing_money);
                            
                    } catch (contract_error& e) {
                        if (!eval_state.evaluate_contract_testing) {
                            if (eval_state.throw_exec_exception)
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error, (exception_msg));
                                
                            eval_state.p_result_trx.operations.resize(1);
                            eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                            required = (eval_state._current_state->get_amount(scope.get_instructions_executed_count()) + transaction_fee).amount;
                            
                        } else
                            FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error_in_testing, (exception_msg));
                    }
                    
                    if (!eval_state.evaluate_contract_testing) {
                        std::map <BalanceIdType, ShareType> withdraw_map;
                        withdraw_enough_balances(balances, required, withdraw_map);
                        //�����۷�BalanceWithdraw OP
                        eval_state.p_result_trx.push_balances_withdraw_operation(BalancesWithdrawOperation(withdraw_map));
                    }
                }
            }
            
            FC_CAPTURE_AND_RETHROW((*this));
        }
        
        void ContractInfoOperation::evaluate(TransactionEvaluationState & eval_state) const {
            try {
                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(not_be_result_of_execute, ("ContractInfoOperation can only exsit in result"));
                    
                oContractEntry contratc_entry = eval_state._current_state->get_contract_entry(contract_id);
                
                if (contratc_entry.valid())
                    FC_CAPTURE_AND_THROW(contract_address_in_use, ("Contract addres already in use"));
                    
                ContractEntry entry;
                
                if (code.code_hash != code.GetHash())
                    FC_CAPTURE_AND_THROW(code_hash_error, ("code hash not match"));
                    
                entry.code = code;
                entry.id = contract_id;
                entry.register_time = register_time;
                entry.trx_id = eval_state.trx.id();
                entry.owner = owner;
                eval_state._current_state->store_contract_entry(entry);
                eval_state._current_state->store<ContractinTrxEntry>(entry.trx_id, contract_id);
                eval_state._current_state->store<ContractTrxEntry>(contract_id, entry.trx_id);
            }
            
            FC_CAPTURE_AND_RETHROW((*this));
        }
        
        void TransferContractOperation::evaluate(TransactionEvaluationState& eval_state)const {
            try {
                FC_ASSERT(transaction_fee.amount >= 0, "transaction should not be negtive num");
                
                if (eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(in_result_of_execute, ("TransferContractOperation can only in origin transaction"));
                    
                ChainInterface* _cur_state = eval_state._current_state;
                oContractEntry entry = _cur_state->get_contract_entry(contract_id);
                
                if (!entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (contract_id));
                    
                if (entry->state == ContractState::deleted)
                    FC_CAPTURE_AND_THROW(contract_destroyed, (contract_id));
                    
                if (!eval_state.check_signature(from))
                    FC_CAPTURE_AND_THROW(missing_signature, (from));
                    
                if (transfer_amount.amount <= 0)
                    FC_CAPTURE_AND_THROW(transfer_amount_not_bigger_than_0, (transfer_amount.amount));
                    
                FC_ASSERT(transfer_amount.asset_id == 0, "transfer asset must be ACT");
                FC_ASSERT(costlimit.asset_id == 0, "Execution of contract must cost ACT");
                bool has_on_deposit = is_contract_has_method(CON_ON_DEPOSIT_INTERFACE, entry->code.abi);
                
                if (has_on_deposit)
                    FC_ASSERT(costlimit.amount > 0, "not enough costlimit");
                    
                else
                    FC_ASSERT(costlimit.amount >= 0, "not enough costlimit");
                    
                //��¼ת����
                eval_state.contract_operator = from;
                
                if (!eval_state.evaluate_contract_testing) {
                    ShareType all_amount = check_balances(eval_state, balances, Address(this->from));
                    ShareType require = 0;
                    require = get_amount_sum(costlimit.amount, transfer_amount.amount);
                    require = get_amount_sum(require, transaction_fee.amount);
                    
                    if (all_amount < require)
                        FC_CAPTURE_AND_THROW(insufficient_funds, ("no enough balances"));
                        
                    eval_state.required_fees = transaction_fee;
                }
                
                if (!has_on_deposit) {
                    if (!eval_state.evaluate_contract_testing) {
                        map<BalanceIdType, ShareType> withdraw_map;
                        ShareType required = get_amount_sum(transaction_fee.amount, transfer_amount.amount);
                        withdraw_enough_balances(balances, required, withdraw_map);
                        eval_state.p_result_trx.operations.resize(0);
                        eval_state.p_result_trx.operations.push_back(TransactionOperation(eval_state.trx));
                        eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                        eval_state.p_result_trx.operations.push_back(DepositContractOperation(contract_id, transfer_amount, deposit_contract_normal));
                    }
                    
                } else {
                    if (!eval_state.skipexec) {
                        lua::lib::GluaStateScope scope;
                        int exception_code = 0;
                        string exception_msg;
                        
                        try {
                            /*
                            �����ɽ�����ף���ִ�д��룬ʹ��ÿ������ִ�еĺ�Լ���붼������һ���������
                            */
                            FC_ASSERT(eval_state.p_result_trx.operations.size() == 0);//һ��������ֻ����һ���д���,��˴�ʱ��Ӧ�û��н������
                            eval_state.p_result_trx.push_transaction(eval_state.trx);
                            GluaStateValue statevalue;
                            statevalue.pointer_value = &eval_state;
                            lua::lib::add_global_string_variable(scope.L(), "caller", ((string)(from)).c_str());
                            lua::lib::add_global_string_variable(scope.L(), "caller_address", ((string)(Address(from))).c_str());
                            lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
                            lua::api::global_glua_chain_api->clear_exceptions(scope.L());
                            int limit = eval_state._current_state->get_limit(0, costlimit.amount);
                            
                            if (limit <= 0)
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                
                            std::stringstream transfer_stream;
                            transfer_stream << transfer_amount.amount;
                            std::string transfer_str = transfer_stream.str();
                            DepositContractOperation deposit_contract_op(contract_id, transfer_amount, deposit_contract_normal);
                            scope.set_instructions_limit(limit);
                            oBalanceEntry obalance_entry = eval_state._current_state->get_balance_entry(deposit_contract_op.balance_id());
                            BalanceEntry balance_entry(WithdrawCondition(WithdrawWithSignature(contract_id), transfer_amount.asset_id, 0, withdraw_contract_type));
                            
                            if (obalance_entry.valid())
                                balance_entry = *obalance_entry;
                                
                            else {
                                balance_entry.balance = 0;
                                balance_entry.deposit_date = eval_state._current_state->now();
                                balance_entry.last_update = eval_state._current_state->now();
                            }
                            
                            BalanceEntry new_balance_entry = balance_entry;
                            new_balance_entry.balance = balance_entry.balance + this->transfer_amount.amount;
                            eval_state._current_state->store_balance_entry(new_balance_entry);
                            scope.execute_contract_api_by_address(contract_id.AddressToString(AddressType::contract_address).c_str(), CON_ON_DEPOSIT_INTERFACE, transfer_str.c_str(), nullptr);//to do:��lua��������
                            eval_state._current_state->store_balance_entry(balance_entry);
                            
                            if (ENABLELVM) {
                                //TODO
                                // this part neeed to tune while debuging
                                // some code need to be added/del
                                TransferTask _transfertask;
                                _transfertask.num_limit = limit;
                                _transfertask.statevalue = reinterpret_cast<intptr_t>(&eval_state);
                                _transfertask.str_caller = ((string)(this->from)).c_str();
                                _transfertask.str_caller_address = ((string)(Address(this->from))).c_str();
                                _transfertask.str_contract_address = this->contract_id.AddressToString(AddressType::contract_address).c_str();
                                _transfertask.str_args = transfer_str.c_str ();
                                thinkyoung::blockchain::ChainInterface* cur_state = eval_state._current_state;
                                oContractEntry entry = cur_state->get_contract_entry(_transfertask.str_contract_address);
                                
                                if (entry.valid()) {
                                    string address_str = entry->id.AddressToString(AddressType::contract_address);
                                    _transfertask.str_contract_id = address_str.c_str();
                                    //*address_size = address_str.length();
                                    //strncpy(address, address_str.c_str(), CONTRACT_ID_MAX_LENGTH - 1);
                                    //address[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
                                }
                                
                                blockchain::oContractEntry entry1 = cur_state->get_contract_entry(thinkyoung::blockchain::Address(std::string(this->contract_id), AddressType::contract_address));
                                
                                if (!entry1.valid()) {}
                                
                                Code& _code = entry1->code;
                                _transfertask.contract_code = _code;
                            }
                            
                            if (scope.L()->force_stopping == true && scope.L()->exit_code == LUA_API_INTERNAL_ERROR)
                                FC_CAPTURE_AND_THROW(lua_executor_internal_error, (""));
                                
                            exception_code = lua::lib::get_lua_state_value(scope.L(), "exception_code").int_value;
                            
                            if (exception_code > 0) {
                                exception_msg = (char*)lua::lib::get_lua_state_value(scope.L(), "exception_msg").string_value;
                                
                                if (exception_code == THINKYOUNG_API_LVM_LIMIT_OVER_ERROR)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                    
                                else {
                                    thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
                                    throw con_err;
                                }
                            }
                            
                            ShareType exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count()).amount;
                            eval_state.exec_cost = Asset(exec_cost, 0);
                            FC_ASSERT(exec_cost <= costlimit.amount&&exec_cost > 0, "costs of execution can be only between 0 and costlimit");
                            ShareType required = get_amount_sum(exec_cost, transfer_amount.amount);
                            required = get_amount_sum(required, transaction_fee.amount);
                            
                            if (!eval_state.evaluate_contract_testing) {
                                map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, required, withdraw_map);
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                eval_state.p_result_trx.operations.push_back(deposit_contract_op);
                            }
                            
                        } catch (contract_run_out_of_money& e) {
                            if (!eval_state.evaluate_contract_testing) {
                                if (eval_state.throw_exec_exception)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_money);
                                    
                                ShareType required = get_amount_sum(costlimit.amount, transaction_fee.amount);
                                map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, required, withdraw_map);
                                eval_state.p_result_trx.operations.resize(0);
                                eval_state.p_result_trx.push_transaction(eval_state.trx);
                                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                
                            } else
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_run_out_of_testing_money);
                                
                        } catch (contract_error&e) {
                            if (!eval_state.evaluate_contract_testing) {
                                if (eval_state.throw_exec_exception)
                                    FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error, (exception_msg));
                                    
                                Asset exec_cost = eval_state._current_state->get_amount(scope.get_instructions_executed_count());
                                std::map<BalanceIdType, ShareType> withdraw_map;
                                withdraw_enough_balances(balances, (exec_cost + transaction_fee).amount, withdraw_map);
                                eval_state.p_result_trx.operations.resize(1);
                                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
                                eval_state.p_result_trx.operations.push_back(BalancesWithdrawOperation(withdraw_map));
                                
                            } else
                                FC_CAPTURE_AND_THROW(thinkyoung::blockchain::contract_execute_error_in_testing, (exception_msg));
                        }
                    }
                }
                
                eval_state.p_result_trx.expiration = eval_state.trx.expiration;
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
    }
}