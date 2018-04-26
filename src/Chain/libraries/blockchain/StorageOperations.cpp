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
            //���������л�
            std::map<std::string, StorageBaseType> storage_after_table = change_storage.storage_after.as<StorageContainerType>().raw_storage_map;
            
            if (change_storage.storage_before.storage_type != StorageValueTypes::storage_value_null) {
                std::map<std::string, StorageBaseType> storage_before_table = change_storage.storage_before.as<StorageContainerType>().raw_storage_map;
                //ɾ������before map�е�
                auto iter_before = storage_before_table.begin();
                
                for (; iter_before != storage_before_table.end(); ++iter_before) {
                }
            }
            
            //��������after map�е�
            for ( auto& iter_after : storage_after_table) {
                StorageContainerBaseType new_storage;
                new_storage.raw_storage = iter_after.second;
                ContractValueEntry value;
                value.index_ = iter_after.first;
                value.storage_value_ = StorageDataType(new_storage);
                value.id_ = contract_id;
                value.value_name_ = storage_name;
                value_vector.emplace_back(value);
                fc_ilog(fc::logger::get("stor_debug"), "update_contract_value(map) contract_id:${contract_id}\
							storage_name: ${storage_name}  index: ${index} value:${value}", ("contract_id", value.index_)\
                        ("storage_name", value.value_name_)("index", value.index_)("value", new_storage.raw_storage));
            }
        }
        void StorageOperation::update_contract_storages_value(const std::string storage_name, const StorageDataChangeType & change_storage,
                std::vector<ContractValueEntry>& contract_change_vector) const {
            if (change_storage.storage_before.storage_type != StorageValueTypes::storage_value_null &&
                    change_storage.storage_after.storage_type == StorageValueTypes::storage_value_null) {
            } else if ( change_storage.storage_after.storage_type != StorageValueTypes::storage_value_null) {
                //����������ֱ���滻
                //map���͵Ļ����������л�, ��������, Ȼ���������л����滻
                auto storage_type = change_storage.storage_after.storage_type;
                
                if (is_any_base_storage_value_type(storage_type)) {
                    ContractValueEntry value;
                    value.storage_value_ = change_storage.storage_after;
                    value.id_ = contract_id;
                    value.value_name_ = storage_name;
                    contract_change_vector.emplace_back(value);
                    fc_ilog(fc::logger::get("stor_debug"), "update_contract_value(Plain) contract_id:${contract_id}\
							storage_name: ${storage_name}  index: ${index} value:${value}", ("contract_id", value.id_)\
                            ("storage_name", value.value_name_)("index", value.index_)("value", value.storage_value_));
                            
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
                fc_ilog(fc::logger::get("stor_debug"), "-------storage operation evaluate start--------");
                
                for (auto iter_change = contract_change_storages.begin(); iter_change != contract_change_storages.end(); ++iter_change) {
                    //storage_after��storage_before���Ͳ�һ��
                    if (iter_change->second.storage_before.storage_type != StorageValueTypes::storage_value_null
                            && iter_change->second.storage_after.storage_type != StorageValueTypes::storage_value_null)
                        FC_ASSERT(iter_change->second.storage_before.storage_type == iter_change->second.storage_after.storage_type,
                                  "the after storage type is not match the before");
                                  
                    update_contract_storages_value(iter_change->first, iter_change->second, value_entry);
                }
                
                auto undo_state = (PendingChainState*)(eval_state._current_state);
                fc_ilog(fc::logger::get("stor_debug"), "storage operation evaluate change_value_size:${value_entry}", ("value_entry", value_entry.size()));
                
                for (auto& value : value_entry) {
                    undo_state->store(value.get_contract_value_id(), value);
                    
                    if (value.is_map_value()) {
                        auto& undo_set = undo_state->_value_map_index[value.get_contract_index_id()];
                        undo_set.index_set.emplace(value.get_contract_value_id());
                        fc_ilog(fc::logger::get("stor_debug"), "storage operation evaluate index_set emplace:${value_id}", ("value_id", value.get_contract_index_id()));
                    }
                }
                
                fc_ilog(fc::logger::get("stor_debug"), "-------storage operation evaluate over--------");
            }
            
            FC_CAPTURE_AND_RETHROW((*this))
        }
    }
}