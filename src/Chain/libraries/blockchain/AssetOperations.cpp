#include <blockchain/AssetOperations.hpp>
#include <blockchain/Exceptions.hpp>
#include <blockchain/PendingChainState.hpp>

#include <blockchain/ForkBlocks.hpp>

namespace thinkyoung {
    namespace blockchain {

        bool is_power_of_ten(uint64_t n)
        {
            switch (n)
            {
            case 1ll:
            case 10ll:
            case 100ll:
            case 1000ll:
            case 10000ll:
            case 100000ll:
            case 1000000ll:
            case 10000000ll:
            case 100000000ll:
            case 1000000000ll:
            case 10000000000ll:
            case 100000000000ll:
            case 1000000000000ll:
            case 10000000000000ll:
            case 100000000000000ll:
            case 1000000000000000ll:
                return true;
            default:
                return false;
            }
        }

        void CreateAssetOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                FC_ASSERT(false, "Disable CreateAssetOperation!");
                if (NOT eval_state._current_state->is_valid_symbol_name(this->symbol))
                    FC_CAPTURE_AND_THROW(invalid_asset_symbol, (symbol));

                oAssetEntry current_asset_entry = eval_state._current_state->get_asset_entry(this->symbol);
                if (current_asset_entry.valid())
                    FC_CAPTURE_AND_THROW(asset_symbol_in_use, (symbol));

                if (this->name.empty())
                    FC_CAPTURE_AND_THROW(invalid_asset_name, (this->name));

                const AssetIdType asset_id = eval_state._current_state->last_asset_id() + 1;
                current_asset_entry = eval_state._current_state->get_asset_entry(asset_id);
                if (current_asset_entry.valid())
                    FC_CAPTURE_AND_THROW(asset_id_in_use, (asset_id));

                oAccountEntry issuer_account_entry;
                if (issuer_account_id != AssetEntry::market_issuer_id)
                {
                    issuer_account_entry = eval_state._current_state->get_account_entry(this->issuer_account_id);
                    if (NOT issuer_account_entry.valid())
                        FC_CAPTURE_AND_THROW(unknown_account_id, (issuer_account_id));
                }

                if (this->maximum_share_supply <= 0 || this->maximum_share_supply > ALP_BLOCKCHAIN_MAX_SHARES)
                    FC_CAPTURE_AND_THROW(invalid_asset_amount, (this->maximum_share_supply));

                if (NOT is_power_of_ten(this->precision))
                    FC_CAPTURE_AND_THROW(invalid_precision, (this->precision));

                const Asset reg_fee(eval_state._current_state->get_asset_registration_fee(this->symbol.size()), 0);
                eval_state.required_fees += reg_fee;

                AssetEntry new_entry;
                new_entry.id = eval_state._current_state->new_asset_id();
                new_entry.symbol = this->symbol;
                new_entry.name = this->name;
                new_entry.description = this->description;
                new_entry.public_data = this->public_data;
                new_entry.issuer_account_id = this->issuer_account_id;
                new_entry.precision = this->precision;
                new_entry.registration_date = eval_state._current_state->now();
                new_entry.last_update = new_entry.registration_date;
                new_entry.current_share_supply = 0;
                new_entry.maximum_share_supply = this->maximum_share_supply;
                new_entry.collected_fees = 0;
                // Initialize flags and issuer_permissions here, instead of
                //   in the struct definition, so that the initialization value
                //   may depend on e.g. block number.  This supports future
                //   hardforks which may want to add new permissions for future
                //   assets without applying them to existing assets.
                new_entry.flags = 0;
                new_entry.issuer_permissions = restricted | retractable | market_halt | balance_halt | supply_unlimit;

                if (issuer_account_entry)
                {
                    new_entry.authority.owners.insert(issuer_account_entry->active_key());
                    new_entry.authority.required = 1;
                }

                eval_state._current_state->store_asset_entry(new_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void UpdateAssetOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                FC_ASSERT(false, "Disable UpdateAssetOperation!");

                oAssetEntry current_asset_entry = eval_state._current_state->get_asset_entry(this->asset_id);
                if (NOT current_asset_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_asset_id, (asset_id));


                // Reject no-ops
                FC_ASSERT(this->name.valid() || this->description.valid() || this->public_data.valid()
                    || this->maximum_share_supply.valid() || this->precision.valid(), "Invalid operaqtion");

                // Cannot update max share supply, or precision if any shares have been issued
                if (current_asset_entry->current_share_supply > 0)
                {
                    FC_ASSERT(!this->maximum_share_supply.valid(), "Cannot update max share supply if any shares have been issued");
                    FC_ASSERT(!this->precision.valid(), "Cannot update precision if any shares have been issued");
                }

                if (!eval_state.verify_authority(current_asset_entry->authority))
                    FC_CAPTURE_AND_THROW(missing_signature, (current_asset_entry->authority));

                if (this->name.valid())
                {
                    if (this->name->empty())
                        FC_CAPTURE_AND_THROW(invalid_asset_name, (*this->name));

                    current_asset_entry->name = *this->name;
                }

                if (this->description.valid())
                    current_asset_entry->description = *this->description;

                if (this->public_data.valid())
                    current_asset_entry->public_data = *this->public_data;

                if (this->maximum_share_supply.valid())
                {
                    if (*this->maximum_share_supply <= 0 || *this->maximum_share_supply > ALP_BLOCKCHAIN_MAX_SHARES)
                        FC_CAPTURE_AND_THROW(invalid_asset_amount, (*this->maximum_share_supply));

                    current_asset_entry->maximum_share_supply = *this->maximum_share_supply;
                }

                if (this->precision.valid())
                {
                    if (NOT is_power_of_ten(*this->precision))
                        FC_CAPTURE_AND_THROW(invalid_precision, (*this->precision));

                    current_asset_entry->precision = *this->precision;
                }

                current_asset_entry->last_update = eval_state._current_state->now();

                eval_state._current_state->store_asset_entry(*current_asset_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }


        void UpdateAssetExtOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            FC_ASSERT(false, "Disable UpdateAssetExtOperation!");

            oAssetEntry current_asset_entry = eval_state._current_state->get_asset_entry(this->asset_id);
            if (NOT current_asset_entry.valid())
                FC_CAPTURE_AND_THROW(unknown_asset_id, (asset_id));


            // Cannot update max share supply, or precision if any shares have been issued
            if (current_asset_entry->current_share_supply > 0)
            {
                if (!(current_asset_entry->flags & supply_unlimit))
                    FC_ASSERT(!this->maximum_share_supply.valid(), "Cannot update max share supply if any shares have been issued");
                FC_ASSERT(!this->precision.valid(), "Cannot update precision if any shares have been issued");
            }

            ilog("@n Verifying authority: ${a}", ("a", current_asset_entry->authority));
            if (!eval_state.verify_authority(current_asset_entry->authority))
                FC_CAPTURE_AND_THROW(missing_signature, (current_asset_entry->authority));

            if (this->name.valid())
            {
                if (this->name->empty())
                    FC_CAPTURE_AND_THROW(invalid_asset_name, (*this->name));

                current_asset_entry->name = *this->name;
            }

            if (this->description.valid())
                current_asset_entry->description = *this->description;

            if (this->public_data.valid())
                current_asset_entry->public_data = *this->public_data;

            if (this->maximum_share_supply.valid())
            {
                if (*this->maximum_share_supply <= 0 || *this->maximum_share_supply > ALP_BLOCKCHAIN_MAX_SHARES)
                    FC_CAPTURE_AND_THROW(invalid_asset_amount, (*this->maximum_share_supply));

                current_asset_entry->maximum_share_supply = *this->maximum_share_supply;
            }

            if (this->precision.valid())
            {
                if (NOT is_power_of_ten(*this->precision))
                    FC_CAPTURE_AND_THROW(invalid_precision, (*this->precision));

                current_asset_entry->precision = *this->precision;
            }


            // you can only remove these permissions, but not add them if there are current shares
            if (current_asset_entry->current_share_supply > 0)
            {
                if (this->issuer_permissions & retractable)
                    FC_ASSERT(current_asset_entry->issuer_permissions & retractable);
                if (this->issuer_permissions & restricted)
                    FC_ASSERT(current_asset_entry->issuer_permissions & restricted);
                if (this->issuer_permissions & market_halt)
                    FC_ASSERT(current_asset_entry->issuer_permissions & market_halt);
                if (this->issuer_permissions & balance_halt)
                    FC_ASSERT(current_asset_entry->issuer_permissions & balance_halt);
                if (this->issuer_permissions & supply_unlimit)
                    FC_ASSERT(current_asset_entry->issuer_permissions & supply_unlimit);
            }
            current_asset_entry->issuer_permissions = this->issuer_permissions;

            if (this->flags & restricted) FC_ASSERT(current_asset_entry->issuer_permissions & restricted, "issuer_permissions needed:restricted");
            if (this->flags & retractable) FC_ASSERT(current_asset_entry->issuer_permissions & retractable, "issuer_permissions needed:retractable");
            if (this->flags & market_halt) FC_ASSERT(current_asset_entry->issuer_permissions & market_halt, "issuer_permissions needed:market_halt");
            if (this->flags & balance_halt) FC_ASSERT(current_asset_entry->issuer_permissions & balance_halt, "issuer_permissions needed:balance_halt");
            if (this->flags & supply_unlimit) FC_ASSERT(current_asset_entry->issuer_permissions & supply_unlimit, "issuer_permissions needed:balance_halt");
            current_asset_entry->flags = this->flags;

            current_asset_entry->transaction_fee = this->transaction_fee;
            current_asset_entry->authority = this->authority;



            FC_ASSERT(this->issuer_account_id != AssetEntry::market_issuer_id, "Assets issued by market can't be updated");
            if (this->issuer_account_id != current_asset_entry->issuer_account_id)
            {
                auto issuer_account_entry = eval_state._current_state->get_account_entry(this->issuer_account_id);
                if (NOT issuer_account_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_account_id, (issuer_account_id));
            }


            current_asset_entry->issuer_account_id = this->issuer_account_id;
            current_asset_entry->last_update = eval_state._current_state->now();

            eval_state._current_state->store_asset_entry(*current_asset_entry);
        }

        void IssueAssetOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                FC_ASSERT(false, "Disable IssueAssetOperation!");
                oAssetEntry current_asset_entry = eval_state._current_state->get_asset_entry(this->amount.asset_id);
                if (NOT current_asset_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_asset_id, (amount.asset_id));

                if (!current_asset_entry->is_user_issued())
                    FC_CAPTURE_AND_THROW(not_user_issued, (*current_asset_entry));

                if (!eval_state.verify_authority(current_asset_entry->authority))
                    FC_CAPTURE_AND_THROW(missing_signature, (current_asset_entry->authority));

                if (this->amount.amount <= 0)
                    FC_CAPTURE_AND_THROW(negative_issue, (amount));

                if (NOT current_asset_entry->can_issue(this->amount))
                    FC_CAPTURE_AND_THROW(over_issue, (amount)(*current_asset_entry));

                current_asset_entry->current_share_supply += this->amount.amount;
                eval_state.add_balance(this->amount);

                current_asset_entry->last_update = eval_state._current_state->now();

                eval_state._current_state->store_asset_entry(*current_asset_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

    }
} // thinkyoung::blockchain
