#include <blockchain/TransactionOperations.hpp>
#include <blockchain/ChainInterface.hpp>
namespace thinkyoung {
    namespace blockchain {

        void TransactionOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
				TransactionIdType id= eval_state.trx.result_trx_id;
                eval_state.evaluate(trx);
				eval_state._current_state->store(trx.id(), ResultTIdEntry(id));
				eval_state._current_state->store(id, RequestIdEntry(trx.id()));
            }
            FC_CAPTURE_AND_RETHROW((*this))
        }

    }
}

