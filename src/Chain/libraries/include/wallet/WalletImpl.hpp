#pragma once

#include <wallet/WalletDb.hpp>
#include <db/LevelMap.hpp>
#include <blockchain/AccountOperations.hpp>
#include <blockchain/AssetOperations.hpp>
#include <blockchain/BalanceOperations.hpp>
#include "WalletEntrys.hpp"
#include "db/FastLevelMap.hpp"
#include <boost/thread/thread.hpp>  
#include <blockchain/ContractOperations.hpp>
namespace thinkyoung {
    namespace wallet {
        namespace detail {
            typedef boost::unique_lock<boost::shared_mutex> writelock;
            typedef boost::shared_lock<boost::shared_mutex> readlock;
            class WalletImpl : public ChainObserver
            {
            public:
                Wallet*                                    self = nullptr;
                bool                                       _is_enabled = true;
                WalletDb                                        _wallet_db;
                ChainDatabasePtr                               _blockchain = nullptr;
                path                                             _data_directory;
                path                                             _current_wallet_path;
                fc::sha512                                       _wallet_password;
                fc::optional<fc::time_point>                     _scheduled_lock_time;
                fc::future<void>                                 _relocker_done;
                fc::future<void>                                 _scan_in_progress;

                unsigned                                         _num_scanner_threads = 1;
                vector<std::unique_ptr<fc::thread>>              _scanner_threads;
                float                                            _scan_progress = 1;

                bool                                             _dirty_balances = true;
                unordered_map<BalanceIdType, BalanceEntry>   _balance_entrys;

                bool                                             _dirty_accounts = true;
                vector<PrivateKeyType>                         _stealth_private_keys;

				// thinkyoung3.0 contract related
				bool                                             _dirty_contracts = true;
				unordered_map<ContractIdType, ContractEntry>   _contract_entrys;

                struct LoginEntry
                {
                    PrivateKeyType key;
                    fc::time_point_sec insertion_time;
                };
                std::map<PublicKeyType, LoginEntry>          _login_map;
                fc::future<void>                                 _login_map_cleaner_done;
                const static short                               _login_cleaner_interval_seconds = 60;
                const static short                               _login_lifetime_seconds = 300;
                db::fast_level_map<ScriptIdType, ScriptEntry>      script_id_to_script_entry_db;
                db::fast_level_map<ScriptRelationKey, std::vector<ScriptIdType>>	   contract_id_event_to_script_id_vector_db;

                vector<function<void(void)>>                   _unlocked_upgrade_tasks;
                boost::shared_mutex                                        m_mutex_for_wallet;   //???ио12?ик??
                boost::shared_mutex                                        m_mutex_for_alp;   //???ио12?ик??
                boost::shared_mutex                                        m_mutex_for_private;   //???ио12?ик??
                WalletImpl();
                ~WalletImpl();

                /**  create_file
                *
                * @param  wallet_file_path  path
                * @param  password  string
                * @param  brainkey  string
                *
                * @return void
                */
                void create_file(const path& wallet_file_name,
                    const string& password,
                    const string& brainkey = string());

                /**
                * On the basis of the wallet path to load the local database
                * @param wallet_filename the path of the wallet to open
                *
                * @return void
                */
                void open_file(const path& wallet_filename);
                /**  Start an asynchronous coroutines to trigger relocker function
                *
                *
                * @return void
                */
                void reschedule_relocker();
                /**  function to trigger wallet lock when time is end
                *
                *
                * @return void
                */
                void relocker();

                /**
                 * This method is called anytime the blockchain state changes including
                 * undo operations.
                 */
                /**  state_changed
                *
                * @param  state  PendingChainStatePtr
                *
                * @return void
                */
                virtual void state_changed(const PendingChainStatePtr& state)override;

                /**
                 *  This method is called anytime a block is applied to the chain.
                 */
                /**  block_applied
                *
                * @param  summary  BlockSummary
                *
                * @return void
                */
                virtual void block_applied(const BlockSummary& summary)override;

                void scan_balances_experimental();

				void scan_contracts();

                /**  Get the secret hash according to the block number
                *
                * @param  block_num  uint32_t
                * @param  delegate_key  PrivateKeyType
                *
                * @return SecretHashType
                */
                SecretHashType get_secret(uint32_t block_num,
                    const PrivateKeyType& delegate_key)const;

                void scan_block(uint32_t block_num);

                WalletTransactionEntry scan_transaction(
                    const SignedTransaction& transaction,
                    uint32_t block_num,
                    const time_point_sec block_timestamp,
                    bool overwrite_existing = false,
                    bool bNeedcreate = false
                    );

                void scan_block_experimental(uint32_t block_num,
                    const map<PrivateKeyType, string>& account_keys,
                    const map<Address, string>& account_balances,
                    const set<string>& account_names);

                TransactionLedgerEntry scan_transaction_experimental(const TransactionEvaluationState& eval_state,
                    uint32_t block_num,
                    const time_point_sec timestamp,
                    bool overwrite_existing);

                TransactionLedgerEntry scan_transaction_experimental(const TransactionEvaluationState& eval_state,
                    uint32_t block_num,
                    const time_point_sec timestamp,
                    const map<PrivateKeyType, string>& account_keys,
                    const map<Address, string>& account_balances,
                    const set<string>& account_names,
                    bool overwrite_existing);

                void scan_transaction_experimental(const TransactionEvaluationState& eval_state,
                    const map<PrivateKeyType, string>& account_keys,
                    const map<Address, string>& account_balances,
                    const set<string>& account_names,
                    TransactionLedgerEntry& entry,
                    bool store_entry);

                bool scan_withdraw(const WithdrawOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, PublicKeyType& from_pub_key, bool bNeedcreate);
                bool scan_withdraw_pay(const WithdrawPayOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, bool bNeedcreate);
				bool scan_withdraw_balances(const BalancesWithdrawOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, PublicKeyType key, bool bNeedcreate);
				bool scan_withdraw_contract(const WithdrawContractOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, PublicKeyType key, bool bNeedcreate);
				bool scan_ondestroycontract_withdraw(const OnDestroyOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, PublicKeyType key, bool bNeedcreate);
				bool scan_ondestroycontract_deposit(const OnDestroyOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, bool has_withdrawal);

				bool scan_deposit(const DepositOperation& op, WalletTransactionEntry& trx_rec, Asset& total_fee, bool bHaswithdraw);               
				bool scan_deposit_contract(const DepositContractOperation & op, WalletTransactionEntry& trx_rec, Asset& total_fee, bool bHaswithdraw);

                bool scan_register_account(const RegisterAccountOperation& op, WalletTransactionEntry& trx_rec);
                bool scan_update_account(const UpdateAccountOperation& op, WalletTransactionEntry& trx_rec);

                bool scan_create_asset(const CreateAssetOperation& op, WalletTransactionEntry& trx_rec);
                bool scan_issue_asset(const IssueAssetOperation& op, WalletTransactionEntry& trx_rec);

                //bool scan_update_feed(const update_feed_operation& op, wallet_transaction_entry& trx_rec );


                /**  Get the pending thansactions int the wallet
                *
                *
                * @return vector<WalletTransactionEntry>
                */
                vector<WalletTransactionEntry> get_pending_transactions()const;

                /**  Construct a transaction withdraw operation
                *
                * @param  amount_to_withdraw  Asset
                * @param  from_account_name  string
                * @param  signed transaction to add withdraw operation
                * @param  required_signatures  unordered_set<Address>
                *
                * @return void
                */
                /*
                void withdraw_to_transaction(const Asset& amount_to_withdraw,
                    const string& from_account_name,
                    SignedTransaction& trx,
                    unordered_set<Address>& required_signatures,
                    const Asset& amount_for_refund = Asset());
                */
                void withdraw_to_transaction(const Asset& amount_to_withdraw,
                    const string& from_account_name,
                    SignedTransaction& trx,
                    unordered_set<Address>& required_signatures);

                /**  authorize_update
                *
                * @param  required_signatures  unordered_set<Address>
                * @param  account  oAccountEntry
                * @param  need_owner_key  bool
                *
                * @return void
                */
                void authorize_update(unordered_set<Address>& required_signatures, oAccountEntry account, bool need_owner_key = false);

                /**  Start scan block according block num
                *
                * @param  start_block_num  uint32_t
                * @param  limit  uint32_t
                *
                * @return void
                */
                void start_scan_task(const uint32_t start_block_num, const uint32_t limit);
                void scan_accounts();
                void scan_balances();

                void login_map_cleaner_task();

                /**  upgrade_version
                *

                *
                * @return void
                */
                void upgrade_version();
                /**  upgrade_version_unlocked
                *

                *
                * @return void
                */
                void upgrade_version_unlocked();

                /**  Set the transaction according vote strategy
                *
                * @param  transaction  SignedTransaction
                * @param  strategy  VoteStrategy
                *
                * @return SlateIdType
                */
                SlateIdType set_delegate_slate(SignedTransaction& transaction, const VoteStrategy strategy)const;
                /**  Get Slate record by strategy
                *
                * @param  strategy  VoteStrategy
                *
                * @return SlateEntry
                */
                SlateEntry get_delegate_slate(const VoteStrategy strategy)const;
                /**  is_receive_account
                *
                * @param  account_name  string
                *
                * @return bool
                */
                bool is_receive_account(const string& account_name)const;
                /**  is_unique_account
                *
                * @param  account_name  string
                *
                * @return bool
                */
                bool is_unique_account(const string& account_name)const;

                /**
                *  Creates a new private key under the specified account. This key
                *  will not be valid for sending TITAN transactions to, but will
                *  be able to receive payments directly.
                *  Not used now
                * @param  account_name  string
                *
                * @return PrivateKeyType
                */
                PrivateKeyType  get_new_private_key(const string& account_name);
                /**  Get a public key from the function get_new_private_key
                *
                * @param  account_name  string
                *
                * @return PublicKeyType
                */
                PublicKeyType   get_new_public_key(const string& account_name);
                /**  get_new_address
                *
                * @param  account_name  string
                * @param  label  string
                *
                * @return Address
                */
                Address           get_new_address(const string& account_name, const string& label = "");

                void sign_transaction(SignedTransaction& transaction, const unordered_set<Address>& required_signatures)const;

                TransactionLedgerEntry apply_transaction_experimental(const SignedTransaction& transaction);

                void transfer_to_contract_trx(SignedTransaction& trx, const Address& to_contract_address, const Asset& asset_to_transfer, const Asset& asset_for_exec, const Asset& transaction_fee, const PublicKeyType& from,const map<BalanceIdType, ShareType>& balances);

                void handle_events(const vector<EventOperation>& event_vector);



            };

        }
    }
} // thinkyoung::wallet::detail
