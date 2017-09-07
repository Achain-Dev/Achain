#pragma once
#include <blockchain/PendingChainState.hpp>
#include <blockchain/TransactionEvaluationState.hpp>

namespace thinkyoung {
    namespace blockchain {

        /**
         *  Transaction creation state is used to build up a transaction
         *  to perform a set of operations given an intial subset of the
         *  blockchain state (pending_state)
         *
         *  When building a transaction that withdraws from balances, orders,
         *  or otherwise the key must be in specified via add_known_key(addr)
         *  prior to performing the operation.  Otherwise, the operation will
         *  fail.
         *
         *
         */
        class TransactionCreationState
        {
        public:
            TransactionCreationState(ChainInterfacePtr prev_state = ChainInterfacePtr());

            /** Adds the key to the set of known keys for the purpose of
             * building this transaction.  Only balances controlled by keys
             * specified in this way will be considered.
             */
            void add_known_key(const Address& addr);

            /** Withdraw from any balance in pending_state._balance_id_to_entry that
             * is of the proper type and for which the owenr key is known and has been
             * specified via add_known_key()
             */
            void withdraw(const Asset& amount);


            PublicKeyType deposit(const Asset&                      amount,
                const PublicKeyType&            to,
                SlateIdType                     slate = 0,
                const optional<PrivateKeyType>& one_time_key = optional<PrivateKeyType>(),
                const string&                     memo = string(),
                const optional<PrivateKeyType>& from = optional<PrivateKeyType>(),
                bool                              stealth = false);

            /**
             * Withdraws enough to pay a fee with the given asset, preferring existing
             * withdraw balances first.
             */

            SignedTransaction           trx;
            PendingChainState          pending_state;
            vector<MultisigCondition>   required_signatures;
            TransactionEvaluationState eval_state;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::TransactionCreationState,
    (trx)
    (pending_state)
    (required_signatures)
    (eval_state)
    )
