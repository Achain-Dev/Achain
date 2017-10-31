#include <blockchain/BalanceOperations.hpp>
#include <blockchain/OperationFactory.hpp>
#include <blockchain/PendingChainState.hpp>
#include <blockchain/TransactionOperations.hpp>
#include <blockchain/TransactionEvaluationState.hpp>
#include <blockchain/AccountOperations.hpp>
#include <blockchain/ContractOperations.hpp> 

#include <blockchain/ForkBlocks.hpp>
#include "wallet/Wallet.hpp"

namespace thinkyoung {
    namespace blockchain {

        TransactionEvaluationState::TransactionEvaluationState(PendingChainState* current_state)
            :_current_state(current_state),
            imessage_length(0), evaluate_contract_result(false),throw_exec_exception(false)
        {
        }

        TransactionEvaluationState::~TransactionEvaluationState()
        {
        }

        bool TransactionEvaluationState::verify_authority(const MultisigMetaInfo& siginfo)
        {
            try {
                uint32_t sig_count = 0;
                ilog("@n verifying authority");
                for (const auto item : siginfo.owners)
                {
                    sig_count += check_signature(item);
                    ilog("@n sig_count: ${s}", ("s", sig_count));
                }
                ilog("@n required: ${s}", ("s", siginfo.required));
                return sig_count >= siginfo.required;
            } FC_CAPTURE_AND_RETHROW((siginfo))
        }

        bool TransactionEvaluationState::check_signature(const Address& a)const
        {
            try {
                return _skip_signature_check || signed_keys.find(a) != signed_keys.end();
            } FC_CAPTURE_AND_RETHROW((a))
        }

        bool TransactionEvaluationState::check_multisig(const MultisigCondition& condition)const
        {
            try {
                uint32_t valid = 0;
                for (auto addr : condition.owners)
                    if (check_signature(addr))
                        valid++;
                return valid >= condition.required;
            } FC_CAPTURE_AND_RETHROW((condition))
        }

        bool TransactionEvaluationState::account_has_signed(const AccountEntry& entry)const
        {
            try {
                if (!entry.is_retracted())
                {
                    if (check_signature(entry.active_key()))
                        return true;

                    if (check_signature(entry.owner_key))
                        return true;
                }
                return false;
            } FC_CAPTURE_AND_RETHROW((entry))
        }

        void TransactionEvaluationState::update_delegate_votes()
        {
            try {
                for (const auto& item : delegate_vote_deltas)
                {
                    const AccountIdType id = item.first;
                    oAccountEntry delegate_entry = _current_state->get_account_entry(id);
                    FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate());

                    const ShareType amount = item.second;
                    delegate_entry->adjust_votes_for(amount);
                    _current_state->store_account_entry(*delegate_entry);
                }
            } FC_CAPTURE_AND_RETHROW()
        }

        void TransactionEvaluationState::validate_required_fee()
        {
            try {
                Asset xts_fees;
                auto fee_itr = balance.find(0);
                if (fee_itr != balance.end()) xts_fees += Asset(fee_itr->second, 0);

                auto max_fee_itr = _max_fee.find(0);
                if (max_fee_itr != _max_fee.end())
                {
                    FC_ASSERT(xts_fees.amount <= max_fee_itr->second, "", ("max_fee", _max_fee)("xts_fees", xts_fees));
                }

                xts_fees += alt_fees_paid;

                if (required_fees > xts_fees)
                {
                    FC_CAPTURE_AND_THROW(insufficient_fee, (required_fees)(alt_fees_paid)(xts_fees));
                }
            } FC_CAPTURE_AND_RETHROW()
        }

        /**
         *  Process all fees and update the asset entrys.
         */
        void TransactionEvaluationState::post_evaluate()
        {
            try {
                for (const auto& item : withdraws)
                {
                    auto asset_rec = _current_state->get_asset_entry(item.first);
                    if (!asset_rec.valid()) FC_CAPTURE_AND_THROW(unknown_asset_id, (item));

                }

                balance[0]; // make sure we have something for this.
                for (const auto& fee : balance)
                {
                    if (fee.second < 0) FC_CAPTURE_AND_THROW(negative_fee, (fee));
                    // if the fee is already in XTS or the fee balance is zero, move along...
                    if (fee.first == 0 || fee.second == 0)
                        continue;

                    auto asset_entry = _current_state->get_asset_entry(fee.first);
                    if (!asset_entry.valid()) FC_CAPTURE_AND_THROW(unknown_asset_id, (fee.first));
                }

                for (const auto& fee : balance)
                {
                    if (fee.second < 0) FC_CAPTURE_AND_THROW(negative_fee, (fee));
                    if (fee.second > 0) // if a fee was paid...
                    {
                        auto asset_entry = _current_state->get_asset_entry(fee.first);
                        if (!asset_entry)
                            FC_CAPTURE_AND_THROW(unknown_asset_id, (fee.first));

                        asset_entry->collected_fees = (fc::safe<ShareType>(asset_entry->collected_fees) + fc::safe<ShareType>(fee.second)).value;
                        _current_state->store_asset_entry(*asset_entry);
                    }
                }
				for (const auto& item : this->withdrawed_contract_balance)
				{
					auto entry=_current_state->get_balance_entry(item);
					if(!(entry.valid()&&entry->balance>=0))
						FC_CAPTURE_AND_THROW(insufficient_funds, (entry->balance));
				}
                /* if( _current_state->get_head_block_num() >= ALP_V0_6_4_FORK_BLOCK_NUM )
                {
                for( const auto& op : trx.operations )
                {
                if( operation_type_enum( op.type ) == cover_op_type )
                {
                FC_ASSERT( balance[ 0 ] <= ALP_BLOCKCHAIN_PRECISION );
                }
                }
                }*/
            } FC_CAPTURE_AND_RETHROW()
        }

        bool TransactionEvaluationState::transaction_entry_analy(const SignedTransaction& trx_arg, std::vector<BalanceEntry>& all_balances, std::vector<AccountEntry>& all_account)
        {
            for (const auto& op : trx.operations)
            {
                if (op.type.value == withdraw_op_type)
                {
                    auto withdraw_op = op.as<WithdrawOperation>();

                    oBalanceEntry current_balance_entry = _current_state->get_balance_entry(withdraw_op.balance_id);
                    if (!current_balance_entry.valid())
                        return false;
                    all_balances.push_back(*current_balance_entry);
                }
                else if (op.type.value == withdraw_pay_op_type)
                {
                    auto withdraw_pay_op = op.as<WithdrawPayOperation>();
                    const AccountIdType account_id = abs(withdraw_pay_op.account_id);
                    oAccountEntry account = _current_state->get_account_entry(account_id);
                    if (!account.valid())
                        return false;

                    if (!account->is_delegate())
                        return false;
                    all_account.push_back(*account);

                }
                else if (op.type.value == register_contract_op_type || op.type.value == call_contract_op_type || op.type.value == transfer_contract_op_type
                    || op.type.value == upgrade_contract_op_type || op.type.value == destroy_contract_op_type)
                {
                    std::map<BalanceIdType, ShareType> balances;
                    if (op.type.value == register_contract_op_type)
                    {
                        auto register_contract_op = op.as<RegisterContractOperation>();
                        balances = register_contract_op.balances;
                    }
                    else if (op.type.value == call_contract_op_type)
                    {
                        auto call_contract_op = op.as<CallContractOperation>();
                        balances = call_contract_op.balances;
                    }
                    else if (op.type.value == transfer_contract_op_type)
                    {
                        auto transfer_contract_op = op.as<TransferContractOperation>();
                        balances = transfer_contract_op.balances;
                    }
                    else if (op.type.value == upgrade_contract_op_type)
                    {
                        auto upgrade_contract_op = op.as<UpgradeContractOperation>();
                        balances = upgrade_contract_op.balances;
                    }
                    else if (op.type.value == destroy_contract_op_type)
                    {
                        auto destroy_contract_op = op.as<DestroyContractOperation>();
                        balances = destroy_contract_op.balances;
                    }

                    for (const auto& balance : balances)
                    {
                        oBalanceEntry current_balance_entry = _current_state->get_balance_entry(balance.first);
                        if (!current_balance_entry.valid())
                            return false;
                        all_balances.push_back(*current_balance_entry);
                    }
                }
            }
            return true;
        }

        bool TransactionEvaluationState::transaction_signature_check(const SignedTransaction& trx_arg,const std::vector<BalanceEntry> all_balances,const std::vector<AccountEntry> all_account)
        {
            const auto trx_digest = trx_arg.digest(_current_state->get_chain_id());
            for (const auto& sig : trx_arg.signatures)
            {
                const auto key = fc::ecc::public_key(sig, trx_digest, _enforce_canonical_signatures).serialize();
                signed_keys.insert(Address(key));
                signed_keys.insert(Address(PtsAddress(key, false, 56)));
                signed_keys.insert(Address(PtsAddress(key, true, 56)));
                signed_keys.insert(Address(PtsAddress(key, false, 0)));
                signed_keys.insert(Address(PtsAddress(key, true, 0)));
            }
            for (const auto& balance_entry : all_balances)
            {
                switch ((WithdrawConditionTypes)balance_entry.condition.type)
                {
                case withdraw_signature_type:
                {
                    const WithdrawWithSignature condition = balance_entry.condition.as<WithdrawWithSignature>();
                    const Address owner = condition.owner;
                    if (!check_signature(owner))
                        return false;
                    break;
                }

                case withdraw_multisig_type:
                {
                    auto multisig = balance_entry.condition.as<WithdrawWithMultisig>();
                    uint32_t valid_signatures = 0;
                    for (const auto& sig : multisig.owners)
                    {
                        // TODO
                        //if( asset_rec->is_restricted() && NOT eval_state._current_state->get_authorization(asset_rec->id, owner) )
                        //continue;
                        valid_signatures += check_signature(sig);
                    }
                    if (valid_signatures < multisig.required)
                        return false;
                    break;
                }

                default:
                    return false;
                }
            }

            for (const auto& account_entry : all_account)
            {
                if (!account_has_signed(account_entry))
                    return false;
            }
            return true;
        }

        void TransactionEvaluationState::evaluate(const SignedTransaction& trx_arg, bool ignore_state)
        {
            SignedTransaction result_trx;
            try {
                trx = trx_arg;
                bool ignore_check_required_fee = false; 
                try {
                    if (_current_state->now() >= trx_arg.expiration)
                    {
                        const auto expired_by_sec = (_current_state->now() - trx_arg.expiration).to_seconds();
                        FC_CAPTURE_AND_THROW(expired_transaction, (trx_arg)(_current_state->now())(expired_by_sec));
                    }

                    // 因为 current_state->now 获取的是slot time，slot_time加上expiration_time很有可能小于交易中的expiration，因此加上一个slot区间
                    if ((_current_state->now() + ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC + ALP_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC) < trx_arg.expiration)
                        FC_CAPTURE_AND_THROW(invalid_transaction_expiration, (trx_arg)(_current_state->now()));

                    if (origin_trx_basic_verify(trx) && (trx.data_size() > ALP_BLOCKCHAIN_MAX_TRX_SIZE))
                    {
                        TransactionIdType trx_id = trx.id();
                        size_t trx_size = trx.data_size();
                        FC_CAPTURE_AND_THROW(out_of_max_trx_size, (trx_id)(trx_size));
                    }

                    if (_current_state->is_known_transaction(trx_arg))
                        FC_CAPTURE_AND_THROW(duplicate_transaction, (trx.id()));

                    if (!trx_arg.check_operations_legality())
                        FC_CAPTURE_AND_THROW(illegal_transaction, (trx_arg)(_current_state->now()));

                    
                    for (const auto& op : trx.operations)
                    {
                        if (
                            (op.type.value >= register_contract_op_type && op.type.value <= transfer_contract_op_type)
                           || (op.type.value == transaction_op_type && trx.result_trx_type == ResultTransactionType::incomplete_result_transaction)  
                           )
                        {
                            ignore_check_required_fee = true;
                            break;
                        }
                    }
					int num_of_signature = trx_arg.signatures.size();
					if (num_of_signature > ALP_BLOCKCHAIN_MAX_SIGNAGTURE_NUM)
					{
						FC_CAPTURE_AND_THROW(too_much_signature, (ALP_BLOCKCHAIN_MAX_SIGNAGTURE_NUM)(num_of_signature));
					}

                    if (!_skip_signature_check)
                    {
                        const auto trx_digest = trx_arg.digest(_current_state->get_chain_id());
						set<fc::ecc::compact_signature> sig_set;
						for (const auto& sig : trx_arg.signatures)//避免对相同的签名做重复解签，可以算是某种优化，但是大部分情况下都没有意义
						{
							sig_set.insert(sig);
						}
						for(const auto& sig:sig_set)
						{
                            const auto key = fc::ecc::public_key(sig, trx_digest, _enforce_canonical_signatures).serialize();
                            signed_keys.insert(Address(key));
                            signed_keys.insert(Address(PtsAddress(key, false, 56)));
                            signed_keys.insert(Address(PtsAddress(key, true, 56)));
                            signed_keys.insert(Address(PtsAddress(key, false, 0)));
                            signed_keys.insert(Address(PtsAddress(key, true, 0)));
                        }
                    }
					
                    current_op_index = 0;
                    result_trx = trx_arg;
                    if (trx_arg.operations.size() > 0 && trx_arg.operations.front().type == OperationTypeEnum::transaction_op_type)
                    {
                        std::vector<Operation>::const_iterator opit = trx_arg.operations.begin();
                        evaluate_operation(*opit);
                        ++opit;
                        ++current_op_index;
                        if (!skipexec)
                        {
                            //FC_ASSERT(is_contract_trxs_same(trx_arg, p_result_trx));//进行operation对比
                            FC_ASSERT(trx_arg.result_trx_id == p_result_trx.id());
                        }
                        evaluate_contract_result = true;
                        while (opit != trx_arg.operations.end())
                        {

                            evaluate_operation(*opit);
                            ++opit;
                            ++current_op_index;
                        }

                        //如果块中有的不完整的结果交易，也需要记录下来
                        if (trx_arg.result_trx_type == ResultTransactionType::incomplete_result_transaction)
                        {
                            trx = trx_arg;
                            _current_state->store_transaction(trx.id(), TransactionEntry(TransactionLocation(), *this));
                            result_trx = p_result_trx;

                            result_trx.result_trx_type = complete_result_transaction;
                            result_trx.result_trx_id = result_trx.id();
                        }

                        evaluate_contract_result = false;
                        p_result_trx.operations.resize(0);
                    }
                    else
                    {
                        for (const auto& op : trx_arg.operations)
                        {
                            evaluate_operation(op);
                            ++current_op_index;
                        }
                    }
					int signum_to_charge = num_of_signature - ALP_BLOCKCHAIN_FREESIGNATURE_LIMIT;
					if (signum_to_charge>=0)
					{
						required_fees.amount += signum_to_charge*ALP_BLOCKCHAIN_EXTRA_SIGNATURE_FEE;
					}
                    if (!ignore_check_required_fee)
                    {
                        post_evaluate();
                        validate_required_fee();
                        update_delegate_votes();
                    }
                    trx = result_trx;
                    _current_state->store_transaction(trx.id(), TransactionEntry(TransactionLocation(), *this));

                    if (!ignore_state && ignore_check_required_fee)
                        FC_CAPTURE_AND_THROW(ignore_check_required_fee_state, (ignore_check_required_fee));
                }
                catch (const fc::exception& e)
                {
                    trx = result_trx;
                    validation_error = e;
                    throw;
                }
            } FC_CAPTURE_AND_RETHROW((result_trx))
        }

        SignedTransaction TransactionEvaluationState::sandbox_evaluate(const SignedTransaction &trx_arg, bool& ignore_check_required_fee)
        {
            try
            {
                trx = trx_arg;

                try
                {
                    if (_current_state->now() >= trx_arg.expiration)
                    {

                        const auto expired_by_sec = (_current_state->now() - trx_arg.expiration).to_seconds();
                        FC_CAPTURE_AND_THROW(expired_transaction, (trx_arg)(_current_state->now())(expired_by_sec));

                    }
                    if ((_current_state->now() + ALP_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC) < trx_arg.expiration)
                        FC_CAPTURE_AND_THROW(invalid_transaction_expiration, (trx_arg)(_current_state->now()));

                    if (_current_state->is_known_transaction(trx_arg))
                        FC_CAPTURE_AND_THROW(duplicate_transaction, (trx.id()));

					if (!trx_arg.check_operations_legality())
						FC_CAPTURE_AND_THROW(illegal_transaction, (trx_arg)(_current_state->now()));


					//check the trx is origin transaction or not
					if (trx.operations.size() == 1 &&
						(trx.operations.front().type.value >= register_contract_op_type	&& trx.operations.front().type.value <= transfer_contract_op_type))
					{
						ignore_check_required_fee = true;
					}


                    if (!_skip_signature_check)
                    {
                        const auto trx_digest = trx_arg.digest(_current_state->get_chain_id());
                        for (const auto& sig : trx_arg.signatures)
                        {
                            const auto key = fc::ecc::public_key(sig, trx_digest, _enforce_canonical_signatures).serialize();
                            signed_keys.insert(Address(key));
                            signed_keys.insert(Address(PtsAddress(key, false, 56)));
                            signed_keys.insert(Address(PtsAddress(key, true, 56)));
                            signed_keys.insert(Address(PtsAddress(key, false, 0)));
                            signed_keys.insert(Address(PtsAddress(key, true, 0)));
                        }
                    }
                    current_op_index = 0;
                    if (trx_arg.operations.size() > 0 && trx_arg.operations.front().type == OperationTypeEnum::transaction_op_type)
                    {
                        std::vector<Operation>::const_iterator opit = trx_arg.operations.begin();
                        evaluate_operation(*opit);
                        ++opit;
                        ++current_op_index;
                        if (!skipexec)
                        {
                            FC_ASSERT(is_contract_trxs_same(trx_arg, p_result_trx));//进行operation对比
                        }

                        evaluate_contract_result = true;
                        while (opit != trx_arg.operations.end())
                        {

                            evaluate_operation(*opit);
                            ++opit;
                            ++current_op_index;
                        }
                        evaluate_contract_result = false;
                        p_result_trx.operations.resize(0);
                    }
                    else
                    {
                        for (const auto& op : trx_arg.operations)
                        {
                            evaluate_operation(op);
                            ++current_op_index;
                        }
                    }

					if (!ignore_check_required_fee)
					{
						post_evaluate();
						validate_required_fee();
						update_delegate_votes();

					}
                   
                    trx = trx_arg;
					p_result_trx.expiration = trx_arg.expiration;
                }
                catch (const fc::exception& e)
                {
                    trx = trx_arg;
                    validation_error = e;
                    throw;
                }

				return p_result_trx;

            } FC_CAPTURE_AND_RETHROW((trx_arg))

        }

        void TransactionEvaluationState::evaluate_operation(const Operation& op)
        {
            try {
                OperationFactory::instance().evaluate(*this, op);
            } FC_CAPTURE_AND_RETHROW((op))
        }

        void TransactionEvaluationState::adjust_vote(const SlateIdType slate_id, const ShareType amount)
        {
            try {
                if (slate_id == 0 || _skip_vote_adjustment)
                    return;

                const oSlateEntry slate_entry = _current_state->get_slate_entry(slate_id);
                if (!slate_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_delegate_slate, (slate_id));

                if (slate_entry->duplicate_slate.empty())
                {
                    for (const AccountIdType id : slate_entry->slate)
                    {
                        if (id >= 0)
                            delegate_vote_deltas[id] = (fc::safe<ShareType>(delegate_vote_deltas[id]) + fc::safe<ShareType>(amount)).value;

                    }
                }
                else
                {
                    for (const AccountIdType id : slate_entry->duplicate_slate)
                        delegate_vote_deltas[id] = (fc::safe<ShareType>(delegate_vote_deltas[id]) + fc::safe<ShareType>(amount)).value;
                }
            } FC_CAPTURE_AND_RETHROW((slate_id)(amount))
        }

        ShareType TransactionEvaluationState::get_fees(AssetIdType id)const
        {
            try {
                auto itr = balance.find(id);
                if (itr != balance.end())
                    return itr->second;
                return 0;
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void TransactionEvaluationState::sub_balance(const BalanceIdType& balance_id, const Asset& amount)
        {
            try {
                fc::safe<ShareType> balance_t = balance[amount.asset_id];
                fc::safe<ShareType> deposits_t = deposits[amount.asset_id];
                fc::safe<ShareType> deltas_t = deltas[current_op_index][amount.asset_id];

                balance_t -= amount.amount;
                deposits_t += amount.amount;
                deltas_t += amount.amount;
                balance[amount.asset_id] = balance_t.value;
                deposits[amount.asset_id] = deposits_t.value;
                deltas[current_op_index][amount.asset_id] = deltas_t.value;
            } FC_CAPTURE_AND_RETHROW((balance_id)(amount))
        }

        void TransactionEvaluationState::add_balance(const Asset& amount)
        {
            try {
                fc::safe<ShareType> balance_t = balance[amount.asset_id];
                fc::safe<ShareType> withdraws_t = withdraws[amount.asset_id];
                fc::safe<ShareType> deltas_t = deltas[current_op_index][amount.asset_id];
                balance_t += amount.amount;
                withdraws_t += amount.amount;
                deltas_t -= amount.amount;
                balance[amount.asset_id] = balance_t.value;
                withdraws[amount.asset_id] = withdraws_t.value;
                deltas[current_op_index][amount.asset_id] = deltas_t.value;
            } FC_CAPTURE_AND_RETHROW((amount))
        }

        /**
         *  Throws if the asset is not known to the blockchain.
         */
        void TransactionEvaluationState::validate_asset(const Asset& asset_to_validate)const
        {
            try {
                auto asset_rec = _current_state->get_asset_entry(asset_to_validate.asset_id);
                if (NOT asset_rec)
                    FC_CAPTURE_AND_THROW(unknown_asset_id, (asset_to_validate));
            } FC_CAPTURE_AND_RETHROW((asset_to_validate))
        }

        bool TransactionEvaluationState::scan_deltas(const uint32_t op_index, const function<bool(const Asset&)> callback)const
        {
            try {
                bool ret = false;
                for (const auto& item : deltas)
                {
                    const uint32_t index = item.first;
                    if (index != op_index) continue;
                    for (const auto& delta_item : item.second)
                    {
                        const Asset delta_amount(delta_item.second, delta_item.first);
                        ret |= callback(delta_amount);
                    }
                }
                return ret;
            } FC_CAPTURE_AND_RETHROW((op_index))
        }

        void TransactionEvaluationState::scan_addresses(const ChainInterface& chain,
            const function<void(const Address&)> callback)const
        {
            try {
                for (const Operation& op : trx.operations)
                {
                    switch (OperationTypeEnum(op.type))
                    {
                    case withdraw_op_type:
                    {
                        const oBalanceEntry balance_entry = chain.get_balance_entry(op.as<WithdrawOperation>().balance_id);
                        if (balance_entry.valid())
                        {
                            const set<Address>& owners = balance_entry->owners();
                            for (const Address& addr : owners)
                                callback(addr);
                        }
                        break;
                    }
                    case deposit_op_type:
                    {
                        const set<Address>& owners = op.as<DepositOperation>().condition.owners();
                        for (const Address& addr : owners)
                            callback(addr);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                    }
                }
            } FC_CAPTURE_AND_RETHROW()
        }


        bool TransactionEvaluationState::is_contract_trxs_same(const SignedTransaction& l_trx, const SignedTransaction& r_trx)const
        {
            try {
                if (l_trx.operations.size() != r_trx.operations.size())
                    return false;

                int op_size = l_trx.operations.size();

                for (int i = 0; i < op_size; ++i)
                {
                    if (NOT is_contract_op_same(l_trx.operations[i], r_trx.operations[i]))
                        return false;
                }

                return true;
            } FC_CAPTURE_AND_RETHROW()
        }

        bool TransactionEvaluationState::is_contract_op_same(const Operation& l_op, const Operation& r_op)const
        {
            try {
                if (l_op.type != r_op.type)
                    return false;

                Operation ll_op(l_op);
                Operation rr_op(r_op);

                if (l_op.type == transaction_op_type)
                {
                    TransactionOperation l_trx_op = l_op.as<TransactionOperation>();
                    SignedTransaction l_trx = l_trx_op.trx;

                    TransactionOperation r_trx_op = r_op.as<TransactionOperation>();
                    SignedTransaction r_trx = r_trx_op.trx;

                    l_trx.expiration = fc::time_point();
                    r_trx.expiration = fc::time_point();

                    ll_op = Operation(TransactionOperation(l_trx));
                    rr_op = Operation(TransactionOperation(r_trx));
                }

                fc::ripemd160 l_hash, r_hash;
                fc::sha512::encoder l_enc;
                fc::raw::pack(l_enc, ll_op);
                l_hash = fc::ripemd160::hash(l_enc.result());

                fc::sha512::encoder r_enc;
                fc::raw::pack(r_enc, rr_op);
                r_hash = fc::ripemd160::hash(r_enc.result());

                if (l_hash == r_hash)
                    return true;
                else
                    return false;

            } FC_CAPTURE_AND_RETHROW()
        }

        bool TransactionEvaluationState::transfer_asset_from_contract(ShareType real_amount_to_transfer, const string& amount_to_transfer_symbol, const Address& from_contract_address, const string& to_account_name)
        {
            try {
                SignedTransaction trx = _current_state->transfer_asset_from_contract(real_amount_to_transfer, amount_to_transfer_symbol, from_contract_address, to_account_name, p_result_trx);
                return true;
            } FC_CAPTURE_AND_RETHROW()
        }

        bool TransactionEvaluationState::transfer_asset_from_contract(ShareType real_amount_to_transfer, const string& amount_to_transfer_symbol, const Address& from_contract_address, const Address& to_account_address)
        {
            try {
                SignedTransaction trx = _current_state->transfer_asset_from_contract(real_amount_to_transfer, amount_to_transfer_symbol, from_contract_address, to_account_address, p_result_trx);
                return true;
            } FC_CAPTURE_AND_RETHROW()
        }

        bool TransactionEvaluationState::is_contract_op(const thinkyoung::blockchain::OperationTypeEnum& op_type)const
        {
            if (op_type >= thinkyoung::blockchain::OperationTypeEnum::register_contract_op_type
                && op_type <= thinkyoung::blockchain::OperationTypeEnum::transfer_contract_op_type)
                return true;
            return false;
        }

        bool TransactionEvaluationState::origin_trx_basic_verify(const SignedTransaction& trx)const
        {
            bool has_contract_op = false;

            for (const auto& op : trx.operations)
            {
                if (op.type == thinkyoung::blockchain::OperationTypeEnum::transaction_op_type)
                    return false;
                else if (is_contract_op(op.type))
                {
                    has_contract_op = true;
                    break;
                }
            }

            if (has_contract_op == true && trx.operations.size() != 1)
                return false;

            return true;
        }

    }
} // thinkyoung::blockchain
