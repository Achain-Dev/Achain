#pragma once
#include <blockchain/ChainInterface.hpp>
#include <fc/reflect/reflect.hpp>
#include <deque>

namespace thinkyoung {
    namespace blockchain {

		struct SandboxAccountInfo
		{
			AccountIdType                   id = 0;
			std::string						name;
			optional<DelegateStats>         delegate_info;
			std::string                     owner_address;
			fc::time_point_sec              registration_date;
			fc::time_point_sec              last_update;
			PublicKeyType                   owner_key;

		};

        class PendingChainState : public ChainInterface, public std::enable_shared_from_this < PendingChainState >
        {
        public:
            PendingChainState(ChainInterfacePtr prev_state = ChainInterfacePtr());
            PendingChainState&           operator = (const PendingChainState&) = default;

            /**
            * Set on a layer of data sources
            *
            * @param  prev_state  ChainInterfacePtr
            *
            * @return void
            */
            void                           set_prev_state(ChainInterfacePtr prev_state);


            /**
            * Get NTP corrected current time
            *
            * @return fc::time_point_sec
            */
            virtual fc::time_point_sec     now()const override;



            /**
            * Determine whether the transaction existence
            *
            * @param  trx  transaction
            *
            * @return bool
            */
            virtual bool                   is_known_transaction(const Transaction& trx)const override;
            /**
            * Get transaction by transaction id
            *
            * @param  trx_id  TransactionIdType
            * @param  exact  bool
            *
            * @return oTransactionEntry
            */
            virtual oTransactionEntry    get_transaction(const TransactionIdType& trx_id, bool exact = true)const override;
            /**
            * store transaction.
            *
            * @param  id  TransactionIdType
            * @param  rec  TransactionEntry
            *
            * @return void
            */
            virtual void                   store_transaction(const TransactionIdType&, const TransactionEntry&) override;

            /**
            * apply changes from this pending state to the previous state
            *
            * @return void
            */
            virtual void                   apply_changes()const;

            /** in sandbox evaluate the transaction and return the results.
            *
            * @param  trx  SignedTransaction
            * @param  required_fees  ShareType
            *
            * @return TransactionEvaluationStatePtr
            */
            virtual TransactionEvaluationStatePtr   sandbox_evaluate_transaction(const SignedTransaction& trx, const ShareType required_fees = 0);

            /** populate undo state with everything that would be necessary to revert this
             * pending state to the previous state.
             */
            /**
            * populate undo state with everything that would be necessary to revert this
            * pending state to the previous state.
            *
            * @param  undo_state_arg  ChainInterfacePtr
            *
            * @return void
            */
            virtual void                   get_undo_state(const ChainInterfacePtr& undo_state)const;

            template<typename T, typename U>
            void populate_undo_state(const ChainInterfacePtr& undo_state, const ChainInterfacePtr& prev_state,
                const T& store_map, const U& remove_set)const
            {
                using V = typename T::mapped_type;
                for (const auto& key : remove_set)
                {
                    const auto prev_entry = prev_state->lookup<V>(key);
                    if (prev_entry.valid()) undo_state->store(key, *prev_entry);
                }
                for (const auto& item : store_map)
                {
                    const auto& key = item.first;
                    const auto prev_entry = prev_state->lookup<V>(key);
                    if (prev_entry.valid()) undo_state->store(key, *prev_entry);
                    else undo_state->remove<V>(key);
                }
            }

            template<typename T, typename U>
            void apply_entrys(const ChainInterfacePtr& prev_state, const T& store_map, const U& remove_set)const
            {
                using V = typename T::mapped_type;
                for (const auto& key : remove_set) prev_state->remove<V>(key);
                for (const auto& item : store_map) prev_state->store(item.first, item.second);
            }

            /**
            * load the state from a variant
            *
            * @param  v  fc::variant
            *
            * @return void
            */
            virtual void                   from_variant(const variant& v);
            /** */
            /**
            * convert the state to a variant
            *
            * @return fc::variant
            */
            virtual variant                to_variant()const;
            /**
            * Get the latest generation block number
            *
            * @return uint32_t
            */
            virtual uint32_t               get_head_block_num()const override;
            virtual SignedBlockHeader      get_block_header(const BlockIdType&)const;
            virtual fc::time_point_sec     get_head_block_timestamp()const override;

            virtual BlockIdType               get_block_id(uint32_t block_num)const;
            map<PropertyIdType, PropertyEntry>                             _property_id_to_entry;
            set<PropertyIdType>                                              _property_id_remove;

            unordered_map<AccountIdType, AccountEntry>                     _account_id_to_entry;
            unordered_set<AccountIdType>                                     _account_id_remove;
            unordered_map<string, AccountIdType>                             _account_name_to_id;
            unordered_map<Address, AccountIdType>                            _account_address_to_id;

            unordered_map<AssetIdType, AssetEntry>                         _asset_id_to_entry;
            unordered_set<AssetIdType>                                       _asset_id_remove;
            unordered_map<string, AssetIdType>                               _asset_symbol_to_id;

            unordered_map<SlateIdType, SlateEntry>                         _slate_id_to_entry;
            unordered_set<SlateIdType>                                       _slate_id_remove;

            unordered_map<BalanceIdType, BalanceEntry>                     _balance_id_to_entry;
            unordered_set<BalanceIdType>                                     _balance_id_remove;

            unordered_map<TransactionIdType, TransactionEntry>             _transaction_id_to_entry;
            unordered_set<TransactionIdType>                                 _transaction_id_remove;
            unordered_set<DigestType>                                         _transaction_digests;


            map<SlotIndex, SlotEntry>                                       _slot_index_to_entry;
            set<SlotIndex>                                                    _slot_index_remove;
            map<time_point_sec, AccountIdType>                               _slot_timestamp_to_delegate;

            unordered_map<ContractIdType, ContractEntry>                      _contract_id_to_entry;
            unordered_set<ContractIdType>                                     _contract_id_remove;
            unordered_map<ContractName, ContractIdType>                       _contract_name_to_id;
            unordered_map<ContractIdType, ContractStorageEntry>                    _contract_id_to_storage;
			unordered_map<TransactionIdType, ResultTIdEntry>					_request_id_to_result_id;
			unordered_set<TransactionIdType>								  _req_to_res_to_remove;
			unordered_map<TransactionIdType, RequestIdEntry>					_result_id_to_request_id;
			unordered_set<TransactionIdType>								  _res_to_req_to_remove;
			unordered_map<TransactionIdType, ContractinTrxEntry>					  _trx_to_contract_id;
			unordered_set<TransactionIdType>					_trx_to_contract_id_remove;
			unordered_map<ContractIdType, ContractTrxEntry>	_contract_to_trx_id;
			unordered_set<ContractIdType>						_contract_to_trx_id_remove;
            vector<EventOperation> event_vector;
			vector<thinkyoung::blockchain::SandboxAccountInfo>                     _vec_wallet_accounts;

        private:
            // Not serialized
            std::weak_ptr<ChainInterface>                                     _prev_state;

            /**
            * According id lookup property
            *
            * @param  id  property_id_type
            *
            * @return oPropertyEntry
            */
            virtual oPropertyEntry property_lookup_by_id(const PropertyIdType)const override;
            /**
            * Insert property into id maps.
            *
            * @param  id  property_id_type
            * @param  entry  PropertyEntry
            *
            * @return void
            */
            virtual void property_insert_into_id_map(const PropertyIdType, const PropertyEntry&)override;
            /**
            * Erase property from id maps.
            *
            * @param  id  property_id_type
            *
            * @return void
            */
            virtual void property_erase_from_id_map(const PropertyIdType)override;

            /**
            * Lookup account entry by account id from blockchain db.
            *
            * @param  id  AccountIdType
            *
            * @return oAccountEntry
            */
            virtual oAccountEntry account_lookup_by_id(const AccountIdType)const override;
            /**
            * Lookup account entry by account name from blockchain db.
            *
            * @param  name  string
            *
            * @return oAccountEntry
            */
            virtual oAccountEntry account_lookup_by_name(const string&)const override;
            /**
            * Lookup account entry by account address from blockchain db.
            *
            * @param  addr  Address
            *
            * @return oAccountEntry
            */
            virtual oAccountEntry account_lookup_by_address(const Address&)const override;

            /**  Insert pair(account_id, AccountEntry) into _account_id_to_entry
            *
            * @param  id  AccountIdType
            * @param  entry  AccountEntry
            *
            * @return void
            */
            virtual void account_insert_into_id_map(const AccountIdType, const AccountEntry&)override;

            /**  Insert pair(account_name, account_id) into _account_name_to_id
            *
            * @param  name  string
            * @param  id  AccountIdType
            *
            * @return void
            */
            virtual void account_insert_into_name_map(const string&, const AccountIdType)override;

            /**  Insert pair(address, account_id) into _account_address_to_id
            *
            * @param  addr  Address
            * @param  id  AccountIdType
            *
            * @return void
            */
            virtual void account_insert_into_address_map(const Address&, const AccountIdType)override;

            /**  Do Nothing !!
            *
            * @param  VoteDel
            *
            * @return void
            */
            virtual void account_insert_into_vote_set(const VoteDel&)override;

            /**  Erase from _account_id_to_entry by account_id
            *
            * @param  id  AccountIdType
            *
            * @return void
            */
            virtual void account_erase_from_id_map(const AccountIdType)override;

            /**  Erase from _account_name_to_id by account_name
            *
            * @param  name  string
            *
            * @return void
            */
            virtual void account_erase_from_name_map(const string&)override;

            /**  Erase from _account_address_to_id by address
            *
            * @param  addr  Address
            *
            * @return void
            */
            virtual void account_erase_from_address_map(const Address&)override;

            /**  Do Nothing !!
            *
            * @param  VoteDel
            *
            * @return void
            */
            virtual void account_erase_from_vote_set(const VoteDel&)override;

            /**  Query AssetEntry by asset_id. first from _asset_id_to_entry, if not found from db
            *
            * @param  id  AssetIdType
            *
            * @return oAssetEntry
            */
            virtual oAssetEntry asset_lookup_by_id(const AssetIdType)const override;

            /**  Query AssetEntry by symbol. first from _asset_symbol_to_id and _asset_id_to_entry,
            *  if not found from db
            *
            * @param  symbol  string
            *
            * @return oAssetEntry
            */
            virtual oAssetEntry asset_lookup_by_symbol(const string&)const override;

            /**  Insert pair(asset_id, AssetEntry) into _asset_id_to_entry
            *
            * @param  id  AssetIdType
            * @param  entry  AssetEntry
            *
            * @return void
            */
            virtual void asset_insert_into_id_map(const AssetIdType, const AssetEntry&)override;

            /**  Insert pair(symbol, asset_id) into _asset_symbol_to_id
            *
            * @param  symbol  string
            * @param  id  AssetIdType
            *
            * @return void
            */
            virtual void asset_insert_into_symbol_map(const string&, const AssetIdType)override;

            /**  Erase from _asset_id_to_entry by asset_id
            *
            * @param  id  AssetIdType
            *
            * @return void
            */
            virtual void asset_erase_from_id_map(const AssetIdType)override;

            /**  Erase from _asset_symbol_to_id by symbol
            *
            * @param  symbol  string
            *
            * @return void
            */
            virtual void asset_erase_from_symbol_map(const string&)override;

            /**  Query SlateEntry by slate_id. first from _slate_id_to_entry, if not found from db
            *
            * @param  id  SlateIdType
            *
            * @return oSlateEntry
            */
            virtual oSlateEntry slate_lookup_by_id(const SlateIdType)const override;

            /**  Insert pair(slate_id, SlateEntry) into _slate_id_to_entry
            *
            * @param  id  SlateIdType
            * @param  entry  SlateEntry
            *
            * @return void
            */
            virtual void slate_insert_into_id_map(const SlateIdType, const SlateEntry&)override;

            /**  Erase from _slate_id_to_entry by slate_id
            *
            * @param  id  SlateIdType
            *
            * @return void
            */
            virtual void slate_erase_from_id_map(const SlateIdType)override;

            /**  Query BalanceEntry by balance_id. first from _balance_id_to_entry, if not found from db
            *
            * @param  id  BalanceIdType
            *
            * @return oBalanceEntry
            */
            virtual oBalanceEntry balance_lookup_by_id(const BalanceIdType&)const override;

            /**  Insert pair(balance_id, BalanceEntry) into _balance_id_to_entry
            *
            * @param  id  BalanceIdType
            * @param  entry  BalanceEntry
            *
            * @return void
            */
            virtual void balance_insert_into_id_map(const BalanceIdType&, const BalanceEntry&)override;

            /**  Erase from _balance_id_to_entry by balance_id
            *
            * @param  id  BalanceIdType
            *
            * @return void
            */
            virtual void balance_erase_from_id_map(const BalanceIdType&)override;

            /**  Query TransactionEntry by transaction_id. first from _transaction_id_to_entry, if not found from db
            *
            * @param  id  TransactionIdType
            *
            * @return oTransactionEntry
            */
            virtual oTransactionEntry transaction_lookup_by_id(const TransactionIdType&)const override;

            /**  Insert pair(transaction_id, TransactionEntry) into _transaction_id_to_entry
            *
            * @param  id  TransactionIdType
            * @param  entry  TransactionEntry
            *
            * @return void
            */
            virtual void transaction_insert_into_id_map(const TransactionIdType&, const TransactionEntry&)override;

            /**  Insert transaction into _transaction_digests
            *
            * @param  trx  transaction
            *
            * @return void
            */
            virtual void transaction_insert_into_unique_set(const Transaction&)override;

            /**  Erase from _transaction_id_to_entry by transaction_id
            *
            * @param  id  TransactionIdType
            *
            * @return void
            */
            virtual void transaction_erase_from_id_map(const TransactionIdType&)override;

            /**  Erase transaction from _transaction_digests
            *
            * @param  trx  transaction
            *
            * @return void
            */
            virtual void transaction_erase_from_unique_set(const Transaction&)override;

            /**  Query SlotEntry by slot_index. first from _slot_index_to_entry, if not found from db
            *
            * @param  index  SlotIndex
            *
            * @return oSlotEntry
            */
            virtual oSlotEntry slot_lookup_by_index(const SlotIndex)const override;

            /**  Query SlotEntry by timestamp. first from _slot_index_to_entry, if not found from db
            *
            * @param  timestamp  time_point_sec
            *
            * @return oSlotEntry
            */
            virtual oSlotEntry slot_lookup_by_timestamp(const time_point_sec)const override;

            /**  Insert pair(slot_index, SlotEntry) into _slot_index_to_entry
            *
            * @param  index  SlotIndex
            * @param  entry  SlotEntry
            *
            * @return void
            */
            virtual void slot_insert_into_index_map(const SlotIndex, const SlotEntry&)override;

            /**  Insert pair(timestamp, delegate_id) into _slot_timestamp_to_delegate
            *
            * @param  timestamp  time_point_sec
            * @param  delegate_id  AccountIdType
            *
            * @return void
            */
            virtual void slot_insert_into_timestamp_map(const time_point_sec, const AccountIdType)override;

            /**  Erase from _slot_index_to_entry by slot_index
            *
            * @param  index  SlotIndex
            *
            * @return void
            */
            virtual void slot_erase_from_index_map(const SlotIndex)override;

            /**  Erase from _slot_timestamp_to_delegate by timestamp
            *
            * @param  timestamp  time_point_sec
            *
            * @return void
            */
            virtual void slot_erase_from_timestamp_map(const time_point_sec)override;

            /**
            * Lookup contractInfo by contract id from  _contract_id_to_info.
            *
            * @param  id  ContractIdType
            *
            * @return oContractInfo
            */
            virtual  oContractEntry  contract_lookup_by_id(const ContractIdType&)const override;

            /**
            * Lookup ContractIdType by contract name from _contract_name_to_id.
            *
            * @param  name  ContractName
            *
            * @return oContractEntry
            */
            virtual  oContractEntry  contract_lookup_by_name(const ContractName&)const override;

            /**
            * Lookup contractStorage by contract id from _contract_id_to_stroage.
            *
            * @param  id  ContractIdType
            *
            * @return oContractStorage
            */
            virtual oContractStorage contractstorage_lookup_by_id(const ContractIdType&)const override;

            /**  Insert pair(contract_id, contractInfo) into _contract_id_to_info
            *
            * @param  id  ContractIdType
            * @param  info ContractInfo
            *
            * @return void
            */
            virtual void contract_insert_into_id_map(const ContractIdType&, const ContractEntry&) override;

            /**  Insert pair(contract_name, contract_id) into _contract_name_to_id
            *
            * @param  id  ContractName
            * @param  storage ContractIdType
            *
            * @return void
            */
            virtual void contract_insert_into_name_map(const ContractName&, const ContractIdType&) override;

            /**  Insert pair(contract_id, contractStorage) into _contract_id_to_stroage
            *
            * @param  id  ContractIdType
            * @param  storage ContractStorage
            *
            * @return void
            */
            virtual void contractstorage_insert_into_id_map(const ContractIdType&, const ContractStorageEntry&) override;

            /**  Erase from _contract_id_to_info by contract_id
            *
            * @param  id  ContractIdType
            *
            * @return void
            */
            virtual void contract_erase_from_id_map(const ContractIdType&) override;

            /**  Erase from  _contract_name_to_id by contract_name
            *
            * @param  name  ContractName
            *
            * @return void
            */
            virtual void contract_erase_from_name_map(const ContractName&) override;

            /**  Erase from _contract_id_to_stroage by contract_id
            *
            * @param  id  ContractIdType
            *
            * @return void
            */
            virtual void contractstorage_erase_from_id_map(const ContractIdType&) override;

			virtual oResultTIdEntry contract_lookup_resultid_by_reqestid(const TransactionIdType&) const override;
			virtual void contract_store_resultid_by_reqestid(const TransactionIdType& req, const ResultTIdEntry& res) override;
			virtual void contract_erase_resultid_by_reqestid(const TransactionIdType& req) override;

			/** lookup request transaction id from db by result transaction id
			*
			* @param    id  TransactionIdType
			*
			* @return oResultTIdEntry
			*/
			virtual oRequestIdEntry contract_lookup_requestid_by_resultid(const TransactionIdType&) const override;
			/** store request transaction id to db by result trx id
			*
			* @param    req TransactionIdType
			*
			* @param    res ResultTIdEntry
			*
			* @return void
			*/
			virtual void contract_store_requestid_by_resultid(const TransactionIdType& res, const RequestIdEntry& req) override;
			/** erase request transaction id from db by result trx id
			*
			* @param req  TransactionIdType
			*
			* @return void
			*/
			virtual void contract_erase_requestid_by_resultid(const TransactionIdType& result) override;
			virtual oContractinTrxEntry contract_lookup_contractid_by_trxid(const TransactionIdType&)const;
			virtual oContractTrxEntry contract_lookup_trxid_by_contract_id(const ContractIdType&) const;
			virtual void contract_store_contractid_by_trxid(const TransactionIdType&, const ContractinTrxEntry&);
			virtual void contract_store_trxid_by_contractid(const ContractIdType& id, const ContractTrxEntry & res);
			virtual void contract_erase_trxid_by_contract_id(const ContractIdType&);
			virtual void contract_erase_contractid_by_trxid(const TransactionIdType&);
        };
        typedef std::shared_ptr<PendingChainState> PendingChainStatePtr;

    }
} // thinkyoung::blockchain


FC_REFLECT(thinkyoung::blockchain::PendingChainState,
    (_property_id_to_entry)
    (_property_id_remove)
    (_account_id_to_entry)
    (_account_id_remove)
    (_account_name_to_id)
    (_account_address_to_id)
    (_asset_id_to_entry)
    (_asset_id_remove)
    (_asset_symbol_to_id)
    (_slate_id_to_entry)
    (_slate_id_remove)
    (_balance_id_to_entry)
    (_balance_id_remove)
    (_transaction_id_to_entry)
    (_transaction_id_remove)
    (_transaction_digests)
    (_slot_index_to_entry)
    (_slot_index_remove)
    (_slot_timestamp_to_delegate)
    (_contract_id_to_entry)
    (_contract_id_remove)
    (_contract_name_to_id)
    (_contract_id_to_storage)
	(_request_id_to_result_id)
	(_req_to_res_to_remove)
	(_result_id_to_request_id)
	(_res_to_req_to_remove)
	(_vec_wallet_accounts)
    )

	FC_REFLECT(thinkyoung::blockchain::SandboxAccountInfo, (id)(name)(delegate_info)(owner_address)(registration_date)(last_update)(owner_key))