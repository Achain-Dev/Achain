/*
author: gzl
date: 2017.11.20
lvm interface in glua runtime
*/
#pragma once

#include <vector>

#include "blockchain/Address.hpp"
#include "blockchain/ChainInterface.hpp"
#include "blockchain/StorageOperations.hpp"
#include "blockchain/TransactionEvaluationState.hpp"
#include "blockchain/Exceptions.hpp"
#include "blockchain/BalanceOperations.hpp"
#include "blockchain/ContractOperations.hpp"
#include "blockchain/Types.hpp"
#include "wallet/Wallet.hpp"
#include "fc/exception/exception.hpp"

#include "glua/thinkyoung_lua_api.h"
/*
TODO
exception : FC_CAPTURE_AND_THROW
return : lvm-function return
*/
namespace lvm {
    namespace api {
        using namespace thinkyoung;
        using namespace thinkyoung::blockchain;
        typedef std::map<std::string, StorageDataChangeType> StorageDataChangeMap;
        typedef std::map<std::string, StorageDataChangeMap> AllStorageDataChange;
        class LvmInterface {
          public:
            explicit LvmInterface(thinkyoung::blockchain::TransactionEvaluationState* ptr);
            ~LvmInterface();

            uint64_t err_num;
            std::vector < std::vector<char> > result;

            //NO OPERATION
            void get_stored_contract_info_by_address(const std::string& contract_address);
            void get_contract_address_by_name(const std::string& contract_name);
            void check_contract_exist_by_address(const std::string& contract_address);
            void check_contract_exist(const std::string& contract_name);
            void open_contract(const std::string& contract_name);
            void open_contract_by_address(const std::string& contract_address);
            void get_storage_value_from_thinkyoung_by_address(const std::string& contract_address, const std::string& storage_name);
            void get_contract_balance_amount (const std::string& contract_address, const std::string& asset_symbol);
            void get_transaction_fee ();
            void get_chain_now ();
            void get_chain_random ();
            void get_transaction_id ();
            void get_header_block_num ();
            void wait_for_future_random (const int next);
            void get_waited (uint32_t num);

            //OPERATION
            void commit_storage_changes_to_thinkyoung(AllStorageDataChange&);
            void transfer_from_contract_to_address(const std::string& contract_address, const std::string& to_address,
                                                   const std::string& asset_type, int64_t amount);
            void transfer_from_contract_to_public_account(const std::string& contract_address, const std::string& to_account_name,
                    const std::string& asset_type, int64_t amount);
            void emit(const std::string& contract_id, const std::string& event_name, const std::string& event_param);

          private:
            void push_result(std::vector<char>& value);

            thinkyoung::blockchain::oContractEntry _entry;
            thinkyoung::blockchain::ChainInterface* _chain_interface;
            thinkyoung::blockchain::TransactionEvaluationState* _evaluate_state;
        };
    }
}