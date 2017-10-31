#pragma once

#include "blockchain/Types.hpp"
#include "blockchain/Operations.hpp"

namespace thinkyoung {
    namespace blockchain {

        struct EventOperation{
            static const OperationTypeEnum type;

            EventOperation(){}

            EventOperation(const ContractIdType& id, const std::string& event_type, const std::string& event_param) : id(id)
            {
                if (event_type.length() > EVENT_TYPE_MAX_LEN)
                    this->event_type = event_type.substr(0, EVENT_TYPE_MAX_LEN);
                else
                    this->event_type = event_type;
                this->is_truncated = false;
                this->event_param = event_param;

                if (this->event_param.length() > EVENT_PARAM_MAX_LEN)
                {
                    this->is_truncated = true;
                    this->event_param = this->event_param.substr(0, EVENT_PARAM_MAX_LEN);
                }
            }

            ContractIdType id;
            std::string event_type;
            std::string event_param;
            bool is_truncated;

            void evaluate(TransactionEvaluationState& eval_state)const;
        };

    }
}


FC_REFLECT(thinkyoung::blockchain::EventOperation, (id)(event_type)(event_param)(is_truncated))