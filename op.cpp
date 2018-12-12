            fc::async([=](){ my->write_to_mysqls(entry); }, "write_block_entry_to_mysql");



balances_operations
	
	ACT transfer
    WithdrawOperation::type = withdraw_op_type;
    DepositOperation::type = deposit_op_type;     

    WithdrawContractOperation::type = withdraw_contract_op_type;  合约账户中取出钱
    DepositContractOperation::type = deposit_contract_op_type; 
    
    BalancesWithdrawOperation::type = balances_withdraw_op_type;

    UpdateBalanceVoteOperation::type = update_balance_vote_op_type;
    ReleaseEscrowOperation::type = release_escrow_op_type;  // not used
    balance_id_to_entry
    
contract_operation
    RegisterContractOperation::type = register_contract_op_type;
    UpgradeContractOperation::type = upgrade_contract_op_type;
    CallContractOperation::type = call_contract_op_type;
    DestroyContractOperation::type = destroy_contract_op_type;
    TransferContractOperation::type = transfer_contract_op_type;

    ContractInfoOperation::type =  contract_info_op_type;
    OnCallSuccessOperation::type = on_call_success_op_type;
    OnDestroyOperation::type =     on_destroy_op_type;
    OnUpgradeOperation::type =     on_upgrade_op_type;
_contract_to_trx_iddb
_trx_to_contract_iddb
contract_name_to_id
contract_id_to_entry
contract_id_to_storage



asset_operation
    CreateAssetOperation::type = create_asset_op_type;
    UpdateAssetOperation::type = update_asset_op_type;
    UpdateAssetExtOperation::type = update_asset_ext_op_type;
    IssueAssetOperation::type = issue_asset_op_type;
	asset_id_to_entry
	asset_symbol_to_id
	
TransactionOperation
    TransactionOperation::type = transaction_op_type;
    _request_to_result_iddb
    _result_to_request_id

	slate_operation
    DefineSlateOperation::type = define_slate_op_type;
    slate_id_to_entry


account_operation
    RegisterAccountOperation::type = register_account_op_type;
    UpdateAccountOperation::type = update_account_op_type;
    UpdateSigningKeyOperation::type = update_signing_key_op_type;
    WithdrawPayOperation::type = withdraw_pay_op_type;
account_address_to_id
account_id_to_entry
account_name_to_id
	
ImessageOperation
    ImessageMemoOperation::type = imessage_memo_op_type;
	
storage_operation
    StorageOperation::type = storage_op_type;
	contract_id_to_storage
	enum TrxTypeEnum {
		trx_type_undefined = 10,
		trx_type_asset_transfer = 11,
		trx_type_agent_get_pay = 12,
		trx_type_register_account = 13,
		trx_type_update_account = 14,
		trx_type_create_asset = 15,
		trx_type_update_asset = 16,

		trx_type_register_contract = 21,
		trx_type_upgrade_contract = 22,
		trx_type_contract_recharge = 23,
		trx_type_destroy_contract = 24,
		trx_type_call_contract = 25,
		trx_type_contract_transfer = 26,

		trx_type_register_contract_result = 31,
		trx_type_upgrade_contract_result = 32,
		trx_type_contract_recharge_result = 33,
		trx_type_destroy_contract_result = 34,
		trx_type_call_contract_result = 35,
		trx_type_contract_result = 36

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
