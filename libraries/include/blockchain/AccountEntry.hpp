#pragma once

#include <blockchain/Asset.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/time.hpp>

namespace thinkyoung {
    namespace blockchain {

        enum AccountType
        {
            titan_account = 0,
            public_account = 1,
            multisig_account = 2
        };

        struct MultisigMetaInfo
        {
            static const AccountType type = multisig_account;

            uint32_t                required = 0;
            std::set<Address>       owners;
        };

        struct AccountMetaInfo
        {
            fc::enum_type<fc::unsigned_int, AccountType> type = public_account;
            vector<char>                                 data;

            AccountMetaInfo(AccountType atype = public_account)
                :type(atype){}

            template<typename AccountType>
            AccountMetaInfo(const AccountType& t)
                : type(AccountType::type)
            {
                data = fc::raw::pack(t);
            }

            template<typename AccountType>
            AccountType as()const
            {
                FC_ASSERT(type == AccountType::type, "", ("AccountType", AccountType::type));
                return fc::raw::unpack<AccountType>(data);
            }
        };

        struct DelegateStats
        {
            ShareType                        votes_for = 0;

            uint8_t                           pay_rate = 0;
            map<uint32_t, PublicKeyType>    signing_key_history;

            uint32_t                          last_block_num_produced = 0;
            optional<SecretHashType>        next_secret_hash;

            ShareType                        pay_balance = 0;
            ShareType                        total_paid = 0;
            ShareType                        total_burned = 0;

            uint32_t                          blocks_produced = 0;
            uint32_t                          blocks_missed = 0;
        };
        typedef fc::optional<DelegateStats> odelegate_stats;

        struct VoteDel
        {
            ShareType        votes = 0;
            AccountIdType   delegate_id = 0;

            VoteDel(ShareType v = 0, AccountIdType del = 0)
                :votes(v), delegate_id(del){}

            friend bool operator == (const VoteDel& a, const VoteDel& b)
            {
                return std::tie(a.votes, a.delegate_id) == std::tie(b.votes, b.delegate_id);
            }

            friend bool operator < (const VoteDel& a, const VoteDel& b)
            {
                if (a.votes != b.votes) return a.votes > b.votes; /* Reverse so maps sort in descending order */
                return a.delegate_id < b.delegate_id; /* Lowest id wins in ties */
            }
        };

        struct AccountEntry;
        typedef fc::optional<AccountEntry> oAccountEntry;

        class ChainInterface;
        struct AccountEntry
        {
            Address           owner_address()const { return Address(owner_key); }

            void              set_active_key(const time_point_sec now, const PublicKeyType& new_key);
            PublicKeyType   active_key()const;
            Address           active_address()const;

            /**
            * check active account whether be retracted.
            *
            * @return bool
            */
            bool              is_retracted()const;

            bool              is_delegate()const;
            uint8_t           delegate_pay_rate()const;
            void              adjust_votes_for(const ShareType delta);
            ShareType        net_votes()const;
            /**
            * Get amount of delegate pay;
            *
            * @return ShareType
            */
            ShareType        delegate_pay_balance()const;

            void              set_signing_key(uint32_t block_num, const PublicKeyType& signing_key);
            PublicKeyType   signing_key()const;
            Address           signing_address()const;

            bool              is_titan_account()const { return meta_data.valid() && meta_data->type == titan_account; }

            AccountIdType                        id = 0;
            std::string                            name;
            fc::variant                            public_data;
            PublicKeyType                        owner_key;
            map<time_point_sec, PublicKeyType>   active_key_history;
            fc::time_point_sec                     registration_date;
            fc::time_point_sec                     last_update;
            optional<DelegateStats>               delegate_info;
            optional<AccountMetaInfo>            meta_data;

            void sanity_check(const ChainInterface&)const;
            static oAccountEntry lookup(const ChainInterface&, const AccountIdType);
            static oAccountEntry lookup(const ChainInterface&, const string&);
            static oAccountEntry lookup(const ChainInterface&, const Address&);
            static void store(ChainInterface&, const AccountIdType, const AccountEntry&);
            static void remove(ChainInterface&, const AccountIdType);
        };

        class AccountDbInterface
        {
            friend struct AccountEntry;

            virtual oAccountEntry account_lookup_by_id(const AccountIdType)const = 0;
            virtual oAccountEntry account_lookup_by_name(const string&)const = 0;
            virtual oAccountEntry account_lookup_by_address(const Address&)const = 0;

            virtual void account_insert_into_id_map(const AccountIdType, const AccountEntry&) = 0;
            virtual void account_insert_into_name_map(const string&, const AccountIdType) = 0;
            virtual void account_insert_into_address_map(const Address&, const AccountIdType) = 0;
            virtual void account_insert_into_vote_set(const VoteDel&) = 0;

            virtual void account_erase_from_id_map(const AccountIdType) = 0;
            virtual void account_erase_from_name_map(const string&) = 0;
            virtual void account_erase_from_address_map(const Address&) = 0;
            virtual void account_erase_from_vote_set(const VoteDel&) = 0;
        };
        struct DelegatePaySalary
        {
            ShareType total_balance;
            ShareType pay_balance;
        };
    }
} // thinkyoung::blockchain

FC_REFLECT_ENUM(thinkyoung::blockchain::AccountType,
    (titan_account)
    (public_account)
    (multisig_account)
    )
    FC_REFLECT(thinkyoung::blockchain::MultisigMetaInfo,
    (required)
    (owners)
    )
    FC_REFLECT(thinkyoung::blockchain::AccountMetaInfo,
    (type)
    (data)
    )
    FC_REFLECT(thinkyoung::blockchain::DelegateStats,
    (votes_for)
    (pay_rate)
    (signing_key_history)
    (last_block_num_produced)
    (next_secret_hash)
    (pay_balance)
    (total_paid)
    (total_burned)
    (blocks_produced)
    (blocks_missed)
    )
    FC_REFLECT(thinkyoung::blockchain::VoteDel,
    (votes)
    (delegate_id)
    )
    FC_REFLECT(thinkyoung::blockchain::AccountEntry,
    (id)
    (name)
    (public_data)
    (owner_key)
    (active_key_history)
    (registration_date)
    (last_update)
    (delegate_info)
    (meta_data)
    )
    FC_REFLECT(thinkyoung::blockchain::DelegatePaySalary, (total_balance)(pay_balance))