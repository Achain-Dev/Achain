//                                   _           _    __ _ _      
//                                  | |         | |  / _(_) |     
//    __ _  ___ _ __   ___ _ __ __ _| |_ ___  __| | | |_ _| | ___ 
//   / _` |/ _ \ '_ \ / _ \ '__/ _` | __/ _ \/ _` | |  _| | |/ _ \`
//  | (_| |  __/ | | |  __/ | | (_| | ||  __/ (_| | | | | | |  __/
//   \__, |\___|_| |_|\___|_|  \__,_|\__\___|\__,_| |_| |_|_|\___|
//    __/ |                                                       
//   |___/                                                        
//
//
// Warning: this is a generated file, any changes made here will be
//          overwritten by the build process.  If you need to change what is
//          generated here, you should either modify the input json files
//          (network_api.json, wallet_api.json, etc) or modify the code
//          generator (thinkyoung_api_generator.cpp) itself
//
#pragma once

#include <api/ApiMetadata.hpp>
#include <blockchain/AccountEntry.hpp>
#include <blockchain/Address.hpp>
#include <blockchain/AssetEntry.hpp>
#include <blockchain/Block.hpp>
#include <blockchain/BlockEntry.hpp>
#include <blockchain/ChainDatabase.hpp>
#include <blockchain/Operations.hpp>
#include <blockchain/PtsAddress.hpp>
#include <blockchain/Transaction.hpp>
#include <blockchain/Types.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>
#include <fc/network/ip.hpp>
#include <fc/optional.hpp>
#include <fc/time.hpp>
#include <net/Node.hpp>
#include <stdint.h>
#include <string>
#include <wallet/Pretty.hpp>
#include <wallet/TransactionBuilder.hpp>
#include <wallet/Wallet.hpp>
#include <wallet/WalletEntrys.hpp>
namespace thinkyoung {
    namespace api {

        class CommonApi
        {
        public:
            /**
             * Returns current blockchain information and parameters.
             *
             * @return json_object
             */
            virtual fc::variant_object blockchain_get_info() const = 0;
            /**
             * Save snapshot of current base asset balances to specified file.
             *
             * @param filename filename to save snapshot to (string, required)
             */
            virtual void blockchain_generate_snapshot(const std::string& filename) const = 0;
            /**
             * fetch all AlpTrxidBalance from block where blocknum of the block lower than block_num or equal to
             * block_num.
             *
             * @param block_num (uint32_t, required)
             *
             * @return vector<alp_trxid_balance>
             */
            virtual std::vector<thinkyoung::blockchain::AlpTrxidBalance> blockchain_get_alp_account_balance_entry(uint32_t block_num) = 0;
            /**
             * A utility to help verify UIA distribution. Returns a snapshot map of all issuances for a particular UIA.
             *
             * @param symbol the UIA for which to compute issuance map (string, required)
             * @param filename filename to save snapshot to (string, required)
             */
            virtual void blockchain_generate_issuance_map(const std::string& symbol, const std::string& filename) const = 0;
            /**
             * Calculate the total supply of an asset from the current blockchain database state.
             *
             * @param asset asset ticker symbol or ID to calculate supply for (string, required)
             *
             * @return asset
             */
            virtual thinkyoung::blockchain::Asset blockchain_calculate_supply(const std::string& asset) const = 0;
            /**
             * Returns true if the local blockchain is synced with the network; false otherwise.
             *
             * @return bool
             */
            virtual bool blockchain_is_synced() const = 0;
            /**
             * Returns the current head block number.
             *
             * @return uint32_t
             */
            virtual uint32_t blockchain_get_block_count() const = 0;
            /**
             * Returns information about blockchain security level.
             *
             * @return blockchain_security_state
             */
            virtual thinkyoung::blockchain::BlockchainSecurityState blockchain_get_security_state() const = 0;
            /**
             * Returns registered accounts starting with a given name upto a the limit provided.
             *
             * @param first_account_name the first account name to include (account_name, optional, defaults to "")
             * @param limit the maximum number of items to list (uint32_t, optional, defaults to 20)
             *
             * @return account_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AccountEntry> blockchain_list_accounts(const std::string& first_account_name = fc::json::from_string("\"\"").as<std::string>(), uint32_t limit = fc::json::from_string("20").as<uint32_t>()) const = 0;
            /**
             * Returns a list of recently updated accounts.
             *
             * @return account_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AccountEntry> blockchain_list_recently_updated_accounts() const = 0;
            /**
             * Returns a list of recently registered accounts.
             *
             * @return account_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AccountEntry> blockchain_list_recently_registered_accounts() const = 0;
            /**
             * Returns registered assets starting with a given name upto a the limit provided.
             *
             * @param first_symbol the prefix of the first asset symbol name to include (asset_symbol, optional,
             *                     defaults to "")
             * @param limit the maximum number of items to list (uint32_t, optional, defaults to 20)
             *
             * @return asset_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AssetEntry> blockchain_list_assets(const std::string& first_symbol = fc::json::from_string("\"\"").as<std::string>(), uint32_t limit = fc::json::from_string("20").as<uint32_t>()) const = 0;
            /**
             * Return a list of transactions that are not yet in a block.
             *
             * @return signed_transaction_pair_array
             */
            virtual std::vector<std::pair<thinkyoung::blockchain::TransactionIdType, thinkyoung::blockchain::SignedTransaction>> blockchain_list_pending_transactions() const = 0;
            /**
             * Get detailed information about an in-wallet transaction.
             *
             * @param transaction_id_prefix the base58 transaction ID to return (string, required)
             * @param exact whether or not a partial match is ok (bool, optional, defaults to false)
             *
             * @return transaction_entry_pair
             */
            virtual std::pair<thinkyoung::blockchain::TransactionIdType, thinkyoung::blockchain::TransactionEntry> blockchain_get_transaction(const std::string& transaction_id_prefix, bool exact = fc::json::from_string("false").as<bool>()) const = 0;
            /**
             * Get pretty information about an in-wallet transaction.
             *
             * @param transaction_id_prefix the base16 transaction ID to return (string, required)
             * @param exact whether or not a partial match is ok (bool, optional, defaults to false)
             *
             * @return pretty_transaction
             */
            virtual thinkyoung::wallet::PrettyTransaction blockchain_get_pretty_transaction(const std::string& transaction_id_prefix, bool exact = fc::json::from_string("false").as<bool>()) const = 0;
            /**
             * Get pretty information about an in-wallet contract transaction.
             *
             * @param transaction_id_prefix the base16 transaction ID to return (string, required)
             * @param exact whether or not a partial match is ok (bool, optional, defaults to false)
             *
             * @return pretty_contract_transaction
             */
            virtual thinkyoung::wallet::PrettyContractTransaction blockchain_get_pretty_contract_transaction(const std::string& transaction_id_prefix, bool exact = fc::json::from_string("false").as<bool>()) const = 0;
            /**
             * Get pretty information about an call-contract result.
             *
             * @param result_id the call-contract ID (string, required)
             *
             * @return contract_trx_info
             */
            virtual thinkyoung::blockchain::ContractTrxInfo blockchain_get_contract_result(const std::string& result_id) const = 0;
            /**
             * Retrieves the block entry for the given block number or ID.
             *
             * @param block block number or ID to retrieve (string, required)
             *
             * @return oblock_entry
             */
            virtual fc::optional<thinkyoung::blockchain::BlockEntry> blockchain_get_block(const std::string& block) const = 0;
            /**
             * Retrieves the detailed transaction information for a block.
             *
             * @param block the number or id of the block to get transactions from (string, required)
             *
             * @return transaction_entry_map
             */
            virtual std::map<thinkyoung::blockchain::TransactionIdType, thinkyoung::blockchain::TransactionEntry> blockchain_get_block_transactions(const std::string& block) const = 0;
            /**
             * Retrieves the entry for the given account name or ID.
             *
             * @param account account name, ID, or public key to retrieve the entry for (string, required)
             *
             * @return optional_account_entry
             */
            virtual fc::optional<thinkyoung::blockchain::AccountEntry> blockchain_get_account(const std::string& account) const = 0;
            /**
             * Retrieves a map of delegate IDs and names defined by the given slate ID or recommending account.
             *
             * @param slate slate ID or recommending account name for which to retrieve the slate of delegates (string,
             *              required)
             *
             * @return map<account_id_type, string>
             */
            virtual std::map<thinkyoung::blockchain::AccountIdType, std::string> blockchain_get_slate(const std::string& slate) const = 0;
            /**
             * Retrieves the specified balance entry.
             *
             * @param balance_id the ID of the balance entry (address, required)
             *
             * @return balance_entry
             */
            virtual thinkyoung::blockchain::BalanceEntry blockchain_get_balance(const thinkyoung::blockchain::Address& balance_id) const = 0;
            /**
             * Lists balance entrys starting at the given balance ID.
             *
             * @param first_balance_id the first balance id to start at (string, optional, defaults to "")
             * @param limit the maximum number of items to list (uint32_t, optional, defaults to 20)
             *
             * @return balance_entry_map
             */
            virtual std::unordered_map<thinkyoung::blockchain::BalanceIdType, thinkyoung::blockchain::BalanceEntry> blockchain_list_balances(const std::string& first_balance_id = fc::json::from_string("\"\"").as<std::string>(), uint32_t limit = fc::json::from_string("20").as<uint32_t>()) const = 0;
            /**
             * Lists balance entrys which are the balance IDs or which can be claimed by signature for this address.
             *
             * @param addr address to scan for (string, required)
             * @param chanced_since Filter all balances that haven't chanced since the provided timestamp (timestamp,
             *                      optional, defaults to "1970-1-1T00:00:01")
             *
             * @return balance_entry_map
             */
            virtual std::unordered_map<thinkyoung::blockchain::BalanceIdType, thinkyoung::blockchain::BalanceEntry> blockchain_list_address_balances(const std::string& addr, const fc::time_point& chanced_since = fc::json::from_string("\"1970-1-1T00:00:01\"").as<fc::time_point>()) const = 0;
            /**
             * Lists all transactions that involve the provided address after the specified time.
             *
             * @param addr address to scan for (string, required)
             * @param filter_before Filter all transactions that occured prior to the specified block number (uint32_t,
             *                      optional, defaults to "0")
             *
             * @return variant_object
             */
            virtual fc::variant_object blockchain_list_address_transactions(const std::string& addr, uint32_t filter_before = fc::json::from_string("\"0\"").as<uint32_t>()) const = 0;
            /**
             * Get the account entry for a given name.
             *
             * @param account_name the name of the account whose public address you want (account_name, required)
             *
             * @return account_balance_summary_type
             */
            virtual thinkyoung::wallet::AccountBalanceSummaryType blockchain_get_account_public_balance(const std::string& account_name) const = 0;
            /**
             * Lists balance entrys which can be claimed by signature for this key.
             *
             * @param key Key to scan for (public_key, required)
             *
             * @return balance_entry_map
             */
            virtual std::unordered_map<thinkyoung::blockchain::BalanceIdType, thinkyoung::blockchain::BalanceEntry> blockchain_list_key_balances(const thinkyoung::blockchain::PublicKeyType& key) const = 0;
            /**
             * Retrieves the entry for the given asset ticker symbol or ID.
             *
             * @param asset asset ticker symbol or ID to retrieve (string, required)
             *
             * @return optional_asset_entry
             */
            virtual fc::optional<thinkyoung::blockchain::AssetEntry> blockchain_get_asset(const std::string& asset) const = 0;
            /**
             * Returns a list of the current round's active delegates in signing order.
             *
             * @param first (uint32_t, optional, defaults to 0)
             * @param count (uint32_t, optional, defaults to 20)
             *
             * @return account_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AccountEntry> blockchain_list_active_delegates(uint32_t first = fc::json::from_string("0").as<uint32_t>(), uint32_t count = fc::json::from_string("20").as<uint32_t>()) const = 0;
            /**
             * Returns a list of all the delegates sorted by vote.
             *
             * @param first (uint32_t, optional, defaults to 0)
             * @param count (uint32_t, optional, defaults to 20)
             *
             * @return account_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AccountEntry> blockchain_list_delegates(uint32_t first = fc::json::from_string("0").as<uint32_t>(), uint32_t count = fc::json::from_string("20").as<uint32_t>()) const = 0;
            /**
             * Returns a descending list of block entrys starting from the specified block number.
             *
             * @param max_block_num the block num to start from; negative to count backwards from the current head
             *                      block (uint32_t, optional, defaults to -1)
             * @param limit max number of blocks to return (uint32_t, optional, defaults to 20)
             *
             * @return block_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::BlockEntry> blockchain_list_blocks(uint32_t max_block_num = fc::json::from_string("-1").as<uint32_t>(), uint32_t limit = fc::json::from_string("20").as<uint32_t>()) = 0;
            /**
             * Returns any delegates who were supposed to produce a given block number but didn't.
             *
             * @param block_number The block to examine (uint32_t, required)
             *
             * @return account_name_array
             */
            virtual std::vector<std::string> blockchain_list_missing_block_delegates(uint32_t block_number) = 0;
            /**
             * dumps the fork data to graphviz format.
             *
             * @param start_block the first block number to consider (uint32_t, optional, defaults to 1)
             * @param end_block the last block number to consider (uint32_t, optional, defaults to -1)
             * @param filename the filename to save to (path, optional, defaults to "")
             *
             * @return string
             */
            virtual std::string blockchain_export_fork_graph(uint32_t start_block = fc::json::from_string("1").as<uint32_t>(), uint32_t end_block = fc::json::from_string("-1").as<uint32_t>(), const thinkyoung::blockchain::FilePath& filename = fc::json::from_string("\"\"").as<thinkyoung::blockchain::FilePath>()) const = 0;
            /**
             * returns a list of all blocks for which there is a fork off of the main chain.
             *
             * @return map<uint32_t, vector<fork_entry>>
             */
            virtual std::map<uint32_t, std::vector<thinkyoung::blockchain::ForkEntry>> blockchain_list_forks() const = 0;
            /**
             * Query the most recent block production slot entrys for the specified delegate.
             *
             * @param delegate_name Delegate whose block production slot entrys to query (string, required)
             * @param limit The maximum number of slot entrys to return (uint32_t, optional, defaults to "10")
             *
             * @return slot_entrys_list
             */
            virtual std::vector<thinkyoung::blockchain::SlotEntry> blockchain_get_delegate_slot_entrys(const std::string& delegate_name, uint32_t limit = fc::json::from_string("\"10\"").as<uint32_t>()) const = 0;
            /**
             * Get the delegate that signed a given block.
             *
             * @param block block number or ID to retrieve the signee for (string, required)
             *
             * @return string
             */
            virtual std::string blockchain_get_block_signee(const std::string& block) const = 0;
            /**
             * Returns the total shares in the genesis block which have never been fully or partially claimed.
             *
             * @return asset
             */
            virtual thinkyoung::blockchain::Asset blockchain_unclaimed_genesis() const = 0;
            /**
             * Verify that the given signature proves the given hash was signed by the given account.
             *
             * @param signer A public key, address, or account name whose signature to check (string, required)
             * @param hash The hash the signature claims to be for (sha256, required)
             * @param signature A signature produced by wallet_sign_hash (compact_signature, required)
             *
             * @return bool
             */
            virtual bool blockchain_verify_signature(const std::string& signer, const fc::sha256& hash, const fc::ecc::compact_signature& signature) const = 0;
            /**
             * TODO.
             *
             * @param path the directory to dump the state into (string, required)
             */
            virtual void blockchain_dump_state(const std::string& path) const = 0;
            /**
             * Takes a signed transaction and broadcasts it to the network.
             *
             * @param trx The transaction to broadcast (signed_transaction, required)
             */
            virtual void blockchain_broadcast_transaction(const thinkyoung::blockchain::SignedTransaction& trx) = 0;
            /**
             * Convert bitcoin address file to thinkyoung address file.
             *
             * @param path The bitcoin address file path. (string, required)
             */
            virtual void blockchain_btc_address_convert(const std::string& path) const = 0;
            /**
             * Get detailed information about an in-wallet transaction.
             *
             * @param transaction_id_prefix the base58 transaction ID to return (string, required)
             * @param exact whether or not a partial match is ok (bool, optional, defaults to false)
             *
             * @return string
             */
            virtual std::string blockchain_get_transaction_rpc(const std::string& transaction_id_prefix, bool exact = fc::json::from_string("false").as<bool>()) const = 0;
            /**
             * set the vm enabled flag of normal node, no effect for the delegate.
             *
             * @param enabled vm enabled flag (bool, required)
             */
            virtual void blockchain_set_node_vm_enabled(bool enabled) = 0;
            /**
             * get the vm enabled flag of normal node.
             *
             * @return bool
             */
            virtual bool blockchain_get_node_vm_enabled() const = 0;
            /**
             * get all contracts on the chain.
             *
             * @return contract_id_list
             */
            virtual vector<string> blockchain_get_all_contracts() const = 0;
            /**
             * get all forever contracts on the chain.
             *
             * @return contract_id_name_map
             */
            virtual unordered_map<string, string> blockchain_get_forever_contracts() const = 0;
            /**
             * list all address converted by public.
             *
             * @param pub_key public key need to list all addresses (string, required)
             *
             * @return string_array
             */
            virtual std::vector<std::string> blockchain_list_pub_all_address(const std::string& pub_key) const = 0;
            /**
             * get event operation of the trx_id.
             *
             * @param block_number The block to examine (uint32_t, required)
             * @param trx_id specific contract id (transaction_id, required)
             *
             * @return eventoperation_array
             */
            virtual std::vector<thinkyoung::blockchain::EventOperation> blockchain_get_events(uint32_t block_number, const thinkyoung::blockchain::TransactionIdType& trx_id) const = 0;
            /**
             * Get a transaction id.
             *
             * @param transaction_to_broadcast The transaction to broadcast to the network (signed_transaction,
             *                                 required)
             *
             * @return transaction_id
             */
            virtual thinkyoung::blockchain::TransactionIdType blockchain_get_transaction_id(const thinkyoung::blockchain::SignedTransaction& transaction_to_broadcast) = 0;
            /**
             * Attempts add or remove <node> from the peer list or try a connection to <node> once.
             *
             * @param node The node (see network_get_peer_info for nodes) (string, required)
             * @param command 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try
             *                a connection to the node once (string, optional, defaults to "add")
             */
            virtual void network_add_node(const std::string& node, const std::string& command = fc::json::from_string("\"add\"").as<std::string>()) = 0;
            /**
             * Returns the number of fully-established connections to other nodes.
             *
             * @return uint32_t
             */
            virtual uint32_t network_get_connection_count() const = 0;
            /**
             * Returns data about each connected node.
             *
             * @param not_firewalled true to output only peers not behind a firewall and false otherwise (bool,
             *                       optional, defaults to false)
             *
             * @return json_object_array
             */
            virtual std::vector<fc::variant_object> network_get_peer_info(bool not_firewalled = fc::json::from_string("false").as<bool>()) const = 0;
            /**
             * Broadcast a previously-created signed transaction to the network.
             *
             * @param transaction_to_broadcast The transaction to broadcast to the network (signed_transaction,
             *                                 required)
             *
             * @return transaction_id
             */
            virtual thinkyoung::blockchain::TransactionIdType network_broadcast_transaction(const thinkyoung::blockchain::SignedTransaction& transaction_to_broadcast) = 0;
            /**
             * Sets advanced node parameters, used for setting up automated tests.
             *
             * @param params A JSON object containing the name/value pairs for the parameters to set (json_object,
             *               required)
             */
            virtual void network_set_advanced_node_parameters(const fc::variant_object& params) = 0;
            /**
             * Sets advanced node parameters, used for setting up automated tests.
             *
             * @return json_object
             */
            virtual fc::variant_object network_get_advanced_node_parameters() const = 0;
            /**
             * Returns the time the transaction was first seen by this client.
             *
             * This interrogates the p2p node's message cache to find out when it first saw this transaction. The data
             * in the message cache is only kept for a few blocks, so you can only use this to ask about recent
             * transactions. This is intended to be used to track message propagation delays in our test network.
             *
             * @param transaction_id the id of the transaction (transaction_id, required)
             *
             * @return message_propagation_data
             */
            virtual thinkyoung::net::MessagePropagationData network_get_transaction_propagation_data(const thinkyoung::blockchain::TransactionIdType& transaction_id) = 0;
            /**
             * Returns the time the block was first seen by this client.
             *
             * This interrogates the p2p node's message cache to find out when it first saw this block. The data in the
             * message cache is only kept for a few blocks, so you can only use this to ask about recent transactions.
             * This is intended to be used to track message propagation delays in our test network.
             *
             * @param block_hash the id of the block (block_id_type, required)
             *
             * @return message_propagation_data
             */
            virtual thinkyoung::net::MessagePropagationData network_get_block_propagation_data(const thinkyoung::blockchain::BlockIdType& block_hash) = 0;
            /**
             * Returns assorted information about the network settings and connections.
             *
             * @return json_object
             */
            virtual fc::variant_object network_get_info() const = 0;
            /**
             * Returns list of potential peers.
             *
             * @return potential_peer_entry_array
             */
            virtual std::vector<thinkyoung::net::PotentialPeerEntry> network_list_potential_peers() const = 0;
            /**
             * Get information on UPNP status including whether it's enabled and what the client believes its IP to be.
             *
             * @return json_object
             */
            virtual fc::variant_object network_get_upnp_info() const = 0;
            /**
             * Get list of ips in blacklist.
             *
             * @return block_ip_array
             */
            virtual std::vector<std::string> network_get_blocked_ips() const = 0;
            /**
             * Returns client's debug name specified in config.json.
             *
             * @return string
             */
            virtual std::string debug_get_client_name() const = 0;
            /**
             * Returns current settings used during local block production.
             *
             * @return variant
             */
            virtual fc::variant delegate_get_config() const = 0;
            /**
             * Set minimum network connection count required for block production.
             *
             * @param count minimum network connection count (uint32_t, required)
             */
            virtual void delegate_set_network_min_connection_count(uint32_t count) = 0;
            /**
             * Set maximum number of transactions allowed in a block.
             *
             * @param count maximum transaction count (uint32_t, required)
             */
            virtual void delegate_set_block_max_transaction_count(uint32_t count) = 0;
            /**
             * Set soft max length.
             *
             * @param soft_length soft max length (int64_t, required)
             */
            virtual void delegate_set_soft_max_imessage_length(int64_t soft_length) = 0;
            /**
             * Set fee coe.
             *
             * @param fee_coe imessage fee coe (string, required)
             */
            virtual void delegate_set_imessage_fee_coe(const std::string& fee_coe) = 0;
            /**
             * Set maximum block size allowed.
             *
             * @param size maximum block size in bytes (uint32_t, required)
             */
            virtual void delegate_set_block_max_size(uint32_t size) = 0;
            /**
             * Set maximum transaction size allowed.
             *
             * @param size maximum transaction size in bytes (uint32_t, required)
             */
            virtual void delegate_set_transaction_max_size(uint32_t size) = 0;
            /**
             * Set whether canonical signatures are required.
             *
             * @param required whether canonical signatures are required (bool, required)
             */
            virtual void delegate_set_transaction_canonical_signatures_required(bool required) = 0;
            /**
             * Set minimum transaction fee allowed.
             *
             * @param fee minimum transaction fee in shares (string, required)
             */
            virtual void delegate_set_transaction_min_fee(const std::string& fee) = 0;
            /**
             * Add specified transaction to blacklist.
             *
             * @param id transaction to add to blacklist (transaction_id, required)
             */
            virtual void delegate_blacklist_add_transaction(const thinkyoung::blockchain::TransactionIdType& id) = 0;
            /**
             * Remove specified transaction from blacklist.
             *
             * @param id transaction to remove from blacklist (transaction_id, required)
             */
            virtual void delegate_blacklist_remove_transaction(const thinkyoung::blockchain::TransactionIdType& id) = 0;
            /**
             * Add specified operation to blacklist.
             *
             * @param id operation to add to blacklist (operation_type, required)
             */
            virtual void delegate_blacklist_add_operation(const thinkyoung::blockchain::OperationTypeEnum& id) = 0;
            /**
             * Remove specified operation from blacklist.
             *
             * @param id operation to remove from blacklist (operation_type, required)
             */
            virtual void delegate_blacklist_remove_operation(const thinkyoung::blockchain::OperationTypeEnum& id) = 0;
            /**
             * Extra information about the wallet.
             *
             * @return json_object
             */
            virtual fc::variant_object wallet_get_info() = 0;
            /**
             * Opens the wallet of the given name.
             *
             * @param wallet_name the name of the wallet to open (wallet_name, required)
             */
            virtual void wallet_open(const std::string& wallet_name) = 0;
            /**
             * Creates a wallet with the given name.
             *
             * @param wallet_name name of the wallet to create (wallet_name, required)
             * @param new_passphrase a passphrase for encrypting the wallet (new_passphrase, required)
             * @param brain_key a strong passphrase that will be used to generate all private keys, defaults to a large
             *                  random number (brainkey, optional, defaults to "")
             */
            virtual void wallet_create(const std::string& wallet_name, const std::string& new_passphrase, const std::string& brain_key = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Returns the wallet name passed to wallet_open.
             *
             * @return optional_wallet_name
             */
            virtual fc::optional<std::string> wallet_get_name() const = 0;
            /**
             * Loads the private key into the specified account. Returns which account it was actually imported to.
             *
             * @param wif_key A private key in bitcoin Wallet Import Format (WIF) (wif_private_key, required)
             * @param account_name the name of the account the key should be imported into, if null then the key must
             *                     belong to an active account (account_name, optional, defaults to null)
             * @param create_new_account If true, the wallet will attempt to create a new account for the name provided
             *                           rather than import the key into an existing account (bool, optional, defaults
             *                           to false)
             * @param rescan If true, the wallet will rescan the blockchain looking for transactions that involve this
             *               private key (bool, optional, defaults to false)
             *
             * @return account_name
             */
            virtual std::string wallet_import_private_key(const std::string& wif_key, const std::string& account_name = fc::json::from_string("null").as<std::string>(), bool create_new_account = fc::json::from_string("false").as<bool>(), bool rescan = fc::json::from_string("false").as<bool>()) = 0;
            /**
             * Closes the curent wallet if one is open.
             */
            virtual void wallet_close() = 0;
            /**
             * Exports the current wallet to a JSON file.
             *
             * @param json_filename the full path and filename of JSON file to generate (filename, required)
             */
            virtual void wallet_backup_create(const fc::path& json_filename) const = 0;
            /**
             * Creates a new wallet from an exported JSON file.
             *
             * @param json_filename the full path and filename of JSON wallet to import (filename, required)
             * @param wallet_name name of the wallet to create (wallet_name, required)
             * @param imported_wallet_passphrase passphrase of the imported wallet (passphrase, required)
             */
            virtual void wallet_backup_restore(const fc::path& json_filename, const std::string& wallet_name, const std::string& imported_wallet_passphrase) = 0;
            /**
             * Enables or disables automatic wallet backups.
             *
             * @param enabled true to enable and false to disable (bool, required)
             *
             * @return bool
             */
            virtual bool wallet_set_automatic_backups(bool enabled) = 0;
            /**
             * Set transaction expiration time.
             *
             * @param seconds seconds before new transactions expire (uint32_t, required)
             *
             * @return uint32_t
             */
            virtual uint32_t wallet_set_transaction_expiration_time(uint32_t seconds) = 0;
            /**
             * Lists transaction history for the specified account.
             *
             * @param account_name the name of the account for which the transaction history will be returned, "" for
             *                     all accounts (string, optional, defaults to "")
             * @param asset_symbol only include transactions involving the specified asset, or "" to include all
             *                     (string, optional, defaults to "")
             * @param limit limit the number of returned transactions; negative for most recent and positive for least
             *              recent. 0 does not limit (int32_t, optional, defaults to 0)
             * @param start_block_num the earliest block number to list transactions from; 0 to include all
             *                        transactions starting from genesis (uint32_t, optional, defaults to 0)
             * @param end_block_num the latest block to list transaction from; -1 to include all transactions ending at
             *                      the head block (uint32_t, optional, defaults to -1)
             *
             * @return pretty_transactions
             */
            virtual std::vector<thinkyoung::wallet::PrettyTransaction> wallet_account_transaction_history(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>(), const std::string& asset_symbol = fc::json::from_string("\"\"").as<std::string>(), int32_t limit = fc::json::from_string("0").as<int32_t>(), uint32_t start_block_num = fc::json::from_string("0").as<uint32_t>(), uint32_t end_block_num = fc::json::from_string("-1").as<uint32_t>()) const = 0;
            /**
             * Lists transaction history for the specified account.
             *
             * @param account_name the name of the account for which the transaction history will be returned, "" for
             *                     all accounts (string, optional, defaults to "")
             * @param asset_symbol only include transactions involving the specified asset, or "" to include all
             *                     (string, optional, defaults to "")
             * @param limit limit the number of returned transactions; negative for most recent and positive for least
             *              recent. 0 does not limit (int32_t, optional, defaults to 0)
             * @param transaction_type transaction type (int32_t, optional, defaults to "2")
             *
             * @return pretty_transactions
             */
            virtual std::vector<thinkyoung::wallet::PrettyTransaction> wallet_transaction_history_splite(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>(), const std::string& asset_symbol = fc::json::from_string("\"\"").as<std::string>(), int32_t limit = fc::json::from_string("0").as<int32_t>(), int32_t transaction_type = fc::json::from_string("\"2\"").as<int32_t>()) const = 0;
            /**
             * Lists wallet's balance at the given time.
             *
             * @param time the date and time for which the balance will be computed (timestamp, required)
             * @param account_name the name of the account for which the historic balance will be returned, "" for all
             *                     accounts (string, optional, defaults to "")
             *
             * @return account_balance_summary_type
             */
            virtual thinkyoung::wallet::AccountBalanceSummaryType wallet_account_historic_balance(const fc::time_point& time, const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) const = 0;
            /**
             * Removes the specified transaction entry from your transaction history. USE WITH CAUTION! Rescan cannot
             * reconstruct all transaction details.
             *
             * @param transaction_id the id (or id prefix) of the transaction entry (string, required)
             */
            virtual void wallet_remove_transaction(const std::string& transaction_id) = 0;
            /**
             * Return any errors for your currently pending transactions.
             *
             * @param filename filename to save pending transaction errors to (path, optional, defaults to "")
             *
             * @return map<transaction_id_type, fc::exception>
             */
            virtual std::map<thinkyoung::blockchain::TransactionIdType, fc::exception> wallet_get_pending_transaction_errors(const thinkyoung::blockchain::FilePath& filename = fc::json::from_string("\"\"").as<thinkyoung::blockchain::FilePath>()) const = 0;
            /**
             * Lock the private keys in wallet, disables spending commands until unlocked.
             */
            virtual void wallet_lock() = 0;
            /**
             * Unlock the private keys in the wallet to enable spending operations.
             *
             * @param timeout the number of seconds to keep the wallet unlocked (uint32_t, required)
             * @param passphrase the passphrase for encrypting the wallet (passphrase, required)
             */
            virtual void wallet_unlock(uint32_t timeout, const std::string& passphrase) = 0;
            /**
             * Change the password of the current wallet.
             *
             * This will change the wallet's spending passphrase, please make sure you remember it.
             *
             * @param old_passphrase the old passphrase for this wallet (passphrase, required)
             * @param passphrase the passphrase for encrypting the wallet (passphrase, required)
             */
            virtual void wallet_change_passphrase(const std::string& old_passphrase, const std::string& passphrase) = 0;
            /**
             * check the password of the current wallet.
             *
             * This will check the wallet's spending passphrase.
             *
             * @param passphrase the passphrase to be checking (passphrase, required)
             *
             * @return bool
             */
            virtual bool wallet_check_passphrase(const std::string& passphrase) = 0;
            /**
             * check address is valid address or an acount name.
             *
             * This will check the address.
             *
             * @param address the address/accountname to be checking (string, required) (string, required)
             *
             * @return bool
             */
            virtual bool wallet_check_address(const std::string& address) = 0;
            /**
             * Return a list of wallets in the current data directory.
             *
             * @return wallet_name_array
             */
            virtual std::vector<std::string> wallet_list() const = 0;
            /**
             * Add new account for receiving payments.
             *
             * @param account_name the name you will use to refer to this receive account (account_name, required)
             * @param private_data Extra data to store with this account entry (json_variant, optional, defaults to
             *                     null)
             *
             * @return address
             */
            virtual thinkyoung::blockchain::Address wallet_account_create(const std::string& account_name, const fc::variant& private_data = fc::json::from_string("null").as<fc::variant>()) = 0;
            /**
             * Updates your approval of the specified account.
             *
             * @param account_name the name of the account to set approval for (account_name, required)
             * @param approval 1, 0, or -1 respectively for approve, neutral, or disapprove (int8_t, optional, defaults
             *                 to 1)
             *
             * @return int8_t
             */
            virtual int8_t wallet_account_set_approval(const std::string& account_name, int8_t approval = fc::json::from_string("1").as<int8_t>()) = 0;
            /**
             * Return all approved account entrys.
             *
             * @param approval 1, 0, or -1 respectively for approve, neutral, or disapprove (int8_t, optional, defaults
             *                 to 1)
             *
             * @return account_entry_array
             */
            virtual std::vector<thinkyoung::blockchain::AccountEntry> wallet_get_all_approved_accounts(int8_t approval = fc::json::from_string("1").as<int8_t>()) = 0;
            /**
             * Creates an address which can be used for a simple (non-TITAN) transfer.
             *
             * @param account_name The account name that will own this address (string, required)
             * @param label (string, optional, defaults to "")
             * @param legacy_network_byte If not -1, use this as the network byte for a BTC-style address. (int32_t,
             *                            optional, defaults to -1)
             *
             * @return string
             */
            virtual std::string wallet_address_create(const std::string& account_name, const std::string& label = fc::json::from_string("\"\"").as<std::string>(), int32_t legacy_network_byte = fc::json::from_string("-1").as<int32_t>()) = 0;
            /**
             * Do a simple (non-TITAN) transfer to an address.
             *
             * @param amount_to_transfer the amount of shares to transfer (string, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_address the address or pubkey to transfer to (string, required)
             * @param memo_message a memo to store with the transaction (information, optional, defaults to "")
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_recommended")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_transfer_to_address(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_address, const thinkyoung::blockchain::Imessage& memo_message = fc::json::from_string("\"\"").as<thinkyoung::blockchain::Imessage>(), const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_recommended\"").as<thinkyoung::wallet::VoteStrategy>()) = 0;
            /**
             * Sends given amount to the given account.
             *
             * @param amount_to_transfer the amount of shares to transfer (string, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (sending_account_name, required)
             * @param to_account_name the account to transfer the shares to (receive_account_name, required)
             * @param memo_message a memo to store with the transaction (information, optional, defaults to "")
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_recommended")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_transfer_to_public_account(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_account_name, const thinkyoung::blockchain::Imessage& memo_message = fc::json::from_string("\"\"").as<thinkyoung::blockchain::Imessage>(), const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_recommended\"").as<thinkyoung::wallet::VoteStrategy>()) = 0;
            /**
             *
             * @param amount how much to transfer (string, required)
             * @param symbol which asset (string, required)
             * @param from_address the balance address to withdraw from (address, required)
             * @param to address or account to receive funds (string, required)
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_none")
             * @param sign_and_broadcast (bool, optional, defaults to true)
             * @param builder_path If specified, will write builder here instead of to DATA_DIR/transactions/latest.trx
             *                     (string, optional, defaults to "")
             *
             * @return transaction_builder
             */
            virtual thinkyoung::wallet::TransactionBuilder wallet_withdraw_from_address(const std::string& amount, const std::string& symbol, const thinkyoung::blockchain::Address& from_address, const std::string& to, const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_none\"").as<thinkyoung::wallet::VoteStrategy>(), bool sign_and_broadcast = fc::json::from_string("true").as<bool>(), const std::string& builder_path = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Scans the blockchain history for operations relevant to this wallet.
             *
             * @param start_block_num the first block to scan (uint32_t, optional, defaults to 0)
             * @param limit the maximum number of blocks to scan (uint32_t, optional, defaults to -1)
             */
            virtual void wallet_rescan_blockchain(uint32_t start_block_num = fc::json::from_string("0").as<uint32_t>(), uint32_t limit = fc::json::from_string("-1").as<uint32_t>()) = 0;
            /**
             * Cancel any current scan task.
             */
            virtual void wallet_cancel_scan() = 0;
            /**
             * Queries your wallet for the specified transaction.
             *
             * @param transaction_id the id (or id prefix) of the transaction (string, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_get_transaction(const std::string& transaction_id) = 0;
            /**
             * Scans the specified transaction.
             *
             * @param transaction_id the id (or id prefix) of the transaction (string, required)
             * @param overwrite_existing true to overwrite existing wallet transaction entry and false otherwise (bool,
             *                           optional, defaults to false)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_scan_transaction(const std::string& transaction_id, bool overwrite_existing = fc::json::from_string("false").as<bool>()) = 0;
            /**
             * Rebroadcasts the specified transaction.
             *
             * @param transaction_id the id (or id prefix) of the transaction (string, required)
             */
            virtual void wallet_rebroadcast_transaction(const std::string& transaction_id) = 0;
            /**
             * Updates the data published about a given account.
             *
             * @param account_name the account that will be updated (account_name, required)
             * @param pay_from_account the account from which fees will be paid (account_name, required)
             * @param public_data public data about the account (json_variant, optional, defaults to null)
             * @param delegate_pay_rate -1 for non-delegates; otherwise the percent of delegate pay to accept per
             *                          produced block (uint8_t, optional, defaults to -1)
             * @param account_type titan_account | public_account - public accounts do not receive memos and all
             *                     payments are made to the active key (string, optional, defaults to "titan_account")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_account_register(const std::string& account_name, const std::string& pay_from_account, const fc::variant& public_data = fc::json::from_string("null").as<fc::variant>(), uint8_t delegate_pay_rate = fc::json::from_string("-1").as<uint8_t>(), const std::string& account_type = fc::json::from_string("\"titan_account\"").as<std::string>()) = 0;
            /**
             * Updates the local private data for an account.
             *
             * @param account_name the account that will be updated (account_name, required)
             * @param private_data private data about the account (json_variant, optional, defaults to null)
             */
            virtual void wallet_account_update_private_data(const std::string& account_name, const fc::variant& private_data = fc::json::from_string("null").as<fc::variant>()) = 0;
            /**
             * Updates the data published about a given account.
             *
             * @param account_name the account that will be updated (account_name, required)
             * @param pay_from_account the account from which fees will be paid (account_name, required)
             * @param public_data public data about the account (json_variant, optional, defaults to null)
             * @param delegate_pay_rate -1 for non-delegates; otherwise the percent of delegate pay to accept per
             *                          produced block (uint8_t, optional, defaults to -1)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_account_update_registration(const std::string& account_name, const std::string& pay_from_account, const fc::variant& public_data = fc::json::from_string("null").as<fc::variant>(), uint8_t delegate_pay_rate = fc::json::from_string("-1").as<uint8_t>()) = 0;
            /**
             * Updates the specified account's active key and broadcasts the transaction.
             *
             * @param account_to_update The name of the account to update the active key of. (account_name, required)
             * @param pay_from_account The account from which fees will be paid. (account_name, required)
             * @param new_active_key WIF private key to update active key to. If empty, a new key will be generated.
             *                       (string, optional, defaults to "")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_account_update_active_key(const std::string& account_to_update, const std::string& pay_from_account, const std::string& new_active_key = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Lists all accounts associated with this wallet.
             *
             * @return wallet_account_entry_array
             */
            virtual std::vector<thinkyoung::wallet::WalletAccountEntry> wallet_list_accounts() const = 0;
            /**
             * Lists all unregistered accounts belonging to this wallet.
             *
             * @return wallet_account_entry_array
             */
            virtual std::vector<thinkyoung::wallet::WalletAccountEntry> wallet_list_unregistered_accounts() const = 0;
            /**
             * Lists all accounts for which we have a private key in this wallet.
             *
             * @return wallet_account_entry_array
             */
            virtual std::vector<thinkyoung::wallet::WalletAccountEntry> wallet_list_my_accounts() const = 0;
            /**
             * Lists all accounts and account addresses for which we have a private key in this wallet.
             *
             * @return account_address_entry_array
             */
            virtual std::vector<thinkyoung::wallet::AccountAddressData> wallet_list_my_addresses() const = 0;
            /**
             * Get the account entry for a given name.
             *
             * @param account_name the name of the account to retrieve (account_name, required)
             *
             * @return wallet_account_entry
             */
            virtual thinkyoung::wallet::WalletAccountEntry wallet_get_account(const std::string& account_name) const = 0;
            /**
             * Get the account entry for a given name.
             *
             * @param account_name the name of the account whose public address you want (account_name, required)
             *
             * @return string
             */
            virtual std::string wallet_get_account_public_address(const std::string& account_name) const = 0;
            /**
             * Remove a contact account from your wallet.
             *
             * @param account_name the name of the contact (account_name, required)
             */
            virtual void wallet_remove_contact_account(const std::string& account_name) = 0;
            /**
             * Rename an account in wallet.
             *
             * @param current_account_name the current name of the account (account_name, required)
             * @param new_account_name the new name for the account (new_account_name, required)
             */
            virtual void wallet_account_rename(const std::string& current_account_name, const std::string& new_account_name) = 0;
            /**
             * Creates a new user issued asset.
             *
             * @param symbol the ticker symbol for the new asset (asset_symbol, required)
             * @param asset_name the name of the asset (string, required)
             * @param issuer_name the name of the issuer of the asset (string, required)
             * @param description a description of the asset (string, required)
             * @param maximum_share_supply the maximum number of shares of the asset (string, required)
             * @param precision defines where the decimal should be displayed, must be a power of 10 (uint64_t,
             *                  required)
             * @param public_data arbitrary data attached to the asset (json_variant, optional, defaults to null)
             * @param is_market_issued creation of a new BitAsset that is created by shorting (bool, optional, defaults
             *                         to false)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_asset_create(const std::string& symbol, const std::string& asset_name, const std::string& issuer_name, const std::string& description, const std::string& maximum_share_supply, uint64_t precision, const fc::variant& public_data = fc::json::from_string("null").as<fc::variant>(), bool is_market_issued = fc::json::from_string("false").as<bool>()) = 0;
            /**
             * Issues new shares of a given asset type.
             *
             * The asset being issued must have been created via wallet_asset_create in a previous block.
             *
             * @param amount the amount of shares to issue (string, required)
             * @param symbol the ticker symbol for asset (asset_symbol, required)
             * @param to_account_name the name of the account to receive the shares (account_name, required)
             * @param memo_message the memo to send to the receiver (information, optional, defaults to "")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_asset_issue(const std::string& amount, const std::string& symbol, const std::string& to_account_name, const thinkyoung::blockchain::Imessage& memo_message = fc::json::from_string("\"\"").as<thinkyoung::blockchain::Imessage>()) = 0;
            /**
             * Issues new UIA shares to specific addresses.
             *
             * This is intended to be used with a helper script to break up snapshots. It will not do any magic for
             * you.
             *
             * @param symbol the ticker symbol for asset (asset_symbol, required)
             * @param addresses A map of addresses-to-amounts to transfer the new shares to (snapshot_map, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_asset_issue_to_addresses(const std::string& symbol, const std::map<std::string, thinkyoung::blockchain::ShareType>& addresses) = 0;
            /**
             * Lists the total asset balances for the specified account.
             *
             * @param account_name the account to get a balance for, or leave empty for all accounts (account_name,
             *                     optional, defaults to "")
             *
             * @return account_balance_summary_type
             */
            virtual thinkyoung::wallet::AccountBalanceSummaryType wallet_account_balance(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) const = 0;
            /**
             * Lists the balance IDs for the specified account.
             *
             * @param account_name the account to get a balance IDs for, or leave empty for all accounts (account_name,
             *                     optional, defaults to "")
             *
             * @return account_balance_id_summary_type
             */
            virtual thinkyoung::wallet::AccountBalanceIdSummaryType wallet_account_balance_ids(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) const = 0;
            /**
             * Lists all public keys in this account.
             *
             * @param account_name the account for which public keys should be listed (account_name, required)
             *
             * @return public_key_summary_array
             */
            virtual std::vector<thinkyoung::wallet::PublicKeySummary> wallet_account_list_public_keys(const std::string& account_name) = 0;
            /**
             * Used to transfer some of the delegate's pay from their balance.
             *
             * @param delegate_name the delegate whose pay is being cashed out (account_name, required)
             * @param to_account_name the account that should receive the funds (account_name, required)
             * @param amount_to_withdraw the amount to withdraw (string, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_delegate_withdraw_pay(const std::string& delegate_name, const std::string& to_account_name, const std::string& amount_to_withdraw) = 0;
            /**
             * query delegate's pay balance.
             *
             * @param delegate_name the delegate whose pay is being cashed out (account_name, required)
             *
             * @return delegate_salarys
             */
            virtual thinkyoung::blockchain::DelegatePaySalary wallet_delegate_pay_balance_query(const std::string& delegate_name) = 0;
            /**
             * query delegate's pay balance.
             *
             * @return delegate_salary_map
             */
            virtual std::map<std::string,thinkyoung::blockchain::DelegatePaySalary> wallet_active_delegate_salary() = 0;
            /**
             * Get delegate produce block statue.
             *
             * @param account_name delegate account name to get statue (string, required)
             *
             * @return bool
             */
            virtual bool wallet_get_delegate_statue(const std::string& account_name) = 0;
            /**
             * Set imessage fee coefficient.
             *
             * @param fee_coe fee coefficient (string, required)
             */
            virtual void wallet_set_transaction_imessage_fee_coe(const std::string& fee_coe) = 0;
            /**
             * Get imessage fee coefficient.
             *
             * @return real_amount
             */
            virtual double wallet_get_transaction_imessage_fee_coe() = 0;
            /**
             * Set imessage soft max length.
             *
             * @param soft_length max soft length (amount, required)
             */
            virtual void wallet_set_transaction_imessage_soft_max_length(int64_t soft_length) = 0;
            /**
             * Get soft max length.
             *
             * @return amount
             */
            virtual int64_t wallet_get_transaction_imessage_soft_max_length() = 0;
            /**
             * Set the fee to add to new transactions.
             *
             * @param fee the wallet transaction fee to set (string, required)
             *
             * @return asset
             */
            virtual thinkyoung::blockchain::Asset wallet_set_transaction_fee(const std::string& fee) = 0;
            /**
             * Returns .
             *
             * @param symbol the wallet transaction if paid in the given asset type (asset_symbol, optional, defaults
             *               to "")
             *
             * @return asset
             */
            virtual thinkyoung::blockchain::Asset wallet_get_transaction_fee(const std::string& symbol = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Reveals the private key corresponding to the specified public key or address; use with caution.
             *
             * @param input public key or address to dump private key for (string, required)
             *
             * @return optional_string
             */
            virtual fc::optional<std::string> wallet_dump_private_key(const std::string& input) const = 0;
            /**
             * Reveals the specified account private key; use with caution.
             *
             * @param account_name account name to dump private key for (string, required)
             * @param key_type which account private key to dump; one of {owner_key, active_key, signing_key}
             *                 (account_key_type, required)
             *
             * @return optional_string
             */
            virtual fc::optional<std::string> wallet_dump_account_private_key(const std::string& account_name, const thinkyoung::wallet::AccountKeyType& key_type) const = 0;
            /**
             * Returns the allocation of votes by this account.
             *
             * @param account_name the account to report votes on, or empty for all accounts (account_name, optional,
             *                     defaults to "")
             *
             * @return account_vote_summary
             */
            virtual thinkyoung::wallet::AccountVoteSummaryType wallet_account_vote_summary(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) const = 0;
            /**
             * Get info about the votes of balances controlled by this account.
             *
             * @param account (account_name, required)
             *
             * @return vote_summary
             */
            virtual thinkyoung::wallet::VoteSummary wallet_check_vote_status(const std::string& account) = 0;
            /**
             * Set a property in the GUI settings DB.
             *
             * @param name the name of the setting to set (string, required)
             * @param value the value to set the setting to (variant, required)
             */
            virtual void wallet_set_setting(const std::string& name, const fc::variant& value) = 0;
            /**
             * Get the value of the given setting.
             *
             * @param name The name of the setting to fetch (string, required)
             *
             * @return optional_variant
             */
            virtual fc::optional<fc::variant> wallet_get_setting(const std::string& name) = 0;
            /**
             * Enable or disable block production for a particular delegate account.
             *
             * @param delegate_name The delegate to enable/disable block production for; ALL for all delegate accounts
             *                      (string, required)
             * @param enabled true to enable block production, false otherwise (bool, required)
             */
            virtual void wallet_delegate_set_block_production(const std::string& delegate_name, bool enabled) = 0;
            /**
             * Enable or disable wallet transaction scanning.
             *
             * @param enabled true to enable transaction scanning, false otherwise (bool, required)
             *
             * @return bool
             */
            virtual bool wallet_set_transaction_scanning(bool enabled) = 0;
            /**
             * Signs the provided message digest with the account key.
             *
             * @param signer A public key, address, or account name whose key to sign with (string, required)
             * @param hash SHA256 digest of the message to sign (sha256, required)
             *
             * @return compact_signature
             */
            virtual fc::ecc::compact_signature wallet_sign_hash(const std::string& signer, const fc::sha256& hash) = 0;
            /**
             * Initiates the login procedure by providing a Alp Login URL.
             *
             * @param server_account Name of the account of the server. The user will be shown this name as the site he
             *                       is logging into. (string, required)
             *
             * @return string
             */
            virtual std::string wallet_login_start(const std::string& server_account) = 0;
            /**
             * Completes the login procedure by finding the user's public account key and shared secret.
             *
             * @param server_key The one-time public key from wallet_login_start. (public_key, required)
             * @param client_key The client's one-time public key. (public_key, required)
             * @param client_signature The client's signature of the shared secret. (compact_signature, required)
             *
             * @return variant
             */
            virtual fc::variant wallet_login_finish(const thinkyoung::blockchain::PublicKeyType& server_key, const thinkyoung::blockchain::PublicKeyType& client_key, const fc::ecc::compact_signature& client_signature) = 0;
            /**
             * Set this balance's voting address and slate.
             *
             * @param balance_id the current name of the account (address, required)
             * @param voter_address The new voting address. If none is specified, tries to re-use existing address.
             *                      (string, optional, defaults to "")
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_all")
             * @param sign_and_broadcast (bool, optional, defaults to "true")
             * @param builder_path If specified, will write builder here instead of to DATA_DIR/transactions/latest.trx
             *                     (string, optional, defaults to "")
             *
             * @return transaction_builder
             */
            virtual thinkyoung::wallet::TransactionBuilder wallet_balance_set_vote_info(const thinkyoung::blockchain::Address& balance_id, const std::string& voter_address = fc::json::from_string("\"\"").as<std::string>(), const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_all\"").as<thinkyoung::wallet::VoteStrategy>(), bool sign_and_broadcast = fc::json::from_string("\"true\"").as<bool>(), const std::string& builder_path = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Publishes the current wallet delegate slate to the public data associated with the account.
             *
             * @param publishing_account_name The account to publish the slate ID under (account_name, required)
             * @param paying_account_name The account to pay transaction fees or leave empty to pay with publishing
             *                            account (account_name, optional, defaults to "")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_publish_slate(const std::string& publishing_account_name, const std::string& paying_account_name = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Publish your current client version to the specified account's public data entry.
             *
             * @param publishing_account_name The account to publish the client version under (account_name, required)
             * @param paying_account_name The account to pay transaction fees with or leave empty to pay with
             *                            publishing account (account_name, optional, defaults to "")
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_publish_version(const std::string& publishing_account_name, const std::string& paying_account_name = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Collect specified account's genesis balances.
             *
             * @param account_name account to collect genesis balances for (account_name, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_collect_genesis_balances(const std::string& account_name) = 0;
            /**
             * Attempts to recover accounts created after last backup was taken and returns number of successful
             * recoveries. Use if you have restored from backup and are missing accounts.
             *
             * @param accounts_to_recover The number of accounts to attept to recover (int32_t, required)
             * @param maximum_number_of_attempts The maximum number of keys to generate trying to recover accounts
             *                                   (int32_t, optional, defaults to 1000)
             *
             * @return int32_t
             */
            virtual int32_t wallet_recover_accounts(int32_t accounts_to_recover, int32_t maximum_number_of_attempts = fc::json::from_string("1000").as<int32_t>()) = 0;
            /**
             * Verify whether the specified transaction made a TITAN deposit to the current wallet; returns null if
             * not.
             *
             * @param transaction_id_prefix the id (or id prefix) of the transaction entry (string, required)
             *
             * @return optional_variant_object
             */
            virtual fc::optional<fc::variant_object> wallet_verify_titan_deposit(const std::string& transaction_id_prefix) = 0;
            /**
             * tries to repair any inconsistent wallet account, key, and transaction entrys.
             *
             * @param collecting_account_name collect any orphan balances into this account (account_name, optional,
             *                                defaults to "")
             */
            virtual void wallet_repair_entrys(const std::string& collecting_account_name = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * regenerates private keys as part of wallet recovery.
             *
             * @param account_name the account the generated keys should be a part of (account_name, required)
             * @param max_key_number the last key number to regenerate (uint32_t, required)
             *
             * @return int32_t
             */
            virtual int32_t wallet_regenerate_keys(const std::string& account_name, uint32_t max_key_number) = 0;
            /**
             * Retract (permanently disable) the specified account in case of master key compromise.
             *
             * @param account_to_retract The name of the account to retract. (account_name, required)
             * @param pay_from_account The account from which fees will be paid. (account_name, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_account_retract(const std::string& account_to_retract, const std::string& pay_from_account) = 0;
            /**
             * delete account from Wallet.
             *
             * @param account_name The name of the account to retract. (string, required)
             *
             * @return bool
             */
            virtual bool wallet_account_delete(const std::string& account_name) = 0;
            /**
             * Do a simple (non-TITAN) transfer to an address.
             *
             * @param amount_to_transfer the amount of shares to transfer (string, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_address the address or pubkey to transfer to (string, required)
             * @param memo_message a memo to store with the transaction (information, optional, defaults to "")
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_recommended")
             *
             * @return string
             */
            virtual std::string wallet_transfer_to_address_rpc(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_address, const thinkyoung::blockchain::Imessage& memo_message = fc::json::from_string("\"\"").as<thinkyoung::blockchain::Imessage>(), const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_recommended\"").as<thinkyoung::wallet::VoteStrategy>()) = 0;
            /**
             * Lists the total asset balances for the specified account.
             *
             * @param account_name the account to get a balance for, or leave empty for all accounts (account_name,
             *                     optional, defaults to "")
             *
             * @return string
             */
            virtual std::string wallet_account_balance_rpc(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) const = 0;
            /**
             * Sends given amount to the given account.
             *
             * @param amount_to_transfer the amount of shares to transfer (string, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (sending_account_name, required)
             * @param to_account_name the account to transfer the shares to (receive_account_name, required)
             * @param memo_message a memo to store with the transaction (information, optional, defaults to "")
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_recommended")
             *
             * @return string
             */
            virtual std::string wallet_transfer_to_public_account_rpc(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_account_name, const thinkyoung::blockchain::Imessage& memo_message = fc::json::from_string("\"\"").as<thinkyoung::blockchain::Imessage>(), const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_recommended\"").as<thinkyoung::wallet::VoteStrategy>()) = 0;
            /**
             * Get owner publickey of specific account.
             *
             * @param account_name the name of the account (account_name, required)
             *
             * @return public_key
             */
            virtual thinkyoung::blockchain::PublicKeyType wallet_get_account_owner_publickey(const std::string& account_name) = 0;
            /**
             * Do a simple transfer to a contract (name or address).
             *
             * @param amount_to_transfer the amount of shares to transfer (real_amount, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_contract the contract name or contract address to transfer to (string, required)
             * @param amount_for_exec the amount of shares to exec on_deposit callback of target contract (real_amount,
             *                        required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry wallet_transfer_to_contract(double amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_contract, double amount_for_exec) = 0;
            /**
             * Do a simple transfer to a contract (name or address) on local endpoint, and do not spread it on P2P
             * network.
             *
             * @param amount_to_transfer the amount of shares to transfer (real_amount, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_contract the contract name or contract address to transfer to (string, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> wallet_transfer_to_contract_testing(double amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_contract) = 0;
            /**
             * get all the contracts belong to the account of the wallet.
             *
             * @param account_name the account in wallet (string, optional, defaults to "")
             *
             * @return contract_id_list
             */
            virtual vector<string> wallet_get_contracts(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * create a simple (non-TITAN) transfer to an address without broadcast.
             *
             * @param amount_to_transfer the amount of shares to transfer (string, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_address the address or pubkey to transfer to (string, required)
             * @param memo_message a memo to store with the transaction (information, optional, defaults to "")
             * @param strategy enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy,
             *                 optional, defaults to "vote_recommended")
             *
             * @return signed_transaction
             */
            virtual thinkyoung::blockchain::SignedTransaction create_transfer_transaction(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_address, const thinkyoung::blockchain::Imessage& memo_message = fc::json::from_string("\"\"").as<thinkyoung::blockchain::Imessage>(), const thinkyoung::wallet::VoteStrategy& strategy = fc::json::from_string("\"vote_recommended\"").as<thinkyoung::wallet::VoteStrategy>()) = 0;
            /**
             * scan to get all the contracts.
             */
            virtual void wallet_scan_contracts() = 0;
            /**
             * Returns version number and associated information for this client.
             *
             * @return json_object
             */
            virtual fc::variant_object about() const = 0;
            /**
             * Returns version number and associated information for this client.
             *
             * @return json_object
             */
            virtual fc::variant_object get_info() const = 0;
            /**
             * shut down the RPC server and exit this client.
             */
            virtual void stop() = 0;
            /**
             * display a list of commands, or detailed help on an individual command.
             *
             * @param command_name the name of the method to get detailed help, or omit this for a list of commands
             *                     (method_name, optional, defaults to "")
             *
             * @return string
             */
            virtual std::string help(const std::string& command_name = fc::json::from_string("\"\"").as<std::string>()) const = 0;
            /**
             * Return information about given Alp address.
             *
             * @param address the address or public key to validate (string, required)
             *
             * @return json_object
             */
            virtual fc::variant_object validate_address(const std::string& address) const = 0;
            /**
             * Execute the given command as if it were typed on the CLI.
             *
             * @param input The entire command input as if it were a CLI input (passphrase, required)
             *
             * @return string
             */
            virtual std::string execute_command_line(const std::string& input) const = 0;
            /**
             * Execute the given file as if it were typed on the CLI.
             *
             * @param script Name of a file containing CLI commands to execute (filename, required)
             */
            virtual void execute_script(const fc::path& script) const = 0;
            /**
             * Takes any no_prerequisites command and an array of its arguments and returns an array of results.
             * Example: batch blockchain_get_blockhash [[1], [2]].
             *
             * @param method_name The command name for calling (string, required)
             * @param parameters_list The list of list of parameters for this command, the return will be the list of
             *                        execute result of corresponding parameters (parameters_list, required)
             *
             * @return variants
             */
            virtual fc::variants batch(const std::string& method_name, const std::vector<fc::variants>& parameters_list) const = 0;
            /**
             * Takes any no_prerequisites command and an array of its arguments and returns an array of results.
             * Example: batch_authenticated blockchain_get_blockhash [[1], [2]].
             *
             * @param method_name The command name for calling (string, required)
             * @param parameters_list The list of list of parameters for this command, the return will be the list of
             *                        execute result of corresponding parameters (parameters_list, required)
             *
             * @return variants
             */
            virtual fc::variants batch_authenticated(const std::string& method_name, const std::vector<fc::variants>& parameters_list) const = 0;
            /**
             * Takes a transaction builder and returns a signed transaction for broadcasting.
             *
             * @param builder (transaction_builder, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry builder_finalize_and_sign(const thinkyoung::wallet::TransactionBuilder& builder) const = 0;
            /**
             * Returns help information as JSON data.
             *
             * @return method_map_type
             */
            virtual std::map<std::string, thinkyoung::api::MethodData> meta_help() const = 0;
            /**
             * Set the username for basic auth for the http server.
             *
             * @param username Username for basic auth (string, optional, defaults to "")
             */
            virtual void rpc_set_username(const std::string& username = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Set the password for basic auth for the http server.
             *
             * @param password Password for basic auth (passphrase, optional, defaults to "")
             */
            virtual void rpc_set_password(const std::string& password = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * Set the port and start rpc server.
             *
             * @param port Port for rpc server (uint32_t, optional, defaults to "65065")
             */
            virtual void rpc_start_server(uint32_t port = fc::json::from_string("\"65065\"").as<uint32_t>()) = 0;
            /**
             * Set the port and start http server.
             *
             * @param port Port for http server (uint32_t, optional, defaults to "65066")
             */
            virtual void http_start_server(uint32_t port = fc::json::from_string("\"65066\"").as<uint32_t>()) = 0;
            /**
             * Update the NTP time right now.
             */
            virtual void ntp_update_time() = 0;
            /**
             * Report disk space taken up by different groups of client files.
             *
             * @return variant
             */
            virtual fc::variant disk_usage() const = 0;
            /**
             * compile contract source code to byte code, and store to binary file.
             *
             * @param filename contract source file name (filename, required)
             *
             * @return filename
             */
            virtual fc::path compile_contract(const fc::path& filename) const = 0;
            /**
             * register a temporary contract.
             *
             * @param owner contract register owner address (string, required)
             * @param codefile contract bytesfile name (filename, required)
             * @param asset_symbol asset symbol name (string, required)
             * @param init_limit the limit of asset amount used to init contract (optional_double, required)
             *
             * @return string
             */
            virtual std::string register_contract(const std::string& owner, const fc::path& codefile, const std::string& asset_symbol, const fc::optional<double>& init_limit) = 0;
            /**
             * register a temporary contract on local endpoint, and do not spread it on P2P network.
             *
             * @param owner contract register owner address (string, required)
             * @param codefile contract bytesfile name (filename, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> register_contract_testing(const std::string& owner, const fc::path& codefile) = 0;
            /**
             * upgrade temporary contract to permanent contract.
             *
             * @param contract_address contract address need to upgrade (string, required)
             * @param upgrader_name upgrader name (string, required)
             * @param new_contract_name permanent contract name (string, required)
             * @param new_contract_desc permanent contract description (information, required)
             * @param asset_symbol symbol of asset used to destroy (string, required)
             * @param exec_limit the limit of asset amount used to destroy contract (optional_double, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry upgrade_contract(const std::string& contract_address, const std::string& upgrader_name, const std::string& new_contract_name, const thinkyoung::blockchain::Imessage& new_contract_desc, const std::string& asset_symbol, const fc::optional<double>& exec_limit) = 0;
            /**
             * test upgrade temporary contract to permanent contract.
             *
             * @param contract_address contract address need to upgrade (string, required)
             * @param upgrader_name upgrader name (string, required)
             * @param new_contract_name permanent contract name (string, required)
             * @param new_contract_desc permanent contract description (information, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> upgrade_contract_testing(const std::string& contract_address, const std::string& upgrader_name, const std::string& new_contract_name, const thinkyoung::blockchain::Imessage& new_contract_desc) = 0;
            /**
             * destroy temporary contract.
             *
             * @param contract_address contract address need to destroy (string, required)
             * @param destroyer_name destroyer name (string, required)
             * @param asset_symbol symbol of asset used to destroy (string, required)
             * @param exec_limit the limit of asset amount used to destroy contract (optional_double, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry destroy_contract(const std::string& contract_address, const std::string& destroyer_name, const std::string& asset_symbol, const fc::optional<double>& exec_limit) = 0;
            /**
             * testing destroy temporary contract.
             *
             * @param contract_address contract address need to destroy (string, required)
             * @param destroyer_name destroyer name (string, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> destroy_contract_testing(const std::string& contract_address, const std::string& destroyer_name) = 0;
            /**
             * call contract by contract name or contract address.
             *
             * @param contract contract name or contract address need to be called (string, required)
             * @param caller_name caller name (string, required)
             * @param function_name function in contract (string, required)
             * @param params parameters which would be passed to function (string, required)
             * @param asset_symbol symbol of asset used to call (string, required)
             * @param call_limit the limit of asset amount used to call contract (optional_double, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry call_contract(const std::string& contract, const std::string& caller_name, const std::string& function_name, const std::string& params, const std::string& asset_symbol, const fc::optional<double>& call_limit) = 0;
            /**
             * get contract entry by contract name or contract address.
             *
             * @param contract contract name or contract address (string, required)
             *
             * @return contract_entry_printable
             */
            virtual thinkyoung::blockchain::ContractEntryPrintable get_contract_info(const std::string& contract) = 0;
            /**
             * get contract balance entry and margin balance entry by contract name or contract address.
             *
             * @param contract contract name or contract address (string, required)
             *
             * @return balance_entry_list
             */
            virtual std::vector<thinkyoung::blockchain::BalanceEntry> get_contract_balance(const std::string& contract) = 0;
            /**
             * call contract function by contract name or contract address on local endpoint, and do not spread it on
             * P2P network.
             *
             * @param contract contract name or contract address need to be called (string, required)
             * @param caller_name caller name (string, required)
             * @param function_name function in contract (string, required)
             * @param params parameters which would be passed to function (string, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> call_contract_testing(const std::string& contract, const std::string& caller_name, const std::string& function_name, const std::string& params) = 0;
            /**
             * call contract function by contract name or contract address on local endpoint, and do not spread it on
             * P2P network.
             *
             * @param contract contract name or contract address need to be called (string, required)
             * @param caller_name caller name (string, required)
             * @param function_name function in contract (string, required)
             * @param params parameters which would be passed to function (string, required)
             *
             * @return eventoperation_array
             */
            virtual std::vector<thinkyoung::blockchain::EventOperation> call_contract_local_emit(const std::string& contract, const std::string& caller_name, const std::string& function_name, const std::string& params) = 0;
            /**
             * call contract offline function by contract name or contract address.
             *
             * @param contract contract name or contract address need to be called (string, required)
             * @param caller_name caller name (string, required)
             * @param function_name offline function in contract (string, required)
             * @param params parameters which would be passed to offline function (string, required)
             *
             * @return string
             */
            virtual std::string call_contract_offline(const std::string& contract, const std::string& caller_name, const std::string& function_name, const std::string& params) = 0;
            /**
             * save contract to file.
             *
             * @param contract contract name or contract address need to be saved (string, required)
             * @param file the file name with full path (filename, required)
             *
             * @return contract_entry_printable
             */
            virtual thinkyoung::blockchain::ContractEntryPrintable load_contract_to_file(const std::string& contract, const fc::path& file) = 0;
            /**
             * get result tranraction id according to request trx id.
             *
             * @param request_id transaction id of request (transaction_id, required)
             *
             * @return transaction_id
             */
            virtual thinkyoung::blockchain::TransactionIdType get_result_trx_id(const thinkyoung::blockchain::TransactionIdType& request_id) = 0;
            /**
             * get request tranraction id according to result trx id.
             *
             * @param request_id transaction id of result (transaction_id, required)
             *
             * @return transaction_id
             */
            virtual thinkyoung::blockchain::TransactionIdType get_request_trx_id(const thinkyoung::blockchain::TransactionIdType& request_id) = 0;
            /**
             * open sandbox, let the state in sandbox related state.
             */
            virtual void sandbox_open() const = 0;
            /**
             * close sandbox, clear sandbox related state.
             */
            virtual void sandbox_close() const = 0;
            /**
             * in sandbox state: register a temporary contract.
             *
             * @param owner contract register owner address (string, required)
             * @param codefile contract bytesfile name (filename, required)
             * @param asset_symbol asset symbol name (string, required)
             * @param initLimit the limit of asset amount used to init contract (optional_double, required)
             *
             * @return string
             */
            virtual std::string sandbox_register_contract(const std::string& owner, const fc::path& codefile, const std::string& asset_symbol, const fc::optional<double>& initLimit) = 0;
            /**
             * in sandbox state: call contract by contract name or contract address.
             *
             * @param contract contract name or contract address need to be called (string, required)
             * @param caller_name destroyer name (string, required)
             * @param function_name function in contract (string, required)
             * @param params parameters which would be passed to function (string, required)
             * @param cost_asset symbol of asset used to call (string, required)
             * @param callLimit the limit of asset amount used to call contract (optional_double, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry sandbox_call_contract(const std::string& contract, const std::string& caller_name, const std::string& function_name, const std::string& params, const std::string& cost_asset, const fc::optional<double>& callLimit) = 0;
            /**
             * in sandbox state: upgrade temporary contract to permanent contract.
             *
             * @param contract_address contract address need to upgrade (string, required)
             * @param upgrader_name upgrader name (string, required)
             * @param new_contract_name permanent contract name (string, required)
             * @param new_contract_desc permanent contract description (information, required)
             * @param asset_symbol symbol of asset used to destroy (string, required)
             * @param exec_limit the limit of asset amount used to destroy contract (optional_double, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry sandbox_upgrade_contract(const std::string& contract_address, const std::string& upgrader_name, const std::string& new_contract_name, const thinkyoung::blockchain::Imessage& new_contract_desc, const std::string& asset_symbol, const fc::optional<double>& exec_limit) = 0;
            /**
             * in sandbox state: upgrade temporary contract to permanent contract.
             *
             * @param contract_address contract address need to upgrade (string, required)
             * @param upgrader_name upgrader name (string, required)
             * @param new_contract_name permanent contract name (string, required)
             * @param new_contract_desc permanent contract description (information, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> sandbox_upgrade_contract_testing(const std::string& contract_address, const std::string& upgrader_name, const std::string& new_contract_name, const thinkyoung::blockchain::Imessage& new_contract_desc) = 0;
            /**
             * in sandbox state: destroy temporary contract.
             *
             * @param contract_address contract address need to destroy (string, required)
             * @param destroyer_name destroyer name (string, required)
             * @param asset_symbol symbol of asset used to destroy (string, required)
             * @param exec_limit the limit of asset amount used to destroy contract (optional_double, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry sandbox_destroy_contract(const std::string& contract_address, const std::string& destroyer_name, const std::string& asset_symbol, const fc::optional<double>& exec_limit) = 0;
            /**
             * in sandbox state: test destroy temporary contract.
             *
             * @param contract_address contract address need to destroy (string, required)
             * @param destroyer_name destroyer name (string, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> sandbox_destroy_contract_testing(const std::string& contract_address, const std::string& destroyer_name) = 0;
            /**
             * in sandbox state: get contract entry by contract name or contract address.
             *
             * @param contract contract name or contract address (string, required)
             *
             * @return contract_entry_printable
             */
            virtual thinkyoung::blockchain::ContractEntryPrintable sandbox_get_contract_info(const std::string& contract) = 0;
            /**
             * in sandbox state: get contract balance entry and margin balance entry by contract name or contract
             * address.
             *
             * @param contract contract name or contract address (string, required)
             *
             * @return balance_entry_list
             */
            virtual std::vector<thinkyoung::blockchain::BalanceEntry> sandbox_get_contract_balance(const std::string& contract) = 0;
            /**
             * in sandbox state: transfer to contract.
             *
             * @param amount_to_transfer the amount of shares to transfer (real_amount, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_contract the contract name or contract address to transfer to (string, required)
             * @param amount_for_exec the amount of shares to exec (real_amount, required)
             *
             * @return transaction_entry
             */
            virtual thinkyoung::wallet::WalletTransactionEntry sandbox_transfer_to_contract(double amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_contract, double amount_for_exec) = 0;
            /**
             * in sandbox state: lookup balance of account.
             *
             * @param account_name the account to lookup balance (account_name, optional, defaults to "")
             *
             * @return account_balance_summary_type
             */
            virtual thinkyoung::wallet::AccountBalanceSummaryType sandbox_account_balance(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * in sandbox compile contract source code to byte code, and store to binary file.
             *
             * @param filename contract source file name (filename, required)
             *
             * @return filename
             */
            virtual fc::path sandbox_compile_contract(const fc::path& filename) const = 0;
            /**
             * in sandbox save contract to a gpc file.
             *
             * @param contract contract name or contract address need to be saved (string, required)
             * @param file the file name with full path (filename, required)
             *
             * @return contract_entry_printable
             */
            virtual thinkyoung::blockchain::ContractEntryPrintable sandbox_load_contract_to_file(const std::string& contract, const fc::path& file) = 0;
            /**
             * in sandbox register a temporary contract on local endpoint, and do not spread it on P2P network.
             *
             * @param owner contract register owner address (string, required)
             * @param codefile contract bytesfile name (filename, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> sandbox_register_contract_testing(const std::string& owner, const fc::path& codefile) = 0;
            /**
             * in sandbox call contract function by contract name or contract address on local endpoint, and do not
             * spread it on P2P network.
             *
             * @param contract contract name or contract address need to be called (string, required)
             * @param caller_name caller name (string, required)
             * @param function_name function in contract (string, required)
             * @param params parameters which would be passed to function (string, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> sandbox_call_contract_testing(const std::string& contract, const std::string& caller_name, const std::string& function_name, const std::string& params) = 0;
            /**
             * in sandbox Do a simple transfer to a contract (name or address) on local endpoint, and do not spread it
             * on P2P network.
             *
             * @param amount_to_transfer the amount of shares to transfer (real_amount, required)
             * @param asset_symbol the asset to transfer (asset_symbol, required)
             * @param from_account_name the source account to draw the shares from (account_name, required)
             * @param to_contract the contract name or contract address to transfer to (string, required)
             *
             * @return asset_array
             */
            virtual std::vector<thinkyoung::blockchain::Asset> sandbox_transfer_to_contract_testing(double amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_contract) = 0;
            /**
             * list the account info of the wallet.
             *
             * @param account_name the account in wallet (string, optional, defaults to "")
             *
             * @return sandbox_account_info_list
             */
            virtual vector<thinkyoung::blockchain::SandboxAccountInfo> sandbox_list_my_addresses(const std::string& account_name = fc::json::from_string("\"\"").as<std::string>()) = 0;
            /**
             * get id of contract registered in specific transaction.
             *
             * @param trx_id specific contract id (transaction_id, required)
             *
             * @return string
             */
            virtual std::string get_contract_registered_in_transaction(const thinkyoung::blockchain::TransactionIdType& trx_id) = 0;
            /**
             * get id of transaction that specific contract registered in .
             *
             * @param contract_id specific contract id (string, required)
             *
             * @return transaction_id
             */
            virtual thinkyoung::blockchain::TransactionIdType get_transaction_id_contract_registered(const std::string& contract_id) = 0;
            /**
             * get contract info from gpc file.
             *
             * @param file contract gpc file name (filename, required)
             *
             * @return code_printable
             */
            virtual thinkyoung::blockchain::CodePrintAble get_contract_info_from_gpc_file(const fc::path& file) = 0;
            /**
             * compile script source code to byte code, and store to binary file.
             *
             * @param filename script source file name (filename, required)
             *
             * @return filename
             */
            virtual fc::path compile_script(const fc::path& filename) const = 0;
            /**
             * Add a compiled script to database.
             *
             * @param filename filename of the specified script (filename, required)
             * @param description description of the script (information, required)
             *
             * @return string
             */
            virtual std::string add_script(const fc::path& filename, const thinkyoung::blockchain::Imessage& description) = 0;
            /**
             * Remove a compiled script form database.
             *
             * @param scriptid scriptid of the specified script (string, required)
             */
            virtual void remove_script(const std::string& scriptid) = 0;
            /**
             * Get info of a script specified by script_id .
             *
             * @param scriptid scriptid of the specified script (string, required)
             *
             * @return scriptentry_printable
             */
            virtual thinkyoung::wallet::ScriptEntryPrintable get_script_info(const std::string& scriptid) = 0;
            /**
             * List all scripts in database .
             *
             * @return scriptentry_printable_list
             */
            virtual std::vector<thinkyoung::wallet::ScriptEntryPrintable> list_scripts() = 0;
            /**
             * disable a compiled script from database.
             *
             * @param scriptid scriptid of the specified script (string, required)
             */
            virtual void disable_script(const std::string& scriptid) = 0;
            /**
             * enable a compiled script from database.
             *
             * @param scriptid scriptid of the specified script (string, required)
             */
            virtual void enable_script(const std::string& scriptid) = 0;
            /**
             * import scripts from a database directory.
             *
             * @param dbfile path of database for importing from (filename, required)
             */
            virtual void import_script_db(const fc::path& dbfile) = 0;
            /**
             * export scripts to a database directory.
             *
             * @param dbfile path of database for exporting to (filename, required)
             */
            virtual void export_script_db(const fc::path& dbfile) = 0;
            /**
             * get contract id and event which binded with specific script.
             *
             * @param script_id id of specific script (string, required)
             *
             * @return string_array
             */
            virtual std::vector<std::string> get_events_bound(const std::string& script_id) = 0;
            /**
             * list all event handlers related with the specified contract id and event type.
             *
             * @param contract_id_str id of the binding contract at blockchain (string, required)
             * @param event_type type of the binding event at blockchain (string, required)
             *
             * @return scriptid_list
             */
            virtual std::vector<std::string> list_event_handler(const std::string& contract_id_str, const std::string& event_type) = 0;
            /**
             * set a new relation with a script to the specified contract id and event type.
             *
             * @param contract_id_str id of contract at blockchain want to bind (string, required)
             * @param event_type type of event at blockchain want to bind (string, required)
             * @param script_id id of local script want to bind with (string, required)
             * @param index the position of script vector want to insert (uint32_t, required)
             */
            virtual void add_event_handler(const std::string& contract_id_str, const std::string& event_type, const std::string& script_id, uint32_t index) = 0;
            /**
             * drop the relation with a script to the specified contract id and event type.
             *
             * @param contract_id_str id of contract at blockchain want to delete (string, required)
             * @param event_type type of event at blockchain want to delete (string, required)
             * @param script_id id of local script want to delete with (string, required)
             */
            virtual void delete_event_handler(const std::string& contract_id_str, const std::string& event_type, const std::string& script_id) = 0;
        };

    }
} // end namespace thinkyoung::api
