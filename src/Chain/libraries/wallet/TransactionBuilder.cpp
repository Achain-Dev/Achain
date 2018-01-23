#include <wallet/Config.hpp>
#include <wallet/Exceptions.hpp>
#include <wallet/Wallet.hpp>
#include <wallet/WalletImpl.hpp>


#include <blockchain/Time.hpp>

#include <fc/crypto/sha256.hpp>

using namespace thinkyoung::wallet;
using namespace thinkyoung::wallet::detail;

void  TransactionBuilder::set_wallet_implementation(std::unique_ptr<thinkyoung::wallet::detail::WalletImpl>& wimpl)
{
    _wimpl = wimpl.get();
}

PublicKeyType TransactionBuilder::order_key_for_account(const Address& account_address, const string& account_name)
{
    auto order_key = order_keys[account_address];
    if (order_key == PublicKeyType())
    {
        order_key = _wimpl->get_new_public_key(account_name);
        order_keys[account_address] = order_key;
    }
    return order_key;
}

TransactionBuilder& TransactionBuilder::release_escrow(const AccountEntry& payer,
    const Address& escrow_account,
    const Address& released_by_address,
    ShareType     amount_to_sender,
    ShareType     amount_to_receiver)
{
    try {
        auto escrow_entry = _wimpl->_blockchain->get_balance_entry(escrow_account);
        FC_ASSERT(escrow_entry.valid());

        auto escrow_condition = escrow_entry->condition.as<withdraw_with_escrow>();

        //deduct_balance( released_by_address, _wimpl->self->get_transaction_fee() );
        // TODO: this is a hack to bypass finalize() call...
        _wimpl->withdraw_to_transaction(_wimpl->self->get_transaction_fee(),
            payer.name,
            trx,
            required_signatures);
        // fetch balance entry, assert that released_by_address is a party to the contract
        trx.release_escrow(escrow_account, released_by_address, amount_to_sender, amount_to_receiver);
        if (released_by_address == Address())
        {
            required_signatures.insert(escrow_condition.sender);
            required_signatures.insert(escrow_condition.receiver);
        }
        else
        {
            required_signatures.insert(released_by_address);
        }
        if (trx.expiration == time_point_sec())
            trx.expiration = blockchain::now() + _wimpl->self->get_transaction_expiration();

        transaction_entry.entry_id = trx.id();
        transaction_entry.created_time = blockchain::now();
        transaction_entry.received_time = transaction_entry.created_time;
        return *this;
    } FC_CAPTURE_AND_RETHROW((payer)(escrow_account)(released_by_address)(amount_to_sender)(amount_to_receiver))
}

TransactionBuilder&TransactionBuilder::register_account(const string& name,
    optional<variant> public_data,
    PublicKeyType owner_key,
    optional<PublicKeyType> active_key,
    optional<uint8_t> delegate_pay,
    optional<AccountMetaInfo> meta_info,
    optional<WalletAccountEntry> paying_account)
{
    try {
        if (!public_data) public_data = variant(fc::variant_object());
        if (!active_key) active_key = owner_key;
        if (!delegate_pay) delegate_pay = -1;

        if (paying_account) deduct_balance(paying_account->owner_address(), Asset());

        trx.register_account(name, *public_data, owner_key, *active_key, *delegate_pay, meta_info);

        LedgerEntry entry;
        entry.from_account = paying_account->owner_key;
        entry.to_account = owner_key;
        entry.memo = "Register account: " + name;
        transaction_entry.ledger_entries.push_back(entry);

        return *this;
    } FC_CAPTURE_AND_RETHROW((name)(public_data)(owner_key)(active_key)(delegate_pay)(paying_account))
}

TransactionBuilder& TransactionBuilder::update_account_registration(const WalletAccountEntry& account,
    optional<variant> public_data,
    optional<PublicKeyType> active_key,
    optional<uint8_t> delegate_pay,
    optional<WalletAccountEntry> paying_account)
{
    if (account.registration_date == fc::time_point_sec())
        FC_THROW_EXCEPTION(unknown_account, "Account is not registered! Cannot update registration.");
    FC_ASSERT(public_data || active_key || delegate_pay, "Nothing to do!");

    //Check at the beginning that we actually have the keys required to sign this thing.
    //Work on a copy so if we fail later, we haven't changed required_signatures.
    auto working_required_signatures = required_signatures;
    _wimpl->authorize_update(working_required_signatures, account, active_key.valid());

    if (!paying_account)
        paying_account = account;

    //Add paying_account to the transactions set of balance holders; he may be liable for the transaction fee.
    deduct_balance(paying_account->owner_address(), Asset());

    if (delegate_pay)
    {
        FC_ASSERT(!account.is_delegate() ||
            *delegate_pay <= account.delegate_pay_rate(), "Pay rate can only be decreased!");

        //If account is not a delegate but wants to become one OR account is a delegate changing his pay rate...
        if ((!account.is_delegate() && *delegate_pay <= 100) ||
            (account.is_delegate() && *delegate_pay != account.delegate_pay_rate()))
        {
            if (!paying_account->is_my_account)
                FC_THROW_EXCEPTION(unknown_account, "Unknown paying account!", ("paying_account", paying_account));

            Asset fee(_wimpl->_blockchain->get_delegate_registration_fee(*delegate_pay));
            if (paying_account->is_delegate() && paying_account->delegate_pay_balance() >= fee.amount)
            {
                //Withdraw into trx, but don't entry it in outstanding_balances because it's a fee
                trx.withdraw_pay(paying_account->id, fee.amount);
                working_required_signatures.insert(paying_account->active_key());
            }
            else
                deduct_balance(paying_account->owner_address(), fee);

            LedgerEntry entry;
            entry.from_account = paying_account->owner_key;
            entry.to_account = account.owner_key;
            transaction_entry.fee = fee;
            //entry.amount = fee;
            if (account.is_delegate())
                entry.memo = "Fee to update " + account.name + "'s delegate pay";
            else
                entry.memo = "Fee to promote " + account.name + " to a delegate";
            transaction_entry.ledger_entries.push_back(entry);
        }
    }
    else delegate_pay = account.delegate_pay_rate();

    fc::optional<PublicKeyType> active_public_key;
    if (active_key)
    {
        if (_wimpl->_blockchain->get_account_entry(*active_key).valid() ||
            _wimpl->_wallet_db.lookup_account(*active_key).valid())
            FC_THROW_EXCEPTION(key_already_registered, "Key already belongs to another account!", ("new_public_key", active_key));

        LedgerEntry entry;
        entry.from_account = paying_account->owner_key;
        entry.to_account = account.owner_key;
        entry.memo = "Update " + account.name + "'s active key";
        transaction_entry.ledger_entries.push_back(entry);
    }

    trx.update_account(account.id, *delegate_pay, public_data, active_key);

    if (public_data)
    {
        LedgerEntry entry;
        entry.from_account = paying_account->owner_key;
        entry.to_account = account.owner_key;
        entry.memo = "Update " + account.name + "'s public data";
        transaction_entry.ledger_entries.push_back(entry);
    }

    required_signatures = working_required_signatures;
    return *this;
}

TransactionBuilder& TransactionBuilder::deposit_asset(const thinkyoung::wallet::WalletAccountEntry& payer,
    const thinkyoung::blockchain::AccountEntry& recipient,
    const Asset& amount,
    const string& memo,
    fc::optional<string> memo_sender,
    const string& alp_account)
{
    try {
        if (recipient.is_retracted())
            FC_CAPTURE_AND_THROW(account_retracted, (recipient));

        if (amount.amount <= 0)
            FC_THROW_EXCEPTION(invalid_asset_amount, "Cannot deposit a negative amount!");

        // Don't automatically truncate memos as long as users still depend on them via deposit ops rather than mail
        if (memo.size() > ALP_BLOCKCHAIN_MAX_EXTENDED_MEMO_SIZE)
            FC_CAPTURE_AND_THROW(memo_too_long, (memo));

        if (!memo_sender.valid())
            memo_sender = payer.name;
        const oWalletAccountEntry memo_account = _wimpl->_wallet_db.lookup_account(*memo_sender);
        FC_ASSERT(memo_account.valid() && memo_account->is_my_account);

        PublicKeyType memo_key = memo_account->owner_key;
        if (!_wimpl->_wallet_db.has_private_key(memo_key))
            memo_key = memo_account->active_key();
        FC_ASSERT(_wimpl->_wallet_db.has_private_key(memo_key));
        if (alp_account != "")
        {
            //trx.from_account = (*memo_sender);
            trx.alp_account = alp_account;
            trx.alp_inport_asset = amount;
        }
        optional<PublicKeyType> titan_one_time_key;
        auto one_time_key = _wimpl->get_new_private_key(payer.name);
        titan_one_time_key = one_time_key.get_public_key();
        trx.deposit_to_account(recipient.active_key(), amount, _wimpl->self->get_private_key(memo_key), memo,
            memo_key, one_time_key, from_memo, recipient.is_titan_account());

        deduct_balance(payer.owner_key, amount);

        LedgerEntry entry;
        entry.from_account = payer.owner_key;
        entry.to_account = recipient.owner_key;
        entry.amount = amount;
        entry.memo = memo;
        if (*memo_sender != payer.name) entry.memo_from_account = memo_account->owner_key;
        transaction_entry.ledger_entries.push_back(std::move(entry));

        auto memo_signature = _wimpl->self->get_private_key(memo_key).sign_compact(fc::sha256::hash(memo.data(),
            memo.size()));
        /*notices.emplace_back(std::make_pair(mail::transaction_notice_message(string(memo),
                                                                             std::move(titan_one_time_key),
                                                                             std::move(memo_signature)),
                                                                             recipient.active_key()));*/

        return *this;
    } FC_CAPTURE_AND_RETHROW((recipient)(amount)(memo))
}


TransactionBuilder& TransactionBuilder::deposit_asset_to_address(const WalletAccountEntry& payer,
    const Address& to_addr,
    const Asset& amount,
    const string& memo)
{
    try {
        if (amount.amount <= 0)
            FC_THROW_EXCEPTION(invalid_asset_amount, "Cannot deposit a negative amount!");

        trx.deposit(to_addr, amount);
        deduct_balance(payer.owner_key, amount);

        LedgerEntry entry;
        entry.from_account = payer.owner_key;
        entry.amount = amount;
        entry.memo = memo;
        transaction_entry.ledger_entries.push_back(std::move(entry));

        return *this;
    } FC_CAPTURE_AND_RETHROW((to_addr)(amount)(memo))
}

TransactionBuilder& TransactionBuilder::deposit_asset_to_multisig(
    const Asset& amount,
    const string& from_name,
    uint32_t m,
    const vector<Address>& addresses)
{
    try {
        if (amount.amount <= 0)
            FC_THROW_EXCEPTION(invalid_asset_amount, "Cannot deposit a negative amount!");

        auto payer = _wimpl->_wallet_db.lookup_account(from_name);
        auto fee = _wimpl->self->get_transaction_fee();
        FC_ASSERT(payer.valid(), "No such account");
        MultisigMetaInfo info;
        info.required = m;
        info.owners = set<Address>(addresses.begin(), addresses.end());
        trx.deposit_multisig(info, amount);

        deduct_balance(payer->owner_key, amount + fee);

        LedgerEntry entry;
        entry.from_account = payer->owner_key;
        entry.amount = amount;
        transaction_entry.ledger_entries.push_back(std::move(entry));

        return *this;
    } FC_CAPTURE_AND_RETHROW((from_name)(addresses)(amount))
}

TransactionBuilder& TransactionBuilder::deposit_asset_with_escrow(const thinkyoung::wallet::WalletAccountEntry& payer,
    const thinkyoung::blockchain::AccountEntry& recipient,
    const thinkyoung::blockchain::AccountEntry& escrow_agent,
    DigestType agreement,
    const Asset& amount,
    const string& memo,
    fc::optional<PublicKeyType> memo_sender,
    const string& alp_account)
{
    try {
        if (recipient.is_retracted())
            FC_CAPTURE_AND_THROW(account_retracted, (recipient));

        if (escrow_agent.is_retracted())
            FC_CAPTURE_AND_THROW(account_retracted, (escrow_agent));

        if (amount.amount <= 0)
            FC_THROW_EXCEPTION(invalid_asset_amount, "Cannot deposit a negative amount!");

        // Don't automatically truncate memos as long as users still depend on them via deposit ops rather than mail
        if (memo.size() > ALP_BLOCKCHAIN_MAX_MEMO_SIZE)
            FC_CAPTURE_AND_THROW(memo_too_long, (memo));

        if (!memo_sender.valid())
            memo_sender = payer.active_key();

        optional<PublicKeyType> titan_one_time_key;
        if (alp_account != "")
        {
            trx.alp_account = alp_account;
            trx.alp_inport_asset = amount;
        }
        if (!recipient.is_titan_account())
        {
            // TODO: user public active receiver key...
        }
        else {
            auto one_time_key = _wimpl->get_new_private_key(payer.name);
            titan_one_time_key = one_time_key.get_public_key();
            auto receiver_key = trx.deposit_to_escrow(
                recipient.active_key(),
                escrow_agent.active_key(),
                agreement,
                amount,
                _wimpl->self->get_private_key(*memo_sender),
                memo,
                *memo_sender,
                one_time_key,
                from_memo);

            KeyData data;
            data.account_address = recipient.owner_address();
            data.public_key = receiver_key;
            data.memo = memo;
            _wimpl->_wallet_db.store_key(data);
        }

        deduct_balance(payer.owner_key, amount);

        LedgerEntry entry;
        entry.from_account = payer.owner_key;
        entry.to_account = recipient.owner_key;
        entry.amount = amount;
        entry.memo = memo;
        if (*memo_sender != payer.active_key())
            entry.memo_from_account = *memo_sender;
        transaction_entry.ledger_entries.push_back(std::move(entry));

        auto memo_signature = _wimpl->self->get_private_key(*memo_sender).sign_compact(fc::sha256::hash(memo.data(),
            memo.size()));


        return *this;
    } FC_CAPTURE_AND_RETHROW((recipient)(amount)(memo))
}




TransactionBuilder& TransactionBuilder::update_signing_key(const string& authorizing_account_name,
    const string& delegate_name,
    const PublicKeyType& signing_key)
{
    try {
        const oWalletAccountEntry authorizing_account_entry = _wimpl->_wallet_db.lookup_account(authorizing_account_name);
        if (!authorizing_account_entry.valid())
            FC_THROW_EXCEPTION(unknown_account, "Unknown authorizing account name!");

        const oAccountEntry delegate_entry = _wimpl->_blockchain->get_account_entry(delegate_name);
        if (!delegate_entry.valid())
            FC_THROW_EXCEPTION(unknown_account, "Unknown delegate account name!");

        trx.update_signing_key(delegate_entry->id, signing_key);
        deduct_balance(authorizing_account_entry->owner_address(), Asset());

        LedgerEntry entry;
        entry.from_account = authorizing_account_entry->active_key();
        entry.to_account = delegate_entry->owner_key;
        entry.memo = "update block signing key";

        transaction_entry.ledger_entries.push_back(entry);

        required_signatures.insert(authorizing_account_entry->active_key());
        return *this;
    } FC_CAPTURE_AND_RETHROW((authorizing_account_name)(delegate_name)(signing_key))
}

TransactionBuilder& TransactionBuilder::update_asset(const string& symbol,
    const optional<string>& name,
    const optional<string>& description,
    const optional<variant>& public_data,
    const optional<double>& maximum_share_supply,
    const optional<uint64_t>& precision,
    const ShareType issuer_fee,
    double market_fee,
    uint32_t flags,
    uint32_t issuer_perms,
    const optional<AccountIdType> issuer_account_id,
    uint32_t required_sigs,
    const vector<Address>& authority
    )
{
    try {
        const oAssetEntry asset_entry = _wimpl->_blockchain->get_asset_entry(symbol);
        FC_ASSERT(asset_entry.valid());

        const oAccountEntry issuer_account_entry = _wimpl->_blockchain->get_account_entry(asset_entry->issuer_account_id);
        if (!issuer_account_entry.valid())
            FC_THROW_EXCEPTION(unknown_account, "Unknown issuer account id!");

        AccountIdType new_issuer_account_id;
        if (issuer_account_id.valid())
            new_issuer_account_id = *issuer_account_id;
        else
            new_issuer_account_id = asset_entry->issuer_account_id;

        uint16_t actual_market_fee = uint16_t(-1);
        if (market_fee >= 0 || market_fee <= ALP_BLOCKCHAIN_MAX_UIA_MARKET_FEE)
            actual_market_fee = ALP_BLOCKCHAIN_MAX_UIA_MARKET_FEE * market_fee;

        trx.update_asset_ext(asset_entry->id, name, description, public_data, maximum_share_supply, precision,
            issuer_fee, actual_market_fee, flags, issuer_perms, new_issuer_account_id, required_sigs, authority);
        deduct_balance(issuer_account_entry->active_key(), Asset());

        LedgerEntry entry;
        entry.from_account = issuer_account_entry->active_key();
        entry.to_account = issuer_account_entry->active_key();
        entry.memo = "update " + symbol + " asset";

        transaction_entry.ledger_entries.push_back(entry);

        ilog("@n adding authority to required signatures: ${a}", ("a", asset_entry->authority));
        for (auto owner : asset_entry->authority.owners)
            required_signatures.insert(owner);
        return *this;
    } FC_CAPTURE_AND_RETHROW((symbol)(name)(description)(public_data)(maximum_share_supply)(precision))
}

TransactionBuilder& TransactionBuilder::finalize(const bool pay_fee, const VoteStrategy strategy)
{
    try {
        FC_ASSERT(!trx.operations.empty(), "Cannot finalize empty transaction");

        if (pay_fee)
            this->pay_fee();

        //outstanding_balance is pair<pair<account address, asset ID>, share_type>
        for (const auto& outstanding_balance : outstanding_balances)
        {
            Asset balance(outstanding_balance.second, outstanding_balance.first.second);
            string account_name = _wimpl->_wallet_db.lookup_account(outstanding_balance.first.first)->name;

            if (balance.amount > 0)
            {
                const Address deposit_address = order_key_for_account(outstanding_balance.first.first, account_name);
                trx.deposit(deposit_address, balance);
            }
            else if (balance.amount < 0)
            {
                _wimpl->withdraw_to_transaction(-balance, account_name, trx, required_signatures);
            }
        }

        if (trx.expiration == time_point_sec())
            trx.expiration = blockchain::now() + _wimpl->self->get_transaction_expiration();

        _wimpl->set_delegate_slate(trx, strategy);

        transaction_entry.entry_id = trx.id();
        transaction_entry.created_time = blockchain::now();
        transaction_entry.received_time = transaction_entry.created_time;

        return *this;
    } FC_CAPTURE_AND_RETHROW((trx))
}

WalletTransactionEntry& TransactionBuilder::sign()
{
    try {
        const auto chain_id = _wimpl->_blockchain->get_chain_id();

        for (const auto& address : required_signatures)
        {
            //Ignore exceptions; this function operates on a best-effort basis, and doesn't actually have to succeed.
            try {
                ilog("@n trying to sign for address ${a}", ("a", address));
                trx.sign(_wimpl->self->get_private_key(address), chain_id);
                ilog("@n    and I succeeded");
            }
            catch (const fc::exception& e)
            {
                wlog("unable to sign for address ${a}:\n${e}", ("a", address)("e", e.to_detail_string()));
                ilog("@n unable to sign for address ${a}:\n${e}", ("a", address)("e", e.to_detail_string()));
            }
        }


        return transaction_entry;
    } FC_CAPTURE_AND_RETHROW()
}


// First handles a margin position close and the collateral is returned.  Calls withdraw_fee() to
// to handle the more common cases.
void TransactionBuilder::pay_fee()
{
    try {
        auto available_balances = all_positive_balances();
        Asset required_fee(0, -1);

        //Choose an asset capable of paying the fee
        for (auto itr = available_balances.begin(); itr != available_balances.end(); ++itr)
            if (_wimpl->self->asset_can_pay_fee(itr->first) &&
                itr->second >= _wimpl->self->get_transaction_fee(itr->first).amount)
            {
                required_fee = _wimpl->self->get_transaction_fee(itr->first);
                if (transaction_entry.fee.amount == _wimpl->_blockchain->get_delegate_registration_fee(100))
                {
                    transaction_entry.fee += required_fee;
                }
                else
                {
                    transaction_entry.fee = required_fee;
                }
                break;
            }

        if (required_fee.asset_id != -1)
        {
            if (transaction_entry.fee.amount == _wimpl->_blockchain->get_delegate_registration_fee(100))
            {
                transaction_entry.fee += required_fee;
            }
            else
            {
                transaction_entry.fee = required_fee;
            }
            //outstanding_balance is pair<pair<account address, asset ID>, share_type>
            for (auto& outstanding_balance : outstanding_balances)
            {
                if (outstanding_balance.first.second != required_fee.asset_id)
                    //Not the right asset type
                    continue;
                if (required_fee.amount > outstanding_balance.second)
                {
                    //Balance can't cover the fee. Eat it and look for more.
                    required_fee.amount -= outstanding_balance.second;
                    outstanding_balance.second = 0;
                    continue;
                }
                //We have enough to pay the fee. Pay it and let's get out of here.
                outstanding_balance.second -= required_fee.amount;
                return;
            }
        }
        else if (withdraw_fee()) return;

        FC_THROW("Unable to pay fee; no remaining balances can cover it and no account can pay it.");
    } FC_RETHROW_EXCEPTIONS(warn, "All balances: ${bals}", ("bals", outstanding_balances))
}


TransactionBuilder& TransactionBuilder::withdraw_from_balance(const BalanceIdType& from, const ShareType amount)
{
    try {
        // TODO ledger entries
        auto obalance = _wimpl->_blockchain->get_balance_entry(from);
        if (obalance.valid())
        {
            trx.withdraw(from, amount);
            for (const auto& owner : obalance->owners())
                required_signatures.insert(owner);
        }
        else // We go ahead and try to use this balance ID as an owner
        {
            auto balances = _wimpl->_blockchain->get_balances_for_address(Address(from));
            FC_ASSERT(balances.size() > 0, "No balance with that ID or owner address!");
            auto balance = balances.begin()->second;
            trx.withdraw(balance.id(), amount);
            for (const auto& owner : balance.owners())
                required_signatures.insert(owner);
        }
        return *this;
    } FC_CAPTURE_AND_RETHROW((from)(amount))
}

TransactionBuilder& TransactionBuilder::deposit_to_balance(const BalanceIdType& to,
    const Asset& amount)
{
    // TODO ledger entries
    trx.deposit(to, amount);
    return *this;
}



//Most common case where the fee gets paid
//Called when pay_fee doesn't find a positive balance in the trx to pay the fee with
bool TransactionBuilder::withdraw_fee()
{
    const auto balances = _wimpl->self->get_spendable_account_balances();

    //Shake 'em down
    for (const auto& item : outstanding_balances)
    {
        const Address& bag_holder = item.first.first;

        //Got any lunch money?
        const oWalletAccountEntry account_rec = _wimpl->_wallet_db.lookup_account(bag_holder);
        if (!account_rec || balances.count(account_rec->name) == 0)
            continue;

        //Well how much?
        const map<AssetIdType, ShareType>& account_balances = balances.at(account_rec->name);
        for (const auto& balance_item : account_balances)
        {
            const Asset balance(balance_item.second, balance_item.first);
            const Asset fee = _wimpl->self->get_transaction_fee(balance.asset_id);
            if (fee.asset_id != balance.asset_id || fee > balance)
                //Forget this cheapskate
                continue;

            //Got some money, do ya? Not anymore!
            deduct_balance(bag_holder, fee);
            if (transaction_entry.fee.amount == _wimpl->_blockchain->get_delegate_registration_fee(100))
            {
                transaction_entry.fee += fee;
            }
            else
            {
                transaction_entry.fee = fee;
            }
            return true;
        }
    }
    return false;
}
