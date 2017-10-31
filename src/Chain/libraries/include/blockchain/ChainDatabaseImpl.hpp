#pragma once

#include <blockchain/ChainDatabase.hpp>
#include <db/CachedLevelMap.hpp>
#include <db/FastLevelMap.hpp>
#include <fc/thread/mutex.hpp>
#include <utilities/ThreadPool.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct fee_index
        {
            ShareType            _fees = 0;
            TransactionIdType   _trx;

            fee_index(ShareType fees = 0, TransactionIdType trx = TransactionIdType())
                :_fees(fees), _trx(trx){}

            friend bool operator == (const fee_index& a, const fee_index& b)
            {
                return std::tie(a._fees, a._trx) == std::tie(b._fees, b._trx);
            }

            friend bool operator < (const fee_index& a, const fee_index& b)
            {
                // Reverse so that highest fee is placed first in sorted maps
                return std::tie(a._fees, a._trx) > std::tie(b._fees, b._trx);
            }
        };

        struct data_version
        {
            BlockIdType id;
            int version;
        };
        namespace detail
        {
            class ChainDatabaseImpl
            {
            public:
                /**
                * load checkpoints from file
                * @param  data_dir  directory  that stores checkpoints
                *
                * @return void
                */
                void                                        load_checkpoints(const fc::path& data_dir)const;
                /**
                * Checks whether replaying is require
                * Get database_version from property database and check whether it equals to ALP_BLOCKCHAIN_DATABASE_VERSION
                * @param  data_dir  path of property database
                *
                * @return bool
                */
                bool                                        replay_required(const fc::path& data_dir);
                /**
                * load database from specified directory
                * @param  data_dir    path of database
                *
                * @return void
                */
                void                                        open_database(const fc::path& data_dir);
                /**  clear_invalidation_of_future_blocks
                * Remove blocks whose block time is 2 days ago from future block list and clear other blocks' invalid flag
                *
                * @return void
                */
                void                                        clear_invalidation_of_future_blocks();
                /**  initialize_genesis
                * Load data of genesis into database
                * @param   optional<path>&  path of the genesis file.If it's  invalid,load data from builtin genesis
                * @param   bool statistics_enabled
                *
                * @return DigestType
                */
                DigestType                                 initialize_genesis(const optional<path>& genesis_file,
                    const bool statistics_enabled);
                /**  populate_indexes
                *
                * Sorts delegates according votes and puts transaction into unique set
                *
                * @return void
                */
                void                                        populate_indexes();

                /**  store_and_index
                * Store a block into database and  organize related data
                * @param  block_id  id of the block
                * @param  block_data  FullBlock
                *
                * @return std::pair<BlockIdType,
                */
                std::pair<BlockIdType, BlockForkData>   store_and_index(const BlockIdType& id, const FullBlock& blk);

                /**  clear_pending
                * Remove transactions which are contained in specified block and start a revalidating procedure
                * @param  block_data  FullBlock
                *
                * @return void
                */
                void                                        clear_pending(const FullBlock& block_data);
                /*
                * revalidate transactions in pending database
                *
                * @return void
                */
                void                                        revalidate_pending();

                /**  switch_to_fork
                * Marks the fork which the specified block are in it to be the current chain
                * @param  block_id  id of the specified block
                *
                * @return void
                */
                void                                        switch_to_fork(const BlockIdType& block_id);

				/**  get_events
				* Query the emit's result of event from the contract.
				* @param  uint32_t  block_index
				* @param  string  trx_id
				*
				* @return vector<EventOperation>
				*/
				vector<EventOperation>                      get_events(uint32_t block_index, const thinkyoung::blockchain::TransactionIdType& trx_id_type);
				
				/**  extend_chain
                * Performs all of the block validation steps and throws if error.
                * @param  block_data  FullBlock
                *
                * @return void
                */

                void                                        extend_chain(const FullBlock& blk);
                /**  get_fork_history
                * Traverse the previous links of all blocks in fork until we find one that is_included
                *
                * The last item in the result will be the only block id that is already included in
                * the blockchain.
                *
                * @param  id  BlockIdType
                *
                * @return std::vector<BlockIdType>
                */
                vector<BlockIdType>                       get_fork_history(const BlockIdType& id);
                /**  pop_block
                *
                * Pop the headblock from current chain and undo changes which are made by the head block
                *
                * @return void
                */
                void                                        pop_block();

                void                                        repair_block(BlockIdType block_id);
                /**  mark_invalid
                * fetch the fork data for block_id, mark it as invalid and
                * then mark every item after it as invalid as well.
                * @param  block_id  BlockIdType
                * @param  reason  fc::exception
                *
                * @return void
                */
                void                                        mark_invalid(const BlockIdType& id, const fc::exception& reason);
                /**  mark_as_unchecked
                * fetch the fork data for block_id, mark it as unchecked
                * @param  block_id  BlockIdType
                *
                * @return void
                */
                void                                        mark_as_unchecked(const BlockIdType& id);
                /**  mark_included
                *  fetch the fork data for block_id, mark it as included or not included
                * @param  block_id  BlockIdType
                * @param  included  bool
                *
                * @return void
                */
                void                                        mark_included(const BlockIdType& id, bool state);

                /**  fetch_blocks_at_number
                * fetch ids of block at specified number
                * @param  block_num  uint32_t
                *
                * @return std::vector<BlockIdType>
                */
                std::vector<BlockIdType>                  fetch_blocks_at_number(uint32_t block_num);

                /**  recursive_mark_as_linked
                * fetch the fork data for block_id, mark it as linked and
                * then mark every item after it as linked as well.
                *
                * @param  ids  std::unordered_set<BlockIdType>
                *
                * @return std::pair<BlockIdType,
                */
                std::pair<BlockIdType, BlockForkData>   recursive_mark_as_linked(const std::unordered_set<BlockIdType>& ids);
                /**  recursive_mark_as_invalid
                * Mark blocks as invalid recursively
                * @param  ids  std::unordered_set<BlockIdType>
                * @param  reason  fc::exception
                *
                * @return void
                */
                void                                        recursive_mark_as_invalid(const std::unordered_set<BlockIdType>& ids,
                    const fc::exception& reason);

                /**  verify_header
                * Verify signee of the block
                * @param  block_digest  DigestBlock
                * @param  block_signee  PublicKeyType
                *
                * @return void
                */
                void                                        verify_header(const DigestBlock& block_digest,
                    const PublicKeyType& block_signee)const;

                /**  update_delegate_production_info
                * Update production info for signing delegate
                * @param  block_header  BlockHeader
                * @param  block_id  BlockIdType
                * @param  block_signee  public key of signing delegate
                * @param  pending_state  PendingChainStatePtr
                *
                * @return void
                */
                void                                        update_delegate_production_info(const BlockHeader& BlockHeader,
                    const BlockIdType& block_id,
                    const PublicKeyType& block_signee,
                    const PendingChainStatePtr& pending_state)const;

                /**  pay_delegate
                * Pay salaries to specified delegates
                * @param  block_id  BlockIdType
                * @param  block_signee  public key of the delegate to be paid
                * @param  pending_state  PendingChainStatePtr
                * @param  entry  oBlockEntry
                *
                * @return void
                */
                void                                        pay_delegate(const BlockIdType& block_id,
                    const PublicKeyType& block_signee,
                    const PendingChainStatePtr& pending_state,
                    oBlockEntry& block_entry)const;

                // void                                        execute_markets( const time_point_sec timestamp,
                //                                                              const pending_chain_state_ptr& pending_state )const;

                /**  apply_transactions
                * Apply transactions contained in the block
                * @param  block_data   the block that contains transactions
                * @param  pending_state  PendingChainStatePtr
                *
                * @return void
                */
                void                                        apply_transactions(const FullBlock& block_data,
                    const PendingChainStatePtr& pending_state);

                /**  update_active_delegate_list
                * Get a list of active delegate that would participate in generating blocks in next round
                * @param  block_num  uint32_t
                * @param  pending_state  PendingChainStatePtr
                *
                * @return void
                */
                void                                        update_active_delegate_list(const uint32_t block_num,
                    const PendingChainStatePtr& pending_state)const;

                /**  update_random_seed
                * Caculate the random seed based on a secret and the random seed
                * @param  new_secret
                * @param  pending_state  PendingChainStatePtr
                * @param  entry  oBlockEntry
                *
                * @return void
                */
                void                                        update_random_seed(const SecretHashType& new_secret,
                    const PendingChainStatePtr& pending_state,
                    oBlockEntry& block_entry)const;

                /**  save_undo_state
                * Save current state and block_id into undo_state map
                * @param  block_num   number of the block
                * @param  block_id  id of the block
                * @param  pending_state  State need to be saved
                *
                * @return void
                */
                void                                        save_undo_state(const uint32_t block_num,
                    const BlockIdType& block_id,
                    const PendingChainStatePtr& pending_state);

                /**  update_head_block
                * Update information about head block
                * @param  block_header  SignedBlockHeader
                * @param  block_id  BlockIdType
                *
                * @return void
                */
                void                                        update_head_block(const SignedBlockHeader& block_header,
                    const BlockIdType& block_id);

                void                                        pay_delegate_v2(const BlockIdType& block_id,
                    const PublicKeyType& block_signee,
                    const PendingChainStatePtr& pending_state,
                    oBlockEntry& block_entry)const;

                void                                        pay_delegate_v1(const BlockIdType& block_id,
                    const PublicKeyType& block_signee,
                    const PendingChainStatePtr& pending_state,
                    oBlockEntry& block_entry)const;


                //void                                        update_active_delegate_list_v1( const uint32_t block_num,
                //                                                                          const pending_chain_state_ptr& pending_state )const;

                ChainDatabase*                                                             self = nullptr;
                unordered_set<ChainObserver*>                                              _observers;

                /* Transaction propagation */
                /**  revalidate_pending
                *

                *
                * @return void
                */
                fc::future<void>                                                            _revalidate_pending;
                ThreadPool                                                                  _thread_pool;
                PendingChainStatePtr                                                     _pending_trx_state = nullptr;
                thinkyoung::db::LevelMap<TransactionIdType, SignedTransaction>                 _pending_transaction_db;
                map<fee_index, TransactionEvaluationStatePtr>                            _pending_fee_index;
                ShareType                                                                  _relay_fee = ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;

                /* Block processing */
                uint32_t /* Only used to skip undo states when possible during replay */    _min_undo_block = 0;

                fc::mutex                                                                   _push_block_mutex;
                ShareType															_block_per_account_reword_amount = ALP_MAX_DELEGATE_PAY_PER_BLOCK;
                thinkyoung::db::LevelMap<BlockIdType, FullBlock>                               _block_id_to_full_block;
                thinkyoung::db::fast_level_map<BlockIdType, PendingChainState>                 _block_id_to_undo_state;

                thinkyoung::db::LevelMap<uint32_t, vector<BlockIdType>>                         _fork_number_db; // All siblings
                thinkyoung::db::LevelMap<BlockIdType, BlockForkData>                          _fork_db;

                thinkyoung::db::LevelMap<BlockIdType, int32_t>                                  _revalidatable_future_blocks_db; //int32_t is unused, this is a set

                thinkyoung::db::LevelMap<uint32_t, BlockIdType>                                 _block_num_to_id_db; // Current chain

                thinkyoung::db::LevelMap<BlockIdType, BlockEntry>                             _block_id_to_block_entry_db; // Statistics

                /* Current primary state */
                BlockIdType                                                               _head_block_id;
                SignedBlockHeader                                                         _head_block_header;

                thinkyoung::db::fast_level_map<uint8_t, PropertyEntry>                           _property_id_to_entry;

                thinkyoung::db::fast_level_map<AccountIdType, AccountEntry>                    _account_id_to_entry;
                thinkyoung::db::fast_level_map<string, AccountIdType>                            _account_name_to_id;
                thinkyoung::db::fast_level_map<Address, AccountIdType>                           _account_address_to_id;
                set<VoteDel>                                                               _delegate_votes;

                thinkyoung::db::fast_level_map<AssetIdType, AssetEntry>                        _asset_id_to_entry;
                thinkyoung::db::fast_level_map<string, AssetIdType>                              _asset_symbol_to_id;

                thinkyoung::db::fast_level_map<SlateIdType, SlateEntry>                        _slate_id_to_entry;

                thinkyoung::db::fast_level_map<BalanceIdType, BalanceEntry>                    _balance_id_to_entry;
                thinkyoung::db::fast_level_map<string, set<AlpTrxidBalance>>							_alp_input_balance_entry;
                thinkyoung::db::fast_level_map<string, AlpBalanceEntry>						_alp_full_entry;
                thinkyoung::db::LevelMap<TransactionIdType, TransactionEntry>                 _transaction_id_to_entry;
                set<UniqueTransactionKey>                                                 _unique_transactions;
                thinkyoung::db::LevelMap<Address, unordered_set<TransactionIdType>>             _address_to_transaction_ids;




                thinkyoung::db::LevelMap<SlotIndex, SlotEntry>                                 _slot_index_to_entry;
                thinkyoung::db::LevelMap<time_point_sec, AccountIdType>                         _slot_timestamp_to_delegate;

                thinkyoung::db::LevelMap<int, data_version>                      _block_extend_status;
                // TODO: Just store whitelist in asset_entry
                //thinkyoung::db::level_map<pair<asset_id_type,address>, object_id_type>             _auth_db;

                map<OperationTypeEnum, std::deque<Operation>>                             _recent_operations;

                // contract related db
                thinkyoung::db::fast_level_map<ContractIdType, ContractEntry>                  _contract_id_to_entry;
                thinkyoung::db::fast_level_map<ContractIdType, ContractStorageEntry>               _contract_id_to_storage;
                thinkyoung::db::fast_level_map<ContractName, ContractIdType>                  _contract_name_to_id;
				thinkyoung::db::fast_level_map<TransactionIdType, ResultTIdEntry>		  _request_to_result_iddb;
				thinkyoung::db::fast_level_map<TransactionIdType, RequestIdEntry>		  _result_to_request_iddb;
				thinkyoung::db::fast_level_map<TransactionIdType, ContractinTrxEntry>		  _trx_to_contract_iddb;
				thinkyoung::db::fast_level_map<ContractIdType,ContractTrxEntry>		  _contract_to_trx_iddb;
                // sandbox contract related
                PendingChainStatePtr	_sandbox_pending_state = nullptr;
                bool                    _is_in_sandbox = false;
            };

        } // detail
    }
} // thinkyoung::blockchain

FC_REFLECT_TYPENAME(std::vector<thinkyoung::blockchain::BlockIdType>)
FC_REFLECT_TYPENAME(std::unordered_set<thinkyoung::blockchain::TransactionIdType>)
FC_REFLECT(thinkyoung::blockchain::fee_index, (_fees)(_trx))
FC_REFLECT(thinkyoung::blockchain::data_version, (id)(version))
