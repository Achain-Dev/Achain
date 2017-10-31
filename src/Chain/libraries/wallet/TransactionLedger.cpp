// TODO: Everything in this file needs to be rewritten in transaction_ledger_experimental.cpp
// When GitHub issue #845 is done, then this file can be deleted

#include <wallet/Exceptions.hpp>
#include <wallet/Wallet.hpp>
#include <wallet/WalletImpl.hpp>
#include <wallet/WalletEntrys.hpp>
#include <blockchain/Time.hpp>
#include <blockchain/TransactionOperations.hpp>
#include <blockchain/ContractOperations.hpp>
#include <blockchain/ImessageOperations.hpp>
#include <sstream>

using namespace thinkyoung::wallet;
using namespace thinkyoung::wallet::detail;


// TODO: No longer needed with scan_genesis_experimental and get_account_balance_entrys
void WalletImpl::scan_balances()
{
    scan_balances_experimental();
    /* Delete ledger entries for any genesis balances before we can reconstruct them */
    const auto my_accounts = self->list_my_accounts();
    for (const auto& account : my_accounts)
    {
        const auto entry_id = fc::ripemd160::hash(account.name);
        auto transaction_entry = _wallet_db.lookup_transaction(entry_id);
        if (transaction_entry.valid())
        {
            transaction_entry->ledger_entries.clear();
            _wallet_db.store_transaction(*transaction_entry);
        }
    }

    const auto timestamp = _blockchain->get_genesis_timestamp();
    const auto scan_balance = [&](const BalanceEntry& bal_rec)
    {
        if (!bal_rec.snapshot_info.valid())
            return;

        if (bal_rec.condition.type != withdraw_signature_type)
            return;

        const auto owner = bal_rec.owner();
        if (!owner.valid())
            return;

        const auto key_rec = _wallet_db.lookup_key(*owner);
        if (!key_rec.valid() || !key_rec->has_private_key())
            return;

        const auto public_key = key_rec->public_key;
        auto entry_id = fc::ripemd160::hash(self->get_key_label(public_key));
        auto transaction_entry = _wallet_db.lookup_transaction(entry_id);
        if (!transaction_entry.valid())
        {
            transaction_entry = WalletTransactionEntry();
            transaction_entry->created_time = timestamp;
            transaction_entry->received_time = timestamp;


        }

        auto entry = LedgerEntry();
        entry.to_account = public_key;
        entry.amount = Asset(bal_rec.snapshot_info->original_balance, bal_rec.condition.asset_id);
        entry.memo = "claim " + bal_rec.snapshot_info->original_address;

        transaction_entry->entry_id = entry_id;
        transaction_entry->is_virtual = true;
        transaction_entry->is_confirmed = true;
        transaction_entry->ledger_entries.push_back(entry);
        _wallet_db.store_transaction(*transaction_entry);
    };
    _blockchain->scan_balances(scan_balance);
}

void WalletImpl::scan_block(uint32_t block_num)
{
    try {
        const SignedBlockHeader block_header = _blockchain->get_block_header(block_num);
        const vector<TransactionEntry> transaction_entrys = _blockchain->get_transactions_for_block(block_header.id());
        fc::async([=]
        {
            vector<fc::future<void>> scan_key_progress;
            for (int i = 0; i < transaction_entrys.size(); i++)
            {
                auto eval_state = transaction_entrys[i];
                scan_key_progress.push_back(_scanner_threads[i%_num_scanner_threads]->async([=]
        {
            try
            {
                scan_transaction(eval_state.trx, block_num, block_header.timestamp,true);
            }
            catch (...)
            {
            }
                }, "scan transaction"));
                if (i % 200 == 0 && i > 0)
                {
                    for (auto fut : scan_key_progress)
                    {
                        try
                        {
                            fut.wait();
        }
                        catch (...)
                        {
                        }
                    }
                    scan_key_progress.clear();
                }
            }
            for (auto fut : scan_key_progress)
            {
                try
                {
                    fut.wait();
                }
                catch (...)
                {
                }
            }
        }).wait();

    } FC_CAPTURE_AND_RETHROW((block_num))
}

WalletTransactionEntry WalletImpl::scan_transaction(
    const SignedTransaction& transaction,
    uint32_t block_num,
    const time_point_sec block_timestamp,
    bool overwrite_existing,
    bool bNeedcreate)
{
    try {
        const TransactionIdType transaction_id = transaction.id();
        const TransactionIdType& entry_id = transaction_id;
        oWalletTransactionEntry transaction_entry;
        {
            readlock rlock(m_mutex_for_wallet);
            transaction_entry = _wallet_db.lookup_transaction(entry_id);
        }
        const auto already_exists = transaction_entry.valid();
        if (!already_exists)
        {
            transaction_entry = WalletTransactionEntry();
            transaction_entry->entry_id = entry_id;
            transaction_entry->created_time = block_timestamp;
            transaction_entry->received_time = block_timestamp;
            transaction_entry->trx = transaction;
        }
        if (already_exists && overwrite_existing)
        {
            transaction_entry->ledger_entries.clear();
        }
        bool new_transaction = !transaction_entry->is_confirmed;

        transaction_entry->block_num = block_num;
        transaction_entry->is_confirmed = true;

        if (already_exists) /* Otherwise will get stored below if this is for me */
        {
            writelock wlock(m_mutex_for_wallet);
            _wallet_db.store_transaction(*transaction_entry);
        }

        auto store_entry = false;

        /* Clear share amounts (but not asset ids) and we will reconstruct them below */
        for (auto& entry : transaction_entry->ledger_entries)
        {
            if (entry.memo.find("yield") == string::npos)
                entry.amount.amount = 0;
        }

        // Assume fees = withdrawals - deposits
        auto total_fee = Asset(0, 0); // Assume all fees paid in base asset

        PublicKeyType withdraw_pub_key;

        // Force scanning all withdrawals first because ledger reconstruction assumes such an ordering
        auto has_withdrawal = false;
        for (const auto& op : transaction.operations)
        {
            readlock(m_mutex_for_wallet);
            switch (OperationTypeEnum(op.type))
            {
            case withdraw_op_type:
                has_withdrawal |= scan_withdraw(op.as<WithdrawOperation>(), *transaction_entry, total_fee, withdraw_pub_key, bNeedcreate);
                break;
            case withdraw_pay_op_type:
                has_withdrawal |= scan_withdraw_pay(op.as<WithdrawPayOperation>(), *transaction_entry, total_fee, bNeedcreate);
                break;
			case balances_withdraw_op_type:
				has_withdrawal |= scan_withdraw_balances(op.as<BalancesWithdrawOperation>(), *transaction_entry, total_fee, withdraw_pub_key, bNeedcreate);
				break;
			case withdraw_contract_op_type:
				has_withdrawal |= scan_withdraw_contract(op.as<WithdrawContractOperation>(), *transaction_entry, total_fee, withdraw_pub_key, bNeedcreate);
				break;
			case on_destroy_op_type:
				has_withdrawal |= scan_ondestroycontract_withdraw(op.as<OnDestroyOperation>(), *transaction_entry, total_fee, withdraw_pub_key, bNeedcreate);
				break;
            default:
                break;
            }
        }
        store_entry |= has_withdrawal;


        // Force scanning all deposits next because ledger reconstruction assumes such an ordering
        auto has_deposit = false;
        bool is_deposit = false;
        for (const auto& op : transaction.operations)
        {
            readlock(m_mutex_for_wallet);
            switch (OperationTypeEnum(op.type))
            {
            case deposit_op_type:
            {
                is_deposit = scan_deposit(op.as<DepositOperation>(), *transaction_entry, total_fee, has_withdrawal);
                has_deposit |= is_deposit;
                break;
            }
			case deposit_contract_op_type:
			{
				is_deposit = scan_deposit_contract(op.as<DepositContractOperation>(), *transaction_entry, total_fee, has_withdrawal);
				has_deposit |= is_deposit;
				overwrite_existing = true;
				break;
			}
			case on_destroy_op_type:
			{
				is_deposit = scan_ondestroycontract_deposit(op.as<OnDestroyOperation>(), *transaction_entry, total_fee, has_withdrawal);
				has_deposit |= is_deposit;
				overwrite_existing = true;
				break;
			}
            default:
                break;
            }
        }
        store_entry |= has_deposit;

        if (new_transaction && is_deposit && transaction_entry && transaction_entry->ledger_entries.size())
        {
            writelock wlock(m_mutex_for_private);
            self->wallet_claimed_transaction(transaction_entry->ledger_entries.back());
        }

        /* Reconstruct fee
        if (has_withdrawal && !has_deposit) 
        {
            for (auto& entry : transaction_entry->ledger_entries)
            {
                if (entry.amount.asset_id == total_fee.asset_id)
                    entry.amount -= total_fee;
            }

        } */
        transaction_entry->fee = total_fee;

        /* When the only withdrawal for asset 0 is the fee (bids) */
    //     if( transaction_entry->ledger_entries.size() > 1 )
    //     {
    //         const auto entries = transaction_entry->ledger_entries;
    //         transaction_entry->ledger_entries.clear();
    //         for( const auto& entry : entries )
    //         {
    //             if( entry.amount != transaction_entry->fee )
    //                 transaction_entry->ledger_entries.push_back( entry );
    // 


        for (const auto& op : transaction.operations)
        {
            readlock(m_mutex_for_wallet);
            switch (OperationTypeEnum(op.type))
            {
            case register_account_op_type:
                store_entry |= scan_register_account(op.as<RegisterAccountOperation>(), *transaction_entry);
                break;
            case update_account_op_type:
                store_entry |= scan_update_account(op.as<UpdateAccountOperation>(), *transaction_entry);
                break;
            case create_asset_op_type:
                store_entry |= scan_create_asset(op.as<CreateAssetOperation>(), *transaction_entry);
                break;
            case update_asset_op_type:
                // TODO
                break;
            case issue_asset_op_type:
                store_entry |= scan_issue_asset(op.as<IssueAssetOperation>(), *transaction_entry);
                break;

            default:
                break;
            }
        }

        if (has_withdrawal)
        {
            auto blockchain_trx_state = _blockchain->get_transaction(transaction_id);
            if (blockchain_trx_state.valid())
            {
                if (!transaction_entry->ledger_entries.empty())
                {
                    /* Remove all yield entries and re-add them */
                    while (!transaction_entry->ledger_entries.empty()
                        && transaction_entry->ledger_entries.back().memo.find("yield") == 0)
                    {
                        transaction_entry->ledger_entries.pop_back();
                    }

                    for (const auto& yield_item : blockchain_trx_state->yield)
                    {
                        auto entry = LedgerEntry();
                        entry.amount = Asset(yield_item.second, yield_item.first);
                        entry.to_account = withdraw_pub_key;
                        entry.from_account = withdraw_pub_key;
                        entry.memo = "yield";
                        transaction_entry->ledger_entries.push_back(entry);
                        writelock wlock(m_mutex_for_private);
                        self->wallet_claimed_transaction(transaction_entry->ledger_entries.back());
                    }

                    if (!blockchain_trx_state->yield.empty())
                    {
                        writelock wlock(m_mutex_for_wallet);
                        _wallet_db.store_transaction(*transaction_entry);
                    }
                }
            }
        }
        else
        {
            if (store_entry && transaction.alp_account != "")
            {
                AlpTrxidBalance alp_entry;
                const auto pretty_trx = self->to_pretty_trx(*transaction_entry);
                for (const auto item : pretty_trx.ledger_entries)
                {
                    if (item.from_account != "")
                    {
                        alp_entry.from_account = item.from_account;
                        break;
                    }
                }
                string strToAccount;
                string strSubAccount;
                self->accountsplit(transaction.alp_account, strToAccount, strSubAccount);
                auto blockchain_trx = _blockchain->get_transaction(transaction_id);
                bool bDiffDeop = false;
                if (blockchain_trx.valid())
                {
                    ShareType alp_amount = 0;
                    string preowner;
                    int iLoop = 0;
                    for (const auto& op : blockchain_trx->trx.operations)
                    {
                        switch (op.type)
                        {
                        case deposit_op_type:
                            auto deop = op.as<DepositOperation>();
                            if (0 == iLoop)
                            {
                                preowner = string(*(deop.condition.owner()));
                                alp_amount += deop.amount;
                            }
                            else
                            {
                                if (preowner == string(*(deop.condition.owner())))
                                {
                                    alp_amount += deop.amount;
                                }
                                else
                                {
                                    alp_amount = 0;
                                    bDiffDeop = true;
                                }
                            }
                            ++iLoop;
                            break;
                        }
                        if (bDiffDeop)
                        {
                            break;
                        }
                    }
                //alp_entry.from_account = transaction.from_account;
                alp_entry.create_time = transaction_entry->created_time;
                alp_entry.alp_account = transaction.alp_account;
                alp_entry.block_num = transaction_entry->block_num;
                transaction_entry->trx.alp_account = transaction.alp_account;
                transaction_entry->trx.alp_inport_asset = transaction.alp_inport_asset;
                alp_entry.trx_id = transaction.id();
                    //alp_entry.asset_trx = transaction.alp_inport_asset;
                    alp_entry.asset_trx = Asset(alp_amount, 0);
                    {
                        writelock wlock(m_mutex_for_alp);
                _blockchain->transaction_insert_to_alp_full_entry(strToAccount, alp_entry);
                //_blockchain->transaction_insert_to_alp_balance(alp_entry.alp_account, alp_entry);
                    }
                }
            }
        }

        /* Only overwrite existing entry if you did not create it or overwriting was explicitly specified */
        if (store_entry && (!already_exists || overwrite_existing))
        {
            writelock wlock(m_mutex_for_wallet);
            _wallet_db.store_transaction(*transaction_entry);
        }
		//check whether or not to scan contracts
		{
            writelock wlock(m_mutex_for_private);
            _dirty_balances |= store_entry;
		}

        return *transaction_entry;
    } FC_CAPTURE_AND_RETHROW()
}

// TODO: Refactor scan_withdraw{_pay}; almost exactly the same
bool WalletImpl::scan_withdraw(const WithdrawOperation& op,
    WalletTransactionEntry& trx_rec, Asset& total_fee,
    PublicKeyType& withdraw_pub_key, bool bNeedcreate)
{
    try {
        const auto bal_rec = _blockchain->get_balance_entry(op.balance_id);
        FC_ASSERT(bal_rec.valid());
        const auto amount = Asset(op.amount, bal_rec->condition.asset_id);

        if (amount.asset_id == total_fee.asset_id)
            total_fee += amount;

        const auto owner = bal_rec->owner();
        if (!owner.valid())
            return false;

        const auto key_rec = _wallet_db.lookup_key(*owner);
        if (key_rec.valid() && key_rec->has_private_key()) /* If we own this balance */
        {
            auto new_entry = true;
            for (auto& entry : trx_rec.ledger_entries)
            {
                if (!entry.from_account.valid()) continue;
                const auto a1 = self->get_account_for_address(*entry.from_account);
                if (!a1.valid()) continue;
                const auto a2 = self->get_account_for_address(key_rec->account_address);
                if (!a2.valid()) continue;
                if (a1->name != a2->name) continue;

                // TODO: We should probably really have a map of asset ids to amounts per ledger entry
                if (entry.amount.asset_id == amount.asset_id)
                {
                    entry.amount += amount;
                    new_entry = false;
                    break;
                }
                else if (entry.amount.amount == 0)
                {
                    if (amount != trx_rec.fee)
                        entry.amount = amount;
                    new_entry = false;
                    break;
                }
            }
            if (new_entry)
            {
                auto entry = LedgerEntry();
                entry.from_account = key_rec->public_key;
                entry.amount = amount;
                trx_rec.ledger_entries.push_back(entry);
            }
            withdraw_pub_key = key_rec->public_key;

            return true;
        }
        else if (!bNeedcreate)
        {
            try{
                auto account_rec = _blockchain->get_account_entry(*owner);
                if (account_rec.valid() && !trx_rec.ledger_entries.empty())
                    //if (account_rec.valid())
                {
                    auto entry = LedgerEntry();
                    entry.from_account = account_rec->owner_key;
                    //printf("%s\n", (string)address(*(entry.from_account)));
                    entry.to_account = account_rec->owner_key;
                    trx_rec.ledger_entries.push_back(entry);
                }
            }
            catch (...)
            {
            }
        }
        return false;
    } FC_CAPTURE_AND_RETHROW()
}
// TODO: Refactor scan_withdraw{_pay}; almost exactly the same
bool WalletImpl::scan_withdraw_balances(const BalancesWithdrawOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, PublicKeyType key, bool bNeedcreate)
{
	try {
		ShareType all_balance = 0;     
		auto it = op.balances.begin();
		BalanceIdType bid;
		while (it != op.balances.end())
		{
			bid = it->first;


			const auto bal_rec = _blockchain->get_balance_entry(bid);
			FC_ASSERT(bal_rec.valid());
			const auto amount = Asset(it->second, bal_rec->condition.asset_id);

			if (amount.asset_id == total_fee.asset_id)
				total_fee += amount;

			const auto owner = bal_rec->owner();
			if (!owner.valid())
				return false;

			const auto key_rec = _wallet_db.lookup_key(*owner);
			if (key_rec.valid() && key_rec->has_private_key()) /* If we own this balance */
			{
				auto new_entry = true;
				for (auto& entry : trx_rec.ledger_entries)
				{
					if (!entry.from_account.valid()) continue;
					const auto a1 = self->get_account_for_address(*entry.from_account);
					if (!a1.valid()) continue;
					const auto a2 = self->get_account_for_address(key_rec->account_address);
					if (!a2.valid()) continue;
					if (a1->name != a2->name) continue;

					// TODO: We should probably really have a map of asset ids to amounts per ledger entry
					if (entry.amount.asset_id == amount.asset_id)
					{
						entry.amount += amount;
						new_entry = false;
						break;
					}
					else if (entry.amount.amount == 0)
					{
						if (amount != trx_rec.fee)
							entry.amount = amount;
						new_entry = false;
						break;
					}
				}
				if (new_entry)
				{
					auto entry = LedgerEntry();
					entry.from_account = key_rec->public_key;
					entry.memo += "withdraw exec cost";
					entry.amount = amount;
					trx_rec.ledger_entries.push_back(entry);
				}
				key = key_rec->public_key;
			}
			else if (!bNeedcreate)
			{
				try {
					auto account_rec = _blockchain->get_account_entry(*owner);
					if (account_rec.valid() && !trx_rec.ledger_entries.empty())
						//if (account_rec.valid())
					{
						auto entry = LedgerEntry();
						entry.from_account = account_rec->owner_key;
						entry.memo = "withdraw exec cost";
						//printf("%s\n", (string)address(*(entry.from_account)));
						trx_rec.ledger_entries.push_back(entry);
					}
				}
				catch (...)
				{
				}
			}
			it++;
		}
		return true;
	} FC_CAPTURE_AND_RETHROW()
}
bool thinkyoung::wallet::detail::WalletImpl::scan_withdraw_contract(const WithdrawContractOperation & op, WalletTransactionEntry & trx_rec, Asset & total_fee, PublicKeyType key, bool bNeedcreate)
{
	try {
		const auto amount = Asset(op.amount); // Always base asset

		if (amount.asset_id == total_fee.asset_id)
			total_fee += amount;
		bool create = true;
		for (auto& entry : trx_rec.ledger_entries)
		{
			if (entry.amount.asset_id == amount.asset_id&&entry.from_contract_balance==op.balance_id)
			{
				entry.amount += amount;
				create = false;
			}
		}
		if (create)
		{
			auto entry = LedgerEntry();
			entry.amount = amount;
			entry.from_contract_balance = op.balance_id;
			entry.memo = "withdraw contract";
			trx_rec.ledger_entries.push_back(entry); 
		}
		return true;
	} FC_CAPTURE_AND_RETHROW()
}
bool thinkyoung::wallet::detail::WalletImpl::scan_ondestroycontract_withdraw(const OnDestroyOperation & op, WalletTransactionEntry & trx_rec, Asset & total_fee, PublicKeyType key, bool bNeedcreate)
{
	try {
		const auto amount = Asset(op.amount); // Always base asset

		if (amount.asset_id == total_fee.asset_id)
			total_fee += amount;
		bool create = true;
		for (auto& entry : trx_rec.ledger_entries)
		{
			if (entry.amount.asset_id == amount.asset_id&&entry.from_contract_balance == op.id)
			{
				entry.amount += amount;
				create = false;
			}
		}
		if (create)
		{
			auto entry = LedgerEntry();
			entry.amount = amount;
			entry.from_contract_balance = op.id;
			entry.memo = "withdraw contract";
			trx_rec.ledger_entries.push_back(entry);
		}
		return true;
	} FC_CAPTURE_AND_RETHROW()                            
}
bool thinkyoung::wallet::detail::WalletImpl::scan_ondestroycontract_deposit(const OnDestroyOperation & op, WalletTransactionEntry & trx_rec, Asset & total_fee, bool bHaswithdraw)
{
	try {
		
		auto contract_map=_wallet_db.get_id_contract_map();
		auto it=contract_map.find(op.id);
		if (it == contract_map.end())
			return false;
		auto owner = it->second.owner;

		const auto okey_rec = _wallet_db.lookup_key(owner);
		Asset amount = op.amount;
		if (okey_rec && okey_rec->has_private_key())
		{
			bool new_entry = true;
			for (auto& entry : trx_rec.ledger_entries)
			{
				if (!entry.to_account.valid()) continue;
				//if (!entry.from_account.valid()) continue;
				const auto account_rec = self->get_account_for_address(okey_rec->public_key);
				if (!account_rec.valid()) continue;
				const auto account_key_rec = _wallet_db.lookup_key(account_rec->owner_address());
				if (!account_key_rec.valid()) continue;
				if (entry.to_account != account_key_rec->public_key) continue;
				PublicKeyType to_account_pk;

				if ((entry.to_account).valid())
				{
					to_account_pk = *(entry.to_account);
				}
				if (!trx_rec.trx.is_cancel() && (to_account_pk == PublicKeyType())) /* cover proceeds */
				{
					if (entry.amount.asset_id != amount.asset_id || entry.memo == "withdraw exec cost") continue;
				}
				entry.to_account = account_key_rec->public_key;
				if (bHaswithdraw)
				{
					to_account_pk = *(entry.to_account);
				}
				entry.amount = amount;
				//entry.memo =
				if (!trx_rec.trx.is_cancel() && (to_account_pk == PublicKeyType())) /* cover proceeds */
				{
					if (amount.asset_id == total_fee.asset_id)
						total_fee += amount;
				}
				new_entry = false;
				break;
			}
			if (new_entry)
			{
				auto entry = LedgerEntry();
				//entry.from_account = okey_rec->public_key;
				entry.memo = "depoist to" + string(okey_rec->public_key);
				entry.to_account = okey_rec->public_key;
				entry.amount = amount;
				trx_rec.ledger_entries.push_back(entry);
			}
		}
	} FC_CAPTURE_AND_RETHROW()
}
// TODO: Refactor scan_withdraw{_pay}; almost exactly the same
bool WalletImpl::scan_withdraw_pay(const WithdrawPayOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, bool bNeedcreate)
{
    try {
        const auto amount = Asset(op.amount); // Always base asset

        if (amount.asset_id == total_fee.asset_id)
            total_fee += amount;

        const auto account_rec = _blockchain->get_account_entry(op.account_id);
        FC_ASSERT(account_rec.valid());
        const auto key_rec = _wallet_db.lookup_key(account_rec->owner_key);
        if (key_rec.valid() && key_rec->has_private_key()) /* If we own this account */
        {
            auto new_entry = true;
            for (auto& entry : trx_rec.ledger_entries)
            {
                if (!entry.from_account.valid()) continue;
                const auto a1 = self->get_account_for_address(*entry.from_account);
                if (!a1.valid()) continue;
                const auto a2 = self->get_account_for_address(key_rec->account_address);
                if (!a2.valid()) continue;
                if (a1->name != a2->name) continue;

                // TODO: We should probably really have a map of asset ids to amounts per ledger entry
                if (entry.amount.asset_id == amount.asset_id)
                {
                    entry.amount += amount;
                    if (entry.memo.empty()) entry.memo = "withdraw pay";
                    new_entry = false;
                    break;
                }
                else if (entry.amount.amount == 0)
                {
                    entry.amount = amount;
                    if (entry.memo.empty()) entry.memo = "withdraw pay";
                    new_entry = false;
                    break;
                }
            }
            if (new_entry)
            {
                auto entry = LedgerEntry();
                entry.from_account = key_rec->public_key;
                entry.amount = amount;
                entry.memo = "withdraw pay";
                trx_rec.ledger_entries.push_back(entry);
            }

            return true;
        }
        else if (!bNeedcreate)
        {
            try{
                if (account_rec.valid())
                {
                    auto entry = LedgerEntry();
                    entry.from_account = account_rec->owner_key;
                    entry.to_account = account_rec->owner_key;
                    trx_rec.ledger_entries.push_back(entry);
                }
            }
            catch (...)
            {
            }
        }
        return false;
    } FC_CAPTURE_AND_RETHROW()
}

bool WalletImpl::scan_register_account(const RegisterAccountOperation& op, WalletTransactionEntry& trx_rec)
{
    auto tmp_entry = _wallet_db.lookup_account(op.name);
    if (tmp_entry.valid())
        _dirty_accounts = true;
    auto opt_key_rec = _wallet_db.lookup_key(op.owner_key);

    if (!opt_key_rec.valid() || !opt_key_rec->has_private_key())
        return false;

    auto opt_account = _wallet_db.lookup_account(Address(op.owner_key));
    if (!opt_account.valid())
    {
        wlog("We have the key but no account for registration operation");
        return false;
    }

    wlog("we detected an account register operation for ${name}", ("name", op.name));
    auto account_name_rec = _blockchain->get_account_entry(op.name);
    FC_ASSERT(account_name_rec.valid());

    for (auto& entry : trx_rec.ledger_entries)
    {
        if (!entry.to_account.valid())
        {
            entry.to_account = op.owner_key;
            entry.amount = Asset(0); // Assume scan_withdraw came first
            entry.memo = "register " + account_name_rec->name; // Can't tell if initially registered as a delegate
            break;
        }
        else if (entry.to_account == op.owner_key)
        {
            entry.amount = Asset(0); // Assume scan_withdraw came first
            break;
        }
    }

    _dirty_accounts = true;
    return true;
}

bool WalletImpl::scan_update_account(const UpdateAccountOperation& op, WalletTransactionEntry& trx_rec)
{
    try {
        auto oaccount = _blockchain->get_account_entry(op.account_id);
        FC_ASSERT(oaccount.valid());
        auto opt_key_rec = _wallet_db.lookup_key(oaccount->owner_key);
        if (!opt_key_rec.valid())
            return false;

        auto opt_account = _wallet_db.lookup_account(Address(oaccount->owner_key));
        if (!opt_account.valid())
        {
            wlog("We have the key but no account for update operation");
            return false;
        }
        wlog("we detected an account update operation for ${name}", ("name", oaccount->name));
        auto account_name_rec = _blockchain->get_account_entry(oaccount->name);
        FC_ASSERT(account_name_rec.valid());

        if (!opt_account->is_my_account)
            return false;

        for (auto& entry : trx_rec.ledger_entries)
        {
            if (!entry.to_account.valid())
            {
                entry.to_account = oaccount->owner_key;
                entry.amount = Asset(0); // Assume scan_withdraw came first
                entry.memo = "update " + oaccount->name;
                break;
            }
            else if (entry.to_account == oaccount->owner_key)
            {
                entry.amount = Asset(0); // Assume scan_withdraw came first
                break;
            }
        }

        _dirty_accounts = true;
        return true;
    } FC_CAPTURE_AND_RETHROW()
}

bool WalletImpl::scan_create_asset(const CreateAssetOperation& op, WalletTransactionEntry& trx_rec)
{
    if (op.issuer_account_id != AssetEntry::market_issuer_id)
    {
        auto oissuer = _blockchain->get_account_entry(op.issuer_account_id);
        FC_ASSERT(oissuer.valid());
        auto opt_key_rec = _wallet_db.lookup_key(oissuer->owner_key);
        if (opt_key_rec.valid() && opt_key_rec->has_private_key())
        {
            for (auto& entry : trx_rec.ledger_entries)
            {
                if (!entry.to_account.valid())
                {
                    entry.to_account = oissuer->owner_key;
                    entry.amount = Asset(0); // Assume scan_withdraw came first
                    entry.memo = "create " + op.symbol + " (" + op.name + ")";
                    return true;
                }
                else if (entry.to_account == oissuer->owner_key)
                {
                    entry.amount = Asset(0); // Assume scan_withdraw came first
                    return true;
                }
            }
        }
    }
    return false;
}

bool WalletImpl::scan_issue_asset(const IssueAssetOperation& op, WalletTransactionEntry& trx_rec)
{
    for (auto& entry : trx_rec.ledger_entries)
    {
        if (entry.from_account.valid())
        {
            const auto opt_key_rec = _wallet_db.lookup_key(*entry.from_account);
            if (opt_key_rec.valid() && opt_key_rec->has_private_key())
            {
                entry.amount = op.amount;
                entry.memo = "issue " + _blockchain->to_pretty_asset(op.amount);
                return true;
            }
        }
    }
    return false;
}


bool WalletImpl::scan_deposit(const DepositOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, bool bHaswithdraw)
{
    try {
        auto amount = Asset(op.amount, op.condition.asset_id);
        if (amount.asset_id == total_fee.asset_id)
            total_fee -= amount;

        bool cache_deposit = false;
        switch ((WithdrawConditionTypes)op.condition.type)
        {
        case withdraw_signature_type:
        {
            const auto deposit = op.condition.as<WithdrawWithSignature>();
            if (deposit.memo)
            {
                oMessageStatus status;
                optional<PrivateKeyType> recipient_key;
                try
                {
                    const PrivateKeyType private_key = self->get_private_key(deposit.owner);
                    status = deposit.decrypt_memo_data(private_key, true);
                    if (status.valid()) recipient_key = private_key;
                }
                catch (const fc::exception&)
                {
                }

                if (!status.valid())
                {
                    vector<fc::future<void>> scan_key_progress;
                    scan_key_progress.resize(_stealth_private_keys.size());
                    for (uint32_t i = 0; i < _stealth_private_keys.size(); ++i)
                    {
                        scan_key_progress[i] = fc::async([&, i]()
                        {
                            _scanner_threads[i % _num_scanner_threads]->async([&]()
                            {
                                if (!status.valid())
                                {
                                    const oMessageStatus inner_status = deposit.decrypt_memo_data(_stealth_private_keys.at(i));
                                    if (inner_status.valid())
                                    {
                                        status = inner_status;
                                        recipient_key = _stealth_private_keys.at(i);
                                    }
                                }
                            }, "decrypt memo").wait();
                        });
                    } // for each key

                    for (auto& fut : scan_key_progress)
                    {
                        try
                        {
                            fut.wait();
                        }
                        catch (const fc::exception&)
                        {
                        }
                    }
                }

                /* If I've successfully decrypted then it's for me */
                if (status.valid() && recipient_key.valid())
                {
                    cache_deposit = true;
                    _wallet_db.cache_memo(*status, *recipient_key, _wallet_password);

                    auto new_entry = true;
                    if (status->memo_flags == from_memo)
                    {
                        // 					vector<ledger_entry>::iterator _iter = trx_rec.ledger_entries.begin();
                        // 					for (; _iter != trx_rec.ledger_entries.end();)
                        // 					{
                        // 						if (!_iter->from_account.valid())
                        // 						{
                        // 							++_iter;
                        // 							continue;
                        // 						}
                        // 						if (_iter->to_account.valid())
                        // 						{
                        // 							_iter = trx_rec.ledger_entries.erase(_iter);
                        // 							continue;
                        // 						}
                        // 						if (!_iter->memo_from_account.valid())
                        // 						{
                        // 							const auto a1 = self->get_key_label(*(_iter->from_account));
                        // 							const auto a2 = self->get_key_label(status->from);
                        // 							if (a1 != a2) continue;
                        // 						}
                        // 
                        // 						new_entry = false;
                        // 						if (!_iter->memo_from_account.valid())
                        // 							_iter->from_account = status->from;
                        // 						_iter->to_account = recipient_key->get_public_key();
                        // 						_iter->amount = amount;
                        // 						_iter->memo = status->get_message();
                        // 						break;
                        // 					}
                        for (auto& entry : trx_rec.ledger_entries)
                        {
                            if (!entry.from_account.valid()) continue;

                            if (!entry.memo_from_account.valid())
                            {
                                const auto a1 = self->get_key_label(*entry.from_account);
                                const auto a2 = self->get_key_label(status->from);
                                if (a1 != a2) continue;
                            }

                            new_entry = false;
                            if (!entry.memo_from_account.valid())
                                entry.from_account = status->from;
                            entry.to_account = recipient_key->get_public_key();
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            break;
                        }
                        if (new_entry)
                        {
                            auto entry = LedgerEntry();
                            entry.from_account = status->from;
                            entry.to_account = recipient_key->get_public_key();
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            trx_rec.ledger_entries.push_back(entry);
                        }
                    }
                    else // to_memo
                    {
                        for (auto& entry : trx_rec.ledger_entries)
                        {
							if (!entry.from_account.valid()) continue;

							if (!entry.memo_from_account.valid())
							{
								const auto a1 = self->get_key_label(*entry.from_account);
								const auto a2 = self->get_key_label(status->from);
								if (a1 != a2) continue;
							}
							new_entry = false;
                            entry.from_account = recipient_key->get_public_key();
                            entry.to_account = status->from;
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            break;
                        }
                        if (new_entry)
                        {
                            auto entry = LedgerEntry();
                            entry.from_account = recipient_key->get_public_key();
                            entry.to_account = status->from;
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            trx_rec.ledger_entries.push_back(entry);
                        }
                    }
                }
            }
            else /* non-TITAN with no memo, market cancel, or cover proceeds */
            {
                const auto okey_rec = _wallet_db.lookup_key(deposit.owner);
                if (okey_rec && okey_rec->has_private_key())
                {
                    bool new_entry = true;
                    cache_deposit = true;
                    for (auto& entry : trx_rec.ledger_entries)
                    {
						if (!entry.to_account.valid()) continue;
                        //if (!entry.from_account.valid()) continue;
                        const auto account_rec = self->get_account_for_address(okey_rec->public_key);
                        if (!account_rec.valid()) continue;
                        const auto account_key_rec = _wallet_db.lookup_key(account_rec->owner_address());
                        if (!account_key_rec.valid()) continue;
						if (entry.to_account != account_key_rec->public_key) continue;
                        PublicKeyType to_account_pk;

                        if ((entry.to_account).valid())
                        {
                            to_account_pk = *(entry.to_account);
                        }
                        if (!trx_rec.trx.is_cancel() && (to_account_pk == PublicKeyType())) /* cover proceeds */
                        {
							if (entry.amount.asset_id != amount.asset_id || entry.memo == "withdraw exec cost") continue;
                        }
                        entry.to_account = account_key_rec->public_key;
                        if (bHaswithdraw)
                        {
                            to_account_pk = *(entry.to_account);
                        }
                        entry.amount += amount;
                        //entry.memo =
                        if (!trx_rec.trx.is_cancel() && (to_account_pk == PublicKeyType())) /* cover proceeds */
                        {
                            if (amount.asset_id == total_fee.asset_id)
                                total_fee += amount;
                        }
                        new_entry = false;
                        break;
                    }
                    if (new_entry)
                    {
                        auto entry = LedgerEntry();
                        //entry.from_account = okey_rec->public_key;
						entry.memo = "depoist to" + string(okey_rec->public_key);
                        entry.to_account = okey_rec->public_key;
                        entry.amount = amount;
                        trx_rec.ledger_entries.push_back(entry);
                    }
                }
            }
            break;
        }

        case withdraw_escrow_type:
        {
            const auto deposit = op.condition.as<withdraw_with_escrow>();
            if (deposit.memo)
            {
                oMessageStatus status;
                optional<PrivateKeyType> recipient_key;
                for (const Address& owner : op.condition.owners())
                {
                    try
                    {
                        const PrivateKeyType private_key = self->get_private_key(owner);
                        status = deposit.decrypt_memo_data(private_key, true);
                        if (status.valid())
                        {
                            recipient_key = private_key;
                            break;
                        }
                    }
                    catch (const fc::exception&)
                    {
                    }
                }

                if (!status.valid())
                {
                    vector<fc::future<void>> scan_key_progress;
                    scan_key_progress.resize(_stealth_private_keys.size());
                    for (uint32_t i = 0; i < _stealth_private_keys.size(); ++i)
                    {
                        scan_key_progress[i] = fc::async([&, i]()
                        {
                            _scanner_threads[i % _num_scanner_threads]->async([&]()
                            {
                                if (!status.valid())
                                {
                                    const oMessageStatus inner_status = deposit.decrypt_memo_data(_stealth_private_keys.at(i));
                                    if (inner_status.valid())
                                    {
                                        status = inner_status;
                                        recipient_key = _stealth_private_keys.at(i);
                                    }
                                }
                            }, "decrypt memo").wait();
                        });
                    } // for each key

                    for (auto& fut : scan_key_progress)
                    {
                        try
                        {
                            fut.wait();
                        }
                        catch (const fc::exception&)
                        {
                        }
                    }
                }

                /* If I've successfully decrypted then it's for me */
                if (status.valid() && recipient_key.valid())
                {
                    cache_deposit = true;
                    _wallet_db.cache_memo(*status, *recipient_key, _wallet_password);

                    auto new_entry = true;
                    if (status->memo_flags == from_memo)
                    {
                        for (auto& entry : trx_rec.ledger_entries)
                        {
                            if (!entry.from_account.valid()) continue;
                            if (!entry.memo_from_account.valid())
                            {
                                const auto a1 = self->get_key_label(*entry.from_account);
                                const auto a2 = self->get_key_label(status->from);
                                if (a1 != a2) continue;
                            }

                            new_entry = false;
                            if (!entry.memo_from_account.valid())
                                entry.from_account = status->from;
                            entry.to_account = recipient_key->get_public_key();
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            break;
                        }
                        if (new_entry)
                        {
                            auto entry = LedgerEntry();
                            entry.from_account = status->from;
                            entry.to_account = recipient_key->get_public_key();
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            trx_rec.ledger_entries.push_back(entry);
                        }
                    }
                    else // to_memo
                    {
                        for (auto& entry : trx_rec.ledger_entries)
                        {
                            if (!entry.from_account.valid()) continue;
                            const auto a1 = self->get_key_label(*entry.from_account);
                            const auto a2 = self->get_key_label(recipient_key->get_public_key());
                            if (a1 != a2) continue;

                            new_entry = false;
                            entry.from_account = recipient_key->get_public_key();
                            entry.to_account = status->from;
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            break;
                        }
                        if (new_entry)
                        {
                            auto entry = LedgerEntry();
                            entry.from_account = recipient_key->get_public_key();
                            entry.to_account = status->from;
                            entry.amount = amount;
                            entry.memo = status->get_message();
                            trx_rec.ledger_entries.push_back(entry);
                        }
                    }
                }
            }
            else /* non-TITAN with no memo, market cancel, or cover proceeds */
            {
                for (const Address& owner : op.condition.owners())
                {
                    const auto okey_rec = _wallet_db.lookup_key(owner);
                    if (okey_rec && okey_rec->has_private_key())
                    {
                        bool new_entry = true;
                        cache_deposit = true;
                        for (auto& entry : trx_rec.ledger_entries)
                        {
                            if (!entry.from_account.valid()) continue;
                            const auto account_rec = self->get_account_for_address(okey_rec->public_key);
                            if (!account_rec.valid()) continue;
                            const auto account_key_rec = _wallet_db.lookup_key(account_rec->owner_address());
                            if (!account_key_rec.valid()) continue;
                            if (!trx_rec.trx.is_cancel()) /* cover proceeds */
                            {
                                if (entry.amount.asset_id != amount.asset_id) continue;
                            }
                            entry.to_account = account_key_rec->public_key;
                            entry.amount = amount;
                            //entry.memo =
                            if (!trx_rec.trx.is_cancel()) /* cover proceeds */
                            {
                                if (amount.asset_id == total_fee.asset_id)
                                    total_fee += amount;
                            }
                            new_entry = false;
                            break;
                        }
                        if (new_entry)
                        {
                            auto entry = LedgerEntry();
                            //entry.from_account = okey_rec->public_key;
                            entry.to_account = okey_rec->public_key;
                            entry.amount = amount;
                            trx_rec.ledger_entries.push_back(entry);
                        }
                    }
                }
            }
            break;
        }

        default:
        {
            break;
        }
        }

        return cache_deposit;
    } FC_CAPTURE_AND_RETHROW()
}

bool thinkyoung::wallet::detail::WalletImpl::scan_deposit_contract(const DepositContractOperation & op, WalletTransactionEntry & trx_rec, Asset & total_fee, bool bHaswithdraw)
{
	try{
		auto entry = LedgerEntry();
		
		bool found = false;
		total_fee -= Asset(op.amount);
		for (auto& it : trx_rec.ledger_entries)
		{
			if (it.amount.asset_id == 0&&it.to_contract_balance ==op.balance_id())
			{
				it.amount =Asset(op.amount,0);
				found = true;
				break;
			}
		}
		if (!found)
		{
			entry.amount = Asset(op.amount);
			entry.to_contract_balance = op.balance_id();
			entry.memo = "to Contract balance " + op.balance_id().AddressToString();
			trx_rec.ledger_entries.push_back(entry);
		}
		
		return true;
	}FC_CAPTURE_AND_RETHROW()
}


WalletTransactionEntry Wallet::scan_all_transaction(bool overwrite_existing)
{
    try {
        const auto all_transaction = my->_wallet_db.get_transactions();
        my->scan_accounts();
        for (const auto transaction : all_transaction)
        {
            const auto transaction_id = transaction.first;
            const auto transaction_entry = my->_blockchain->get_transaction(transaction_id, false);
            if (!transaction_entry.valid())
                continue;



            const auto block_num = transaction_entry->chain_location.block_num;
            const auto block = my->_blockchain->get_block_header(block_num);
            const auto entry = my->scan_transaction(transaction_entry->trx, block_num, block.timestamp, overwrite_existing, true);
        }
        if (my->_dirty_balances) my->scan_balances_experimental();
        if (my->_dirty_accounts) my->scan_accounts();
		if (my->_dirty_contracts) my->scan_contracts();
        WalletTransactionEntry entry;
        return entry;
    }FC_CAPTURE_AND_RETHROW()
}

WalletTransactionEntry Wallet::scan_transaction(const string& transaction_id_prefix, bool overwrite_existing)
{
    try {
        FC_ASSERT(is_open(), "Wallet not open!");
        FC_ASSERT(is_unlocked(), "Wallet not unlock!");

        if (transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string(TransactionIdType()).size())
            FC_THROW_EXCEPTION(invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix", transaction_id_prefix));

        const auto transaction_id = variant(transaction_id_prefix).as<TransactionIdType>();
        const auto transaction_entry = my->_blockchain->get_transaction(transaction_id, false);
        if (!transaction_entry.valid())
            FC_THROW_EXCEPTION(transaction_not_found, "Transaction not found!", ("transaction_id_prefix", transaction_id_prefix));

        my->scan_accounts();
		my->scan_contracts();

        const auto block_num = transaction_entry->chain_location.block_num;
        const auto block = my->_blockchain->get_block_header(block_num);
        const auto entry = my->scan_transaction(transaction_entry->trx, block_num, block.timestamp, overwrite_existing, true);

        if (my->_dirty_balances) my->scan_balances_experimental();
        if (my->_dirty_accounts) my->scan_accounts();
		if (my->_dirty_contracts) my->scan_contracts();
        return entry;
    } FC_CAPTURE_AND_RETHROW()
}

vector<WalletTransactionEntry> Wallet::get_transactions(const string& transaction_id_prefix)
{
    try {
        FC_ASSERT(is_open(), "Wallet not open!");

        if (transaction_id_prefix.size() > string(TransactionIdType()).size())
            FC_THROW_EXCEPTION(invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix", transaction_id_prefix));

        auto transactions = vector<WalletTransactionEntry>();
        const auto entrys = my->_wallet_db.get_transactions();
        for (const auto& entry : entrys)
        {
            const auto transaction_id = string(entry.first);
            if (string(transaction_id).find(transaction_id_prefix) != 0) continue;
            transactions.push_back(entry.second);
        }
        return transactions;
    } FC_CAPTURE_AND_RETHROW()
}

void WalletImpl::sign_transaction(SignedTransaction& transaction, const unordered_set<Address>& required_signatures)const
{
    try {
        const auto chain_id = _blockchain->get_chain_id();
        for (const auto& addr : required_signatures)
            transaction.sign(self->get_private_key(addr), chain_id);
    } FC_CAPTURE_AND_RETHROW((transaction)(required_signatures))
}

void Wallet::cache_transaction(WalletTransactionEntry& transaction_entry, bool store)
{
    try {
        my->_blockchain->store_pending_transaction(transaction_entry.trx, true, true,true);
        my->scan_balances_experimental();
		if (store)
		{
			transaction_entry.entry_id = transaction_entry.trx.id();
			transaction_entry.created_time = blockchain::now();
			transaction_entry.received_time = transaction_entry.created_time;
			my->_wallet_db.store_transaction(transaction_entry);
		}
    } FC_CAPTURE_AND_RETHROW((transaction_entry))
}

/**
 * @return the list of all transactions related to this wallet
 */
vector<WalletTransactionEntry> Wallet::get_transaction_history(const string& account_name,
    uint32_t start_block_num,
    uint32_t end_block_num,
    const string& asset_symbol,
    TrxType trx_search_type)const
{
    try {
        FC_ASSERT(is_open(), "Wallet not open!");
        if (end_block_num != -1) FC_ASSERT(start_block_num <= end_block_num);

        vector<WalletTransactionEntry> history_entrys;
        const auto& transactions = my->_wallet_db.get_transactions();

        AssetIdType asset_id = 0;
        if (!asset_symbol.empty() && asset_symbol != ALP_BLOCKCHAIN_SYMBOL)
        {
            try
            {
                asset_id = my->_blockchain->get_asset_id(asset_symbol);
            }
            catch (const fc::exception&)
            {
                FC_THROW_EXCEPTION(invalid_asset_symbol, "Invalid asset symbol!", ("asset_symbol", asset_symbol));
            }
        }

        for (const auto& item : transactions)
        {
            const auto& tx_entry = item.second;

            if (tx_entry.block_num < start_block_num) continue;
            if (end_block_num != -1 && tx_entry.block_num > end_block_num) continue;
            if (tx_entry.ledger_entries.empty()) continue; /* TODO: Temporary */

            if (!account_name.empty())
            {
                bool match = false;
                for (const auto& entry : tx_entry.ledger_entries)
                {
                    if (entry.from_account.valid() && ((trx_search_type == trx_type_all) || trx_search_type == trx_type_withdraw))
                    {
                        const auto account_entry = get_account_for_address(*entry.from_account);
                        const auto account_entry_name = get_account(account_name);
                        if (account_entry.valid()){
                            match |= (account_entry->name == account_name || (account_entry_name.active_key() == account_entry->active_key()) || (account_entry_name.active_address() == account_entry->active_address()));
                            match |= account_entry_name.owner_address() == account_entry->owner_address();
                        }
                        else
                        {
                            printf("cant find\n");
                        }
                        if (match) break;
                    }
                    if (entry.to_account.valid() && ((trx_search_type == trx_type_all) || (trx_search_type == trx_type_desipate)))
                    {
                        const auto account_entry = get_account_for_address(*entry.to_account);
                        const auto account_entry_name = get_account(account_name);
                        if (account_entry.valid()){
                            match |= (account_entry->name == account_name || (account_entry_name.active_key() == account_entry->active_key()) || (account_entry_name.active_address() == account_entry->active_address()));
                            match |= account_entry_name.owner_address() == account_entry->owner_address();
                        }
                        if (match) break;
                    }
                }
                if (!match) continue;
            }

            if (asset_id != 0)
            {
                bool match = false;
                for (const auto& entry : tx_entry.ledger_entries)
                    match |= entry.amount.amount > 0 && entry.amount.asset_id == asset_id;
                match |= tx_entry.fee.amount > 0 && tx_entry.fee.asset_id == asset_id;
                if (!match) continue;
            }
            if (!tx_entry.is_confirmed && (tx_entry.trx.expiration < blockchain::now()))
            {
                continue;
            }
            history_entrys.push_back(tx_entry);
        }

        return history_entrys;
    } FC_CAPTURE_AND_RETHROW()
}
vector<WalletTransactionEntry> Wallet::get_transaction_history_splite(const string& account_name,
    const string& asset_symbol,
    TrxType trx_search_type)const
{
    try {
        FC_ASSERT(is_open());
        vector<WalletTransactionEntry> history_entrys;
        const auto& transactions = my->_wallet_db.get_transactions();
        AssetIdType asset_id = 0;
        if (!asset_symbol.empty() && asset_symbol != ALP_BLOCKCHAIN_SYMBOL)
        {
            try
            {
                asset_id = my->_blockchain->get_asset_id(asset_symbol);
            }
            catch (const fc::exception&)
            {
                FC_THROW_EXCEPTION(invalid_asset_symbol, "Invalid asset symbol!", ("asset_symbol", asset_symbol));
            }
        }

        for (const auto& item : transactions)
        {
            const auto& tx_entry = item.second;
            if (tx_entry.ledger_entries.empty()) continue; /* TODO: Temporary */
            if (!account_name.empty())
            {
                bool match = false;
                for (const auto& entry : tx_entry.ledger_entries)
                {
                    if (entry.from_account.valid() && ((trx_search_type == trx_type_all) || trx_search_type == trx_type_withdraw))
                    {
                        const auto account_entry = get_account_for_address(*entry.from_account);
                        const auto account_entry_name = get_account(account_name);
                        if (account_entry.valid()){
                            match |= (account_entry->name == account_name || (account_entry_name.active_key() == account_entry->active_key()) || (account_entry_name.active_address() == account_entry->active_address()));
                            match |= account_entry_name.owner_address() == account_entry->owner_address();
                        }
                        if (match) break;
                    }
                    if (entry.to_account.valid() && ((trx_search_type == trx_type_all) || (trx_search_type == trx_type_desipate)))
                    {
                        const auto account_entry = get_account_for_address(*entry.to_account);
                        const auto account_entry_name = get_account(account_name);
                        if (account_entry.valid()){
                            match |= (account_entry->name == account_name || (account_entry_name.active_key() == account_entry->active_key()) || (account_entry_name.active_address() == account_entry->active_address()));
                            match |= account_entry_name.owner_address() == account_entry->owner_address();
                        }
                        if (match) break;
                    }
                }
                if (!match) continue;
            }
            else
            {
                bool match = false;
                for (const auto& entry : tx_entry.ledger_entries)
                {
                    if (trx_search_type == trx_type_all)
                    {
                        match = true;
                        break;
                    }
                    if (entry.from_account.valid() && (trx_search_type == trx_type_withdraw))
                    {
                        const auto account_entry = get_account_for_address(*entry.from_account);
                        bool withdraw_pay = false;
                        if (entry.to_account.valid() && (entry.from_account == entry.to_account) && (entry.memo.find("withdraw pay") == 0)){
                            withdraw_pay = true;
                        }
                        if (account_entry.valid() && (!withdraw_pay))
                            match |= account_entry->is_my_account;
                        //if (account_entry.valid()) match |= account_entry->name == account_name;
                        if (match) break;
                    }
                    if (entry.to_account.valid() && ((trx_search_type == trx_type_desipate)))
                    {
                        const auto account_entry = get_account_for_address(*entry.to_account);
                        if (account_entry.valid())
                            match |= account_entry->is_my_account;
                        //if (account_entry.valid()) match |= account_entry->name == account_name;
                        if (match) break;
                    }
                }
                if (!match) continue;
            }
            if (asset_id != 0)
            {
                bool match = false;
                for (const auto& entry : tx_entry.ledger_entries)
                    match |= entry.amount.amount > 0 && entry.amount.asset_id == asset_id;
                match |= tx_entry.fee.amount > 0 && tx_entry.fee.asset_id == asset_id;
                if (!match) continue;
            }
            if (!tx_entry.is_confirmed && (tx_entry.trx.expiration < blockchain::now()))
            {
                continue;
            }
            history_entrys.push_back(tx_entry);
        }
        return history_entrys;
    } FC_CAPTURE_AND_RETHROW()
}
vector<PrettyTransaction> Wallet::get_pretty_transaction_history(const string& account_name,
    uint32_t start_block_num,
    uint32_t end_block_num,
    const string& asset_symbol,
    TrxType trx_splite,
    bool wither_splite)const
{
    try {

        // TODO: Validate all input
        vector<WalletTransactionEntry> history;
        if (wither_splite)
        {
            history = get_transaction_history_splite(account_name, asset_symbol, trx_splite);
        }
        else
        {
            history = get_transaction_history(account_name, start_block_num, end_block_num, asset_symbol);
        }
        vector<PrettyTransaction> pretties;
        pretties.reserve(history.size());
        for (const auto& item : history) pretties.push_back(to_pretty_trx(item));

        const auto sorter = [](const PrettyTransaction& a, const PrettyTransaction& b) -> bool
        {
            if (a.is_confirmed == b.is_confirmed && a.block_num != b.block_num)
                return a.block_num < b.block_num;

            if (a.timestamp != b.timestamp)
                return a.timestamp < b.timestamp;

            return string(a.trx_id).compare(string(b.trx_id)) < 0;
        };
        std::sort(pretties.begin(), pretties.end(), sorter);

        // TODO: Handle pagination

        const auto errors = get_pending_transaction_errors();
        for (auto& trx : pretties)
        {
            if (trx.is_virtual || trx.is_confirmed) continue;
            if (errors.count(trx.trx_id) <= 0) continue;
            const auto trx_rec = my->_blockchain->get_transaction(trx.trx_id);
            if (trx_rec.valid())
            {
                trx.block_num = trx_rec->chain_location.block_num;
                trx.is_confirmed = true;
                continue;
            }
            trx.error = errors.at(trx.trx_id);
        }

        vector<string> account_names;
        bool account_specified = !account_name.empty();
        if (!account_specified)
        {
            const auto accounts = list_my_accounts();
            for (const auto& account : accounts)
                account_names.push_back(account.name);
        }
        else
        {
            account_names.push_back(account_name);
        }

        /* Tally up running balances */
        for (const auto& name : account_names)
        {
            map<AssetIdType, Asset> running_balances;
            for (auto& trx : pretties)
            {
                const auto fee_asset_id = trx.fee.asset_id;
                if (running_balances.count(fee_asset_id) <= 0)
                    running_balances[fee_asset_id] = Asset(0, fee_asset_id);

                auto any_from_me = false;
                for (auto& entry : trx.ledger_entries)
                {
                    const auto amount_asset_id = entry.amount.asset_id;
                    if (running_balances.count(amount_asset_id) <= 0)
                        running_balances[amount_asset_id] = Asset(0, amount_asset_id);

                    auto from_me = false;
                    from_me |= name == entry.from_account;
                    from_me |= (entry.from_account.find(name + " ") == 0); /* If payer != sender */
                    if (from_me)
                    {
                        /* Special check to ignore asset issuing */
                        if ((running_balances[amount_asset_id] - entry.amount) >= Asset(0, amount_asset_id))
                            running_balances[amount_asset_id] -= entry.amount;

                        /* Subtract fee once on the first entry */
  /*                      if (!trx.is_virtual && !any_from_me)
                            running_balances[fee_asset_id] -= trx.fee;
							*/
                    }
                    any_from_me |= from_me;

                    /* Special case to subtract fee if we canceled a bid */
                    if (!trx.is_virtual && trx.is_market_cancel && amount_asset_id != fee_asset_id)
                        running_balances[fee_asset_id] -= trx.fee;

                    auto to_me = false;
                    to_me |= name == entry.to_account;
                    to_me |= (entry.to_account.find(name + " ") == 0); /* If payer != sender */
                    if (to_me) running_balances[amount_asset_id] += entry.amount;

                    entry.running_balances[name][amount_asset_id] = running_balances[amount_asset_id];
                    entry.running_balances[name][fee_asset_id] = running_balances[fee_asset_id];
                }

                if (account_specified)
                {
                    /* Don't return fees we didn't pay */
                    if (trx.is_virtual || (!any_from_me && !trx.is_market_cancel))
                    {
                        trx.fee = Asset();
                    }
                }
            }
        }
        for (auto& entrys : pretties)
        {
            vector<PrettyLedgerEntry>::iterator iter = entrys.ledger_entries.begin();
            string memoTemp;
            for (; iter != entrys.ledger_entries.end();)
            {
                if (iter->amount == Asset(0, 0) && entrys.ledger_entries.size() != 1)
                {
                    if (iter->memo != "")
                    {
                        memoTemp = iter->memo;
                    }
                    iter = entrys.ledger_entries.erase(iter);
                    continue;
                }
                if (Address::is_valid(iter->from_account))
                {
                    auto account_rec = my->_blockchain->get_account_by_address(iter->from_account);
                    if (account_rec.valid())
                    {
                        iter->from_account = account_rec->name;
                    }
                }
                if (Address::is_valid(iter->to_account))
                {
                    auto account_rec_toaccount = my->_blockchain->get_account_by_address(iter->to_account);
                    if (account_rec_toaccount.valid())
                    {
                        iter->to_account = account_rec_toaccount->name;
                    }
                }
                if (memoTemp != "")
                {
                    iter->memo = memoTemp;
                }
                ++iter;
            }
        }
        return pretties;
    } FC_CAPTURE_AND_RETHROW()
}

void Wallet::remove_transaction_entry(const string& entry_id)
{
    const auto& entrys = my->_wallet_db.get_transactions();
    for (const auto& entry : entrys)
    {
        if (string(entry.first).find(entry_id) == 0)
        {
            my->_wallet_db.remove_transaction(entry.first);
            return;
        }
    }
}

PrettyTransaction Wallet::to_pretty_trx(const WalletTransactionEntry& trx_rec) const
{
    PrettyTransaction pretty_trx;
    string op_memo;
    pretty_trx.is_virtual = trx_rec.is_virtual;
    pretty_trx.is_confirmed = trx_rec.is_confirmed;
    pretty_trx.is_market = trx_rec.is_market;
    pretty_trx.is_market_cancel = !trx_rec.is_virtual && trx_rec.is_market && trx_rec.trx.is_cancel();
    pretty_trx.trx_id = !trx_rec.is_virtual ? trx_rec.trx.id() : trx_rec.entry_id;
    pretty_trx.block_num = trx_rec.block_num;

    for (const auto& entry : trx_rec.ledger_entries)
    {
        auto pretty_entry = PrettyLedgerEntry();

        if (entry.from_account.valid())
        {
            pretty_entry.from_account = get_key_label(*entry.from_account);
            if (entry.memo_from_account.valid())
                pretty_entry.from_account += " as " + get_key_label(*entry.memo_from_account);
        }
        else if (trx_rec.is_virtual && trx_rec.block_num <= 0)
            pretty_entry.from_account = "GENESIS";
        else if (trx_rec.is_market)
            pretty_entry.from_account = "MARKET";
        else{
            //pretty_entry.from_account = "UNKNOWN"; 
            for (const auto & op : trx_rec.trx.operations)
            {
                switch (op.type)
                {
                case withdraw_op_type:
                {
                    auto withdraw_op = op.as<WithdrawOperation>();
                    auto bal_rec = my->_blockchain->get_balance_entry(withdraw_op.balance_id);
                    if (bal_rec.valid())
                    {
                        pretty_entry.from_account = (string)(*(bal_rec->condition.owner()));
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        if (entry.to_account.valid())
            pretty_entry.to_account = get_key_label(*entry.to_account);
        else if (trx_rec.is_market)
            pretty_entry.to_account = "MARKET";
        else
        {
            //pretty_entry.to_account = "UNKNOWN";
            for (const auto & op : trx_rec.trx.operations)
            {
                switch (op.type)
                {
                case deposit_op_type:
                {
                    auto deposit_op = op.as<DepositOperation>();
                    pretty_entry.to_account = (string)(*(deposit_op.condition.owner()));
                    break;
                }
                default:
                    break;
                }
            }
        }
        /* To fix running balance calculation when withdrawing delegate pay */
        if (pretty_entry.from_account == pretty_entry.to_account)
        {
            if (entry.memo.find("withdraw pay") == 0)
                pretty_entry.from_account = "NETWORK";
        }

        /* Fix labels for yield payments */
        if (entry.memo.find("yield") == 0)
        {
            pretty_entry.from_account = "NETWORK";

            if (entry.to_account)
            {
                const auto key_entry = my->_wallet_db.lookup_key(*entry.to_account);
                if (key_entry.valid())
                {
                    const auto account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                    if (account_entry.valid())
                        pretty_entry.to_account = account_entry->name;
                }
            }
        }
        else if (entry.memo.find("burn") == 0)
        {
            pretty_entry.to_account = "NETWORK";
        }
        else if (entry.memo.find("collect vested") == 0)
        {
            pretty_entry.from_account = "SHAREDROP";
        }

        /* I'm sorry - Vikram */
        /* You better be. - Dan */
        if (pretty_entry.from_account.find("SHORT") == 0
            && pretty_entry.to_account.find("SHORT") == 0)
            pretty_entry.to_account.replace(0, 5, "MARGIN");

        if (pretty_entry.from_account.find("MARKET") == 0
            && pretty_entry.to_account.find("SHORT") == 0)
            pretty_entry.to_account.replace(0, 5, "MARGIN");

        if (pretty_entry.from_account.find("SHORT") == 0
            && pretty_entry.to_account.find("MARKET") == 0)
            pretty_entry.from_account.replace(0, 5, "MARGIN");

        if (pretty_entry.to_account.find("SHORT") == 0
            && entry.memo.find("payoff") == 0)
            pretty_entry.to_account.replace(0, 5, "MARGIN");

        if (pretty_entry.from_account.find("SHORT") == 0
            && entry.memo.find("cover") == 0)
            pretty_entry.from_account.replace(0, 5, "MARGIN");


        for (const auto & op : trx_rec.trx.operations)
        {
            switch (op.type)
            {
            case imessage_memo_op_type:
            {
                auto message_op = op.as<ImessageMemoOperation>();
                op_memo = message_op.imessage;
                break;
            }
            default:
                break;
            }
        }

        pretty_entry.amount = entry.amount;
        if (op_memo.empty() || op_memo == "")
            pretty_entry.memo = entry.memo;
        else
            pretty_entry.memo = op_memo;

        pretty_trx.ledger_entries.push_back(pretty_entry);
    }

    if (!pretty_trx.is_virtual && !pretty_trx.is_market)
    {
        uint16_t unknown_count = 0;
        uint16_t from_name_count = 0;
        string from_name;
        for (const PrettyLedgerEntry& entry : pretty_trx.ledger_entries)
        {
            if (entry.from_account == "UNKNOWN")
            {
                ++unknown_count;
            }
            else if (ChainInterface::is_valid_account_name(entry.from_account))
            {
                ++from_name_count;
                if (!from_name.empty() && entry.from_account != from_name)
                {
                    from_name_count = 0;
                    break;
                }
                from_name = entry.from_account;
            }
        }

        if (from_name_count > 0 && unknown_count > 0 && from_name_count + unknown_count == pretty_trx.ledger_entries.size())
        {
            for (PrettyLedgerEntry& entry : pretty_trx.ledger_entries)
                entry.from_account = from_name;
        }
    }

    pretty_trx.fee = trx_rec.fee;
    pretty_trx.timestamp = std::min<time_point_sec>(trx_rec.created_time, trx_rec.received_time);
    pretty_trx.expiration_timestamp = trx_rec.trx.expiration;

    return pretty_trx;
}


PrettyTransaction		Wallet::to_pretty_trx(const thinkyoung::blockchain::TransactionEntry& trx_entry, const std::string addr_for_fee) const
{

    PrettyTransaction pretty_trx;
    string op_memo;
    std::string local_addr_for_fee = addr_for_fee;
    if (local_addr_for_fee != "")
    {
        auto local_account_entry = my->_blockchain->get_account_by_address(addr_for_fee);
        if (local_account_entry.valid())
        {
            local_addr_for_fee = local_account_entry->name;
        }
    }
    std::string from_account_str;
    auto total_fee = Asset(0, 0);

    auto block_num = trx_entry.chain_location.block_num;
    auto block_position = trx_entry.chain_location.trx_num;
    auto trx = trx_entry.trx;
    auto block_healder = my->_blockchain->get_block_header(block_num);

    pretty_trx.is_virtual = false;
    pretty_trx.is_confirmed = true;
    pretty_trx.is_market = false;
    pretty_trx.is_market_cancel = false;
    pretty_trx.trx_id = trx_entry.trx.id();
    pretty_trx.block_num = block_num;
    pretty_trx.block_position = block_position;

    // default set normal transaction type 
    pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::normal_transaction;

    //create null account
    auto pretty_entry = PrettyLedgerEntry();


    //find transaction from by transaction entry
    for (const auto & op : trx_entry.trx.operations)
    {
        switch (op.type)
        {
        case withdraw_op_type:
        {
            auto withdraw_op = op.as<WithdrawOperation>();
            auto bal_entry = my->_blockchain->get_balance_entry(withdraw_op.balance_id);
            if (bal_entry.valid())
            {
                pretty_entry.from_account = (string)(*(bal_entry->condition.owner()));

                total_fee += Asset(withdraw_op.amount);

				{
					auto account_entry = my->_blockchain->get_account_by_address(pretty_entry.from_account);
					if (account_entry.valid())
					{
						pretty_entry.from_account_name = account_entry->name;
					}
				}
				if (pretty_entry.from_account_name.size() == 0)
					from_account_str = pretty_entry.from_account;
				else
					from_account_str = pretty_entry.from_account_name;

			}
			break;
		}
		case withdraw_pay_op_type:
		{
			const auto withdraw_pay_op = op.as<WithdrawPayOperation>();
            auto account_entry = my->_blockchain->get_account_entry(withdraw_pay_op.account_id);
            FC_ASSERT(account_entry.valid(), "account entry not valid");

			pretty_entry.from_account = "NETWORK";
			pretty_entry.from_account_name = "NETWORK";
			pretty_entry.memo = "delegate " + account_entry->name + " withdraw pay";
			total_fee += Asset(withdraw_pay_op.amount);

            pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::withdraw_pay_transaction;
		}
		default:
			break;
		}

    }

    if (block_num <= 0)
        pretty_entry.from_account = "GENESIS";

    for (const auto& op : trx.operations)
    {
        switch (OperationTypeEnum(op.type))
        {
        case deposit_op_type:
        {
            auto deposit_op = op.as<DepositOperation>();

			switch ((WithdrawConditionTypes)deposit_op.condition.type)
			{
			case withdraw_signature_type:
			{
				const auto deposit = deposit_op.condition.as<WithdrawWithSignature>();
				pretty_entry.to_account = (string)deposit.owner;
				auto account_entry = my->_blockchain->get_account_by_address(pretty_entry.to_account);
				if (account_entry.valid())
				{
					pretty_entry.to_account_name = account_entry->name;
				}
				pretty_entry.amount = Asset(deposit_op.amount);
				total_fee -= Asset(deposit_op.amount);
				break;
			}
			case withdraw_escrow_type:
			{
				auto deposit_es_op = deposit_op.condition.as<withdraw_with_escrow>();
				auto amount = Asset(deposit_op.amount, deposit_op.condition.asset_id);
				pretty_entry.to_account = (string)deposit_es_op.receiver;
				auto account_entry = my->_blockchain->get_account_by_address(pretty_entry.to_account);
				if (account_entry.valid())
				{
					pretty_entry.to_account_name = account_entry->name;
				}
				pretty_entry.amount = amount;
				total_fee -= amount;
				break;
			}
			default:
				break;
			}
		}
		default:
			break;

        }
    }



    for (const auto& op : trx.operations)
    {
        switch (OperationTypeEnum(op.type))
        {
        case register_account_op_type:
        {
            auto register_op = op.as<RegisterAccountOperation>();

			auto account_name_entry = my->_blockchain->get_account_entry(register_op.name);
			if (!account_name_entry.valid())
			{
				break;
			}
			pretty_entry.to_account = (string)account_name_entry->owner_address();
			pretty_entry.to_account_name = register_op.name;
			pretty_entry.amount = Asset(0); // Assume scan_withdraw came first

            if (register_op.delegate_pay_rate > 100)
            {
                pretty_entry.memo = "register account " + account_name_entry->name; // Can't tell if initially registered as a delegate
                pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::register_account_transaction;
            }
            else
            {
                pretty_entry.memo = "register delegate " + account_name_entry->name; // Can't tell if initially registered as a delegate
                pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::register_delegate_transaction;
            }

			break;
		}
		case update_account_op_type:
		{
			auto update_op = op.as<UpdateAccountOperation>();
			auto oaccount = my->_blockchain->get_account_entry(update_op.account_id);
			if (!oaccount.valid())
				break;
			auto account_name_entry = my->_blockchain->get_account_entry(oaccount->name);
			if (!account_name_entry.valid())
			{
				break;
			}
			pretty_entry.to_account = (string)account_name_entry->owner_address();
			pretty_entry.to_account_name = account_name_entry->name;
			pretty_entry.amount = Asset(0); // Assume scan_withdraw came first

            //这边无法通过受托率来区分出哪种交易是升级账户为代理，哪种交易是更新账户信息
            //因此只能考虑通过手续费来进行判断，但是手续费并不一定是一个固定的数额
            //所以现在只能简单粗暴得用一个1000ALP来进行划分，只要是大于1000ALP都认为是升级代理
            if (total_fee <= Asset( 1000 * ALP_BLOCKCHAIN_PRECISION ))
            {
                pretty_entry.memo = "update " + account_name_entry->name;
                pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::update_account_transaction;
            }
            else
            {
                pretty_entry.memo = "upgrade " + account_name_entry->name;
                pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::upgrade_account_transaction;
            }

			break;

		}
		case create_asset_op_type:
		{
			auto create_asset_op = op.as<CreateAssetOperation>();
			auto oissuer = my->_blockchain->get_account_entry(create_asset_op.issuer_account_id);
			pretty_entry.to_account = (string)oissuer->owner_address();
			auto account_entry = my->_blockchain->get_account_by_address(pretty_entry.to_account);
			if (account_entry.valid())
			{
				pretty_entry.to_account_name = account_entry->name;
			}
			pretty_entry.amount = Asset(0); // Assume scan_withdraw came first
			pretty_entry.memo = "create " + create_asset_op.symbol + " (" + create_asset_op.name + ")";
			break;
		}
		case update_asset_op_type:
			// TODO
			break;
		case issue_asset_op_type:
		{
			auto	issue_asset_op = op.as<IssueAssetOperation>();
			pretty_entry.amount = issue_asset_op.amount;
			pretty_entry.memo = "issue " + my->_blockchain->to_pretty_asset(issue_asset_op.amount);
			break;
		}

        default:
            break;
        }
    }
    for (const auto & op : trx.operations)
    {
        switch (op.type)
        {
        case imessage_memo_op_type:
        {
            auto message_op = op.as<ImessageMemoOperation>();
            op_memo = message_op.imessage;
            break;
        }
        default:
            break;
        }
    }
    if (!(op_memo.empty() || op_memo == ""))
        pretty_entry.memo = op_memo;

    pretty_trx.ledger_entries.push_back(pretty_entry);


    if (!pretty_trx.is_virtual && !pretty_trx.is_market)
    {
        uint16_t unknown_count = 0;
        uint16_t from_name_count = 0;
        string from_name;
        for (const PrettyLedgerEntry& entry : pretty_trx.ledger_entries)
        {
            if (entry.from_account == "UNKNOWN")
            {
                ++unknown_count;
            }
            else if (ChainInterface::is_valid_account_name(entry.from_account))
            {
                ++from_name_count;
                if (!from_name.empty() && entry.from_account != from_name)
                {
                    from_name_count = 0;
                    break;
                }
                from_name = entry.from_account;
            }
        }

        if (from_name_count > 0 && unknown_count > 0 && from_name_count + unknown_count == pretty_trx.ledger_entries.size())
        {
            for (PrettyLedgerEntry& entry : pretty_trx.ledger_entries)
                entry.from_account = from_name;
        }
    }
    if (addr_for_fee != "")
    {
        if (local_addr_for_fee == from_account_str || pretty_entry.from_account == "NETWORK")
            pretty_trx.fee = total_fee;
        else
            pretty_trx.fee = Asset(0, 0);
    }
    else
        pretty_trx.fee = total_fee;
    pretty_trx.timestamp = block_healder.timestamp;
    pretty_trx.expiration_timestamp = trx_entry.trx.expiration;

    return pretty_trx;


}


PrettyContractTransaction		Wallet::to_pretty_contract_trx(const thinkyoung::blockchain::TransactionEntry& trx_entry) const
{
    PrettyContractTransaction pretty_trx;
    SignedTransaction result_trx = trx_entry.trx;

    FC_ASSERT((trx_entry.trx.operations.size() > 0) && (trx_entry.trx.operations[0].type == thinkyoung::blockchain::OperationTypeEnum::transaction_op_type) , 
        "not result contract transaction");
    SignedTransaction orig_trx = trx_entry.trx.operations[0].as<thinkyoung::blockchain::TransactionOperation>().trx;

    TransactionIdType result_trx_id = result_trx.id();
    TransactionIdType orig_trx_id = orig_trx.id();

    FC_ASSERT((orig_trx.operations.size() > 0) && 
        ((orig_trx.operations[0].type >= fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(register_contract_op_type))
            && (orig_trx.operations[0].type <= fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(transfer_contract_op_type))), "invalid original contract transaction");


    thinkyoung::blockchain::OperationTypeEnum contract_op_type = orig_trx.operations[0].type;

    uint32_t block_num = trx_entry.chain_location.block_num;
    uint32_t block_position = trx_entry.chain_location.trx_num;
    auto block_healder = my->_blockchain->get_block_header(block_num);

    pretty_trx.block_num = block_num;
    pretty_trx.block_position = block_position;
    pretty_trx.result_trx_id = result_trx_id;
    pretty_trx.orig_trx_id = orig_trx_id;

    if (contract_op_type == thinkyoung::blockchain::OperationTypeEnum::register_contract_op_type)
    {
        pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::register_contract_transaction;

        PrettyContractLedgerEntry to_contract_ledger_entry;
        vector<PrettyContractLedgerEntry> from_contract_ledger_entries;

        thinkyoung::blockchain::RegisterContractOperation register_contract_op = orig_trx.operations[0].as<thinkyoung::blockchain::RegisterContractOperation>();
        Address contract_owner = thinkyoung::blockchain::Address(register_contract_op.owner);
        ContractIdType contract_id = register_contract_op.get_contract_id();

        to_contract_ledger_entry.from_account = contract_owner.AddressToString();
        to_contract_ledger_entry.to_account = contract_id.AddressToString(AddressType::contract_address);

        oAccountEntry account_entry = my->_blockchain->get_account_by_address(to_contract_ledger_entry.from_account);
        if (account_entry.valid())
            to_contract_ledger_entry.from_account_name = account_entry->name;

        oContractEntry contract_entry = my->_blockchain->get_contract_entry(contract_id);

        FC_ASSERT(contract_entry.valid(), "contract entry not valid");
                
        if (contract_entry.valid() && (contract_entry->level == ContractLevel::forever))
            to_contract_ledger_entry.to_account_name = contract_entry->contract_name;

        to_contract_ledger_entry.amount = Asset(ALP_DEFAULT_CONTRACT_MARGIN, 0);

        ShareType all_cost = 0;
        bool register_success = false;

        for (const auto& op : result_trx.operations)
        {
            // 注册的合约成功上链
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(contract_info_op_type))
                register_success = true;
            
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(balances_withdraw_op_type))
            {
                BalancesWithdrawOperation balance_withdraw_op = op.as<BalancesWithdrawOperation>();
                for (const auto& balance : balance_withdraw_op.balances)
                    all_cost = all_cost + balance.second;
            }
        }

        if (register_success)
        {
            to_contract_ledger_entry.fee = Asset(all_cost - ALP_DEFAULT_CONTRACT_MARGIN, 0);
            //to_contract_ledger_entry.memo = "register contract success";
            pretty_trx.is_completed = 0;
        }
        else
        {
            to_contract_ledger_entry.fee = Asset(all_cost, 0);
            //to_contract_ledger_entry.memo = "register contract not success";
            pretty_trx.is_completed = 1;
        }

        pretty_trx.to_contract_ledger_entry = to_contract_ledger_entry;
        // from_contract_ledger_entries 为空
        pretty_trx.from_contract_ledger_entries = from_contract_ledger_entries;
    }
    else if (contract_op_type == thinkyoung::blockchain::OperationTypeEnum::upgrade_contract_op_type)
    {
        pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::upgrade_contract_transaction;

        PrettyContractLedgerEntry to_contract_ledger_entry;
        vector<PrettyContractLedgerEntry> from_contract_ledger_entries;

        thinkyoung::blockchain::UpgradeContractOperation upgrade_contract_op = orig_trx.operations[0].as<thinkyoung::blockchain::UpgradeContractOperation>();
        ContractIdType contract_id = upgrade_contract_op.id;

        to_contract_ledger_entry.to_account = contract_id.AddressToString(AddressType::contract_address);

        oContractEntry contract_entry = my->_blockchain->get_contract_entry(contract_id);

        FC_ASSERT(contract_entry.valid(), "contract entry not valid");

        if (contract_entry.valid())
        {
            if (contract_entry->level == ContractLevel::forever)
                to_contract_ledger_entry.to_account_name = contract_entry->contract_name;

            to_contract_ledger_entry.from_account = Address(contract_entry->owner).AddressToString();
            oAccountEntry account_entry = my->_blockchain->get_account_by_address(to_contract_ledger_entry.from_account);
            if (account_entry.valid())
                to_contract_ledger_entry.from_account_name = account_entry->name;
        }

        //合约升级者的花费
        ShareType all_cost = 0;
        //合约的所有的出账金额
        ShareType withdraw_from_contract = 0;
        //从合约转账到普通账户中的金额
        ShareType deposit_to_account = 0;
        bool upgrade_success = false;

        for (const auto& op : result_trx.operations)
        {
            // 合约成功升级
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(on_upgrade_op_type))
                upgrade_success = true;

            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(balances_withdraw_op_type))
            {
                BalancesWithdrawOperation balance_withdraw_op = op.as<BalancesWithdrawOperation>();
                for (const auto& balance : balance_withdraw_op.balances)
                    all_cost = all_cost + balance.second;
            }

            // 合约出账(含保证金) 
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(withdraw_contract_op_type))
            {
                WithdrawContractOperation withdraw_contract_op = op.as<WithdrawContractOperation>();
                withdraw_from_contract = withdraw_from_contract + withdraw_contract_op.amount;
            }

            // 用户入账
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(deposit_op_type))
            {
                DepositOperation deposit_op = op.as<DepositOperation>();
                deposit_to_account = deposit_to_account + deposit_op.amount;

                // 向用户转账结果交易
                PrettyContractLedgerEntry from_contract_ledger_entry;
                from_contract_ledger_entry.from_account = contract_id.AddressToString(AddressType::contract_address);
                from_contract_ledger_entry.from_account_name = to_contract_ledger_entry.to_account_name;

                from_contract_ledger_entry.to_account = deposit_op.condition.owner()->AddressToString();
                oAccountEntry account_entry = my->_blockchain->get_account_by_address(from_contract_ledger_entry.to_account);
                if (account_entry.valid())
                    from_contract_ledger_entry.to_account_name = account_entry->name;

                from_contract_ledger_entry.amount = Asset(deposit_op.amount, 0);
                from_contract_ledger_entry.fee = Asset(0, 0);

                from_contract_ledger_entries.push_back(from_contract_ledger_entry);
            }
        }
        to_contract_ledger_entry.amount = Asset(0, 0);
        to_contract_ledger_entry.fee = Asset(all_cost, 0);

        if (upgrade_success)
        { 
            //to_contract_ledger_entry.memo = "upgrade contract success";
            pretty_trx.is_completed = 0;

            // 保证金出账结果交易
            ShareType withdraw_margin = withdraw_from_contract - deposit_to_account;
            PrettyContractLedgerEntry from_contract_ledger_entry;
            from_contract_ledger_entry.from_account = contract_id.AddressToString(AddressType::contract_address);
            from_contract_ledger_entry.from_account_name = to_contract_ledger_entry.to_account_name;

            from_contract_ledger_entry.to_account = to_contract_ledger_entry.from_account;
            oAccountEntry account_entry = my->_blockchain->get_account_by_address(from_contract_ledger_entry.to_account);
            if (account_entry.valid())
                from_contract_ledger_entry.to_account_name = account_entry->name;

            from_contract_ledger_entry.amount = Asset(0, 0);
            from_contract_ledger_entry.fee = Asset(withdraw_margin, 0);
            from_contract_ledger_entry.memo = "spend margin as fee";

            from_contract_ledger_entries.push_back(from_contract_ledger_entry);
        }
        else
        {
            //to_contract_ledger_entry.memo = "upgrade contract not success";
            pretty_trx.is_completed = 1;
        }
        
        pretty_trx.to_contract_ledger_entry = to_contract_ledger_entry;
        pretty_trx.from_contract_ledger_entries = from_contract_ledger_entries;
    }
    else if (contract_op_type == thinkyoung::blockchain::OperationTypeEnum::destroy_contract_op_type)
    {
        pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::destroy_contract_transaction;

        PrettyContractLedgerEntry to_contract_ledger_entry;
        vector<PrettyContractLedgerEntry> from_contract_ledger_entries;

        thinkyoung::blockchain::DestroyContractOperation destroy_contract_op = orig_trx.operations[0].as<thinkyoung::blockchain::DestroyContractOperation>();
        ContractIdType contract_id = destroy_contract_op.id;

        to_contract_ledger_entry.to_account = contract_id.AddressToString(AddressType::contract_address);

        oContractEntry contract_entry = my->_blockchain->get_contract_entry(contract_id);

        FC_ASSERT(contract_entry.valid(), "contract entry not valid");

        if (contract_entry.valid())
        {
            to_contract_ledger_entry.from_account = Address(contract_entry->owner).AddressToString();
            oAccountEntry account_entry = my->_blockchain->get_account_by_address(to_contract_ledger_entry.from_account);
            if (account_entry.valid())
                to_contract_ledger_entry.from_account_name = account_entry->name;
        }

        //合约销毁者的花费
        ShareType all_cost = 0;
        bool destroy_success = false;

        for (const auto& op : result_trx.operations)
        {
            // 合约成功销毁
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(on_destroy_op_type))
            {
                destroy_success = true;

                // 合约账户中on_destroy没有退完的余额退还给合约所有者  
                OnDestroyOperation on_destroy_op = op.as<OnDestroyOperation>();

                if (on_destroy_op.amount.amount > 0)
                {
                    PrettyContractLedgerEntry from_contract_ledger_entry;
                    from_contract_ledger_entry.from_account = contract_id.AddressToString(AddressType::contract_address);
                    from_contract_ledger_entry.from_account_name = to_contract_ledger_entry.to_account_name;

                    from_contract_ledger_entry.to_account = to_contract_ledger_entry.from_account;
                    oAccountEntry account_entry = my->_blockchain->get_account_by_address(from_contract_ledger_entry.to_account);
                    if (account_entry.valid())
                        from_contract_ledger_entry.to_account_name = account_entry->name;

                    from_contract_ledger_entry.amount = on_destroy_op.amount;
                    from_contract_ledger_entry.fee = Asset(0, 0);
                    from_contract_ledger_entry.memo = "the left amount pay back to owner";

                    from_contract_ledger_entries.push_back(from_contract_ledger_entry);
                }
            }

            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(balances_withdraw_op_type))
            {
                BalancesWithdrawOperation balance_withdraw_op = op.as<BalancesWithdrawOperation>();
                for (const auto& balance : balance_withdraw_op.balances)
                    all_cost = all_cost + balance.second;
            }

            // 用户入账(含退还保证金)
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(deposit_op_type))
            {
                DepositOperation deposit_op = op.as<DepositOperation>();

                // 向用户转账结果交易
                PrettyContractLedgerEntry from_contract_ledger_entry;
                from_contract_ledger_entry.from_account = contract_id.AddressToString(AddressType::contract_address);
                from_contract_ledger_entry.from_account_name = to_contract_ledger_entry.to_account_name;

                from_contract_ledger_entry.to_account = deposit_op.condition.owner()->AddressToString();
                oAccountEntry account_entry = my->_blockchain->get_account_by_address(from_contract_ledger_entry.to_account);
                if (account_entry.valid())
                    from_contract_ledger_entry.to_account_name = account_entry->name;

                from_contract_ledger_entry.amount = Asset(deposit_op.amount, 0);
                from_contract_ledger_entry.fee = Asset(0, 0);

                from_contract_ledger_entries.push_back(from_contract_ledger_entry);
            }
        }
        to_contract_ledger_entry.amount = Asset(0, 0);
        to_contract_ledger_entry.fee = Asset(all_cost, 0);

        if (destroy_success)
        {
            //to_contract_ledger_entry.memo = "destroy contract success";
            pretty_trx.is_completed = 0;
        }
        else
        {
            //to_contract_ledger_entry.memo = "destroy contract not success";
            pretty_trx.is_completed = 1;
        }

        pretty_trx.to_contract_ledger_entry = to_contract_ledger_entry;
        pretty_trx.from_contract_ledger_entries = from_contract_ledger_entries;
    }
    else if (contract_op_type == thinkyoung::blockchain::OperationTypeEnum::call_contract_op_type)
    {
        pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::call_contract_transaction;

        PrettyContractLedgerEntry to_contract_ledger_entry;
        vector<PrettyContractLedgerEntry> from_contract_ledger_entries;

        thinkyoung::blockchain::CallContractOperation call_contract_op = orig_trx.operations[0].as<thinkyoung::blockchain::CallContractOperation>();
        Address contract_caller = thinkyoung::blockchain::Address(call_contract_op.caller);
        ContractIdType contract_id = call_contract_op.contract;

        //当交易为调用合约交易时，记录下调用合约的方法与调用合约传入的参数
        pretty_trx.reserved.clear();
        pretty_trx.reserved.push_back(call_contract_op.method);
        pretty_trx.reserved.push_back(call_contract_op.args);

        to_contract_ledger_entry.from_account = contract_caller.AddressToString();
        to_contract_ledger_entry.to_account = contract_id.AddressToString(AddressType::contract_address);

        oAccountEntry account_entry = my->_blockchain->get_account_by_address(to_contract_ledger_entry.from_account);
        if (account_entry.valid())
            to_contract_ledger_entry.from_account_name = account_entry->name;

        oContractEntry contract_entry = my->_blockchain->get_contract_entry(contract_id);

        FC_ASSERT(contract_entry.valid(), "contract entry not valid");

        if (contract_entry.valid() && (contract_entry->level == ContractLevel::forever))
            to_contract_ledger_entry.to_account_name = contract_entry->contract_name;

        //合约调用者的花费
        ShareType all_cost = 0;
        bool call_success = false;

        for (const auto& op : result_trx.operations)
        {
            // 合约成功调用
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(on_call_success_op_type))
                call_success = true;

            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(balances_withdraw_op_type))
            {
                BalancesWithdrawOperation balance_withdraw_op = op.as<BalancesWithdrawOperation>();
                for (const auto& balance : balance_withdraw_op.balances)
                    all_cost = all_cost + balance.second;
            }

            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(deposit_op_type))
            {
                DepositOperation deposit_op = op.as<DepositOperation>();

                // 向用户转账结果交易
                PrettyContractLedgerEntry from_contract_ledger_entry;
                from_contract_ledger_entry.from_account = contract_id.AddressToString(AddressType::contract_address);
                from_contract_ledger_entry.from_account_name = to_contract_ledger_entry.to_account_name;

                from_contract_ledger_entry.to_account = deposit_op.condition.owner()->AddressToString();
                oAccountEntry account_entry = my->_blockchain->get_account_by_address(from_contract_ledger_entry.to_account);
                if (account_entry.valid())
                    from_contract_ledger_entry.to_account_name = account_entry->name;

                from_contract_ledger_entry.amount = Asset(deposit_op.amount, 0);
                from_contract_ledger_entry.fee = Asset(0, 0);

                from_contract_ledger_entries.push_back(from_contract_ledger_entry);
            }
        }

        to_contract_ledger_entry.amount = Asset(0, 0);
        to_contract_ledger_entry.fee = Asset(all_cost, 0);

        if (call_success)
        {
            //to_contract_ledger_entry.memo = "call contract success";
            pretty_trx.is_completed = 0;
        }
        else
        {
            //to_contract_ledger_entry.memo = "call contract not success";
            pretty_trx.is_completed = 1;
        }

        pretty_trx.to_contract_ledger_entry = to_contract_ledger_entry;
        pretty_trx.from_contract_ledger_entries = from_contract_ledger_entries;
    }
    else if (contract_op_type == thinkyoung::blockchain::OperationTypeEnum::transfer_contract_op_type)
    {
        pretty_trx.trx_type = thinkyoung::blockchain::TransactionType::transfer_contract_transaction;

        PrettyContractLedgerEntry to_contract_ledger_entry;
        vector<PrettyContractLedgerEntry> from_contract_ledger_entries;

        thinkyoung::blockchain::TransferContractOperation transfer_contract_op = orig_trx.operations[0].as<thinkyoung::blockchain::TransferContractOperation>();
        Address contract_depositer = thinkyoung::blockchain::Address(transfer_contract_op.from);
        ContractIdType contract_id = transfer_contract_op.contract_id;

        to_contract_ledger_entry.from_account = contract_depositer.AddressToString();
        to_contract_ledger_entry.to_account = contract_id.AddressToString(AddressType::contract_address);

        oAccountEntry account_entry = my->_blockchain->get_account_by_address(to_contract_ledger_entry.from_account);
        if (account_entry.valid())
            to_contract_ledger_entry.from_account_name = account_entry->name;

        oContractEntry contract_entry = my->_blockchain->get_contract_entry(contract_id);

        FC_ASSERT(contract_entry.valid(), "contract entry not valid");

        if (contract_entry.valid() && (contract_entry->level == ContractLevel::forever))
            to_contract_ledger_entry.to_account_name = contract_entry->contract_name;
       
        ShareType transfer_amount = transfer_contract_op.transfer_amount.amount;

        //合约转账者的花费
        ShareType all_cost = 0;
        bool transfer_success = false;

        for (const auto& op : result_trx.operations)
        {
            // 向合约成功充值
            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(deposit_contract_op_type))
                transfer_success = true;

            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(balances_withdraw_op_type))
            {
                BalancesWithdrawOperation balance_withdraw_op = op.as<BalancesWithdrawOperation>();
                for (const auto& balance : balance_withdraw_op.balances)
                    all_cost = all_cost + balance.second;
            }

            if (op.type == fc::enum_type<uint8_t, thinkyoung::blockchain::OperationTypeEnum>(deposit_op_type))
            {
                DepositOperation deposit_op = op.as<DepositOperation>();

                // 向用户转账结果交易
                PrettyContractLedgerEntry from_contract_ledger_entry;
                from_contract_ledger_entry.from_account = contract_id.AddressToString(AddressType::contract_address);
                from_contract_ledger_entry.from_account_name = to_contract_ledger_entry.to_account_name;

                from_contract_ledger_entry.to_account = deposit_op.condition.owner()->AddressToString();
                oAccountEntry account_entry = my->_blockchain->get_account_by_address(from_contract_ledger_entry.to_account);
                if (account_entry.valid())
                    from_contract_ledger_entry.to_account_name = account_entry->name;

                from_contract_ledger_entry.amount = Asset(deposit_op.amount, 0);
                from_contract_ledger_entry.fee = Asset(0, 0);

                from_contract_ledger_entries.push_back(from_contract_ledger_entry);
            }
        }
        
        all_cost = all_cost - transfer_amount;
        to_contract_ledger_entry.amount = Asset(transfer_amount, 0);
        to_contract_ledger_entry.fee = Asset(all_cost, 0);

        if (transfer_success)
        {
            //to_contract_ledger_entry.memo = "transfer to contract success";
            pretty_trx.is_completed = 0;
        }
        else
        {
            //to_contract_ledger_entry.memo = "transfer to contract not success";
            pretty_trx.is_completed = 1;
        }

        pretty_trx.to_contract_ledger_entry = to_contract_ledger_entry;
        pretty_trx.from_contract_ledger_entries = from_contract_ledger_entries;
    }

    pretty_trx.timestamp = block_healder.timestamp;
    pretty_trx.expiration_timestamp = trx_entry.trx.expiration;

    return pretty_trx;
}


WalletTransactionEntry Wallet::get_transaction(const string& transaction_id_prefix)const
{
    FC_ASSERT(is_open(), "Wallet not open!");

    if (transaction_id_prefix.size() > string(TransactionIdType()).size())
        FC_THROW_EXCEPTION(invalid_transaction_id, "Invalid transaction ID!", ("transaction_id_prefix", transaction_id_prefix));

    const auto& items = my->_wallet_db.get_transactions();
    for (const auto& item : items)
    {
        if (string(item.first).find(transaction_id_prefix) == 0)
            return item.second;
    }

    FC_THROW_EXCEPTION(transaction_not_found, "Transaction not found!", ("transaction_id_prefix", transaction_id_prefix));
}

AccountBalanceSummaryType Wallet::compute_historic_balance(const string &account_name,
    uint32_t block_num)const
{
    try {
        const vector<PrettyTransaction> ledger = get_pretty_transaction_history(account_name,
            0, block_num,
            "");
        map<string, map<AssetIdType, ShareType>> balances;

        for (const auto& trx : ledger)
        {
            for (const auto& entry : trx.ledger_entries)
            {
                for (const auto &account_balances : entry.running_balances)
                {
                    const string name = account_balances.first;
                    for (const auto &balance : account_balances.second)
                    {
                        if (balance.second.amount == 0)
                        {
                            balances[name].erase(balance.first);
                        }
                        else {
                            balances[name][balance.first] = balance.second.amount;
                        }
                    }
                }
            }
        }

        return balances;
    } FC_CAPTURE_AND_RETHROW((account_name)(block_num))
}
