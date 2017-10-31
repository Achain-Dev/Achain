#pragma once

#include <blockchain/Types.hpp>
#include <blockchain/WithdrawTypes.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct SnapshotEntry
        {
            SnapshotEntry(){}

            SnapshotEntry(const string& a, ShareType b)
                :original_address(a), original_balance(b){}

            string        original_address;
            ShareType    original_balance = 0;
        };
        typedef fc::optional<SnapshotEntry> oSnapshotEntry;

        struct BalanceEntry;
        typedef fc::optional<BalanceEntry> oBalanceEntry;

        class ChainInterface;
        struct BalanceEntry
        {
            BalanceEntry(){}

            BalanceEntry(const WithdrawCondition& c)
                :condition(c){}

            BalanceEntry(const Address& owner, const Asset& balance, SlateIdType delegate_id);

            BalanceIdType            id()const { return condition.get_address(); }

            SlateIdType              slate_id()const { return condition.slate_id; }

            set<Address>               owners()const;
            optional<Address>          owner()const;
            bool                       is_owner(const Address& addr)const;

            AssetIdType              asset_id()const { return condition.asset_id; }
            /**
            * Get spendable balance .
            *
            * @param at_time
            *
            * @return Asset
            */
            Asset                      get_spendable_balance(const time_point_sec at_time)const;

            WithdrawCondition         condition;
            ShareType                 balance = 0;
            optional<Address>          restricted_owner;
            oSnapshotEntry           snapshot_info;
            fc::time_point_sec         deposit_date;
            fc::time_point_sec         last_update;
            variant                    meta_data; // extra meta data about every balance

            static BalanceIdType get_multisig_balance_id(AssetIdType asset_id, uint32_t m, const vector<Address>& addrs);

            void sanity_check(const ChainInterface&)const;
            static oBalanceEntry lookup(const ChainInterface&, const BalanceIdType&);
            static void store(ChainInterface&, const BalanceIdType&, const BalanceEntry&);
            static void remove(ChainInterface&, const BalanceIdType&);
        };

        class BalanceDbInterface
        {
            friend struct BalanceEntry;

            virtual oBalanceEntry balance_lookup_by_id(const BalanceIdType&)const = 0;
            virtual void balance_insert_into_id_map(const BalanceIdType&, const BalanceEntry&) = 0;
            virtual void balance_erase_from_id_map(const BalanceIdType&) = 0;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::SnapshotEntry, (original_address)(original_balance))
FC_REFLECT(thinkyoung::blockchain::BalanceEntry, (condition)(balance)(restricted_owner)(snapshot_info)(deposit_date)(last_update)(meta_data))
