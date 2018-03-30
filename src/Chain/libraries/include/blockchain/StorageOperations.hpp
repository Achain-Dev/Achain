#pragma once

#include <blockchain/ContractEntry.hpp>
#include <blockchain/Operations.hpp>

namespace thinkyoung {
    namespace blockchain {
    
        struct StorageDataChangeType {
            StorageDataType storage_before;
            StorageDataType storage_after;
        };
        
        struct StorageOperation {
            static const OperationTypeEnum type;
            
            StorageOperation() {}
            
            ContractIdType contract_id;
            std::map<std::string, StorageDataChangeType> contract_change_storages;
            
            template<typename StorageBaseType, typename StorageContainerType, typename StorageContainerBaseType>
            void update_contract_value(const StorageDataChangeType& change_storage, std::vector<ContractValueEntry>& value)const;
            
            void update_contract_storages_value(const std::string storage_name, const StorageDataChangeType& change_storage,
                                                std::vector<ContractValueEntry>& contract_change_vector) const;
                                                
            void evaluate(TransactionEvaluationState& eval_state)const;
        };
        
    }
} // thinkyoung::blockchain


FC_REFLECT(thinkyoung::blockchain::StorageDataChangeType, (storage_before)(storage_after))
FC_REFLECT(thinkyoung::blockchain::StorageOperation, (contract_id)(contract_change_storages))
