#pragma once

#include <fc/io/enum_type.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/reflect.hpp>

/**
 *  The C keyword 'not' is NOT friendly on VC++ but we still want to use
 *  it for readability, so we will have the pre-processor convert it to the
 *  more traditional form.  The goal here is to make the understanding of
 *  the validation logic as english-like as possible.
 */
#define NOT !
#define AND &&
#define OR ||

namespace thinkyoung {
    namespace blockchain {
    
        class TransactionEvaluationState;
        
        struct ContractTrxInfo {
            uint32_t block_num;
            TransactionIdType trx_id;
        };
        
        enum OperationTypeEnum {
            null_op_type = 0,
            
            // balances
            withdraw_op_type = 1,
            deposit_op_type = 2,
            
            // accounts
            register_account_op_type = 3,
            update_account_op_type = 4,
            withdraw_pay_op_type = 5,
            
            // assets
            create_asset_op_type = 6,
            update_asset_op_type = 7,
            issue_asset_op_type = 8,
            
            // reserved
            // reserved_op_1_type         = 10, // Skip; see below
            reserved_op_2_type = 11,
            reserved_op_3_type = 17,
            define_slate_op_type = 18,
            
            // reserved
            reserved_op_4_type = 21,
            reserved_op_5_type = 22,
            release_escrow_op_type = 23,
            update_signing_key_op_type = 24,
            update_balance_vote_op_type = 27,
            
            // assets
            update_asset_ext_op_type = 30,
            //memo
            imessage_memo_op_type = 66,
            
            contract_info_op_type = 68,
            
            register_contract_op_type = 70,
            upgrade_contract_op_type = 71,
            destroy_contract_op_type = 72,
            call_contract_op_type = 73,
            transfer_contract_op_type = 74,
            // contract
            withdraw_contract_op_type = 80,
            deposit_contract_op_type = 82,
            
            // balances withdraw
            balances_withdraw_op_type = 88,
            
            transaction_op_type = 90,
            storage_op_type = 91,
            
            // event
            event_op_type = 100,
            
            // on functions in contracts
            on_destroy_op_type = 108,
            on_upgrade_op_type = 109,
            
            // contract call success
            on_call_success_op_type = 110
            
        };
        /**
         *  A poly-morphic operator that modifies the blockchain database
         *  is some manner.
         */
        struct Operation {
            Operation() :type(null_op_type) {}
            
            Operation(const Operation& o)
                :type(o.type), data(o.data) {}
                
            Operation(Operation&& o)
                :type(o.type), data(std::move(o.data)) {}
                
            template<typename OperationType>
            Operation(const OperationType& t) {
                type = OperationType::type;
                data = fc::raw::pack(t);
            }
            
            template<typename OperationType>
            OperationType as()const {
                FC_ASSERT((OperationTypeEnum)type == OperationType::type, "", ("type", type)("OperationType", OperationType::type));
                return fc::raw::unpack<OperationType>(data);
            }
            
            Operation& operator=(const Operation& o) {
                if (this == &o) return *this;
                
                type = o.type;
                data = o.data;
                return *this;
            }
            
            Operation& operator=(Operation&& o) {
                if (this == &o) return *this;
                
                type = o.type;
                data = std::move(o.data);
                return *this;
            }
            
            fc::enum_type<uint8_t, OperationTypeEnum> type;
            std::vector<char> data;
        };
        
    }
} // thinkyoung::blockchain

FC_REFLECT_ENUM(thinkyoung::blockchain::OperationTypeEnum,
                (null_op_type)
                (withdraw_op_type)
                (deposit_op_type)
                (register_account_op_type)
                (update_account_op_type)
                (withdraw_pay_op_type)
                (create_asset_op_type)
                (update_asset_op_type)
                (issue_asset_op_type)
                (reserved_op_2_type)
                (reserved_op_3_type)
                (define_slate_op_type)
                (reserved_op_4_type)
                (reserved_op_5_type)
                (release_escrow_op_type)
                (update_signing_key_op_type)
                (update_balance_vote_op_type)
                (update_asset_ext_op_type)
                (contract_info_op_type)
                (imessage_memo_op_type)
                (register_contract_op_type)
                (upgrade_contract_op_type)
                (destroy_contract_op_type)
                (call_contract_op_type)
                (withdraw_contract_op_type)
                (transfer_contract_op_type)
                (deposit_contract_op_type)
                (balances_withdraw_op_type)
                (transaction_op_type)
                (storage_op_type)
                (event_op_type)
                (on_destroy_op_type)
                (on_upgrade_op_type)
                (on_call_success_op_type)
               )

FC_REFLECT(thinkyoung::blockchain::Operation, (type)(data))
FC_REFLECT(thinkyoung::blockchain::ContractTrxInfo, (block_num)(trx_id))

namespace fc {
    void to_variant(const thinkyoung::blockchain::Operation& var, variant& vo);
    void from_variant(const variant& var, thinkyoung::blockchain::Operation& vo);
}
