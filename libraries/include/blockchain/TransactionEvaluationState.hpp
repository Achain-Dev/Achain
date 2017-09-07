#pragma once
#include <blockchain/Condition.hpp>
#include <blockchain/Transaction.hpp>
#include <blockchain/EventOperations.hpp>
#include <blockchain/Types.hpp>
#include <blockchain/BalanceEntry.hpp>

namespace thinkyoung {
    namespace blockchain {

        class PendingChainState;

        /**
         *  While evaluating a transaction there is a lot of intermediate
         *  state that must be tracked.  Any shares withdrawn from the
         *  database must be stored in the transaction state until they
         *  are sent back to the database as either new balances or
         *  as fees collected.
         *
         *  Some outputs such as markets, options, etc require certain
         *  payments to be made.  So payments made are tracked and
         *  compared against payments required.
         *
         */
        class TransactionEvaluationState
        {
        public:
            TransactionEvaluationState(PendingChainState* current_state = nullptr);

            ~TransactionEvaluationState();
            ShareType get_fees(AssetIdType id = 0)const;

            void evaluate(const SignedTransaction& trx, bool ignore_state = true);
            bool transaction_signature_check(const SignedTransaction& trx,const std::vector<BalanceEntry> all_balances,const std::vector<AccountEntry> all_account);
            bool transaction_entry_analy(const SignedTransaction& trx_arg, std::vector<BalanceEntry>& all_balances, std::vector<AccountEntry>& all_account);
			SignedTransaction sandbox_evaluate(const SignedTransaction &trx_arg, bool& ignore_check_required_fee);
            void evaluate_operation(const Operation& op);
            bool verify_authority(const MultisigMetaInfo& siginfo);

            /** perform any final operations based upon the current state of
             * the operation such as updating fees paid etc.
             */
            void post_evaluate();

            /** can be specalized to handle many different forms of
             * fee payment.
             */
            void validate_required_fee();

            /**
             * apply collected vote changes
             */
            void update_delegate_votes();

            bool check_signature(const Address& a)const;
            bool check_multisig(const MultisigCondition& a)const;

            bool account_has_signed(const AccountEntry& entry)const;

            void sub_balance(const BalanceIdType& addr, const Asset& amount);
            void add_balance(const Asset& amount);

            /** any time a balance is deposited increment the vote for the delegate,
             * if delegate_id is negative then it is a vote against abs(delegate_id)
             */
            void adjust_vote(SlateIdType slate, ShareType amount);

            void validate_asset(const Asset& a)const;

            bool scan_deltas(const uint32_t op_index, const function<bool(const Asset&)> callback)const;

            void scan_addresses(const ChainInterface&, const function<void(const Address&)> callback)const;

            //compare trxs whether same or not, for register contract and call contract
            bool is_contract_trxs_same(const SignedTransaction& l_trx, const SignedTransaction& r_trx)const;

            bool is_contract_op_same(const Operation& l_op, const Operation& r_op)const;
            
            bool transfer_asset_from_contract(
                ShareType real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const Address& from_contract_address,
                const string& to_account_name);

            bool transfer_asset_from_contract(
                ShareType real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const Address& from_contract_address,
                const Address& to_account_address);

            bool is_contract_op(const thinkyoung::blockchain::OperationTypeEnum& op_type)const;
            bool origin_trx_basic_verify(const SignedTransaction& trx)const;

            SignedTransaction                             trx;
            set<Address>                                   signed_keys;

            // increases with funds are withdrawn, decreases when funds are deposited or fees paid
            optional<fc::exception>                        validation_error;

            // track deposits and withdraws by asset type
            unordered_map<AssetIdType, ShareType>       deposits;
            unordered_map<AssetIdType, ShareType>       withdraws;
            unordered_map<AssetIdType, ShareType>       yield;
            //record for refund 
            std::pair <BalanceIdType, ShareType > deposit_behavior;
            map<uint32_t, map<AssetIdType, ShareType>>  deltas;

            Asset                                          required_fees;
            Asset                                          total_withdraw;
            /**
             *  The total fees paid by in alternative asset types (like BitUSD) calculated
             *  by using the median feed price
             */
            Asset                                          alt_fees_paid;

            /**
             *  As operation withdraw funds, input balance grows...
             *  As operations consume funds (deposit) input balance decreasesy
             *
             *  Any left-over input balance can be seen as fees
             *
             *  @note - this value should always equal the sum of deposits-withdraws
             *  and is maintained for the purpose of seralization.
             */
            map<AssetIdType, ShareType>                 balance;

            unordered_map<AccountIdType, ShareType>     delegate_vote_deltas;

            ImessageIdType                                imessage_length;
            // Not serialized
            ChainInterface*                               _current_state = nullptr;

            bool                                           _skip_signature_check = false;
            bool                                           _enforce_canonical_signatures = false;
            bool                                           _skip_vote_adjustment = false;

            // For pay_fee op
            unordered_map<AssetIdType, ShareType>       _max_fee;
            bool                                           evaluate_contract_testing = false;
            // for contract
            // 结果交易
            SignedTransaction                 p_result_trx;
            uint32_t                                       current_op_index = 0;
            bool                                           skipexec;//用于表示是否跳过合约代码的执行
			bool										  throw_exec_exception;
            Asset                                          exec_cost;
            PublicKeyType                                  contract_operator;
            bool                                           evaluate_contract_result = false; //是否为结果交易, 用于防止从合约账户取钱，以及修改storage等交易独立于合约执行出现

            // for event
            vector<EventOperation>							event_vector;

			//for on destroy operation
			ShareType                                      _contract_balance_remain;
			vector<BalanceIdType>   withdrawed_contract_balance;
			set<Address>			owner_balance_not_usedup;
			set<Address>            deposit_address;
			int						deposit_count=0;

        };
        typedef shared_ptr<TransactionEvaluationState> TransactionEvaluationStatePtr;

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::TransactionEvaluationState,
    (trx)
    (signed_keys)
    (validation_error)
    (deposits)
    (withdraws)
    (yield)
    (deltas)
    (required_fees)
    (alt_fees_paid)
    (balance)
    (delegate_vote_deltas)
    (imessage_length)
    (skipexec)
    )
