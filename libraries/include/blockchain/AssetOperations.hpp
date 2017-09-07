#pragma once

#include <blockchain/Asset.hpp>
#include <blockchain/Operations.hpp>
#include <blockchain/AccountEntry.hpp>
#include <blockchain/Types.hpp>
#include <blockchain/AssetEntry.hpp>

namespace thinkyoung {
    namespace blockchain {

        bool is_power_of_ten(uint64_t n);

        /**
         *  Creates / defines an asset type but does not
         *  allocate it to anyone. Use issue_asset_operation
         */
        struct CreateAssetOperation
        {
            static const OperationTypeEnum type;

            /**
             * Symbols may only contain A-Z and 0-9 and up to 5
             * characters and must be unique.
             */
            std::string      symbol;

            /**
             * Names are a more complete description and may
             * contain any kind of characters or spaces.
             */
            std::string      name;
            /**
             *  Describes the asset and its purpose.
             */
            std::string      description;
            /**
             * Other information relevant to this asset.
             */
            fc::variant      public_data;

            /**
             *  Assets can only be issued by individuals that
             *  have registered a name.
             */
            AccountIdType  issuer_account_id;

            /** The maximum number of shares that may be allocated */
            ShareType       maximum_share_supply = 0;

            uint64_t         precision = 0;

            void evaluate(TransactionEvaluationState& eval_state)const;
        };

        /**
         * This operation updates an existing issuer entry provided
         * it is signed by a proper key.
         */
        struct UpdateAssetOperation
        {
            static const OperationTypeEnum type;

            AssetIdType                asset_id;
            optional<std::string>        name;
            optional<std::string>        description;
            optional<fc::variant>        public_data;
            optional<ShareType>         maximum_share_supply;
            optional<uint64_t>           precision;

            void evaluate(TransactionEvaluationState& eval_state)const;
        };

        /**
         * This operation updates an existing issuer entry provided
         * it is signed by a proper key.
         */
        struct UpdateAssetExtOperation : public UpdateAssetOperation
        {
            static const OperationTypeEnum type;
            UpdateAssetExtOperation(){}
            UpdateAssetExtOperation(const UpdateAssetOperation& c) :UpdateAssetOperation(c){}

            /**
             * A restricted asset can only be held/controlled by keys
             * on the authorized list.
             */
            uint32_t           flags = 0;
            uint32_t           issuer_permissions = -1;
            AccountIdType    issuer_account_id;
            uint16_t           market_fee = 0;

            /**
             *  The issuer can specify a transaction fee (of the asset type)
             *  that will be paid to the issuer with every transaction that
             *  references this asset type.
             */
            ShareType          transaction_fee = 0;
            MultisigMetaInfo  authority;

            void evaluate(TransactionEvaluationState& eval_state)const;
        };

        /**
         *  Transaction must be signed by the active key
         *  on the issuer_name_entry.
         *
         *  The resulting amount of shares must be below
         *  the maximum share supply.
         */
        struct IssueAssetOperation
        {
            static const OperationTypeEnum type;

            IssueAssetOperation(Asset a = Asset()) :amount(a){}

            Asset amount;

            void evaluate(TransactionEvaluationState& eval_state)const;
        };


    }
} // thinkyoung::blockchain


FC_REFLECT(thinkyoung::blockchain::CreateAssetOperation,
    (symbol)
    (name)
    (description)
    (public_data)
    (issuer_account_id)
    (maximum_share_supply)
    (precision)
    )
    FC_REFLECT(thinkyoung::blockchain::UpdateAssetOperation,
    (asset_id)
    (name)
    (description)
    (public_data)
    (maximum_share_supply)
    (precision)
    )

    FC_REFLECT_DERIVED(thinkyoung::blockchain::UpdateAssetExtOperation,
    (thinkyoung::blockchain::UpdateAssetOperation),
    (flags)
    (issuer_permissions)
    (issuer_account_id)
    (transaction_fee)
    (market_fee)
    (authority))


    FC_REFLECT(thinkyoung::blockchain::IssueAssetOperation,
    (amount)
    )
