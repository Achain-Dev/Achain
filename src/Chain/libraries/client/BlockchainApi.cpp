#include <blockchain/Exceptions.hpp>
#include <blockchain/AccountOperations.hpp>
#include <blockchain/BalanceOperations.hpp>
#include <blockchain/Time.hpp>
#include <client/Client.hpp>
#include <client/ClientImpl.hpp>

#include <fc/thread/non_preemptable_scope_check.hpp>

namespace thinkyoung {
    namespace client {
        namespace detail {
        
            vector<AccountEntry> ClientImpl::blockchain_list_active_delegates(uint32_t first, uint32_t count)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                if (first > 0) --first;
                
                FC_ASSERT(first < ALP_BLOCKCHAIN_NUM_DELEGATES, "Only the first 99 delegates are active");
                FC_ASSERT(first + count <= ALP_BLOCKCHAIN_NUM_DELEGATES, "Only the first 99 delegates are active");
                vector<AccountIdType> all_delegate_ids = _chain_db->get_active_delegates();
                FC_ASSERT(all_delegate_ids.size() == ALP_BLOCKCHAIN_NUM_DELEGATES, "The num of active delegates should equal to ALP_BLOCKCHAIN_NUM_DELEGATES");
                vector<AccountIdType> delegate_ids(all_delegate_ids.begin() + first, all_delegate_ids.begin() + first + count);
                vector<AccountEntry> delegate_entrys;
                delegate_entrys.reserve(count);
                
                for (const auto& delegate_id : delegate_ids) {
                    auto delegate_entry = _chain_db->get_account_entry(delegate_id);
                    FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate());
                    delegate_entrys.push_back(*delegate_entry);
                }
                
                return delegate_entrys;
            }
            
            vector<AccountEntry> ClientImpl::blockchain_list_delegates(uint32_t first, uint32_t count)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                if (first > 0) --first;
                
                vector<AccountIdType> delegate_ids = _chain_db->get_delegates_by_vote(first, count);
                vector<AccountEntry> delegate_entrys;
                delegate_entrys.reserve(delegate_ids.size());
                
                for (const auto& delegate_id : delegate_ids) {
                    auto delegate_entry = _chain_db->get_account_entry(delegate_id);
                    FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate(), "There is a invalid delegate entry in result returned by get_delegates_by_vote");
                    delegate_entrys.push_back(*delegate_entry);
                }
                
                return delegate_entrys;
            }
            
            vector<string> ClientImpl::blockchain_list_missing_block_delegates(uint32_t block_num) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                FC_ASSERT(_chain_db->get_statistics_enabled(), "blockchain_list_missing_block_delegates is only avaliable in statistics mode");
                
                if (block_num == 0 || block_num == 1)
                    return vector<string>();
                    
                vector<string> delegates;
                const auto this_block = _chain_db->get_block_header(block_num);
                const auto prev_block = _chain_db->get_block_header(block_num - 1);
                auto timestamp = prev_block.timestamp;
                timestamp += ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                
                while (timestamp != this_block.timestamp)
                    // if time interval between prev_block and this block greater than ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC,
                    // it shows maybe something wrong happened and delegate did not product block.
                {
                    const auto slot_entry = _chain_db->get_slot_entry(timestamp);
                    FC_ASSERT(slot_entry.valid(), "Invalid slot entry");
                    const auto delegate_entry = _chain_db->get_account_entry(slot_entry->index.delegate_id);
                    FC_ASSERT(delegate_entry.valid(), "No such account entry with the account id in slot entry");
                    delegates.push_back(delegate_entry->name);
                    timestamp += ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                }
                
                return delegates;
            }
            vector<AlpTrxidBalance> ClientImpl::blockchain_get_alp_account_balance_entry(const uint32_t block_num) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                uint32_t last_scan_num = _wallet->get_last_scanned_block_number_for_alp();
                auto results = _chain_db->fetch_alp_full_entry(block_num, last_scan_num);
                _wallet->start_scan(block_num, -1);
                return results;
            }
            vector<BlockEntry> ClientImpl::blockchain_list_blocks(const uint32_t max_block_num, const uint32_t limit) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    FC_ASSERT(max_block_num > 0, "Max_block_num should bigger than 0");
                    FC_ASSERT(limit > 0, "Limit should bigger than 0");
                    const uint32_t head_block_num = _chain_db->get_head_block_num();
                    uint32_t start_block_num = head_block_num;
                    
                    if (max_block_num <= head_block_num) {
                        start_block_num = max_block_num;
                        
                    } else {
                        //FC_ASSERT( -max_block_num <= head_block_num );
                        start_block_num = head_block_num + max_block_num + 1;
                    }
                    
                    const uint32_t count = std::min(limit, start_block_num);
                    vector<BlockEntry> entrys;
                    entrys.reserve(count);
                    
                    for (uint32_t block_num = start_block_num; block_num > 0; --block_num) {
                        oBlockEntry entry = _chain_db->get_block_entry(block_num);
                        
                        if (entry.valid()) entrys.push_back(std::move(*entry));
                        
                        if (entrys.size() >= count) break;
                    }
                    
                    return entrys;
                }
                
                FC_CAPTURE_AND_RETHROW((max_block_num)(limit))
            }
            
            std::vector<std::pair<thinkyoung::blockchain::TransactionIdType, thinkyoung::blockchain::SignedTransaction>>  ClientImpl::blockchain_list_pending_transactions() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                std::vector<std::pair<thinkyoung::blockchain::TransactionIdType, thinkyoung::blockchain::SignedTransaction>> trxs;
                vector<TransactionEvaluationStatePtr> pending = _chain_db->get_pending_transactions();
                trxs.reserve(pending.size());
                
                for (const auto& trx_eval_ptr : pending) {
                    trxs.push_back(std::make_pair(trx_eval_ptr->trx.id(), trx_eval_ptr->trx));
                }
                
                return trxs;
            }
            
            
            uint32_t detail::ClientImpl::blockchain_get_block_count() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _chain_db->get_head_block_num();
            }
            
            oAccountEntry detail::ClientImpl::blockchain_get_account(const string& account)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                    
                    // if all of the characters are digit
                    if (std::all_of(account.begin(), account.end(), ::isdigit))
                        return _chain_db->get_account_entry(std::stoi(account));
                        
                    else if (account.substr(0, string(ALP_ADDRESS_PREFIX).size()) == ALP_ADDRESS_PREFIX) {
                        //Magic number 39 is hopefully longer than the longest address and shorter than the shortest key. Hopefully.
                        if (account.length() < 39)
                            return _chain_db->get_account_entry(Address(account));
                            
                        else
                            return _chain_db->get_account_entry(Address(blockchain::PublicKeyType(account)));
                            
                    } else
                        return _chain_db->get_account_entry(account);
                        
                } catch (...) {
                }
                
                return oAccountEntry();
            }
            
            map<AccountIdType, string> detail::ClientImpl::blockchain_get_slate(const string& slate)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                map<AccountIdType, string> delegates;
                SlateIdType slate_id = 0;
                
                if (!std::all_of(slate.begin(), slate.end(), ::isdigit)) {
                    // get account entry by account name
                    const oAccountEntry account_entry = _chain_db->get_account_entry(slate);
                    FC_ASSERT(account_entry.valid(), "The slate is not a valid account");
                    
                    try {
                        FC_ASSERT(account_entry->public_data.is_object());
                        const auto public_data = account_entry->public_data.get_object();
                        FC_ASSERT(public_data.contains("slate_id"));
                        FC_ASSERT(public_data["slate_id"].is_uint64(), "Slate id shuld be uint64");
                        slate_id = public_data["slate_id"].as<SlateIdType>();
                        
                    } catch (const fc::exception&) {
                        return delegates;
                    }
                    
                } else {
                    auto slate_str = fc::variant(slate);
                    slate_id = slate_str.as<SlateIdType>();
                }
                
                const oSlateEntry slate_entry = _chain_db->get_slate_entry(slate_id);
                FC_ASSERT(slate_entry.valid(), "No such slate entry with");
                
                for (const AccountIdType id : slate_entry->slate) {
                    const oAccountEntry delegate_entry = _chain_db->get_account_entry(id);
                    
                    if (delegate_entry.valid()) {
                        if (delegate_entry->is_delegate()) delegates[id] = delegate_entry->name;
                        
                        else delegates[id] = '(' + delegate_entry->name + ')';
                        
                    } else {
                        delegates[id] = std::to_string(id);
                    }
                }
                
                return delegates;
            }
            
            BalanceEntry detail::ClientImpl::blockchain_get_balance(const BalanceIdType& balance_id)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                const auto balance_entry = _chain_db->get_balance_entry(balance_id);
                FC_ASSERT(balance_entry.valid(), "No such balance entry with this slate id");
                return *balance_entry;
            }
            
            oAssetEntry detail::ClientImpl::blockchain_get_asset(const string& asset)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                    
                    if (!std::all_of(asset.begin(), asset.end(), ::isdigit))
                        return _chain_db->get_asset_entry(asset);
                        
                    else
                        return _chain_db->get_asset_entry(std::stoi(asset));
                        
                } catch (...) {
                }
                
                return oAssetEntry();
            }
            
            
            pair<TransactionIdType, TransactionEntry> detail::ClientImpl::blockchain_get_transaction(const string& transaction_id_prefix,
                    bool exact)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto id_prefix = variant(transaction_id_prefix).as<TransactionIdType>();
                    const oTransactionEntry entry = _chain_db->get_transaction(id_prefix, exact);
                    FC_ASSERT(entry.valid(), "Transaction not found!");
                    return std::make_pair(entry->trx.id(), *entry);
                }
                
                FC_CAPTURE_AND_RETHROW((transaction_id_prefix)(exact))
            }
            
            
            PrettyTransaction detail::ClientImpl::blockchain_get_pretty_transaction(const string& transaction_id_prefix,
                    bool exact)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto id_prefix = variant(transaction_id_prefix).as<TransactionIdType>();
                    const oTransactionEntry entry = _chain_db->get_transaction(id_prefix, exact);
                    FC_ASSERT(entry.valid(), "Transaction not found!");
                    return _wallet->to_pretty_trx(*entry, "");
                }
                
                FC_CAPTURE_AND_RETHROW((transaction_id_prefix)(exact))
            }
            
            PrettyContractTransaction detail::ClientImpl::blockchain_get_pretty_contract_transaction(const string& transaction_id_prefix,
                    bool exact)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto id_prefix = variant(transaction_id_prefix).as<TransactionIdType>();
                    const oTransactionEntry entry = _chain_db->get_transaction(id_prefix, exact);
                    FC_ASSERT(entry.valid(), "Transaction not found!");
                    return _wallet->to_pretty_contract_trx(*entry);
                }
                
                FC_CAPTURE_AND_RETHROW((transaction_id_prefix)(exact))
            }
            
            thinkyoung::blockchain::ContractTrxInfo detail::ClientImpl::blockchain_get_contract_result(const std::string& result_id) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    //
                    const auto id = variant(result_id).as<TransactionIdType>();
                    const oResultTIdEntry  result = _chain_db->get_transaction_from_result(id);
                    FC_ASSERT(result.valid(), "Result not found!");
                    auto res = result->res;
                    oTransactionEntry trx = _chain_db->get_transaction(res, true);
                    FC_ASSERT(trx.valid(), "transaction not found!");
                    const uint32_t blocknum = trx->chain_location.block_num;
                    thinkyoung::blockchain::ContractTrxInfo contract_result;
                    contract_result.block_num = blocknum;
                    contract_result.trx_id = res;
                    return contract_result;
                }
                
                FC_CAPTURE_AND_RETHROW((result_id))
            }
            
            oBlockEntry detail::ClientImpl::blockchain_get_block(const string& block)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    try {
                        ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                        
                        if (block.size() == string(BlockIdType()).size())
                            return _chain_db->get_block_entry(BlockIdType(block));
                            
                        else
                            return _chain_db->get_block_entry(std::stoi(block));
                            
                    } catch (...) {
                    }
                    
                    return oBlockEntry();
                }
                
                FC_CAPTURE_AND_RETHROW((block))
            }
            
            unordered_map<BalanceIdType, BalanceEntry> detail::ClientImpl::blockchain_list_balances(const string& first, uint32_t limit)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    FC_ASSERT(limit > 0, "Limit should be bigger than 0");
                    BalanceIdType id;
                    
                    if (!first.empty()) id = variant(first).as<BalanceIdType>();
                    
                    return _chain_db->get_balances(id, limit);
                }
                
                FC_CAPTURE_AND_RETHROW((first)(limit))
            }
            
            AccountBalanceSummaryType detail::ClientImpl::blockchain_get_account_public_balance(const string& account_name) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto& acct = _wallet->get_account(account_name);
                    map<AssetIdType, ShareType> balances;
                    
                    for (const auto& pair : _chain_db->get_balances_for_key(acct.active_key()))
                        balances[pair.second.asset_id()] += pair.second.balance;
                        
                    AccountBalanceSummaryType ret;
                    ret[account_name] = balances;
                    return ret;
                }
                
                FC_CAPTURE_AND_RETHROW((account_name))
            }
            
            unordered_map<BalanceIdType, BalanceEntry> detail::ClientImpl::blockchain_list_address_balances(const string& raw_addr,
                    const time_point& after)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    Address addr;
                    
                    try {
                        addr = Address(raw_addr);
                        
                    } catch (const fc::exception&) {
                        addr = Address(PtsAddress(raw_addr));
                    }
                    
                    auto result = _chain_db->get_balances_for_address(addr);
                    
                    for (auto itr = result.begin(); itr != result.end();) {
                        if (fc::time_point(itr->second.last_update) <= after)
                            itr = result.erase(itr);
                            
                        else
                            ++itr;
                    }
                    
                    return result;
                }
                
                FC_CAPTURE_AND_RETHROW((raw_addr)(after))
            }
            
            fc::variant_object detail::ClientImpl::blockchain_list_address_transactions(const string& raw_addr, uint32_t after_block)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    fc::mutable_variant_object results;
                    Address addr;
                    
                    try {
                        addr = Address(raw_addr);
                        
                    } catch (...) {
                        addr = Address(PtsAddress(raw_addr));
                    }
                    
                    auto transactions = _chain_db->fetch_address_transactions(addr);
                    ilog("Found ${num} transactions for ${addr}", ("num", transactions.size())("addr", raw_addr));
                    
                    if (after_block > 0)
                        transactions.erase(std::remove_if(transactions.begin(), transactions.end(),
                        [after_block](const blockchain::TransactionEntry& t) -> bool {
                        return t.chain_location.block_num <= after_block;
                    }), transactions.end());
                    ilog("Found ${num} transactions after block ${after_block}", ("num", transactions.size())("after_block", after_block));
                    
                    for (const auto& trx : transactions) {
                        fc::mutable_variant_object bundle("timestamp", _chain_db->get_block(trx.chain_location.block_num).timestamp);
                        bundle["trx"] = trx;
                        results[string(trx.trx.id())] = bundle;
                    }
                    
                    return results;
                }
                
                FC_CAPTURE_AND_RETHROW((raw_addr)(after_block))
            }
            
            unordered_map<BalanceIdType, BalanceEntry> detail::ClientImpl::blockchain_list_key_balances(const PublicKeyType& key)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    return _chain_db->get_balances_for_key(key);
                }
                
                FC_CAPTURE_AND_RETHROW((key))
            }
            std::vector<fc::string> ClientImpl::blockchain_list_pub_all_address(const std::string& pub_key) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                std::vector<fc::string> result;
                
                try {
                    if (fc::string::npos != pub_key.find(ALP_ADDRESS_PREFIX)) {
                        auto pub_type = thinkyoung::blockchain::PublicKeyType(pub_key);
                        result.push_back(fc::string(Address(pub_type)));
                        //result.push_back(fc::string(Address(PtsAddress(fc::ecc::public_key(pub_type)))));
                        result.push_back(fc::string(Address(PtsAddress(pub_type.operator fc::ecc::public_key()))));
                        
                    } else {
                        throw "invalid pub key";
                    }
                    
                } catch (fc::exception& e) {
                    result.clear();
                    return result;
                }
                
                return result;
            }
            
            vector<AccountEntry> detail::ClientImpl::blockchain_list_accounts(const string& first, uint32_t limit)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    FC_ASSERT(limit > 0, "Limit should bigger than 0");
                    return _chain_db->get_accounts(first, limit);
                }
                
                FC_CAPTURE_AND_RETHROW((first)(limit))
            }
            
            vector<AccountEntry> detail::ClientImpl::blockchain_list_recently_updated_accounts()const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                FC_ASSERT(_chain_db->get_statistics_enabled(), "Blockchain_list_recently_updated_accounts is only avaliable in statistics mode");
                vector<Operation> account_updates = _chain_db->get_recent_operations(update_account_op_type);
                vector<AccountEntry> accounts;
                accounts.reserve(account_updates.size());
                
                for (const Operation& op : account_updates) {
                    auto oaccount = _chain_db->get_account_entry(op.as<UpdateAccountOperation>().account_id);
                    
                    if (oaccount)
                        accounts.push_back(*oaccount);
                }
                
                return accounts;
            }
            
            vector<AccountEntry> detail::ClientImpl::blockchain_list_recently_registered_accounts()const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                FC_ASSERT(_chain_db->get_statistics_enabled(), "Blockchain_list_recently_registered_accounts is only avaliable in statistics mode");
                vector<Operation> account_registrations = _chain_db->get_recent_operations(register_account_op_type);
                vector<AccountEntry> accounts;
                accounts.reserve(account_registrations.size());
                
                for (const Operation& op : account_registrations) {
                    auto oaccount = _chain_db->get_account_entry(op.as<RegisterAccountOperation>().owner_key);
                    
                    if (oaccount)
                        accounts.push_back(*oaccount);
                }
                
                return accounts;
            }
            
            vector<AssetEntry> detail::ClientImpl::blockchain_list_assets(const string& first, uint32_t limit)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                FC_ASSERT(limit > 0, "Limit should bigger than 0");
                return _chain_db->get_assets(first, limit);
            }
            
            
            variant_object ClientImpl::blockchain_get_info()const {
                auto info = fc::mutable_variant_object();
                info["blockchain_id"] = _chain_db->get_chain_id();
                info["name"] = ALP_BLOCKCHAIN_NAME;
                info["symbol"] = ALP_BLOCKCHAIN_SYMBOL;
                info["address_prefix"] = ALP_ADDRESS_PREFIX;
                info["db_version"] = ALP_BLOCKCHAIN_DATABASE_VERSION;
                info["genesis_timestamp"] = _chain_db->get_genesis_timestamp();
                info["block_interval"] = ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                info["delegate_num"] = ALP_BLOCKCHAIN_NUM_DELEGATES;
                info["max_delegate_pay_issued_per_block"] = _chain_db->get_product_reword_fee();
                info["max_delegate_reg_fee"] = _chain_db->get_delegate_registration_fee(100);
                info["name_size_max"] = ALP_BLOCKCHAIN_MAX_NAME_SIZE;
                info["memo_size_max"] = ALP_BLOCKCHAIN_MAX_MESSAGE_SIZE;
                info["data_size_max"] = ALP_BLOCKCHAIN_MAX_NAME_DATA_SIZE;
                info["symbol_size_max"] = ALP_BLOCKCHAIN_MAX_SUB_SYMBOL_SIZE;
                info["symbol_size_min"] = ALP_BLOCKCHAIN_MIN_SYMBOL_SIZE;
                info["asset_shares_max"] = ALP_BLOCKCHAIN_MAX_SHARES;
                info["short_symbol_asset_reg_fee"] = _chain_db->get_asset_registration_fee(ALP_BLOCKCHAIN_MIN_SYMBOL_SIZE);
                info["long_symbol_asset_reg_fee"] = _chain_db->get_asset_registration_fee(ALP_BLOCKCHAIN_MAX_SUB_SYMBOL_SIZE);
                info["statistics_enabled"] = _chain_db->get_statistics_enabled();
                info["relay_fee"] = _chain_db->get_relay_fee();
                info["max_pending_queue_size"] = ALP_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE;
                info["max_trx_per_second"] = ALP_BLOCKCHAIN_MAX_TRX_PER_SECOND;
                return info;
            }
            
            void ClientImpl::blockchain_generate_snapshot(const string& filename)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _chain_db->generate_snapshot(fc::path(filename));
                }
                
                FC_CAPTURE_AND_RETHROW((filename))
            }
            
            void ClientImpl::blockchain_generate_issuance_map(const string& symbol, const string& filename)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _chain_db->generate_issuance_map(symbol, fc::path(filename));
                }
                
                FC_CAPTURE_AND_RETHROW((filename))
            }
            
            
            Asset ClientImpl::blockchain_calculate_supply(const string& asset)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                AssetIdType asset_id;
                
                if (std::all_of(asset.begin(), asset.end(), ::isdigit))
                    asset_id = std::stoi(asset);
                    
                else
                    asset_id = _chain_db->get_asset_id(asset);
                    
                return _chain_db->calculate_supply(asset_id);
            }
            void ClientImpl::blockchain_btc_address_convert(const std::string& path) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                std::vector<string> snapshot;
                char c[256];
                FILE * fp;
#ifdef WIN32
                fopen_s(&fp, path.c_str(), "r");
#else
                fp = fopen(path.c_str(), "r");
#endif
                
                if (fp != NULL) {
                    while (true) {
                        ::memset(c, 0, 256);
                        
                        if (fgets(c, 256, fp) != NULL) {
                            std::string s = c;
                            size_t idepance = s.find(',');
                            std::string pow_address = s.substr(0, idepance);
                            std::string balance = s.substr(idepance, s.size() - idepance - 4);
                            std::string allstring = (std::string)thinkyoung::blockchain::Address(thinkyoung::blockchain::PtsAddress(pow_address));
                            allstring += balance;
                            snapshot.push_back(allstring);
                            
                        } else {
                            break;
                        }
                    }
                    
                    fclose(fp);
                    fp = NULL;
                }
                
#ifdef WIN32
                fopen_s(&fp, path.c_str(), "r");
#else
                fp = fopen(path.c_str(), "r");
#endif
                
                if (fp != NULL) {
                    std::vector<std::string >::iterator iter = snapshot.begin();
                    
                    for (; iter != snapshot.end(); ++iter) {
                        fputs(iter->c_str(), fp);
                        fputs("\r\n", fp);
                    }
                    
                    fclose(fp);
                    fp = NULL;
                }
            }
            thinkyoung::blockchain::BlockchainSecurityState ClientImpl::blockchain_get_security_state()const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                BlockchainSecurityState state;
                int64_t required_confirmations = _chain_db->get_required_confirmations();
                double participation_rate = _chain_db->get_average_delegate_participation();
                
                if (participation_rate > 100) participation_rate = 100;
                
                state.estimated_confirmation_seconds = (uint32_t)(required_confirmations * ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
                state.participation_rate = participation_rate;
                
                if (!blockchain_is_synced()) {
                    state.alert_level = thinkyoung::blockchain::BlockchainSecurityState::grey;
                    
                } else if (required_confirmations <= ALP_BLOCKCHAIN_NUM_DELEGATES / 2
                           && participation_rate > 80) {
                    state.alert_level = thinkyoung::blockchain::BlockchainSecurityState::green;
                    
                } else if (required_confirmations > ALP_BLOCKCHAIN_NUM_DELEGATES
                           || participation_rate < 60) {
                    state.alert_level = thinkyoung::blockchain::BlockchainSecurityState::red;
                    
                } else {
                    state.alert_level = thinkyoung::blockchain::BlockchainSecurityState::yellow;
                }
                
                return state;
            }
            
            bool ClientImpl::blockchain_is_synced() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return (blockchain::now() - _chain_db->get_head_block().timestamp) < fc::seconds(ALP_BLOCKCHAIN_NUM_DELEGATES * ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
            }
            
            
            
            map<TransactionIdType, TransactionEntry> ClientImpl::blockchain_get_block_transactions(const string& block)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                vector<TransactionEntry> transactions;
                
                if (block.size() == 40)
                    transactions = _chain_db->get_transactions_for_block(BlockIdType(block));
                    
                else
                    transactions = _chain_db->get_transactions_for_block(_chain_db->get_block_id(std::stoi(block)));
                    
                map<TransactionIdType, TransactionEntry> transactions_map;
                
                for (const auto& transaction : transactions)
                    transactions_map[transaction.trx.id()] = transaction;
                    
                return transactions_map;
            }
            
            std::string ClientImpl::blockchain_export_fork_graph(uint32_t start_block, uint32_t end_block, const thinkyoung::blockchain::FilePath& filename)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _chain_db->export_fork_graph(start_block, end_block, filename);
            }
            
            std::map<uint32_t, vector<ForkEntry>> ClientImpl::blockchain_list_forks()const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _chain_db->get_forks_list();
            }
            
            vector<SlotEntry> ClientImpl::blockchain_get_delegate_slot_entrys(const string& delegate_name, uint32_t limit)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    FC_ASSERT(limit > 0, "Limit should be bigger than 0");
                    const oAccountEntry delegate_entry = _chain_db->get_account_entry(delegate_name);
                    FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate(), "${n} is not a delegate!", ("n", delegate_name));
                    return _chain_db->get_delegate_slot_entrys(delegate_entry->id, limit);
                }
                
                FC_CAPTURE_AND_RETHROW((delegate_name)(limit))
            }
            
            string ClientImpl::blockchain_get_block_signee(const string& block)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                if (block.size() == 40)
                    return _chain_db->get_block_signee(BlockIdType(block)).name;
                    
                else
                    return _chain_db->get_block_signee(std::stoi(block)).name;
            }
            
            bool ClientImpl::blockchain_verify_signature(const string& signer, const fc::sha256& hash, const fc::ecc::compact_signature& signature) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto key = PublicKeyType(signer);
                    return key == fc::ecc::public_key(signature, hash);
                    
                } catch (...) {
                    try {
                        auto addr = Address(signer);
                        return addr == Address(fc::ecc::public_key(signature, hash));
                        
                    } catch (...) {
                        oAccountEntry rec = blockchain_get_account(signer);
                        
                        if (!rec.valid())
                            return false;
                            
                        // logic shamelessly copy-pasted from signed_block_header::validate_signee()
                        // NB LHS of == operator is thinkyoung::blockchain::public_key_type and RHS is fc::ecc::public_key,
                        //   the opposite order won't compile (look at operator== prototype in public_key_type class declaration)
                        return rec->active_key() == fc::ecc::public_key(signature, hash);
                    }
                }
            }
            
            void ClientImpl::blockchain_dump_state(const string& path, const std::string& ldbname )const {


                    _p2p_node->dump_peerdb_state(fc::path(path), ldbname);

                    if ("blacklist.leveldb" == ldbname ||
                        "peers.leveldb" == ldbname)
                    {
                        return;
                    }
                    // set limit in  sandbox state
                    if (_chain_db->get_is_in_sandbox())
                        FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                  
                    _chain_db->dump_state(fc::path(path), ldbname);
                    return;

            }
            
            
            thinkyoung::blockchain::Asset ClientImpl::blockchain_unclaimed_genesis() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _chain_db->unclaimed_genesis();
            }
            
            
            
            void ClientImpl::blockchain_broadcast_transaction(const SignedTransaction& trx) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto collector = _chain_db->get_account_entry(_config.relay_account_name);
                auto accept_fee = [this](const Asset& fee) -> bool {
                    //auto feed_price = _chain_db->get_active_feed_price(fee.asset_id);//check????
                    //if( !feed_price )
                    return false;
                    
                    //Forgive up to a 5% change in price from the last sync by the lightwallet
                    // asset required = asset(_config.light_relay_fee * .95) * *feed_price;
                    //return fee >= required;
                };
                
                if (collector && _config.light_relay_fee) {
                    for (Operation op : trx.operations)
                        if (op.type == deposit_op_type) {
                            DepositOperation deposit = op.as<DepositOperation>();
                            ilog("Checking if deposit ${d} is to ${c}", ("d", deposit)("c", collector->owner_address()));
                            
                            if (deposit.condition.owner() && *deposit.condition.owner() == collector->owner_address() &&
                                    ((deposit.condition.asset_id == 0 && deposit.amount >= _config.light_relay_fee) ||
                                     accept_fee(Asset(deposit.amount, deposit.condition.asset_id)))) {
                                network_broadcast_transaction(trx);
                                return;
                            }
                        }
                        
                    FC_THROW("Do I look like some kind of charity? You want your transactions sent, you pay like everyone else!");
                }
                
                network_broadcast_transaction(trx);
            }
            
            
            
            std::string detail::ClientImpl::blockchain_get_transaction_rpc(const string& transaction_id_prefix,
                    bool exact)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto id_prefix = variant(transaction_id_prefix).as<TransactionIdType>();
                    const oTransactionEntry record = _chain_db->get_transaction(id_prefix, exact);
                    FC_ASSERT(record.valid(), "Transaction not found!");
                    std::string result = "{\"result\":\"SUCCESS\",\"message\":\"\"}";
                    return result;
                    
                } catch (fc::exception er) {
                    std::string result = "{\"result\":\"ERROR\",\"message\":\"" + er.to_string() + "\"}";
                    return result;
                }
            }
            void ClientImpl::blockchain_set_node_vm_enabled(bool enabled) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _chain_db->set_node_vm_enabled(enabled);
            }
            
            bool ClientImpl::blockchain_get_node_vm_enabled() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _chain_db->get_node_vm_enabled();
            }
            
            
            vector<string>  ClientImpl::blockchain_get_all_contracts() const {
                vector<ContractIdType> tmp = _chain_db->get_all_contract_entries();
                vector<string> result;
                
                for (auto iter = tmp.begin(); iter != tmp.end(); ++iter) {
                    result.push_back( iter->AddressToString(AddressType::contract_address));
                }
                
                return result;
            }
            
            unordered_map<string, string> ClientImpl::blockchain_get_forever_contracts() const {
                vector<string> tmp = blockchain_get_all_contracts();
                unordered_map<string, string> result;
                
                for (auto iter = tmp.begin(); iter != tmp.end(); ++iter) {
                    auto entry = _chain_db->get_contract_entry(Address(*iter, AddressType::contract_address));
                    
                    if (entry->level == ContractLevel::forever)
                        result[*iter] = entry->contract_name;
                }
                
                return result;
            }
            
            vector<EventOperation> ClientImpl::blockchain_get_events(uint32_t block_index, const thinkyoung::blockchain::TransactionIdType& trx_id) const {
                vector<EventOperation> ops;
                
                if (_chain_db) {
                    ops = _chain_db->get_events(block_index, trx_id);
                }
                
                return ops;
            }
            thinkyoung::blockchain::TransactionIdType ClientImpl::blockchain_get_transaction_id(const thinkyoung::blockchain::SignedTransaction& transaction_to_broadcast) {
                return transaction_to_broadcast.id();
            }
        }
    }
} // namespace thinkyoung::client::detail
