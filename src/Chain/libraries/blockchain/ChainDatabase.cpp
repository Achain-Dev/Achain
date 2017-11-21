#include <blockchain/AssetOperations.hpp>
#include <blockchain/BalanceOperations.hpp>
#include <blockchain/TransactionOperations.hpp>
#include <blockchain/ChainDatabase.hpp>
#include <blockchain/ChainDatabaseImpl.hpp>
#include <blockchain/Checkpoints.hpp>
#include <blockchain/Config.hpp>
#include <blockchain/Exceptions.hpp>
#include <blockchain/GenesisState.hpp>
#include <blockchain/GenesisJson.hpp>

#include <blockchain/Time.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
#include <fc/thread/unique_lock.hpp>

#include <iomanip>
#include <iostream>

#include <blockchain/ForkBlocks.hpp>
#include <fc/real128.hpp>
#include <blockchain/api_extern.hpp>
#include <thread>
#include <blockchain/ContractOperations.hpp>
namespace thinkyoung {
    namespace blockchain {
    
        const static short MAX_RECENT_OPERATIONS = 20;
        
        namespace detail {
            void ChainDatabaseImpl::revalidate_pending() {
                _pending_fee_index.clear();
                int count = 0;
                vector<TransactionIdType> trx_to_discard;
                _pending_trx_state = std::make_shared<PendingChainState>(self->shared_from_this());
                unsigned num_pending_transaction_considered = 0;
                auto itr = _pending_transaction_db.begin();
                
                while (itr.valid()) {
                    SignedTransaction trx = itr.value();
                    const TransactionIdType trx_id = itr.key();
                    assert(trx_id == trx.id());
                    
                    try {
                        TransactionEvaluationStatePtr eval_state = self->evaluate_transaction(trx, _relay_fee, false, true);
                        
                        if (eval_state->p_result_trx.operations.size() > 0) {
                            eval_state->p_result_trx.operations.resize(0);
                        }
                        
                        ShareType fees = eval_state->get_fees();
                        _pending_fee_index[fee_index(fees, trx_id)] = eval_state;
                        count++;
                        
                        if (count > ALP_BLOCKCHAIN_REVALIDATE_MAX_TRX_COUNT)
                            break;
                            
                        ilog("revalidated pending transaction id ${id}", ("id", trx_id));
                        
                    } catch (const fc::canceled_exception&) {
                        throw;
                        
                    } catch (const fc::exception& e) {
                        trx_to_discard.push_back(trx_id);
                        ilog("discarding invalid transaction: ${id} ${e}",
                             ("id", trx_id)("e", e.to_detail_string()));
                    }
                    
                    ++num_pending_transaction_considered;
                    ++itr;
                }
                
                for (const auto& item : trx_to_discard)
                    _pending_transaction_db.remove(item);
                    
                ilog("revalidate_pending complete, there are now ${pending_count} evaluated transactions, ${num_pending_transaction_considered} raw transactions",
                     ("pending_count", _pending_fee_index.size())
                     ("num_pending_transaction_considered", num_pending_transaction_considered));
            }
            
            void ChainDatabaseImpl::load_checkpoints(const fc::path& data_dir)const {
                try {
                    for (const auto& item : CHECKPOINT_BLOCKS)
                        LAST_CHECKPOINT_BLOCK_NUM = std::max(item.first, LAST_CHECKPOINT_BLOCK_NUM);
                        
                    const fc::path checkpoint_file = data_dir / "checkpoints.json";
                    decltype(CHECKPOINT_BLOCKS) external_checkpoints;
                    fc::oexception file_exception;
                    
                    if (fc::exists(checkpoint_file)) {
                        try {
                            external_checkpoints = fc::json::from_file(checkpoint_file).as<decltype(external_checkpoints)>();
                            
                        } catch (const fc::exception& e) {
                            file_exception = e;
                        }
                    }
                    
                    uint32_t external_checkpoint_max_block_num = 0;
                    
                    for (const auto& item : external_checkpoints)
                        external_checkpoint_max_block_num = std::max(item.first, external_checkpoint_max_block_num);
                        
                    if (external_checkpoint_max_block_num >= LAST_CHECKPOINT_BLOCK_NUM) {
                        ulog("Using blockchain checkpoints from file: ${x}", ("x", checkpoint_file.preferred_string()));
                        CHECKPOINT_BLOCKS = external_checkpoints;
                        LAST_CHECKPOINT_BLOCK_NUM = external_checkpoint_max_block_num;
                        return;
                    }
                    
                    if (!file_exception.valid()) {
                        fc::remove_all(checkpoint_file);
                        fc::json::save_to_file(CHECKPOINT_BLOCKS, checkpoint_file);
                        
                    } else {
                        ulog("Error loading blockchain checkpoints from file: ${x}", ("x", checkpoint_file.preferred_string()));
                    }
                    
                    if (!CHECKPOINT_BLOCKS.empty())
                        ulog("Using built-in blockchain checkpoints");
                }
                
                FC_CAPTURE_AND_RETHROW((data_dir))
            }
            
            bool ChainDatabaseImpl::replay_required(const fc::path& data_dir) {
                try {
                    _property_id_to_entry.open(data_dir / "index/property_id_to_entry");
                    const oPropertyEntry entry = self->get_property_entry(PropertyIdType::database_version);
                    _property_id_to_entry.close();
                    return !entry.valid() || entry->value.as_uint64() != ALP_BLOCKCHAIN_DATABASE_VERSION;
                }
                
                FC_CAPTURE_AND_RETHROW((data_dir))
            }
            
            void ChainDatabaseImpl::open_database(const fc::path& data_dir) {
                try {
                    _block_id_to_full_block.open(data_dir / "raw_chain/block_id_to_block_data_db");
                    _block_id_to_undo_state.open(data_dir / "index/block_id_to_undo_state");
                    _fork_number_db.open(data_dir / "index/fork_number_db");
                    _fork_db.open(data_dir / "index/fork_db");
                    _revalidatable_future_blocks_db.open(data_dir / "index/future_blocks_db");
                    _block_num_to_id_db.open(data_dir / "raw_chain/block_num_to_id_db");
                    _block_id_to_block_entry_db.open(data_dir / "index/block_id_to_block_entry_db");
                    _property_id_to_entry.open(data_dir / "index/property_id_to_entry");
                    _account_id_to_entry.open(data_dir / "index/account_id_to_entry");
                    _account_name_to_id.open(data_dir / "index/account_name_to_id");
                    _account_address_to_id.open(data_dir / "index/account_address_to_id");
                    //contract db
                    _contract_id_to_entry.open(data_dir / "index/contract_id_to_entry");
                    _contract_id_to_storage.open(data_dir / "index/contract_id_to_storage");
                    _contract_name_to_id.open(data_dir / "index/contract_name_to_id");
                    _result_to_request_iddb.open(data_dir / "index/_result_to_request_id");
                    _asset_id_to_entry.open(data_dir / "index/asset_id_to_entry");
                    _asset_symbol_to_id.open(data_dir / "index/asset_symbol_to_id");
                    _slate_id_to_entry.open(data_dir / "index/slate_id_to_entry");
                    _balance_id_to_entry.open(data_dir / "index/balance_id_to_entry");
                    _transaction_id_to_entry.open(data_dir / "index/transaction_id_to_entry");
                    _address_to_transaction_ids.open(data_dir / "index/address_to_transaction_ids");
                    _alp_input_balance_entry.open(data_dir / "index/_alp_input_balance_entry");
                    _alp_full_entry.open(data_dir / "index/_alp_full_entry");
                    _block_extend_status.open(data_dir / "index/_block_extend_status");
                    _pending_transaction_db.open(data_dir / "index/pending_transaction_db");
                    _slot_index_to_entry.open(data_dir / "index/slot_index_to_entry");
                    _slot_timestamp_to_delegate.open(data_dir / "index/slot_timestamp_to_delegate");
                    _request_to_result_iddb.open(data_dir / "index/_request_to_result_iddb");
                    _contract_to_trx_iddb.open(data_dir / "index/_contract_to_trx_iddb");
                    _trx_to_contract_iddb.open(data_dir / "index/_trx_to_contract_iddb");
                    _pending_trx_state = std::make_shared<PendingChainState>(self->shared_from_this());
                    clear_invalidation_of_future_blocks();
                }
                
                FC_CAPTURE_AND_RETHROW((data_dir))
            }
            
            void ChainDatabaseImpl::populate_indexes() {
                try {
                    for (auto iter = _account_id_to_entry.unordered_begin();
                            iter != _account_id_to_entry.unordered_end(); ++iter) {
                        const AccountEntry& entry = iter->second;
                        
                        if (!entry.is_retracted() && entry.is_delegate())
                            _delegate_votes.emplace(entry.net_votes(), entry.id);
                    }
                    
                    for (auto iter = _transaction_id_to_entry.begin(); iter.valid(); ++iter) {
#ifdef __linux__
                        const Transaction trx = iter.value().trx;
#else
                        const Transaction& trx = iter.value().trx;
#endif
                        
                        if (trx.expiration > self->now())
                            _unique_transactions.emplace(trx, self->get_chain_id());
                    }
                }
                
                FC_CAPTURE_AND_RETHROW()
            }
            
            void ChainDatabaseImpl::clear_invalidation_of_future_blocks() {
                for (auto block_id_itr = _revalidatable_future_blocks_db.begin(); block_id_itr.valid();) {
                    auto full_data = _block_id_to_full_block.fetch(block_id_itr.key());
                    fc::time_point_sec now = blockchain::now();
                    fc::time_point_sec block_time = fc::time_point_sec(full_data.timestamp);
                    
                    //int32_t aaa = (now - block_time).to_seconds();
                    if ((now - block_time).to_seconds() > (2 * 24 * 60 * 60)) { //[CN]时间超过2天就删除[CN]
                        auto temp_id = block_id_itr.key();
                        ++block_id_itr;
                        _revalidatable_future_blocks_db.remove(temp_id, false);
                        
                    } else {
                        ++block_id_itr;
                    }
                }
                
                for (auto block_id_itr = _revalidatable_future_blocks_db.begin(); block_id_itr.valid(); ++block_id_itr) {
                    mark_as_unchecked(block_id_itr.key());
                }
            }
            
            DigestType ChainDatabaseImpl::initialize_genesis(const optional<path>& genesis_file, const bool statistics_enabled) {
                try {
                    GenesisState config;
                    DigestType chain_id;
                    
                    if (!genesis_file.valid()) {
                        std::cout << "Initializing state from built-in genesis file\n";
                        config = get_builtin_genesis_block_config();
                        chain_id = get_builtin_genesis_block_state_hash();
                        
                    } else {
                        std::cout << "Initializing state from genesis file: " << genesis_file->generic_string() << "\n";
                        FC_ASSERT(fc::exists(*genesis_file), "Genesis file '${file}' was not found.", ("file", *genesis_file));
                        
                        if (genesis_file->extension() == ".json") {
                            config = fc::json::from_file(*genesis_file).as<GenesisState>();
                            
                        } else if (genesis_file->extension() == ".dat") {
                            fc::ifstream in(*genesis_file);
                            fc::raw::unpack(in, config);
                            
                        } else {
                            FC_ASSERT(false, "Invalid genesis format '${format}'", ("format", genesis_file->extension()));
                        }
                        
                        fc::sha256::encoder enc;
                        fc::raw::pack(enc, config);
                        chain_id = enc.result();
                    }
                    
                    self->set_chain_id(chain_id);
                    // Check genesis state
                    FC_ASSERT(config.delegates.size() >= ALP_BLOCKCHAIN_NUM_DELEGATES,
                              "genesis.json does not contain enough initial delegates!",
                              ("required", ALP_BLOCKCHAIN_NUM_DELEGATES)("provided", config.delegates.size()));
                    const fc::time_point_sec timestamp = config.timestamp;
                    // Initialize delegates
                    AccountIdType account_id = 0;
                    
                    for (const GenesisDelegate& delegate : config.delegates) {
                        ++account_id;
                        AccountEntry rec;
                        rec.id = account_id;
                        rec.name = delegate.name;
                        rec.owner_key = delegate.owner;
                        rec.set_active_key(timestamp, delegate.owner);
                        rec.registration_date = timestamp;
                        rec.last_update = timestamp;
                        rec.delegate_info = DelegateStats();
                        rec.delegate_info->pay_rate = 100;
                        rec.set_signing_key(0, delegate.owner);
                        self->store_account_entry(rec);
                    }
                    
                    // For loading balances originally snapshotted from other chains
                    const auto convert_raw_address = [](const string& raw_address) -> Address {
                        //static const vector<string> alp_prefixes{ "ALP", "KEY", "DVS", "XTS" };
                        try {
                            return Address(raw_address);
                            
                        } catch (const fc::exception&) {
                            //for( const string& prefix : alp_prefixes )
                            //{
                            // if( raw_address.find( prefix ) == 0 )
                            //  {
                            //  return address( ALP_ADDRESS_PREFIX + raw_address.substr( prefix.size() ) );
                            // }
                            //}
                        }
                        
                        FC_THROW_EXCEPTION(invalid_pts_address, "Invalid raw address format!", ("raw_address", raw_address));
                    };
                    // Initialize signature balances
                    ShareType total_base_supply = 0;
                    
                    for (const auto& genesis_balance : config.initial_balances) {
                        const auto addr = convert_raw_address(genesis_balance.raw_address);
                        BalanceEntry initial_balance(addr, Asset(genesis_balance.balance, 0), 0);
                        /* In case of redundant balances */
                        const auto cur = self->get_balance_entry(initial_balance.id());
                        
                        if (cur.valid()) initial_balance.balance += cur->balance;
                        
                        initial_balance.snapshot_info = SnapshotEntry(genesis_balance.raw_address, genesis_balance.balance);
                        initial_balance.last_update = config.timestamp;
                        self->store_balance_entry(initial_balance);
                        total_base_supply += genesis_balance.balance;
                    }
                    
                    // Initialize base asset
                    AssetIdType asset_id = 0;
                    AssetEntry base_asset;
                    base_asset.id = asset_id;
                    base_asset.symbol = ALP_BLOCKCHAIN_SYMBOL;
                    base_asset.name = ALP_BLOCKCHAIN_NAME;
                    base_asset.description = ALP_BLOCKCHAIN_DESCRIPTION;
                    base_asset.public_data = variant("");
                    base_asset.issuer_account_id = 0;
                    base_asset.precision = ALP_BLOCKCHAIN_PRECISION;
                    base_asset.registration_date = timestamp;
                    base_asset.last_update = timestamp;
                    base_asset.current_share_supply = total_base_supply;
                    base_asset.maximum_share_supply = ALP_BLOCKCHAIN_MAX_SHARES;
                    base_asset.collected_fees = 0;
                    base_asset.flags = AssetPermissions::none;
                    base_asset.issuer_permissions = AssetPermissions::none;
                    self->store_asset_entry(base_asset);
                    
                    // Initialize initial market assets
                    for (const GenesisAsset& asset : config.market_assets) {
                        ++asset_id;
                        AssetEntry rec;
                        rec.id = asset_id;
                        rec.symbol = asset.symbol;
                        rec.name = asset.name;
                        rec.description = asset.description;
                        rec.public_data = variant("");
                        rec.issuer_account_id = AssetEntry::market_issuer_id;
                        rec.precision = asset.precision;
                        rec.registration_date = timestamp;
                        rec.last_update = timestamp;
                        rec.current_share_supply = 0;
                        rec.maximum_share_supply = ALP_BLOCKCHAIN_MAX_SHARES;
                        rec.collected_fees = 0;
                        self->store_asset_entry(rec);
                    }
                    
                    //add fork_data for the genesis block to the fork database
                    BlockForkData gen_fork;
                    gen_fork.is_valid = true;
                    gen_fork.is_included = true;
                    gen_fork.is_linked = true;
                    gen_fork.is_known = true;
                    _fork_db.store(BlockIdType(), gen_fork);
                    self->store_property_entry(PropertyIdType::last_asset_id, variant(asset_id));
                    self->store_property_entry(PropertyIdType::last_account_id, variant(account_id));
                    self->set_active_delegates(self->next_round_active_delegates());
                    self->set_statistics_enabled(statistics_enabled);
                    self->set_node_vm_enabled(false);
                    return chain_id;
                }
                
                FC_CAPTURE_AND_RETHROW((genesis_file)(statistics_enabled))
            }
            
            std::vector<BlockIdType> ChainDatabaseImpl::fetch_blocks_at_number(uint32_t block_num) {
                try {
                    const auto itr = _fork_number_db.find(block_num);
                    
                    if (itr.valid()) return itr.value();
                    
                    return vector<BlockIdType>();
                }
                
                FC_CAPTURE_AND_RETHROW((block_num))
            }
            
            void ChainDatabaseImpl::clear_pending(const FullBlock& block_data) {
                try {
                    for (const SignedTransaction& trx : block_data.user_transactions) {
                        if (!trx.operations.empty() && trx.operations[0].type == transaction_op_type) {
                            TransactionOperation forward_trx_op = trx.operations[0].as<TransactionOperation>();
                            SignedTransaction forward_trx = forward_trx_op.trx;
                            _pending_transaction_db.remove(forward_trx.id());
                        }
                        
                        _pending_transaction_db.remove(trx.id());
                    }
                    
                    _pending_fee_index.clear();
                    _pending_trx_state = std::make_shared<PendingChainState>(self->shared_from_this());
                    
                    // this schedules the revalidate-pending-transactions task to execute in this thread
                    // as soon as this current task (probably pushing a block) gets around to yielding.
                    // This was changed from waiting on the old _revalidate_pending to prevent yielding
                    // during the middle of pushing a block.  If that happens, the database is in an
                    // inconsistent state and it confuses the p2p network code.
                    // We skip this step if we are dealing with blocks prior to the last checkpointed block
                    if (_head_block_header.block_num >= LAST_CHECKPOINT_BLOCK_NUM) {
                        if (!_revalidate_pending.valid() || _revalidate_pending.ready())
                            _revalidate_pending = fc::async([=]() {
                            revalidate_pending();
                        }, "revalidate_pending");
                    }
                }
                
                FC_CAPTURE_AND_RETHROW((block_data))
            }
            
            std::pair<BlockIdType, BlockForkData> ChainDatabaseImpl::recursive_mark_as_linked(const std::unordered_set<BlockIdType>& ids) {
                BlockForkData longest_fork;
                uint32_t highest_block_num = 0;
                BlockIdType last_block_id;
                std::unordered_set<BlockIdType> next_ids = ids;
                
                //while there are any next blocks for current block number being processed
                while (next_ids.size()) {
                    std::unordered_set<BlockIdType> pending; //builds list of all next blocks for the current block number being processed
                    
                    //mark as linked all blocks at the current block number being processed
                    for (const BlockIdType& next_id : next_ids) {
                        BlockForkData entry = _fork_db.fetch(next_id);
                        entry.is_linked = true;
                        pending.insert(entry.next_blocks.begin(), entry.next_blocks.end());
                        //ilog( "store: ${id} => ${data}", ("id",next_id)("data",entry) );
                        _fork_db.store(next_id, entry);
                        //keep one of the block ids of the current block number being processed (simplify this code)
                        const FullBlock& next_block = _block_id_to_full_block.fetch(next_id);
                        
                        if (next_block.block_num > highest_block_num) {
                            highest_block_num = next_block.block_num;
                            last_block_id = next_id;
                            longest_fork = entry;
                        }
                    }
                    
                    next_ids = pending; //conceptually this increments the current block number being processed
                }
                
                return std::make_pair(last_block_id, longest_fork);
            }
            void ChainDatabaseImpl::recursive_mark_as_invalid(const std::unordered_set<BlockIdType>& ids, const fc::exception& reason) {
                std::unordered_set<BlockIdType> next_ids = ids;
                
                while (next_ids.size()) {
                    std::unordered_set<BlockIdType> pending;
                    
                    for (const BlockIdType& next_id : next_ids) {
                        BlockForkData entry = _fork_db.fetch(next_id);
                        assert(!entry.valid()); //make sure we don't invalidate a previously validated entry
                        entry.is_valid = false;
                        entry.invalid_reason = reason;
                        pending.insert(entry.next_blocks.begin(), entry.next_blocks.end());
                        //ilog( "store: ${id} => ${data}", ("id",next_id)("data",entry) );
                        _fork_db.store(next_id, entry);
                    }
                    
                    next_ids = pending;
                }
            }
            
            /**
             *  Place the block in the block tree, the block tree contains all blocks
             *  and tracks whether they are valid, linked, and current.
             *
             *  There are several options for this block:
             *
             *  1) It extends an existing block
             *      - a valid chain
             *      - an invalid chain
             *      - an unlinked chain
             *  2) It is free floating and doesn't link to anything we have
             *      - create two entries into the database
             *          - one for this block
             *          - placeholder for previous
             *      - mark both as unlinked
             *  3) It provides the missing link between the genesis block and an existing chain
             *      - all next blocks need to be updated to change state to 'linked'
             *
             *  Returns the pair of the block id and block_fork_data of the block with the highest block number
             *  in the fork which contains the new block, in all of the above cases where the new block is linked;
             *  otherwise, returns the block id and fork data of the new block
             */
            std::pair<BlockIdType, BlockForkData> ChainDatabaseImpl::store_and_index(const BlockIdType& block_id,
                    const FullBlock& block_data) {
                try {
                    //we should never try to store a block we've already seen (verify not in any of our databases)
#ifdef _DEBUG
                    if (_block_id_to_full_block.fetch_optional(block_id)) {
                        FC_CAPTURE_AND_THROW(store_and_index_a_seen_block, ("store_and_index_a_seen_block"));
                    }
                    
#endif // DEBUG
#ifndef NDEBUG
                    {
                        //check block id is not in fork_data, or if it is, make sure it's just a placeholder for block we are waiting for
                        optional<BlockForkData> fork_data = _fork_db.fetch_optional(block_id);
                        assert(!fork_data || !fork_data->is_known);
                        //check block not in parallel_blocks database
                        vector<BlockIdType> parallel_blocks = fetch_blocks_at_number(block_data.block_num);
                        assert(std::find(parallel_blocks.begin(), parallel_blocks.end(), block_id) == parallel_blocks.end());
                    }
#endif
                    // first of all store this block at the given block number
                    _block_id_to_full_block.store(block_id, block_data);
                    
                    if (self->get_statistics_enabled()) {
                        BlockEntry entry;
                        DigestBlock& temp = entry;
                        temp = DigestBlock(block_data);
                        entry.id = block_id;
                        entry.block_size = block_data.block_size();
                        entry.latency = blockchain::now() - block_data.timestamp;
                        _block_id_to_block_entry_db.store(block_id, entry);
                    }
                    
                    // update the parallel block list (fork_number_db):
                    // get vector of all blocks with same block number, add this block to that list, then update the database
                    vector<BlockIdType> parallel_blocks = fetch_blocks_at_number(block_data.block_num);
                    parallel_blocks.push_back(block_id);
                    _fork_number_db.store(block_data.block_num, parallel_blocks);
                    // Tell our previous block that we are one of it's next blocks (update previous block's next_blocks set)
                    BlockForkData prev_fork_data;
                    auto prev_itr = _fork_db.find(block_data.previous);
                    
                    if (prev_itr.valid()) { // we already know about its previous (note: we always know about genesis block)
                        elog("           we already know about its previous: ${p}", ("p", block_data.previous));
                        prev_fork_data = prev_itr.value();
                        
                    } else { //if we don't know about the previous block even as a placeholder, create a placeholder for the previous block (placeholder block defaults as unlinked)
                        elog("           we don't know about its previous: ${p}", ("p", block_data.previous));
                        prev_fork_data.is_linked = false; //this is only a placeholder, we don't know what its previous block is, so it can't be linked
                    }
                    
                    prev_fork_data.next_blocks.insert(block_id);
                    _fork_db.store(block_data.previous, prev_fork_data);
                    auto cur_itr = _fork_db.find(block_id);
                    
                    if (cur_itr.valid()) { //if placeholder was previously created for block
                        BlockForkData current_fork = cur_itr.value();
                        current_fork.is_known = true; //was placeholder, now a known block
                        ilog("          current_fork: ${fork}", ("fork", current_fork));
                        ilog("          prev_fork: ${prev_fork}", ("prev_fork", prev_fork_data));
                        // if new block is linked to genesis block, recursively mark all its next blocks as linked and return longest descendant block
                        assert(!current_fork.is_linked);
                        
                        if (prev_fork_data.is_linked) {
                            current_fork.is_linked = true;
                            //if previous block is invalid, mark the new block as invalid too (block can't be valid if any previous block in its chain is invalid)
                            bool prev_block_is_invalid = prev_fork_data.is_valid && !*prev_fork_data.is_valid;
                            
                            if (prev_block_is_invalid) {
                                current_fork.is_valid = false;
                                current_fork.invalid_reason = prev_fork_data.invalid_reason;
                            }
                            
                            _fork_db.store(block_id, current_fork); //update placeholder fork_block entry with block data
                            
                            if (prev_block_is_invalid) { //if previous block was invalid, mark all descendants as invalid and return current_block
                                recursive_mark_as_invalid(current_fork.next_blocks, *prev_fork_data.invalid_reason);
                                return std::make_pair(block_id, current_fork);
                                
                            } else { //we have a potentially viable alternate chain, mark the descendant blocks as linked and return the longest end block from descendant chains
                                std::pair<BlockIdType, BlockForkData> longest_fork = recursive_mark_as_linked(current_fork.next_blocks);
                                return longest_fork;
                            }
                            
                        } else { //this new block is not linked to genesis block, so no point in determining its longest descendant block, just return it and let it be skipped over
                            _fork_db.store(block_id, current_fork); //update placeholder fork_block entry with block data
                            return std::make_pair(block_id, current_fork);
                        }
                        
                    } else { //no placeholder exists for this new block, just set its link flag
                        BlockForkData current_fork;
                        current_fork.is_known = true;
                        current_fork.is_linked = prev_fork_data.is_linked; //is linked if it's previous block is linked
                        bool prev_block_is_invalid = prev_fork_data.is_valid && !*prev_fork_data.is_valid;
                        
                        if (prev_block_is_invalid) {
                            current_fork.is_valid = false;
                            current_fork.invalid_reason = prev_fork_data.invalid_reason;
                        }
                        
                        //ilog( "          current_fork: ${id} = ${fork}", ("id",block_id)("fork",current_fork) );
                        _fork_db.store(block_id, current_fork); //add new fork_block entry to database
                        //this is first time we've seen this block mentioned, so we don't know about any linked descendants from it,
                        //and therefore this is the last block in this chain that we know about, so just return that
                        return std::make_pair(block_id, current_fork);
                    }
                }
                
                FC_CAPTURE_AND_RETHROW((block_id))
            }
            
            void ChainDatabaseImpl::mark_invalid(const BlockIdType& block_id, const fc::exception& reason) {
                // fetch the fork data for block_id, mark it as invalid and
                // then mark every item after it as invalid as well.
                auto fork_data = _fork_db.fetch(block_id);
                assert(!fork_data.valid()); //make sure we're not invalidating a block that we previously have validated
                fork_data.is_valid = false;
                fork_data.invalid_reason = reason;
                _fork_db.store(block_id, fork_data);
                recursive_mark_as_invalid(fork_data.next_blocks, reason);
            }
            
            void ChainDatabaseImpl::mark_as_unchecked(const BlockIdType& block_id) {
                // fetch the fork data for block_id, mark it as unchecked
                auto fork_data = _fork_db.fetch(block_id);
                assert(!fork_data.valid()); //make sure we're not unchecking a block that we previously have validated
                fork_data.is_valid.reset(); //mark as unchecked (i.e. we will check validity again later during switch_to_fork)
                fork_data.invalid_reason.reset();
                elog("store: ${id} => ${data}", ("id", block_id)("data", fork_data));
                _fork_db.store(block_id, fork_data);
                // then mark every block after it as unchecked as well.
                std::unordered_set<BlockIdType>& next_ids = fork_data.next_blocks;
                
                while (next_ids.size()) {
                    std::unordered_set<BlockIdType> pending_blocks_for_next_loop_iteration;
                    
                    for (const auto& next_block_id : next_ids) {
                        BlockForkData entry = _fork_db.fetch(next_block_id);
                        entry.is_valid.reset(); //mark as unchecked (i.e. we will check validity again later during switch_to_fork)
                        entry.invalid_reason.reset();
                        pending_blocks_for_next_loop_iteration.insert(entry.next_blocks.begin(), entry.next_blocks.end());
                        dlog("store: ${id} => ${data}", ("id", next_block_id)("data", entry));
                        _fork_db.store(next_block_id, entry);
                    }
                    
                    next_ids = pending_blocks_for_next_loop_iteration;
                }
            }
            
            void ChainDatabaseImpl::mark_included(const BlockIdType& block_id, bool included) {
                try {
                    //ilog( "included: ${block_id} = ${state}", ("block_id",block_id)("state",included) );
                    auto fork_data = _fork_db.fetch(block_id);
                    //if( fork_data.is_included != included )
                    {
                        fork_data.is_included = included;
                        
                        if (included) {
                            fork_data.is_valid = true;
                        }
                        
                        //ilog( "store: ${id} => ${data}", ("id",block_id)("data",fork_data) );
                        _fork_db.store(block_id, fork_data);
                    }
                    // fetch the fork data for block_id, mark it as included and
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("block_id", block_id)("included", included))
            }
            
            void ChainDatabaseImpl::switch_to_fork(const BlockIdType& block_id) {
                try {
                    if (block_id == _head_block_id) //if block_id is current head block, do nothing
                        return; //this is necessary to avoid unnecessarily popping the head block in this case
                        
                    ilog("switch from fork ${id} to ${to_id}", ("id", _head_block_id)("to_id", block_id));
                    vector<BlockIdType> history = get_fork_history(block_id);
                    
                    while (history.back() != _head_block_id) {
                        ilog("    pop ${id}", ("id", _head_block_id));
                        pop_block();
                    }
                    
                    for (int32_t i = history.size() - 2; i >= 0; --i) {
                        ilog("    extend ${i}", ("i", history[i]));
                        extend_chain(self->get_block(history[i]));
                    }
                }
                
                FC_CAPTURE_AND_RETHROW((block_id))
            }
            
            void ChainDatabaseImpl::apply_transactions(const FullBlock& block_data,
                    const PendingChainStatePtr& pending_state) {
                try {
                    uint32_t index = 0;
                    vector<std::future<bool>> signature_check_progress;
                    signature_check_progress.resize(block_data.user_transactions.size());
                    uint32_t trx_num = 0;
                    bool all_trx_check = true;
                    
                    //多线程测试代码
                    for (const auto& trx : block_data.user_transactions) {
                        TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pending_state.get());
                        trx_eval_state->_skip_signature_check = !self->_verify_transaction_signatures;
                        
                        if (!trx_eval_state->_skip_signature_check) {
                            std::vector<BalanceEntry> all_balances;
                            std::vector<AccountEntry> all_accounts;
                            trx_eval_state->transaction_entry_analy(trx, all_balances, all_accounts);
                            auto check_thread = [=]()->bool {
                                return trx_eval_state->transaction_signature_check(trx, all_balances, all_accounts);
                            };
                            signature_check_progress[trx_num] = _thread_pool.submit(check_thread);
                            trx_eval_state->_skip_signature_check = true;
                        }
                        
                        //end test
                        FC_ASSERT(trx.data_size() <= ALP_BLOCKCHAIN_MAX_TRX_SIZE, "trx size is out of the max trx size range");
                        trx_eval_state->skipexec = !(self->generating_block);
                        
                        if (trx_eval_state->skipexec)
                            trx_eval_state->skipexec = !self->get_node_vm_enabled();
                            
                        if (trx.result_trx_type == ResultTransactionType::incomplete_result_transaction)
                            trx_eval_state->skipexec = false;
                            
                        trx_eval_state->evaluate(trx);
                        
                        if (trx.result_trx_type == ResultTransactionType::origin_transaction) {
                            const TransactionIdType& trx_id = trx.id();
                            oTransactionEntry entry = pending_state->lookup<TransactionEntry>(trx_id);
                            FC_ASSERT(entry.valid(), "Invalid transaction");
                            entry->chain_location = TransactionLocation(block_data.block_num, trx_num);
                            pending_state->store_transaction(trx_id, *entry);
                            
                        } else if (trx.result_trx_type == ResultTransactionType::incomplete_result_transaction) {
                            const TransactionIdType& trx_id = trx.id();
                            oTransactionEntry entry = pending_state->lookup<TransactionEntry>(trx_id);
                            FC_ASSERT(entry.valid(), "Invalid transaction");
                            entry->chain_location = TransactionLocation(block_data.block_num, trx_num);
                            pending_state->store_transaction(trx_id, *entry);
                            const TransactionIdType& origin_trx_id = trx.operations[0].as<TransactionOperation>().trx.id();
                            oTransactionEntry origin_entry = pending_state->lookup<TransactionEntry>(origin_trx_id);
                            FC_ASSERT(origin_entry.valid(), "Invalid origin transaction");
                            origin_entry->chain_location = TransactionLocation(block_data.block_num, trx_num);
                            pending_state->store_transaction(origin_trx_id, *origin_entry);
                            const TransactionIdType& result_trx_id = trx.result_trx_id;
                            oTransactionEntry result_entry = pending_state->lookup<TransactionEntry>(result_trx_id);
                            FC_ASSERT(result_entry.valid(), "Invalid result transaction");
                            result_entry->chain_location = TransactionLocation(block_data.block_num, trx_num);
                            pending_state->store_transaction(result_trx_id, *result_entry);
                            
                        } else if (trx.result_trx_type == ResultTransactionType::complete_result_transaction) {
                            const TransactionIdType& trx_id = trx.id();
                            oTransactionEntry entry = pending_state->lookup<TransactionEntry>(trx_id);
                            FC_ASSERT(entry.valid(), "Invalid transaction");
                            entry->chain_location = TransactionLocation(block_data.block_num, trx_num);
                            pending_state->store_transaction(trx_id, *entry);
                            const TransactionIdType& origin_trx_id = trx.operations[0].as<TransactionOperation>().trx.id();
                            oTransactionEntry origin_entry = pending_state->lookup<TransactionEntry>(origin_trx_id);
                            FC_ASSERT(origin_entry.valid(), "Invalid origin transaction");
                            origin_entry->chain_location = TransactionLocation(block_data.block_num, trx_num);
                            pending_state->store_transaction(origin_trx_id, *origin_entry);
                            
                        } else {
                            ;
                        }
                        
                        pending_state->event_vector.insert(pending_state->event_vector.end(), trx_eval_state->event_vector.begin(), trx_eval_state->event_vector.end());
                        // TODO:  capture the evaluation state with a callback for wallets...
                        // summary.transaction_states.emplace_back( std::move(trx_eval_state) );
                        ++trx_num;
                    }
                    
                    for (auto& fut : signature_check_progress) {
                        try {
                            if (fut.valid())
                                if (!fut.get()) {
                                    all_trx_check = false;
                                }
                                
                        } catch (const fc::exception&) {
                        }
                    }
                    
                    signature_check_progress.clear();
                    
                    // test start
                    if (!all_trx_check) {
                        FC_CAPTURE_AND_THROW(missing_signature);
                    }
                    
                    //test end
                }
                
                FC_CAPTURE_AND_RETHROW((block_data))
            }
            
            void ChainDatabaseImpl::pay_delegate(const BlockIdType& block_id,
                                                 const PublicKeyType& block_signee,
                                                 const PendingChainStatePtr& pending_state,
                                                 oBlockEntry& entry)const {
                try {
                    // if( pending_state->get_head_block_num() < ALP_V0_6_0_FORK_BLOCK_NUM )
                    // return pay_delegate_v2( block_id, block_signee, pending_state, entry );
                    oAssetEntry base_asset_entry = pending_state->get_asset_entry(AssetIdType(0));
                    FC_ASSERT(base_asset_entry.valid(), "Invalid asset");
                    oAccountEntry delegate_entry = self->get_account_entry(Address(block_signee));
                    FC_ASSERT(delegate_entry.valid(), "Invalid account entry");
                    delegate_entry = pending_state->get_account_entry(delegate_entry->id);
                    FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate() && delegate_entry->delegate_info.valid(), "Invalid delegate");
                    const uint8_t pay_rate_percent = delegate_entry->delegate_info->pay_rate;
                    FC_ASSERT(pay_rate_percent >= 0 && pay_rate_percent <= 100, "Invalid pay_rate_percent");
                    const ShareType max_new_shares = self->get_max_delegate_pay_issued_per_block();
                    const ShareType accepted_new_shares = (max_new_shares * pay_rate_percent) / 100;
                    base_asset_entry->current_share_supply += accepted_new_shares;
                    static const uint32_t blocks_per_two_weeks = 14 * ALP_BLOCKCHAIN_BLOCKS_PER_DAY;
                    const ShareType max_collected_fees = base_asset_entry->collected_fees / blocks_per_two_weeks;
                    const ShareType accepted_collected_fees = (max_collected_fees * pay_rate_percent) / 100;
                    const ShareType destroyed_collected_fees = max_collected_fees - accepted_collected_fees;
                    FC_ASSERT(max_collected_fees >= 0 && accepted_collected_fees >= 0 && destroyed_collected_fees >= 0, "Invalid collected fee");
                    base_asset_entry->collected_fees -= max_collected_fees;
                    base_asset_entry->current_share_supply -= destroyed_collected_fees;
                    const ShareType accepted_paycheck = accepted_new_shares + accepted_collected_fees;
                    //const share_type accepted_paycheck = accepted_new_shares;
                    ChainDatabaseImpl * temp = (ChainDatabaseImpl *)this;
                    temp->_block_per_account_reword_amount = accepted_paycheck;
                    FC_ASSERT(accepted_paycheck >= 0);
                    delegate_entry->delegate_info->votes_for += accepted_paycheck;
                    delegate_entry->delegate_info->pay_balance += accepted_paycheck;
                    delegate_entry->delegate_info->total_paid += accepted_paycheck;
                    pending_state->store_account_entry(*delegate_entry);
                    pending_state->store_asset_entry(*base_asset_entry);
                    
                    if (entry.valid()) {
                        entry->signee_shares_issued = accepted_new_shares;
                        entry->signee_fees_collected = accepted_collected_fees;
                        entry->signee_fees_destroyed = destroyed_collected_fees;
                    }
                }
                
                FC_CAPTURE_AND_RETHROW((block_id)(block_signee)(entry))
            }
            
            void ChainDatabaseImpl::save_undo_state(const uint32_t block_num,
                                                    const BlockIdType& block_id,
                                                    const PendingChainStatePtr& pending_state) {
                try {
                    if (block_num < _min_undo_block)
                        return;
                        
                    PendingChainStatePtr undo_state = std::make_shared<PendingChainState>(pending_state);
                    pending_state->get_undo_state(undo_state);
                    
                    if (block_num > ALP_BLOCKCHAIN_MAX_UNDO_HISTORY) {
                        const uint32_t old_block_num = block_num - ALP_BLOCKCHAIN_MAX_UNDO_HISTORY;
                        const BlockIdType& old_block_id = self->get_block_id(old_block_num);
                        _block_id_to_undo_state.remove(old_block_id);
                    }
                    
                    _block_id_to_undo_state.store(block_id, *undo_state);
                }
                
                FC_CAPTURE_AND_RETHROW((block_num)(block_id))
            }
            
            void ChainDatabaseImpl::verify_header(const DigestBlock& block_digest, const PublicKeyType& block_signee)const {
                try {
                    if (block_digest.block_num > 1 && block_digest.block_num != _head_block_header.block_num + 1)
                        FC_CAPTURE_AND_THROW(block_numbers_not_sequential, (block_digest)(_head_block_header));
                        
                    if (block_digest.previous != _head_block_id)
                        FC_CAPTURE_AND_THROW(invalid_previous_block_id, (block_digest)(_head_block_id));
                        
                    if (block_digest.timestamp.sec_since_epoch() % ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC != 0)
                        FC_CAPTURE_AND_THROW(invalid_block_time);
                        
                    if (block_digest.block_num > 1 && block_digest.timestamp <= _head_block_header.timestamp)
                        FC_CAPTURE_AND_THROW(time_in_past, (block_digest.timestamp)(_head_block_header.timestamp));
                        
                    fc::time_point_sec now = thinkyoung::blockchain::now();
                    auto delta_seconds = (block_digest.timestamp - now).to_seconds();
                    
                    if (block_digest.timestamp > (now + ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 2))
                        FC_CAPTURE_AND_THROW(time_in_future, (block_digest.timestamp)(now)(delta_seconds));
                        
                    if (NOT block_digest.validate_digest())
                        FC_CAPTURE_AND_THROW(invalid_block_digest);
                        
                    FC_ASSERT(block_digest.validate_unique(), "Block not unique");
                    auto expected_delegate = self->get_slot_signee(block_digest.timestamp, self->get_active_delegates());
                    
                    if (block_signee != expected_delegate.signing_key())
                        FC_CAPTURE_AND_THROW(invalid_delegate_signee, (expected_delegate.id));
                }
                
                FC_CAPTURE_AND_RETHROW((block_digest)(block_signee))
            }
            
            void ChainDatabaseImpl::update_head_block(const SignedBlockHeader& block_header,
                    const BlockIdType& block_id) {
                try {
                    _head_block_header = block_header;
                    _head_block_id = block_id;
                }
                
                FC_CAPTURE_AND_RETHROW((block_header)(block_id))
            }
            
            /**
             *  A block should be produced every ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC. If we do not have a
             *  block for any multiple of this interval between produced_block and the current head block
             *  then we need to lookup the delegates that should have produced a block during that interval
             *  and increment their blocks_missed.
             *
             *  We also need to increment the blocks_produced for the delegate that actually produced the
             *  block.
             *
             *  Note that produced_block has already been verified by the caller and that updates are
             *  applied to pending_state.
             */
            void ChainDatabaseImpl::update_delegate_production_info(const BlockHeader& block_header,
                    const BlockIdType& block_id,
                    const PublicKeyType& block_signee,
                    const PendingChainStatePtr& pending_state)const {
                try {
                    /* Update production info for signing delegate */
                    AccountIdType delegate_id = self->get_delegate_entry_for_signee(block_signee).id;
                    oAccountEntry delegate_entry = pending_state->get_account_entry(delegate_id);
                    FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate(), "Invalid delegate Entry");
                    DelegateStats& delegate_info = *delegate_entry->delegate_info;
                    
                    /* Validate secret */
                    if (delegate_info.next_secret_hash.valid()) {
                        const SecretHashType hash_of_previous_secret = fc::ripemd160::hash(block_header.previous_secret);
                        FC_ASSERT(hash_of_previous_secret == *delegate_info.next_secret_hash,
                                  "",
                                  ("previous_secret", block_header.previous_secret)
                                  ("hash_of_previous_secret", hash_of_previous_secret)
                                  ("delegate_entry", delegate_entry));
                    }
                    
                    delegate_info.blocks_produced += 1;
                    delegate_info.next_secret_hash = block_header.next_secret_hash;
                    delegate_info.last_block_num_produced = block_header.block_num;
                    pending_state->store_account_entry(*delegate_entry);
                    
                    if (self->get_statistics_enabled()) {
                        const SlotEntry slot(block_header.timestamp, delegate_id, block_id);
                        pending_state->store_slot_entry(slot);
                    }
                    
                    /* Update production info for missing delegates */
                    uint64_t required_confirmations = self->get_required_confirmations();
                    time_point_sec block_timestamp;
                    auto head_block = self->get_head_block();
                    
                    if (head_block.block_num > 0) block_timestamp = head_block.timestamp + ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                    
                    else block_timestamp = block_header.timestamp;
                    
                    const auto& active_delegates = self->get_active_delegates();
                    
                    for (; block_timestamp < block_header.timestamp;
                            block_timestamp += ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC,
                            required_confirmations += 2) {
                        /* Note: Active delegate list has not been updated yet so we can use the timestamp */
                        delegate_id = self->get_slot_signee(block_timestamp, active_delegates).id;
                        delegate_entry = pending_state->get_account_entry(delegate_id);
                        FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate(), "Id got from slot is not a valid delegate id ");
                        delegate_entry->delegate_info->blocks_missed += 1;
                        pending_state->store_account_entry(*delegate_entry);
                        
                        if (self->get_statistics_enabled())
                            pending_state->store_slot_entry(SlotEntry(block_timestamp, delegate_id));
                    }
                    
                    /* Update required confirmation count */
                    required_confirmations -= 1;
                    
                    if (required_confirmations < 1) required_confirmations = 1;
                    
                    if (required_confirmations > ALP_BLOCKCHAIN_NUM_DELEGATES * 3)
                        required_confirmations = 3 * ALP_BLOCKCHAIN_NUM_DELEGATES;
                        
                    pending_state->set_required_confirmations(required_confirmations);
                }
                
                FC_CAPTURE_AND_RETHROW((block_header)(block_id)(block_signee))
            }
            
            void ChainDatabaseImpl::update_random_seed(const SecretHashType& new_secret,
                    const PendingChainStatePtr& pending_state,
                    oBlockEntry& entry)const {
                try {
                    const auto current_seed = pending_state->get_current_random_seed();
                    fc::sha512::encoder enc;
                    fc::raw::pack(enc, new_secret);
                    fc::raw::pack(enc, current_seed);
                    const auto& new_seed = fc::ripemd160::hash(enc.result());
                    pending_state->store_property_entry(PropertyIdType::last_random_seed_id, variant(new_seed));
                    
                    if (entry.valid()) entry->random_seed = new_seed;
                }
                
                FC_CAPTURE_AND_RETHROW((new_secret)(entry))
            }
            
            void ChainDatabaseImpl::update_active_delegate_list(const uint32_t block_num,
                    const PendingChainStatePtr& pending_state)const {
                try {//todo remove????
                    // if( pending_state->get_head_block_num() < ALP_V0_7_0_FORK_BLOCK_NUM )
                    // return update_active_delegate_list_v1( block_num, pending_state );
                    if (block_num % ALP_BLOCKCHAIN_NUM_DELEGATES != 0)
                        return;
                        
                    auto active_del = self->next_round_active_delegates();
                    const size_t num_del = active_del.size();
                    // Perform a random shuffle of the sorted delegate list.
                    fc::sha256 rand_seed = fc::sha256::hash(pending_state->get_current_random_seed());
                    
                    for (uint32_t i = 0, x = 0; i < num_del; i++) {
                        // we only use xth element of hash once,
                        // then when all 4 elements have been used,
                        // we re-mix the hash by running sha256() again
                        //
                        // the algorithm used is the second algorithm described in
                        // http://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#The_modern_algorithm
                        //
                        // previous implementation suffered from bias due to
                        // picking from all elements, see
                        // http://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#Implementation_errors
                        // in addition to various problems related to
                        // pre-increment operation
                        //
                        uint64_t r = rand_seed._hash[x];
                        uint32_t choices = num_del - i;
                        uint32_t j = ((uint32_t)(r % choices)) + i;
                        std::swap(active_del[i], active_del[j]);
                        x = (x + 1) & 3;
                        
                        if (x == 0)
                            rand_seed = fc::sha256::hash(rand_seed);
                    }
                    
                    pending_state->set_active_delegates(active_del);
                }
                
                FC_CAPTURE_AND_RETHROW((block_num))
            }
            
            /*
            * Query the emit's result of event from the contract.
            */
            vector<EventOperation> ChainDatabaseImpl::get_events(uint32_t block_index, const thinkyoung::blockchain::TransactionIdType& trx_id_type) {
                vector<EventOperation> ops;
                
                try {
                    FullBlock block_data = self->get_block(block_index);
                    
                    for (const auto& item : block_data.user_transactions) {
                        TransactionIdType trx_id = item.id();
                        
                        for (const auto& op : item.operations) {
                            if (thinkyoung::blockchain::event_op_type == op.type.value
                                    && trx_id == trx_id_type) {
                                EventOperation eop = op.as<EventOperation>();
                                ops.push_back(eop);
                            }
                        }
                    }
                    
                } catch (const fc::exception& e) {
                    wlog("error get_events block: ${e}", ("e", e.to_detail_string()));
                }
                
                return ops;
            }
            
            /**
             *  Performs all of the block validation steps and throws if error.
             */
            void ChainDatabaseImpl::extend_chain(const FullBlock& block_data) {
                try {
                    const time_point start_time = time_point::now();
                    const BlockIdType& block_id = block_data.id();
                    BlockSummary summary;
                    
                    try {
                        PublicKeyType block_signee;
                        
                        if (block_data.block_num > LAST_CHECKPOINT_BLOCK_NUM) {
                            block_signee = block_data.signee();
                            
                        } else {
                            const auto iter = CHECKPOINT_BLOCKS.find(block_data.block_num);
                            
                            if (iter != CHECKPOINT_BLOCKS.end() && iter->second != block_id)
                                FC_CAPTURE_AND_THROW(failed_checkpoint_verification, (block_id)(*iter));
                                
                            // Skip signature validation
                            block_signee = self->get_slot_signee(block_data.timestamp, self->get_active_delegates()).signing_key();
                        }
                        
                        // NOTE: Secret is validated later in update_delegate_production_info()
                        verify_header(DigestBlock(block_data), block_signee);
                        summary.block_data = block_data;
                        // Create a pending state to track changes that would apply as we evaluate the block
                        PendingChainStatePtr pending_state = std::make_shared<PendingChainState>(self->shared_from_this());
                        summary.applied_changes = pending_state;
                        /** Increment the blocks produced or missed for all delegates. This must be done
                         *  before applying transactions because it depends upon the current active delegate order.
                         **/
                        update_delegate_production_info(block_data, block_id, block_signee, pending_state);
                        oBlockEntry block_entry;
                        
                        if (self->get_statistics_enabled()) block_entry = self->get_block_entry(block_id);
                        
                        apply_transactions(block_data, pending_state);
                        summary.applied_changes->event_vector = pending_state->event_vector;
                        pay_delegate(block_id, block_signee, pending_state, block_entry);
                        update_active_delegate_list(block_data.block_num, pending_state);
                        update_random_seed(block_data.previous_secret, pending_state, block_entry);
                        save_undo_state(block_data.block_num, block_id, pending_state);
                        self->store_extend_status(block_id, 1);
                        // TODO: Verify idempotency
                        pending_state->apply_changes();
                        mark_included(block_id, true);
                        update_head_block(block_data, block_id);
                        clear_pending(block_data);
                        _block_num_to_id_db.store(block_data.block_num, block_id);
                        
                        if (block_entry.valid()) {
                            block_entry->processing_time = time_point::now() - start_time;
                            _block_id_to_block_entry_db.store(block_id, *block_entry);
                        }
                        
                        self->store_extend_status(block_id, 2);
                        
                        if (thinkyoung::client::g_client->get_wallet() != nullptr&&thinkyoung::client::g_client->get_wallet()->is_open()) {
                            if (!thinkyoung::client::g_client->get_wallet()->get_my_delegates(thinkyoung::wallet::enabled_delegate_status).empty()) {
                                auto fee = thinkyoung::client::g_client->get_delegate_config().transaction_min_fee;
                                _relay_fee = fee > ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE ? fee : ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;
                                
                            } else {
                                auto fee = thinkyoung::client::g_client->get_wallet()->get_transaction_fee();
                                _relay_fee = fee.amount > ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE ? fee.amount : ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;
                                //_relay_fee = ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;
                            }
                        }
                        
                    } catch (const fc::exception& e) {
                        wlog("error applying block: ${e}", ("e", e.to_detail_string()));
                        mark_invalid(block_id, e);
                        self->store_extend_status(block_id, 2);
                        throw;
                    }
                    
                    // Purge expired transactions from unique cache
                    auto iter = _unique_transactions.begin();
                    
                    while (iter != _unique_transactions.end() && iter->expiration <= self->now())
                        iter = _unique_transactions.erase(iter);
                        
                    //Schedule the observer notifications for later; the chain is in a
                    //non-premptable state right now, and observers may yield.
                    if ((now() - block_data.timestamp).to_seconds() < ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC)
                        for (ChainObserver* o : _observers)
                            fc::async([o, summary] {o->block_applied(summary); }, "call_block_applied_observer");
                }
                
                FC_CAPTURE_AND_RETHROW((block_data))
            }
            
            /**
             * Traverse the previous links of all blocks in fork until we find one that is_included
             *
             * The last item in the result will be the only block id that is already included in
             * the blockchain.
             */
            std::vector<BlockIdType> ChainDatabaseImpl::get_fork_history(const BlockIdType& id) {
                try {
                    std::vector<BlockIdType> history;
                    history.push_back(id);
                    BlockIdType next_id = id;
                    
                    while (true) {
                        auto header = self->get_block_header(next_id);
                        //ilog( "header: ${h}", ("h",header) );
                        history.push_back(header.previous);
                        
                        if (header.previous == BlockIdType()) {
                            ilog("return: ${h}", ("h", history));
                            return history;
                        }
                        
                        auto prev_fork_data = _fork_db.fetch(header.previous);
                        /// this shouldn't happen if the database invariants are properly maintained
                        FC_ASSERT(prev_fork_data.is_linked, "we hit a dead end, this fork isn't really linked!");
                        
                        if (prev_fork_data.is_included) {
                            ilog("return: ${h}", ("h", history));
                            return history;
                        }
                        
                        next_id = header.previous;
                    }
                    
                    ilog("${h}", ("h", history));
                    return history;
                }
                
                FC_CAPTURE_AND_RETHROW((id))
            }
            
            void ChainDatabaseImpl::pop_block() {
                try {
                    assert(_head_block_header.block_num != 0);
                    
                    if (_head_block_header.block_num == 0) {
                        elog("attempting to pop block 0");
                        return;
                    }
                    
                    // update the is_included flag on the fork data
                    mark_included(_head_block_id, false);
                    // update the block_num_to_block_id index
                    _block_num_to_id_db.remove(_head_block_header.block_num);
                    auto previous_block_id = _head_block_header.previous;
                    const auto undo_iter = _block_id_to_undo_state.unordered_find(_head_block_id);
                    FC_ASSERT(undo_iter != _block_id_to_undo_state.unordered_end(), "_block_id_to_undo_state is empty");
                    const auto& undo_state = undo_iter->second;
                    thinkyoung::blockchain::PendingChainStatePtr undo_state_ptr = std::make_shared<thinkyoung::blockchain::PendingChainState>(undo_state);
                    undo_state_ptr->set_prev_state(self->shared_from_this());
                    undo_state_ptr->apply_changes();
                    _head_block_id = previous_block_id;
                    
                    if (_head_block_id == BlockIdType())
                        _head_block_header = SignedBlockHeader();
                        
                    else
                        _head_block_header = self->get_block_header(_head_block_id);
                        
                    //Schedule the observer notifications for later; the chain is in a
                    //non-premptable state right now, and observers may yield.
                    for (ChainObserver* o : _observers)
                        fc::async([o, undo_state_ptr] { o->state_changed(undo_state_ptr); }, "call_state_changed_observer");
                }
                
                FC_CAPTURE_AND_RETHROW()
            }
            
            void ChainDatabaseImpl::repair_block(BlockIdType block_id) {
                try {
                    auto full_block = self->get_block(block_id);
                    mark_included(block_id, false);
                    _block_num_to_id_db.remove(full_block.block_num);
                    auto previous_block_id = full_block.previous;
                    const auto undo_iter = _block_id_to_undo_state.unordered_find(block_id);
                    FC_ASSERT(undo_iter != _block_id_to_undo_state.unordered_end(), "_block_id_to_undo_state is empty");
                    const auto& undo_state = undo_iter->second;
                    thinkyoung::blockchain::PendingChainStatePtr undo_state_ptr = std::make_shared<thinkyoung::blockchain::PendingChainState>(undo_state);
                    undo_state_ptr->set_prev_state(self->shared_from_this());
                    undo_state_ptr->apply_changes();
                    _head_block_id = previous_block_id;
                    
                    if (_head_block_id == BlockIdType())
                        _head_block_header = SignedBlockHeader();
                        
                    else
                        _head_block_header = self->get_block_header(_head_block_id);
                        
                    for (ChainObserver* o : _observers)
                        fc::async([o, undo_state_ptr] { o->state_changed(undo_state_ptr); }, "call_state_changed_observer");
                }
                
                FC_CAPTURE_AND_RETHROW()
            }
        } // namespace detail
        
        ChainDatabase::ChainDatabase()
            :my(new detail::ChainDatabaseImpl()) {
            my->self = this;
            generating_block = false;
        }
        
        ChainDatabase::~ChainDatabase() {
            try {
                close();
                
            } catch (const fc::exception& e) {
                elog("unexpected exception closing database\n ${e}", ("e", e.to_detail_string()));
            }
        }
        
        std::vector<AccountIdType> ChainDatabase::next_round_active_delegates()const {
            return get_delegates_by_vote(0, ALP_BLOCKCHAIN_NUM_DELEGATES);
        }
        
        std::vector<AccountIdType> ChainDatabase::get_delegates_by_vote(uint32_t first, uint32_t count)const {
            try {
                auto del_vote_itr = my->_delegate_votes.begin();
                std::vector<AccountIdType> sorted_delegates;
                
                if (count > my->_delegate_votes.size())
                    count = my->_delegate_votes.size();
                    
                sorted_delegates.reserve(count);
                uint32_t pos = 0;
                
                while (sorted_delegates.size() < count && del_vote_itr != my->_delegate_votes.end()) {
                    if (pos >= first)
                        sorted_delegates.push_back(del_vote_itr->delegate_id);
                        
                    ++pos;
                    ++del_vote_itr;
                }
                
                return sorted_delegates;
            }
            
            FC_RETHROW_EXCEPTIONS(warn, "")
        }
        
        std::vector<AccountIdType> ChainDatabase::get_all_delegates_by_vote()const {
            try {
                auto del_vote_itr = my->_delegate_votes.begin();
                std::vector<AccountIdType> sorted_delegates;
                sorted_delegates.reserve(my->_delegate_votes.size());
                uint32_t pos = 0;
                
                while (del_vote_itr != my->_delegate_votes.end()) {
                    sorted_delegates.push_back(del_vote_itr->delegate_id);
                    ++pos;
                    ++del_vote_itr;
                }
                
                return sorted_delegates;
            }
            
            FC_RETHROW_EXCEPTIONS(warn, "")
        }
        
        
        void ChainDatabase::open(const fc::path& data_dir, const fc::optional<fc::path>& genesis_file, const bool statistics_enabled,
                                 const std::function<void(float)> replay_status_callback) {
            try {
                std::exception_ptr error_opening_database;
                
                try {
                    //This function will yield the first time it is called. Do that now, before calling push_block
                    now();
                    my->load_checkpoints(data_dir.parent_path());
                    
                    if (!my->replay_required(data_dir)) {
                        my->open_database(data_dir);
                        uint32_t head_block_num = 0;
                        BlockIdType head_block_id;
                        my->_block_num_to_id_db.last(head_block_num, head_block_id);
                        
                        if (head_block_num > 0) {
                            my->_head_block_id = head_block_id;;
                            my->_head_block_header = get_block_header(head_block_id);
                        }
                        
                        my->populate_indexes();
                        
                    } else {
                        wlog("Database inconsistency detected; erasing state and attempting to replay blockchain");
                        fc::remove_all(data_dir / "index");
                        
                        if (fc::is_directory(data_dir / "raw_chain/block_id_to_block_data_db")) {
                            if (!fc::is_directory(data_dir / "raw_chain/block_id_to_data_original"))
                                fc::rename(data_dir / "raw_chain/block_id_to_block_data_db", data_dir / "raw_chain/block_id_to_data_original");
                        }
                        
                        // During replay we implement stop-and-copy garbage collection on the raw blocks
                        decltype(my->_block_id_to_full_block) block_id_to_data_original;
                        block_id_to_data_original.open(data_dir / "raw_chain/block_id_to_data_original");
                        const size_t original_size = fc::directory_size(data_dir / "raw_chain/block_id_to_data_original");
                        my->open_database(data_dir);
                        store_property_entry(PropertyIdType::database_version, variant(ALP_BLOCKCHAIN_DATABASE_VERSION));
                        const auto toggle_leveldb = [this](const bool enabled) {
                            my->_block_id_to_undo_state.toggle_leveldb(enabled);
                            my->_property_id_to_entry.toggle_leveldb(enabled);
                            my->_account_id_to_entry.toggle_leveldb(enabled);
                            my->_account_name_to_id.toggle_leveldb(enabled);
                            my->_account_address_to_id.toggle_leveldb(enabled);
                            //contract db related
                            my->_contract_id_to_entry.toggle_leveldb(enabled);
                            my->_contract_id_to_storage.toggle_leveldb(enabled);
                            my->_contract_name_to_id.toggle_leveldb(enabled);
                            my->_result_to_request_iddb.toggle_leveldb(enabled);
                            my->_asset_id_to_entry.toggle_leveldb(enabled);
                            my->_asset_symbol_to_id.toggle_leveldb(enabled);
                            my->_slate_id_to_entry.toggle_leveldb(enabled);
                            my->_balance_id_to_entry.toggle_leveldb(enabled);
                            my->_request_to_result_iddb.toggle_leveldb(enabled);
                            my->_trx_to_contract_iddb.toggle_leveldb(enabled);
                            my->_contract_to_trx_iddb.toggle_leveldb(enabled);
                        };
                        // For the duration of replaying, we allow certain databases to postpone flushing until we finish
                        toggle_leveldb(false);
                        my->initialize_genesis(genesis_file, statistics_enabled);
                        // Load block num -> id db into memory and clear from disk for replaying
                        map<uint32_t, BlockIdType> num_to_id;
                        {
                            for (auto itr = my->_block_num_to_id_db.begin(); itr.valid(); ++itr)
                                num_to_id.emplace_hint(num_to_id.end(), itr.key(), itr.value());
                                
                            my->_block_num_to_id_db.close();
                            fc::remove_all(data_dir / "raw_chain/block_num_to_id_db");
                            my->_block_num_to_id_db.open(data_dir / "raw_chain/block_num_to_id_db");
                        }
                        
                        if (!replay_status_callback) {
                            std::cout << "Please be patient, this will take several minutes...\r\nReplaying blockchain..."
                                      << std::flush << std::fixed;
                                      
                        } else {
                            replay_status_callback(0);
                        }
                        
                        uint32_t blocks_indexed = 0;
                        const auto total_blocks = num_to_id.size();
                        const auto genesis_time = get_genesis_timestamp();
                        const auto start_time = blockchain::now();
                        const auto insert_block = [&](const FullBlock& block) {
                            if (blocks_indexed % 200 == 0) {
                                float progress;
                                
                                if (total_blocks) {
                                    progress = float(blocks_indexed) / total_blocks;
                                    
                                } else {
                                    const auto seconds = (start_time - genesis_time).to_seconds();
                                    progress = float(blocks_indexed * ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC) / seconds;
                                }
                                
                                progress *= 100;
                                
                                if (!replay_status_callback) {
                                    std::cout << "\rReplaying blockchain... "
                                              "Approximately " << std::setprecision(2) << progress << "% complete." << std::flush;
                                              
                                } else {
                                    replay_status_callback(progress);
                                }
                            }
                            
                            push_block(block);
                            ++blocks_indexed;
                            //if( blocks_indexed % 1000 == 0 )
                            //{
                            //set_db_cache_write_through( true );
                            //set_db_cache_write_through( false );
                            //}
                        };
                        
                        try {
                            if (num_to_id.empty()) {
                                for (auto block_itr = block_id_to_data_original.begin(); block_itr.valid(); ++block_itr)
                                    insert_block(block_itr.value());
                                    
                            } else {
                                const uint32_t last_known_block_num = num_to_id.crbegin()->first;
                                
                                if (last_known_block_num > ALP_BLOCKCHAIN_MAX_UNDO_HISTORY)
                                    my->_min_undo_block = last_known_block_num - ALP_BLOCKCHAIN_MAX_UNDO_HISTORY;
                                    
                                for (const auto& num_id : num_to_id) {
                                    const auto oblock = block_id_to_data_original.fetch_optional(num_id.second);
                                    
                                    if (oblock.valid()) insert_block(*oblock);
                                }
                            }
                            
                        } catch (thinkyoung::blockchain::store_and_index_a_seen_block&  e) {
                            block_id_to_data_original.close();
                            fc::remove_all(data_dir / "raw_chain/block_id_to_data_original");
                            fc::remove_all(data_dir / "index");
                            exit(0);
                        }
                        
                        // Re-enable flushing on all cached databases we disabled it on above
                        toggle_leveldb(true);
                        block_id_to_data_original.close();
                        fc::remove_all(data_dir / "raw_chain/block_id_to_data_original");
                        const size_t final_size = fc::directory_size(data_dir / "raw_chain/block_id_to_block_data_db");
                        std::cout << "\rSuccessfully replayed " << blocks_indexed << " blocks in "
                                  << (blockchain::now() - start_time).to_seconds() << " seconds.                          "
                                  "\nBlockchain size changed from "
                                  << original_size / 1024 / 1024 << "MiB to "
                                  << final_size / 1024 / 1024 << "MiB.\n" << std::flush;
                    }
                    
                    // Process the pending transactions to cache by fees
                    for (auto pending_itr = my->_pending_transaction_db.begin(); pending_itr.valid(); ++pending_itr) {
                        try {
                            const auto trx = pending_itr.value();
                            const auto trx_id = trx.id();
                            const auto eval_state = evaluate_transaction(trx, my->_relay_fee);
                            ShareType fees = eval_state->get_fees();
                            my->_pending_fee_index[fee_index(fees, trx_id)] = eval_state;
                            my->_pending_transaction_db.store(trx_id, trx);
                            
                        } catch (const fc::exception& e) {
                            wlog("Error processing pending transaction: ${e}", ("e", e.to_detail_string()));
                        }
                    }
                    
                } catch (...) {
                    error_opening_database = std::current_exception();
                }
                
                if (error_opening_database) {
                    elog("Error opening database!");
                    close();
                    fc::remove_all(data_dir / "index");
                    std::rethrow_exception(error_opening_database);
                }
            }
            
            FC_CAPTURE_AND_RETHROW((data_dir))
        }
        
        void ChainDatabase::close() {
            try {
                my->_pending_transaction_db.close();
                my->_block_id_to_full_block.close();
                my->_block_id_to_undo_state.close();
                my->_fork_number_db.close();
                my->_fork_db.close();
                my->_revalidatable_future_blocks_db.close();
                my->_block_num_to_id_db.close();
                my->_block_id_to_block_entry_db.close();
                my->_property_id_to_entry.close();
                my->_account_id_to_entry.close();
                my->_account_name_to_id.close();
                my->_account_address_to_id.close();
                //contract db
                my->_contract_id_to_entry.close();
                my->_result_to_request_iddb.close();
                my->_contract_name_to_id.close();
                my->_contract_id_to_storage.close();
                my->_asset_id_to_entry.close();
                my->_asset_symbol_to_id.close();
                my->_slate_id_to_entry.close();
                my->_balance_id_to_entry.close();
                my->_transaction_id_to_entry.close();
                my->_address_to_transaction_ids.close();
                my->_alp_input_balance_entry.close();
                my->_alp_full_entry.close();
                my->_block_extend_status.close();
                my->_slot_index_to_entry.close();
                my->_slot_timestamp_to_delegate.close();
                my->_request_to_result_iddb.close();
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        AccountEntry ChainDatabase::get_delegate_entry_for_signee(const PublicKeyType& block_signee)const {
            auto delegate_entry = get_account_entry(Address(block_signee));
            FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate(), "Invalid delegate!");
            return *delegate_entry;
        }
        
        AccountEntry ChainDatabase::get_block_signee(const BlockIdType& block_id)const {
            auto block_header = get_block_header(block_id);
            auto delegate_entry = get_account_entry(Address(block_header.signee()));
            FC_ASSERT(delegate_entry.valid() && delegate_entry->is_delegate(), "Invalid delegate!");
            return *delegate_entry;
        }
        
        AccountEntry ChainDatabase::get_block_signee(uint32_t block_num)const {
            return get_block_signee(get_block_id(block_num));
        }
        
        AccountEntry ChainDatabase::get_slot_signee(const time_point_sec timestamp,
                const std::vector<AccountIdType>& ordered_delegates)const {
            try {
                auto slot_number = blockchain::get_slot_number(timestamp);
                auto delegate_pos = slot_number % ALP_BLOCKCHAIN_NUM_DELEGATES;
                FC_ASSERT(delegate_pos < ordered_delegates.size(), "Delegate pos should be smaller than number of delegates");
                auto delegate_id = ordered_delegates[delegate_pos];
                auto delegate_entry = get_account_entry(delegate_id);
                FC_ASSERT(delegate_entry.valid(), "Invalid account entry");
                FC_ASSERT(delegate_entry->is_delegate(), "Not a delegate ");
                return *delegate_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((timestamp)(ordered_delegates))
        }
        
        optional<time_point_sec> ChainDatabase::get_next_producible_block_timestamp(const vector<AccountIdType>& delegate_ids)const {
            try {
                auto next_block_time = blockchain::get_slot_start_time(blockchain::now());
                
                if (next_block_time <= now()) next_block_time += ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                
                auto last_block_time = next_block_time + (ALP_BLOCKCHAIN_NUM_DELEGATES * ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
                auto active_delegates = get_active_delegates();
                
                for (; next_block_time < last_block_time; next_block_time += ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC) {
                    auto slot_number = blockchain::get_slot_number(next_block_time);
                    auto delegate_pos = slot_number % ALP_BLOCKCHAIN_NUM_DELEGATES;
                    FC_ASSERT(delegate_pos < active_delegates.size(), "Delegate pos should be smaller than number of delegates");
                    auto delegate_id = active_delegates[delegate_pos];
                    
                    if (std::find(delegate_ids.begin(), delegate_ids.end(), delegate_id) != delegate_ids.end())
                        return next_block_time;
                }
                
                return optional<time_point_sec>();
            }
            
            FC_CAPTURE_AND_RETHROW((delegate_ids))
        }
        
        TransactionEvaluationStatePtr ChainDatabase::evaluate_transaction(const SignedTransaction& trx,
                const ShareType required_fees, bool contract_vm_exec, bool skip_signature_check, bool throw_exec_exception) {
            try {
                if (!my->_pending_trx_state)
                    my->_pending_trx_state = std::make_shared<PendingChainState>(shared_from_this());
                    
                PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(my->_pending_trx_state);
                PendingChainStatePtr          pend_state_res = std::make_shared<PendingChainState>(my->_pending_trx_state);
                TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
                
                if (skip_signature_check)
                    trx_eval_state->_skip_signature_check = skip_signature_check;
                    
                trx_eval_state->skipexec = !generating_block; //to check ! evaluate_transaction,基本只在store pending的时候被调用，此时如果是代理就需要执行代码，不是则不用执行
                
                //如果普通节点打开了VM开关
                if (trx_eval_state->skipexec)
                    trx_eval_state->skipexec = !get_node_vm_enabled();
                    
                trx_eval_state->throw_exec_exception = throw_exec_exception;
                
                //钱包创建交易首次本地验证，执行一次解释器
                if (trx_eval_state->skipexec && contract_vm_exec)
                    trx_eval_state->skipexec = !contract_vm_exec;
                    
                if (trx_eval_state->origin_trx_basic_verify(trx) == false)
                    FC_CAPTURE_AND_THROW(illegal_transaction, (trx));
                    
                bool no_check_required_fee = false;
                ShareType fees = 0;
                
                try {
                    trx_eval_state->evaluate(trx, false);
                    
                } catch (thinkyoung::blockchain::ignore_check_required_fee_state& e) {
                    no_check_required_fee = true;
                }
                
                //如果有结果交易，就不将原始请求在此处写入DB了，原始请求会在evaluate结果交易时被保存
                if (trx_eval_state->p_result_trx.operations.size() > 0)
                    trx_eval_state->_current_state->remove<TransactionEntry>(trx.id());
                    
                fees = trx_eval_state->get_fees() + trx_eval_state->alt_fees_paid.amount;
                
                if (!no_check_required_fee && (fees < required_fees)) {
                    ilog("Transaction ${id} needed relay fee ${required_fees} but only had ${fees}", ("id", trx.id())("required_fees", required_fees)("fees", fees));
                    FC_CAPTURE_AND_THROW(insufficient_relay_fee, (fees)(required_fees));
                }
                
                // apply changes from this transaction to _pending_trx_state
                //在store_pending的过程中如果没有结果交易，就直接保存,(结果交易evaluate首个交易op后产生的result在交易evalute过程中被移除)
                //有结果交易则，取消evaluate方法中缓存的改动(在有结果交易时，原始请求并不需要修改缓存)
                if (trx_eval_state->p_result_trx.operations.size() < 1) {
                    pend_state->apply_changes();
                    
                } else {
                    //验证下结果交易是否可行
                    TransactionEvaluationStatePtr trx_eval_state_res = std::make_shared<TransactionEvaluationState>(pend_state_res.get());
                    //  trx_eval_state_res->skipexec = !generating_block;
                    //  if (trx_eval_state_res->skipexec)
                    //      trx_eval_state_res->skipexec = !get_node_vm_enabled();
                    //
                    //  //钱包创建交易首次本地验证，执行一次解释器
                    //  if (trx_eval_state_res->skipexec && contract_vm_exec)
                    //      trx_eval_state_res->skipexec = !contract_vm_exec;
                    trx_eval_state_res->skipexec = true;
                    trx_eval_state->throw_exec_exception = throw_exec_exception;
                    trx_eval_state_res->evaluate(trx_eval_state->p_result_trx);
                    pend_state_res->apply_changes();
                    TransactionEvaluationStatePtr eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
                    eval_state->p_result_trx = trx_eval_state->p_result_trx;
                    eval_state->trx = trx;
                    return eval_state;
                }
                
                return trx_eval_state;
            }
            
            FC_CAPTURE_AND_RETHROW((trx))
        }
        
        optional<fc::exception> ChainDatabase::get_transaction_error(const SignedTransaction& transaction, const ShareType min_fee) {
            try {
                try {
                    auto pending_state = std::make_shared<PendingChainState>(shared_from_this());
                    TransactionEvaluationStatePtr eval_state = std::make_shared<TransactionEvaluationState>(pending_state.get());
                    eval_state->evaluate(transaction);
                    const ShareType fees = eval_state->get_fees() + eval_state->alt_fees_paid.amount;
                    
                    if (fees < min_fee)
                        FC_CAPTURE_AND_THROW(insufficient_relay_fee, (fees)(min_fee));
                        
                } catch (const fc::canceled_exception&) {
                    throw;
                    
                } catch (const fc::exception& e) {
                    return e;
                }
                
                return optional<fc::exception>();
            }
            
            FC_CAPTURE_AND_RETHROW((transaction))
        }
        
        SignedBlockHeader ChainDatabase::get_block_header(const BlockIdType& block_id)const {
            try {
                return get_block(block_id);
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        SignedBlockHeader ChainDatabase::get_block_header(uint32_t block_num)const {
            try {
                return get_block_header(get_block_id(block_num));
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        
        oBlockEntry ChainDatabase::get_block_entry(const BlockIdType& block_id) const {
            try {
                oBlockEntry entry = my->_block_id_to_block_entry_db.fetch_optional(block_id);
                
                if (!entry.valid()) {
                    try {
                        const FullBlock block_data = get_block(block_id);
                        entry = BlockEntry();
                        DigestBlock& temp = *entry;
                        temp = DigestBlock(block_data);
                        entry->id = block_id;
                        entry->block_size = block_data.block_size();
                        
                    } catch (const fc::exception&) {
                    }
                }
                
                return entry;
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        oBlockEntry ChainDatabase::get_block_entry(uint32_t block_num) const {
            try {
                return get_block_entry(get_block_id(block_num));
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        
        BlockIdType ChainDatabase::get_block_id(uint32_t block_num) const {
            try {
                return my->_block_num_to_id_db.fetch(block_num);
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        
        vector<TransactionEntry> ChainDatabase::get_transactions_for_block(const BlockIdType& block_id)const {
            try {
                const FullBlock block = get_block(block_id);
                vector<TransactionEntry> entrys;
                entrys.reserve(block.user_transactions.size());
                
                for (const SignedTransaction& transaction : block.user_transactions)
                    entrys.push_back(my->_transaction_id_to_entry.fetch(transaction.id()));
                    
                return entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        DigestBlock ChainDatabase::get_block_digest(const BlockIdType& block_id)const {
            try {
                return DigestBlock(get_block(block_id));
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        DigestBlock ChainDatabase::get_block_digest(uint32_t block_num)const {
            try {
                return get_block_digest(get_block_id(block_num));
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        
        FullBlock ChainDatabase::get_block(const BlockIdType& block_id)const {
            try {
                return my->_block_id_to_full_block.fetch(block_id);
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        FullBlock ChainDatabase::get_block(uint32_t block_num)const {
            try {
                return get_block(get_block_id(block_num));
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        
        SignedBlockHeader ChainDatabase::get_head_block()const {
            try {
                return my->_head_block_header;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        uint32_t ChainDatabase::find_block_num(fc::time_point_sec &time)const {
            try {
                uint32_t start = 1, end = get_head_block_num();
                auto start_block_time = get_block_header(start).timestamp;
                
                if (start_block_time >= time) {
                    return start;
                }
                
                auto end_block_time = get_block_header(end).timestamp;
                
                if (end_block_time <= time) {
                    return end;
                }
                
                while (end > start + 1) {
                    double relative = double(time.sec_since_epoch() - start_block_time.sec_since_epoch())
                                      / (end_block_time.sec_since_epoch() - start_block_time.sec_since_epoch());
                    uint32_t mid = start + ((end - start) * relative);
                    
                    if (mid == start) {
                        mid++;
                    }
                    
                    auto mid_block_time = get_block_header(mid).timestamp;
                    
                    if (mid_block_time > time) {
                        end = mid;
                        end_block_time = mid_block_time;
                        
                    } else {
                        start = mid;
                        start_block_time = mid_block_time;
                    }
                }
                
                return start;
            }
            
            FC_CAPTURE_AND_RETHROW((time))
        }
        
        /**
         *  Adds the block to the database and manages any reorganizations as a result.
         *
         *  Returns the block_fork_data of the new block, not necessarily the head block
         */
        BlockForkData ChainDatabase::push_block(const FullBlock& block_data) {
            try {
                uint32_t head_block_num = get_head_block_num();
                
                if (head_block_num > ALP_BLOCKCHAIN_MAX_UNDO_HISTORY &&
                        block_data.block_num <= (head_block_num - ALP_BLOCKCHAIN_MAX_UNDO_HISTORY)) {
                    elog("block ${new_block_hash} (number ${new_block_num}) is on a fork older than "
                         "our undo history would allow us to switch to (current head block is number ${head_block_num}, undo history is ${undo_history})",
                         ("new_block_hash", block_data.id())("new_block_num", block_data.block_num)
                         ("head_block_num", head_block_num)("undo_history", ALP_BLOCKCHAIN_MAX_UNDO_HISTORY));
                    FC_THROW_EXCEPTION(block_older_than_undo_history,
                                       "block ${new_block_hash} (number ${new_block_num}) is on a fork older than "
                                       "our undo history would allow us to switch to (current head block is number ${head_block_num}, undo history is ${undo_history})",
                                       ("new_block_hash", block_data.id())("new_block_num", block_data.block_num)
                                       ("head_block_num", head_block_num)("undo_history", ALP_BLOCKCHAIN_MAX_UNDO_HISTORY));
                }
                
                // only allow a single fiber attempt to push blocks at any given time,
                // this method is not re-entrant.
                fc::unique_lock<fc::mutex> lock(my->_push_block_mutex);
                // The above check probably isn't enough.  We need to make certain that
                // no other code sees the chain_database in an inconsistent state.
                // The lock above prevents two push_blocks from happening at the same time,
                // but we also need to ensure the wallet, blockchain, delegate, &c. loops don't
                // see partially-applied blocks
                ASSERT_TASK_NOT_PREEMPTED();
                const BlockIdType& block_id = block_data.id();
                optional<BlockForkData> fork_data = get_block_fork_data(block_id);
                
                if (fork_data.valid() && fork_data->is_known) return *fork_data;
                
                std::pair<BlockIdType, BlockForkData> longest_fork = my->store_and_index(block_id, block_data);
                assert(get_block_fork_data(block_id) && "can't get fork data for a block we just successfully pushed");
                
                /*
                store_and_index has returned the potential chain with the longest_fork (highest block number other than possible the current head block number)
                if (longest_fork is linked and not known to be invalid and is higher than the current head block number)
                highest_unchecked_block_number = longest_fork blocknumber;
                do
                foreach next_fork_to_try in all blocks at same block number
                if (next_fork_try is linked and not known to be invalid)
                try
                switch_to_fork(next_fork_to_try) //this throws if block in fork is invalid, then we'll try another fork
                return
                catch block from future and add to database for potential revalidation on startup or if we get from another peer later
                catch any other invalid block and do nothing
                --highest_unchecked_block_number
                while(highest_unchecked_block_number > 0)
                */
                if (longest_fork.second.can_link()) {
                    FullBlock longest_fork_block = my->_block_id_to_full_block.fetch(longest_fork.first);
                    uint32_t highest_unchecked_block_number = longest_fork_block.block_num;
                    
                    if (highest_unchecked_block_number > head_block_num) {
                        do {
                            optional<vector<BlockIdType>> parallel_blocks = my->_fork_number_db.fetch_optional(highest_unchecked_block_number);
                            
                            if (parallel_blocks)
                            
                                //for all blocks at same block number
                                for (const BlockIdType& next_fork_to_try_id : *parallel_blocks) {
                                    BlockForkData next_fork_to_try = my->_fork_db.fetch(next_fork_to_try_id);
                                    
                                    if (next_fork_to_try.can_link())
                                        try {
                                            my->switch_to_fork(next_fork_to_try_id); //verify this works if next_fork_to_try is current head block
                                            return *get_block_fork_data(block_id);
                                            
                                        } catch (const time_in_future&) {
                                            // Blocks from the future can become valid later, so keep a list of these blocks that we can iterate over
                                            // whenever we think our clock time has changed from it's standard flow
                                            my->_revalidatable_future_blocks_db.store(block_id, 0);
                                            wlog("fork rejected because it has block with time in future, storing block id for revalidation later");
                                            
                                        } catch (const fc::exception& e) { //swallow any invalidation exceptions except for time_in_future invalidations
                                            wlog("fork permanently rejected as it has permanently invalid block: ${x}", ("x", e.to_detail_string()));
                                        }
                                }
                                
                            --highest_unchecked_block_number;
                        } while (highest_unchecked_block_number > 0); // while condition should only fail if we've never received a valid block yet
                    } //end if fork is longer than current chain (including possibly by extending chain)
                    
                } else {
                    elog("unable to link longest fork ${f}", ("f", longest_fork));
                    blockchain::ntp_time();
                    //       blockchain::update_ntp_time();
                    my->clear_invalidation_of_future_blocks();
                }
                
                return *get_block_fork_data(block_id);
            }
            
            FC_CAPTURE_AND_RETHROW((block_data))
        }
        
        std::vector<BlockIdType> ChainDatabase::get_fork_history(const BlockIdType& id) {
            return my->get_fork_history(id);
        }
        
        /** return the timestamp from the head block */
        fc::time_point_sec ChainDatabase::now()const {
            if (my->_head_block_header.block_num <= 0) { /* Genesis */
                auto slot_number = blockchain::get_slot_number(blockchain::now());
                return blockchain::get_slot_start_time(slot_number - 1);
            }
            
            return my->_head_block_header.timestamp;
        }
        
        AssetIdType ChainDatabase::get_asset_id(const string& symbol)const {
            try {
                auto arec = get_asset_entry(symbol);
                FC_ASSERT(arec.valid(), "Invalid asset entry");
                return arec->id;
            }
            
            FC_CAPTURE_AND_RETHROW((symbol))
        }
        
        bool ChainDatabase::is_valid_symbol(const string& symbol)const {
            try {
                return get_asset_entry(symbol).valid();
            }
            
            FC_CAPTURE_AND_RETHROW((symbol))
        }
        
        /**
        *   Calculates the percentage of blocks produced in the last 10 rounds as an average
        *   measure of the delegate participation rate.
        *
        *   @return a value between 0 to 100
        */
        double ChainDatabase::get_average_delegate_participation()const {
            try {
                const auto head_num = get_head_block_num();
                const auto now = thinkyoung::blockchain::now();
                
                if (head_num < 1) {
                    return 0;
                    
                } else if (head_num <= ALP_BLOCKCHAIN_NUM_DELEGATES) {
                    // what percent of the maximum total blocks that could have been produced
                    // have been produced.
                    const auto expected_blocks = (now - get_block_header(1).timestamp).to_seconds() / ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                    return 100 * double(head_num) / expected_blocks;
                    
                } else {
                    // if 10*N blocks ago is longer than 10*N*INTERVAL_SEC ago then we missed blocks, calculate
                    // in terms of percentage time rather than percentage blocks.
                    const auto starting_time = get_block_header(head_num - ALP_BLOCKCHAIN_NUM_DELEGATES).timestamp;
                    const auto expected_production = (now - starting_time).to_seconds() / ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                    return 100 * double(ALP_BLOCKCHAIN_NUM_DELEGATES) / expected_production;
                }
            }
            
            FC_RETHROW_EXCEPTIONS(warn, "")
        }
        
        vector<Operation> ChainDatabase::get_recent_operations(OperationTypeEnum t)const {
            const auto& recent_op_queue = my->_recent_operations[t];
            vector<Operation> recent_ops(recent_op_queue.size());
            std::copy(recent_op_queue.begin(), recent_op_queue.end(), recent_ops.begin());
            return recent_ops;
        }
        
        void ChainDatabase::store_recent_operation(const Operation& o) {
            auto& recent_op_queue = my->_recent_operations[o.type];
            recent_op_queue.push_back(o);
            
            if (recent_op_queue.size() > MAX_RECENT_OPERATIONS)
                recent_op_queue.pop_front();
        }
        
        oResultTIdEntry ChainDatabase::get_transaction_from_result(const TransactionIdType& result_id) const {
            try {
                auto request = contract_lookup_requestid_by_resultid(result_id);
                auto result = contract_lookup_resultid_by_reqestid(result_id);
                return result;
            }
            
            FC_CAPTURE_AND_RETHROW((result_id))
        }
        
        
        oTransactionEntry ChainDatabase::get_transaction(const TransactionIdType& trx_id, bool exact)const {
            try {
                auto trx_rec = my->_transaction_id_to_entry.fetch_optional(trx_id);
                
                if (trx_rec || exact) {
                    if (trx_rec)
                        FC_ASSERT(trx_rec->trx.id() == trx_id, "", ("trx_rec->id", trx_rec->trx.id()));
                        
                    return trx_rec;
                }
                
                auto itr = my->_transaction_id_to_entry.lower_bound(trx_id);
                
                if (itr.valid()) {
                    auto id = itr.key();
                    
                    if (memcmp((char*)&id, (const char*)&trx_id, 4) != 0)
                        return oTransactionEntry();
                        
                    return itr.value();
                }
                
                return oTransactionEntry();
            }
            
            FC_CAPTURE_AND_RETHROW((trx_id)(exact))
        }
        
        void ChainDatabase::store_transaction(const TransactionIdType& entry_id,
                                              const TransactionEntry& entry_to_store) {
            try {
                store(entry_id, entry_to_store);
            }
            
            FC_CAPTURE_AND_RETHROW((entry_id)(entry_to_store))
        }
        
        void ChainDatabase::scan_balances(const function<void(const BalanceEntry&)> callback)const {
            try {
                for (auto iter = my->_balance_id_to_entry.unordered_begin();
                        iter != my->_balance_id_to_entry.unordered_end(); ++iter) {
                    callback(iter->second);
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void  ChainDatabase::scan_contracts(const function<void(const ContractEntry&)> callback)const {
            try {
                for (auto iter = my->_contract_id_to_entry.unordered_begin();
                        iter != my->_contract_id_to_entry.unordered_end(); ++iter) {
                    callback(iter->second);
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void ChainDatabase::scan_transactions(const function<void(const TransactionEntry&)> callback)const {
            try {
                for (auto iter = my->_transaction_id_to_entry.begin();
                        iter.valid(); ++iter) {
                    callback(iter.value());
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void ChainDatabase::scan_unordered_accounts(const function<void(const AccountEntry&)> callback)const {
            try {
                for (auto iter = my->_account_id_to_entry.unordered_begin();
                        iter != my->_account_id_to_entry.unordered_end(); ++iter) {
                    callback(iter->second);
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void ChainDatabase::scan_ordered_accounts(const function<void(const AccountEntry&)> callback)const {
            try {
                for (auto iter = my->_account_name_to_id.ordered_first(); iter.valid(); ++iter) {
                    const oAccountEntry& entry = lookup<AccountEntry>(iter.value());
                    
                    if (entry.valid()) callback(*entry);
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void ChainDatabase::scan_unordered_assets(const function<void(const AssetEntry&)> callback)const {
            try {
                for (auto iter = my->_asset_id_to_entry.unordered_begin();
                        iter != my->_asset_id_to_entry.unordered_end(); ++iter) {
                    callback(iter->second);
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void ChainDatabase::scan_ordered_assets(const function<void(const AssetEntry&)> callback)const {
            try {
                for (auto iter = my->_asset_symbol_to_id.ordered_first(); iter.valid(); ++iter) {
                    const oAssetEntry& entry = lookup<AssetEntry>(iter.value());
                    
                    if (entry.valid()) callback(*entry);
                }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        /** this should throw if the trx is invalid */
        TransactionEvaluationStatePtr ChainDatabase::store_pending_transaction(const SignedTransaction& trx, bool override_limits, bool contract_vm_exec, bool cache) {
            try {
                auto trx_id = trx.id();
                
                if (override_limits)
                    ilog("storing new local transaction with id ${id}", ("id", trx_id));
                    
                auto current_itr = my->_pending_transaction_db.find(trx_id);
                
                if (current_itr.valid())
                    return nullptr;
                    
                ShareType relay_fee = my->_relay_fee;
                
                if (!override_limits) {
                    if (my->_pending_fee_index.size() > ALP_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE) {
                        auto overage = my->_pending_fee_index.size() - ALP_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE;
                        relay_fee = relay_fee * overage * overage;
                    }
                }
                
                TransactionEvaluationStatePtr eval_state = evaluate_transaction(trx, relay_fee, contract_vm_exec, false, cache);
                const ShareType fees = eval_state->get_fees() + eval_state->alt_fees_paid.amount;
                
                //if( fees < my->_relay_fee )
                //   FC_CAPTURE_AND_THROW( insufficient_relay_fee, (fees)(my->_relay_fee) );
                /*
                代理在收到块的时候在对合约调用结果进行校验
                但是这里并不会在验块的时候被调用，因此并不需要对合约调用交易产生的结果与交易中携带的结果进行对比
                这里需要的如果有结果交易，就把结果交易存下
                */
                if (eval_state->p_result_trx.operations.size() > 0) {
                    eval_state->p_result_trx.operations.resize(0);
                }
                
                my->_pending_fee_index[fee_index(fees, trx_id)] = eval_state;
                my->_pending_transaction_db.store(trx_id, trx);//结果交易不保存到pendingdb中
                return eval_state;
            }
            
            FC_CAPTURE_AND_RETHROW((trx)(override_limits))
        }
        
        /** returns all transactions that are valid (independent of each other) sorted by fee */
        std::vector<TransactionEvaluationStatePtr> ChainDatabase::get_pending_transactions()const {
            std::vector<TransactionEvaluationStatePtr> trxs;
            
            for (const auto& item : my->_pending_fee_index) {
                trxs.push_back(item.second);
            }
            
            return trxs;
        }
        
        
        
        
        FullBlock ChainDatabase::generate_block(const time_point_sec block_timestamp, const DelegateConfig& config) {
            try {
                const time_point start_time = time_point::now();
                const PendingChainStatePtr pending_state = std::make_shared<PendingChainState>(shared_from_this());
                //if( pending_state->get_head_block_num() >= ALP_V0_4_9_FORK_BLOCK_NUM )
                // my->execute_markets( block_timestamp, pending_state );
                const SignedBlockHeader head_block = get_head_block();
                // Initialize block
                FullBlock new_block;
                size_t block_size = new_block.block_size();
                
                if (config.block_max_transaction_count > 0 && config.block_max_size > block_size) {
                    // Evaluate pending transactions
                    const vector<TransactionEvaluationStatePtr> pending_trx = get_pending_transactions();
                    
                    for (const TransactionEvaluationStatePtr& item : pending_trx) {
                        // Check block production time limit
                        if (time_point::now() - start_time >= config.block_max_production_time)
                            break;
                            
                        const SignedTransaction& new_transaction = item->trx;
                        
                        try {
                            // Check transaction size limit
                            size_t transaction_size = new_transaction.data_size();
                            
                            if (transaction_size > config.transaction_max_size) {
                                wlog("Excluding transaction ${id} of size ${size} because it exceeds transaction size limit ${limit}",
                                     ("id", new_transaction.id())("size", transaction_size)("limit", config.transaction_max_size));
                                continue;
                            }
                            
                            // Check block size limit
                            if (block_size + transaction_size > config.block_max_size) {
                                wlog("Excluding transaction ${id} of size ${size} because block would exceed block size limit ${limit}",
                                     ("id", new_transaction.id())("size", transaction_size)("limit", config.block_max_size));
                                continue;
                            }
                            
                            // Check transaction blacklist
                            if (!config.transaction_blacklist.empty()) {
                                const TransactionIdType id = new_transaction.id();
                                
                                if (config.transaction_blacklist.count(id) > 0) {
                                    wlog("Excluding blacklisted transaction ${id}", ("id", id));
                                    continue;
                                }
                            }
                            
                            // Check operation blacklist
                            if (!config.operation_blacklist.empty()) {
                                optional<OperationTypeEnum> blacklisted_op;
                                
                                for (const Operation& op : new_transaction.operations) {
                                    if (config.operation_blacklist.count(op.type) > 0) {
                                        blacklisted_op = op.type;
                                        break;
                                    }
                                }
                                
                                if (blacklisted_op.valid()) {
                                    wlog("Excluding transaction ${id} because of blacklisted operation ${op}",
                                         ("id", new_transaction.id())("op", *blacklisted_op));
                                    continue;
                                }
                            }
                            
                            // Validate transaction
                            auto origin_trx_state = std::make_shared<PendingChainState>(pending_state);
                            auto res_trx_state = std::make_shared<PendingChainState>(pending_state);
                            SignedTransaction trx = new_transaction;
                            auto pending_trx_state = origin_trx_state;
                            {
                                auto trx_eval_state = std::make_shared<TransactionEvaluationState>(pending_trx_state.get());
                                trx_eval_state->_enforce_canonical_signatures = config.transaction_canonical_signatures_required;
                                trx_eval_state->_skip_signature_check = true;
                                trx_eval_state->skipexec = false;
                                trx_eval_state->evaluate(new_transaction);
                                
                                if (trx_eval_state->p_result_trx.operations.size() > 0) {
                                    auto result_eval_state = std::make_shared<TransactionEvaluationState>(res_trx_state.get());
                                    result_eval_state->_enforce_canonical_signatures = config.transaction_canonical_signatures_required;
                                    result_eval_state->_skip_signature_check = true;
                                    pending_trx_state = res_trx_state;
                                    result_eval_state->skipexec = true;
                                    result_eval_state->evaluate(trx_eval_state->p_result_trx);
                                    const ImessageIdType iMessageLength = trx_eval_state->imessage_length;
                                    
                                    if (iMessageLength > config.transaction_imessage_max_soft_length) {
                                        continue;
                                    }
                                    
                                    ShareType imessage_fee = 0;
                                    
                                    if (iMessageLength > ALP_BLOCKCHAIN_MAX_FREE_MESSAGE_SIZE) {
                                        imessage_fee = config.transaction_imessage_min_fee_coe * (iMessageLength - ALP_BLOCKCHAIN_MAX_FREE_MESSAGE_SIZE);
                                    }
                                    
                                    // Check transaction fee limit
                                    const ShareType transaction_fee = result_eval_state->get_fees(0) + result_eval_state->alt_fees_paid.amount;
                                    
                                    if (transaction_fee < config.transaction_min_fee + imessage_fee) {
                                        wlog("Excluding transaction ${id} with fee ${fee} because it does not meet transaction fee limit ${limit}",
                                             ("id", trx_eval_state->p_result_trx.id())("fee", transaction_fee)("limit", config.transaction_min_fee));
                                        continue;
                                    }
                                    
                                    //原始交易产生的结果交易大小超过代理设置的交易大小上限，则构建一个不完整的结果交易，
                                    //原始交易以及完整的结果交易的交易id包含在这个不完整的结果交易中
                                    if (trx_eval_state->p_result_trx.data_size() > config.transaction_max_size) {
                                        SignedTransaction result_trx = trx_eval_state->p_result_trx;
                                        result_trx.result_trx_id = result_trx.id();
                                        result_trx.result_trx_type = ResultTransactionType::incomplete_result_transaction;
                                        result_trx.operations.resize(1);
                                        trx = result_trx;
                                        
                                    } else {
                                        SignedTransaction result_trx = trx_eval_state->p_result_trx;
                                        result_trx.result_trx_id = result_trx.id();
                                        result_trx.result_trx_type = ResultTransactionType::complete_result_transaction;
                                        trx = result_trx;
                                    }
                                    
                                } else {
                                    const ImessageIdType iMessageLength = trx_eval_state->imessage_length;
                                    
                                    if (iMessageLength > config.transaction_imessage_max_soft_length) {
                                        continue;
                                    }
                                    
                                    ShareType imessage_fee = 0;
                                    
                                    if (iMessageLength > ALP_BLOCKCHAIN_MAX_FREE_MESSAGE_SIZE) {
                                        imessage_fee = config.transaction_imessage_min_fee_coe * (iMessageLength - ALP_BLOCKCHAIN_MAX_FREE_MESSAGE_SIZE);
                                    }
                                    
                                    // Check transaction fee limit
                                    const ShareType transaction_fee = trx_eval_state->get_fees(0) + trx_eval_state->alt_fees_paid.amount;
                                    
                                    if (transaction_fee < config.transaction_min_fee + imessage_fee) {
                                        wlog("Excluding transaction ${id} with fee ${fee} because it does not meet transaction fee limit ${limit}",
                                             ("id", new_transaction.id())("fee", transaction_fee)("limit", config.transaction_min_fee));
                                        continue;
                                    }
                                }
                            }
                            
                            // Check block size limit
                            if (block_size + transaction_size > config.block_max_size) {
                                wlog("Excluding transaction ${id} of size ${size} because block would exceed block size limit ${limit}",
                                     ("id", new_transaction.id())("size", transaction_size)("limit", config.block_max_size));
                                continue;
                            }
                            
                            pending_trx_state->apply_changes();
                            new_block.user_transactions.push_back(trx);
                            block_size += trx.data_size();
                            
                            // Check block transaction count limit
                            if (new_block.user_transactions.size() >= config.block_max_transaction_count)
                                break;
                                
                        } catch (const fc::canceled_exception&) {
                            throw;
                            
                        } catch (const fc::exception& e) {
                            wlog("Pending transaction was found to be invalid in context of block\n${trx}\n${e}",
                                 ("trx", fc::json::to_pretty_string(new_transaction))("e", e.to_detail_string()));
                        }
                    }
                }
                
                // Populate block header
                new_block.previous = head_block.block_num > 0 ? head_block.id() : BlockIdType();
                new_block.block_num = head_block.block_num + 1;
                new_block.timestamp = block_timestamp;
                new_block.transaction_digest = DigestBlock(new_block).calculate_transaction_digest();
                // if( new_block.block_num < ALP_V0_7_0_FORK_BLOCK_NUM )
                // new_block.transaction_digest = digest_type( "c8cf12fe3180ed901a58a0697a522f1217de72d04529bd255627a4ad6164f0f0" );
                return new_block;
            }
            
            FC_CAPTURE_AND_RETHROW((block_timestamp)(config))
        }
        
        void ChainDatabase::add_observer(ChainObserver* observer) {
            my->_observers.insert(observer);
        }
        
        void ChainDatabase::remove_observer(ChainObserver* observer) {
            my->_observers.erase(observer);
        }
        
        bool ChainDatabase::is_known_block(const BlockIdType& block_id)const {
            try {
                auto fork_data = get_block_fork_data(block_id);
                return fork_data && fork_data->is_known;
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        bool ChainDatabase::is_included_block(const BlockIdType& block_id)const {
            try {
                auto fork_data = get_block_fork_data(block_id);
                return fork_data && fork_data->is_included;
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        optional<BlockForkData> ChainDatabase::get_block_fork_data(const BlockIdType& block_id)const {
            try {
                return my->_fork_db.fetch_optional(block_id);
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        uint32_t ChainDatabase::get_block_num(const BlockIdType& block_id)const {
            try {
                if (block_id == BlockIdType()) return 0;
                
                return get_block(block_id).block_num;
            }
            
            FC_CAPTURE_AND_RETHROW((block_id))
        }
        
        uint32_t ChainDatabase::get_head_block_num()const {
            try {
                return my->_head_block_header.block_num;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        fc::time_point_sec ChainDatabase::get_head_block_timestamp()const {
            try {
                return my->_head_block_header.timestamp;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        vector<EventOperation> ChainDatabase::get_events(uint32_t block_index, const thinkyoung::blockchain::TransactionIdType& trx_id) {
            return my->get_events(block_index, trx_id);
        }
        
        BlockIdType ChainDatabase::get_head_block_id()const {
            try {
                return my->_head_block_id;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        unordered_map<BalanceIdType, BalanceEntry> ChainDatabase::get_balances(const BalanceIdType& first, uint32_t limit)const {
            try {
                unordered_map<BalanceIdType, BalanceEntry> entrys;
                
                for (auto iter = my->_balance_id_to_entry.ordered_lower_bound(first); iter.valid(); ++iter) {
                    entrys[iter.key()] = iter.value();
                    
                    if (entrys.size() >= limit) break;
                }
                
                return entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((first)(limit))
        }
        
        unordered_map<BalanceIdType, BalanceEntry> ChainDatabase::get_balances_for_address(const Address& addr)const {
            try {
                unordered_map<BalanceIdType, BalanceEntry> entrys;
                const auto scan_balance = [&addr, &entrys](const BalanceEntry& entry) {
                    if (entry.is_owner(addr))
                        entrys[entry.id()] = entry;
                };
                scan_balances(scan_balance);
                return entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((addr))
        }
        
        unordered_map<BalanceIdType, BalanceEntry> ChainDatabase::get_balances_for_key(const PublicKeyType& key)const {
            try {
                unordered_map<BalanceIdType, BalanceEntry> entrys;
                const vector<Address> addrs {
                    Address(key),
                    Address(PtsAddress(key, false, 56)),
                    Address(PtsAddress(key, true, 56)),
                    Address(PtsAddress(key, false, 0)),
                    Address(PtsAddress(key, true, 0))
                };
                const auto scan_balance = [&addrs, &entrys](const BalanceEntry& entry) {
                    for (const Address& addr : addrs) {
                        if (entry.is_owner(addr)) {
                            entrys[entry.id()] = entry;
                            break;
                        }
                    }
                };
                scan_balances(scan_balance);
                return entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((key))
        }
        
        vector<AccountEntry> ChainDatabase::get_accounts(const string& first, uint32_t limit)const {
            try {
                vector<AccountEntry> entrys;
                entrys.reserve(std::min(size_t(limit), my->_account_name_to_id.size()));
                
                for (auto iter = my->_account_name_to_id.ordered_lower_bound(first); iter.valid(); ++iter) {
                    const oAccountEntry& entry = lookup<AccountEntry>(iter.value());
                    
                    if (entry.valid()) entrys.push_back(*entry);
                    
                    if (entrys.size() >= limit) break;
                }
                
                return entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((first)(limit))
        }
        
        oAccountEntry ChainDatabase::get_account_by_address(const string  address_str) const {
            auto account_id = my->_account_address_to_id.unordered_find(Address(address_str));
            oAccountEntry entry;
            
            if (account_id != my->_account_address_to_id.unordered_end())
                entry = lookup<AccountEntry>(account_id->second);
                
            return entry;
        }
        
        vector<AssetEntry> ChainDatabase::get_assets(const string& first, uint32_t limit)const {
            try {
                vector<AssetEntry> entrys;
                entrys.reserve(std::min(size_t(limit), my->_asset_symbol_to_id.size()));
                
                for (auto iter = my->_asset_symbol_to_id.ordered_lower_bound(first); iter.valid(); ++iter) {
                    const oAssetEntry& entry = lookup<AssetEntry>(iter.value());
                    
                    if (entry.valid()) entrys.push_back(*entry);
                    
                    if (entrys.size() >= limit) break;
                }
                
                return entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((first)(limit))
        }
        
        string ChainDatabase::export_fork_graph(uint32_t start_block, uint32_t end_block, const fc::path& filename)const {
            FC_ASSERT(start_block >= 0, "Invalid start_block number");
            FC_ASSERT(end_block >= start_block, "End num should bigger than start num");
            std::stringstream out;
            out << "digraph G { \n";
            out << "rankdir=LR;\n";
            bool first = true;
            fc::time_point_sec start_time;
            std::map<uint32_t, vector<SignedBlockHeader>> nodes_by_rank;
            
            //std::set<uint32_t> ranks_in_use;
            for (auto block_itr = my->_block_id_to_full_block.begin(); block_itr.valid(); ++block_itr) {
                const FullBlock& block = block_itr.value();
                
                if (first) {
                    first = false;
                    start_time = block.timestamp;
                }
                
                std::cout << block.block_num << "  start " << start_block << "  end " << end_block << "\n";
                
                if (block.block_num >= start_block && block.block_num <= end_block) {
                    unsigned rank = (unsigned)((block.timestamp - start_time).to_seconds() / ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
                    //ilog( "${id} => ${r}", ("id",fork_itr.key())("r",fork_data) );
                    nodes_by_rank[rank].push_back(block);
                }
            }
            
            for (const auto& item : nodes_by_rank) {
                out << "{rank=same l" << item.first << "[style=invis, shape=point] ";
                
                for (const auto& entry : item.second)
                    out << "; \"" << std::string(entry.id()) << "\"";
                    
                out << ";}\n";
            }
            
            for (const auto& blocks_at_time : nodes_by_rank) {
                for (const auto& block : blocks_at_time.second) {
                    auto delegate_entry = get_block_signee(block.id());
                    out << '"' << std::string(block.id()) << "\" "
                        << "[label=<"
                        << std::string(block.id()).substr(0, 5)
                        << "<br/>" << blocks_at_time.first
                        << "<br/>" << block.block_num
                        << "<br/>" << delegate_entry.name
                        << ">,style=filled,rank=" << blocks_at_time.first << "];\n";
                    out << '"' << std::string(block.id()) << "\" -> \"" << std::string(block.previous) << "\";\n";
                }
            }
            
            out << "edge[style=invis];\n";
            bool first2 = true;
            
            for (const auto& item : nodes_by_rank) {
                if (first2)
                    first2 = false;
                    
                else
                    out << "->";
                    
                out << "l" << item.first;
            }
            
            out << ";\n";
            out << "}";
            
            if (filename == "")
                return out.str();
                
            FC_ASSERT(!fc::exists(fc::path(filename)), "path ${n} not exsits!", ("n", filename));
            std::ofstream fileout(filename.generic_string().c_str());
            fileout << out.str();
            return std::string();
        }
        uint32_t ChainDatabase::get_fork_list_num() {
            uint32_t return_value = 0;
            
            for (auto iter = my->_fork_db.begin(); iter.valid(); ++iter) {
                uint32_t temp = get_block_num(iter.key());
                
                if (return_value < temp) {
                    return_value = temp;
                }
            }
            
            return return_value;
        }
        std::map<uint32_t, std::vector<ForkEntry>> ChainDatabase::get_forks_list()const {
            std::map<uint32_t, std::vector<ForkEntry>> fork_blocks;
            
            for (auto iter = my->_fork_db.begin(); iter.valid(); ++iter) {
                try {
                    auto fork_iter = iter.value();
                    
                    if (fork_iter.next_blocks.size() > 1) {
                        vector<ForkEntry> forks;
                        
                        for (const auto& forked_block_id : fork_iter.next_blocks) {
                            ForkEntry fork;
                            BlockForkData fork_data = my->_fork_db.fetch(forked_block_id);
                            fork.block_id = forked_block_id;
                            fork.signing_delegate = get_block_signee(forked_block_id).id;
                            fork.is_valid = fork_data.is_valid;
                            fork.invalid_reason = fork_data.invalid_reason;
                            fork.is_current_fork = fork_data.is_included;
                            
                            if (get_statistics_enabled()) {
                                const oBlockEntry& entry = my->_block_id_to_block_entry_db.fetch_optional(forked_block_id);
                                
                                if (entry.valid()) {
                                    fork.latency = entry->latency;
                                    fork.transaction_count = entry->user_transaction_ids.size();
                                    fork.size = entry->block_size;
                                    fork.timestamp = entry->timestamp;
                                }
                            }
                            
                            forks.push_back(fork);
                        }
                        
                        //fork_blocks[get_block_num(iter.key())] = forks;   原来代码
                        
                        /*
                        在数据文件错误时会出现_fork_db中存在的fork_data,此时直接 get_block_num(iter.key()) 会出现找不到的状况
                        在未作判断时会异常中断程序导致无法自动恢复
                        检测到异常后先重建index,可以修复部分状况
                        但若_block_id_to_full_block中存在blocknum不连续的块如1,3,缺第2块,
                        在不获取到缺失块的情况下无法自动恢复.
                        对于full_db中不存在的块不返回fork信息也没什么影响
                        可以考虑判断是否is_known,如果不存在则不返回该fork信息，也不抛出异常，设置下次重建index,返回,等待程序进行块同步
                        */
                        if (iter.value().is_known) {
                            fork_blocks[get_block_num(iter.key())] = forks;
                            
                        } else {
                            my->_property_id_to_entry.remove(static_cast<uint8_t>(PropertyIdType::database_version));
                        }
                    }
                    
                } catch (const fc::exception&) {
                    wlog("error fetching block num of block ${b} while building fork list", ("b", iter.key()));
                    throw;
                }
            }
            
            return fork_blocks;
        }
        
        vector<SlotEntry> ChainDatabase::get_delegate_slot_entrys(const AccountIdType delegate_id, uint32_t limit)const {
            try {
                FC_ASSERT(limit > 0, "Invalid limit");
                vector<SlotEntry> slot_entrys;
                slot_entrys.reserve(std::min(limit, get_head_block_num()));
                const SlotIndex key = SlotIndex(delegate_id, my->_head_block_header.timestamp);
                
                for (auto iter = my->_slot_index_to_entry.lower_bound(key); iter.valid(); ++iter) {
                    const SlotEntry& entry = iter.value();
                    
                    if (entry.index.delegate_id != delegate_id) break;
                    
                    slot_entrys.push_back(entry);
                    
                    if (slot_entrys.size() >= limit) break;
                }
                
                return slot_entrys;
            }
            
            FC_CAPTURE_AND_RETHROW((delegate_id)(limit))
        }
        
        
        
        
        PendingChainStatePtr ChainDatabase::get_pending_state()const {
            return my->_pending_trx_state;
        }
        
        bool ChainDatabase::get_is_in_sandbox()const {
            return my->_is_in_sandbox;
        }
        
        void ChainDatabase::set_is_in_sandbox(bool sandbox) {
            my->_is_in_sandbox = sandbox;
        }
        
        PendingChainStatePtr ChainDatabase::get_sandbox_pending_state() {
            if (my->_sandbox_pending_state == nullptr)
                my->_sandbox_pending_state = std::make_shared<PendingChainState>(shared_from_this());
                
            return my->_sandbox_pending_state;
        }
        
        void ChainDatabase::set_sandbox_pending_state(PendingChainStatePtr state_ptr) {
            my->_sandbox_pending_state = state_ptr;
        }
        
        void ChainDatabase::clear_sandbox_pending_state() {
            my->_sandbox_pending_state = nullptr;
        }
        
        bool ChainDatabase::store_balance_entries_for_sandbox() {
            my->_sandbox_pending_state->_balance_id_to_entry.insert(my->_balance_id_to_entry.unordered_begin(), my->_balance_id_to_entry.unordered_end());
            return true;
        }
        
        bool ChainDatabase::store_account_entries_for_sandbox(const vector<thinkyoung::wallet::WalletAccountEntry>& vec_account_entry) {
            for (auto iter = vec_account_entry.begin(); iter != vec_account_entry.end(); ++iter) {
                SandboxAccountInfo tmp;
                AccountEntry  entry = (AccountEntry)(*iter);
                tmp.id = entry.id;
                tmp.name = entry.name;
                tmp.delegate_info = entry.delegate_info;
                tmp.owner_address = entry.owner_address().AddressToString();
                tmp.owner_key = entry.owner_key;
                tmp.registration_date = entry.registration_date;
                tmp.last_update = entry.last_update;
                my->_sandbox_pending_state->_vec_wallet_accounts.push_back(std::move(tmp));
            }
            
            return true;
        }
        
        bool ChainDatabase::is_known_transaction(const Transaction& trx)const {
            try {
                return my->_unique_transactions.count(UniqueTransactionKey(trx, get_chain_id())) > 0;
            }
            
            FC_CAPTURE_AND_RETHROW((trx))
        }
        
        void ChainDatabase::set_relay_fee(ShareType shares) {
            my->_relay_fee = shares;
        }
        
        ShareType ChainDatabase::get_relay_fee() {
            return my->_relay_fee;
        }
        
        ShareType ChainDatabase::get_product_reword_fee() {
            return my->_block_per_account_reword_amount;
        }
        
        
        
        
        
        void ChainDatabase::generate_issuance_map(const string& symbol, const fc::path& filename)const {
            try {
                map<string, ShareType> issuance_map;
                auto uia = get_asset_entry(symbol);
                FC_ASSERT(uia.valid(), "${n} is not a avaliable asset symbol", ("n", symbol));
                const auto scan_trx = [&](const TransactionEntry trx_rec) {
                    // Did we issue the asset in this transaction?
                    bool issued = false;
                    
                    for (auto op : trx_rec.trx.operations) {
                        if (op.type == issue_asset_op_type) {
                            auto issue = op.as<IssueAssetOperation>();
                            
                            if (issue.amount.asset_id != uia->id)
                                continue;
                                
                            issued = true;
                            break;
                        }
                    }
                    
                    if (issued) {
                        for (auto op : trx_rec.trx.operations) {
                            // make sure we didn't withdraw any of that op, that would only happen when someone is trying to be tricky
                            if (op.type == withdraw_op_type) {
                                auto withdraw = op.as<WithdrawOperation>();
                                auto obal = get_balance_entry(withdraw.balance_id);
                                FC_ASSERT(obal->asset_id() != uia->id, "There was a withdraw for this UIA in an op that issued it!");
                            }
                            
                            if (op.type == deposit_op_type) {
                                auto deposit = op.as<DepositOperation>();
                                
                                if (deposit.condition.asset_id != uia->id)
                                    continue;
                                    
                                FC_ASSERT(deposit.condition.type == withdraw_signature_type,
                                          "I can't process deposits for any condition except withdraw_signature yet");
                                auto raw_addr = string(*deposit.condition.owner());
                                
                                if (issuance_map.find(raw_addr) != issuance_map.end())
                                    issuance_map[raw_addr] += deposit.amount;
                                    
                                else
                                    issuance_map[raw_addr] = deposit.amount;
                            }
                        }
                    }
                };
                scan_transactions(scan_trx);
                fc::json::save_to_file(issuance_map, filename);
            }
            
            FC_CAPTURE_AND_RETHROW((symbol)(filename))
        }
        
        // NOTE: Only base asset 0 is snapshotted and addresses can have multiple entries
        void ChainDatabase::generate_snapshot(const fc::path& filename)const {
            try {
                GenesisState snapshot = get_builtin_genesis_block_config();
                snapshot.timestamp = now();
                snapshot.initial_balances.clear();
                snapshot.sharedrop_balances.reserve_balances.clear();
                
                for (auto iter = my->_balance_id_to_entry.unordered_begin();
                        iter != my->_balance_id_to_entry.unordered_end(); ++iter) {
                    const BalanceEntry& entry = iter->second;
                    
                    if (entry.asset_id() != 0) continue;
                    
                    GenesisBalance balance;
                    
                    if (entry.snapshot_info.valid()) {
                        balance.raw_address = entry.snapshot_info->original_address;
                        
                    } else {
                        const auto owner = entry.owner();
                        
                        if (!owner.valid()) continue;
                        
                        balance.raw_address = string(*owner);
                    }
                    
                    balance.balance = entry.balance;
                    
                    if (entry.condition.type == withdraw_signature_type)
                        snapshot.initial_balances.push_back(balance);
                }
                
                // Add outstanding delegate pay balances
                for (auto iter = my->_account_id_to_entry.unordered_begin();
                        iter != my->_account_id_to_entry.unordered_end(); ++iter) {
                    const AccountEntry& entry = iter->second;
                    
                    if (!entry.is_delegate()) continue;
                    
                    if (entry.is_retracted()) continue;
                    
                    GenesisBalance balance;
                    balance.raw_address = string(entry.owner_address());
                    balance.balance = entry.delegate_pay_balance();
                    snapshot.initial_balances.push_back(balance);
                }
                
                fc::json::save_to_file(snapshot, filename);
            }
            
            FC_CAPTURE_AND_RETHROW((filename))
        }
        
        Asset ChainDatabase::calculate_supply(const AssetIdType asset_id)const {
            const auto entry = get_asset_entry(asset_id);
            FC_ASSERT(entry.valid(), "Invalidate asset id");
            // Add fees
            Asset total(entry->collected_fees, asset_id);
            
            // Add balances
            for (auto iter = my->_balance_id_to_entry.unordered_begin();
                    iter != my->_balance_id_to_entry.unordered_end(); ++iter) {
                const BalanceEntry& balance = iter->second;
                
                if (balance.asset_id() == total.asset_id)
                    total.amount += balance.balance;
            }
            
            // If base asset
            if (asset_id == AssetIdType(0)) {
                // Add pay balances
                for (auto account_itr = my->_account_id_to_entry.unordered_begin();
                        account_itr != my->_account_id_to_entry.unordered_end(); ++account_itr) {
                    const AccountEntry& account = account_itr->second;
                    
                    if (account.delegate_info.valid())
                        total.amount += account.delegate_info->pay_balance;
                }
                
            } else { // If non-base asset
                //
            }
            
            return total;
        }
        
        
        Asset ChainDatabase::unclaimed_genesis() {
            try {
                Asset unclaimed_total;
                const auto genesis_date = get_genesis_timestamp();
                const auto scan_balance = [&unclaimed_total, &genesis_date](const BalanceEntry& entry) {
                    if (entry.snapshot_info.valid()) {
                        if (entry.last_update <= genesis_date)
                            unclaimed_total.amount += entry.balance;
                    }
                };
                scan_balances(scan_balance);
                return unclaimed_total;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        
        
        
        vector<AlpTrxidBalance> ChainDatabase::fetch_alp_input_balance(const uint32_t & block_num) {
            try {
                vector < AlpTrxidBalance > results;
                
                for (auto iter = my->_alp_input_balance_entry.unordered_begin();
                        iter != my->_alp_input_balance_entry.unordered_end(); ++iter) {
                    AlpTrxidBalance alpTemp;
                    alpTemp.block_num = block_num + 1;
                    set<AlpTrxidBalance>::iterator block_iter = iter->second.lower_bound(alpTemp);
                    
                    for (; block_iter != iter->second.end(); ++block_iter) {
                        results.push_back(*block_iter);
                    }
                }
                
                return results;
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        vector<AlpTrxidBalance> ChainDatabase::fetch_alp_full_entry(const uint32_t& block_num, const uint32_t & last_scan_block_num) {
            try {
                vector < AlpTrxidBalance > results;
                
                if (block_num > last_scan_block_num) {
                    return results;
                }
                
                for (auto iter = my->_alp_full_entry.unordered_begin();
                        iter != my->_alp_full_entry.unordered_end(); ++iter) {
                    auto block_iter = iter->second.alp_block_sort.lower_bound(block_num + 1);
                    
                    for (; block_iter != iter->second.alp_block_sort.end(); block_iter++) {
                        if (block_iter->second.block_num <= last_scan_block_num) {
                            results.emplace_back(block_iter->second);
                        }
                    }
                }
                
                return results;
            }
            
            FC_CAPTURE_AND_RETHROW((block_num))
        }
        void ChainDatabase::transaction_insert_to_alp_full_entry(const string & alp_account, const AlpTrxidBalance & alp_balance_record) {
            auto iter_entry = my->_alp_full_entry.unordered_find(alp_account);
            AlpBalanceEntry alpentrys;
            
            if (iter_entry != my->_alp_full_entry.unordered_end()) {
                alpentrys = iter_entry->second;
                
                if (alpentrys.alp_trxid_sort.count(alp_balance_record.trx_id) != 0) {
                    return;
                }
            }
            
            alpentrys.alp_trxid_sort.insert(alp_balance_record.trx_id);
            alpentrys.alp_block_sort.insert(std::make_pair(alp_balance_record.block_num, alp_balance_record));
            my->_alp_full_entry.store(alp_account, alpentrys);
        }
        void ChainDatabase::transaction_erase_from_alp_full_entry(const string & alp_account, const AlpTrxidBalance & alp_balance_record) {
            auto iter_entry = my->_alp_full_entry.unordered_find(alp_account);
            
            if (iter_entry != my->_alp_full_entry.unordered_end()) {
                AlpBalanceEntry alpentrys = iter_entry->second;
                alpentrys.alp_trxid_sort.erase(alp_balance_record.trx_id);
                auto iter = alpentrys.alp_block_sort.lower_bound(alp_balance_record.block_num);
                
                for (; iter != alpentrys.alp_block_sort.end();) {
                    if (iter->second.block_num != alp_balance_record.block_num) {
                        break;
                    }
                    
                    if (iter->second.trx_id == alp_balance_record.trx_id) {
                        iter = alpentrys.alp_block_sort.erase(iter);
                        
                    } else {
                        ++iter;
                    }
                }
                
                if (!(alpentrys.alp_block_sort.empty())) {
                    my->_alp_full_entry.store(alp_account, alpentrys);
                    
                } else {
                    my->_alp_full_entry.remove(alp_account);
                }
            }
        }
        void ChainDatabase::transaction_insert_to_alp_balance(const string & alp_account, const AlpTrxidBalance & alp_balance_entry) {
            auto iter_ids = my->_alp_input_balance_entry.unordered_find(alp_account);
            set<AlpTrxidBalance> ids;
            
            if (iter_ids != my->_alp_input_balance_entry.unordered_end()) {
                ids = iter_ids->second;
            }
            
            ids.insert(alp_balance_entry);
            my->_alp_input_balance_entry.store(alp_account, ids);
        }
        void ChainDatabase::transaction_erase_from_alp_balance(const string & alp_account, const AlpTrxidBalance & alp_balance_entry) {
            auto iter_ids = my->_alp_input_balance_entry.unordered_find(alp_account);
            
            if (iter_ids != my->_alp_input_balance_entry.unordered_end()) {
                auto ids = iter_ids->second;
                ids.erase(alp_balance_entry);
                
                if (!(ids.empty())) {
                    my->_alp_input_balance_entry.store(alp_account, ids);
                    
                } else {
                    my->_alp_input_balance_entry.remove(alp_account);
                }
            }
        }
        vector<TransactionEntry> ChainDatabase::fetch_address_transactions(const Address& addr) {
            try {
                vector<TransactionEntry> results;
                const auto transaction_ids = my->_address_to_transaction_ids.fetch_optional(addr);
                
                if (transaction_ids.valid()) {
                    for (const TransactionIdType& transaction_id : *transaction_ids) {
                        oTransactionEntry entry = get_transaction(transaction_id);
                        
                        if (entry.valid()) results.push_back(std::move(*entry));
                    }
                }
                
                return results;
            }
            
            FC_CAPTURE_AND_RETHROW((addr))
        }
        
        oPropertyEntry ChainDatabase::property_lookup_by_id(const PropertyIdType id)const {
            const auto iter = my->_property_id_to_entry.unordered_find(static_cast<uint8_t>(id));
            
            if (iter != my->_property_id_to_entry.unordered_end()) return iter->second;
            
            return oPropertyEntry();
        }
        
        void ChainDatabase::property_insert_into_id_map(const PropertyIdType id, const PropertyEntry& entry) {
            my->_property_id_to_entry.store(static_cast<uint8_t>(id), entry);
        }
        
        void ChainDatabase::property_erase_from_id_map(const PropertyIdType id) {
            my->_property_id_to_entry.remove(static_cast<uint8_t>(id));
        }
        
        oAccountEntry ChainDatabase::account_lookup_by_id(const AccountIdType id)const {
            const auto iter = my->_account_id_to_entry.unordered_find(id);
            
            if (iter != my->_account_id_to_entry.unordered_end()) return iter->second;
            
            return oAccountEntry();
        }
        
        oAccountEntry ChainDatabase::account_lookup_by_name(const string& name)const {
            const auto iter = my->_account_name_to_id.unordered_find(name);
            
            if (iter != my->_account_name_to_id.unordered_end()) return account_lookup_by_id(iter->second);
            
            return oAccountEntry();
        }
        
        oAccountEntry ChainDatabase::account_lookup_by_address(const Address& addr)const {
            const auto iter = my->_account_address_to_id.unordered_find(addr);
            
            if (iter != my->_account_address_to_id.unordered_end()) return account_lookup_by_id(iter->second);
            
            return oAccountEntry();
        }
        
        void ChainDatabase::account_insert_into_id_map(const AccountIdType id, const AccountEntry& entry) {
            my->_account_id_to_entry.store(id, entry);
        }
        
        void ChainDatabase::account_insert_into_name_map(const string& name, const AccountIdType id) {
            my->_account_name_to_id.store(name, id);
        }
        
        void ChainDatabase::account_insert_into_address_map(const Address& addr, const AccountIdType id) {
            my->_account_address_to_id.store(addr, id);
        }
        
        void ChainDatabase::account_insert_into_vote_set(const VoteDel& vote) {
            my->_delegate_votes.insert(vote);
        }
        
        void ChainDatabase::account_erase_from_id_map(const AccountIdType id) {
            my->_account_id_to_entry.remove(id);
        }
        
        void ChainDatabase::account_erase_from_name_map(const string& name) {
            my->_account_name_to_id.remove(name);
        }
        
        void ChainDatabase::account_erase_from_address_map(const Address& addr) {
            my->_account_address_to_id.remove(addr);
        }
        
        void ChainDatabase::account_erase_from_vote_set(const VoteDel& vote) {
            my->_delegate_votes.erase(vote);
        }
        
        oAssetEntry ChainDatabase::asset_lookup_by_id(const AssetIdType id)const {
            const auto iter = my->_asset_id_to_entry.unordered_find(id);
            
            if (iter != my->_asset_id_to_entry.unordered_end()) return iter->second;
            
            return oAssetEntry();
        }
        
        oAssetEntry ChainDatabase::asset_lookup_by_symbol(const string& symbol)const {
            const auto iter = my->_asset_symbol_to_id.unordered_find(symbol);
            
            if (iter != my->_asset_symbol_to_id.unordered_end()) return asset_lookup_by_id(iter->second);
            
            return oAssetEntry();
        }
        
        void ChainDatabase::asset_insert_into_id_map(const AssetIdType id, const AssetEntry& entry) {
            my->_asset_id_to_entry.store(id, entry);
        }
        
        void ChainDatabase::asset_insert_into_symbol_map(const string& symbol, const AssetIdType id) {
            my->_asset_symbol_to_id.store(symbol, id);
        }
        
        void ChainDatabase::asset_erase_from_id_map(const AssetIdType id) {
            my->_asset_id_to_entry.remove(id);
        }
        
        void ChainDatabase::asset_erase_from_symbol_map(const string& symbol) {
            my->_asset_symbol_to_id.remove(symbol);
        }
        
        oSlateEntry ChainDatabase::slate_lookup_by_id(const SlateIdType id)const {
            const auto iter = my->_slate_id_to_entry.unordered_find(id);
            
            if (iter != my->_slate_id_to_entry.unordered_end()) return iter->second;
            
            return oSlateEntry();
        }
        
        void ChainDatabase::slate_insert_into_id_map(const SlateIdType id, const SlateEntry& entry) {
            my->_slate_id_to_entry.store(id, entry);
        }
        
        void ChainDatabase::slate_erase_from_id_map(const SlateIdType id) {
            my->_slate_id_to_entry.remove(id);
        }
        
        oBalanceEntry ChainDatabase::balance_lookup_by_id(const BalanceIdType& id)const {
            const auto iter = my->_balance_id_to_entry.unordered_find(id);
            
            if (iter != my->_balance_id_to_entry.unordered_end()) return iter->second;
            
            return oBalanceEntry();
        }
        
        void ChainDatabase::balance_insert_into_id_map(const BalanceIdType& id, const BalanceEntry& entry) {
            my->_balance_id_to_entry.store(id, entry);
        }
        
        void ChainDatabase::balance_erase_from_id_map(const BalanceIdType& id) {
            my->_balance_id_to_entry.remove(id);
        }
        void ChainDatabase::status_insert_into_block_map(const BlockIdType& id, const int& version) {
            data_version temp_version;
            temp_version.id = id;
            temp_version.version = version;
            my->_block_extend_status.store(1, temp_version);
        }
        void ChainDatabase::status_erase_from_block_map() {
            my->_block_extend_status.remove(1);
        }
        
        oTransactionEntry ChainDatabase::transaction_lookup_by_id(const TransactionIdType& id)const {
            return my->_transaction_id_to_entry.fetch_optional(id);
        }
        
        void ChainDatabase::transaction_insert_into_id_map(const TransactionIdType& id, const TransactionEntry& entry) {
            my->_transaction_id_to_entry.store(id, entry);
            
            if (get_statistics_enabled()) {
                const auto scan_address = [&](const Address& addr) {
                    auto ids = my->_address_to_transaction_ids.fetch_optional(addr);
                    
                    if (!ids.valid()) ids = unordered_set<TransactionIdType>();
                    
                    ids->insert(id);
                    my->_address_to_transaction_ids.store(addr, *ids);
                };
                entry.scan_addresses(*this, scan_address);
                
                for (const Operation& op : entry.trx.operations)
                    store_recent_operation(op);
            }
        }
        
        void ChainDatabase::transaction_insert_into_unique_set(const Transaction& trx) {
            my->_unique_transactions.emplace_hint(my->_unique_transactions.cend(), trx, get_chain_id());
        }
        
        void ChainDatabase::transaction_erase_from_id_map(const TransactionIdType& id) {
            if (get_statistics_enabled()) {
                const oTransactionEntry entry = transaction_lookup_by_id(id);
                
                if (entry.valid()) {
                    const auto scan_address = [&](const Address& addr) {
                        auto ids = my->_address_to_transaction_ids.fetch_optional(addr);
                        
                        if (!ids.valid()) return;
                        
                        ids->erase(id);
                        
                        if (!ids->empty()) my->_address_to_transaction_ids.store(addr, *ids);
                        
                        else my->_address_to_transaction_ids.remove(addr);
                    };
                    entry->scan_addresses(*this, scan_address);
                }
            }
            
            my->_transaction_id_to_entry.remove(id);
        }
        
        void ChainDatabase::transaction_erase_from_unique_set(const Transaction& trx) {
            my->_unique_transactions.erase(UniqueTransactionKey(trx, get_chain_id()));
        }
        
        oSlotEntry ChainDatabase::slot_lookup_by_index(const SlotIndex index)const {
            return my->_slot_index_to_entry.fetch_optional(index);
        }
        
        oSlotEntry ChainDatabase::slot_lookup_by_timestamp(const time_point_sec timestamp)const {
            const optional<AccountIdType> delegate_id = my->_slot_timestamp_to_delegate.fetch_optional(timestamp);
            
            if (!delegate_id.valid()) return oSlotEntry();
            
            return my->_slot_index_to_entry.fetch_optional(SlotIndex(*delegate_id, timestamp));
        }
        
        void ChainDatabase::slot_insert_into_index_map(const SlotIndex index, const SlotEntry& entry) {
            my->_slot_index_to_entry.store(index, entry);
        }
        
        void ChainDatabase::slot_insert_into_timestamp_map(const time_point_sec timestamp, const AccountIdType delegate_id) {
            my->_slot_timestamp_to_delegate.store(timestamp, delegate_id);
        }
        
        void ChainDatabase::slot_erase_from_index_map(const SlotIndex index) {
            my->_slot_index_to_entry.remove(index);
        }
        
        void ChainDatabase::slot_erase_from_timestamp_map(const time_point_sec timestamp) {
            my->_slot_timestamp_to_delegate.remove(timestamp);
        }
        uint32_t    ChainDatabase::get_forkdb_num() {
            return m_fork_num_before;
        }
        void ChainDatabase::store_extend_status(const BlockIdType& id, int version) {
            status_insert_into_block_map(id, version);
        }
        std::pair<BlockIdType, int>  ChainDatabase::get_last_extend_status() {
            try {
                auto iter = my->_block_extend_status.begin();
                
                if (iter.valid())
                    return std::make_pair<BlockIdType, int>(iter.value().id, iter.value().version);
                    
                else {
                    if (my->_block_extend_status.size() > 0) {
                        my->_block_extend_status.remove(my->_block_extend_status.begin().key());
                    }
                    
                    return std::make_pair<BlockIdType, int>(BlockIdType(), 0);
                }
                
            } catch (...) {
                if (my->_block_extend_status.size() > 0) {
                    my->_block_extend_status.remove(my->_block_extend_status.begin().key());
                }
                
                return std::make_pair<BlockIdType, int>(BlockIdType(), 0);
            }
        }
        void    ChainDatabase::set_forkdb_num(uint32_t forkdb_num) {
            m_fork_num_before = forkdb_num;
        }
        void   ChainDatabase::repair_database() {
            auto last_status = get_last_extend_status();
            
            if (last_status.second == 1) {
                my->repair_block(last_status.first);
            }
        }
        
        void ChainDatabase::dump_state(const fc::path& path)const {
            try {
                const auto dir = fc::absolute(path);
                FC_ASSERT(!fc::exists(dir), "Directory ${n} exsits!", ("n", dir));
                fc::create_directories(dir);
                fc::path next_path;
                ulog("This will take a while...");
                next_path = dir / "_block_extend_status.json";
                my->_block_extend_status.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_block_id_to_full_block.json";
                my->_block_id_to_full_block.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_block_id_to_undo_state.json";
                my->_block_id_to_undo_state.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_fork_db.json";
                my->_fork_db.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_revalidatable_future_blocks_db.json";
                my->_revalidatable_future_blocks_db.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_block_num_to_id_db.json";
                my->_block_num_to_id_db.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_block_id_to_block_entry_db.json";
                my->_block_id_to_block_entry_db.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_property_id_to_entry.json";
                my->_property_id_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_account_id_to_entry.json";
                my->_account_id_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_account_name_to_id.json";
                my->_account_name_to_id.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_account_address_to_id.json";
                my->_account_address_to_id.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_asset_id_to_entry.json";
                my->_asset_id_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_asset_symbol_to_id.json";
                my->_asset_symbol_to_id.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_slate_id_to_entry.json";
                my->_slate_id_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_balance_id_to_entry.json";
                my->_balance_id_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_transaction_id_to_entry.json";
                my->_transaction_id_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_address_to_transaction_ids.json";
                my->_address_to_transaction_ids.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                /*
                next_path = dir / "_delegate_votes.json";
                my->_delegate_votes.export_to_json(next_path);
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_property_id_to_entry.json";
                my->_property_id_to_entry.export_to_json(next_path);
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_block_num_to_id_db.json";
                my->_block_num_to_id_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_block_id_to_block_entry_db.json";
                my->_block_id_to_block_entry_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_block_id_to_block_data_db.json";
                my->_block_id_to_block_data_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_id_to_transaction_entry_db.json";
                my->_id_to_transaction_entry_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_asset_db.json";
                my->_asset_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_balance_db.json";
                my->_balance_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                
                
                next_path = dir / "_account_db.json";
                my->_account_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_address_to_account_db.json";
                my->_address_to_account_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_account_index_db.json";
                my->_account_index_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_symbol_index_db.json";
                my->_symbol_index_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                
                next_path = dir / "_delegate_vote_index_db.json";
                my->_delegate_vote_index_db.export_to_json( next_path );
                ulog( "Dumped ${p}", ("p",next_path) );
                */
                next_path = dir / "_slot_index_to_entry.json";
                my->_slot_index_to_entry.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_slot_timestamp_to_delegate.json";
                my->_slot_timestamp_to_delegate.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "_fork_number_db.json";
                my->_fork_number_db.export_to_json(next_path);
                ulog("Dumped ${p}", ("p", next_path));
                next_path = dir / "alp_transaction_balance_db.json";
                my->_alp_input_balance_entry.export_to_json(next_path);
                next_path = dir / "_contract_id_to_info_db.json";
                my->_contract_id_to_entry.export_to_json(next_path);
                next_path = dir / "_contract_id_to_storage_db.json";
                my->_contract_id_to_storage.export_to_json(next_path);
                next_path = dir / "_contract_name_to_id_db.json";
                my->_contract_name_to_id.export_to_json(next_path);
                next_path = dir / "_result_to_request_iddb.json";
                my->_result_to_request_iddb.export_to_json(next_path);
                next_path = dir / "_request_to_result_iddb.json";
                my->_request_to_result_iddb.export_to_json(next_path);
                next_path = dir / "_trx_to_contract_iddb.json";
                my->_trx_to_contract_iddb.export_to_json(next_path);
                next_path = dir / "_contract_to_trx_iddb.json";
                my->_contract_to_trx_iddb.export_to_json(next_path);
            }
            
            FC_CAPTURE_AND_RETHROW((path))
        }
        
        oContractEntry  ChainDatabase::contract_lookup_by_id(const ContractIdType& id)const {
            const auto iter = my->_contract_id_to_entry.unordered_find(id);
            
            if (iter != my->_contract_id_to_entry.unordered_end()) return iter->second;
            
            return oContractEntry();
        }
        
        oContractEntry  ChainDatabase::contract_lookup_by_name(const ContractName& name)const {
            const auto iter = my->_contract_name_to_id.unordered_find(name);
            
            if (iter != my->_contract_name_to_id.unordered_end()) return contract_lookup_by_id(iter->second);
            
            return oContractEntry();
        }
        
        oContractStorage ChainDatabase::contractstorage_lookup_by_id(const ContractIdType& id)const {
            const auto iter = my->_contract_id_to_storage.unordered_find(id);
            
            if (iter != my->_contract_id_to_storage.unordered_end()) return iter->second;
            
            return oContractStorage();
        }
        
        void ChainDatabase::contract_insert_into_id_map(const ContractIdType& id, const ContractEntry& info) {
            my->_contract_id_to_entry.store(id, info);
        }
        
        void ChainDatabase::contractstorage_insert_into_id_map(const ContractIdType& id, const ContractStorageEntry& storage) {
            my->_contract_id_to_storage.store(id, storage);
        }
        
        void ChainDatabase::contract_insert_into_name_map(const ContractName& name, const ContractIdType& id) {
            my->_contract_name_to_id.store(name, id);
        }
        
        void ChainDatabase::contract_erase_from_id_map(const ContractIdType& id) {
            my->_contract_id_to_entry.remove(id);
        }
        
        void ChainDatabase::contractstorage_erase_from_id_map(const ContractIdType& id) {
            my->_contract_id_to_storage.remove(id);
        }
        
        void ChainDatabase::contract_erase_from_name_map(const ContractName& name) {
            my->_contract_name_to_id.remove(name);
        }
        
        oResultTIdEntry thinkyoung::blockchain::ChainDatabase::contract_lookup_resultid_by_reqestid(const TransactionIdType & id) const {
            auto it = my->_request_to_result_iddb.unordered_find(id);
            
            if (it != my->_request_to_result_iddb.unordered_end())
                return it->second;
                
            return oResultTIdEntry();
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_store_resultid_by_reqestid(const TransactionIdType & req, const ResultTIdEntry & res) {
            my->_request_to_result_iddb.store(req, res);
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_erase_resultid_by_reqestid(const TransactionIdType & req) {
            my->_request_to_result_iddb.remove(req);
        }
        
        oRequestIdEntry thinkyoung::blockchain::ChainDatabase::contract_lookup_requestid_by_resultid(const TransactionIdType &id) const {
            auto it = my->_result_to_request_iddb.unordered_find(id);
            
            if (it != my->_result_to_request_iddb.unordered_end())
                return it->second;
                
            return oRequestIdEntry();
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_store_requestid_by_resultid(const TransactionIdType & res, const RequestIdEntry & req) {
            my->_result_to_request_iddb.store(res, req);
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_erase_requestid_by_resultid(const TransactionIdType & result) {
            my->_result_to_request_iddb.remove(result);
        }
        
        oContractinTrxEntry thinkyoung::blockchain::ChainDatabase::contract_lookup_contractid_by_trxid(const TransactionIdType &id) const {
            auto it = my->_trx_to_contract_iddb.unordered_find(id);
            
            if (it != my->_trx_to_contract_iddb.unordered_end())
                return it->second;
                
            return oContractinTrxEntry();
        }
        
        oContractTrxEntry thinkyoung::blockchain::ChainDatabase::contract_lookup_trxid_by_contract_id(const ContractIdType &id) const {
            auto it = my->_contract_to_trx_iddb.unordered_find(id);
            
            if (it != my->_contract_to_trx_iddb.unordered_end())
                return it->second;
                
            return oContractTrxEntry();
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_store_contractid_by_trxid(const TransactionIdType & tid, const ContractinTrxEntry & cid) {
            my->_trx_to_contract_iddb.store(tid, cid);
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_store_trxid_by_contractid(const ContractIdType & cid, const ContractTrxEntry & tid) {
            my->_contract_to_trx_iddb.store(cid, tid);
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_erase_trxid_by_contract_id(const ContractIdType &id) {
            my->_contract_to_trx_iddb.remove(id);
        }
        
        void thinkyoung::blockchain::ChainDatabase::contract_erase_contractid_by_trxid(const TransactionIdType & tid) {
            my->_trx_to_contract_iddb.remove(tid);
        }
        
        vector<ContractIdType> ChainDatabase::get_all_contract_entries() const {
            vector<ContractIdType> vec_contract;
            
            for (auto iter = my->_contract_id_to_entry.unordered_begin(); iter != my->_contract_id_to_entry.unordered_end(); ++iter) {
                vec_contract.push_back(iter->first);
            }
            
            return vec_contract;
        }
        
    }
} // thinkyoung::blockchain
