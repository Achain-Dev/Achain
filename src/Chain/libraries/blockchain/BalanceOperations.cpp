#include <blockchain/BalanceOperations.hpp>
#include <blockchain/Exceptions.hpp>
#include <blockchain/PendingChainState.hpp>
#include <blockchain/TransactionOperations.hpp>
#include <blockchain/ForkBlocks.hpp>
#include <sstream>


namespace thinkyoung {
    namespace blockchain {

        BalanceIdType DepositOperation::balance_id()const
        {
            return condition.get_address();
        }

        DepositOperation::DepositOperation(const Address& owner,
            const Asset& amnt,
            SlateIdType slate_id)
        {
            FC_ASSERT(amnt.amount > 0, "Amount should be bigger than 0");
            amount = amnt.amount;
            condition = WithdrawCondition(WithdrawWithSignature(owner),
                amnt.asset_id, slate_id);
        }

        void DepositOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
				if (eval_state.deposit_count > ALP_BLOCKCHAIN_TRANSACTION_MAX_DEPOSIT_NUM)
					FC_CAPTURE_AND_THROW(too_much_deposit,(eval_state.deposit_count));
				++eval_state.deposit_count;
                if (this->amount <= 0)
                    FC_CAPTURE_AND_THROW(negative_deposit, (amount));

                switch (WithdrawConditionTypes(this->condition.type))
                {
                case withdraw_signature_type:
                case withdraw_multisig_type:
                case withdraw_escrow_type:
                    break;
                default:
                    FC_CAPTURE_AND_THROW(invalid_withdraw_condition, (*this));
                }


                const BalanceIdType deposit_balance_id = this->balance_id();

                oBalanceEntry cur_entry = eval_state._current_state->get_balance_entry(deposit_balance_id);
                if (!cur_entry.valid())
                {
                    cur_entry = BalanceEntry(this->condition);
                    if (this->condition.type == withdraw_escrow_type)
                        cur_entry->meta_data = variant_object("creating_transaction_id", eval_state.trx.id());
                }
				auto owner = cur_entry->owner();
				if (owner.valid())
				{
					if (!eval_state.deposit_address.insert(*owner).second)
					{
						FC_CAPTURE_AND_THROW(deposit_to_one_address_twice,(*owner));
					}
				}
                if (cur_entry->balance == 0)
                {
                    cur_entry->deposit_date = eval_state._current_state->now();
                }
                else
                {
                    fc::uint128 old_sec_since_epoch(cur_entry->deposit_date.sec_since_epoch());
                    fc::uint128 new_sec_since_epoch(eval_state._current_state->now().sec_since_epoch());

                    fc::uint128 avg = (old_sec_since_epoch * cur_entry->balance) + (new_sec_since_epoch * this->amount);
                    avg /= (fc::uint128(cur_entry->balance) + fc::uint128(this->amount));

                    cur_entry->deposit_date = time_point_sec(avg.to_integer());
                }
                fc::safe<ShareType> temp = cur_entry->balance;
                temp += this->amount;
                cur_entry->balance = temp.value;
                eval_state.sub_balance(deposit_balance_id, Asset(this->amount, cur_entry->condition.asset_id));

                if (cur_entry->condition.asset_id == 0 && cur_entry->condition.slate_id)
                    eval_state.adjust_vote(cur_entry->condition.slate_id, this->amount);

                cur_entry->last_update = eval_state._current_state->now();

                const oAssetEntry asset_rec = eval_state._current_state->get_asset_entry(cur_entry->condition.asset_id);
                FC_ASSERT(asset_rec.valid(), "Invalid asset entry");

                // if( eval_state._current_state->get_head_block_num() >= ALP_V0_6_0_FORK_BLOCK_NUM )
                //{
                //FC_ASSERT( !eval_state._current_state->is_fraudulent_asset( *asset_rec ) );
                //}
                if (asset_rec->is_restricted())
                {
                    for (const auto& owner : cur_entry->owners())
                    {
                        // TODO
                        //FC_ASSERT( eval_state._current_state->get_authorization(asset_rec->id, owner) );
                    }
                }

                eval_state._current_state->store_balance_entry(*cur_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void WithdrawOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {

                if (this->amount <= 0)
                    FC_CAPTURE_AND_THROW(negative_withdraw, (amount));

                oBalanceEntry current_balance_entry = eval_state._current_state->get_balance_entry(this->balance_id);
                if (!current_balance_entry.valid())
                    FC_CAPTURE_AND_THROW(unknown_balance_entry, (balance_id));

                if (this->amount > current_balance_entry->get_spendable_balance(eval_state._current_state->now()).amount)
                    FC_CAPTURE_AND_THROW(insufficient_funds, (current_balance_entry)(amount));

                auto asset_rec = eval_state._current_state->get_asset_entry(current_balance_entry->condition.asset_id);
                FC_ASSERT(asset_rec.valid(), "Invalid asset entry");
                bool issuer_override = asset_rec->is_retractable() && eval_state.verify_authority(asset_rec->authority);

                if (!issuer_override)
                {
                    FC_ASSERT(!asset_rec->is_balance_frozen(), "Balance frozen");

                    switch ((WithdrawConditionTypes)current_balance_entry->condition.type)
                    {
                    case withdraw_signature_type:
                    {
                        const WithdrawWithSignature condition = current_balance_entry->condition.as<WithdrawWithSignature>();
                        const Address owner = condition.owner;
                        if (!eval_state.check_signature(owner))
                            FC_CAPTURE_AND_THROW(missing_signature, (owner));
                        // TODO
                        //if( asset_rec->is_restricted() )
                        //FC_ASSERT( eval_state._current_state->get_authorization(asset_rec->id, owner) );
                        break;
                    }

                    case withdraw_multisig_type:
                    {
                        auto multisig = current_balance_entry->condition.as<WithdrawWithMultisig>();
                        uint32_t valid_signatures = 0;
                        for (const auto& sig : multisig.owners)
                        {
                            // TODO
                            //if( asset_rec->is_restricted() && NOT eval_state._current_state->get_authorization(asset_rec->id, owner) )
                            //continue;
                            valid_signatures += eval_state.check_signature(sig);
                        }
                        if (valid_signatures < multisig.required)
                            FC_CAPTURE_AND_THROW(missing_signature, (valid_signatures)(multisig));
                        break;
                    }

                    default:
                        FC_CAPTURE_AND_THROW(invalid_withdraw_condition, (current_balance_entry->condition));
                    }
                }

                // update delegate vote on withdrawn account..
                if (current_balance_entry->condition.asset_id == 0 && current_balance_entry->condition.slate_id)
                    eval_state.adjust_vote(current_balance_entry->condition.slate_id, -this->amount);
                fc::safe<ShareType> temp = current_balance_entry->balance;
                temp -= this->amount;
                current_balance_entry->balance = temp.value;
                eval_state.add_balance(Asset(this->amount, current_balance_entry->condition.asset_id));
				auto owner = current_balance_entry->owner();
				if (temp > 0 && owner.valid())
				{
					auto res=eval_state.owner_balance_not_usedup.insert(*owner);
					if (!res.second)
						FC_CAPTURE_AND_THROW(too_much_balances_withdraw_not_used_up,(*owner));
				}
                current_balance_entry->last_update = eval_state._current_state->now();
				
                eval_state._current_state->store_balance_entry(*current_balance_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void WithdrawContractOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(in_result_of_execute, ("WithdrawContractOperation can only in result transaction"));
				BalanceEntry balance_entry(WithdrawCondition(WithdrawWithSignature(contract), 0, 0, withdraw_contract_type));

                if (this->amount <= 0)
                    FC_CAPTURE_AND_THROW(negative_withdraw, (amount));
				
                oBalanceEntry current_balance_entry = eval_state._current_state->get_balance_entry(this->balance_id);
				if (!current_balance_entry.valid())
				{
					if (contract == ContractIdType())
					{
						FC_CAPTURE_AND_THROW(unknown_balance_entry,(balance_id));
					}
					else
					{
						FC_ASSERT(balance_id == balance_entry.condition.get_address());
					}
					balance_entry.balance = 0;
					balance_entry.deposit_date = eval_state._current_state->now();
					balance_entry.last_update = eval_state._current_state->now();
					current_balance_entry = balance_entry;
				}
                FC_ASSERT(current_balance_entry->condition.asset_id == 0, "Invalid balance_id, asset type should be ALP");
                auto asset_rec = eval_state._current_state->get_asset_entry(current_balance_entry->condition.asset_id);
                FC_ASSERT(asset_rec.valid(), "Invalid asset entry");

                current_balance_entry->balance -= this->amount;
                eval_state.add_balance(Asset(this->amount, current_balance_entry->condition.asset_id));

                current_balance_entry->last_update = eval_state._current_state->now();

                eval_state._current_state->store_balance_entry(*current_balance_entry);
				eval_state.withdrawed_contract_balance.push_back(balance_id);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void ReleaseEscrowOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                FC_ASSERT(!"This operation is not enabled yet!");

                auto escrow_balance_entry = eval_state._current_state->get_balance_entry(this->escrow_id);
                FC_ASSERT(escrow_balance_entry.valid(), "Invalid balance entry");

                if (!eval_state.check_signature(this->released_by))
                    FC_ASSERT(false, "transaction not signed by releasor");
                FC_ASSERT(escrow_balance_entry->balance >= 0);
                auto escrow_condition = escrow_balance_entry->condition.as<withdraw_with_escrow>();
                auto total_released = uint64_t((fc::safe<ShareType>(amount_to_sender) + fc::safe<ShareType>(amount_to_receiver)).value);
                //auto total_released = uint64_t(amount_to_sender) + uint64_t(amount_to_receiver);

                FC_ASSERT(total_released <= static_cast<uint64_t>(escrow_balance_entry->balance));
                FC_ASSERT(total_released >= amount_to_sender); // check for addition overflow
                FC_ASSERT(total_released >= amount_to_receiver); // check for addition overflow

                escrow_balance_entry->balance -= total_released;
                auto asset_rec = eval_state._current_state->get_asset_entry(escrow_balance_entry->condition.asset_id);

                if (asset_rec->is_restricted())
                {
                    // TODO
                    //if( amount_to_sender > 0 )
                    //FC_ASSERT( eval_state._current_state->get_authorization( escrow_balance_entry->condition.asset_id, escrow_condition.sender ) );
                    //if( amount_to_receiver > 0 )
                    //FC_ASSERT( eval_state._current_state->get_authorization( escrow_balance_entry->condition.asset_id, escrow_condition.receiver ) );
                }

                bool retracting = false;
                if (asset_rec->is_retractable())
                {
                    if (eval_state.verify_authority(asset_rec->authority))
                    {
                        retracting = true;
                    }
                }

                if (escrow_condition.sender == this->released_by)
                {
                    FC_ASSERT(amount_to_sender == 0);
                    FC_ASSERT(amount_to_receiver <= escrow_balance_entry->balance);

                    if (!eval_state.check_signature(escrow_condition.sender) && !retracting)
                        FC_CAPTURE_AND_THROW(missing_signature, (escrow_condition.sender));

                    BalanceEntry new_balance_entry(escrow_condition.receiver,
                        Asset(amount_to_receiver, escrow_balance_entry->asset_id()),
                        escrow_balance_entry->slate_id());
                    auto current_receiver_balance = eval_state._current_state->get_balance_entry(new_balance_entry.id());

                    if (current_receiver_balance)
                        current_receiver_balance->balance += amount_to_receiver;
                    else
                        current_receiver_balance = new_balance_entry;

                    eval_state._current_state->store_balance_entry(*current_receiver_balance);
                }
                else if (escrow_condition.receiver == this->released_by)
                {
                    FC_ASSERT(amount_to_receiver == 0);
                    FC_ASSERT(amount_to_sender <= escrow_balance_entry->balance);

                    if (!eval_state.check_signature(escrow_condition.receiver) && !retracting)
                        FC_CAPTURE_AND_THROW(missing_signature, (escrow_condition.receiver));

                    BalanceEntry new_balance_entry(escrow_condition.sender,
                        Asset(amount_to_sender, escrow_balance_entry->asset_id()),
                        escrow_balance_entry->slate_id());
                    auto current_sender_balance = eval_state._current_state->get_balance_entry(new_balance_entry.id());

                    if (current_sender_balance)
                        current_sender_balance->balance += amount_to_sender;
                    else
                        current_sender_balance = new_balance_entry;

                    eval_state._current_state->store_balance_entry(*current_sender_balance);
                }
                else if (escrow_condition.escrow == this->released_by)
                {
                    if (!eval_state.check_signature(escrow_condition.escrow) && !retracting)
                        FC_CAPTURE_AND_THROW(missing_signature, (escrow_condition.escrow));
                    // get a balance entry for the receiver, create it if necessary and deposit funds
                    {
                        BalanceEntry new_balance_entry(escrow_condition.receiver,
                            Asset(amount_to_receiver, escrow_balance_entry->asset_id()),
                            escrow_balance_entry->slate_id());
                        auto current_receiver_balance = eval_state._current_state->get_balance_entry(new_balance_entry.id());

                        if (current_receiver_balance)
                            current_receiver_balance->balance += amount_to_receiver;
                        else
                            current_receiver_balance = new_balance_entry;
                        eval_state._current_state->store_balance_entry(*current_receiver_balance);
                    }
                    //  get a balance entry for the sender, create it if necessary and deposit funds
         {
             BalanceEntry new_balance_entry(escrow_condition.sender,
                 Asset(amount_to_sender, escrow_balance_entry->asset_id()),
                 escrow_balance_entry->slate_id());
             auto current_sender_balance = eval_state._current_state->get_balance_entry(new_balance_entry.id());

             if (current_sender_balance)
                 current_sender_balance->balance += amount_to_sender;
             else
                 current_sender_balance = new_balance_entry;
             eval_state._current_state->store_balance_entry(*current_sender_balance);
         }
                }
                else if (Address() == this->released_by)
                {
                    if (!eval_state.check_signature(escrow_condition.sender) && !retracting)
                        FC_CAPTURE_AND_THROW(missing_signature, (escrow_condition.sender));
                    if (!eval_state.check_signature(escrow_condition.receiver) && !retracting)
                        FC_CAPTURE_AND_THROW(missing_signature, (escrow_condition.receiver));
                    // get a balance entry for the receiver, create it if necessary and deposit funds
                    {
                        BalanceEntry new_balance_entry(escrow_condition.receiver,
                            Asset(amount_to_receiver, escrow_balance_entry->asset_id()),
                            escrow_balance_entry->slate_id());
                        auto current_receiver_balance = eval_state._current_state->get_balance_entry(new_balance_entry.id());

                        if (current_receiver_balance)
                            current_receiver_balance->balance += amount_to_receiver;
                        else
                            current_receiver_balance = new_balance_entry;
                        eval_state._current_state->store_balance_entry(*current_receiver_balance);
                    }
                    //  get a balance entry for the sender, create it if necessary and deposit funds
         {
             BalanceEntry new_balance_entry(escrow_condition.sender,
                 Asset(amount_to_sender, escrow_balance_entry->asset_id()),
                 escrow_balance_entry->slate_id());
             auto current_sender_balance = eval_state._current_state->get_balance_entry(new_balance_entry.id());

             if (current_sender_balance)
                 current_sender_balance->balance += amount_to_sender;
             else
                 current_sender_balance = new_balance_entry;
             eval_state._current_state->store_balance_entry(*current_sender_balance);
         }
                }
                else
                {
                    FC_ASSERT(false, "not released by a party to the escrow transaction");
                }

                eval_state._current_state->store_balance_entry(*escrow_balance_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        void BalancesWithdrawOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(not_be_result_of_execute, ("BalancesWithdrawOperation can only exsit in result"));

                Address contract_operator_address = Address(eval_state.contract_operator);
                if (!eval_state.check_signature(contract_operator_address))
                    FC_CAPTURE_AND_THROW(missing_signature, (contract_operator_address));

                std::map < BalanceIdType, ShareType > ::const_iterator it = balances.begin();
                while (it != balances.end())
                {
                    if (it->second <= 0)
                        FC_CAPTURE_AND_THROW(negative_withdraw, (it->second));

                    oBalanceEntry current_balance_entry = eval_state._current_state->get_balance_entry(it->first);
                    if (!current_balance_entry.valid())
                        FC_CAPTURE_AND_THROW(unknown_balance_entry, (it->first));

                    FC_ASSERT(current_balance_entry->asset_id() == 0, "BalancesWithdrawOperation asset must use ALP");

                    if (it->second > current_balance_entry->get_spendable_balance(eval_state._current_state->now()).amount)
                        FC_CAPTURE_AND_THROW(insufficient_funds, (current_balance_entry)(it->second));

                    auto asset_rec = eval_state._current_state->get_asset_entry(current_balance_entry->condition.asset_id);
                    FC_ASSERT(asset_rec.valid(), "Invalid asset entry");
                    bool issuer_override = asset_rec->is_retractable() && eval_state.verify_authority(asset_rec->authority);

                    if (!issuer_override)
                    {
                        FC_ASSERT(!asset_rec->is_balance_frozen(), "Balance frozen");

                        switch ((WithdrawConditionTypes)current_balance_entry->condition.type)
                        {
                        case withdraw_signature_type:
                        {
                            const WithdrawWithSignature condition = current_balance_entry->condition.as<WithdrawWithSignature>();
                            const Address owner = condition.owner;
                            if (!eval_state.check_signature(owner))
                                FC_CAPTURE_AND_THROW(missing_signature, (owner));
                            // TODO
                            //if( asset_rec->is_restricted() )
                            //FC_ASSERT( eval_state._current_state->get_authorization(asset_rec->id, owner) );

                            if (Address(eval_state.contract_operator) != owner)
                                FC_CAPTURE_AND_THROW(operator_and_owner_not_the_same, (eval_state.contract_operator)(it->first));

                            break;
                        }

                        default:
                            FC_CAPTURE_AND_THROW(invalid_withdraw_condition, (current_balance_entry->condition));
                        }
                    }

                    // update delegate vote on withdrawn account..
                    if (current_balance_entry->condition.asset_id == 0 && current_balance_entry->condition.slate_id)
                        eval_state.adjust_vote(current_balance_entry->condition.slate_id, -it->second);

                    current_balance_entry->balance -= it->second;
                    eval_state.add_balance(Asset(it->second, current_balance_entry->condition.asset_id));

                    current_balance_entry->last_update = eval_state._current_state->now();

                    eval_state._current_state->store_balance_entry(*current_balance_entry);
                    ++it;
                }

            }FC_CAPTURE_AND_RETHROW((*this));
        }

        void UpdateBalanceVoteOperation::evaluate(TransactionEvaluationState& eval_state)const
        {
            try {
                FC_ASSERT(false, "Disable UpdateBalanceVoteOperation!");
                auto current_balance_entry = eval_state._current_state->get_balance_entry(this->balance_id);
                FC_ASSERT(current_balance_entry.valid(), "No such balance!");
                FC_ASSERT(current_balance_entry->condition.asset_id == 0, "Only ALP balances can have restricted owners.");
                FC_ASSERT(current_balance_entry->condition.type == withdraw_signature_type, "Restricted owners not enabled for anything but basic balances");

                // TODO
                //if( asset_rec->is_restricted() )
                //FC_ASSERT( eval_state._current_state->get_authorization(asset_rec->id, owner) );

                auto last_update_secs = current_balance_entry->last_update.sec_since_epoch();
                ilog("last_update_secs is: ${secs}", ("secs", last_update_secs));

                auto balance = current_balance_entry->balance;
                auto fee = ALP_BLOCKCHAIN_PRECISION / 2;
                FC_ASSERT(balance > fee);

                auto asset_rec = eval_state._current_state->get_asset_entry(current_balance_entry->condition.asset_id);

                if (current_balance_entry->condition.slate_id)
                {
                    eval_state.adjust_vote(current_balance_entry->condition.slate_id, -balance);
                }
                current_balance_entry->balance -= balance;
                current_balance_entry->last_update = eval_state._current_state->now();

                ilog("I'm storing a balance entry whose last update is: ${secs}", ("secs", current_balance_entry->last_update));
                eval_state._current_state->store_balance_entry(*current_balance_entry);

                auto new_restricted_owner = current_balance_entry->restricted_owner;
                auto new_slate = current_balance_entry->condition.slate_id;


                if (this->new_restricted_owner.valid() && (this->new_restricted_owner != new_restricted_owner))
                {
                    ilog("@n new restricted owner specified and its not the existing one");
                    for (const auto& owner : current_balance_entry->owners()) //eventually maybe multisig can delegate vote
                    {
                        if (!eval_state.check_signature(owner))
                            FC_CAPTURE_AND_THROW(missing_signature, (owner));
                    }
                    new_restricted_owner = this->new_restricted_owner;
                    new_slate = this->new_slate;
                }
                else // NOT this->new_restricted_owner.valid() || (this->new_restricted_owner == new_restricted_owner)
                {
                    auto restricted_owner = current_balance_entry->restricted_owner;
                    /*
                    FC_ASSERT( restricted_owner.valid(),
                    "Didn't specify a new restricted owner, but one currently exists." );
                    */
                    ilog("@n now: ${secs}", ("secs", eval_state._current_state->now().sec_since_epoch()));
                    ilog("@n last update: ${secs}", ("secs", last_update_secs));
                    FC_ASSERT(eval_state._current_state->now().sec_since_epoch() - last_update_secs
                        >= ALP_BLOCKCHAIN_VOTE_UPDATE_PERIOD_SEC,
                        "You cannot update your vote this frequently with only the voting key!");

                    if (NOT eval_state.check_signature(*restricted_owner))
                    {
                        for (const auto& owner : current_balance_entry->owners()) //eventually maybe multisig can delegate vote
                        {
                            if (NOT eval_state.check_signature(owner))
                                FC_CAPTURE_AND_THROW(missing_signature, (owner));
                        }
                    }
                    new_slate = this->new_slate;
                }

                const auto owner = current_balance_entry->owner();
                FC_ASSERT(owner.valid(), "Invalid owner");
                WithdrawCondition new_condition(WithdrawWithSignature(*owner), 0, new_slate);
                BalanceEntry newer_balance_entry(new_condition);
                auto new_balance_entry = eval_state._current_state->get_balance_entry(newer_balance_entry.id());
                if (!new_balance_entry.valid())
                    new_balance_entry = current_balance_entry;
                new_balance_entry->condition = new_condition;

                if (new_balance_entry->balance == 0)
                {
                    new_balance_entry->deposit_date = eval_state._current_state->now();
                }
                else
                {
                    fc::uint128 old_sec_since_epoch(current_balance_entry->deposit_date.sec_since_epoch());
                    fc::uint128 new_sec_since_epoch(eval_state._current_state->now().sec_since_epoch());

                    fc::uint128 avg = (old_sec_since_epoch * new_balance_entry->balance) + (new_sec_since_epoch * balance);
                    avg /= (new_balance_entry->balance + balance);

                    new_balance_entry->deposit_date = time_point_sec(avg.to_integer());
                }

                new_balance_entry->last_update = eval_state._current_state->now();
                new_balance_entry->balance += (balance - fee);
                new_balance_entry->restricted_owner = new_restricted_owner;

                eval_state.add_balance(Asset(fee, 0));

                // update delegate vote on deposited account..
                if (new_balance_entry->condition.slate_id)
                    eval_state.adjust_vote(new_balance_entry->condition.slate_id, (balance - fee));

                ilog("I'm storing a balance entry whose last update is: ${secs}", ("secs", new_balance_entry->last_update));
                eval_state._current_state->store_balance_entry(*new_balance_entry);

            } FC_CAPTURE_AND_RETHROW((*this))
        }

        BalanceIdType DepositContractOperation::balance_id()const
        {
            return condition.get_address();
        }

        void DepositContractOperation::evaluate(TransactionEvaluationState & eval_state) const
        {
            try {

                if (!eval_state.evaluate_contract_result)
                    FC_CAPTURE_AND_THROW(not_be_result_of_execute, ("DepositContractOperation can only exsit in result"));

                if (this->amount <= 0)
                    FC_CAPTURE_AND_THROW(negative_deposit, (amount));

                switch (WithdrawConditionTypes(this->condition.type))
                {
                case withdraw_signature_type:
                case withdraw_multisig_type:
                case withdraw_escrow_type:
                    break;
                default:
                    FC_CAPTURE_AND_THROW(invalid_withdraw_condition, (*this));
                }

                const BalanceIdType deposit_balance_id = this->balance_id();

                oBalanceEntry cur_entry = eval_state._current_state->get_balance_entry(deposit_balance_id);
                if (!cur_entry.valid())
                {
                    cur_entry = BalanceEntry(this->condition);
                    if (this->condition.type == withdraw_escrow_type)
                        cur_entry->meta_data = variant_object("creating_transaction_id", eval_state.trx.id());
                }

                if (cur_entry->balance == 0)
                {
                    cur_entry->deposit_date = eval_state._current_state->now();
                }
                else
                {
                    fc::uint128 old_sec_since_epoch(cur_entry->deposit_date.sec_since_epoch());
                    fc::uint128 new_sec_since_epoch(eval_state._current_state->now().sec_since_epoch());

                    fc::uint128 avg = (old_sec_since_epoch * cur_entry->balance) + (new_sec_since_epoch * this->amount);
                    avg /= (cur_entry->balance + this->amount);

                    cur_entry->deposit_date = time_point_sec(avg.to_integer());
                }

                cur_entry->balance += this->amount;
                eval_state.sub_balance(deposit_balance_id, Asset(this->amount, cur_entry->condition.asset_id));

                if (cur_entry->condition.asset_id == 0 && cur_entry->condition.slate_id)
                    eval_state.adjust_vote(cur_entry->condition.slate_id, this->amount);

                cur_entry->last_update = eval_state._current_state->now();

                const oAssetEntry asset_rec = eval_state._current_state->get_asset_entry(cur_entry->condition.asset_id);
                FC_ASSERT(asset_rec.valid(), "Invalid asset entry");

                // if( eval_state._current_state->get_head_block_num() >= ALP_V0_6_0_FORK_BLOCK_NUM )
                //{
                //FC_ASSERT( !eval_state._current_state->is_fraudulent_asset( *asset_rec ) );
                //}
                if (asset_rec->is_restricted())
                {
                    for (const auto& owner : cur_entry->owners())
                    {
                        // TODO
                        //FC_ASSERT( eval_state._current_state->get_authorization(asset_rec->id, owner) );
                    }
                }

                eval_state._current_state->store_balance_entry(*cur_entry);
            } FC_CAPTURE_AND_RETHROW((*this))
        }

    }
} // thinkyoung::blockchain
