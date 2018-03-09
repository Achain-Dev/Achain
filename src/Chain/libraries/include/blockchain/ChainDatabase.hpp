#pragma once

#include <blockchain/ChainInterface.hpp>
#include <blockchain/DelegateConfig.hpp>
#include <blockchain/PendingChainState.hpp>
#include <wallet/WalletEntrys.hpp>

namespace thinkyoung {
    namespace blockchain {
    
        namespace detail {
            class ChainDatabaseImpl;
        }
        
        class TransactionEvaluationState;
        typedef std::shared_ptr<TransactionEvaluationState> TransactionEvaluationStatePtr;
        
        struct BlockSummary {
            FullBlock                                    block_data;
            PendingChainStatePtr                       applied_changes;
        };
        
        struct BlockForkData {
            BlockForkData() :is_linked(false), is_included(false), is_known(false) {}
            
            bool invalid()const {
                if (!!is_valid) return !*is_valid;
                
                return false;
            }
            bool valid()const {
                if (!!is_valid) return *is_valid;
                
                return false;
            }
            bool can_link()const {
                return is_linked && !invalid();
            }
            
            std::unordered_set<BlockIdType> next_blocks; ///< IDs of all blocks that come after
            bool                              is_linked;   ///< is linked to genesis block
            
            /** if at any time this block was determined to be valid or invalid then this
             * flag will be set.
             */
            fc::optional<bool>           is_valid;
            fc::optional<fc::exception>  invalid_reason;
            bool                         is_included; ///< is included in the current chain database
            bool                         is_known; ///< do we know the content of this block (false if placeholder)
        };
        
        struct ForkEntry {
            ForkEntry()
                : signing_delegate(0),
                  transaction_count(0),
                  latency(0),
                  size(0),
                  is_current_fork(false)
            {}
            
            BlockIdType block_id;
            AccountIdType signing_delegate;
            uint32_t transaction_count;
            fc::microseconds latency;
            uint32_t size;
            fc::time_point_sec timestamp;
            fc::optional<bool> is_valid;
            fc::optional<fc::exception> invalid_reason;
            bool is_current_fork;
        };
        typedef fc::optional<ForkEntry> oForkEntry;
        
        class ChainObserver {
          public:
            virtual ~ChainObserver() {}
            
            /**
             * This method is called anytime the blockchain state changes including
             * undo operations.
             */
            virtual void state_changed(const PendingChainStatePtr& state) = 0;
            
            /**
             *  This method is called anytime a block is applied to the chain.
             */
            virtual void block_applied(const BlockSummary& summary) = 0;
        };
        
        class ChainDatabase : public ChainInterface, public std::enable_shared_from_this < ChainDatabase > {
          public:
            ChainDatabase();
            virtual ~ChainDatabase()override;
            
            /**  Open leveldb file
            *
            * @param  data_dir  fc::path
            * @param  genesis_file  fc::optional<fc::path>
            * @param  statistics_enabled  bool
            * @param  std::function<void
            *
            * @return void
            */
            void open(const fc::path& data_dir, const fc::optional<fc::path>& genesis_file, const bool statistics_enabled,
                      const std::function<void(float)> replay_status_callback = std::function<void(float)>());
                      
            /**  Close leveldb file
            *
            *
            * @return void
            */
            void close();
            
            /**  Add to the observers set
            *
            * @param  observer  ChainObserver*
            *
            * @return void
            */
            void add_observer(ChainObserver* observer);
            
            /**  Remove from the observers set
            *
            * @param  observer  ChainObserver*
            *
            * @return void
            */
            void remove_observer(ChainObserver* observer);
            
            /**  Set relay fee
            *
            * @param  shares  ShareType
            *
            * @return void
            */
            
            void set_relay_fee(ShareType shares);
            /**  Get relay fee
            *
            *
            * @return ShareType
            */
            ShareType get_relay_fee();
            
            /**
            * Get the reward of generating one block
            *
            * @return ShareType
            */
            ShareType get_product_reword_fee();
            
            /**  Calculates the percentage of blocks produced in the last 10 rounds as an average
            *    measure of the delegate participation rate.
            *
            * @return double  between 0 to 100
            */
            double get_average_delegate_participation()const;
            
            /**  The state of the blockchain after applying all pending transactions.
            *
            *
            * @return PendingChainStatePtr
            */
            PendingChainStatePtr                    get_pending_state()const;
            
            
            /**  get property of _is_in_sandbox
            *
            * @return bool
            */
            bool    get_is_in_sandbox()const;
            
            /**  set property of _is_in_sandbox
            *
            * @param  bool sandbox
            * @return void
            */
            void    set_is_in_sandbox(bool sandbox);
            
            /**  get the state of sandbox after applying transactions.
            *
            *
            * @return PendingChainStatePtr
            */
            PendingChainStatePtr    get_sandbox_pending_state();
            
            /**  set the state of sandbox after applying transactions.
            *
            *
            * @return void
            */
            void                set_sandbox_pending_state(PendingChainStatePtr state_ptr);
            
            /**  clear the state of sandbox.
            *
            *
            * @return void
            */
            void                    clear_sandbox_pending_state();
            
            
            bool                store_balance_entries_for_sandbox();
            
            bool                store_account_entries_for_sandbox(const vector<thinkyoung::wallet::WalletAccountEntry>& vec_account_entry);
            /**
            * Save the transaction to the broadcasting area
            *
            * @param trx the SignedTransaction to save
            * @param override_limits - stores the transaction even if the pending queue is full,
            *                          if false then it will require exponential fee increases
            *                          as the queue fills.
            *
            * @return TransactionEvaluationStatePtr  (shared_ptr)
            */
            TransactionEvaluationStatePtr           store_pending_transaction(const SignedTransaction& trx,
                    bool override_limits = true, bool contract_vm_exec = false, bool cache=false);
            /**
            * Return a list of transactions that are not yet in a block.
            *
            * @return  vector<TransactionEvaluationStatePtr>
            */
            vector<TransactionEvaluationStatePtr>   get_pending_transactions()const;
            
            std::vector<TransactionEvaluationStatePtr> get_rebroadcast_pending_transactions()const;
            
            std::vector<TransactionEvaluationStatePtr> get_generate_pending_transactions()const;
            
            /**  Is transaction in the _unique_transactions set
            *
            * @param  trx  transaction
            *
            * @return bool
            */
            virtual bool                               is_known_transaction(const Transaction& trx)const override;
            
            /**  Produce a block for the given timeslot, the block is not signed because that is the
            *  role of the wallet.
            *
            * @param  block_timestamp  time_point_sec
            * @param  config  DelegateConfig
            *
            * @return FullBlock
            */
            FullBlock                  generate_block(const time_point_sec block_timestamp,
                    const DelegateConfig& config = DelegateConfig());
                    
            /**  Get BlockForkData from _fork_db
            *
            * @param  block_id  BlockIdType
            *
            * @return optional<BlockForkData>
            */
            optional<BlockForkData>   get_block_fork_data(const BlockIdType&)const;
            
            /**  BlockForkData is not a placeholder return true, otherwise return false
            *
            * @param  block_id  BlockIdType
            *
            * @return bool
            */
            bool                        is_known_block(const BlockIdType& id)const;
            
            /**  BlockForkData is extended into chain return true, otherwise return false
            *
            * @param  block_id  BlockIdType
            *
            * @return bool
            */
            bool                        is_included_block(const BlockIdType& id)const;
            
            /**  Get delegate entry from signee (public key)
            *
            * @param  block_signee  PublicKeyType
            *
            * @return AccountEntry
            */
            AccountEntry              get_delegate_entry_for_signee(const PublicKeyType& block_signee)const;
            
            /**  Get entry of the account which signed the specified block
            *
            *
            * @param block_id id of the specified block
            * @return AccountEntry
            */
            AccountEntry              get_block_signee(const BlockIdType& block_id)const;
            
            /** Get entry of the account which signed the specified block
            *
            *
            * @param block_num num of the specified block
            * @return AccountEntry
            */
            AccountEntry              get_block_signee(uint32_t block_num)const;
            
            /**  Dump db file to json file
            *
            * @param  path  fc::path
            *
            * @return void
            */
            void  dump_state(const fc::path& path, const fc::string& ldbname)const;
            
            /**  Calculate by timestamp and get the delegate of the slot
            *
            * @param  timestamp  time_point_sec
            * @param  ordered_delegates  std::vector<AccountIdType>
            *
            * @return AccountEntry
            */
            AccountEntry              get_slot_signee(const time_point_sec timestamp,
                    const std::vector<AccountIdType>& ordered_delegates)const;
                    
            /**  Get next producible block timestamp
            *
            * @param  delegate_ids  vector<AccountIdType>
            *
            * @return optional<time_point_sec>
            */
            optional<time_point_sec>    get_next_producible_block_timestamp(const vector<AccountIdType>& delegate_ids)const;
            
            /**
            * Fetches all transactions that involve the provided address.
            *
            * @param addr address to scan for (string, required)
            *
            * @return variant_object
            */
            vector<TransactionEntry>  fetch_address_transactions(const Address& addr);
            
            /**  Fetch ALP input balance in blocks that block_num of them are lower of the given block_num
            *  or equal to
            *
            * @param  block_num  uint32_t
            *
            * @return vector<AlpTrxidBalance>
            */
            vector<AlpTrxidBalance> fetch_alp_input_balance(const uint32_t &  block_num);
            
            /**  Insert the ALP transaction balance to db _alp_input_balance_entry
            *
            * @param  alp_account  string
            * @param  alp_balance_entry  AlpTrxidBalance
            *
            * @return void
            */
            void transaction_insert_to_alp_balance(const string & alp_account, const AlpTrxidBalance & alp_balance_entry);
            
            /** Erase the ALP transaction balance from db _alp_input_balance_entry
            *
            * @param  alp_account  string
            * @param  alp_balance_entry  AlpTrxidBalance
            *
            * @return void
            */
            void transaction_erase_from_alp_balance(const string & alp_account, const AlpTrxidBalance & alp_balance_entry);
            vector<AlpTrxidBalance> fetch_alp_full_entry(const uint32_t& block_num, const uint32_t& last_scan_block_num);
            //Must handle account before you use this function
            void transaction_insert_to_alp_full_entry(const string& alp_accout, const AlpTrxidBalance& alp_balance_entry);
            //Must handle account before you use this function
            void transaction_erase_from_alp_full_entry(const string& alp_accout, const AlpTrxidBalance& alp_balance_entry);
            
            
            /**  Find block num by time (use the method of bisection)
            *
            * @param  time  fc::time_point_sec
            *
            * @return uint32_t
            */
            uint32_t                    find_block_num(fc::time_point_sec &time)const;
            
            /**  Get block_num by block_id
            *
            * @param  block_id  BlockIdType
            *
            * @return uint32_t
            */
            uint32_t                    get_block_num(const BlockIdType&)const;
            
            /**  Get block header by block_id
            *
            * @param  block_id  BlockIdType
            *
            * @return SignedBlockHeader
            */
            virtual SignedBlockHeader         get_block_header(const BlockIdType&)const;
            
            /**  Get block header by block_num
            *
            * @param  block_num  uint32_t
            *
            * @return SignedBlockHeader
            */
            SignedBlockHeader         get_block_header(uint32_t block_num)const;
            
            /**  Get DigestBlock by block_id, copy DigestBlock from FullBlock
            *
            * @param  block_id  BlockIdType
            *
            * @return DigestBlock
            */
            DigestBlock                get_block_digest(const BlockIdType&)const;
            
            /**  Get DigestBlock by block_num, copy DigestBlock from FullBlock
            *
            * @param  block_num  uint32_t
            *
            * @return DigestBlock
            */
            DigestBlock                get_block_digest(uint32_t block_num)const;
            
            /**  Get FullBlock by block_id
            *
            * @param  block_id  BlockIdType
            *
            * @return FullBlock
            */
            FullBlock                  get_block(const BlockIdType&)const;
            
            /**  Get FullBlock by block_num
            *
            * @param  block_num  uint32_t
            *
            * @return FullBlock
            */
            FullBlock                  get_block(uint32_t block_num)const;
            
            /**
            * Retrieves the detailed transaction information for a block.
            *
            * @param id the number or id of the block to get transactions from
            *
            * @return vector<TransactionEntry>
            */
            
            vector<TransactionEntry>  get_transactions_for_block(const BlockIdType&)const;
            /**  Get the SignedBlockHeader of head block
            *
            *
            * @return SignedBlockHeader
            */
            SignedBlockHeader         get_head_block()const;
            
            /**  Get the block_num of head block
            *
            *
            * @return uint32_t
            */
            virtual uint32_t            get_head_block_num()const override;
            
            virtual fc::time_point_sec  get_head_block_timestamp()const override;
            
            /**  Get the block_id of head block
            *
            *
            * @return BlockIdType
            */
            BlockIdType               get_head_block_id()const;
            
            /**  Get get_events by block_index and trx_id
            *
            * @param  block_index  uint32_t
            * @param  trx_id  thinkyoung::blockchain::TransactionIdType
            *
            * @return vector<EventOperation>
            */
            vector<EventOperation>    get_events(uint32_t block_index, const thinkyoung::blockchain::TransactionIdType& trx_id);
            
            /**  Get block_id by block_num
            *
            * @param  block_num  uint32_t
            *
            * @return BlockIdType
            */
            BlockIdType               get_block_id(uint32_t block_num)const;
            
            /**  Get block entry by block_id
            *
            * @param  block_id  BlockIdType
            *
            * @return oBlockEntry
            */
            oBlockEntry               get_block_entry(const BlockIdType& block_id)const;
            
            /**  Get block entry by block_num
            *
            * @param  block_num  uint32_t
            *
            * @return oBlockEntry
            */
            oBlockEntry               get_block_entry(uint32_t block_num)const;
            
            /**  Get result entry by result_id
            *
            * @param  result_id  TransactionIdType
            *
            * @return oResultTIdEntry
            */
            oResultTIdEntry get_transaction_from_result(const TransactionIdType& result_id) const;
            
            /**  Get transaction by trx_id, find exactly when exact is true, otherwise find the
            *  lower one
            *
            * @param  trx_id  TransactionIdType
            * @param  exact  bool
            *
            * @return oTransactionEntry
            */
            virtual oTransactionEntry get_transaction(const TransactionIdType& trx_id,
                    bool exact = true)const override;
                    
            /**  Store transaction to db
            *
            * @param  entry_id  TransactionIdType
            * @param  entry_to_store  TransactionEntry
            *
            * @return void
            */
            virtual void                store_transaction(const TransactionIdType&,
                    const TransactionEntry&) override;
                    
            /**
            * Lists balance entries starting at the given balance ID.
            *
            * @param first the first balance id to start at (string, optional, defaults to "")
            * @param limit the maximum number of items to list (uint32_t, optional, defaults to 20)
            *
            * @return balance_entry_map
            */
            unordered_map<BalanceIdType, BalanceEntry>     get_balances(const BalanceIdType& first,
                    uint32_t limit)const;
                    
            /**
            * Retrieves balance entries according address
            *
            * @param addr  address
            *
            * @return unordered_map<BalanceIdType, BalanceEntry>
            */
            unordered_map<BalanceIdType, BalanceEntry>     get_balances_for_address(const Address& addr)const;
            
            /**
            * Retrieves balance entries according publickey
            *
            * @param key  publickey
            *
            * @return unordered_map<BalanceIdType, BalanceEntry>
            */
            unordered_map<BalanceIdType, BalanceEntry>     get_balances_for_key(const PublicKeyType& key)const;
            
            /**
            * Returns registered accounts starting with a given name upto a the limit provided.
            *
            * @param first the first account name to include (account_name, optional, defaults to "")
            * @param limit the maximum number of items to list (uint32_t, optional, defaults to 20)
            *
            * @return  vector<AccountEntry>
            */
            vector<AccountEntry>                             get_accounts(const string& first,
                    uint32_t limit)const;
                    
            /**
            * Returns registered account entry by given address.
            *
            * @param address_str the account address.
            *
            * @return  vector<AccountEntry>
            */
            oAccountEntry                                    get_account_by_address(const string  address_str) const;
            
            /**
            * Returns registered assets starting with a given name upto a the limit provided.
            *
            * @param first_symbol the prefix of the first asset symbol name to include (asset_symbol, optional, defaults to
            *                     "")
            * @param limit the maximum number of items to list (uint32_t, optional, defaults to 20)
            *
            * @return asset_entry_array
            */
            vector<AssetEntry>                               get_assets(const string& first_symbol,
                    uint32_t limit)const;
                    
            /**
            * Query the most recent block production slot entrys for the specified delegate.
            *
            * @param delegate_id Id of delegate whose block production slot entrys to query (string, required)
            * @param limit The maximum number of slot entrys to return (uint32_t, optional, defaults to "10")
            *
            * @return std::vector<SlotEntry>
            */
            std::vector<SlotEntry> get_delegate_slot_entrys(const AccountIdType delegate_id, uint32_t limit)const;
            
            /**
            * returns a list of all blocks for which there is a fork off of the main chain.
            *
            * @return map<uint32_t, vector<fork_entry>>
            */
            std::map<uint32_t, std::vector<ForkEntry> > get_forks_list()const;
            
            /**  Get the Max of the block_num in the _fork_db
            *
            *
            * @return uint32_t
            */
            uint32_t get_fork_list_num();
            
            /**  Export fork graph
            *
            * @param  start_block  uint32_t
            * @param  end_block  uint32_t
            * @param  filename  fc::path
            *
            * @return string
            */
            std::string export_fork_graph(uint32_t start_block = 1, uint32_t end_block = -1, const fc::path& filename = "")const;
            
            /** Should perform any chain reorganization required
            *   return the pending chain state generated as a result of pushing the block,
            *   this state can be used by wallets to scan for changes without the wallets
            *   having to process raw transactions.
            *
            * @param  block_data  FullBlock
            *
            * @return BlockForkData
            */
            BlockForkData push_block(const FullBlock& block_data);
            
            /**
            * Traverse the previous links of all blocks in fork until we find one that is_included
            *
            * The last item in the result will be the only block id that is already included in
            * the blockchain.
            *
            * @param  id  BlockIdType
            *
            * @return std::vector<BlockIdType>
            */
            vector<BlockIdType> get_fork_history(const BlockIdType& id);
            
            /**  Evaluate the transaction and return the results.
            *
            * @param  trx  SignedTransaction
            * @param  required_fees  ShareType
            *
            * @return TransactionEvaluationStatePtr
            */
            virtual TransactionEvaluationStatePtr   evaluate_transaction(const SignedTransaction& trx, const ShareType required_fees = 0, bool contract_vm_exec = false, bool skip_signature_check = false, bool throw_exec_exception=false);
            
            /**  Evaluate the transaction and return exception
            *
            * @param  transaction  SignedTransaction
            * @param  min_fee  ShareType
            *
            * @return optional<fc::exception>
            */
            optional<fc::exception>                    get_transaction_error(const SignedTransaction& transaction, const ShareType min_fee);
            
            /** Return the timestamp from the head block
            /**  now
            *
            *
            * @return fc::time_point_sec
            */
            virtual time_point_sec             now()const override;
            
            /** Top delegates by current vote, projected to be active in the next round
            /**  next_round_active_delegates
            *
            *
            * @return std::vector<AccountIdType>
            */
            vector<AccountIdType>            next_round_active_delegates()const;
            
            /**
            * Returns a list of the delegates sorted by vote at the range of (first, first+count).
            * sort by VoteDel.votes desc first, if the VoteDel.votes equals then sort by delegate_id
            *
            * @param first (uint32_t, optional, defaults to 0)
            * @param count (uint32_t, optional, defaults to -1)
            *
            * @return account_entry_array
            */
            vector<AccountIdType>            get_delegates_by_vote(uint32_t first = 0, uint32_t count = uint32_t(-1))const;
            
            /**  Returns a list of all the delegates sorted by vote.
            * sort by VoteDel.votes desc first, if the VoteDel.votes equals then sort by delegate_id
            *
            * @return std::vector<AccountIdType>
            */
            std::vector<AccountIdType>      get_all_delegates_by_vote()const;
            
            /**  Do the callback function(AccountEntry)   (not in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_unordered_accounts(const function<void(const AccountEntry&)>)const;
            
            /**  Do the callback function(AccountEntry)    (in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_ordered_accounts(const function<void(const AccountEntry&)>)const;
            
            /**  Do the callback function(AssetEntry)   (not in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_unordered_assets(const function<void(const AssetEntry&)>)const;
            
            /**  Do the callback function(AssetEntry)    (in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_ordered_assets(const function<void(const AssetEntry&)>)const;
            
            /**  Do the callback function(BalanceEntry)    (in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_balances(const function<void(const BalanceEntry&)> callback)const;
            
            /**  Do the callback function(TransactionEntry)    (in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_transactions(const function<void(const TransactionEntry&)> callback)const;
            
            
            /**  Do the callback function(ContractEntry)    (in character sequence)
            *
            * @param  callback function
            *
            * @return void
            */
            void                               scan_contracts(const function<void(const ContractEntry&)> callback)const;
            
            /**  Is asset symbol valid
            *
            * @param  asset_symbol  string
            *
            * @return bool
            */
            bool                               is_valid_symbol(const string& asset_symbol)const;
            
            /**  Get asset symbol by asset_id
            *
            * @param  asset_id  AssetIdType
            *
            * @return string
            */
            string                             get_asset_symbol(const AssetIdType asset_id)const;
            
            /**  Get asset id by asset_symbol
            *
            * @param  asset_symbol  string
            *
            * @return AssetIdType
            */
            AssetIdType                      get_asset_id(const string& asset_symbol)const;
            
            /**
            * Returns recent operations of specified type
            *
            * @param t OperationTypeEnum
            *
            * @return vector<Operation>
            */
            virtual vector<Operation>          get_recent_operations(OperationTypeEnum t)const;
            
            /**  Store recent operation in recent_op_queue FIFO
            * if the FIFO size reach the max setting size, then pop the first from the FIFO
            *
            * @param  o  Operation
            *
            * @return void
            */
            virtual void                       store_recent_operation(const Operation& o);
            
            /**
            * Save snapshot of current base asset balances to specified file.
            *
            * @param filename filename to save snapshot to (string, required)
            *
            * @return void
            */
            void                               generate_snapshot(const fc::path& filename)const;
            
            /**  generate_issuance_map
            *
            * @param  symbol  string
            * @param  filename  fc::path
            *
            * @return void
            */
            void                               generate_issuance_map(const string& symbol, const fc::path& filename)const;
            
            /**
            * Calculate the supply of the  specified asset.
            *
            * @param asset_id id of the specified asset
            *
            * @return Asset
            */
            Asset                              calculate_supply(const AssetIdType asset_id)const;
            
            /**  Total unclaimed balance amount  (last_update <= genesis_date)
            *
            *
            * @return Asset
            */
            Asset                              unclaimed_genesis();
            
            // Applies only when pushing new blocks; gets enabled in delegate loop
            bool                               _verify_transaction_signatures = false;
            
            /**  Get forkdb num
            *
            *
            * @return uint32_t
            */
            uint32_t    get_forkdb_num();
            
            void store_extend_status(const BlockIdType& id, int version);
            std::pair<BlockIdType, int> get_last_extend_status();
            /**  Set forkdb num
            *
            * @param  forkdb_num  uint32_t
            *
            * @return void
            */
            void    set_forkdb_num(uint32_t forkdb_num);
            /**  Repair error block data
            *
            *
            * @return void
            */
            void   repair_database();
            SignedTransaction transfer_asset_from_contract(
                double real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const Address& from_contract_address,
                const string& to_account_name);
                
            SignedTransaction transfer_asset_from_contract(
                double real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const Address& from_contract_address,
                const Address& to_account_address);
                
            /**  get all contract entries from db
            *
            * @param
            *
            * @return vector<ContractEntry>
            */
            vector<ContractIdType> get_all_contract_entries() const;
            
          private:
            unique_ptr<detail::ChainDatabaseImpl> my;
            uint32_t m_fork_num_before;
            
            /**  Get property by id from db
            *
            * @param  id  property_id_type
            *
            * @return oPropertyEntry
            */
            virtual oPropertyEntry property_lookup_by_id(const PropertyIdType)const override;
            
            /**  Store property by id to db
            *
            * @param  id  property_id_type
            * @param  entry  PropertyEntry
            *
            * @return void
            */
            virtual void property_insert_into_id_map(const PropertyIdType, const PropertyEntry&)override;
            
            /**  Erase property by id
            *
            * @param  id  property_id_type
            *
            * @return void
            */
            virtual void property_erase_from_id_map(const PropertyIdType)override;
            
            /**  Get AccountEntry by id
            *
            * @param  id  AccountIdType
            *
            * @return oAccountEntry
            */
            virtual oAccountEntry account_lookup_by_id(const AccountIdType)const override;
            
            /**  Get AccountEntry by name
            *
            * @param  name  string
            *
            * @return oAccountEntry
            */
            virtual oAccountEntry account_lookup_by_name(const string&)const override;
            
            /**  Get AccountEntry by address
            *
            * @param  addr  Address
            *
            * @return oAccountEntry
            */
            virtual oAccountEntry account_lookup_by_address(const Address&)const override;
            
            /**  Store AccountEntry to db by account_id
            *
            * @param  id  AccountIdType
            * @param  entry  AccountEntry
            *
            * @return void
            */
            virtual void account_insert_into_id_map(const AccountIdType, const AccountEntry&)override;
            
            /**  Store account id to db by name
            *
            * @param  name  string
            * @param  id  AccountIdType
            *
            * @return void
            */
            virtual void account_insert_into_name_map(const string&, const AccountIdType)override;
            
            /**  Store account id to db by address
            *
            * @param  addr  Address
            * @param  id  AccountIdType
            *
            * @return void
            */
            virtual void account_insert_into_address_map(const Address&, const AccountIdType)override;
            
            /**  Insert vote delegate into _delegate_votes
            *
            * @param  vote  VoteDel
            *
            * @return void
            */
            virtual void account_insert_into_vote_set(const VoteDel&)override;
            
            /**  Erase AccountEntry from db by account_id
            *
            * @param  id  AccountIdType
            *
            * @return void
            */
            virtual void account_erase_from_id_map(const AccountIdType)override;
            
            /**  Erase account id from db by name
            *
            * @param  name  string
            *
            * @return void
            */
            virtual void account_erase_from_name_map(const string&)override;
            
            /**  Erase account id from db by address
            *
            * @param  addr  Address
            *
            * @return void
            */
            virtual void account_erase_from_address_map(const Address&)override;
            
            /**  Erase vote delegate from _delegate_votes
            *
            * @param  vote  VoteDel
            *
            * @return void
            */
            virtual void account_erase_from_vote_set(const VoteDel&)override;
            
            /**  Get AssetEntry by asset_id
            *
            * @param  id  AssetIdType
            *
            * @return oAssetEntry
            */
            virtual oAssetEntry asset_lookup_by_id(const AssetIdType)const override;
            
            /**  Get AssetEntry by symbol
            *
            * @param  symbol  string
            *
            * @return oAssetEntry
            */
            virtual oAssetEntry asset_lookup_by_symbol(const string&)const override;
            
            /**  Store AssetEntry to db by asset_id
            *
            * @param  id  AssetIdType
            * @param  entry  AssetEntry
            *
            * @return void
            */
            virtual void asset_insert_into_id_map(const AssetIdType, const AssetEntry&)override;
            
            /**  Store asset id to db by symbol
            *
            * @param  symbol  string
            * @param  id  AssetIdType
            *
            * @return void
            */
            virtual void asset_insert_into_symbol_map(const string&, const AssetIdType)override;
            
            /**  Erase AssetEntry from db by asset_id
            *
            * @param  id  AssetIdType
            *
            * @return void
            */
            virtual void asset_erase_from_id_map(const AssetIdType)override;
            
            /**  Erase asset id from db by symbol
            *
            * @param  symbol  string
            *
            * @return void
            */
            virtual void asset_erase_from_symbol_map(const string&)override;
            
            /**  Get SlateEntry from db by slate_id
            *
            * @param  id  SlateIdType
            *
            * @return oSlateEntry
            */
            virtual oSlateEntry slate_lookup_by_id(const SlateIdType)const override;
            
            /**  Store SlateEntry to db by slate_id
            *
            * @param  id  SlateIdType
            * @param  entry  SlateEntry
            *
            * @return void
            */
            virtual void slate_insert_into_id_map(const SlateIdType, const SlateEntry&)override;
            
            /**  Erase SlateEntry from db by slate_id
            *
            * @param  id  SlateIdType
            *
            * @return void
            */
            virtual void slate_erase_from_id_map(const SlateIdType)override;
            
            /**  Get BalanceEntry from db by balance_id
            *
            * @param  id  BalanceIdType
            *
            * @return oBalanceEntry
            */
            virtual oBalanceEntry balance_lookup_by_id(const BalanceIdType&)const override;
            
            /**  Store BalanceEntry to db by balance_id
            *
            * @param  id  BalanceIdType
            * @param  entry  BalanceEntry
            *
            * @return void
            */
            virtual void balance_insert_into_id_map(const BalanceIdType&, const BalanceEntry&)override;
            
            /**  Erase BalanceEntry from db by balance_id
            *
            * @param  id  BalanceIdType
            *
            * @return void
            */
            virtual void balance_erase_from_id_map(const BalanceIdType&)override;
            
            virtual void status_insert_into_block_map(const BlockIdType&, const int&);
            virtual void status_erase_from_block_map();
            /**  Get TransactionEntry from db by transaction_id
            *
            * @param  id  TransactionIdType
            *
            * @return oTransactionEntry
            */
            virtual oTransactionEntry transaction_lookup_by_id(const TransactionIdType&)const override;
            
            /**  Store TransactionEntry to db by transaction_id
            *
            * @param  id  TransactionIdType
            * @param  entry  TransactionEntry
            *
            * @return void
            */
            virtual void transaction_insert_into_id_map(const TransactionIdType&, const TransactionEntry&)override;
            
            /**  Insert transaction into _unique_transactions set
            *
            * @param  trx  transaction
            *
            * @return void
            */
            virtual void transaction_insert_into_unique_set(const Transaction&)override;
            
            /**  Erase TransactionEntry from db by transaction_id
            *
            * @param  id  TransactionIdType
            *
            * @return void
            */
            virtual void transaction_erase_from_id_map(const TransactionIdType&)override;
            
            /**  Erase transaction from _unique_transactions set
            *
            * @param  trx  transaction
            *
            * @return void
            */
            virtual void transaction_erase_from_unique_set(const Transaction&)override;
            
            /**  Get SlotEntry from db by slot_index
            *
            * @param  index  SlotIndex
            *
            * @return oSlotEntry
            */
            virtual oSlotEntry slot_lookup_by_index(const SlotIndex)const override;
            
            /**  Get SlotEntry from db by timestamp
            *
            * @param  timestamp  time_point_sec
            *
            * @return oSlotEntry
            */
            virtual oSlotEntry slot_lookup_by_timestamp(const time_point_sec)const override;
            
            /**  Store SlotEntry to db by slot_index
            *
            * @param  index  SlotIndex
            * @param  entry  SlotEntry
            *
            * @return void
            */
            virtual void slot_insert_into_index_map(const SlotIndex, const SlotEntry&)override;
            
            /**  Store delegate id to db by timestamp
            *
            * @param  timestamp  time_point_sec
            * @param  delegate_id  AccountIdType
            *
            * @return void
            */
            virtual void slot_insert_into_timestamp_map(const time_point_sec, const AccountIdType)override;
            
            /**  Erase SlotEntry from db by slot_index
            *
            * @param  index  SlotIndex
            *
            * @return void
            */
            virtual void slot_erase_from_index_map(const SlotIndex)override;
            
            /**  Erase delegate id from db by timestamp
            *
            * @param  timestamp  time_point_sec
            *
            * @return void
            */
            virtual void slot_erase_from_timestamp_map(const time_point_sec)override;
            
            /**
            * Lookup contractInfo by contract id from blockchain db.
            *
            * @param  id  ContractIdType
            *
            * @return oContractInfo
            */
            virtual  oContractEntry  contract_lookup_by_id(const ContractIdType&)const override;
            
            
            /**
            * Lookup ContractIdType by contract name from blockchain db.
            *
            * @param  name  ContractName
            *
            * @return oContractEntry
            */
            virtual  oContractEntry  contract_lookup_by_name(const ContractName&)const override;
            
            /**
            * Lookup contractStorage by contract id from blockchain db.
            *
            * @param  id  ContractIdType
            *
            * @return oContractStorage
            */
            virtual oContractStorage contractstorage_lookup_by_id(const ContractIdType&)const override;
            
            /**  Store contractInfo to db by contract_id
            *
            * @param  id  ContractIdType
            * @param  info  contractInfo
            *
            * @return void
            */
            virtual void contract_insert_into_id_map(const ContractIdType&, const ContractEntry&) override;
            
            /**  Store contractStorage to db by contract_id
            *
            * @param  id  ContractIdType
            * @param  storage  contractStorage
            *
            * @return void
            */
            virtual void contractstorage_insert_into_id_map(const ContractIdType&, const ContractStorageEntry&) override;
            
            /**  Store contractId to db by contract_name
            *
            * @param  name  ContractName
            * @param  id  ContractIdType
            *
            * @return void
            */
            virtual void contract_insert_into_name_map(const ContractName&, const ContractIdType&) override;
            
            /**  Erase from db by contract_id
            *
            * @param  id  ContractIdType
            *
            * @return void
            */
            virtual void contract_erase_from_id_map(const ContractIdType&) override;
            
            /**  Erase from db by contract_id
            *
            * @param  id  ContractIdType
            *
            * @return void
            */
            virtual void contractstorage_erase_from_id_map(const ContractIdType&) override;
            
            /**  Erase from  db by contract_name
            *
            * @param  name  ContractName
            *
            * @return void
            */
            virtual void contract_erase_from_name_map(const ContractName&) override;
            
            /** lookup result transaction id from db by request transaction id
            *
            * @param    id  TransactionIdType
            *
            * @return oResultTIdEntry
            */
            virtual oResultTIdEntry contract_lookup_resultid_by_reqestid(const TransactionIdType&) const override;
            /** store result transaction id to db by request trx id
            *
            * @param    req TransactionIdType
            *
            * @param    res ResultTIdEntry
            *
            * @return void
            */
            virtual void contract_store_resultid_by_reqestid(const TransactionIdType& req, const ResultTIdEntry& res) override;
            /** erase result transaction id from db by request trx id
            *
            * @param req  TransactionIdType
            *
            * @return void
            */
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
            virtual void contract_erase_contractid_by_trxid(const TransactionIdType& );
            
            
          public:
            bool generating_block;
            
        };
        typedef shared_ptr<ChainDatabase> ChainDatabasePtr;
        
    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::BlockForkData, (next_blocks)(is_linked)(is_valid)(invalid_reason)(is_included)(is_known))
FC_REFLECT(thinkyoung::blockchain::ForkEntry, (block_id)(signing_delegate)(transaction_count)(latency)(size)(timestamp)(is_valid)(invalid_reason)(is_current_fork))
