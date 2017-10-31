#include <blockchain/AccountEntry.hpp>
#include <blockchain/ChainInterface.hpp>

namespace thinkyoung {
    namespace blockchain {

        ShareType AccountEntry::delegate_pay_balance()const
        {
            FC_ASSERT(is_delegate(), "Delegate_pay_balance is only avaliable for delegates");
            return delegate_info->pay_balance;
        }

        bool AccountEntry::is_delegate()const
        {
            return delegate_info.valid();
        }

        ShareType AccountEntry::net_votes()const
        {
            FC_ASSERT(is_delegate(), "Delegate_pay_balance is only avaliable ");
            return delegate_info->votes_for;
        }

        void AccountEntry::adjust_votes_for(const ShareType delta)
        {
            FC_ASSERT(is_delegate(), "Delegate_pay_balance is only avaliable ");
            delegate_info->votes_for += delta;
        }

        bool AccountEntry::is_retracted()const
        {
            return active_key() == PublicKeyType();
        }

        Address AccountEntry::active_address()const
        {
            return Address(active_key());
        }

        void AccountEntry::set_active_key(const time_point_sec now, const PublicKeyType& new_key)
        {
            try {
                FC_ASSERT(now != fc::time_point_sec(), "Invalid time");
                active_key_history[now] = new_key;
            } FC_CAPTURE_AND_RETHROW((now)(new_key))
        }

        PublicKeyType AccountEntry::active_key()const
        {
            FC_ASSERT(!active_key_history.empty(), "No active key");
            return active_key_history.rbegin()->second;
        }

        uint8_t AccountEntry::delegate_pay_rate()const
        {
            if (!is_delegate())
                return -1;

            return delegate_info->pay_rate;
        }

        void AccountEntry::set_signing_key(uint32_t block_num, const PublicKeyType& signing_key)
        {
            FC_ASSERT(is_delegate(), "Only delegate accounts have signing key!");
            delegate_info->signing_key_history[block_num] = signing_key;
        }

        PublicKeyType AccountEntry::signing_key()const
        {
            FC_ASSERT(is_delegate(), "Only delegate accounts have signing key!");
            FC_ASSERT(!delegate_info->signing_key_history.empty());
            return delegate_info->signing_key_history.crbegin()->second;
        }

        Address AccountEntry::signing_address()const
        {
            return Address(signing_key());
        }

        void AccountEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(id > 0, "Invalid id");
                FC_ASSERT(!name.empty(), "Name should not be empty");
                FC_ASSERT(!active_key_history.empty(), "An account shuld have at least one active key");
                if (delegate_info.valid())
                {
                    FC_ASSERT(delegate_info->votes_for >= 0);
                    FC_ASSERT(delegate_info->pay_rate <= 100);
                    FC_ASSERT(!delegate_info->signing_key_history.empty());
                    FC_ASSERT(delegate_info->pay_balance >= 0);
                }
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oAccountEntry AccountEntry::lookup(const ChainInterface& db, const AccountIdType id)
        {
            try {
                return db.account_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        oAccountEntry AccountEntry::lookup(const ChainInterface& db, const string& name)
        {
            try {
                return db.account_lookup_by_name(name);
            } FC_CAPTURE_AND_RETHROW((name))
        }

        oAccountEntry AccountEntry::lookup(const ChainInterface& db, const Address& addr)
        {
            try {
                return db.account_lookup_by_address(addr);
            } FC_CAPTURE_AND_RETHROW((addr))
        }

        void AccountEntry::store(ChainInterface& db, const AccountIdType id, const AccountEntry& entry)
        {
            try {
                const oAccountEntry prev_entry = db.lookup<AccountEntry>(id);
                if (prev_entry.valid())
                {
                    if (prev_entry->name != entry.name)
                        db.account_erase_from_name_map(prev_entry->name);

                    if (prev_entry->owner_address() != entry.owner_address())
                        db.account_erase_from_address_map(prev_entry->owner_address());

                    if (prev_entry->active_key() != entry.active_key())
                    {
                        for (const auto& item : prev_entry->active_key_history)
                        {
                            const PublicKeyType& active_key = item.second;
                            db.account_erase_from_address_map(Address(active_key));
                        }
                    }

                    if (prev_entry->is_delegate())
                    {
                        if (!entry.is_delegate() || prev_entry->signing_key() != entry.signing_key())
                        {
                            for (const auto& item : prev_entry->delegate_info->signing_key_history)
                            {
                                const PublicKeyType& signing_key = item.second;
                                db.account_erase_from_address_map(Address(signing_key));
                            }
                        }

                        if (!prev_entry->is_retracted())
                        {
                            if (entry.is_retracted() || !entry.is_delegate() || prev_entry->net_votes() != entry.net_votes())
                                db.account_erase_from_vote_set(VoteDel(prev_entry->net_votes(), prev_entry->id));
                        }
                    }
                }

                db.account_insert_into_id_map(id, entry);
                db.account_insert_into_name_map(entry.name, id);
                db.account_insert_into_address_map(entry.owner_address(), id);

                for (const auto& item : entry.active_key_history)
                {
                    const PublicKeyType& active_key = item.second;
                    if (active_key != PublicKeyType()) db.account_insert_into_address_map(Address(active_key), id);
                }

                if (entry.is_delegate())
                {
                    for (const auto& item : entry.delegate_info->signing_key_history)
                    {
                        const PublicKeyType& signing_key = item.second;
                        if (signing_key != PublicKeyType()) db.account_insert_into_address_map(Address(signing_key), id);
                    }

                    if (!entry.is_retracted())
                        db.account_insert_into_vote_set(VoteDel(entry.net_votes(), id));
                }
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void AccountEntry::remove(ChainInterface& db, const AccountIdType id)
        {
            try {
                const oAccountEntry prev_entry = db.lookup<AccountEntry>(id);
                if (prev_entry.valid())
                {
                    db.account_erase_from_id_map(id);
                    db.account_erase_from_name_map(prev_entry->name);
                    db.account_erase_from_address_map(prev_entry->owner_address());

                    for (const auto& item : prev_entry->active_key_history)
                    {
                        const PublicKeyType& active_key = item.second;
                        db.account_erase_from_address_map(Address(active_key));
                    }

                    if (prev_entry->is_delegate())
                    {
                        for (const auto& item : prev_entry->delegate_info->signing_key_history)
                        {
                            const PublicKeyType& signing_key = item.second;
                            db.account_erase_from_address_map(Address(signing_key));
                        }

                        if (!prev_entry->is_retracted())
                            db.account_erase_from_vote_set(VoteDel(prev_entry->net_votes(), prev_entry->id));
                    }
                }
            } FC_CAPTURE_AND_RETHROW((id))
        }

    }
} // thinkyoung::blockchain
