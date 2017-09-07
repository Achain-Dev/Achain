#include <blockchain/Exceptions.hpp>
#include <blockchain/PendingChainState.hpp>
#include <blockchain/Time.hpp>
#include <fc/io/raw_variant.hpp>

namespace thinkyoung {
    namespace blockchain {

        PendingChainState::PendingChainState(ChainInterfacePtr prev_state)
            : _prev_state(prev_state)
        {
        }

        void PendingChainState::set_prev_state(ChainInterfacePtr prev_state)
        {
            _prev_state = prev_state;
        }

        uint32_t PendingChainState::get_head_block_num()const
        {
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return 1;
            return prev_state->get_head_block_num();
        }

        fc::time_point_sec PendingChainState::get_head_block_timestamp()const
        {
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return fc::time_point_sec(0);
            return prev_state->get_head_block_timestamp();
        }

        fc::time_point_sec PendingChainState::now()const
        {
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return get_slot_start_time(blockchain::now());
            return prev_state->now();
        }
        /*  check????
        oprice pending_chain_state::get_active_feed_price( const asset_id_type quote_id, const asset_id_type base_id )const
        {
        const chain_interface_ptr prev_state = _prev_state.lock();
        FC_ASSERT( prev_state );
        return prev_state->get_active_feed_price( quote_id, base_id );
        }*/

        /** Apply changes from this pending state to the previous state */
        void PendingChainState::apply_changes()const
        {
            ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return;

            apply_entrys(prev_state, _property_id_to_entry, _property_id_remove);
            apply_entrys(prev_state, _account_id_to_entry, _account_id_remove);
            apply_entrys(prev_state, _asset_id_to_entry, _asset_id_remove);
            apply_entrys(prev_state, _slate_id_to_entry, _slate_id_remove);
            apply_entrys(prev_state, _balance_id_to_entry, _balance_id_remove);
            apply_entrys(prev_state, _transaction_id_to_entry, _transaction_id_remove);
            apply_entrys(prev_state, _slot_index_to_entry, _slot_index_remove);
            //contract related
            apply_entrys(prev_state, _contract_id_to_entry, _contract_id_remove);
            apply_entrys(prev_state, _contract_id_to_storage, _contract_id_remove);
			apply_entrys(prev_state, _request_id_to_result_id, _req_to_res_to_remove);
			apply_entrys(prev_state, _result_id_to_request_id, _res_to_req_to_remove);
			apply_entrys(prev_state, _trx_to_contract_id, _trx_to_contract_id_remove);
			apply_entrys(prev_state, _contract_to_trx_id, _contract_to_trx_id_remove);
            /** do this last because it could have side effects on other entrys while
             * we manage the short index
             */
            //apply_entrys( prev_state, _feed_index_to_entry, _feed_index_remove );
        }

        TransactionEvaluationStatePtr   PendingChainState::sandbox_evaluate_transaction(const SignedTransaction& trx, const ShareType required_fees)
        {
            try
            {
                //get and set related states
                PendingChainStatePtr pending_state = std::make_shared<PendingChainState>(shared_from_this());
                TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pending_state.get());

				if (trx_eval_state->origin_trx_basic_verify(trx) == false)
					FC_CAPTURE_AND_THROW(illegal_transaction, (trx));

                //for origin trx
                //set skipexec to false, so all nodes can execute contract
                trx_eval_state->skipexec = false;
				bool no_check_required_fee = false;
				
				SignedTransaction result_trx = trx_eval_state->sandbox_evaluate(trx, no_check_required_fee);

                ShareType fees = trx_eval_state->get_fees() + trx_eval_state->alt_fees_paid.amount;
				if (!no_check_required_fee && (fees < required_fees))
                {
                    ilog("Transaction ${id} needed relay fee ${required_fees} but only had ${fees}", ("id", trx.id())("required_fees", required_fees)("fees", fees));
                    FC_CAPTURE_AND_THROW(insufficient_relay_fee, (fees)(required_fees));
                }

                //if has result_trx, then apply result trx
                if (result_trx.operations.size() != 0)
                {

                    //for result trx
                    pending_state = std::make_shared<PendingChainState>(shared_from_this());
                    trx_eval_state = std::make_shared<TransactionEvaluationState>(pending_state.get());
                    trx_eval_state->skipexec = false;
					no_check_required_fee = false;
					trx_eval_state->sandbox_evaluate(result_trx, no_check_required_fee);

					fees = trx_eval_state->get_fees() + trx_eval_state->alt_fees_paid.amount;
					if (!no_check_required_fee && (fees < required_fees))
					{
						ilog("Transaction ${id} needed relay fee ${required_fees} but only had ${fees}", ("id", trx.id())("required_fees", required_fees)("fees", fees));
						FC_CAPTURE_AND_THROW(insufficient_relay_fee, (fees)(required_fees));
					}

                }


                //apply changes
                pending_state->apply_changes();

                return trx_eval_state;
            } FC_CAPTURE_AND_RETHROW((trx))

        }

        oTransactionEntry PendingChainState::get_transaction(const TransactionIdType& trx_id, bool exact)const
        {
            return lookup<TransactionEntry>(trx_id);
        }

        bool PendingChainState::is_known_transaction(const Transaction& trx)const
        {
            try {
                if (_transaction_digests.count(trx.digest(get_chain_id())) > 0) return true;
                ChainInterfacePtr prev_state = _prev_state.lock();
                if (prev_state) return prev_state->is_known_transaction(trx);
                return false;
            } FC_CAPTURE_AND_RETHROW((trx))
        }

        void PendingChainState::store_transaction(const TransactionIdType& id, const TransactionEntry& rec)
        {
            store(id, rec);
        }

        void PendingChainState::get_undo_state(const ChainInterfacePtr& undo_state_arg)const
        {
            auto undo_state = std::dynamic_pointer_cast<PendingChainState>(undo_state_arg);
            ChainInterfacePtr prev_state = _prev_state.lock();
            FC_ASSERT(prev_state, "Get preview state failed!");

            populate_undo_state(undo_state, prev_state, _property_id_to_entry, _property_id_remove);
            populate_undo_state(undo_state, prev_state, _account_id_to_entry, _account_id_remove);
            populate_undo_state(undo_state, prev_state, _asset_id_to_entry, _asset_id_remove);
            populate_undo_state(undo_state, prev_state, _slate_id_to_entry, _slate_id_remove);
            populate_undo_state(undo_state, prev_state, _balance_id_to_entry, _balance_id_remove);
            populate_undo_state(undo_state, prev_state, _transaction_id_to_entry, _transaction_id_remove);
            populate_undo_state(undo_state, prev_state, _slot_index_to_entry, _slot_index_remove);
            //contract related
            populate_undo_state(undo_state, prev_state, _contract_id_to_entry, _contract_id_remove);
            populate_undo_state(undo_state, prev_state, _contract_id_to_storage, _contract_id_remove);
			populate_undo_state(undo_state, prev_state, _request_id_to_result_id, _req_to_res_to_remove);
			populate_undo_state(undo_state, prev_state, _result_id_to_request_id, _res_to_req_to_remove);
			populate_undo_state(undo_state, prev_state, _trx_to_contract_id,_trx_to_contract_id_remove);
			populate_undo_state(undo_state, prev_state, _contract_to_trx_id, _contract_to_trx_id_remove);
        }

        /** load the state from a variant */
        void PendingChainState::from_variant(const fc::variant& v)
        {
            fc::from_variant(v, *this);
        }

        /** convert the state to a variant */
        fc::variant PendingChainState::to_variant()const
        {
            fc::variant v;
            fc::to_variant(*this, v);
            return v;
        }


        oPropertyEntry PendingChainState::property_lookup_by_id(const PropertyIdType id)const
        {
            const auto iter = _property_id_to_entry.find(id);
            if (iter != _property_id_to_entry.end()) return iter->second;
            if (_property_id_remove.count(id) > 0) return oPropertyEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oPropertyEntry();
            return prev_state->lookup<PropertyEntry>(id);
        }

        void PendingChainState::property_insert_into_id_map(const PropertyIdType id, const PropertyEntry& entry)
        {
            _property_id_remove.erase(id);
            _property_id_to_entry[id] = entry;
        }

        void PendingChainState::property_erase_from_id_map(const PropertyIdType id)
        {
            _property_id_to_entry.erase(id);
            _property_id_remove.insert(id);
        }

        oAccountEntry PendingChainState::account_lookup_by_id(const AccountIdType id)const
        {
            const auto iter = _account_id_to_entry.find(id);
            if (iter != _account_id_to_entry.end()) return iter->second;
            if (_account_id_remove.count(id) > 0) return oAccountEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oAccountEntry();
            return prev_state->lookup<AccountEntry>(id);
        }

        oAccountEntry PendingChainState::account_lookup_by_name(const string& name)const
        {
            const auto iter = _account_name_to_id.find(name);
            if (iter != _account_name_to_id.end()) return _account_id_to_entry.at(iter->second);
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oAccountEntry();
            const oAccountEntry entry = prev_state->lookup<AccountEntry>(name);
            if (entry.valid() && _account_id_remove.count(entry->id) == 0) return *entry;
            return oAccountEntry();
        }

        oAccountEntry PendingChainState::account_lookup_by_address(const Address& addr)const
        {
            const auto iter = _account_address_to_id.find(addr);
            if (iter != _account_address_to_id.end()) return _account_id_to_entry.at(iter->second);
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oAccountEntry();
            const oAccountEntry entry = prev_state->lookup<AccountEntry>(addr);
            if (entry.valid() && _account_id_remove.count(entry->id) == 0) return *entry;
            return oAccountEntry();
        }

        void PendingChainState::account_insert_into_id_map(const AccountIdType id, const AccountEntry& entry)
        {
            _account_id_remove.erase(id);
            _account_id_to_entry[id] = entry;
        }

        void PendingChainState::account_insert_into_name_map(const string& name, const AccountIdType id)
        {
            _account_name_to_id[name] = id;
        }

        void PendingChainState::account_insert_into_address_map(const Address& addr, const AccountIdType id)
        {
            _account_address_to_id[addr] = id;
        }

        void PendingChainState::account_insert_into_vote_set(const VoteDel&)
        {
        }

        void PendingChainState::account_erase_from_id_map(const AccountIdType id)
        {
            _account_id_to_entry.erase(id);
            _account_id_remove.insert(id);
        }

        void PendingChainState::account_erase_from_name_map(const string& name)
        {
            _account_name_to_id.erase(name);
        }

        void PendingChainState::account_erase_from_address_map(const Address& addr)
        {
            _account_address_to_id.erase(addr);
        }

        void PendingChainState::account_erase_from_vote_set(const VoteDel&)
        {
        }

        oAssetEntry PendingChainState::asset_lookup_by_id(const AssetIdType id)const
        {
            const auto iter = _asset_id_to_entry.find(id);
            if (iter != _asset_id_to_entry.end()) return iter->second;
            if (_asset_id_remove.count(id) > 0) return oAssetEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oAssetEntry();
            return prev_state->lookup<AssetEntry>(id);
        }

        oAssetEntry PendingChainState::asset_lookup_by_symbol(const string& symbol)const
        {
            const auto iter = _asset_symbol_to_id.find(symbol);
            if (iter != _asset_symbol_to_id.end()) return _asset_id_to_entry.at(iter->second);
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oAssetEntry();
            const oAssetEntry entry = prev_state->lookup<AssetEntry>(symbol);
            if (entry.valid() && _asset_id_remove.count(entry->id) == 0) return *entry;
            return oAssetEntry();
        }

        void PendingChainState::asset_insert_into_id_map(const AssetIdType id, const AssetEntry& entry)
        {
            _asset_id_remove.erase(id);
            _asset_id_to_entry[id] = entry;
        }

        void PendingChainState::asset_insert_into_symbol_map(const string& symbol, const AssetIdType id)
        {
            _asset_symbol_to_id[symbol] = id;
        }

        void PendingChainState::asset_erase_from_id_map(const AssetIdType id)
        {
            _asset_id_to_entry.erase(id);
            _asset_id_remove.insert(id);
        }

        void PendingChainState::asset_erase_from_symbol_map(const string& symbol)
        {
            _asset_symbol_to_id.erase(symbol);
        }

        oSlateEntry PendingChainState::slate_lookup_by_id(const SlateIdType id)const
        {
            const auto iter = _slate_id_to_entry.find(id);
            if (iter != _slate_id_to_entry.end()) return iter->second;
            if (_slate_id_remove.count(id) > 0) return oSlateEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oSlateEntry();
            return prev_state->lookup<SlateEntry>(id);
        }

        void PendingChainState::slate_insert_into_id_map(const SlateIdType id, const SlateEntry& entry)
        {
            _slate_id_remove.erase(id);
            _slate_id_to_entry[id] = entry;
        }

        void PendingChainState::slate_erase_from_id_map(const SlateIdType id)
        {
            _slate_id_to_entry.erase(id);
            _slate_id_remove.insert(id);
        }

        oBalanceEntry PendingChainState::balance_lookup_by_id(const BalanceIdType& id)const
        {
            const auto iter = _balance_id_to_entry.find(id);
            if (iter != _balance_id_to_entry.end()) return iter->second;
            if (_balance_id_remove.count(id) > 0) return oBalanceEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oBalanceEntry();
            return prev_state->lookup<BalanceEntry>(id);
        }

        void PendingChainState::balance_insert_into_id_map(const BalanceIdType& id, const BalanceEntry& entry)
        {
            _balance_id_remove.erase(id);
            _balance_id_to_entry[id] = entry;
        }

        void PendingChainState::balance_erase_from_id_map(const BalanceIdType& id)
        {
            _balance_id_to_entry.erase(id);
            _balance_id_remove.insert(id);
        }

        oTransactionEntry PendingChainState::transaction_lookup_by_id(const TransactionIdType& id)const
        {
            const auto iter = _transaction_id_to_entry.find(id);
            if (iter != _transaction_id_to_entry.end()) return iter->second;
            if (_transaction_id_remove.count(id) > 0) return oTransactionEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oTransactionEntry();
            return prev_state->lookup<TransactionEntry>(id);
        }

        void PendingChainState::transaction_insert_into_id_map(const TransactionIdType& id, const TransactionEntry& entry)
        {
            _transaction_id_remove.erase(id);
            _transaction_id_to_entry[id] = entry;
        }

        void PendingChainState::transaction_insert_into_unique_set(const Transaction& trx)
        {
            _transaction_digests.insert(trx.digest(get_chain_id()));
        }

        void PendingChainState::transaction_erase_from_id_map(const TransactionIdType& id)
        {
            _transaction_id_to_entry.erase(id);
            _transaction_id_remove.insert(id);
        }

        void PendingChainState::transaction_erase_from_unique_set(const Transaction& trx)
        {
            _transaction_digests.erase(trx.digest(get_chain_id()));
        }


        oSlotEntry PendingChainState::slot_lookup_by_index(const SlotIndex index)const
        {
            const auto iter = _slot_index_to_entry.find(index);
            if (iter != _slot_index_to_entry.end()) return iter->second;
            if (_slot_index_remove.count(index) > 0) return oSlotEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oSlotEntry();
            return prev_state->lookup<SlotEntry>(index);
        }

        oSlotEntry PendingChainState::slot_lookup_by_timestamp(const time_point_sec timestamp)const
        {
            const auto iter = _slot_timestamp_to_delegate.find(timestamp);
            if (iter != _slot_timestamp_to_delegate.end()) return _slot_index_to_entry.at(SlotIndex(iter->second, timestamp));
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oSlotEntry();
            const oSlotEntry entry = prev_state->lookup<SlotEntry>(timestamp);
            if (entry.valid() && _slot_index_remove.count(entry->index) == 0) return *entry;
            return oSlotEntry();
        }

        void PendingChainState::slot_insert_into_index_map(const SlotIndex index, const SlotEntry& entry)
        {
            _slot_index_remove.erase(index);
            _slot_index_to_entry[index] = entry;
        }

        void PendingChainState::slot_insert_into_timestamp_map(const time_point_sec timestamp, const AccountIdType delegate_id)
        {
            _slot_timestamp_to_delegate[timestamp] = delegate_id;
        }

        void PendingChainState::slot_erase_from_index_map(const SlotIndex index)
        {
            _slot_index_to_entry.erase(index);
            _slot_index_remove.insert(index);
        }

        void PendingChainState::slot_erase_from_timestamp_map(const time_point_sec timestamp)
        {
            _slot_timestamp_to_delegate.erase(timestamp);
        }

        oContractEntry  PendingChainState::contract_lookup_by_id(const ContractIdType& id)const
        {
            const auto iter = _contract_id_to_entry.find(id);
            if (iter != _contract_id_to_entry.end()) return  iter->second;
            if (_contract_id_remove.count(id) > 0) return oContractEntry();
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oContractEntry();
            return prev_state->lookup<ContractEntry>(id);
        }

        oContractEntry  PendingChainState::contract_lookup_by_name(const ContractName& name)const
        {
            const auto iter = _contract_name_to_id.find(name);
            if (iter != _contract_name_to_id.end()) return contract_lookup_by_id(iter->second);
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oContractEntry();
            const oContractEntry entry = prev_state->lookup<ContractEntry>(name);
            if (entry.valid() && _contract_id_remove.count(entry->id) == 0) return *entry;
            return oContractEntry();
        }

        oContractStorage PendingChainState::contractstorage_lookup_by_id(const ContractIdType& id)const
        {
            const auto iter = _contract_id_to_storage.find(id);
            if (iter != _contract_id_to_storage.end()) return iter->second;
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (!prev_state) return oContractStorage();
            const oContractStorage entry = prev_state->lookup<ContractStorageEntry>(id);
            if (entry.valid() && _contract_id_remove.count(id) == 0) return entry;
            return oContractStorage();
        }

        void PendingChainState::contract_insert_into_id_map(const ContractIdType& id, const ContractEntry& entry)
        {
            _contract_id_remove.erase(id);
            _contract_id_to_entry[id] = entry;
        }

        void PendingChainState::contract_insert_into_name_map(const ContractName& name, const ContractIdType& id)
        {
            //_contract_id_remove.erase(id);
            _contract_name_to_id[name] = id;
        }

        void PendingChainState::contractstorage_insert_into_id_map(const ContractIdType& id, const ContractStorageEntry& storage)
        {
            //_contract_id_remove.erase(id);
            _contract_id_to_storage[id] = storage;
        }

        void PendingChainState::contract_erase_from_id_map(const ContractIdType& id)
        {
            _contract_id_to_entry.erase(id);
            _contract_id_remove.insert(id);
        }

        void PendingChainState::contract_erase_from_name_map(const ContractName& name)
        {
            // ContractIdType id = _contract_name_to_id[name];
            _contract_name_to_id.erase(name);
            //_contract_id_remove.insert(id);
        }
		oResultTIdEntry PendingChainState::contract_lookup_resultid_by_reqestid(const TransactionIdType& req) const
		{

			auto it = _request_id_to_result_id.find(req);
			if (it != _request_id_to_result_id.end())
				return  it->second;
			if (_req_to_res_to_remove.count(req) > 0)
				return oResultTIdEntry();
			const ChainInterfacePtr prev_state = _prev_state.lock();
			if (!prev_state)
				return oResultTIdEntry();
			return prev_state->lookup<ResultTIdEntry>(req);
			
		}
		void PendingChainState::contract_store_resultid_by_reqestid(const TransactionIdType& req, const ResultTIdEntry& res)
		{
			_req_to_res_to_remove.erase(req);
			_request_id_to_result_id[req] = res;
		}
		void PendingChainState::contract_erase_resultid_by_reqestid(const TransactionIdType& req)
		{
			_request_id_to_result_id.erase(req);
			_req_to_res_to_remove.insert(req);
		}
		oRequestIdEntry PendingChainState::contract_lookup_requestid_by_resultid(const TransactionIdType &res) const
		{
			auto it = _result_id_to_request_id.find(res);
			if (it != _result_id_to_request_id.end())
				return it->second;
			if (_res_to_req_to_remove.count(res) > 0)
				return oRequestIdEntry();
			const ChainInterfacePtr prev_state = _prev_state.lock();
			if (!prev_state)
				return oRequestIdEntry();
			return prev_state->lookup<RequestIdEntry>(res);

		}
		void PendingChainState::contract_store_requestid_by_resultid(const TransactionIdType & res, const RequestIdEntry & req)
		{
			_res_to_req_to_remove.erase(res);
			_result_id_to_request_id[res] = req;
		}
		void PendingChainState::contract_erase_requestid_by_resultid(const TransactionIdType & result)
		{
			_result_id_to_request_id.erase(result);
			_res_to_req_to_remove.insert(result);
		}
		oContractinTrxEntry PendingChainState::contract_lookup_contractid_by_trxid(const TransactionIdType &id) const
		{
			auto it = _trx_to_contract_id.find(id);
			if (it != _trx_to_contract_id.end())
				return it->second;
			if (_trx_to_contract_id_remove.count(id) > 0)
				return oContractinTrxEntry();
			const ChainInterfacePtr prev_state = _prev_state.lock();
			if (!prev_state)
				return oContractinTrxEntry();
			return prev_state->lookup<ContractinTrxEntry>(id);
		}
		oContractTrxEntry PendingChainState::contract_lookup_trxid_by_contract_id(const ContractIdType &id) const
		{
			auto it = _contract_to_trx_id.find(id);
			if (it != _contract_to_trx_id.end())
				return it->second;
			if (_contract_to_trx_id_remove.count(id) > 0)
				return oContractTrxEntry();
			const ChainInterfacePtr prev_state = _prev_state.lock();
			if (!prev_state)
				return oContractTrxEntry();
			return prev_state->lookup<ContractTrxEntry>(id);
			
		}
		void PendingChainState::contract_store_contractid_by_trxid(const TransactionIdType & tid, const ContractinTrxEntry & cid)
		{
			_trx_to_contract_id[tid] = cid;
			_trx_to_contract_id_remove.erase(tid);
		}
		void PendingChainState::contract_store_trxid_by_contractid(const ContractIdType & cid, const ContractTrxEntry & tid)
		{
			_contract_to_trx_id[cid] = tid;
			_contract_to_trx_id_remove.erase(cid);
		}
		void PendingChainState::contract_erase_trxid_by_contract_id(const ContractIdType &id)
		{
			_contract_to_trx_id.erase(id);
			_contract_to_trx_id_remove.insert(id);
		}
		void PendingChainState::contract_erase_contractid_by_trxid(const TransactionIdType &id)
		{
			this->_trx_to_contract_id.erase(id);
			_trx_to_contract_id_remove.insert(id);
		}
        void PendingChainState::contractstorage_erase_from_id_map(const ContractIdType& id)
        {
            _contract_id_to_storage.erase(id);
            //_contract_id_remove.insert(id);
        }

        thinkyoung::blockchain::BlockIdType PendingChainState::get_block_id(uint32_t block_num) const
        {
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (prev_state) return prev_state->get_block_id(block_num);
            return BlockIdType();
        }

        thinkyoung::blockchain::SignedBlockHeader PendingChainState::get_block_header(const BlockIdType& id) const
        {
            const ChainInterfacePtr prev_state = _prev_state.lock();
            if (prev_state) return prev_state->get_block_header(id);
            return SignedBlockHeader();
        }

    }
} // thinkyoung::blockchain
