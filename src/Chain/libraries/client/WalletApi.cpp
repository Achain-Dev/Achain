#include <client/Client.hpp>
#include <client/ClientImpl.hpp>
#include <utilities/KeyConversion.hpp>
#include <utilities/Words.hpp>
#include <utilities/CommonApi.hpp>
#include <wallet/Config.hpp>
#include <wallet/Exceptions.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/thread/non_preemptable_scope_check.hpp>
#include "cli/locale.hpp"

namespace thinkyoung {
    namespace client {
        namespace detail {
        
            int8_t detail::ClientImpl::wallet_account_set_approval(const string& account_name, int8_t approval) {
                try {
                    // set limit in  sandbox state
                    if (_chain_db->get_is_in_sandbox())
                        FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                        
                    _wallet->set_account_approval(account_name, approval);
                    return _wallet->get_account_approval(account_name);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name)("approval", approval))
            }
            
            std::vector<thinkyoung::blockchain::AccountEntry> ClientImpl::wallet_get_all_approved_accounts(int8_t approval) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    return _wallet->get_all_approved_accounts(approval);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "account db read fail!")
            }
            
            void detail::ClientImpl::wallet_open(const string& wallet_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->open(fc::trim(wallet_name));
                reschedule_delegate_loop();
            }
            
            fc::optional<variant> detail::ClientImpl::wallet_get_setting(const string& name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->get_setting(name);
            }
            
            void detail::ClientImpl::wallet_set_setting(const string& name, const variant& value) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->set_setting(name, value);
            }
            
            
            void detail::ClientImpl::wallet_create(const string& wallet_name, const string& password, const string& brain_key) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                string trimmed_name = fc::trim(wallet_name);
                
                if (brain_key.size() && brain_key.size() < ALP_WALLET_MIN_BRAINKEY_LENGTH) FC_CAPTURE_AND_THROW(brain_key_too_short);
                
                if (password.size() < ALP_WALLET_MIN_PASSWORD_LENGTH) FC_CAPTURE_AND_THROW(password_too_short);
                
                if (trimmed_name.size() == 0) FC_CAPTURE_AND_THROW(fc::invalid_arg_exception, (trimmed_name));
                
                _wallet->create(trimmed_name, password, brain_key);
                reschedule_delegate_loop();
            }
            
            fc::optional<string> detail::ClientImpl::wallet_get_name() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->is_open() ? _wallet->get_wallet_name() : fc::optional<string>();
            }
            
            void detail::ClientImpl::wallet_close() {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                if (_wallet) {
                    _wallet->close();
                    reschedule_delegate_loop();
                }
            }
            
            void detail::ClientImpl::wallet_backup_create(const fc::path& json_filename)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->export_to_json(json_filename);
            }
            
            void detail::ClientImpl::wallet_backup_restore(const fc::path& json_filename,
                    const string& wallet_name,
                    const string& imported_wallet_passphrase) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->create_from_json(json_filename, wallet_name, imported_wallet_passphrase);
                reschedule_delegate_loop();
            }
            
            // This should be able to get an encrypted private key or WIF key out of any reasonable JSON object
            void read_keys(const fc::variant& vo, vector<PrivateKeyType>& keys, const string& password) {
                ilog("@n read_keys");
                
                try {
                    const auto wif_key = vo.as_string();
                    const auto key = thinkyoung::utilities::wif_to_key(wif_key);
                    
                    if (key.valid()) keys.push_back(*key);
                    
                } catch (...) {
                    //ilog("@n I couldn't parse that as a wif key: ${vo}", ("vo", vo));
                }
                
                try {
                    const auto key_bytes = vo.as<vector<char>>();
                    const auto password_bytes = fc::sha512::hash(password.c_str(), password.size());
                    const auto key_plain_text = fc::aes_decrypt(password_bytes, key_bytes);
                    keys.push_back(fc::raw::unpack<PrivateKeyType>(key_plain_text));
                    
                } catch (...) {
                    //ilog("@n I couldn't parse that as a byte array: ${vo}", ("vo", vo));
                }
                
                try {
                    const auto obj = vo.get_object();
                    
                    for (const auto& kv : obj) {
                        read_keys(kv.value(), keys, password);
                    }
                    
                } catch (const thinkyoung::wallet::invalid_password&) {
                    throw;
                    
                } catch (...) {
                    //ilog("@n I couldn't parse that as an object: ${o}", ("o", vo));
                }
                
                try {
                    const auto arr = vo.as<vector<variant>>();
                    
                    for (const auto& obj : arr) {
                        read_keys(obj, keys, password);
                    }
                    
                } catch (const thinkyoung::wallet::invalid_password&) {
                    throw;
                    
                } catch (...) {
                    //ilog("@n I couldn't parse that as an array: ${o}", ("o", vo));
                }
                
                //ilog("@n I couldn't parse that as anything!: ${o}", ("o", vo));
            }
            /*  check????
            uint32_t detail::ClientImpl::wallet_import_keys_from_json( const fc::path& json_filename,
            const string& imported_wallet_passphrase,
            const string& account )
            { try {
            FC_ASSERT( fc::exists( json_filename ) );
            FC_ASSERT( _wallet->is_open() );
            FC_ASSERT( _wallet->is_unlocked() );
            _wallet->get_account( account );
            
            const auto object = fc::json::from_file<fc::variant>( json_filename );
            vector<private_key_type> keys;
            read_keys( object, keys, imported_wallet_passphrase );
            
            uint32_t count = 0;
            for( const auto& key : keys )
            count += _wallet->friendly_import_private_key( key, account );
            
            _wallet->auto_backup( "json_key_import" );
            ulog( "Successfully imported ${n} new private keys into account ${name}", ("n",count)("name",account) );
            
            _wallet->start_scan( 0, 1 );
            
            return count;
            } FC_CAPTURE_AND_RETHROW( (json_filename) ) }
            */
            
            bool detail::ClientImpl::wallet_set_automatic_backups(bool enabled) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->set_automatic_backups(enabled);
                return _wallet->get_automatic_backups();
            }
            
            uint32_t detail::ClientImpl::wallet_set_transaction_expiration_time(uint32_t secs) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->set_transaction_expiration(secs);
                return _wallet->get_transaction_expiration();
            }
            
            void detail::ClientImpl::wallet_lock() {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->lock();
                reschedule_delegate_loop();
            }
            
            void detail::ClientImpl::wallet_unlock(uint32_t timeout, const string& password) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->unlock(password, timeout);
                reschedule_delegate_loop();
                
                if (_config.wallet_callback_url.size() > 0) {
                    _http_callback_signal_connection =
                        _wallet->wallet_claimed_transaction.connect(
                    [=](LedgerEntry e) {
                        this->wallet_http_callback(_config.wallet_callback_url, e);
                    });
                }
            }
            
            void detail::ClientImpl::wallet_http_callback(const string& url, const LedgerEntry& e) {
                fc::async([=]() {
                    fc::url u(url);
                    
                    if (u.host()) {
                        auto endpoints = fc::resolve(*u.host(), u.port() ? *u.port() : 80);
                        
                        for (auto ep : endpoints) {
                            fc::http::connection con;
                            con.connect_to(ep);
                            auto response = con.request("POST", url, fc::json::to_string(e));
                            
                            if (response.status == fc::http::reply::OK)
                                return;
                        }
                    }
                }
                         );
            }
            
            vector<thinkyoung::wallet::PrettyTransaction> detail::ClientImpl::wallet_transaction_history_splite(const std::string& account_name,
                    const std::string& asset_symbol,
                    int32_t limit,
                    int32_t transaction_type)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto history = _wallet->get_pretty_transaction_history(account_name, 0, -1, asset_symbol, (thinkyoung::wallet::Wallet::TrxType)transaction_type, true);
                    
                    if (limit == 0 || abs(limit) >= history.size()) {
                        return history;
                        
                    } else if (limit > 0) {
                        return vector<PrettyTransaction>(history.begin(), history.begin() + limit);
                        
                    } else {
                        return vector<PrettyTransaction>(history.end() - abs(limit), history.end());
                    }
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "")
            }
            void detail::ClientImpl::wallet_change_passphrase(const std::string& old_password, const string& new_password) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->auto_backup("passphrase_change");
                _wallet->change_passphrase(old_password, new_password);
                reschedule_delegate_loop();
            }
            
            bool detail::ClientImpl::wallet_check_passphrase(const std::string& passphrase) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->check_passphrase(passphrase);
            }
            
            bool detail::ClientImpl::wallet_check_address(const std::string& address) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                size_t first = address.find_first_of("ACT");
                
                if (first != std::string::npos&&first == 0) {
                    string strToAccount;
                    string strSubAccount;
                    _wallet->accountsplit(address, strToAccount, strSubAccount);
                    return Address::is_valid(strToAccount);
                    
                } else {
                    return _wallet->is_valid_account_name(address);
                }
            }
            
            map<TransactionIdType, fc::exception> detail::ClientImpl::wallet_get_pending_transaction_errors(const thinkyoung::blockchain::FilePath& filename)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                const auto& errors = _wallet->get_pending_transaction_errors();
                
                if (filename != "") {
                    FC_ASSERT(!fc::exists(filename));
                    std::ofstream out(filename.c_str());
                    out << fc::json::to_pretty_string(errors);
                }
                
                return errors;
            }
            /*wallet_transaction_entry detail::ClientImpl::wallet_asset_authorize_key( const string& paying_account_name,//check????
                                                                                       const string& symbol,
                                                                                       const string& key,
                                                                                       const object_id_type& meta )
                                                                                       {
                                                                                       address addr;
                                                                                       try {
                                                                                       try { addr = address( public_key_type( key ) ); }
                                                                                       catch ( ... ) { addr = address( key ); }
                                                                                       }
                                                                                       catch ( ... )
                                                                                       {
                                                                                       auto account = _chain_db->get_account_entry( key );
                                                                                       FC_ASSERT( account.valid() );
                                                                                       addr = account->active_key();
                                                                                       }
                                                                                       auto entry = _wallet->asset_authorize_key( paying_account_name, symbol, addr, meta, true );
                                                                                       _wallet->cache_transaction( entry );
                                                                                       network_broadcast_transaction( entry.trx );
                                                                                       return entry;
                                                                                       }*/
            
            WalletTransactionEntry detail::ClientImpl::wallet_publish_slate(const string& publishing_account_name,
                    const string& paying_account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->publish_slate(publishing_account_name, paying_account_name, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            WalletTransactionEntry detail::ClientImpl::wallet_publish_version(const string& publishing_account_name,
                    const string& paying_account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->publish_version(publishing_account_name, paying_account_name, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            WalletTransactionEntry detail::ClientImpl::wallet_collect_genesis_balances(const string& account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                const auto filter = [](const BalanceEntry& entry) -> bool {
                    return entry.condition.type == withdraw_signature_type && entry.snapshot_info.valid();
                };
                auto entry = _wallet->collect_account_balances(account_name, filter, "collect genesis", true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            // wallet_transaction_entry detail::ClientImpl::wallet_collect_vested_balances( const string& account_name )
            // {
            //     const auto filter = []( const balance_entry& entry ) -> bool
            //     {
            //         return entry.condition.type == withdraw_vesting_type;
            //     };
            //     auto entry = _wallet->collect_account_balances( account_name, filter, "collect vested", true );
            //     _wallet->cache_transaction( entry );
            //     network_broadcast_transaction( entry.trx );
            //     return entry;
            // }
            
            // wallet_transaction_entry detail::ClientImpl::wallet_delegate_update_signing_key( const string& authorizing_account_name,
            //                                                                                          const string& delegate_name,
            //                                                                                          const public_key_type& signing_key )
            // {
            //    auto entry = _wallet->update_signing_key( authorizing_account_name, delegate_name, signing_key, true );
            //    _wallet->cache_transaction( entry );
            //    network_broadcast_transaction( entry.trx );
            //    return entry;
            // }
            
            int32_t detail::ClientImpl::wallet_recover_accounts(int32_t accounts_to_recover, int32_t maximum_number_of_attempts) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->auto_backup("before_account_recovery");
                return _wallet->recover_accounts(accounts_to_recover, maximum_number_of_attempts);
            }
            
            // wallet_transaction_entry detail::ClientImpl::wallet_recover_titan_deposit_info( const string& transaction_id_prefix,
            //                                                                                   const string& recipient_account )
            // {
            //     return _wallet->recover_transaction( transaction_id_prefix, recipient_account );
            // }
            
            optional<variant_object> detail::ClientImpl::wallet_verify_titan_deposit(const string& transaction_id_prefix) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->verify_titan_deposit(transaction_id_prefix);
            }
            
            // wallet_transaction_entry detail::ClientImpl::wallet_transfer(
            //         const string& amount_to_transfer,
            //         const string& asset_symbol,
            //         const string& from_account_name,
            //         const string& to_account_name,
            //         const string& memo_message,
            //         const vote_strategy& strategy )
            // {
            //     return wallet_transfer_from(amount_to_transfer, asset_symbol, from_account_name, from_account_name,
            //                                 to_account_name, memo_message, strategy);
            // }
            
            WalletTransactionEntry detail::ClientImpl::wallet_transfer_to_public_account(
                const string& amount_to_transfer,
                const string& asset_symbol,
                const string& from_account_name,
                const string& to_account_name,
                const string& memo_message,
                const VoteStrategy& strategy) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                const oAccountEntry account_entry = _chain_db->get_account_entry(to_account_name);
                FC_ASSERT(account_entry.valid() && !account_entry->is_retracted());
                auto entry = _wallet->transfer_asset_to_address(amount_to_transfer,
                             asset_symbol,
                             from_account_name,
                             account_entry->owner_address(),
                             memo_message,
                             strategy,
                             true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            string  detail::ClientImpl::wallet_address_create(const string& account_name,
                    const string& label,
                    int legacy_network_byte) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto pubkey = _wallet->get_new_public_key(account_name);
                    
                    if (legacy_network_byte == -1)
                        return (string(Address(pubkey)) + INVALIDE_SUB_ADDRESS);
                        
                    else if (legacy_network_byte == 0 || legacy_network_byte == 56)
                        return string(PtsAddress(pubkey, true, legacy_network_byte));
                        
                    else
                        FC_ASSERT(false, "Unsupported network byte");
                }
                
                FC_CAPTURE_AND_RETHROW((account_name)(label)(legacy_network_byte))
            }
            
            
            // wallet_transaction_entry detail::ClientImpl::wallet_transfer_to_legacy_address(
            //         double amount_to_transfer,
            //         const string& asset_symbol,
            //         const string& from_account_name,
            //      const pts_address& to_address,
            //      const string& alp_account,
            //         const string& memo_message,
            //         const vote_strategy& strategy )
            // {
            //     auto entry =  _wallet->transfer_asset_to_address( amount_to_transfer,
            //                                                        asset_symbol,
            //                                                        from_account_name,
            //                                                        address( to_address ),
            //                                                        memo_message,
            //                                                        strategy,
            //                                                        true );
            //     _wallet->cache_transaction( entry );
            //     network_broadcast_transaction( entry.trx );
            //     return entry;
            //
            // }
            
            
            
            WalletTransactionEntry detail::ClientImpl::wallet_transfer_to_address(
                const string& amount_to_transfer,
                const string& asset_symbol,
                const string& from_account_name,
                const string& to_address,
                const string& memo_message,
                const VoteStrategy& strategy) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                string strToAccount;
                string strSubAccount;
                _wallet->accountsplit(to_address, strToAccount, strSubAccount);
                Address effective_address;
                
                if (Address::is_valid(strToAccount))
                    effective_address = Address(strToAccount);
                    
                else
                    effective_address = Address(PublicKeyType(strToAccount));
                    
                auto entry = _wallet->transfer_asset_to_address(amount_to_transfer,
                             asset_symbol,
                             from_account_name,
                             effective_address,
                             memo_message,
                             strategy,
                             true,
                             strSubAccount);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            thinkyoung::blockchain::SignedTransaction ClientImpl::create_transfer_transaction(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_address, const thinkyoung::blockchain::Imessage& memo_message /* = fc::json::from_string("").as<thinkyoung::blockchain::Imessage>() */, const thinkyoung::wallet::VoteStrategy& strategy) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                string strToAccount;
                string strSubAccount;
                _wallet->accountsplit(to_address, strToAccount, strSubAccount);
                Address effective_address;
                
                if (Address::is_valid(strToAccount))
                    effective_address = Address(strToAccount);
                    
                else
                    effective_address = Address(PublicKeyType(strToAccount));
                    
                auto entry = _wallet->transfer_asset_to_address(amount_to_transfer,
                             asset_symbol,
                             from_account_name,
                             effective_address,
                             memo_message,
                             strategy,
                             true,
                             strSubAccount);
                //                _wallet->cache_transaction(entry);
                return entry.trx;
            }
            // wallet_transaction_entry detail::ClientImpl::wallet_transfer_from(
            //         const string& amount_to_transfer,
            //         const string& asset_symbol,
            //         const string& paying_account_name,
            //         const string& from_account_name,
            //      const string& to_account_name,
            //         const string& memo_message,
            //         const vote_strategy& strategy )
            // {
            //     asset amount = _chain_db->to_ugly_asset(amount_to_transfer, asset_symbol);
            //     auto payer = _wallet->get_account(paying_account_name);
            //     auto recipient = _wallet->get_account(to_account_name);
            //     transaction_builder_ptr builder = _wallet->create_transaction_builder();
            //     auto entry = builder->deposit_asset(payer, recipient, amount,
            //                                          memo_message, from_account_name)
            //                           .finalize( true, strategy )
            //                           .sign();
            //     _wallet->cache_transaction( entry );
            //     network_broadcast_transaction( entry.trx );
            //     /*for( auto&& notice : builder->encrypted_notifications() )
            //         _mail_client->send_encrypted_message(std::move(notice),
            //                                              from_account_name,
            //                                              to_account_name,
            //                                              recipient.owner_key);*/
            //
            //     return entry;
            // }
            //
            // balance_id_type detail::ClientImpl::wallet_multisig_get_balance_id(
            //                                         const string& asset_symbol,
            //                                         uint32_t m,
            //                                         const vector<address>& addresses )const
            // {
            //     auto id = _chain_db->get_asset_id( asset_symbol );
            //     return balance_entry::get_multisig_balance_id( id, m, addresses );
            // }
            //
            // wallet_transaction_entry detail::ClientImpl::wallet_multisig_deposit(
            //                                                     const string& amount,
            //                                                     const string& symbol,
            //                                                     const string& from_name,
            //                                                     uint32_t m,
            //                                                     const vector<address>& addresses,
            //                                                     const vote_strategy& strategy )
            // {
            //     asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
            //     auto builder = _wallet->create_transaction_builder();
            //     builder->deposit_asset_to_multisig( ugly_asset, from_name, m, addresses );
            //     auto entry = builder->finalize( true, strategy ).sign();
            //     _wallet->cache_transaction( entry );
            //     network_broadcast_transaction( entry.trx );
            //     return entry;
            // }
            
            TransactionBuilder detail::ClientImpl::wallet_withdraw_from_address(
                const string& amount,
                const string& symbol,
                const Address& from_address,
                const string& to,
                const VoteStrategy& strategy,
                bool sign,
                const string& builder_path) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                Address to_address;
                
                try {
                    auto acct = _wallet->get_account(to);
                    to_address = acct.owner_address();
                    
                } catch (...) {
                    to_address = Address(to);
                }
                
                Asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
                auto builder = _wallet->create_transaction_builder();
                auto fee = _wallet->get_transaction_fee();
                builder->withdraw_from_balance(from_address, ugly_asset.amount + fee.amount);
                builder->deposit_to_balance(to_address, ugly_asset);
                builder->finalize(false, strategy);
                
                if (sign) {
                    builder->sign();
                    network_broadcast_transaction(builder->transaction_entry.trx);
                }
                
                _wallet->write_latest_builder(*builder, builder_path);
                return *builder;
            }
            
            // transaction_builder detail::ClientImpl::wallet_withdraw_from_legacy_address(
            //                                                     const string& amount,
            //                                                     const string& symbol,
            //                                                     const pts_address& from_address,
            //                                                     const string& to,
            //                                                     const vote_strategy& strategy,
            //                                                     bool sign,
            //                                                     const string& builder_path )const
            // {
            //     address to_address;
            //     try {
            //         auto acct = _wallet->get_account( to );
            //         to_address = acct.owner_address();
            //     } catch (...) {
            //         to_address = address( to );
            //     }
            //     asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
            //     auto builder = _wallet->create_transaction_builder();
            //     auto fee = _wallet->get_transaction_fee();
            //     builder->withdraw_from_balance( from_address, ugly_asset.amount + fee.amount );
            //     builder->deposit_to_balance( to_address, ugly_asset );
            //     builder->finalize( false, strategy );
            //     if( sign )
            //         builder->sign();
            //     _wallet->write_latest_builder( *builder, builder_path );
            //     return *builder;
            // }
            //
            // transaction_builder detail::ClientImpl::wallet_multisig_withdraw_start(
            //                                                     const string& amount,
            //                                                     const string& symbol,
            //                                                     const balance_id_type& from,
            //                                                     const address& to_address,
            //                                                     const vote_strategy& strategy,
            //                                                     const string& builder_path )const
            // {
            //     asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
            //     auto builder = _wallet->create_transaction_builder();
            //     auto fee = _wallet->get_transaction_fee();
            //     builder->withdraw_from_balance( from, ugly_asset.amount + fee.amount );
            //     builder->deposit_to_balance( to_address, ugly_asset );
            //     _wallet->write_latest_builder( *builder, builder_path );
            //     return *builder;
            // }
            //
            // transaction_builder detail::ClientImpl::wallet_builder_add_signature(
            //                                             const transaction_builder& builder,
            //                                             bool broadcast )
            // { try {
            //     auto new_builder = _wallet->create_transaction_builder( builder );
            //     if( new_builder->transaction_entry.trx.signatures.empty() )
            //         new_builder->finalize( false );
            //     new_builder->sign();
            //     if( broadcast )
            //     {
            //         try {
            //             network_broadcast_transaction( new_builder->transaction_entry.trx );
            //         }
            //         catch(...) {
            //             ulog("I tried to broadcast the transaction but it was not valid.");
            //         }
            //     }
            //     _wallet->write_latest_builder( *new_builder, "" );
            //     return *new_builder;
            // } FC_CAPTURE_AND_RETHROW( (builder)(broadcast) ) }
            //
            // transaction_builder detail::ClientImpl::wallet_builder_file_add_signature(
            //                                             const string& builder_path,
            //                                             bool broadcast )
            // { try {
            //     auto new_builder = _wallet->create_transaction_builder_from_file( builder_path );
            //     if( new_builder->transaction_entry.trx.signatures.empty() )
            //         new_builder->finalize( false );
            //     new_builder->sign();
            //     if( broadcast )
            //     {
            //         try {
            //             network_broadcast_transaction( new_builder->transaction_entry.trx );
            //         }
            //         catch(...) {
            //             ulog("I tried to broadcast the transaction but it was not valid.");
            //         }
            //     }
            //     _wallet->write_latest_builder( *new_builder, builder_path );
            //     _wallet->write_latest_builder( *new_builder, "" ); // always write to "latest"
            //     return *new_builder;
            // } FC_CAPTURE_AND_RETHROW( (broadcast)(builder_path) ) }
            //
            // wallet_transaction_entry detail::ClientImpl::wallet_release_escrow( const string& paying_account_name,
            //                                                                       const address& escrow_balance_id,
            //                                                                       const string& released_by,
            //                                                                       double amount_to_sender,
            //                                                                       double amount_to_receiver )
            // {
            //     auto payer = _wallet->get_account(paying_account_name);
            //     auto balance_rec = _chain_db->get_balance_entry( escrow_balance_id );
            //     FC_ASSERT( balance_rec.valid() );
            //     FC_ASSERT( balance_rec->condition.type == withdraw_escrow_type );
            //     FC_ASSERT( released_by == "sender" ||
            //                released_by == "receiver" ||
            //                released_by == "agent" );
            //
            //     auto asset_rec = _chain_db->get_asset_entry( balance_rec->asset_id() );
            //     FC_ASSERT( asset_rec.valid() );
            //     if( asset_rec->precision )
            //     {
            //        amount_to_sender   *= asset_rec->precision;
            //        amount_to_receiver *= asset_rec->precision;
            //     }
            //
            //     auto escrow_cond = balance_rec->condition.as<withdraw_with_escrow>();
            //     address release_by_address;
            //
            //     if( released_by == "sender" ) release_by_address = escrow_cond.sender;
            //     if( released_by == "receiver" ) release_by_address = escrow_cond.receiver;
            //     if( released_by == "agent" ) release_by_address = escrow_cond.escrow;
            //
            //     transaction_builder_ptr builder = _wallet->create_transaction_builder();
            //     auto entry = builder->release_escrow( payer, escrow_balance_id, release_by_address, amount_to_sender, amount_to_receiver )
            //   //  TODO: restore this function       .finalize()
            //                                         .sign();
            //
            //     _wallet->cache_transaction( entry );
            //     network_broadcast_transaction( entry.trx );
            //
            //     /* TODO: notify other parties of the transaction.
            //     for( auto&& notice : builder->encrypted_notifications() )
            //         _mail_client->send_encrypted_message(std::move(notice),
            //                                              from_account_name,
            //                                              to_account_name,
            //                                              recipient.owner_key);
            //
            //     */
            //     return entry;
            // }
            //
            // wallet_transaction_entry detail::ClientImpl::wallet_transfer_from_with_escrow(
            //         const string& amount_to_transfer,
            //         const string& asset_symbol,
            //         const string& paying_account_name,
            //         const string& from_account_name,
            //      const string& to_account_name,
            //         const string& escrow_account_name,
            //         const digest_type&   agreement,
            //         const string& memo_message,
            //         const vote_strategy& strategy )
            // {
            //     asset amount = _chain_db->to_ugly_asset(amount_to_transfer, asset_symbol);
            //     auto sender = _wallet->get_account(from_account_name);
            //     auto payer = _wallet->get_account(paying_account_name);
            //     auto recipient = _wallet->get_account(to_account_name);
            //     auto escrow_account = _wallet->get_account(escrow_account_name);
            //     transaction_builder_ptr builder = _wallet->create_transaction_builder();
            //
            //     auto entry = builder->deposit_asset_with_escrow(payer, recipient, escrow_account, agreement,
            //                                                      amount,memo_message, sender.owner_key)
            //                           .finalize( true, strategy )
            //                           .sign();
            //     _wallet->cache_transaction( entry );
            //     network_broadcast_transaction( entry.trx );
            //  /*
            //     for( auto&& notice : builder->encrypted_notifications() )
            //         _mail_client->send_encrypted_message(std::move(notice),
            //                                              from_account_name,
            //                                              to_account_name,
            //                                              recipient.owner_key);
            //                                           */
            //
            //     return entry;
            // }
            
            WalletTransactionEntry detail::ClientImpl::wallet_asset_create(
                const string& symbol,
                const string& asset_name,
                const string& issuer_name,
                const string& description,
                const string& maximum_share_supply,
                uint64_t precision,
                const variant& public_data,
                bool is_market_issued /* = false */) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->create_asset(symbol, asset_name, description, public_data, issuer_name,
                                                   maximum_share_supply, precision, is_market_issued, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            
            // wallet_transaction_entry detail::ClientImpl::wallet_asset_update(  //check???? to recover????
            //         const string& symbol,
            //         const optional<string>& name,
            //         const optional<string>& description,
            //         const optional<variant>& public_data,
            //         const optional<double>& maximum_share_supply,
            //         const optional<uint64_t>& precision,
            //         const share_type& issuer_fee,
            //         double issuer_market_fee,
            //         const vector<asset_permissions>& flags,
            //         const vector<asset_permissions>& issuer_permissions,
            //         const string& issuer_account_name,
            //         uint32_t required_sigs,
            //         const vector<address>& authority
            //       )
            // {
            //    uint32_t flags_int = 0;
            //    uint32_t issuer_perms_int = 0;
            //    for( auto item : flags ) flags_int |= item;
            //    for( auto item : issuer_permissions ) issuer_perms_int |= item;
            //    auto entry = _wallet->update_asset( symbol, name, description, public_data, maximum_share_supply,
            //                                         precision, issuer_fee, issuer_market_fee, flags_int,
            //                                         issuer_perms_int, issuer_account_name,
            //                                         required_sigs, authority, true );
            //
            //    _wallet->cache_transaction( entry );
            //    network_broadcast_transaction( entry.trx );
            //    return entry;
            // }
            
            
            WalletTransactionEntry detail::ClientImpl::wallet_asset_issue(
                const string& real_amount,
                const string& symbol,
                const string& to_account_name,
                const string& memo_message) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->issue_asset(real_amount, symbol, to_account_name, memo_message, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            WalletTransactionEntry detail::ClientImpl::wallet_asset_issue_to_addresses(
                const string& symbol,
                const map<string, ShareType>& addresses) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->issue_asset_to_addresses(symbol, addresses);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            
            vector<string> detail::ClientImpl::wallet_list() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->list();
            }
            
            vector<WalletAccountEntry> detail::ClientImpl::wallet_list_accounts() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->list_accounts();
            }
            
            vector<thinkyoung::wallet::AccountAddressData> detail::ClientImpl::wallet_list_my_addresses() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->list_addresses();
            }
            
            vector<WalletAccountEntry> detail::ClientImpl::wallet_list_my_accounts() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->list_my_accounts();
            }
            //
            // vector<wallet_account_entry> detail::ClientImpl::wallet_list_favorite_accounts() const
            // {
            //   return _wallet->list_favorite_accounts();
            // }
            
            vector<WalletAccountEntry> detail::ClientImpl::wallet_list_unregistered_accounts() const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->list_unregistered_accounts();
            }
            
            void detail::ClientImpl::wallet_remove_contact_account(const string& account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->remove_contact_account(account_name);
            }
            
            void detail::ClientImpl::wallet_account_rename(const string& current_account_name,
                    const string& new_account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->rename_account(current_account_name, new_account_name);
                _wallet->auto_backup("account_rename");
            }
            
            WalletAccountEntry detail::ClientImpl::wallet_get_account(const string& account_name) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    return _wallet->get_account(account_name);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name))
            }
            
            string detail::ClientImpl::wallet_get_account_public_address(const string& account_name) const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto acct = _wallet->get_account(account_name);
                    return (string(acct.owner_address()) + INVALIDE_SUB_ADDRESS);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name))
            }
            
            
            vector<PrettyTransaction> detail::ClientImpl::wallet_account_transaction_history(const string& account_name,
                    const string& asset_symbol,
                    int32_t limit,
                    uint32_t start_block_num,
                    uint32_t end_block_num)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto history = _wallet->get_pretty_transaction_history(account_name, start_block_num, end_block_num, asset_symbol);
                    
                    if (limit == 0 || abs(limit) >= history.size()) {
                        return history;
                        
                    } else if (limit > 0) {
                        return vector<PrettyTransaction>(history.begin(), history.begin() + limit);
                        
                    } else {
                        return vector<PrettyTransaction>(history.end() - abs(limit), history.end());
                    }
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "")
            }
            
            AccountBalanceSummaryType detail::ClientImpl::wallet_account_historic_balance(const time_point& time,
                    const string& account)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    fc::time_point_sec target(time);
                    return _wallet->compute_historic_balance(account,
                            _self->get_chain()->find_block_num(target));
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "")
            }
            
            void detail::ClientImpl::wallet_remove_transaction(const string& transaction_id) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _wallet->remove_transaction_entry(transaction_id);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("transaction_id", transaction_id))
            }
            
            void detail::ClientImpl::wallet_rebroadcast_transaction(const string& transaction_id) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto entrys = _wallet->get_transactions(transaction_id);
                    
                    for (const auto& entry : entrys) {
                        if (entry.is_virtual) continue;
                        
                        network_broadcast_transaction(entry.trx);
                        std::cout << "Rebroadcasted transaction: " << string(entry.trx.id()) << "\n";
                    }
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("transaction_id", transaction_id))
            }
            
            
            string detail::ClientImpl::wallet_import_private_key(const string& wif_key_to_import,
                    const string& account_name,
                    bool create_account,
                    bool wallet_rescan_blockchain) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                optional<string> name;
                
                if (!account_name.empty())
                    name = account_name;
                    
                //add limit for import private key to existing account
                const auto current_account = _wallet->get_wallet_db().lookup_account(account_name);
                
                if (current_account.valid())
                    FC_THROW_EXCEPTION(invalid_name, "This name is already in your wallet!");
                    
                const auto existing_registered_account = _chain_db->get_account_entry(account_name);
                
                if (existing_registered_account.valid())
                    FC_THROW_EXCEPTION(invalid_name, "This name is already registered with the blockchain!");
                    
                const PublicKeyType new_public_key = _wallet->import_wif_private_key(wif_key_to_import, name, create_account);
                
                if (wallet_rescan_blockchain)
                    _wallet->start_scan(0, -1);
                    
                else
                    _wallet->start_scan(0, 1);
                    
                const oWalletAccountEntry account_entry = _wallet->get_account_for_address(Address(new_public_key));
                FC_ASSERT(account_entry.valid(), "No account for the key we just imported!?");
                _wallet->auto_backup("key_import");
                return account_entry->name;
            }
            
            optional<string> detail::ClientImpl::wallet_dump_private_key(const string& input)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    string RelAddress;
                    string Temp;
                    _wallet->accountsplit(input, RelAddress, Temp);
                    
                    try {
                        ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                        //If input is an address...
                        return utilities::key_to_wif(_wallet->get_private_key(Address(RelAddress)));
                        
                    } catch (...) {
                        try {
                            ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                            //If input is a public key...
                            return utilities::key_to_wif(_wallet->get_private_key(Address(PublicKeyType(RelAddress))));
                            
                        } catch (...) {
                        }
                    }
                    
                    return optional<string>();
                }
                
                FC_CAPTURE_AND_RETHROW((input))
            }
            
            optional<string> detail::ClientImpl::wallet_dump_account_private_key(const string& account_name,
                    const AccountKeyType& key_type)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    const auto account_entry = _wallet->get_account(account_name);
                    
                    switch (key_type) {
                        case owner_key:
                            return utilities::key_to_wif(_wallet->get_private_key(account_entry.owner_address()));
                            
                        case active_key:
                            return utilities::key_to_wif(_wallet->get_private_key(account_entry.active_address()));
                            
                        case signing_key:
                            FC_ASSERT(account_entry.is_delegate());
                            return utilities::key_to_wif(_wallet->get_private_key(account_entry.signing_address()));
                            
                        default:
                            return optional<string>();
                    }
                }
                
                FC_CAPTURE_AND_RETHROW((account_name)(key_type))
            }
            
            
            
            
            
            
            Address ClientImpl::wallet_account_create(const string& account_name,
                    const variant& private_data) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                const auto result = _wallet->create_account(account_name, private_data);
                _wallet->auto_backup("account_create");
                return Address(result);
            }
            
            // void ClientImpl::wallet_account_set_favorite( const string& account_name, bool is_favorite )
            // {
            //     _wallet->account_set_favorite( account_name, is_favorite );
            // }
            
            void ClientImpl::wallet_rescan_blockchain(const uint32_t start_block_num, const uint32_t limit) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _wallet->start_scan(start_block_num, limit);
                }
                
                FC_CAPTURE_AND_RETHROW((start_block_num)(limit))
            }
            
            void ClientImpl::wallet_cancel_scan() {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _wallet->cancel_scan();
                }
                
                FC_CAPTURE_AND_RETHROW()
            }
            
            vector<string> ClientImpl::wallet_get_contracts(const string &account_name) {
                try {
                    return _wallet->get_contracts(account_name);
                }
                
                FC_CAPTURE_AND_RETHROW()
            }
            
            void ClientImpl::wallet_scan_contracts() {
                try {
                    _wallet->scan_contracts();
                }
                
                FC_CAPTURE_AND_RETHROW()
            }
            
            WalletTransactionEntry ClientImpl::wallet_scan_transaction(const string& transaction_id, bool overwrite_existing) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    if (transaction_id == "ALL") {
                        return _wallet->scan_all_transaction(overwrite_existing);
                    }
                    
                    return _wallet->scan_transaction(transaction_id, overwrite_existing);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("transaction_id", transaction_id)("overwrite_existing", overwrite_existing))
            }
            
            // void ClientImpl::wallet_scan_transaction_experimental( const string& transaction_id, bool overwrite_existing )
            // { try {
            // #ifndef ALP_TEST_NETWORK
            //    FC_ASSERT( false, "This command is for developer testing only!" );
            // #endif
            //    _wallet->scan_transaction_experimental( transaction_id, overwrite_existing );
            // } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }
            
            // void ClientImpl::wallet_add_transaction_note_experimental( const string& transaction_id, const string& note )
            // { try {
            // #ifndef ALP_TEST_NETWORK
            //    FC_ASSERT( false, "This command is for developer testing only!" );
            // #endif
            //    _wallet->add_transaction_note_experimental( transaction_id, note );
            // } FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("note",note) ) }
            
            // set<pretty_transaction_experimental> ClientImpl::wallet_transaction_history_experimental( const string& account_name )const
            // { try {
            // #ifndef ALP_TEST_NETWORK
            //    FC_ASSERT( false, "This command is for developer testing only!" );
            // #endif
            //    return _wallet->transaction_history_experimental( account_name );
            // } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }
            
            WalletTransactionEntry ClientImpl::wallet_get_transaction(const string& transaction_id) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    return _wallet->get_transaction(transaction_id);
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("transaction_id", transaction_id))
            }
            
            WalletTransactionEntry ClientImpl::wallet_account_register(const string& account_name,
                    const string& pay_with_account,
                    const fc::variant& data,
                    uint8_t delegate_pay_rate,
                    const string& new_account_type) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto entry = _wallet->register_account(account_name, data, delegate_pay_rate,
                                                           pay_with_account, variant(new_account_type).as<AccountType>(),
                                                           true);
                    _wallet->cache_transaction(entry);
                    network_broadcast_transaction(entry.trx);
                    return entry;
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name)("data", data))
            }
            
            variant_object ClientImpl::wallet_get_info() {
                return _wallet->get_info().get_object();
            }
            
            void ClientImpl::wallet_account_update_private_data(const string& account_to_update,
                    const variant& private_data) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->update_account_private_data(account_to_update, private_data);
            }
            
            WalletTransactionEntry ClientImpl::wallet_account_update_registration(
                const string& account_to_update,
                const string& pay_from_account,
                const variant& public_data,
                uint8_t delegate_pay_rate) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->update_registered_account(account_to_update, pay_from_account, public_data, delegate_pay_rate, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            WalletTransactionEntry detail::ClientImpl::wallet_account_update_active_key(const std::string& account_to_update,
                    const std::string& pay_from_account,
                    const std::string& new_active_key) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->update_active_key(account_to_update, pay_from_account, new_active_key, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            WalletTransactionEntry detail::ClientImpl::wallet_account_retract(const std::string& account_to_update,
                    const std::string& pay_from_account) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->retract_account(account_to_update, pay_from_account, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            
            vector<PublicKeySummary> ClientImpl::wallet_account_list_public_keys(const string& account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                vector<PublicKeySummary> summaries;
                vector<PublicKeyType> keys = _wallet->get_public_keys_in_account(account_name);
                summaries.reserve(keys.size());
                
                for (const auto& key : keys) {
                    summaries.push_back(_wallet->get_public_key_summary(key));
                }
                
                return summaries;
            }
            
            // vector<thinkyoung::wallet::escrow_summary> ClientImpl::wallet_escrow_summary( const string& account_name ) const
            // { try {
            //    return _wallet->get_escrow_balances( account_name );
            // } FC_CAPTURE_AND_RETHROW( (account_name) ) }
            
            AccountBalanceSummaryType ClientImpl::wallet_account_balance(const string& account_name)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    if (!account_name.empty() && !_chain_db->is_valid_account_name(account_name))
                        FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                        
                    return _wallet->get_spendable_account_balances(account_name);
                }
                
                FC_CAPTURE_AND_RETHROW((account_name))
            }
            
            AccountBalanceIdSummaryType ClientImpl::wallet_account_balance_ids(const string& account_name)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    return _wallet->get_account_balance_ids(account_name);
                }
                
                FC_CAPTURE_AND_RETHROW((account_name))
            }
            
            // account_extended_balance_type ClientImpl::wallet_account_balance_extended( const string& account_name )const
            // {
            // #if 0
            //     const map<string, vector<balance_entry>>& balance_entrys = _wallet->get_account_balance_entrys( account_name, false, -1 );
            //
            //     map<string, map<string, map<asset_id_type, share_type>>> raw_balances;
            //     for( const auto& item : balance_entrys )
            //     {
            //         const string& account_name = item.first;
            //         for( const auto& balance_entry : item.second )
            //         {
            //             string type_label;
            //             if( balance_entry.snapshot_info.valid() )
            //             {
            //                 switch( withdraw_condition_types( balance_entry.condition.type ) )
            //                 {
            //                     case withdraw_signature_type:
            //                         type_label = "GENESIS";
            //                         break;
            //                     case withdraw_vesting_type:
            //                         type_label = "SHAREDROP";
            //                         break;
            //                     default:
            //                         break;
            //                 }
            //             }
            //             if( type_label.empty() )
            //                 type_label = balance_entry.condition.type_label();
            //
            //             const asset& balance = balance_entry.get_spendable_balance( _chain_db->get_pending_state()->now() );
            //             raw_balances[ account_name ][ type_label ][ balance.asset_id ] += balance.amount;
            //         }
            //     }
            //
            //     map<string, map<string, vector<asset>>> extended_balances;
            //     for( const auto& item : raw_balances )
            //     {
            //         const string& account_name = item.first;
            //         for( const auto& type_item : item.second )
            //         {
            //             const string& type_label = type_item.first;
            //             for( const auto& balance_item : type_item.second )
            //             {
            //                 extended_balances[ account_name ][ type_label ].emplace_back( balance_item.second, balance_item.first );
            //             }
            //         }
            //     }
            //
            //     return extended_balances;
            // #endif
            //     return account_extended_balance_type();
            // }
            
            // account_vesting_balance_summary_type ClientImpl::wallet_account_vesting_balances( const string& account_name )const
            // { try {
            //     return _wallet->get_account_vesting_balances( account_name );
            // } FC_CAPTURE_AND_RETHROW( (account_name) ) }
            //
            
            
            
            
            DelegatePaySalary ClientImpl::wallet_delegate_pay_balance_query(const string& delegate_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto salary = _wallet->query_delegate_salary(delegate_name);
                return salary;
            }
            
            
            std::map<std::string, thinkyoung::blockchain::DelegatePaySalary>  ClientImpl::wallet_active_delegate_salary() {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto salary = _wallet->query_delegate_salarys();
                return salary;
            }
            WalletTransactionEntry ClientImpl::wallet_delegate_withdraw_pay(const string& delegate_name,
                    const string& to_account_name,
                    const string& amount_to_withdraw) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto entry = _wallet->withdraw_delegate_pay(delegate_name, amount_to_withdraw, to_account_name, true);
                _wallet->cache_transaction(entry);
                network_broadcast_transaction(entry.trx);
                return entry;
            }
            void ClientImpl::wallet_set_transaction_imessage_fee_coe(const string& fee_coe) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto ipos = fee_coe.find(".");
                    
                    if (ipos != string::npos) {
                        string str = fee_coe.substr(ipos + 1);
                        int64_t precision_input = static_cast<int64_t>(pow(10, str.size()));
                        FC_ASSERT((precision_input <= ALP_BLOCKCHAIN_PRECISION), "Precision is not correct");
                    }
                    
                    double dFee = std::stod(fee_coe);
                    _wallet->set_transaction_imessage_fee_coe(static_cast<int64_t>(floor(dFee * ALP_BLOCKCHAIN_PRECISION + 0.5)));
                }
                
                FC_CAPTURE_AND_RETHROW((fee_coe))
            }
            double ClientImpl::wallet_get_transaction_imessage_fee_coe() {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                auto fee_coe = _wallet->get_transaction_imessage_fee_coe();
                return ((double)fee_coe) / ALP_BLOCKCHAIN_PRECISION;
            }
            void ClientImpl::wallet_set_transaction_imessage_soft_max_length(int64_t soft_length) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _wallet->set_transaction_imessage_soft_max_length(soft_length);
                }
                
                FC_CAPTURE_AND_RETHROW((soft_length))
            }
            int64_t ClientImpl::wallet_get_transaction_imessage_soft_max_length() {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->get_transaction_imessage_soft_max_length();
            }
            Asset ClientImpl::wallet_set_transaction_fee(const string& fee) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    oAssetEntry asset_entry = _chain_db->get_asset_entry(AssetIdType());
                    FC_ASSERT(asset_entry.valid());
                    FC_ASSERT(utilities::isNumber(fee), "fee is not a number");
                    auto ipos = fee.find(".");
                    
                    if (ipos != string::npos) {
                        string str = fee.substr(ipos + 1);
                        int64_t precision_input = static_cast<int64_t>(pow(10, str.size()));
                        FC_ASSERT((static_cast<uint64_t>(precision_input) <= asset_entry->precision), "Precision is not correct");
                    }
                    
                    double dFee = std::stod(fee);
                    _wallet->set_transaction_fee(Asset(static_cast<ShareType>(floor(dFee * asset_entry->precision + 0.5))));
                    return _wallet->get_transaction_fee();
                }
                
                FC_CAPTURE_AND_RETHROW((fee))
            }
            
            Asset ClientImpl::wallet_get_transaction_fee(const string& fee_symbol) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                if (fee_symbol.empty())
                    return _wallet->get_transaction_fee(_chain_db->get_asset_id(ALP_BLOCKCHAIN_SYMBOL));
                    
                return _wallet->get_transaction_fee(_chain_db->get_asset_id(fee_symbol));
            }
            
            AccountVoteSummaryType ClientImpl::wallet_account_vote_summary(const string& account_name)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                if (!account_name.empty() && !_chain_db->is_valid_account_name(account_name))
                    FC_CAPTURE_AND_THROW(invalid_account_name, (account_name));
                    
                return _wallet->get_account_vote_summary(account_name);
            }
            
            VoteSummary   ClientImpl::wallet_check_vote_status(const string& account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->get_vote_status(account_name);
            }
            
            void ClientImpl::wallet_delegate_set_block_production(const string& delegate_name, bool enabled) {
                _wallet->set_delegate_block_production(delegate_name, enabled);
                reschedule_delegate_loop();
            }
            bool ClientImpl::wallet_get_delegate_statue(const std::string& account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->wallet_get_delegate_statue(account_name);
            }
            bool ClientImpl::wallet_set_transaction_scanning(bool enabled) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->set_transaction_scanning(enabled);
                return _wallet->get_transaction_scanning();
            }
            
            fc::ecc::compact_signature ClientImpl::wallet_sign_hash(const string& signer, const fc::sha256& hash) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->sign_hash(signer, hash);
            }
            
            std::string ClientImpl::wallet_login_start(const std::string &server_account) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->login_start(server_account);
            }
            
            fc::variant ClientImpl::wallet_login_finish(const PublicKeyType &server_key,
                    const PublicKeyType &client_key,
                    const fc::ecc::compact_signature &client_signature) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _wallet->login_finish(server_key, client_key, client_signature);
            }
            
            
            TransactionBuilder ClientImpl::wallet_balance_set_vote_info(const BalanceIdType& balance_id,
                    const string& voter_address,
                    const VoteStrategy& strategy,
                    bool sign_and_broadcast,
                    const string& builder_path) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                Address new_voter;
                
                if (voter_address == "") {
                    auto balance = _chain_db->get_balance_entry(balance_id);
                    
                    if (balance.valid() && balance->restricted_owner.valid())
                        new_voter = *balance->restricted_owner;
                        
                    else
                        FC_ASSERT(false, "Didn't specify a voter address and none currently exists.");
                        
                } else {
                    new_voter = Address(voter_address);
                }
                
                auto builder = _wallet->create_transaction_builder(_wallet->set_vote_info(balance_id, new_voter, strategy));
                
                if (sign_and_broadcast) {
                    auto entry = builder->sign();
                    _wallet->cache_transaction(entry);
                    network_broadcast_transaction(entry.trx);
                }
                
                _wallet->write_latest_builder(*builder, builder_path);
                return *builder;
            }
            
            
            void ClientImpl::wallet_repair_entrys(const string& collecting_account_name) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    _wallet->auto_backup("before_entry_repair");
                    optional<string> account_name;
                    
                    if (!collecting_account_name.empty())
                        account_name = collecting_account_name;
                        
                    return _wallet->repair_entrys(account_name);
                }
                
                FC_CAPTURE_AND_RETHROW((collecting_account_name))
            }
            
            int32_t ClientImpl::wallet_regenerate_keys(const std::string& account, uint32_t number_to_regenerate) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _wallet->auto_backup("before_key_regeneration");
                return _wallet->regenerate_keys(account, number_to_regenerate);
            }
            
            std::string ClientImpl::wallet_transfer_to_address_rpc(const std::string& amount_to_transfer, const std::string& asset_symbol, const std::string& from_account_name, const std::string& to_address, const thinkyoung::blockchain::Imessage& memo_message /* = fc::json::from_string("").as<std::string>() */, const thinkyoung::wallet::VoteStrategy& strategy /* = fc::json::from_string("vote_recommended").as<thinkyoung::wallet::vote_strategy>() */) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto  result = wallet_transfer_to_address(amount_to_transfer, asset_symbol,
                                   from_account_name, to_address, memo_message, strategy);
                    std::string res = "{\"result\":\"SUCCESS\",\"message\":\"";
                    res += result.entry_id.str();
                    res += "\"}";
                    return res;
                    
                } catch (fc::exception e) {
                    std::string res = "{\"result\":\"ERROR\",\"message\":\"";
                    res += e.to_string();
                    res += "\"}";
                    return res;
                }
            }
            std::string detail::ClientImpl::wallet_transfer_to_public_account_rpc(
                const std::string& amount_to_transfer,
                const string& asset_symbol,
                const string& from_account_name,
                const string& to_account_name,
                const thinkyoung::blockchain::Imessage& memo_message,
                const thinkyoung::wallet::VoteStrategy& strategy) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                const oAccountEntry account_record = _chain_db->get_account_entry(to_account_name);
                FC_ASSERT(account_record.valid() && !account_record->is_retracted());
                auto record = _wallet->transfer_asset_to_address(amount_to_transfer,
                              asset_symbol,
                              from_account_name,
                              account_record->owner_address(),
                              memo_message,
                              strategy,
                              true);
                _wallet->cache_transaction(record);
                network_broadcast_transaction(record.trx);
                string result = "{\"result\":\"SUCCESS\",\"message\":\"" + string(record.trx.id()) + "\"}";
                return result;
            }
            string ClientImpl::wallet_account_balance_rpc(const string& account_name)const {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                try {
                    auto result = wallet_account_balance(account_name);
                    std::string res = "{\"result\":\"SUCCESS\",\"message\":\"";
                    
                    if (result.begin() == result.end()) {
                        res += "0\"}";
                        
                    } else {
                        char buf[52] = { 0 };
                        auto re_val = result.begin()->second;
                        auto resu = re_val.find(thinkyoung::blockchain::Asset(0, 0).asset_id);
                        
                        if (resu == re_val.end()) {
                            res += "0\"}";
                            
                        } else {
                            sprintf(buf, "%lld", resu->second);
                            res += buf;
                            res += "\"}";
                        }
                    }
                    
                    return res;
                    
                } catch (fc::exception e) {
                    std::string res = "{\"result\":\"ERROR\",\"message\":\"";
                    res += e.to_string();
                    res += "\"}";
                    return  res;
                }
            }
        }
    }
} // namespace thinkyoung::client::detail
