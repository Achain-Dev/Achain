#pragma once

#include <blockchain/ChainDatabase.hpp>
#include <blockchain/TransactionCreationState.hpp>
#include <wallet/Pretty.hpp>
#include <wallet/TransactionBuilder.hpp>
#include <wallet/WalletDb.hpp>

#include <fc/signals.hpp>
#define INVALIDE_SUB_ADDRESS ("ffffffffffffffffffffffffffffffff")
namespace thinkyoung {
    namespace wallet {
    
        using namespace thinkyoung::blockchain;
        
        namespace detail {
            class WalletImpl;
        }
        
        typedef map<string, vector<BalanceEntry>> AccountBalanceEntrySummaryType;
        typedef map<string, unordered_set<BalanceIdType>> AccountBalanceIdSummaryType;
        typedef map<string, map<AssetIdType, ShareType>> AccountBalanceSummaryType;
        typedef map<string, map<string, vector<Asset>>> AccountExtendedBalanceType;
        
        typedef map<string, vector<PrettyReserveBalance>> AccountReserveBalanceSummaryType;
        
        typedef map<string, int64_t> AccountVoteSummaryType;
        
        // typedef std::pair<order_type_enum, vector<string>> order_description;
        
        enum DelegateStatusFlags {
            any_delegate_status = 0,
            enabled_delegate_status = 1 << 0,
            active_delegate_status = 1 << 1,
            disabled_delegate_status = 1 << 2,
            inactive_delegate_status = 1 << 3
        };
        
        enum AccountKeyType {
            owner_key = 0,
            active_key = 1,
            signing_key = 2
        };
        
        class Wallet {
          public:
            Wallet(ChainDatabasePtr chain, bool enabled = true);
            virtual ~Wallet();
            enum TrxType {
                trx_type_desipate,
                trx_type_withdraw,
                trx_type_all
            };
            /**
            * Initializes the transaction creator
            *
            * @param  c  transaction_creation_state
            * @param  account_name  specific account to initializes creation for creating transaction
            *
            * @return void
            */
            void initialize_transaction_creator(TransactionCreationState& c, const string& account_name);
            /**  sign_transaction_creator
            * Unrealized function
            *
            * @param  c  transaction_creation_state
            *
            * @return void
            */
            void sign_transaction_creator(TransactionCreationState& c);
            
            //Emitted when wallet is locked or unlocked. Argument is true if wallet is now locked; false otherwise.
            fc::signal<void(bool)>  wallet_lock_state_changed;
            //Emitted when wallet claims a new transaction. Argument is new ledger entry.
            fc::signal<void(LedgerEntry)> wallet_claimed_transaction;
            //Emitted when someone (partially or fully) fills your short, thereby giving you a margin position
            fc::signal<void(LedgerEntry)> update_margin_position;
            
            /**
            * Set the wallet data path
            *
            * @param  data_dir  wallet path
            *
            * @return void
            */
            void    set_data_directory(const path& data_dir);
            /**
            * Gets the current data path for the wallet
            *
            * @return path
            */
            path    get_data_directory()const;
            
            
            /**
            * Creates a wallet with the given name.
            *
            * @param wallet_name name of the wallet to create
            * @param password a passphrase for encrypting the wallet
            * @param brainkey a strong passphrase that will be used to generate all private keys, defaults to a large random
            *                  number
            */
            void    create(const string& wallet_name,
                           const string& password,
                           const string& brainkey = string());
            /**
            * On the basis of the wallet name to load the local database
            * @param wallet_name the name of the wallet to open
            *
            *@return void
            */
            void    open(const string& wallet_name);
            /**
            * Close the current wallet
            *
            *@return void
            */
            void    close();
            
            
            /**
            * Whether the client open wallet function
            *
            * @return bool
            */
            bool    is_enabled()const;
            /**
            * To determine whether a wallet is opened
            *
            * @return bool
            */
            bool    is_open()const;
            /**
            * Returns the current wallet's name
            *
            * @return string
            */
            string  get_wallet_name()const;
            
            /**
            * Exports the current wallet to a JSON file.
            *
            * @param json_filename the full path and filename of JSON file to generate
            *
            * @return void
            */
            void    export_to_json(const path& filename)const;
            /**
            * Creates a new wallet from an exported JSON file.
            *
            * @param filename the full path and filename of JSON wallet to import
            * @param wallet_name name of the wallet to create
            * @param passphrase passphrase of the imported wallet
            */
            void    create_from_json(const path& filename, const string& wallet_name, const string& passphrase);
            
            /**
            * Perform a wallet backup
            *
            * @param reason the reason of the backup
            *
            * @return void
            */
            void    auto_backup(const string& reason)const;
            
            /**
            * Write latest transaction builder into specific file
            *
            * @param  builder  the transaction builder to write into.
            * @param  alternate_path  file path. if it is "" then write into latest.trx
            *
            * @return void
            */
            void    write_latest_builder(const TransactionBuilder& builder,
                                         const string& alternate_path);
                                         
            /**
            * Set wallet version
            *
            * @param  v  wallet version
            *
            * @return void
            */
            void                   set_version(uint32_t v);
            /**
            * Get current wallet version
            *
            * @return uint32_t
            */
            uint32_t               get_version()const;
            
            /**
            * Enables or disables automatic wallet backups.
            *
            * @param enabled true to enable and false to disable (bool, required)
            *
            * @return bool
            */
            void                   set_automatic_backups(bool enabled);
            /**
            * Get automatic backup property
            *
            * @return bool
            */
            bool                   get_automatic_backups()const;
            
            /**
            * Set wallet property of transaction scanning
            *
            * @param  enabled  enable or disable transaction scanning
            *
            * @return void
            */
            void                   set_transaction_scanning(bool enabled);
            /**
            * Get wallet property of transaction scanning
            *
            * @return bool
            */
            bool                   get_transaction_scanning()const;
            
            /**
            * Set wallet property of last scanned block number
            *
            * @param  block_num  last scanned block number
            *
            * @return void
            */
            void                   set_last_scanned_block_number(uint32_t block_num);
            void                    set_last_scanned_block_number_for_alp(uint32_t block_num);
            /**
            * Get wallet property of last scanned block number.
            *
            * @return uint32_t
            */
            uint32_t               get_last_scanned_block_number()const;
            uint32_t                get_last_scanned_block_number_for_alp()const;
            
            /**
            * Set wallet property of every transaction's fee
            *
            * @param  fee  one transaction fee
            *
            * @return void
            */
            void                   set_transaction_fee(const Asset& fee);
            /**
            * Get wallet property of the specified asset's transaction's fee
            *
            * @param  desired_fee_asset_id  specified asset
            *
            * @return Asset
            */
            Asset                  get_transaction_fee(const AssetIdType desired_fee_asset_id = 0)const;
            Asset                  get_transaction_imessage_fee(const std::string & imessage = "")const;
            /**
            * Whether the fees of the specified assets can be paid with specified assets
            *
            * @param  desired_fee_asset_id  specified asset
            *
            * @return bool
            */
            bool                   asset_can_pay_fee(const AssetIdType desired_fee_asset_id = 0)const;
            
            /**
            * Set transaction expiration time.
            *
            * @param secs transactions effective seconds
            *
            * @return uint32_t
            */
            void                   set_transaction_expiration(uint32_t secs);
            /**
            * Get transaction expiration time.
            *
            * @return uint32_t
            */
            uint32_t               get_transaction_expiration()const;
            void set_transaction_imessage_fee_coe(const ImessageIdType& coe);
            ImessageIdType get_transaction_imessage_fee_coe()const;
            void set_transaction_imessage_soft_max_length(const ImessageLengthIdType& length);
            ImessageLengthIdType get_transaction_imessage_soft_max_length()const;
            /**
            * Get scan process count
            *
            * @return float
            */
            float                  get_scan_progress()const;
            
            /**
            * Set specified setting of the wallet
            * @param  name  setting name
            * @param  value  setting value
            *
            * @return void
            */
            void                   set_setting(const string& name, const variant& value);
            /**
            * Get specified setting of the wallet
            *
            * @param  name  setting name
            *
            * @return fc::optional<variant>
            */
            fc::optional<variant>  get_setting(const string& name)const;
            
            /**
            * Lock the private keys in wallet, disables spending commands until unlocked.
            *
            * @return void
            */
            void                               lock();
            /**
            * Unlock the private keys in the wallet to enable spending operations.
            *
            * @param password the password for encrypting the wallet
            * @param timeout_seconds the number of seconds to keep the wallet unlocked
            *
            * @return void
            */
            void                               unlock(const string& password, uint32_t timeout_seconds);
            /**
            * Get wallet status of lock
            *
            * @return bool
            */
            bool                               is_locked()const;
            /**
            * Get wallet status of unlock
            *
            * @return bool
            */
            bool                               is_unlocked()const;
            /**
            * Get the wallet unlocking deadline
            *
            * @return fc::optional<fc::time_point_sec>
            */
            fc::optional<fc::time_point_sec>   unlocked_until()const;
            
            /**
            * Change the password of the current wallet.The need to change the password interface enter the original password.
            *
            * This will change the wallet's spending passphrase, please make sure you remember it.
            *
            * @param old_passphrase the old passphrase for this wallet
            * @param passphrase the passphrase for encrypting the wallet
            *
            * @return void
            */
            void                               change_passphrase(const string &old_passphrase, const string& new_passphrase);
            /**
            * Change the password of the current wallet.
            *
            * This will change the wallet's spending passphrase, please make sure you remember it.
            *
            * @param passphrase the passphrase for encrypting the wallet
            *
            * @return bool
            */
            bool                                check_passphrase(const string& passphrase);
            /**
            * Get active private key of the specified account
            *
            * @param  account_name  the specified account's name
            *
            * @return PrivateKeyType
            */
            PrivateKeyType           get_active_private_key(const string& account_name)const;
            /**
            * Get active public key of the specified account
            *
            * @param  account_name  the specified account's name
            *
            * @return PublicKeyType
            */
            PublicKeyType            get_active_public_key(const string& account_name)const;
            /**
            * Get owner public key of the specified account
            *
            * @param  account_name  the specified account's name
            *
            * @return PublicKeyType
            */
            PublicKeyType            get_owner_public_key(const string& account_name)const;
            
            /**
            * Convert to public key summary contains a compressed format address the compressed format, and so on
            *
            * @param  pubkey  PublicKeyType
            *
            * @return PublicKeySummary
            */
            PublicKeySummary         get_public_key_summary(const PublicKeyType& pubkey) const;
            /**
            * Get all of the public key from the specified account
            *
            * @param  account_name  the specified account name
            *
            * @return vector<PublicKeyType>
            */
            vector<PublicKeyType>    get_public_keys_in_account(const string& account_name)const;
            /**
            * Queries your wallet for the specified transaction.
            *
            * @param transaction_id the id (or id prefix) of the transaction (string, required)
            *
            * @return transaction_entry
            */
            WalletTransactionEntry get_transaction(const string& transaction_id_prefix)const;
            
            
            /**
            * All transactions in pending zones.
            *
            * @return vector<WalletTransactionEntry>
            */
            vector<WalletTransactionEntry>          get_pending_transactions()const;
            /**
            * Return any errors for your currently pending transactions.
            *
            * @return map<TransactionIdType, fc::exception>
            */
            map<TransactionIdType, fc::exception>    get_pending_transaction_errors()const;
            
            /**
            * Start to scan block data.
            *
            * @param start_block_num start scanning block number.
            * @param limit the number of need to scan the block
            *
            * @return void
            */
            void start_scan(const uint32_t start_block_num, const uint32_t limit);
            /**
            * Cancel the current scan thread
            *
            * @return void
            */
            void cancel_scan();
            
            /**
            * get all the contracts in wallet
            *
            * @return vector<string>
            */
            vector<string> get_contracts(const string &account_name = "all");
            
            /**
            * Scan all transactions wallet.
            *
            * @param overwrite_existing Whether reconstruction already exists transaction
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry         scan_all_transaction(bool overwrite_existing);
            
            /**
            * Scan the specified transaction from wallet db.
            *
            * @param transaction_id_prefix the specified transaction
            * @param overwrite_existing Whether reconstruction already exists transaction
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry         scan_transaction(const string& transaction_id_prefix, bool overwrite_existing);
            /**
            * To be deleted.
            *
            * @return PublicKeyType
            */
            TransactionLedgerEntry          scan_transaction_experimental(const string& transaction_id_prefix, bool overwrite_existing);
            /**
            * To be deleted.
            *
            * @return PublicKeyType
            */
            void add_transaction_note_experimental(const string& transaction_id_prefix, const string& note);
            /**
            * To be deleted.
            *
            * @return PublicKeyType
            */
            set<PrettyTransactionExperimental> transaction_history_experimental(const string& account_name);
            /**
            * To be deleted.
            *
            * @return PublicKeyType
            */
            PrettyTransactionExperimental to_pretty_transaction_experimental(const TransactionLedgerEntry& entry);
            /**
            * Get the specified transaction data.
            *
            * @param transaction_id_prefix the specified transaction's ID
            *
            * @return PublicKeyType
            */
            vector<WalletTransactionEntry> get_transactions(const string& transaction_id_prefix);
            
            /**
            * Add new account for receiving payments.
            *
            * @param account_name the name you will use to refer to this receive account (account_name, required)
            * @param private_data Extra data to store with this account entry (json_variant, optional, defaults to null)
            *
            * @return PublicKeyType
            */
            PublicKeyType  create_account(const string& account_name,
                                          const variant& private_data = variant());
            /**
            * Delete the specific account from current wallet
            *
            * @param  account_name  the specific account name
            *
            * @return bool
            */
            bool delete_account(const string& account_name);
            /**
            * Updates the specified account private data
            *
            * @param  account_to_update  account name
            * @param  private_data  new private data
            *
            * @return void
            */
            void update_account_private_data(const string& account_to_update,
                                             const variant& private_data);
                                             
            /**  account_set_favorite
            * Updates the specified account favorite property
            *
            * @param  account_name  account name
            * @param  is_favorite  true or false
            *
            * @return void
            */
            void account_set_favorite(const string& account_name,
                                      const bool is_favorite);
            /**
            * Retrieves account entry from wallet db according name
            *
            * @param account_name name of specified account
            *
            * @return WalletAccountEntry
            */
            WalletAccountEntry get_account(const string& account_name)const;
            /**
            * Split the initial address into base address and extension
            *
            * @param original the original account which length is 67 or 68
            * @param to_account the address which used in underlying wallet
            * @param sub_account the address which only used in centralized services
            *
            * @return void
            */
            static void accountsplit(const string & original, string & to_account, string & sub_account);
            /**
            * Verify legitimacy of account name
            *
            * @param account name
            *
            * @return bool
            */
            bool is_valid_account_name(const string & accountname);
            /**
            * Adds account to the local wallet db
            *
            * @param  account_name  string
            * @param  key  PublicKeyType
            * @param  private_data  variant
            *
            * @return void
            */
            void     add_contact_account(const string& account_name,
                                         const PublicKeyType& key,
                                         const variant& private_data = variant());
                                         
            /**
            * Delete account from the local wallet db
            *
            * @param  account_name  string
            *
            * @return void
            */
            void     remove_contact_account(const string& account_name);
            
            /**
            * rename local account name. Be sure the account is not registered
            * @param  old_account_name  string
            * @param  new_account_name  string
            *
            * @return void
            */
            void     rename_account(const string& old_contact_name,
                                    const string& new_contact_name);
                                    
            /**
            * List wallet all contact accounts.
            *
            * @return vector<WalletContactEntry>
            */
            vector<WalletContactEntry> list_contacts()const;
            /**
            * Get contact according to data entered. Data include account_name or public_key or address or btc_address.
            *
            * @param  data  account_name or public_key or address or btc_address.
            *
            * @return oWalletContactEntry
            */
            oWalletContactEntry get_contact(const variant& data)const;
            /**
            * Get contact according to data entered. Data include account_name or public_key or address or btc_address.
            *
            * @param  label  account_name or public_key or address or btc_address.
            *
            * @return oWalletContactEntry
            */
            oWalletContactEntry get_contact(const string& label)const;
            /**
            * Add contact in wallet db
            *
            * @param  contact  ContactData
            *
            * @return WalletContactEntry
            */
            WalletContactEntry add_contact(const ContactData& contact);
            /**
            * Remove contact in wallet db
            *
            * @param  data  account_name or public_key or address or btc_address.
            *
            * @return oWalletContactEntry
            */
            oWalletContactEntry remove_contact(const variant& data);
            /**
            * Remove contact in wallet db
            *
            * @param  label  account_name or public_key or address or btc_address.
            *
            * @return oWalletContactEntry
            */
            oWalletContactEntry remove_contact(const string& label);
            
            /**
            * Get wallet data from the address entered.
            *
            * @param  addr  Address
            *
            * @return oWalletAccountEntry
            */
            oWalletAccountEntry  get_account_for_address(Address addr)const;
            
            /**
             * Return general information about the wallet
             *
             * @return variant
             **/
            variant get_info()const;
            
            
            /**
            * Setting delegate generate block status
            *
            * @param  delegate_name  delegate name or  ALL means all delegate in wallet
            * @param  enabled  true or false
            *
            * @return void
            */
            void set_delegate_block_production(const string& delegate_id, bool enabled = true);
            /**
            * Get generate block state from designated delegate
            *
            * @param  account_name  account name
            *
            * @return bool
            */
            bool wallet_get_delegate_statue(const std::string& account_name);
            /**
            * Gets the specified delegate data based on delegate status
            *
            * @param  delegates_to_retrieve  Type is delegate_status_flags. Uses int type to allow ORing multiple flags
            *
            * @return vector<WalletAccountEntry>
            */
            vector<WalletAccountEntry> get_my_delegates(uint32_t delegates_to_retrieve = any_delegate_status)const;
            
            /**  get_next_producible_block_timestamp
            *
            * @param  delegate_entrys  vector<WalletAccountEntry>
            *
            * @return optional<time_point_sec>
            */
            optional<time_point_sec> get_next_producible_block_timestamp(const vector<WalletAccountEntry>& delegate_entrys)const;
            
            /**
            * sign a block if this wallet controls the key for the active delegate, or throw
            *
            * @param  header  SignedBlockHeader
            *
            * @return void
            */
            void sign_block(SignedBlockHeader& header)const;
            
            /**
            * Sign a block based on the input address or public key
            *
            * @param  signer  address or public key
            * @param  hash  fc::sha256
            *
            * @return fc::ecc::compact_signature
            */
            fc::ecc::compact_signature  sign_hash(const string& signer, const fc::sha256& hash)const;
            
            /**
            * Return a list of wallets in the current data directory.
            *
            * @return vector<string>
            */
            vector<string> list() const; // list wallets
            
            /**
            * List wallet all accounts.
            *
            * @return vector<WalletAccountEntry>
            */
            vector<WalletAccountEntry> list_accounts()const;
            /**
            * List all favorite accounts.
            *
            * @return vector<WalletAccountEntry>
            */
            vector<WalletAccountEntry> list_favorite_accounts()const;
            /**
            * List all unregistered accounts.
            *
            * @return vector<WalletAccountEntry>
            */
            vector<WalletAccountEntry> list_unregistered_accounts()const;
            /**
            * List all my accounts in current wallet.
            *
            * @return vector<WalletAccountEntry>
            */
            vector<WalletAccountEntry> list_my_accounts()const;
            /**
            * List all my accounts and account's address in current wallet.
            *
            * @return vector<WalletAccountEntry>
            */
            vector<AccountAddressData> list_addresses() const;
            
            /**
            * Import private key into current wallet. If the account has been registered in the chain is not used to enter the name
            *
            * @param  key  PrivateKeyType
            * @param  account_name  string
            *
            * @return bool
            */
            bool               friendly_import_private_key(const PrivateKeyType& key, const string& account_name);
            /**
            * Import a private key into current wallet.
            *
            * @param  new_private_key  PrivateKeyType
            * @param  account_name  account name
            * @param  create_account  Are Forced Create an account.If the account is not registered it must be true
            *
            * @return PublicKeyType
            */
            PublicKeyType import_private_key(const PrivateKeyType& new_private_key,
                                             const optional<string>& account_name,
                                             bool create_account = false);
                                             
            /**
            * Loads the private key into the specified account. Returns which account it was actually imported to.
            *
            * @param wif_key A private key in thinkyoung wallet import format
            * @param account_name the name of the account the key should be imported into, if null then the key must belong to
            *                     an active account
            * @param create_account If true, the wallet will attempt to create a new account for the name provided rather
            *                           than import the key into an existing account
            *
            * @return PublicKeyType
            */
            PublicKeyType import_wif_private_key(const string& wif_key,
                                                 const optional<string>& account_name,
                                                 bool create_account = false);
                                                 
            /**
            * Creates an address which can be used for a simple (non-TITAN) transfer.
            *
            * @param account_name The account name that will own this address
            *
            * @return string
            */
            PublicKeyType       get_new_public_key(const string& account_name);
            /**
            * create a new address .
            *
            * @param  account_name  string
            * @param  label  nothing happened.
            *
            * @return Address
            */
            Address               create_new_address(const string& account_name, const string& label = "");
            
            
            /**
            * nothing happened.will delete.
            *
            * @param  addr  Address
            * @param  label  string
            *
            * @return void
            */
            void              set_address_label(const Address& addr, const string& label);
            /**
            * nothing happened.will delete.
            *
            * @param  addr  Address
            *
            * @return string
            */
            string            get_address_label(const Address& addr);
            /**
            *  nothing happened.will delete.
            *
            * @param  addr  Address
            * @param  group_label  string
            *
            * @return void
            */
            void              set_address_group_label(const Address& addr, const string& group_label);
            /**
            *  nothing happened.will delete.
            *
            * @param  addr  Address
            *
            * @return string
            */
            string            get_address_group_label(const Address& addr);
            /**
            *  nothing happened.will delete.
            *
            * @param  group_label  string
            *
            * @return vector<Address>
            */
            vector<Address>   get_addresses_for_group_label(const string& group_label);
            
            /**
            * Transaction Generation Methods
            *
            * @return std::shared_ptr<TransactionBuilder>
            */
            std::shared_ptr<TransactionBuilder> create_transaction_builder();
            /**
            * Create transaction builder based on old builder.
            *
            * @param  old_builder  TransactionBuilder
            *
            * @return std::shared_ptr<TransactionBuilder>
            */
            std::shared_ptr<TransactionBuilder> create_transaction_builder(const TransactionBuilder& old_builder);
            /**
            * Create transaction builder based on old builder file.
            *
            * @param  old_builder_path  string
            *
            * @return std::shared_ptr<TransactionBuilder>
            */
            std::shared_ptr<TransactionBuilder> create_transaction_builder_from_file(const string& old_builder_path = "");
            
            
            /**
            * Save transaction to broadcast area and local wallet data area
            *
            * @param transaction_entry the transaction to save
            *
            * @return void
            */
            void cache_transaction(WalletTransactionEntry& transaction_entry, bool store=true);
            
            /**
             *  Multi-Part transfers provide additional security by not combining inputs, but they
             *  show up to the user as multiple unique transfers.  This is an advanced feature
             *  that should probably have some user interface support to merge these transfers
             *  into one logical transfer.
             */
            vector<SignedTransaction> multipart_transfer(
                double real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const string& from_account_name,
                const string& to_account_name,
                const string& memo_message,
                bool sign
            );
            /**
            *  This transfer works like a bitcoin transaction combining multiple inputs
            *  and producing a single output. The only different aspect with transfer_asset is that
            *  this will send to a address.
            *
            * @param real_amount_to_transfer the amount of shares to transfer
            * @param amount_to_transfer_symbol the asset to transfer like ALP
            * @param from_account_name the source account to draw the shares from
            * @param to_address the address or pubkey to transfer to
            * @param memo_message a memo to store with the transaction
            * @param selection_method enumeration [vote_none | vote_all | vote_random | vote_recommended] (vote_strategy, optional,
            *                 defaults to "vote_recommended")
            * @param sign whether sign the transaction
            * @param alp_account centralized system address
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry transfer_asset_to_address(
                const string& real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const string& from_account_name,
                const Address& to_address,
                const string& memo_message,
                VoteStrategy selection_method,
                bool sign,
                const string& alp_account = ""
            );
            
            WalletTransactionEntry transfer_asset_to_contract(
                double real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const string& from_account_name,
                const Address& to_contract_address,
                double exec_cost,
                bool sign,
                bool is_testing = false);
                
            std::vector<thinkyoung::blockchain::Asset> transfer_asset_to_contract_testing(
                double real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const string& from_account_name,
                const Address& to_contract_address,
                bool sign);
                
            WalletTransactionEntry upgrade_contract(
                const Address& contract_id,
                const string& upgrader_name,
                const string& contract_name,
                const string& contract_desc,
                const std::string& asset_symbol,
                const double exec_limit,
                bool sign = true,
                bool is_testing = false);
                
            std::vector<thinkyoung::blockchain::Asset> upgrade_contract_testing(
                const Address& contract_id,
                const string& upgrader_name,
                const string& contract_name,
                const string& contract_desc,
                bool sign = true
            );
            
            WalletTransactionEntry destroy_contract(
                const Address& contract_id,
                const string& destroyer_name,
                const std::string& asset_symbol,
                double exec_limit,
                bool sign = true,
                bool is_testting = false);
                
            std::vector<thinkyoung::blockchain::Asset> destroy_contract_testing(
                const Address& contract_id,
                const string& destroyer_name,
                bool sign = true
            );
            
            /*
            transaction_builder builder_transfer_asset_to_address(
            double real_amount_to_transfer,
            const string& amount_to_transfer_symbol,
            const string& from_account_name,
            const address& to_address,
            const string& memo_message,
            vote_strategy selection_method
            );
            */
            
            /**
            * This transfer works like a bitcoin send many transaction combining multiple inputs
            * and producing a single output.
            *
            * @param  amount_to_transfer_symbol total amount
            * @param  from_account_name  from account name
            * @param  to_address_amounts many to account address and amount
            * @param  memo_message  string
            * @param  sign  whether need sign
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry transfer_asset_to_many_address(
                const string& amount_to_transfer_symbol,
                const string& from_account_name,
                const unordered_map<Address, double>& to_address_amounts,
                const string& memo_message,
                bool sign
            );
            
            /**
            * register a new account
            *
            * @param  account_to_register  account name to register
            * @param  json_data   public data
            * @param  delegate_pay_rate uint8_t 0-100 or 255 default 255
            * @param  pay_with_account_name  pay from account name
            * @param  new_account_type  account_type such as titan_account multisig_account and so on.
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry register_account(
                const string& account_name,
                const variant& json_data,
                uint8_t delegate_pay_rate,
                const string& pay_with_account_name,
                thinkyoung::blockchain::AccountType new_account_type,
                bool sign
            );
            /**
            * Update Account Registration Information
            *
            * @param  account_name  account name to register
            * @param  pay_from_account   pay from account name
            * @param  public_data  public data
            * @param  delegate_pay_rate  0-100 or 255 default 255
            * @param  sign  bool
            *
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry update_registered_account(
                const string& account_name,
                const string& pay_from_account,
                optional<variant> public_data,
                uint8_t delegate_pay_rate,
                bool sign
            );
            /**
            * Update active key. active key change can hide your owner address.
            *
            * @param  account_to_update  account name
            * @param  pay_from_account  pay account name
            * @param  new_active_key  new active private key.
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry update_active_key(
                const std::string& account_to_update,
                const std::string& pay_from_account,
                const std::string& new_active_key,
                bool sign
            );
            /**
            * retract has registered account
            *
            * @param  account_to_retract  account name
            * @param  pay_from_account  pay fee from account
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry retract_account(
                const std::string& account_to_retract,
                const std::string& pay_from_account,
                bool sign
            );
            /**
            * Designated delegate to receive salary
            *
            * @param  delegate_name  delegate name
            * @param  real_amount_to_withdraw  amount
            * @param  withdraw_to_account_name  Receive salary account
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry withdraw_delegate_pay(
                const string& delegate_name,
                const string& amount_to_withdraw,
                const string& withdraw_to_account_name,
                bool sign
            );
            /**
            * Query delegate salary
            *
            * @param  delegate_name  string
            *
            * @return ShareType
            */
            DelegatePaySalary query_delegate_salary(
                const string& delegate_name);
            std::map<std::string, thinkyoung::blockchain::DelegatePaySalary> query_delegate_salarys();
            
            /**
            * The balance of all the money designated to vote again according to the voter_address
            *
            * @param  balance_id  BalanceIdType
            * @param  voter_address  Address
            * @param  strategy  VoteStrategy
            *
            * @return TransactionBuilder
            */
            TransactionBuilder set_vote_info(
                const BalanceIdType& balance_id,
                const Address& voter_address,
                VoteStrategy selection_method
            );
            /**
            * Broadcast voting combination from current wallet
            *
            * @param  account_to_publish_under  string
            * @param  account_to_pay_with  string
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry publish_slate(
                const string& account_to_publish_under,
                const string& account_to_pay_with,
                bool sign
            );
            /**
            *  Broadcast current wallet version
            *
            * @param  account_to_publish_under  string
            * @param  account_to_pay_with  pay account name
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry publish_version(
                const string& account_to_publish_under,
                const string& account_to_pay_with,
                bool sign
            );
            /**
            * Collect all the designated account balance by the filter.
            *
            * @param  account_name  string
            * @param  filter
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry collect_account_balances(
                const string& account_name,
                const function<bool(const BalanceEntry&)> filter,
                const string& memo_message,
                bool sign
            );
            /**
            * To Do.
            *
            * @param  paying_account_name  string
            * @param  symbol  string
            * @param  key  PublicKeyType
            * @param  meta  bool
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry asset_authorize_key(
                const string& paying_account_name,
                const string& symbol,
                const Address& key,
                const ObjectIdType meta,
                bool sign
            );
            /**
            * update delegate's signature which used to generate block
            *
            * @param  authorizing_account_name
            * @param  delegate_name  string
            * @param  signing_key  PublicKeyType
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry update_signing_key(
                const string& authorizing_account_name,
                const string& delegate_name,
                const PublicKeyType& signing_key,
                bool sign
            );
            /**
            * create a new asset.
            *
            * @param  symbol  new asset's symbol when the longer symbol the lower the price
            * @param  asset_name  asset name
            * @param  description  string
            * @param  data  variant
            * @param  issuer_account_name  string
            * @param  max_share_supply  double
            * @param  precision  uint64_t
            * @param  is_market_issued  no used.
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry create_asset(
                const string& symbol,
                const string& asset_name,
                const string& description,
                const variant& data,
                const string& issuer_name,
                const string& max_share_supply,
                uint64_t precision,
                bool is_market_issued,
                bool sign
            );
            /**
            * update the specific asset.
            *
            * @param  symbol  string
            * @param  name  optional<string>
            * @param  description  optional<string>
            * @param  public_data  optional<variant>
            * @param  maximum_share_supply  optional<double>
            * @param  precision  optional<uint64_t>
            * @param  issuer_fee  ShareType
            * @param  market_fee  double
            * @param  flags  uint32_t
            * @param  issuer_perms  uint32_t
            * @param  issuer_account_name  string
            * @param  required_sigs  uint32_t
            * @param  authority  vector<Address>
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry update_asset(
                const string& symbol,
                const optional<string>& name,
                const optional<string>& description,
                const optional<variant>& public_data,
                const optional<double>& maximum_share_supply,
                const optional<uint64_t>& precision,
                const ShareType issuer_fee,
                double market_fee,
                uint32_t flags,
                uint32_t issuer_perms,
                const string& issuer_account_name,
                uint32_t required_sigs,
                const vector<Address>& authority,
                bool sign
            );
            /**
            * issue the asset by who created it.
            *
            * @param  amount_to_issue  double
            * @param  symbol  string
            * @param  to_account_name  string
            * @param  memo_message  string
            * @param  sign  bool
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry issue_asset(
                const string& amount,
                const string& symbol,
                const string& to_account_name,
                const string& memo_message,
                bool sign
            );
            /**
            * issue the asset to addresses by who created it.
            *
            * @param  symbol  string
            * @param  map<string
            * @param  addresses  ShareType>
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry issue_asset_to_addresses(
                const string& symbol,
                const map<string, ShareType>& addresses);
                
                
                
                
                
            /**
            * Get account name from wallet db by public key.
            *
            * @param  key  PublicKeyType
            *
            * @return string
            */
            string                             get_key_label(const PublicKeyType& key)const;
            PrettyTransaction                  to_pretty_trx(const WalletTransactionEntry& trx_rec) const;
            
            
            /**
            * Formatted transactions to pretty transaction.
            *
            * @param  trx_entry  transaction entry
            * @param  addr_for_fee for calculation fees. IF it is "" then no change.
            *
            * @return string
            */
            PrettyTransaction                   to_pretty_trx(const thinkyoung::blockchain::TransactionEntry& trx_entry, const std::string addr_for_fee) const;
            
            
            PrettyContractTransaction           to_pretty_contract_trx(const thinkyoung::blockchain::TransactionEntry& trx_entry) const;
            
            
            /**
            * Updates your approval of the specified account.
            *
            * @param account_name the name of the account to set approval for
            * @param approval 1, 0, or -1 respectively for approve, neutral, or disapprove   defaults to 1
            *
            * @return int8_t
            */
            void                               set_account_approval(const string& account_name, int8_t approval);
            /**
            * Get all approved accounts.
            *
            * @param approval 1 0 -1 According to the value to get the corresponding account entry
            *
            * @return void
            */
            vector<AccountEntry>                get_all_approved_accounts(const int8_t approval);
            /**
            * clear all account approval.
            *
            * @param account_name nothing to effect
            *
            * @return void
            */
            void                                clear_account_approval(const string& account_name);
            /**
            * Get your approval of the specified account.
            *
            * @param account_name the name of the account to get approval
            *
            * @return int8_t
            */
            int8_t                             get_account_approval(const string& account_name)const;
            
            /**
            * Validate the address.
            *
            * @param  addr  Address
            *
            * @return bool
            */
            bool                               is_sending_address(const Address& addr)const;
            /**
            * Validate the address.
            *
            * @param  addr  Address
            *
            * @return bool
            */
            bool                               is_receive_address(const Address& addr)const;
            
            /**
            * By specifying function to scan balance.
            *
            * @param  function<void
            *
            * @return void
            */
            void                               scan_balances(const function<void(const BalanceIdType&,
                    const BalanceEntry&)> callback)const;
            /**
            * Get spendable balance entry controlled by this account.
            *
            * @param account_name (account_name, required)
            *
            * @return account_balance_entry_summary_type
            */
            AccountBalanceEntrySummaryType get_spendable_account_balance_entrys(const string& account_name = "")const;
            /**
            * Get spendable balance entry controlled by this account.
            *
            * @param  account_name  string
            *
            * @return AccountBalanceSummaryType
            */
            AccountBalanceSummaryType       get_spendable_account_balances(const string& account_name = "")const;
            
            /**
            * Get all the balance ids from the specific account
            *
            * @param  account_name  string
            *
            * @return AccountBalanceIdSummaryType
            */
            AccountBalanceIdSummaryType    get_account_balance_ids(const string& account_name = "")const;
            
            AccountReserveBalanceSummaryType get_account_reserve_balances(const string& account_name = "")const;
            /**
            * Get the specific account voting combination
            *
            * @param  account_name  string
            *
            * @return AccountVoteSummaryType
            */
            AccountVoteSummaryType          get_account_vote_summary(const string& account_name = "")const;
            
            /**
            * Not used.
            *
            * @param  account_name  string
            *
            * @return vector<EscrowSummary>
            */
            vector<EscrowSummary>             get_escrow_balances(const string& account_name);
            
            
            /**
            * According to the account name lookup for all relevant transactions .
            *
            * @param account_name the name of the account for which the transaction history will be returned, "" for all accounts
            * @param start_block_num from then on this block number began to collect account transaction history
            * @param end_block_num from then on this block number end to collect account transaction history
            * @param asset_symbol asset type symbol
            * @param trx_search_type transaction type, such as trx_type_all,trx_type_desipate,trx_type_withdraw.
            *
            * @return vector<PrettyTransaction>
            */
            vector<WalletTransactionEntry>  get_transaction_history(const string& account_name = string(),
                    uint32_t start_block_num = 0,
                    uint32_t end_block_num = -1,
                    const string& asset_symbol = "",
                    TrxType trx_search_type = trx_type_all)const;
                    
            /**
            * According to the account name lookup for all relevant transactions .
            *
            * @param account_name the name of the account for which the transaction history will be returned, "" for all accounts
            * @param asset_symbol asset type symbol
            * @param trx_search_type transaction type, such as trx_type_all,trx_type_desipate,trx_type_withdraw.
            *
            * @return vector<PrettyTransaction>
            */
            vector<WalletTransactionEntry>  get_transaction_history_splite(const string& account_name = string(),
                    const string& asset_symbol = "",
                    TrxType trx_search_type = trx_type_all)const;
            /**
            * According to the account name lookup for all relevant transactions and format conversion.
            *
            * @param account_name the name of the account for which the transaction history will be returned, "" for all accounts
            * @param start_block_num from then on this block number began to collect account transaction history
            * @param end_block_num from then on this block number end to collect account transaction history
            * @param asset_symbol asset type symbol
            * @param trx_splite transaction type, such as trx_type_all,trx_type_desipate,trx_type_withdraw.
            * @param wither_splite Determine whether the transaction type classification
            *
            * @return vector<PrettyTransaction>
            */
            vector<PrettyTransaction>         get_pretty_transaction_history(const string& account_name = string(),
                    uint32_t start_block_num = 0,
                    uint32_t end_block_num = -1,
                    const string& asset_symbol = "",
                    TrxType trx_splite = trx_type_all,
                    bool wither_splite = false)const;
                    
            /**
            * Lists wallet's balance at the given block number.
            *
            * @param account_name the name of the account for which the historic balance will be returned, "" for all accounts
            * @param block_num the block number for which the balance will be computed (timestamp, required)
            *
            * @return account_balance_summary_type
            */
            AccountBalanceSummaryType       compute_historic_balance(const string &account_name,
                    uint32_t block_num)const;
                    
            /**
            * Removes the specified transaction entry from your transaction history.
            *
            * @param entry_id the id (or id prefix) of the transaction entry
            */
            void                               remove_transaction_entry(const string& entry_id);
            
            /**
            * Try to fix all the entry of the account in the current wallet
            *
            * @param  collecting_account_name  optional<string>
            *
            * @return void
            */
            void                               repair_entrys(const optional<string>& collecting_account_name);
            /**
            * Regenerate the account active private key based on the input of seed.
            *
            * @param  account_name  string
            * @param  num_keys_to_regenerate  uint32_t
            *
            * @return uint32_t
            */
            uint32_t                           regenerate_keys(const string& account_name, uint32_t num_keys_to_regenerate);
            /**
            * Update wallet account specified number of active private key
            *
            * @param  number_of_accounts  int32_t
            * @param  max_number_of_attempts  int32_t
            *
            * @return int32_t
            */
            int32_t                            recover_accounts(int32_t number_of_accounts, int32_t max_number_of_attempts);
            
            /**
            * recover transaction info by rebuild transaction entry.
            *
            * @param  transaction_id_prefix  string
            * @param  recipient_account  string
            *
            * @return WalletTransactionEntry
            */
            WalletTransactionEntry          recover_transaction(const string& transaction_id_prefix, const string& recipient_account);
            /**
            * Not used.
            *
            * @param  transaction_id_prefix  string
            *
            * @return optional<variant_object>
            */
            optional<variant_object>           verify_titan_deposit(const string& transaction_id_prefix);
            /**
            * Get info about the votes of balances controlled by this account.
            *
            * @param account_name (account_name, required)
            *
            * @return VoteSummary
            */
            VoteSummary get_vote_status(const string& account_name);
            
            /**
            * Get private key by address.
            *
            * @param  addr  Address
            *
            * @return PrivateKeyType
            */
            PrivateKeyType get_private_key(const Address& addr)const;
            /**
            * Get public key by address.
            *
            * @param  addr  Address
            *
            * @return PublicKeyType
            */
            PublicKeyType get_public_key(const Address& addr) const;
            
            /**
            * Http rpc server function for login.
            *
            * @param  account_name  Address
            *
            * @return PublicKeyType
            */
            std::string login_start(const std::string& account_name);
            
            /**
            * Http rpc server function for logout.
            *
            * @param  account_name  Address
            *
            * @return PublicKeyType
            */
            fc::variant login_finish(const PublicKeyType& server_key,
                                     const PublicKeyType& client_key,
                                     const fc::ecc::compact_signature& client_signature);
                                     
            WalletTransactionEntry register_contract(const string& owner, const fc::path codefile, const string& asset_symbol, double init_limit, bool is_testing = false);
            std::vector<thinkyoung::blockchain::Asset> register_contract_testing(const string& owner, const fc::path codefile);
            
            WalletTransactionEntry call_contract(const string caller, const ContractIdType contract, const string method, const string& arguments, const string& asset_symbol, double cost_limit, bool is_testing = false);
            std::vector<thinkyoung::blockchain::Asset> call_contract_testing(const string caller, const ContractIdType contract, const string method, const string& arguments);
            std::vector<thinkyoung::blockchain::EventOperation> call_contract_local_emit(const string caller, const ContractIdType contract, const string method, const string& arguments);
            std::string call_contract_offline(const string caller, const ContractIdType contract, const string method, const string& arguments);
            
            void get_enough_balances(const string& account_name, const Asset target, std::map<BalanceIdType, ShareType>& balances, unordered_set<Address>& required_signatures);
            void sandbox_get_enough_balances(const string& account_name, const Asset target, std::map<BalanceIdType, ShareType>& balances, unordered_set<Address>& required_signatures);
            AccountBalanceEntrySummaryType sandbox_get_spendable_account_balance_entries(const string& account_name);
            
            //sandbox relate function
            ChainInterfacePtr get_correct_state_ptr() const;
            WalletDb& get_wallet_db() const;
            /*WalletTransactionEntry sandbox_register_contract(const string& owner, const fc::path codefile, const string& asset_symbol, double init_limit);*/
            void scan_contracts();
            
            vector<ScriptEntry> list_scripts();
            
            oScriptEntry get_script_entry(const ScriptIdType&);
            ScriptIdType add_script(const fc::path& filename, const string& description = string(""));
            void delete_script(const ScriptIdType& script_id);
            void disable_script(const ScriptIdType& script_id);
            void enable_script(const ScriptIdType& script_id);
            void import_script_db(const fc::path& src_path);
            void export_script_db(const fc::path & des_path);
            vector<ScriptIdType> list_event_handler(const ContractIdType& contract_id, const std::string& event_type);
            void add_event_handler(const ContractIdType& contract_id, const std::string& event_type, const ScriptIdType& script_id, uint32_t index);
            void delete_event_handler(const ContractIdType& contract_id, const std::string& event_type, const ScriptIdType& script_id);
            std::vector<std::string> get_events_bound(const std::string& script_id);
          private:
          
            unique_ptr<detail::WalletImpl> my;
          public:
            bool _generating_block;
        };
        
        typedef shared_ptr<Wallet> WalletPtr;
        typedef std::weak_ptr<Wallet> WalletWeakPtr;
        
    }
} // thinkyoung::wallet

FC_REFLECT_ENUM(thinkyoung::wallet::AccountKeyType,
                (owner_key)
                (active_key)
                (signing_key)
               )
