#include <blockchain/StorageOperations.hpp>
#include <blockchain/TransactionEvaluationState.hpp>
#include <blockchain/Exceptions.hpp>
#include <blockchain/ChainInterface.hpp>
#include <blockchain/PendingChainState.hpp>

namespace thinkyoung {
    namespace blockchain {
    
        template<typename StorageBaseType, typename StorageContainerType, typename StorageContainerBaseType>
        void StorageOperation::update_contract_value(const std::string storage_name, const StorageDataChangeType& change_storage,
                std::vector<ContractValueEntry>& value_vector) const {
            //都做反序列化
            std::map<std::string, StorageBaseType> storage_after_table = change_storage.storage_after.as<StorageContainerType>().raw_storage_map;
            
            if (change_storage.storage_before.storage_type != StorageValueTypes::storage_value_null) {
                std::map<std::string, StorageBaseType> storage_before_table = change_storage.storage_before.as<StorageContainerType>().raw_storage_map;
                //删除所有before map中的
                auto iter_before = storage_before_table.begin();
                
                for (; iter_before != storage_before_table.end(); ++iter_before) {
                }
            }
            
            //增加所有after map中的
            for ( auto& iter_after : storage_after_table) {
                StorageContainerBaseType new_storage;
                new_storage.raw_storage = iter_after.second;
                ContractValueEntry value;
                value.index_ = iter_after.first;
                value.storage_value_ = StorageDataType(new_storage);
                value.id_ = contract_id;
                value.value_name_ = storage_name;
                value_vector.emplace_back(value);
            }
        }
        void StorageOperation::update_contract_storages_value(const std::string storage_name, const StorageDataChangeType & change_storage,
                std::vector<ContractValueEntry>& contract_change_vector) const {
            if (change_storage.storage_before.storage_type != StorageValueTypes::storage_value_null &&
                    change_storage.storage_after.storage_type == StorageValueTypes::storage_value_null) {
            } else if ( change_storage.storage_after.storage_type != StorageValueTypes::storage_value_null) {
                //基本类型则直接替换
                //map类型的话先做反序列化, 再做更新, 然后再做序列化后替换
                auto storage_type = change_storage.storage_after.storage_type;
                
                if (is_any_base_storage_value_type(storage_type)) {
                    ContractValueEntry value;
                    value.storage_value_ = change_storage.storage_after;
                    value.id_ = contract_id;
                    value.value_name_ = storage_name;
                    contract_change_vector.emplace_back(value);
                    
                } else if (storage_type == StorageValueTypes::storage_value_int_table)
                    update_contract_value<LUA_INTEGER, StorageIntTableType, StorageIntType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_number_table)
                    update_contract_value<double, StorageNumberTableType, StorageNumberType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_bool_table)
                    update_contract_value<bool, StorageBoolTableType, StorageBoolType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_string_table)
                    update_contract_value<std::string, StorageStringTableType, StorageStringType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_int_array)
                    update_contract_value<LUA_INTEGER, StorageIntArrayType, StorageIntType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_number_array)
                    update_contract_value<double, StorageNumberArrayType, StorageNumberType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_bool_array)
                    update_contract_value<bool, StorageBoolArrayType, StorageBoolType>(storage_name, change_storage, contract_change_vector);
                    
                else if (storage_type == StorageValueTypes::storage_value_string_array)
                    update_contract_value<std::string, StorageStringArrayType, StorageStringType>(storage_name, change_storage, contract_change_vector);
                    
            } else {
            }
        }
        
        
        void StorageOperation::evaluate(TransactionEvaluationState& eval_state)const {
            try {
                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(not_be_result_of_execute, (type));
                    
                //
                std::unordered_set<ContractValueIdType> eval_set;
                std::vector<ContractValueEntry> value_entry;
                std::vector<ContractValueEntry> remove_value_entry;
                
                for (auto iter_change = contract_change_storages.begin(); iter_change != contract_change_storages.end(); ++iter_change) {
                    //storage_after和storage_before类型不一致
                    if (iter_change->second.storage_before.storage_type != StorageValueTypes::storage_value_null
                            && iter_change->second.storage_after.storage_type != StorageValueTypes::storage_value_null)
                        FC_ASSERT(iter_change->second.storage_before.storage_type == iter_change->second.storage_after.storage_type,
                                  "the after storage type is not match the before");
                                  
                    update_contract_storages_value(iter_change->first, iter_change->second, value_entry);
                }
                
                auto undo_state = (PendingChainState*)(eval_state._current_state);
                
                for (auto& value : value_entry) {
                    undo_state->store(value.get_contract_value_id(), value);
                    
                    if (value.is_map_value()) {
                        auto& undo_set = undo_state->_value_map_index[value.get_contract_index_id()];
                        undo_set.index_set.emplace(value.get_contract_value_id());
                    }
                }
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
    }
}