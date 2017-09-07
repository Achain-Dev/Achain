#pragma once

#include <blockchain/Operations.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct DefineSlateOperation
        {
            static const OperationTypeEnum type;

            vector<signed_int> slate;

            void evaluate(TransactionEvaluationState& eval_state)const;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::DefineSlateOperation, (slate))
