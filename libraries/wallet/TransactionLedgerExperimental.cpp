#include <blockchain/Time.hpp>
#include <wallet/Exceptions.hpp>
#include <wallet/Wallet.hpp>
#include <wallet/WalletImpl.hpp>
#include <boost/lexical_cast.hpp>
#include <fc/io/json.hpp> // TODO: Temporary

namespace thinkyoung {
    namespace wallet {

        void detail::WalletImpl::scan_balances_experimental()
        {
            try {
                const auto pending_state = _blockchain->get_pending_state();

                const auto scan_balance = [&](const BalanceEntry& entry)
                {
                    const BalanceIdType id = entry.id();
                    const oBalanceEntry pending_entry = pending_state->get_balance_entry(id);
                    if (!pending_entry.valid()) return;

                    const set<Address>& owners = pending_entry->owners();
                    for (const Address& owner : owners)
                    {
                        const oWalletKeyEntry key_entry = _wallet_db.lookup_key(owner);
                        if (!key_entry.valid() || !key_entry->has_private_key()) continue;

                        _balance_entrys[id] = std::move(*pending_entry);
                        break;
                    }
                };

                _balance_entrys.clear();
                _blockchain->scan_balances(scan_balance);
                _dirty_balances = false;
            } FC_CAPTURE_AND_RETHROW()
        }

		void detail::WalletImpl::scan_contracts()
		{
			try
			{
				const auto pending_state = _blockchain->get_pending_state();

				const auto scan_contracts = [&](const ContractEntry& contract_entry)
				{
					auto contract_id = contract_entry.id;

					//check whether the owner of the contract is in wallet account list
					const oWalletKeyEntry key_entry = _wallet_db.lookup_key(Address(contract_entry.owner));
					if (key_entry.valid() && key_entry->has_private_key())
					{
						_contract_entrys[contract_id] = contract_entry;
						//store walletDb
						_wallet_db.store_contract(contract_entry);
					}
						
		
				};

				_contract_entrys.clear();
				_blockchain->scan_contracts(scan_contracts);
				_dirty_contracts = false;

			}FC_CAPTURE_AND_RETHROW()

		}

        void detail::WalletImpl::scan_accounts()
        {
            try {
                const auto check_address = [&](const Address& addr) -> bool
                {
                    if (_wallet_db.lookup_account(addr).valid()) return true;
                    const oWalletKeyEntry& key_entry = _wallet_db.lookup_key(addr);
                    return key_entry.valid() && key_entry->has_private_key();
                };

                const auto rename_repeat_local_account = [&](const AccountEntry& blockchain_entry) -> bool
                {
                    auto local_entry = _wallet_db.lookup_account(blockchain_entry.name);
                    if (local_entry.valid())
                    {
                        auto local_wallet_entrys = _wallet_db.get_accounts();
                        for (auto temp_local : local_wallet_entrys)
                        {
                            if (temp_local.second.name != blockchain_entry.name)
                                continue;
                            if (temp_local.second.owner_key != blockchain_entry.owner_key)
                            {
                                for (int i = 0; i < 100; i++)
                                {
                                    try {
                                        int ran = rand() % 1000;
                                        fc::string new_name = temp_local.second.name + "local";
                                        new_name += boost::lexical_cast<string>(ran);
                                        const auto registered_account = _blockchain->get_account_entry(new_name);
                                        if (registered_account.valid())
                                            continue;
                                        const auto new_account = _wallet_db.lookup_account(new_name);
                                        if (new_account.valid())
                                            continue;
                                        self->rename_account(local_entry->name, new_name);
                                        break;
                                    }
                                    catch (...)
                                    {
                                    }
                                }
                            }
                        }
                    }
                    return true;
                };
                const auto scan_account = [&](const AccountEntry& blockchain_entry)
                {
                    rename_repeat_local_account(blockchain_entry);
                    bool store_account = check_address(blockchain_entry.owner_address());
                    if (!store_account) store_account = check_address(blockchain_entry.active_address());
                    if (!store_account && blockchain_entry.is_delegate()) store_account = check_address(blockchain_entry.signing_address());
                    if (!store_account) return;
                    _wallet_db.store_account(blockchain_entry, _wallet_password);
                };

                _blockchain->scan_unordered_accounts(scan_account);

                if (self->is_unlocked())
                {
                    _stealth_private_keys.clear();
                    const auto& account_keys = _wallet_db.get_account_private_keys(_wallet_password);
                    _stealth_private_keys.reserve(account_keys.size());
                    for (const auto& item : account_keys)
                        _stealth_private_keys.push_back(item.first);
                }

                _dirty_accounts = false;
            } FC_CAPTURE_AND_RETHROW()
        }

        void detail::WalletImpl::scan_block_experimental(uint32_t block_num,
            const map<PrivateKeyType, string>& account_keys,
            const map<Address, string>& account_balances,
            const set<string>& account_names)
        {
            try {
                const SignedBlockHeader block_header = _blockchain->get_block_header(block_num);
                const vector<TransactionEntry> transaction_entrys = _blockchain->get_transactions_for_block(block_header.id());
                for (const TransactionEvaluationState& eval_state : transaction_entrys)
                {
                    try
                    {
                        scan_transaction_experimental(eval_state, block_num, block_header.timestamp,
                            account_keys, account_balances, account_names, true);
                    }
                    catch (const fc::exception& e)
                    {
                        elog("Error scanning transaction ${t}: ${e}", ("t", eval_state)("e", e.to_detail_string()));
                    }
                }
            } FC_CAPTURE_AND_RETHROW((block_num)(account_balances)(account_names))
        }

        // For initiating manual transaction scanning
        TransactionLedgerEntry detail::WalletImpl::scan_transaction_experimental(const TransactionEvaluationState& eval_state,
            uint32_t block_num,
            const time_point_sec timestamp,
            bool overwrite_existing)
        {
            try {
                const map<PrivateKeyType, string> account_keys = _wallet_db.get_account_private_keys(_wallet_password);

                // TODO: Move this into a separate function
                map<Address, string> account_balances;
                //const account_balance_entry_summary_type balance_entry_summary = self->get_account_balance_entrys( "", true, -1 );
                const AccountBalanceEntrySummaryType balance_entry_summary;
                for (const auto& balance_item : balance_entry_summary)
                {
                    const string& account_name = balance_item.first;
                    for (const auto& balance_entry : balance_item.second)
                    {
                        const BalanceIdType& balance_id = balance_entry.id();
                        switch (WithdrawConditionTypes(balance_entry.condition.type))
                        {
                        case withdraw_signature_type:
                            if (balance_entry.snapshot_info.valid())
                                account_balances[balance_id] = "GENESIS-" + balance_entry.snapshot_info->original_address;
                            else
                                account_balances[balance_id] = account_name;
                            break;
                        default:
                            account_balances[balance_id] = balance_entry.condition.type_label() + "-" + string(balance_id);
                            break;
                        }
                    }
                }

                set<string> account_names;
                const vector<WalletAccountEntry> accounts = self->list_my_accounts();
                for (const WalletAccountEntry& account : accounts)
                    account_names.insert(account.name);

                return scan_transaction_experimental(eval_state, block_num, timestamp, account_keys,
                    account_balances, account_names, overwrite_existing);
            } FC_CAPTURE_AND_RETHROW((eval_state)(block_num)(timestamp)(overwrite_existing))
        }

        TransactionLedgerEntry detail::WalletImpl::scan_transaction_experimental(const TransactionEvaluationState& eval_state,
            uint32_t block_num,
            const time_point_sec timestamp,
            const map<PrivateKeyType, string>& account_keys,
            const map<Address, string>& account_balances,
            const set<string>& account_names,
            bool overwrite_existing)
        {
            try {
                TransactionLedgerEntry entry;

                const TransactionIdType transaction_id = eval_state.trx.id();
                const TransactionIdType& entry_id = transaction_id;
                const bool existing_entry = _wallet_db.experimental_transactions.count(entry_id) > 0; // TODO
                if (existing_entry)
                    entry = _wallet_db.experimental_transactions.at(entry_id);

                entry.id = entry_id;
                entry.block_num = block_num;
                entry.timestamp = std::min(entry.timestamp, timestamp);
                entry.delta_amounts.clear();
                entry.transaction_id = transaction_id;

                scan_transaction_experimental(eval_state, account_keys, account_balances, account_names, entry,
                    overwrite_existing || !existing_entry);

                return entry;
            } FC_CAPTURE_AND_RETHROW((eval_state)(block_num)(timestamp)(account_balances)(account_names)(overwrite_existing))
        }

        /* This is the master transaction scanning function. The brief layout:
         * - Perform some initialization
         * - Define lambdas for scanning each supported operation
         *   - These return true if the operation is relevant to this wallet
         * - Scan the transaction's operations
         * - Perform some cleanup and entry-keeping
         */
        void detail::WalletImpl::scan_transaction_experimental(const TransactionEvaluationState& eval_state,
            const map<PrivateKeyType, string>& account_keys,
            const map<Address, string>& account_balances,
            const set<string>& account_names,
            TransactionLedgerEntry& entry,
            bool store_entry)
        {
            try {
                map<string, map<AssetIdType, ShareType>> raw_delta_amounts;
                uint16_t op_index = 0;
                uint16_t withdrawal_count = 0;
                vector<MemoStatus> titan_memos;

                // Used by scan_withdraw and scan_deposit below
                const auto collect_balance = [&](const BalanceIdType& balance_id, const Asset& delta_amount) -> bool
                {
                    if (account_balances.count(balance_id) > 0) // First check canonical labels
                    {
                        // TODO: Need to save balance labels locally before emptying them so they can be deleted from the chain
                        // OR keep the owner information around in the eval state and keep track of that somehow
                        const string& delta_label = account_balances.at(balance_id);
                        raw_delta_amounts[delta_label][delta_amount.asset_id] += delta_amount.amount;
                        return true;
                    }
                    else if (entry.delta_labels.count(op_index) > 0) // Next check custom labels
                    {
                        const string& delta_label = entry.delta_labels.at(op_index);
                        raw_delta_amounts[delta_label][delta_amount.asset_id] += delta_amount.amount;
                        return account_names.count(delta_label) > 0;
                    }
                    else // Fallback to using the balance id as the label
                    {
                        // TODO: We should use the owner, not the ID -- also see case 1 above
                        const string delta_label = string(balance_id);
                        raw_delta_amounts[delta_label][delta_amount.asset_id] += delta_amount.amount;
                        return false;
                    }
                };

                const auto scan_withdraw = [&](const WithdrawOperation& op) -> bool
                {
                    ++withdrawal_count;
                    const auto scan_delta = [&](const Asset& delta_amount) -> bool
                    {
                        return collect_balance(op.balance_id, delta_amount);
                    };
                    return eval_state.scan_deltas(op_index, scan_delta);
                };

                // TODO: Recipient address label and memo message needs to be saved at time of creation by sender
                // to do this make a wrapper around trx.deposit_to_account just like my->withdraw_to_transaction
                const auto scan_deposit = [&](const DepositOperation& op) -> bool
                {
                    const BalanceIdType& balance_id = op.balance_id();

                    const auto scan_withdraw_with_signature = [&](const WithdrawWithSignature& condition) -> bool
                    {
                        if (condition.memo.valid())
                        {
                            bool stealth = false;
                            oMessageStatus status;
                            PrivateKeyType recipient_key;

                            try
                            {
                                const PrivateKeyType key = self->get_private_key(condition.owner);
                                status = condition.decrypt_memo_data(key, true);
                                if (status.valid()) recipient_key = key;
                            }
                            catch (const fc::exception&)
                            {
                            }

                            if (!status.valid())
                            {
                                vector<fc::future<void>> decrypt_memo_futures;
                                decrypt_memo_futures.resize(_stealth_private_keys.size());

                                for (uint32_t key_index = 0; key_index < _stealth_private_keys.size(); ++key_index)
                                {
                                    decrypt_memo_futures[key_index] = fc::async([&, key_index]()
                                    {
                                        _scanner_threads[key_index % _num_scanner_threads]->async([&]()
                                        {
                                            if (status.valid()) return;
                                            const PrivateKeyType& key = _stealth_private_keys.at(key_index);
                                            const oMessageStatus inner_status = condition.decrypt_memo_data(key);
                                            if (inner_status.valid())
                                            {
                                                stealth = true;
                                                status = inner_status;
                                                recipient_key = key;
                                            }
                                        }, "decrypt memo").wait();
                                    });
                                }

                                for (auto& decrypt_memo_future : decrypt_memo_futures)
                                {
                                    try
                                    {
                                        decrypt_memo_future.wait();
                                    }
                                    catch (const fc::exception&)
                                    {
                                    }
                                }
                            }

                            // If I've successfully decrypted then it's for me
                            if (status.valid())
                            {
                                _wallet_db.cache_memo(*status, recipient_key, _wallet_password);

                                if (stealth) titan_memos.push_back(*status);

                                const string& delta_label = account_keys.at(recipient_key);
                                entry.delta_labels[op_index] = delta_label;

                                const string& memo = status->get_message();
                                if (!memo.empty()) entry.operation_notes[op_index] = memo;
                            }
                        }

                        const auto scan_delta = [&](const Asset& delta_amount) -> bool
                        {
                            return collect_balance(balance_id, delta_amount);
                        };

                        return eval_state.scan_deltas(op_index, scan_delta);
                    };

                    switch (WithdrawConditionTypes(op.condition.type))
                    {
                    case withdraw_signature_type:
                        return scan_withdraw_with_signature(op.condition.as<WithdrawWithSignature>());
                        // TODO: Other withdraw types
                    default:
                        break;
                    }

                    return false;
                };

                const auto scan_register_account = [&](const RegisterAccountOperation& op) -> bool
                {
                    const string& account_name = op.name;

                    if (entry.operation_notes.count(op_index) == 0)
                    {
                        const string note = "register account " + account_name;
                        entry.operation_notes[op_index] = note;
                    }

                    return account_names.count(account_name) > 0;
                };

                const auto scan_update_account = [&](const UpdateAccountOperation& op) -> bool
                {
                    const oAccountEntry account_entry = _blockchain->get_account_entry(op.account_id);
                    const string& account_name = account_entry.valid() ? account_entry->name : std::to_string(op.account_id);

                    if (entry.operation_notes.count(op_index) == 0)
                    {
                        const string note = "update account " + account_name;
                        entry.operation_notes[op_index] = note;
                    }

                    return account_names.count(account_name) > 0;
                };

                const auto scan_withdraw_pay = [&](const WithdrawPayOperation& op) -> bool
                {
                    const oAccountEntry account_entry = _blockchain->get_account_entry(op.account_id);
                    const string& account_name = account_entry.valid() ? account_entry->name : std::to_string(op.account_id);

                    const string delta_label = "INCOME-" + account_name;
                    const auto scan_delta = [&](const Asset& delta_amount) -> bool
                    {
                        raw_delta_amounts[delta_label][delta_amount.asset_id] += delta_amount.amount;
                        return account_names.count(account_name) > 0;
                    };

                    return eval_state.scan_deltas(op_index, scan_delta);
                };

                const auto scan_create_asset = [&](const CreateAssetOperation& op) -> bool
                {
                    const oAccountEntry account_entry = _blockchain->get_account_entry(op.issuer_account_id);
                    const string& account_name = account_entry.valid() ? account_entry->name : string("MARKET");

                    if (entry.operation_notes.count(op_index) == 0)
                    {
                        const string note = account_name + " created asset " + op.symbol;
                        entry.operation_notes[op_index] = note;
                    }

                    return account_names.count(account_name) > 0;
                };

                const auto scan_issue_asset = [&](const IssueAssetOperation& op) -> bool
                {
                    const auto scan_delta = [&](const Asset& delta_amount) -> bool
                    {
                        const oAssetEntry asset_entry = _blockchain->get_asset_entry(delta_amount.asset_id);
                        string account_name = "GOD";
                        if (asset_entry.valid())
                        {
                            const oAccountEntry account_entry = _blockchain->get_account_entry(asset_entry->issuer_account_id);
                            account_name = account_entry.valid() ? account_entry->name : std::to_string(asset_entry->issuer_account_id);
                        }

                        const string delta_label = "ISSUER-" + account_name;
                        raw_delta_amounts[delta_label][delta_amount.asset_id] += delta_amount.amount;

                        return account_names.count(account_name) > 0;
                    };

                    return eval_state.scan_deltas(op_index, scan_delta);
                };

                const auto scan_update_signing_key = [&](const UpdateSigningKeyOperation& op) -> bool
                {
                    const oAccountEntry account_entry = _blockchain->get_account_entry(op.account_id);
                    const string& account_name = account_entry.valid() ? account_entry->name : std::to_string(op.account_id);

                    if (entry.operation_notes.count(op_index) == 0)
                    {
                        const string note = "update signing key for " + account_name;
                        entry.operation_notes[op_index] = note;
                    }

                    return account_names.count(account_name) > 0;
                };

                bool relevant_to_me = false;
                for (const auto& op : eval_state.trx.operations)
                {
                    bool result = false;
                    switch (OperationTypeEnum(op.type))
                    {
                    case withdraw_op_type:
                        result = scan_withdraw(op.as<WithdrawOperation>());
                        break;
                    case deposit_op_type:
                        result = scan_deposit(op.as<DepositOperation>());
                        break;
                    case register_account_op_type:
                        result = scan_register_account(op.as<RegisterAccountOperation>());
                        _dirty_accounts |= result;
                        break;
                    case update_account_op_type:
                        result = scan_update_account(op.as<UpdateAccountOperation>());
                        _dirty_accounts |= result;
                        break;
                    case withdraw_pay_op_type:
                        result = scan_withdraw_pay(op.as<WithdrawPayOperation>());
                        break;
                    case create_asset_op_type:
                        result = scan_create_asset(op.as<CreateAssetOperation>());
                        break;
                    case update_asset_op_type:
                        // TODO
                        break;
                    case issue_asset_op_type:
                        result = scan_issue_asset(op.as<IssueAssetOperation>());
                        break;
                        //case create_asset_prop_op_type:
                        //    // TODO
                        //    break;

                    case define_slate_op_type:
                        // Don't care; do nothing
                        break;

                    case release_escrow_op_type:
                        // TODO
                        break;
                    case update_signing_key_op_type:
                        result = scan_update_signing_key(op.as<UpdateSigningKeyOperation>());
                        break;
                        // case relative_bid_op_type:
                        // TODO
                        // break;

                    case update_balance_vote_op_type:
                        // TODO
                        break;
                    default:
                        // Ignore everything else
                        break;
                    }

                    relevant_to_me |= result;
                    ++op_index;
                }

                // Kludge to build pretty incoming TITAN transfers
                // We only support such transfers when all operations are withdrawals plus a single deposit
                const auto rescan_with_titan_info = [&]() -> bool
                {
                    if (withdrawal_count + titan_memos.size() != eval_state.trx.operations.size())
                        return false;

                    if (titan_memos.size() != 1)
                        return false;

                    if (entry.delta_labels.size() != 1)
                        return false;

                    const MemoStatus& memo = titan_memos.front();
                    if (!memo.has_valid_signature)
                        return false;

                    string account_name;
                    const Address from_address(memo.from);
                    const oAccountEntry chain_account_entry = _blockchain->get_account_entry(from_address);
                    const oWalletAccountEntry local_account_entry = _wallet_db.lookup_account(from_address);
                    if (chain_account_entry.valid())
                        account_name = chain_account_entry->name;
                    else if (local_account_entry.valid())
                        account_name = local_account_entry->name;
                    else
                        return false;

                    for (uint16_t i = 0; i < eval_state.trx.operations.size(); ++i)
                    {
                        if (OperationTypeEnum(eval_state.trx.operations.at(i).type) == withdraw_op_type)
                            entry.delta_labels[i] = account_name;
                    }

                    return true;
                };

                if (rescan_with_titan_info())
                    return scan_transaction_experimental(eval_state, account_keys, account_balances, account_names, entry, store_entry);

                for (const auto& delta_item : eval_state.balance)
                {
                    const Asset delta_amount(delta_item.second, delta_item.first);
                    if (delta_amount.amount != 0)
                        raw_delta_amounts["FEE"][delta_amount.asset_id] += delta_amount.amount;
                }

                for (const auto& item : raw_delta_amounts)
                {
                    const string& delta_label = item.first;
                    for (const auto& delta_item : item.second)
                        entry.delta_amounts[delta_label].emplace_back(delta_item.second, delta_item.first);
                }

                if (relevant_to_me && store_entry) // TODO
                {
                    ulog("wallet_transaction_entry_v2:\n${rec}", ("rec", fc::json::to_pretty_string(entry)));
                    _wallet_db.experimental_transactions[entry.id] = entry;
                }

                _dirty_balances |= relevant_to_me;
            } FC_CAPTURE_AND_RETHROW((eval_state)(account_balances)(account_names)(entry)(store_entry))
        }

        TransactionLedgerEntry detail::WalletImpl::apply_transaction_experimental(const SignedTransaction& transaction)
        {
            try {
                const TransactionEvaluationStatePtr eval_state = _blockchain->store_pending_transaction(transaction, true);
                FC_ASSERT(eval_state != nullptr);
                return scan_transaction_experimental(*eval_state, -1, blockchain::now(), true);
            } FC_CAPTURE_AND_RETHROW((transaction))
        }

        TransactionLedgerEntry Wallet::scan_transaction_experimental(const string& transaction_id_prefix, bool overwrite_existing)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");

                // TODO: Separate this finding logic
                if (transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string(TransactionIdType()).size())
                    FC_THROW_EXCEPTION(invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix", transaction_id_prefix));

                const auto transaction_id = variant(transaction_id_prefix).as<TransactionIdType>();
                const auto transaction_entry = my->_blockchain->get_transaction(transaction_id, false);
                if (!transaction_entry.valid())
                    FC_THROW_EXCEPTION(transaction_not_found, "Transaction not found!", ("transaction_id_prefix", transaction_id_prefix));

                const auto block_num = transaction_entry->chain_location.block_num;
                const auto block = my->_blockchain->get_block_header(block_num);

                return my->scan_transaction_experimental(*transaction_entry, block_num, block.timestamp, overwrite_existing);
            } FC_CAPTURE_AND_RETHROW((transaction_id_prefix)(overwrite_existing))
        }

        void Wallet::add_transaction_note_experimental(const string& transaction_id_prefix, const string& note)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");

                // TODO: Separate this finding logic
                if (transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string(TransactionIdType()).size())
                    FC_THROW_EXCEPTION(invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix", transaction_id_prefix));

                const auto transaction_id = variant(transaction_id_prefix).as<TransactionIdType>();
                const auto transaction_entry = my->_blockchain->get_transaction(transaction_id, false);
                if (!transaction_entry.valid())
                    FC_THROW_EXCEPTION(transaction_not_found, "Transaction not found!", ("transaction_id_prefix", transaction_id_prefix));

                const auto entry_id = transaction_entry->trx.id();
                if (my->_wallet_db.experimental_transactions.count(entry_id) == 0) // TODO
                    FC_THROW_EXCEPTION(transaction_not_found, "Transaction not found!", ("transaction_id_prefix", transaction_id_prefix));

                auto entry = my->_wallet_db.experimental_transactions[entry_id];
                if (!note.empty())
                    entry.operation_notes[transaction_entry->trx.operations.size()] = note;
                else
                    entry.operation_notes.erase(transaction_entry->trx.operations.size());
                my->_wallet_db.experimental_transactions[entry_id] = entry;

            } FC_CAPTURE_AND_RETHROW((transaction_id_prefix)(note))
        }

        set<PrettyTransactionExperimental> Wallet::transaction_history_experimental(const string& account_name)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");

                set<PrettyTransactionExperimental> history;

                const auto& transactions = my->_wallet_db.get_transactions();
                for (const auto& item : transactions)
                {
                    try
                    {
                        scan_transaction_experimental(string(item.second.trx.id()), false);
                    }
                    catch (...)
                    {
                    }
                }

                for (const auto& item : my->_wallet_db.experimental_transactions) // TODO
                {
                    history.insert(to_pretty_transaction_experimental(item.second));
                }

                const auto account_entrys = list_my_accounts();
                // TODO: Merge this into to_pretty_trx
                map<string, map<AssetIdType, ShareType>> balances;
                for (const auto& entry : history)
                {
                    for (const auto& item : entry.delta_amounts)
                    {
                        const string& label = item.first;
                        const auto has_label = [&](const WalletAccountEntry& account_entry)
                        {
                            return account_entry.name == label;
                        };

                        if (!std::any_of(account_entrys.begin(), account_entrys.end(), has_label))
                            continue;

                        for (const Asset& delta_amount : item.second)
                            balances[label][delta_amount.asset_id] += delta_amount.amount;
                    }

                    for (const auto& item : balances)
                    {
                        const string& label = item.first;
                        for (const auto& balance_item : item.second)
                        {
                            const Asset balance_amount(balance_item.second, balance_item.first);
                            entry.balances.push_back(std::make_pair(label, balance_amount));
                        }
                    }
                }

                return history;
            } FC_CAPTURE_AND_RETHROW((account_name))
        }

        PrettyTransactionExperimental Wallet::to_pretty_transaction_experimental(const TransactionLedgerEntry& entry)
        {
            try {
                PrettyTransactionExperimental result;
                TransactionLedgerEntry& ugly_result = result;
                ugly_result = entry;

                for (const auto& item : entry.delta_amounts)
                {
                    const string& delta_label = item.first;
                    for (const Asset& delta_amount : item.second)
                    {
                        if (delta_amount.amount >= 0)
                            result.outputs.emplace_back(delta_label, delta_amount);
                        else
                            result.inputs.emplace_back(delta_label, -delta_amount);
                    }
                }

                result.notes.reserve(entry.operation_notes.size());
                for (const auto& item : entry.operation_notes)
                {
                    const auto& note = item.second;
                    result.notes.push_back(note);
                }

                const auto delta_compare = [](const std::pair<string, Asset>& a, const std::pair<string, Asset>& b) -> bool
                {
                    const string& a_label = a.first;
                    const string& b_label = b.first;

                    const Asset& a_amount = a.second;
                    const Asset& b_amount = b.second;

                    if (a_label == b_label)
                        return a_amount.asset_id < b_amount.asset_id;

                    if (islower(a_label[0]) != islower(b_label[0]))
                        return islower(a_label[0]);

                    if ((a_label.find("FEE") == string::npos) != (b_label.find("FEE") == string::npos))
                        return a_label.find("FEE") == string::npos;

                    return a < b;
                };

                std::sort(result.inputs.begin(), result.inputs.end(), delta_compare);
                std::sort(result.outputs.begin(), result.outputs.end(), delta_compare);

                return result;
            } FC_CAPTURE_AND_RETHROW((entry))
        }

    }
} // thinkyoung::wallet
