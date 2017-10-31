#pragma once

#include <blockchain/Transaction.hpp>
#include <blockchain/Types.hpp>
#include <blockchain/WithdrawTypes.hpp>

namespace thinkyoung {
    namespace blockchain {

        enum AssetPermissions
        {
            none = 0,
            retractable = 1 << 0, ///<! The issuer can sign inplace of the owner
            restricted = 1 << 1, ///<! The issuer whitelists public keys
            market_halt = 1 << 2, ///<! The issuer can/did freeze all markets
            balance_halt = 1 << 3, ///<! The issuer can/did freeze all balances
            supply_unlimit = 1 << 4  ///<! The issuer can change the supply at will
        };

        struct AssetEntry;
        typedef fc::optional<AssetEntry> oAssetEntry;

        class ChainInterface;
        struct AssetEntry
        {
            enum
            {
                god_issuer_id = 0,
                null_issuer_id = -1,
                market_issuer_id = -2
            };


            bool is_user_issued()const        { return issuer_account_id > god_issuer_id; };

            bool is_retractable()const        { return is_user_issued() && (flags & retractable); }
            bool is_restricted()const         { return is_user_issued() && (flags & restricted); }

            bool is_balance_frozen()const     { return is_user_issued() && (flags & balance_halt); }
            bool is_supply_unlimited()const   { return is_user_issued() && (flags & supply_unlimit); }

            bool can_issue(const Asset& amount)const;
            bool can_issue(const ShareType amount)const;
            ShareType available_shares()const;

            Asset asset_from_string(const string& amount)const;
            string amount_to_string(ShareType amount, bool append_symbol = true)const;

            AssetIdType       id;
            std::string         symbol;
            std::string         name;
            std::string         description;
            fc::variant         public_data;
            AccountIdType     issuer_account_id = null_issuer_id;
            uint64_t            precision = 0;
            fc::time_point_sec  registration_date;
            fc::time_point_sec  last_update;
            ShareType          maximum_share_supply = 0;
            ShareType          current_share_supply = 0;
            ShareType          collected_fees = 0;
            uint32_t            flags = 0;
            uint32_t            issuer_permissions = -1;

            /**
             *  The issuer can specify a transaction fee (of the asset type)
             *  that will be paid to the issuer with every transaction that
             *  references this asset type.
             */
            ShareType          transaction_fee = 0;
            /**
             * 0 for no fee, 10000 for 100% fee.
             * This is used for gateways that want to continue earning market trading fees
             * when their assets are used.
             */
            uint16_t            market_fee = 0;
            MultisigMetaInfo  authority;

            void sanity_check(const ChainInterface&)const;
            static oAssetEntry lookup(const ChainInterface&, const AssetIdType);
            static oAssetEntry lookup(const ChainInterface&, const string&);
            static void store(ChainInterface&, const AssetIdType, const AssetEntry&);
            static void remove(ChainInterface&, const AssetIdType);
        };

        class AssetDbInterface
        {
            friend struct AssetEntry;

            virtual oAssetEntry asset_lookup_by_id(const AssetIdType)const = 0;
            virtual oAssetEntry asset_lookup_by_symbol(const string&)const = 0;

            virtual void asset_insert_into_id_map(const AssetIdType, const AssetEntry&) = 0;
            virtual void asset_insert_into_symbol_map(const string&, const AssetIdType) = 0;

            virtual void asset_erase_from_id_map(const AssetIdType) = 0;
            virtual void asset_erase_from_symbol_map(const string&) = 0;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT_ENUM(thinkyoung::blockchain::AssetPermissions,
    (none)
    (retractable)
    (restricted)
    (market_halt)
    (balance_halt)
    (supply_unlimit)
    )
    FC_REFLECT(thinkyoung::blockchain::AssetEntry,
    (id)
    (symbol)
    (name)
    (description)
    (public_data)
    (issuer_account_id)
    (precision)
    (registration_date)
    (last_update)
    (maximum_share_supply)
    (current_share_supply)
    (collected_fees)
    (flags)
    (issuer_permissions)
    (transaction_fee)
    (market_fee)
    (authority)
    )
