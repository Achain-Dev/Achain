#include <blockchain/AccountOperations.hpp>
#include <blockchain/Exceptions.hpp>
#include <blockchain/PendingChainState.hpp>
#include <blockchain/TransactionEvaluationState.hpp>
#include <fc/time.hpp>

#include <blockchain/ForkBlocks.hpp>

namespace thinkyoung {
    namespace blockchain {

        bool RegisterAccountOperation::is_delegate()const
        {
            return delegate_pay_rate <= 100;
        }

        void RegisterAccountOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                if (!eval_state._current_state->is_valid_account_name(this->name))
                    FC_CAPTURE_AND_THROW(invalid_account_name, (name));

                oAccountEntry current_account = eval_state._current_state->get_account_entry(this->name);
                if (current_account.valid())
                    FC_CAPTURE_AND_THROW(account_already_registered, (name));

                current_account = eval_state._current_state->get_account_entry(this->owner_key);
                if (current_account.valid())
                    FC_CAPTURE_AND_THROW(account_key_in_use, (this->owner_key)(current_account));

                current_account = eval_state._current_state->get_account_entry(this->active_key);
                if (current_account.valid())
                    FC_CAPTURE_AND_THROW(account_key_in_use, (this->active_key)(current_account));

                const time_point_sec now = eval_state._current_state->now();

                AccountEntry new_entry;
                new_entry.id = eval_state._current_state->new_account_id();
                new_entry.name = this->name;
                new_entry.public_data = this->public_data;
                new_entry.owner_key = this->owner_key;
                new_entry.set_active_key(now, this->active_key);
                new_entry.registration_date = now;
                new_entry.last_update = now;
                new_entry.meta_data = this->meta_data;

                if (!new_entry.meta_data.valid())
                    new_entry.meta_data = AccountMetaInfo(titan_account);

                if (this->is_delegate())
                {
                    new_entry.delegate_info = DelegateStats();
                    new_entry.delegate_info->pay_rate = this->delegate_pay_rate;
                    new_entry.set_signing_key(eval_state._current_state->get_head_block_num(), this->active_key);

                    const Asset reg_fee(eval_state._current_state->get_delegate_registration_fee(this->delegate_pay_rate), 0);
                    eval_state.required_fees += reg_fee;
                }

                eval_state._current_state->store_account_entry(new_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        bool UpdateAccountOperation::is_retracted()const
        {
            return this->active_key.valid() && *this->active_key == PublicKeyType();
        }

        bool UpdateAccountOperation::is_delegate()const
        {
            return delegate_pay_rate <= 100;
        }

        void UpdateAccountOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                oAccountEntry current_entry = eval_state._current_state->get_account_entry(this->account_id);
                if (!current_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_account_id, (account_id));

                if (current_entry->is_retracted())
                    FC_CAPTURE_AND_THROW(account_retracted, (current_entry));

                // If updating active key
                if (this->active_key.valid() && *this->active_key != current_entry->active_key())
                {
                    if (!this->is_retracted())
                    {
                        const oAccountEntry account_with_same_key = eval_state._current_state->get_account_entry(*this->active_key);
                        if (account_with_same_key.valid())
                            FC_CAPTURE_AND_THROW(account_key_in_use, (*this->active_key)(account_with_same_key));
                    }
                    else
                    {
                        if (current_entry->is_delegate() && current_entry->delegate_pay_balance() > 0)
                            FC_CAPTURE_AND_THROW(pay_balance_remaining, (*current_entry));
                    }

                    if (!eval_state.check_signature(current_entry->owner_key))
                        FC_CAPTURE_AND_THROW(missing_signature, (*this));

                    current_entry->set_active_key(eval_state._current_state->now(), *this->active_key);
                }
                else
                {
                    if (!eval_state.account_has_signed(*current_entry))
                        FC_CAPTURE_AND_THROW(missing_signature, (*this));
                }

                if (this->public_data.valid())
                    current_entry->public_data = *this->public_data;

                // Delegates accounts cannot revert to a normal account
                if (current_entry->is_delegate())
                    FC_ASSERT(this->is_delegate(), "Delegates accounts cannot revert to a normal account");

                if (this->is_delegate())
                {
                    if (current_entry->is_delegate())
                    {
                        // Delegates cannot increase their pay rate
                        FC_ASSERT(this->delegate_pay_rate <= current_entry->delegate_info->pay_rate, "Delegates cannot increase their pay rate");
                        current_entry->delegate_info->pay_rate = this->delegate_pay_rate;
                    }
                    else
                    {
                        current_entry->delegate_info = DelegateStats();
                        current_entry->delegate_info->pay_rate = this->delegate_pay_rate;
                        current_entry->set_signing_key(eval_state._current_state->get_head_block_num(), current_entry->active_key());

                        const Asset reg_fee(eval_state._current_state->get_delegate_registration_fee(this->delegate_pay_rate), 0);
                        eval_state.required_fees += reg_fee;
                    }
                }

                current_entry->last_update = eval_state._current_state->now();

                eval_state._current_state->store_account_entry(*current_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void WithdrawPayOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                if (this->amount <= 0)
                    FC_CAPTURE_AND_THROW(negative_withdraw, (amount));

                const AccountIdType account_id = abs(this->account_id);
                oAccountEntry account = eval_state._current_state->get_account_entry(account_id);
                if (!account.valid())
                    FC_CAPTURE_AND_THROW(unknown_account_id, (account_id));

                if (!account->is_delegate())
                    FC_CAPTURE_AND_THROW(not_a_delegate, (account));

                if (account->delegate_info->pay_balance < this->amount)
                    FC_CAPTURE_AND_THROW(insufficient_funds, (account)(amount));

                if (!eval_state.account_has_signed(*account))
                    FC_CAPTURE_AND_THROW(missing_signature, (*this));
                fc::safe<ShareType> pay_balance = account->delegate_info->pay_balance;
                pay_balance -= this->amount;
                account->delegate_info->pay_balance = pay_balance.value;
                fc::safe<ShareType> delegate_vote_deltas = eval_state.delegate_vote_deltas[account_id];
                delegate_vote_deltas -= this->amount;
                eval_state.delegate_vote_deltas[account_id] = delegate_vote_deltas.value;
                eval_state.add_balance(Asset(this->amount, 0));

                eval_state._current_state->store_account_entry(*account);
            } FC_CAPTURE_AND_RETHROW((*this))
        }
        void UpdateSigningKeyOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                FC_ASSERT(false, "Disable UpdateSigningKeyOperation!");
                oAccountEntry account_rec = eval_state._current_state->get_account_entry(this->account_id);
                if (!account_rec.valid())
                    FC_CAPTURE_AND_THROW(unknown_account_id, (account_id));

                if (account_rec->is_retracted())
                    FC_CAPTURE_AND_THROW(account_retracted, (*account_rec));

                if (!account_rec->is_delegate())
                    FC_CAPTURE_AND_THROW(not_a_delegate, (*account_rec));

                oAccountEntry existing_entry = eval_state._current_state->get_account_entry(this->signing_key);
                if (existing_entry.valid())
                    FC_CAPTURE_AND_THROW(account_key_in_use, (*existing_entry));

                if (!eval_state.check_signature(account_rec->signing_address()) && !eval_state.account_has_signed(*account_rec))
                    FC_CAPTURE_AND_THROW(missing_signature, (*this));

                account_rec->set_signing_key(eval_state._current_state->get_head_block_num(), this->signing_key);
                account_rec->last_update = eval_state._current_state->now();

                eval_state._current_state->store_account_entry(*account_rec);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

    }
} // thinkyoung::blockchain
