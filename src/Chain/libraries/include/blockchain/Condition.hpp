#pragma once

#include <blockchain/Types.hpp>
namespace thinkyoung {
    namespace blockchain {

        // The difference between a condition and normal business logic is that
        // condition proof equivalence classes always yield the same condition
        // and so you can embed the proof at the right "scope" in the transaction
        // without doing anything complicated.    
        // eval(with[cond](expr)) would mean:   push_permission prove("with[cond](expr)"); eval(expr)

        struct Condition
        {
        };
        struct proof
        {
            Condition get_condition();
        };

        struct MultisigCondition : Condition
        {
            uint32_t                 required; // 0 means anyone can claim. -1 means the DAC can
            std::set<Address>        owners;

            MultisigCondition() :required(-1), owners(set<Address>()){}
            MultisigCondition(uint32_t m, set<Address> owners)
                :required(m), owners(owners)
            {}
        };

        // pow
        // delegate_fraud
        // timelock
        // BTC spv utxo claim
        // cross-chain trading
        // 

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::Condition, BOOST_PP_SEQ_NIL);
FC_REFLECT(thinkyoung::blockchain::MultisigCondition, (required)(owners));
