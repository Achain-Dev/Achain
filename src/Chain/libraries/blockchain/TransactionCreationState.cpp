#include <blockchain/TransactionCreationState.hpp>
#include <blockchain/BalanceOperations.hpp>

#include <algorithm>

namespace thinkyoung {
    namespace blockchain {

        TransactionCreationState::TransactionCreationState(ChainInterfacePtr prev_state)
            :pending_state(prev_state), required_signatures(1), eval_state(&pending_state)
        {
            eval_state._skip_vote_adjustment = true;
        }

        void TransactionCreationState::withdraw(const Asset& amount_to_withdraw)
        {
            ShareType left_to_withdraw = amount_to_withdraw.amount;
            for (auto item : pending_state._balance_id_to_entry)
            {
                if (item.second.asset_id() == amount_to_withdraw.asset_id)
                {
                    auto owner = item.second.owner();
                    if (owner && eval_state.signed_keys.find(*owner) != eval_state.signed_keys.end() &&
                        item.second.balance > 0)
                    {
                        auto withdraw_from_balance = std::min<ShareType>(item.second.balance, left_to_withdraw);
                        left_to_withdraw -= withdraw_from_balance;

                        trx.withdraw(item.first, withdraw_from_balance);

                        eval_state.evaluate_operation(trx.operations.back());

                        required_signatures.front().owners.insert(*owner);
                        required_signatures.front().required = required_signatures.front().owners.size();

                        if (left_to_withdraw == 0)
                            break;
                    }
                }
            }
            FC_ASSERT(left_to_withdraw == 0, "Unable to withdraw requested amount.",
                ("left_to_withdraw", left_to_withdraw)
                ("all_balances", pending_state._balance_id_to_entry)
                ("available_keys", eval_state.signed_keys));
        }

        void TransactionCreationState::add_known_key(const Address& key)
        {
            eval_state.signed_keys.insert(key);
        }

        PublicKeyType TransactionCreationState::deposit(const Asset&           amount,
            const PublicKeyType&            to,
            SlateIdType                     slate,
            const optional<PrivateKeyType>& one_time_key,
            const string&                     memo,
            const optional<PrivateKeyType>& from,
            bool                              stealth)
        {
            PublicKeyType receive_key = to;
            if (!one_time_key)
            {
                trx.deposit(to, amount);
            }
            else
            {
                WithdrawWithSignature by_account;
                receive_key = by_account.encrypt_memo_data(*one_time_key,
                    to,
                    from ? *from
                    : fc::ecc::private_key(),
                    memo,
                    from ? from->get_public_key()
                    : one_time_key->get_public_key(),
                    from_memo,
                    stealth);

                DepositOperation op;
                op.amount = amount.amount;
                op.condition = WithdrawCondition(by_account, amount.asset_id, slate);
                trx.operations.emplace_back(op);

            }
            eval_state.evaluate_operation(trx.operations.back());
            return receive_key;
        }

    }
}
