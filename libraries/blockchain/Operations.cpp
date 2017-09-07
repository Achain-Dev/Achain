#include <blockchain/AccountOperations.hpp>
#include <blockchain/AssetOperations.hpp>
#include <blockchain/BalanceOperations.hpp>
#include <blockchain/OperationFactory.hpp>
#include <blockchain/Operations.hpp>
#include <blockchain/SlateOperations.hpp>
#include <blockchain/ImessageOperations.hpp>
#include <blockchain/ContractOperations.hpp>
#include <blockchain/TransactionOperations.hpp>
#include <blockchain/StorageOperations.hpp>
#include <blockchain/EventOperations.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/reflect/variant.hpp>

namespace thinkyoung {
    namespace blockchain {
        const OperationTypeEnum WithdrawOperation::type = withdraw_op_type;
        const OperationTypeEnum DepositOperation::type = deposit_op_type;
        const OperationTypeEnum RegisterAccountOperation::type = register_account_op_type;
        const OperationTypeEnum UpdateAccountOperation::type = update_account_op_type;
        const OperationTypeEnum WithdrawPayOperation::type = withdraw_pay_op_type;
        const OperationTypeEnum CreateAssetOperation::type = create_asset_op_type;
        const OperationTypeEnum UpdateAssetOperation::type = update_asset_op_type;
        const OperationTypeEnum IssueAssetOperation::type = issue_asset_op_type;
        const OperationTypeEnum DefineSlateOperation::type = define_slate_op_type;
        const OperationTypeEnum ReleaseEscrowOperation::type = release_escrow_op_type;
        const OperationTypeEnum UpdateSigningKeyOperation::type = update_signing_key_op_type;
        const OperationTypeEnum UpdateBalanceVoteOperation::type = update_balance_vote_op_type;
        const OperationTypeEnum UpdateAssetExtOperation::type = update_asset_ext_op_type;
        const OperationTypeEnum ImessageMemoOperation::type = imessage_memo_op_type;
        const OperationTypeEnum ContractInfoOperation::type = contract_info_op_type;
        const OperationTypeEnum RegisterContractOperation::type = register_contract_op_type;
        const OperationTypeEnum UpgradeContractOperation::type = upgrade_contract_op_type;
        const OperationTypeEnum DestroyContractOperation::type = destroy_contract_op_type;
        const OperationTypeEnum WithdrawContractOperation::type = withdraw_contract_op_type;
        const OperationTypeEnum TransferContractOperation::type = transfer_contract_op_type;
        const OperationTypeEnum BalancesWithdrawOperation::type = balances_withdraw_op_type;
        const OperationTypeEnum TransactionOperation::type = transaction_op_type;
        const OperationTypeEnum StorageOperation::type = storage_op_type;
        const OperationTypeEnum CallContractOperation::type = call_contract_op_type;
        const OperationTypeEnum EventOperation::type = event_op_type;
        const OperationTypeEnum DepositContractOperation::type = deposit_contract_op_type;
		const OperationTypeEnum OnDestroyOperation::type = on_destroy_op_type;
		const OperationTypeEnum OnUpgradeOperation::type = on_upgrade_op_type;
        const OperationTypeEnum OnCallSuccessOperation::type = on_call_success_op_type;
        static bool first_chain = []()->bool{
            thinkyoung::blockchain::OperationFactory::instance().register_operation<WithdrawOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<DepositOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<RegisterAccountOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<UpdateAccountOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<WithdrawPayOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<CreateAssetOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<UpdateAssetOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<IssueAssetOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<DefineSlateOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<ReleaseEscrowOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<UpdateSigningKeyOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<UpdateBalanceVoteOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<UpdateAssetExtOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<ImessageMemoOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<ContractInfoOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<RegisterContractOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<UpgradeContractOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<DestroyContractOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<WithdrawContractOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<TransferContractOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<BalancesWithdrawOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<TransactionOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<StorageOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<CallContractOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<EventOperation>();
            thinkyoung::blockchain::OperationFactory::instance().register_operation<DepositContractOperation>();
			thinkyoung::blockchain::OperationFactory::instance().register_operation<OnDestroyOperation>();
			thinkyoung::blockchain::OperationFactory::instance().register_operation<OnUpgradeOperation>();
			thinkyoung::blockchain::OperationFactory::instance().register_operation<OnCallSuccessOperation>();

            return true;
        }();

        OperationFactory& OperationFactory::instance()
        {
            static std::unique_ptr<OperationFactory> inst(new OperationFactory());
            return *inst;
        }

        void OperationFactory::to_variant(const thinkyoung::blockchain::Operation& in, fc::variant& output)
        {
            try {
                auto converter_itr = _converters.find(in.type.value);
                FC_ASSERT(converter_itr != _converters.end(), "No such converter!");
                converter_itr->second->to_variant(in, output);
            } FC_RETHROW_EXCEPTIONS(warn, "")
        }

        void OperationFactory::from_variant(const fc::variant& in, thinkyoung::blockchain::Operation& output)
        {
            try {
                auto obj = in.get_object();

                if (obj["type"].as_string() == "define_delegate_slate_op_type")
                {
                    output.type = define_slate_op_type;
                    return;
                }

                output.type = obj["type"].as<OperationTypeEnum>();

                auto converter_itr = _converters.find(output.type.value);
                FC_ASSERT(converter_itr != _converters.end());
                converter_itr->second->from_variant(in, output);
            } FC_RETHROW_EXCEPTIONS(warn, "", ("in", in))
        }

    }
} // thinkyoung::blockchain

namespace fc {
    void to_variant(const thinkyoung::blockchain::Operation& var, variant& vo)
    {
        thinkyoung::blockchain::OperationFactory::instance().to_variant(var, vo);
    }

    void from_variant(const variant& var, thinkyoung::blockchain::Operation& vo)
    {
        thinkyoung::blockchain::OperationFactory::instance().from_variant(var, vo);
    }
}
