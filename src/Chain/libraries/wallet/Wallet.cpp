#include <fc/real128.hpp>

#include <wallet/Config.hpp>
#include <wallet/Exceptions.hpp>
#include <wallet/Wallet.hpp>
#include <wallet/WalletImpl.hpp>

#include <blockchain/Time.hpp>
#include <cli/Pretty.hpp>
#include <utilities/GitRevision.hpp>
#include <utilities/KeyConversion.hpp>

#include <algorithm>
#include <fstream>
#include <thread>

//#include <fc/io/json.hpp>//add for fc::json::to_pretty_string(builder)  ???

#include <blockchain/ForkBlocks.hpp>
#include "blockchain/ImessageOperations.hpp"
#include "blockchain/Exceptions.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"
#include <blockchain/ContractOperations.hpp>
#include <utilities/CommonApi.hpp>
namespace thinkyoung {
    namespace wallet {
    
        namespace detail {
        
        
            WalletImpl::WalletImpl() {
                _num_scanner_threads = std::max(_num_scanner_threads, std::thread::hardware_concurrency());
                _scanner_threads.reserve(_num_scanner_threads);
                
                for (uint32_t i = 0; i < _num_scanner_threads; ++i)
                    _scanner_threads.push_back(std::unique_ptr<fc::thread>(new fc::thread("wallet_scanner_" + std::to_string(i))));
            }
            
            WalletImpl::~WalletImpl() {
            }
            
            void WalletImpl::state_changed(const PendingChainStatePtr& state) {
                if (!self->is_open() || !self->is_unlocked()) return;
                
                const auto last_unlocked_scanned_number = self->get_last_scanned_block_number();
                
                if (_blockchain->get_head_block_num() < last_unlocked_scanned_number) {
                    self->set_last_scanned_block_number(_blockchain->get_head_block_num());
                    self->set_last_scanned_block_number_for_alp(_blockchain->get_head_block_num());
                }
            }
            
            void WalletImpl::block_applied(const BlockSummary& summary) {
                if (!self->is_open() || !self->is_unlocked()) return;
                
                auto ntptime = blockchain::ntp_time();
                fc::time_point t = (ntptime.valid() ? *ntp_time() : fc::time_point::now());
                
                if ((t - _blockchain->get_head_block_timestamp()) <= fc::seconds(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 2))
                    handle_events(summary.applied_changes->event_vector);
                    
                if (!self->get_transaction_scanning()) return;
                
                if (summary.block_data.block_num <= self->get_last_scanned_block_number()) return;
                
                self->start_scan(std::min(self->get_last_scanned_block_number() + 1, summary.block_data.block_num), -1);
            }
            void WalletImpl::handle_events(const vector<EventOperation>& event_vector) {
                auto map_end = contract_id_event_to_script_id_vector_db.unordered_end();
                
                for (auto i : event_vector) {
                    ScriptRelationKey key(i.id, i.event_type);
                    auto res = contract_id_event_to_script_id_vector_db.unordered_find(key);
                    
                    if (res == map_end)
                        continue;
                        
                    vector < ScriptIdType > vec_for_remove = res->second;
                    
                    for (auto sid : res->second) {
                        oScriptEntry script = self->get_script_entry(sid);
                        
                        if (!script.valid()) {
                            auto it = vec_for_remove.begin();
                            
                            while (it != vec_for_remove.end()) {
                                if (sid == *it) {
                                    it = vec_for_remove.erase(it);
                                    
                                } else
                                    it++;
                            }
                            
                            continue;
                        }
                        
                        if (script->enable == false)
                            continue;
                            
                        lua::lib::GluaStateScope scope;
                        auto code_stream = lua::api::global_glua_chain_api->get_bytestream_from_code(scope.L(), script->code);
                        
                        if (!code_stream)
                            continue;
                            
                        try {
                            lua::lib::add_global_bool_variable(scope.L(), "truncated", i.is_truncated);
                            lua::lib::add_global_string_variable(scope.L(), "event_type", i.event_type.c_str());
                            lua::lib::add_global_string_variable(scope.L(), "param", i.event_param.c_str());
                            lua::lib::add_global_string_variable(scope.L(), "contract_id", i.id.AddressToString().c_str());
                            lua::lib::run_compiled_bytestream(scope.L(), code_stream.get());
                            
                        } catch (fc::exception e) {
                            printf("Exception:%s\n", e.to_detail_string().c_str());
                        }
                    }
                    
                    if (vec_for_remove.size() < res->second.size())
                        contract_id_event_to_script_id_vector_db.store(key, vec_for_remove);
                }
            }
            
            vector<WalletTransactionEntry> WalletImpl::get_pending_transactions()const {
                return _wallet_db.get_pending_transactions();
            }
            
            void WalletImpl::transfer_to_contract_trx(SignedTransaction& trx, const Address& to_contract_address, const Asset& asset_to_transfer, const Asset& asset_for_exec, const Asset& transaction_fee, const PublicKeyType& from, const  map<BalanceIdType, ShareType>& balances) {
                trx.operations.emplace_back(TransferContractOperation(to_contract_address, asset_to_transfer, asset_for_exec, transaction_fee, from, balances));
            }
            
            /*
            void WalletImpl::withdraw_to_transaction(
                const Asset& amount_to_withdraw,
                const string& from_account_name,
                SignedTransaction& trx,
                unordered_set<Address>& required_signatures,
                const Asset& amount_for_refund
                )
            {
                try {
                    FC_ASSERT(!from_account_name.empty());
            
                    bool deal_for_refund = true;
            
                    auto amount_remaining = amount_for_refund;
                    if (amount_remaining == Asset())
                        amount_remaining.asset_id = amount_to_withdraw.asset_id;
            
                    const AccountBalanceEntrySummaryType balance_entrys = self->get_spendable_account_balance_entrys(from_account_name);
                    if (balance_entrys.find(from_account_name) == balance_entrys.end())
                        FC_CAPTURE_AND_THROW(insufficient_funds, (from_account_name)(amount_to_withdraw)(balance_entrys));
                    for (const auto& entry : balance_entrys.at(from_account_name))
                    {
                        Asset balance = entry.get_spendable_balance(_blockchain->get_pending_state()->now());
                        if (balance.amount <= 0 || balance.asset_id != amount_remaining.asset_id)
                            continue;
            
                        const auto owner = entry.owner();
                        if (!owner.valid())
                            continue;
            
                        //先处理可以退的withdraw，再处理不可以退的withdraw
                        if (deal_for_refund)
                        {
                            if (amount_remaining.amount > balance.amount)
                            {
                                trx.withdraw(entry.id(), balance.amount, true);
                                required_signatures.insert(*owner);
                                amount_remaining -= balance;
                            }
                            else
                            {
                                if (amount_remaining.amount > 0)
                                {
                                    trx.withdraw(entry.id(), amount_remaining.amount, true);
                                    required_signatures.insert(*owner);
                                    balance -= amount_remaining;
                                }
                                deal_for_refund = false;
                                amount_remaining = amount_to_withdraw - amount_for_refund;
                            }
                        }
            
                        if (!deal_for_refund)
                        {
                            if (amount_remaining.amount > balance.amount)
                            {
                                trx.withdraw(entry.id(), balance.amount, false);
                                required_signatures.insert(*owner);
                                amount_remaining -= balance;
                            }
                            else
                            {
                                if (amount_remaining.amount > 0)
                                {
                                    trx.withdraw(entry.id(), amount_remaining.amount, false);
                                    required_signatures.insert(*owner);
                                }
                                return;
                            }
                        }
                    }
            
                    const string required = _blockchain->to_pretty_asset(amount_to_withdraw);
                    const string available = _blockchain->to_pretty_asset(amount_to_withdraw - amount_remaining);
                    FC_CAPTURE_AND_THROW(insufficient_funds, (required)(available)(balance_entrys));
                } FC_CAPTURE_AND_RETHROW((amount_to_withdraw)(from_account_name)(trx)(required_signatures)(amount_for_refund))
            }
            */
            
            void WalletImpl::withdraw_to_transaction(
                const Asset& amount_to_withdraw,
                const string& from_account_name,
                SignedTransaction& trx,
                unordered_set<Address>& required_signatures
            ) {
                try {
                    FC_ASSERT(!from_account_name.empty());
                    auto amount_remaining = amount_to_withdraw;
                    const AccountBalanceEntrySummaryType balance_entrys = self->get_spendable_account_balance_entrys(from_account_name);
                    
                    if (balance_entrys.find(from_account_name) == balance_entrys.end())
                        FC_CAPTURE_AND_THROW(insufficient_funds, (from_account_name)(amount_to_withdraw)(balance_entrys));
                        
                    for (const auto& entry : balance_entrys.at(from_account_name)) {
                        const Asset balance = entry.get_spendable_balance(_blockchain->get_pending_state()->now());
                        
                        if (balance.amount <= 0 || balance.asset_id != amount_remaining.asset_id)
                            continue;
                            
                        const auto owner = entry.owner();
                        
                        if (!owner.valid())
                            continue;
                            
                        if (amount_remaining.amount > balance.amount) {
                            trx.withdraw(entry.id(), balance.amount);
                            required_signatures.insert(*owner);
                            amount_remaining -= balance;
                            
                        } else {
                            trx.withdraw(entry.id(), amount_remaining.amount);
                            required_signatures.insert(*owner);
                            return;
                        }
                    }
                    
                    const string required = _blockchain->to_pretty_asset(amount_to_withdraw);
                    const string available = _blockchain->to_pretty_asset(amount_to_withdraw - amount_remaining);
                    FC_CAPTURE_AND_THROW(insufficient_funds, (required)(available)(balance_entrys));
                }
                
                FC_CAPTURE_AND_RETHROW((amount_to_withdraw)(from_account_name)(trx)(required_signatures))
            }
            
            // TODO: What about retracted accounts?
            void WalletImpl::authorize_update(unordered_set<Address>& required_signatures, oAccountEntry account, bool need_owner_key) {
                oWalletKeyEntry oauthority_key = _wallet_db.lookup_key(account->owner_key);
                // We do this check a lot and it doesn't fit conveniently into a loop because we're interested in two types of keys.
                // Instead, we extract it into a function.
                auto accept_key = [&]()->bool {
                    if (oauthority_key.valid() && oauthority_key->has_private_key()) {
                        required_signatures.insert(oauthority_key->get_address());
                        return true;
                    }
                    
                    return false;
                };
                
                if (accept_key()) return;
                
                if (!need_owner_key) {
                    oauthority_key = _wallet_db.lookup_key(account->active_address());
                    
                    if (accept_key()) return;
                }
                
                auto dot = account->name.find('.');
                
                while (dot != string::npos) {
                    account = _blockchain->get_account_entry(account->name.substr(dot + 1));
                    FC_ASSERT(account.valid(), "Parent account is not valid; this should never happen.");
                    oauthority_key = _wallet_db.lookup_key(account->active_address());
                    
                    if (accept_key()) return;
                    
                    oauthority_key = _wallet_db.lookup_key(account->owner_key);
                    
                    if (accept_key()) return;
                    
                    dot = account->name.find('.');
                }
            }
            
            SecretHashType WalletImpl::get_secret(uint32_t block_num,
                                                  const PrivateKeyType& delegate_key)const {
                BlockIdType header_id;
                
                if (block_num != uint32_t(-1) && block_num > 1) {
                    auto block_header = _blockchain->get_block_header(block_num - 1);
                    header_id = block_header.id();
                }
                
                fc::sha512::encoder key_enc;
                fc::raw::pack(key_enc, delegate_key);
                fc::sha512::encoder enc;
                fc::raw::pack(enc, key_enc.result());
                fc::raw::pack(enc, header_id);
                return fc::ripemd160::hash(enc.result());
            }
            
            void WalletImpl::reschedule_relocker() {
                if (!_relocker_done.valid() || _relocker_done.ready())
                    _relocker_done = fc::async([this]() {
                    relocker();
                }, "wallet_relocker");
            }
            
            void WalletImpl::relocker() {
                fc::time_point now = fc::time_point::now();
                ilog("Starting wallet relocker task at time: ${t}", ("t", now));
                
                if (!_scheduled_lock_time.valid() || now >= *_scheduled_lock_time) {
                    /* Don't relock if we have enabled delegates */
                    if (!self->get_my_delegates(enabled_delegate_status).empty()) {
                        ulog("Wallet not automatically relocking because there are enabled delegates!");
                        return;
                    }
                    
                    self->lock();
                    
                } else {
                    if (!_relocker_done.canceled()) {
                        ilog("Scheduling wallet relocker task for time: ${t}", ("t", *_scheduled_lock_time));
                        _relocker_done = fc::schedule([this]() {
                            relocker();
                        },
                        *_scheduled_lock_time,
                        "wallet_relocker");
                    }
                }
            }
            
            void WalletImpl::start_scan_task(const uint32_t start_block_num, const uint32_t limit) {
                try {
                    fc::oexception scan_exception;
                    
                    try {
                        const uint32_t head_block_num = _blockchain->get_head_block_num();
                        uint32_t current_block_num = std::max(start_block_num, 1u);
                        uint32_t prev_block_num = current_block_num - 1;
                        uint32_t count = 0;
                        const bool track_progress = current_block_num < head_block_num && limit > 0;
                        
                        if (track_progress) {
                            ulog("Beginning scan at block ${n}...", ("n", current_block_num));
                            _scan_progress = 0;
                        }
                        
                        const auto update_progress = [=](const uint32_t count) {
                            if (!track_progress) return;
                            
                            const uint32_t total = std::min({ limit, head_block_num, head_block_num - current_block_num + 1 });
                            
                            if (total == 0) return;
                            
                            _scan_progress = float(count) / total;
                            
                            if (count % 10000 == 0) ulog("Scanning ${p} done...", ("p", cli::pretty_percent(_scan_progress, 1)));
                        };
                        
                        if (start_block_num == 0) {
                            scan_balances();
                            scan_accounts();
                            scan_contracts();
                            
                        } else if (_dirty_accounts) {
                            scan_accounts();
                            
                        } else if (_dirty_contracts) {
                            scan_contracts();
                        }
                        
                        while (current_block_num <= head_block_num && count < limit && !_scan_in_progress.canceled()) {
                            try {
                                scan_block(current_block_num);
                                
                            } catch (const fc::exception& e) {
                                elog("Error scanning block ${n}: ${e}", ("n", current_block_num)("e", e.to_detail_string()));
                            }
                            
                            ++count;
                            prev_block_num = current_block_num;
                            self->set_last_scanned_block_number_for_alp(prev_block_num);
                            ++current_block_num;
                            
                            if (count > 1) {
                                update_progress(count);
                                
                                if (count % 10 == 0) fc::usleep(fc::microseconds(1));
                            }
                        }
                        
                        self->set_last_scanned_block_number(std::max(prev_block_num, self->get_last_scanned_block_number()));
                        
                        if (track_progress) {
                            _scan_progress = 1;
                            ulog("Scan complete.");
                        }
                        
                        if (_dirty_balances) scan_balances_experimental();
                        
                        if (_dirty_accounts) scan_accounts();
                        
                        if (_dirty_contracts) scan_contracts();
                        
                    } catch (const fc::exception& e) {
                        scan_exception = e;
                    }
                    
                    if (scan_exception.valid()) {
                        _scan_progress = -1;
                        ulog("Scan failure.");
                        throw *scan_exception;
                    }
                }
                
                FC_CAPTURE_AND_RETHROW((start_block_num)(limit))
            }
            
            void WalletImpl::upgrade_version() {
                const uint32_t current_version = self->get_version();
                
                if (current_version > ALP_WALLET_VERSION) {
                    FC_THROW_EXCEPTION(unsupported_version, "Wallet version newer than client supports!",
                                       ("wallet_version", current_version)("supported_version", ALP_WALLET_VERSION));
                                       
                } else if (current_version == ALP_WALLET_VERSION) {
                    return;
                }
                
                ulog("Upgrading wallet...");
                std::exception_ptr upgrade_failure_exception;
                
                try {
                    self->auto_backup("version_upgrade");
                    
                    if (current_version < 100) {
                        self->set_automatic_backups(true);
                        self->set_transaction_scanning(self->get_my_delegates(enabled_delegate_status).empty());
                        /* Check for old index format genesis claim virtual transactions */
                        auto present = false;
                        _blockchain->scan_balances([&](const BalanceEntry& bal_rec) {
                            if (!bal_rec.snapshot_info.valid()) return;
                            
                            const auto id = bal_rec.id().addr;
                            present |= _wallet_db.lookup_transaction(id).valid();
                        });
                        
                        if (present) {
                            const function<void(void)> rescan = [&]() {
                                /* Upgrade genesis claim virtual transaction indexes */
                                _blockchain->scan_balances([&](const BalanceEntry& bal_rec) {
                                    if (!bal_rec.snapshot_info.valid()) return;
                                    
                                    const auto id = bal_rec.id().addr;
                                    _wallet_db.remove_transaction(id);
                                });
                                scan_balances();
                            };
                            _unlocked_upgrade_tasks.push_back(rescan);
                        }
                    }
                    
                    if (current_version < 101) {
                        /* Check for old index format market order virtual transactions */
                        auto present = false;
                        const auto items = _wallet_db.get_transactions();
                        
                        for (const auto& item : items) {
                            const auto id = item.first;
                            const auto trx_rec = item.second;
                            
                            if (trx_rec.is_virtual && trx_rec.is_market) {
                                present = true;
                                _wallet_db.remove_transaction(id);
                            }
                        }
                        
                        if (present) {
                            const auto start = 1;
                            const auto end = _blockchain->get_head_block_num();
                            /* Upgrade market order virtual transaction indexes */
                            //for( auto block_num = start; block_num <= end; block_num++ )
                            //{
                            //const auto block_timestamp = _blockchain->get_block_header( block_num ).timestamp;
                            // const auto market_trxs = _blockchain->get_market_transactions( block_num );
                            //for( const auto& market_trx : market_trxs )
                            //scan_market_transaction( market_trx, block_num, block_timestamp );
                            //}
                        }
                    }
                    
                    if (current_version < 102) {
                        self->set_transaction_fee(Asset(ALP_WALLET_DEFAULT_TRANSACTION_FEE));
                        self->set_transaction_imessage_fee_coe(ALP_BLOCKCHAIN_MIN_MESSAGE_FEE_COE);
                        self->set_transaction_imessage_soft_max_length(ALP_BLOCKCHAIN_MAX_SOFT_MAX_MESSAGE_SIZE);
                    }
                    
                    if (current_version < 106) {
                        self->set_transaction_expiration(ALP_WALLET_DEFAULT_TRANSACTION_EXPIRATION_SEC);
                    }
                    
                    if (current_version < 107) {
                        const auto items = _wallet_db.get_transactions();
                        
                        for (const auto& item : items) {
                            const auto id = item.first;
                            const auto trx_rec = item.second;
                            
                            if (trx_rec.is_virtual && trx_rec.is_market && trx_rec.block_num == 554801)
                                _wallet_db.remove_transaction(id);
                        }
                    }
                    
                    if (current_version < 111) {
                        self->set_transaction_imessage_fee_coe(ALP_BLOCKCHAIN_MIN_MESSAGE_FEE_COE);
                        self->set_transaction_imessage_soft_max_length(ALP_BLOCKCHAIN_MAX_SOFT_MAX_MESSAGE_SIZE);
                        const function<void(void)> repair = [&]() {
                            self->repair_entrys(optional<string>());
                            self->start_scan(0, -1);
                        };
                        _unlocked_upgrade_tasks.push_back(repair);
                    }
                    
                    if (current_version < 112) {
                        self->set_last_scanned_block_number_for_alp(0);
                    }
                    
                    if (_unlocked_upgrade_tasks.empty()) {
                        self->set_version(ALP_WALLET_VERSION);
                        ulog("Wallet successfully upgraded.");
                        
                    } else {
                        ulog("Please unlock your wallet to complete the upgrade...");
                    }
                    
                } catch (...) {
                    upgrade_failure_exception = std::current_exception();
                }
                
                if (upgrade_failure_exception) {
                    ulog("Wallet upgrade failure.");
                    std::rethrow_exception(upgrade_failure_exception);
                }
            }
            
            void WalletImpl::upgrade_version_unlocked() {
                if (_unlocked_upgrade_tasks.empty()) return;
                
                ulog("Continuing wallet upgrade...");
                std::exception_ptr upgrade_failure_exception;
                
                try {
                    for (const auto& task : _unlocked_upgrade_tasks) task();
                    
                    _unlocked_upgrade_tasks.clear();
                    self->set_version(ALP_WALLET_VERSION);
                    ulog("Wallet successfully upgraded.");
                    
                } catch (...) {
                    upgrade_failure_exception = std::current_exception();
                }
                
                if (upgrade_failure_exception) {
                    ulog("Wallet upgrade failure.");
                    std::rethrow_exception(upgrade_failure_exception);
                }
            }
            
            
            void WalletImpl::create_file(const path& wallet_file_path,
                                         const string& password,
                                         const string& brainkey) {
                try {
                    FC_ASSERT(self->is_enabled(), "Wallet is disabled in this client!");
                    
                    if (fc::exists(wallet_file_path))
                        FC_THROW_EXCEPTION(wallet_already_exists, "Wallet file already exists!", ("wallet_file_path", wallet_file_path));
                        
                    if (password.size() < ALP_WALLET_MIN_PASSWORD_LENGTH)
                        FC_THROW_EXCEPTION(password_too_short, "Password too short!", ("size", password.size()));
                        
                    std::exception_ptr create_file_failure;
                    
                    try {
                        self->close();
                        _wallet_db.open(wallet_file_path);
                        _wallet_password = fc::sha512::hash(password.c_str(), password.size());
                        MasterKey new_master_key;
                        ExtendedPrivateKey epk;
                        
                        if (!brainkey.empty()) {
                            auto base = fc::sha512::hash(brainkey.c_str(), brainkey.size());
                            
                            /* strengthen the key a bit */
                            for (uint32_t i = 0; i < 100ll * 1000ll; ++i)
                                base = fc::sha512::hash(base);
                                
                            epk = ExtendedPrivateKey(base);
                            
                        } else {
                            wlog("generating random");
                            epk = ExtendedPrivateKey(PrivateKeyType::generate());
                        }
                        
                        _wallet_db.set_master_key(epk, _wallet_password);
                        self->set_version(ALP_WALLET_VERSION);
                        self->set_automatic_backups(true);
                        self->set_transaction_scanning(true);
                        self->set_last_scanned_block_number(_blockchain->get_head_block_num());
                        self->set_last_scanned_block_number_for_alp(0);
                        self->set_transaction_fee(Asset(ALP_WALLET_DEFAULT_TRANSACTION_FEE));
                        self->set_transaction_expiration(ALP_WALLET_DEFAULT_TRANSACTION_EXPIRATION_SEC);
                        self->set_transaction_imessage_fee_coe(ALP_BLOCKCHAIN_MIN_MESSAGE_FEE_COE);
                        self->set_transaction_imessage_soft_max_length(ALP_BLOCKCHAIN_MAX_SOFT_MAX_MESSAGE_SIZE);
                        _wallet_db.close();
                        _wallet_db.open(wallet_file_path);
                        _current_wallet_path = wallet_file_path;
                        script_id_to_script_entry_db.close();
                        script_id_to_script_entry_db.open(wallet_file_path / "script_id_to_script_entry_db");
                        contract_id_event_to_script_id_vector_db.close();
                        contract_id_event_to_script_id_vector_db.open(wallet_file_path / "contract_id_event_to_script_id_vector_db");
                        FC_ASSERT(_wallet_db.validate_password(_wallet_password));
                        
                    } catch (...) {
                        create_file_failure = std::current_exception();
                    }
                    
                    if (create_file_failure) {
                        self->close();
                        fc::remove_all(wallet_file_path);
                        std::rethrow_exception(create_file_failure);
                    }
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "Unable to create wallet '${wallet_file_path}'", ("wallet_file_path", wallet_file_path))
            }
            
            void WalletImpl::open_file(const path& wallet_file_path) {
                try {
                    FC_ASSERT(self->is_enabled(), "Wallet is disabled in this client!");
                    
                    if (!fc::exists(wallet_file_path))
                        FC_THROW_EXCEPTION(no_such_wallet, "No such wallet exists!", ("wallet_file_path", wallet_file_path));
                        
                    if (self->is_open() && _current_wallet_path == wallet_file_path)
                        return;
                        
                    std::exception_ptr open_file_failure;
                    
                    try {
                        self->close();
                        _current_wallet_path = wallet_file_path;
                        _wallet_db.open(wallet_file_path);
                        upgrade_version();
                        self->set_data_directory(fc::absolute(wallet_file_path.parent_path()));
                        script_id_to_script_entry_db.close();
                        script_id_to_script_entry_db.open(wallet_file_path / "script_id_to_script_entry_db");
                        contract_id_event_to_script_id_vector_db.close();
                        contract_id_event_to_script_id_vector_db.open(wallet_file_path / "contract_id_event_to_script_id_vector_db");
                        
                    } catch (...) {
                        open_file_failure = std::current_exception();
                    }
                    
                    if (open_file_failure) {
                        self->close();
                        std::rethrow_exception(open_file_failure);
                    }
                }
                
                FC_RETHROW_EXCEPTIONS(warn, "Unable to open wallet ${wallet_file_path}", ("wallet_file_path", wallet_file_path))
            }
            
            /**
             *  Creates a new private key under the specified account. This key
             *  will not be valid for sending TITAN transactions to, but will
             *  be able to receive payments directly.
             */
            PrivateKeyType WalletImpl::get_new_private_key(const string& account_name) {
                try {
                    if (NOT self->is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                    
                    if (NOT self->is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                    
                    if (NOT is_receive_account(account_name))
                        FC_CAPTURE_AND_THROW(unknown_receive_account, (account_name));
                        
                    const auto current_account = _wallet_db.lookup_account(account_name);
                    FC_ASSERT(current_account.valid());
                    return _wallet_db.generate_new_account_child_key(_wallet_password, account_name);
                }
                
                FC_CAPTURE_AND_RETHROW((account_name))
            }
            
            PublicKeyType WalletImpl::get_new_public_key(const string& account_name) {
                try {
                    return get_new_private_key(account_name).get_public_key();
                }
                
                FC_CAPTURE_AND_RETHROW((account_name))
            }
            
            Address WalletImpl::get_new_address(const string& account_name, const string& label) {
                try {
                    auto addr = Address(get_new_public_key(account_name));
                    auto okey = _wallet_db.lookup_key(addr);
                    FC_ASSERT(okey.valid(), "Key I just created does not exist");
                    _wallet_db.store_key(*okey);
                    return addr;
                }
                
                FC_CAPTURE_AND_RETHROW((account_name))
            }
            
            SlateIdType WalletImpl::set_delegate_slate(SignedTransaction& transaction, const VoteStrategy strategy)const {
                try {
                    SlateIdType slate_id = 0;
                    
                    if (strategy == vote_none) return slate_id;
                    
                    const SlateEntry entry = get_delegate_slate(strategy);
                    slate_id = entry.id();
                    
                    if (slate_id == 0) return slate_id;
                    
                    if (!_blockchain->get_slate_entry(slate_id).valid()) transaction.define_slate(entry.slate);
                    
                    transaction.set_slates(slate_id);
                    return slate_id;
                }
                
                FC_CAPTURE_AND_RETHROW((transaction)(strategy))
            }
            
            /**
             *  Any account for which this wallet owns the active private key.
             */
            bool WalletImpl::is_receive_account(const string& account_name)const {
                FC_ASSERT(self->is_open());
                
                if (!_blockchain->is_valid_account_name(account_name)) return false;
                
                auto opt_account = _wallet_db.lookup_account(account_name);
                
                if (!opt_account.valid()) return false;
                
                auto opt_key = _wallet_db.lookup_key(opt_account->active_address());
                
                if (!opt_key.valid()) return false;
                
                return opt_key->has_private_key();
            }
            
            bool WalletImpl::is_unique_account(const string& account_name)const {
                //There are two possibilities here. First, the wallet has multiple entrys named account_name
                //Second, the wallet has a different entry named account_name than the blockchain does.
                //Check that the wallet has at most one account named account_name
                auto known_accounts = _wallet_db.get_accounts();
                bool found = false;
                
                for (const auto& known_account : known_accounts) {
                    if (known_account.second.name == account_name) {
                        if (found) return false;
                        
                        found = true;
                    }
                }
                
                if (!found)
                    //The wallet does not contain an account with this name. No conflict is possible.
                    return true;
                    
                //The wallet has an account named account_name. Check that it matches with the blockchain
                auto local_account = _wallet_db.lookup_account(account_name);
                auto registered_account = _blockchain->get_account_entry(account_name);
                
                if (local_account && registered_account)
                    return local_account->owner_key == registered_account->owner_key;
                    
                return local_account || registered_account;
            }
            
            /**
             *  Select a slate of delegates from those approved by this wallet. Specify
             *  strategy as vote_none, vote_all, or vote_random. The slate
             *  returned will contain no more than ALP_BLOCKCHAIN_MAX_SLATE_SIZE delegates.
             */
            SlateEntry WalletImpl::get_delegate_slate(const VoteStrategy strategy)const {
                if (strategy == vote_none)
                    return SlateEntry();
                    
                vector<AccountIdType> for_candidates;
                const auto account_items = _wallet_db.get_accounts();
                
                for (const auto& item : account_items) {
                    const auto account_entry = item.second;
                    
                    if (!account_entry.is_delegate() && strategy != vote_recommended) continue;
                    
                    if (account_entry.approved <= 0) continue;
                    
                    for_candidates.push_back(account_entry.id);
                }
                
                std::random_shuffle(for_candidates.begin(), for_candidates.end());
                size_t slate_size = 0;
                
                if (strategy == vote_all) {
                    slate_size = std::min<size_t>(ALP_BLOCKCHAIN_MAX_SLATE_SIZE, for_candidates.size());
                    
                } else if (strategy == vote_random) {
                    slate_size = std::min<size_t>(ALP_BLOCKCHAIN_MAX_SLATE_SIZE / 3, for_candidates.size());
                    slate_size = rand() % (slate_size + 1);
                    
                } else if (strategy == vote_recommended && for_candidates.size() < ALP_BLOCKCHAIN_MAX_SLATE_SIZE) {
                    unordered_map<AccountIdType, int> recommended_candidate_ranks;
                    
                    //Tally up the recommendation count for all delegates recommended by delegates I approve of
                    for (const auto approved_candidate : for_candidates) {
                        oAccountEntry candidate_entry = _blockchain->get_account_entry(approved_candidate);
                        
                        if (!candidate_entry.valid()) continue;
                        
                        if (candidate_entry->is_retracted()) continue;
                        
                        if (!candidate_entry->public_data.is_object()
                                || !candidate_entry->public_data.get_object().contains("slate_id"))
                            continue;
                            
                        if (!candidate_entry->public_data.get_object()["slate_id"].is_uint64()) {
                            //Delegate is doing something non-kosher with their slate_id. Disapprove of them.
                            self->set_account_approval(candidate_entry->name, -1);
                            continue;
                        }
                        
                        oSlateEntry recommendations = _blockchain->get_slate_entry(candidate_entry->public_data.get_object()["slate_id"].as<SlateIdType>());
                        
                        if (!recommendations.valid()) {
                            //Delegate is doing something non-kosher with their slate_id. Disapprove of them.
                            self->set_account_approval(candidate_entry->name, -1);
                            continue;
                        }
                        
                        for (const auto recommended_candidate : recommendations->slate)
                            ++recommended_candidate_ranks[recommended_candidate];
                    }
                    
                    //Disqualify non-delegates and delegates I actively disapprove of
                    for (const auto& acct_rec : account_items)
                        if (!acct_rec.second.is_delegate() || acct_rec.second.approved < 0)
                            recommended_candidate_ranks.erase(acct_rec.second.id);
                            
                    //Remove from rankings candidates I already approve of
                    for (const auto approved_id : for_candidates)
                        if (recommended_candidate_ranks.find(approved_id) != recommended_candidate_ranks.end())
                            recommended_candidate_ranks.erase(approved_id);
                            
                    //Remove non-delegates from for_candidates
                    vector<AccountIdType> delegates;
                    
                    for (const auto id : for_candidates) {
                        const oAccountEntry entry = _blockchain->get_account_entry(id);
                        
                        if (!entry.valid()) continue;
                        
                        if (!entry->is_delegate()) continue;
                        
                        if (entry->is_retracted()) continue;
                        
                        delegates.push_back(id);
                    }
                    
                    for_candidates = delegates;
                    
                    //While I can vote for more candidates, and there are more recommendations to vote for...
                    while (for_candidates.size() < ALP_BLOCKCHAIN_MAX_SLATE_SIZE && recommended_candidate_ranks.size() > 0) {
                        int best_rank = 0;
                        AccountIdType best_ranked_candidate;
                        
                        //Add highest-ranked candidate to my list to vote for and remove him from rankings
                        for (const auto& ranked_candidate : recommended_candidate_ranks)
                            if (ranked_candidate.second > best_rank) {
                                best_rank = ranked_candidate.second;
                                best_ranked_candidate = ranked_candidate.first;
                            }
                            
                        for_candidates.push_back(best_ranked_candidate);
                        recommended_candidate_ranks.erase(best_ranked_candidate);
                    }
                    
                    slate_size = for_candidates.size();
                }
                
                SlateEntry entry;
                
                for (const AccountIdType id : for_candidates) entry.slate.insert(id);
                
                FC_ASSERT(entry.slate.size() <= ALP_BLOCKCHAIN_MAX_SLATE_SIZE);
                return entry;
            }
            
            
            
        } // thinkyoung::Wallet::detail
        
        Wallet::Wallet(ChainDatabasePtr blockchain, bool enabled)
            : my(new detail::WalletImpl()) {
            my->self = this;
            my->_is_enabled = enabled;
            my->_blockchain = blockchain;
            my->_blockchain->add_observer(my.get());
            this->_generating_block = false;
        }
        
        Wallet::~Wallet() {
            close();
        }
        
        void Wallet::set_data_directory(const path& data_dir) {
            my->_data_directory = data_dir;
        }
        
        path Wallet::get_data_directory()const {
            return my->_data_directory;
        }
        
        WalletDb& Wallet::get_wallet_db() const {
            return my->_wallet_db;
        }
        
        void Wallet::scan_contracts() {
            my->scan_contracts();
        }
        
        void Wallet::create(const string& wallet_name,
                            const string& password,
                            const string& brainkey) {
            try {
                FC_ASSERT(is_enabled(), "Wallet is disabled in this client!");
                
                if (!my->_blockchain->is_valid_account_name(wallet_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid name for a wallet!", ("wallet_name", wallet_name));
                    
                auto wallet_file_path = fc::absolute(get_data_directory()) / wallet_name;
                
                if (fc::exists(wallet_file_path))
                    FC_THROW_EXCEPTION(wallet_already_exists, "Wallet name already exists!", ("wallet_name", wallet_name));
                    
                if (password.size() < ALP_WALLET_MIN_PASSWORD_LENGTH)
                    FC_THROW_EXCEPTION(password_too_short, "Password too short!", ("size", password.size()));
                    
                std::exception_ptr wallet_create_failure;
                
                try {
                    my->create_file(wallet_file_path, password, brainkey);
                    open(wallet_name);
                    unlock(password, ALP_WALLET_DEFAULT_UNLOCK_TIME_SEC);
                    
                } catch (...) {
                    wallet_create_failure = std::current_exception();
                }
                
                if (wallet_create_failure) {
                    close();
                    std::rethrow_exception(wallet_create_failure);
                }
            }
            
            FC_RETHROW_EXCEPTIONS(warn, "Unable to create wallet '${wallet_name}'", ("wallet_name", wallet_name))
        }
        
        void Wallet::open(const string& wallet_name) {
            try {
                FC_ASSERT(is_enabled(), "Wallet is disabled in this client!");
                
                if (!my->_blockchain->is_valid_account_name(wallet_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid name for a wallet!", ("wallet_name", wallet_name));
                    
                auto wallet_file_path = fc::absolute(get_data_directory()) / wallet_name;
                
                if (!fc::exists(wallet_file_path))
                    FC_THROW_EXCEPTION(no_such_wallet, "No such wallet exists!", ("wallet_name", wallet_name));
                    
                std::exception_ptr open_file_failure;
                
                try {
                    my->open_file(wallet_file_path);
                    
                } catch (...) {
                    open_file_failure = std::current_exception();
                }
                
                if (open_file_failure) {
                    close();
                    std::rethrow_exception(open_file_failure);
                }
                
                my->scan_accounts();
                my->scan_balances_experimental();
                my->scan_contracts();
            }
            
            FC_CAPTURE_AND_RETHROW((wallet_name))
        }
        
        void Wallet::close() {
            try {
                lock();
                
                try {
                    ilog("Canceling wallet relocker task...");
                    my->_relocker_done.cancel_and_wait("Wallet::close()");
                    ilog("Wallet relocker task canceled");
                    
                } catch (const fc::exception& e) {
                    wlog("Unexpected exception from wallet's relocker() : ${e}", ("e", e));
                    
                } catch (...) {
                    wlog("Unexpected exception from wallet's relocker()");
                }
                
                my->_balance_entrys.clear();
                my->_dirty_balances = true;
                my->_wallet_db.close();
                my->script_id_to_script_entry_db.close();
                my->contract_id_event_to_script_id_vector_db.close();
                my->_current_wallet_path = fc::path();
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        bool Wallet::is_enabled() const {
            return my->_is_enabled;
        }
        
        bool Wallet::is_open()const {
            return my->_wallet_db.is_open();
        }
        
        string Wallet::get_wallet_name()const {
            return my->_current_wallet_path.filename().generic_string();
        }
        
        void Wallet::export_to_json(const path& filename)const {
            try {
                if (fc::exists(filename))
                    FC_THROW_EXCEPTION(file_already_exists, "Filename to export to already exists!", ("filename", filename));
                    
                if (filename == "")
                    FC_THROW_EXCEPTION(filename_not_regular, "Filename isn't a regular name!", ("filename", filename));
                    
                FC_ASSERT(is_open(), "Wallet not open!");
                my->_wallet_db.export_to_json(filename);
            }
            
            FC_CAPTURE_AND_RETHROW((filename))
        }
        
        void Wallet::create_from_json(const path& filename, const string& wallet_name, const string& passphrase) {
            try {
                FC_ASSERT(is_enabled(), "Wallet is disabled in this client!");
                
                if (!fc::exists(filename))
                    FC_THROW_EXCEPTION(file_not_found, "Filename to import from could not be found!", ("filename", filename));
                    
                if (!my->_blockchain->is_valid_account_name(wallet_name))
                    FC_THROW_EXCEPTION(invalid_wallet_name, "Invalid name for a wallet!", ("wallet_name", wallet_name));
                    
                create(wallet_name, passphrase);
                std::exception_ptr import_failure;
                
                try {
                    set_version(0);
                    my->_wallet_db.import_from_json(filename);
                    close();
                    open(wallet_name);
                    unlock(passphrase, ALP_WALLET_DEFAULT_UNLOCK_TIME_SEC);
                    
                } catch (...) {
                    import_failure = std::current_exception();
                }
                
                if (import_failure) {
                    close();
                    fc::path wallet_file_path = fc::absolute(get_data_directory()) / wallet_name;
                    fc::remove_all(wallet_file_path);
                    std::rethrow_exception(import_failure);
                }
            }
            
            FC_CAPTURE_AND_RETHROW((filename)(wallet_name))
        }
        
        void Wallet::auto_backup(const string& reason)const {
            try {
                if (!get_automatic_backups()) return;
                
                ulog("Backing up wallet...");
                fc::path wallet_path = my->_current_wallet_path;
                std::string wallet_name = wallet_path.filename().string();
                fc::path wallet_dir = wallet_path.parent_path();
                fc::path backup_path;
                
                while (true) {
                    fc::time_point_sec now(time_point::now());
                    std::string backup_filename = wallet_name + "-" + now.to_non_delimited_iso_string();
                    
                    if (!reason.empty()) backup_filename += "-" + reason;
                    
                    backup_filename += ".json";
                    backup_path = wallet_dir / ".backups" / wallet_name / backup_filename;
                    
                    if (!fc::exists(backup_path)) break;
                    
                    fc::usleep(fc::seconds(1));
                }
                
                export_to_json(backup_path);
                ulog("Wallet automatically backed up to: ${f}", ("f", backup_path));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::set_version(uint32_t v) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                my->_wallet_db.set_property(version, variant(v));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        uint32_t Wallet::get_version()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                try {
                    return my->_wallet_db.get_property(version).as<uint32_t>();
                    
                } catch (...) {
                }
                
                return 0;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::set_automatic_backups(bool enabled) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                my->_wallet_db.set_property(automatic_backups, variant(enabled));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        bool Wallet::get_automatic_backups()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                try {
                    return my->_wallet_db.get_property(automatic_backups).as<bool>();
                    
                } catch (...) {
                }
                
                return true;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::set_transaction_scanning(bool enabled) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                my->_wallet_db.set_property(transaction_scanning, variant(enabled));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        bool Wallet::get_transaction_scanning()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (list_my_accounts().empty())
                    return false;
                    
                try {
                    return my->_wallet_db.get_property(transaction_scanning).as<bool>();
                    
                } catch (...) {
                }
                
                return true;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::unlock(const string& password, uint32_t timeout_seconds) {
            try {
                std::exception_ptr unlock_error;
                
                try {
                    FC_ASSERT(is_open(), "Need open");
                    
                    if (timeout_seconds < 1)
                        FC_THROW_EXCEPTION(invalid_timeout, "Invalid timeout!");
                        
                    const uint32_t max_timeout_seconds = std::numeric_limits<uint32_t>::max() - thinkyoung::blockchain::now().sec_since_epoch();
                    
                    if (timeout_seconds > max_timeout_seconds)
                        FC_THROW_EXCEPTION(invalid_timeout, "Timeout too large!", ("max_timeout_seconds", max_timeout_seconds));
                        
                    fc::time_point now = fc::time_point::now();
                    fc::time_point new_lock_time = now + fc::seconds(timeout_seconds);
                    
                    if (new_lock_time.sec_since_epoch() <= now.sec_since_epoch())
                        FC_THROW_EXCEPTION(invalid_timeout, "Invalid timeout!");
                        
                    if (password.size() < ALP_WALLET_MIN_PASSWORD_LENGTH)
                        FC_THROW_EXCEPTION(password_too_short, "Invalid password!");
                        
                    my->_wallet_password = fc::sha512::hash(password.c_str(), password.size());
                    
                    if (!my->_wallet_db.validate_password(my->_wallet_password))
                        FC_THROW_EXCEPTION(invalid_password, "Invalid password!");
                        
                    my->upgrade_version_unlocked();
                    my->_scheduled_lock_time = new_lock_time;
                    ilog("Wallet unlocked at time: ${t}", ("t", fc::time_point_sec(now)));
                    my->reschedule_relocker();
                    wallet_lock_state_changed(false);
                    ilog("Wallet unlocked until time: ${t}", ("t", fc::time_point_sec(*my->_scheduled_lock_time)));
                    my->scan_accounts();
                    
                } catch (...) {
                    unlock_error = std::current_exception();
                }
                
                if (unlock_error) {
                    lock();
                    std::rethrow_exception(unlock_error);
                }
            }
            
            FC_CAPTURE_AND_RETHROW((timeout_seconds))
        }
        
        void Wallet::lock() {
            cancel_scan();
            
            try {
                my->_login_map_cleaner_done.cancel_and_wait("wallet::lock()");
                
            } catch (const fc::exception& e) {
                wlog("Unexpected exception from wallet's login_map_cleaner() : ${e}", ("e", e));
                
            } catch (...) {
                wlog("Unexpected exception from wallet's login_map_cleaner()");
            }
            
            my->_stealth_private_keys.clear();
            my->_dirty_accounts = true;
            my->_dirty_contracts = true;
            my->_wallet_password = fc::sha512();
            my->_scheduled_lock_time = fc::optional<fc::time_point>();
            wallet_lock_state_changed(true);
            ilog("Wallet locked at time: ${t}", ("t", blockchain::now()));
            
            if (my->_blockchain->get_is_in_sandbox()) {
                my->_blockchain->set_is_in_sandbox(false);
                my->_blockchain->clear_sandbox_pending_state();
            }
        }
        
        bool Wallet::check_passphrase(const string& passphrase) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                if (passphrase.size() < ALP_WALLET_MIN_PASSWORD_LENGTH) FC_CAPTURE_AND_THROW(password_too_short);
                
                fc::sha512 password_input = fc::sha512::hash(passphrase.c_str(), passphrase.size());
                return my->_wallet_db.validate_password(password_input);
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        
        void Wallet::change_passphrase(const string & old_passphrase, const string& new_passphrase) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                if (new_passphrase.size() < ALP_WALLET_MIN_PASSWORD_LENGTH) FC_CAPTURE_AND_THROW(password_too_short);
                
                fc::sha512 password_input = fc::sha512::hash(old_passphrase.c_str(), old_passphrase.size());
                
                if (!my->_wallet_db.validate_password(password_input)) {
                    FC_THROW_EXCEPTION(invalid_password, "Invalid password!");
                }
                
                auto new_password = fc::sha512::hash(new_passphrase.c_str(), new_passphrase.size());
                my->_wallet_db.change_password(my->_wallet_password, new_password);
                my->_wallet_password = new_password;
                my->_dirty_accounts = true;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        bool Wallet::is_unlocked()const {
            FC_ASSERT(is_open(), "Wallet not open!");
            return !Wallet::is_locked();
        }
        
        bool Wallet::is_locked()const {
            FC_ASSERT(is_open(), "Wallet not open!");
            return my->_wallet_password == fc::sha512();
        }
        
        fc::optional<fc::time_point_sec> Wallet::unlocked_until()const {
            FC_ASSERT(is_open(), "Wallet not open!");
            return my->_scheduled_lock_time ? *my->_scheduled_lock_time : fc::optional<fc::time_point_sec>();
        }
        
        void Wallet::set_setting(const string& name, const variant& value) {
            my->_wallet_db.store_setting(name, value);
        }
        
        fc::optional<variant> Wallet::get_setting(const string& name)const {
            return my->_wallet_db.lookup_setting(name);
        }
        
        bool Wallet::delete_account(const string& account_name) {
            FC_ASSERT(is_open());
            FC_ASSERT(is_unlocked());
            const auto current_account = my->_wallet_db.lookup_account(account_name);
            
            if (!current_account.valid())
                FC_THROW_EXCEPTION(invalid_name, "No account with this name in wallet!");
                
            my->_wallet_db.remove_account(account_name);
            start_scan(0, -1);
            return true;
        }
        PublicKeyType Wallet::create_account(const string& account_name,
                                             const variant& private_data) {
            try {
                if (!my->_blockchain->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!!");
                const auto num_accounts_before = list_my_accounts().size();
                const auto current_account = my->_wallet_db.lookup_account(account_name);
                
                if (current_account.valid())
                    FC_THROW_EXCEPTION(invalid_name, "This name is already in your wallet!");
                    
                const auto existing_registered_account = my->_blockchain->get_account_entry(account_name);
                
                if (existing_registered_account.valid())
                    FC_THROW_EXCEPTION(invalid_name, "This name is already registered with the blockchain!");
                    
                const PublicKeyType account_public_key = my->_wallet_db.generate_new_account(my->_wallet_password, account_name, private_data);
                
                if (num_accounts_before == 0) {
                    set_last_scanned_block_number(my->_blockchain->get_head_block_num());
                    set_last_scanned_block_number_for_alp(0);
                }
                
                my->_dirty_accounts = true;
                return account_public_key;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        void Wallet::account_set_favorite(const string& account_name,
                                          const bool is_favorite) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                auto judged_account = my->_wallet_db.lookup_account(account_name);
                
                if (!judged_account.valid()) {
                    const auto blockchain_acct = my->_blockchain->get_account_entry(account_name);
                    
                    if (!blockchain_acct.valid())
                        FC_THROW_EXCEPTION(unknown_account, "Unknown account name!");
                        
                    add_contact_account(account_name, blockchain_acct->owner_key);
                    return account_set_favorite(account_name, is_favorite);
                }
                
                judged_account->is_favorite = is_favorite;
                my->_wallet_db.store_account(*judged_account);
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(is_favorite))
        }
        
        /**
         *  A contact is an account for which this wallet does not have the private
         *  key. If account_name is globally registered then this call will assume
         *  it is the same account and will fail if the key is not the same.
         *
         *  @param account_name - the name the account is known by to this wallet
         *  @param key - the public key that will be used for sending TITAN transactions
         *               to the account.
         */
        void Wallet::add_contact_account(const string& account_name,
                                         const PublicKeyType& key,
                                         const variant& private_data) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (!my->_blockchain->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                auto current_registered_account = my->_blockchain->get_account_entry(account_name);
                
                if (current_registered_account.valid() && current_registered_account->owner_key != key)
                    FC_THROW_EXCEPTION(invalid_name,
                                       "Account name is already registered under a different key! Provided: ${p}, registered: ${r}",
                                       ("p", key)("r", current_registered_account->active_key()));
                                       
                if (current_registered_account.valid() && current_registered_account->is_retracted())
                    FC_CAPTURE_AND_THROW(account_retracted, (current_registered_account));
                    
                auto current_account = my->_wallet_db.lookup_account(account_name);
                
                if (current_account.valid()) {
                    wlog("current account is valid... ${account}", ("account", *current_account));
                    FC_ASSERT(current_account->owner_address() == Address(key),
                              "Account with ${name} already exists", ("name", account_name));
                              
                    if (current_registered_account.valid()) {
                        blockchain::AccountEntry& as_blockchain_account_entry = *current_account;
                        as_blockchain_account_entry = *current_registered_account;
                    }
                    
                    if (!private_data.is_null())
                        current_account->private_data = private_data;
                        
                    my->_wallet_db.store_account(*current_account);
                    return;
                    
                } else {
                    current_account = my->_wallet_db.lookup_account(Address(key));
                    
                    if (current_account.valid()) {
                        if (current_account->name != account_name)
                            FC_THROW_EXCEPTION(duplicate_key,
                                               "Provided key already belongs to another wallet account! Provided: ${p}, existing: ${e}",
                                               ("p", account_name)("e", current_account->name));
                                               
                        if (current_registered_account.valid()) {
                            blockchain::AccountEntry& as_blockchain_account_entry = *current_account;
                            as_blockchain_account_entry = *current_registered_account;
                        }
                        
                        if (!private_data.is_null())
                            current_account->private_data = private_data;
                            
                        my->_wallet_db.store_account(*current_account);
                        return;
                    }
                    
                    if (!current_registered_account.valid()) {
                        const time_point_sec now = blockchain::now();
                        current_registered_account = AccountEntry();
                        current_registered_account->name = account_name;
                        current_registered_account->owner_key = key;
                        current_registered_account->set_active_key(now, key);
                        current_registered_account->last_update = now;
                    }
                    
                    my->_wallet_db.add_contact_account(*current_registered_account, private_data);
                }
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(key))
        }
        
        // TODO: This function is sometimes used purely for error checking of the account_name; refactor
        WalletAccountEntry Wallet::get_account(const string& account_name)const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (!my->_blockchain->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                auto local_account = my->_wallet_db.lookup_account(account_name);
                auto chain_account = my->_blockchain->get_account_entry(account_name);
                
                if (!local_account.valid() && !chain_account.valid())
                    FC_THROW_EXCEPTION(unknown_account, "Unknown account name!", ("account_name", account_name));
                    
                if (local_account.valid() && chain_account.valid()) {
                    if (local_account->owner_key != chain_account->owner_key) {
                        wlog("local account is owned by someone different public key than blockchain account");
                        wdump((local_account)(chain_account));
                    }
                }
                
                if (chain_account.valid()) {
                    my->_wallet_db.store_account(*chain_account);
                    local_account = my->_wallet_db.lookup_account(account_name);
                }
                
                FC_ASSERT(local_account.valid());
                return *local_account;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        void Wallet::accountsplit(const string & original, string & to_account, string & sub_account) {
            if (original.size() < 66) {
                to_account = original;
                return;
            }
            
            to_account = original.substr(0, original.size() - 32);
            sub_account = original.substr(original.size() - 32);
            
            if (INVALIDE_SUB_ADDRESS == sub_account) {
                sub_account = "";
                
            } else {
                sub_account = original;
            }
        }
        bool Wallet::is_valid_account_name(const string & accountname) {
            try {
                return my->_blockchain->is_valid_account_name(accountname);
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        void Wallet::remove_contact_account(const string& account_name) {
            try {
                if (!my->is_unique_account(account_name))
                    FC_THROW_EXCEPTION(duplicate_account_name,
                                       "Local account name conflicts with registered name. Please rename your local account first.", ("account_name", account_name));
                                       
                const auto oaccount = my->_wallet_db.lookup_account(account_name);
                
                if (my->_wallet_db.has_private_key(Address(oaccount->owner_key)))
                    FC_THROW_EXCEPTION(not_contact_account, "You can only remove contact accounts!", ("account_name", account_name));
                    
                my->_wallet_db.remove_contact_account(account_name);
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        void Wallet::rename_account(const string& old_account_name,
                                    const string& new_account_name) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (!my->_blockchain->is_valid_account_name(old_account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid old account name!", ("old_account_name", old_account_name));
                    
                if (!my->_blockchain->is_valid_account_name(new_account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid new account name!", ("new_account_name", new_account_name));
                    
                optional<PublicKeyType> old_key;
                auto registered_account = my->_blockchain->get_account_entry(old_account_name);
                bool have_registered = false;
                
                for (const auto& item : my->_wallet_db.get_accounts()) {
                    const WalletAccountEntry& local_account = item.second;
                    
                    if (local_account.name != old_account_name)
                        continue;
                        
                    if (!registered_account.valid() || registered_account->owner_key != local_account.owner_key) {
                        old_key = local_account.owner_key;
                        break;
                    }
                    
                    have_registered |= registered_account.valid() && registered_account->owner_key == local_account.owner_key;
                }
                
                if (!old_key.valid()) {
                    if (registered_account.valid())
                        FC_THROW_EXCEPTION(key_already_registered, "You cannot rename a registered account!");
                        
                    FC_THROW_EXCEPTION(unknown_account, "Unknown account name!", ("old_account_name", old_account_name));
                }
                
                registered_account = my->_blockchain->get_account_entry(*old_key);
                
                if (registered_account.valid() && registered_account->name != new_account_name) {
                    FC_THROW_EXCEPTION(key_already_registered, "That account is already registered to a different name!",
                                       ("desired_name", new_account_name)("registered_name", registered_account->name));
                }
                
                registered_account = my->_blockchain->get_account_entry(new_account_name);
                
                if (registered_account.valid())
                    FC_THROW_EXCEPTION(duplicate_account_name, "Your new account name is already registered!");
                    
                const auto new_account = my->_wallet_db.lookup_account(new_account_name);
                
                if (new_account.valid())
                    FC_THROW_EXCEPTION(duplicate_account_name, "You already have the new account name in your wallet!");
                    
                my->_wallet_db.rename_account(*old_key, new_account_name);
                my->_dirty_accounts = true;
            }
            
            FC_CAPTURE_AND_RETHROW((old_account_name)(new_account_name))
        }
        
        bool Wallet::friendly_import_private_key(const PrivateKeyType& key, const string& account_name) {
            try {
                const auto addr = Address(key.get_public_key());
                
                try {
                    get_private_key(addr);
                    // We already have this key and import_private_key would fail if we tried. Do nothing.
                    return false;
                    
                } catch (const fc::exception&) {
                }
                
                const oAccountEntry blockchain_account_entry = my->_blockchain->get_account_entry(addr);
                
                if (blockchain_account_entry.valid() && blockchain_account_entry->name != account_name) {
                    // This key exists on the blockchain and I don't have it - don't associate it with a name when you import it
                    import_private_key(key, optional<string>(), false);
                    
                } else {
                    import_private_key(key, account_name, false);
                }
                
                return true;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        PublicKeyType Wallet::import_private_key(const PrivateKeyType& new_private_key,
                const optional<string>& account_name,
                bool create_account) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                const PublicKeyType new_public_key = new_private_key.get_public_key();
                const Address new_address = Address(new_public_key);
                // Try to associate with an existing registered account
                const oAccountEntry blockchain_account_entry = my->_blockchain->get_account_entry(new_address);
                
                if (blockchain_account_entry.valid()) {
                    if (account_name.valid()) {
                        FC_ASSERT(*account_name == blockchain_account_entry->name,
                                  "That key already belongs to a registered account with a different name!",
                                  ("blockchain_account_entry", *blockchain_account_entry)
                                  ("account_name", *account_name));
                    }
                    
                    my->_wallet_db.store_account(*blockchain_account_entry);
                    my->_wallet_db.import_key(my->_wallet_password, blockchain_account_entry->name, new_private_key, true);
                    PrivateKeyType active_private_key;
                    PublicKeyType active_public_key;
                    Address active_address;
                    active_private_key = my->_wallet_db.get_account_child_key(new_private_key, 0);
                    active_public_key = active_private_key.get_public_key();
                    active_address = Address(active_public_key);
                    KeyData active_key;
                    active_key.account_address = Address(new_public_key);
                    active_key.public_key = active_public_key;
                    active_key.encrypt_private_key(my->_wallet_password, active_private_key);
                    my->_wallet_db.store_key(active_key);
                    oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(blockchain_account_entry->name);
                    FC_ASSERT(account_entry.valid(), "Account name not found!");
                    account_entry->set_active_key(blockchain::now(), active_public_key);
                    account_entry->last_update = blockchain::now();
                    my->_wallet_db.store_account(*account_entry);
                    return new_public_key;
                }
                
                // Try to associate with an existing local account
                oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(new_address);
                
                if (account_entry.valid()) {
                    if (account_name.valid()) {
                        FC_ASSERT(*account_name == account_entry->name,
                                  "That key already belongs to a local account with a different name!",
                                  ("account_entry", *account_entry)
                                  ("account_name", *account_name));
                    }
                    
                    my->_wallet_db.import_key(my->_wallet_password, account_entry->name, new_private_key, true);
                    return new_public_key;
                }
                
                FC_ASSERT(account_name.valid(), "Unknown key! You must specify an account name!");
                // Check if key is already associated with an existing local account
                const oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(new_address);
                
                if (key_entry.valid() && key_entry->has_private_key()) {
                    account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                    
                    if (account_entry.valid()) {
                        FC_ASSERT(*account_name == account_entry->name,
                                  "That key already belongs to a local account with a different name!",
                                  ("account_entry", *account_entry)
                                  ("account_name", *account_name));
                    }
                    
                    my->_wallet_db.import_key(my->_wallet_password, account_entry->name, new_private_key, true);
                    return new_public_key;
                }
                
                account_entry = my->_wallet_db.lookup_account(*account_name);
                
                if (!account_entry.valid()) {
                    FC_ASSERT(create_account,
                              "Could not find an account with that name!",
                              ("account_name", *account_name));
                    // TODO: Replace with wallet_db.add_account
                    add_contact_account(*account_name, new_public_key);
                    account_entry = my->_wallet_db.lookup_account(*account_name);
                    FC_ASSERT(account_entry.valid(), "Error creating new account!");
                }
                
                my->_wallet_db.import_key(my->_wallet_password, account_entry->name, new_private_key, true);
                return new_public_key;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(create_account))
        }
        
        PublicKeyType Wallet::import_wif_private_key(const string& wif_key,
                const optional<string>& account_name,
                bool create_account) {
            try {
                FC_ASSERT(is_open(), "Need open");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto key = thinkyoung::utilities::wif_to_key(wif_key);
                
                if (key.valid()) {
                    import_private_key(*key, account_name, create_account);
                    my->_dirty_accounts = true;
                    return key->get_public_key();
                }
                
                FC_ASSERT(false, "Error parsing WIF private key");
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        void Wallet::start_scan(const uint32_t start_block_num, const uint32_t limit) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                if (my->_scan_in_progress.valid() && !my->_scan_in_progress.ready())
                    return;
                    
                if (!get_transaction_scanning()) {
                    my->_scan_progress = 1;
                    ulog("Wallet transaction scanning is disabled!");
                    return;
                }
                
                const auto scan_chain_task = [=]() {
                    my->start_scan_task(start_block_num, limit);
                };
                my->_scan_in_progress = fc::async(scan_chain_task, "scan_chain_task");
                my->_scan_in_progress.on_complete([](fc::exception_ptr ep) {
                    if (ep) elog("Error during scanning: ${e}", ("e", ep->to_detail_string()));
                });
            }
            
            FC_CAPTURE_AND_RETHROW((start_block_num)(limit))
        }
        
        void Wallet::cancel_scan() {
            try {
                try {
                    ilog("Canceling wallet scan_chain_task...");
                    my->_scan_in_progress.cancel_and_wait("wallet::cancel_scan()");
                    ilog("Wallet scan_chain_task canceled...");
                    
                } catch (const fc::exception& e) {
                    wlog("Unexpected exception from wallet's scan_chain_task: ${e}", ("e", e.to_detail_string()));
                    
                } catch (...) {
                    wlog("Unexpected exception from wallet's scan_chain_task");
                }
                
                my->_scan_progress = 1;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        vector<string> Wallet::get_contracts(const string &account_name) {
            try {
                vector<string> result;
                // get all the contract in the accounts of the wallet
                auto id_account = my->_wallet_db.get_id_contract_map();
                
                if ("" == account_name) {
                    for (auto iter = id_account.begin(); iter != id_account.end(); ++iter) {
                        result.push_back(iter->first.AddressToString(AddressType::contract_address));
                    }
                    
                } else {
                    oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(account_name);
                    
                    if (!account_entry.valid())
                        FC_THROW_EXCEPTION(invalid_account_name, "the account is not in this wallet!");
                        
                    for (auto iter = id_account.begin(); iter != id_account.end(); ++iter) {
                        if (iter->second.owner == account_entry->owner_key)
                            result.push_back(iter->first.AddressToString(AddressType::contract_address));
                    }
                }
                
                return result;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        VoteSummary Wallet::get_vote_status(const string& account_name) {
            uint64_t total_possible = 0;
            uint64_t total = 0;
            auto summary = VoteSummary();
            summary.up_to_date_with_recommendation = true;
            auto my_slate = my->get_delegate_slate(vote_recommended);
            const AccountBalanceEntrySummaryType items = get_spendable_account_balance_entrys(account_name);
            
            for (const auto& item : items) {
                const auto& entrys = item.second;
                
                for (const auto& entry : entrys) {
                    if (entry.asset_id() != 0)
                        continue;
                        
                    if (entry.slate_id() != my_slate.id() && entry.balance > 1 * ALP_BLOCKCHAIN_PRECISION) // ignore dust
                        summary.up_to_date_with_recommendation = false;
                        
                    auto oslate = my->_blockchain->get_slate_entry(entry.slate_id());
                    
                    if (oslate.valid())
                        total += entry.get_spendable_balance(my->_blockchain->now()).amount * oslate->slate.size();
                        
                    total_possible += entry.get_spendable_balance(my->_blockchain->now()).amount * ALP_BLOCKCHAIN_MAX_SLATE_SIZE;
                }
            }
            
            if (total_possible == 0)
                summary.utilization = 0;
                
            else
                summary.utilization = float(total) / float(total_possible);
                
            summary.negative_utilization = 0;
            return summary;
        }
        
        PrivateKeyType Wallet::get_private_key(const Address& addr)const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto key = my->_wallet_db.lookup_key(addr);
                FC_ASSERT(key.valid());
                FC_ASSERT(key->has_private_key());
                return key->decrypt_private_key(my->_wallet_password);
            }
            
            FC_CAPTURE_AND_RETHROW((addr))
        }
        
        PublicKeyType  Wallet::get_public_key(const Address& addr) const {
            FC_ASSERT(is_open(), "Wallet not open!");
            auto key = my->_wallet_db.lookup_key(addr);
            FC_ASSERT(key.valid(), "No known key for this address.");
            return key->public_key;
        }
        bool Wallet::wallet_get_delegate_statue(const string & account) {
            FC_ASSERT(is_open());
            auto delegate_entry = get_account(account);
            FC_ASSERT(delegate_entry.is_delegate(), "${name} is not a delegate.", ("name", account));
            auto key = my->_wallet_db.lookup_key(delegate_entry.signing_address());
            FC_ASSERT(key.valid() && key->has_private_key(), "Unable to find private key for ${name}.", ("name", account));
            return delegate_entry.block_production_enabled;
        }
        void Wallet::set_delegate_block_production(const string& delegate_name, bool enabled) {
            FC_ASSERT(is_open(), "Wallet not open!");
            std::vector<WalletAccountEntry> delegate_entrys;
            const auto empty_before = get_my_delegates(enabled_delegate_status).empty();
            
            if (delegate_name != "ALL") {
                if (!my->_blockchain->is_valid_account_name(delegate_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid delegate name!", ("delegate_name", delegate_name));
                    
                auto delegate_entry = get_account(delegate_name);
                FC_ASSERT(delegate_entry.is_delegate(), "${name} is not a delegate.", ("name", delegate_name));
                auto key = my->_wallet_db.lookup_key(delegate_entry.signing_address());
                FC_ASSERT(key.valid() && key->has_private_key(), "Unable to find private key for ${name}.", ("name", delegate_name));
                delegate_entrys.push_back(delegate_entry);
                
            } else {
                delegate_entrys = get_my_delegates(any_delegate_status);
            }
            
            for (auto& delegate_entry : delegate_entrys) {
                delegate_entry.block_production_enabled = enabled;
                my->_wallet_db.store_account(delegate_entry);
            }
            
            const auto empty_after = get_my_delegates(enabled_delegate_status).empty();
            
            if (empty_before == empty_after) {
                return;
                
            } else if (empty_before) {
                ulog("Wallet transaction scanning has been automatically disabled due to enabled delegates!");
                set_transaction_scanning(false);
                
            } else { /* if( empty_after ) */
                ulog("Wallet transaction scanning has been automatically re-enabled!");
                set_transaction_scanning(true);
            }
        }
        
        vector<WalletAccountEntry> Wallet::get_my_delegates(uint32_t delegates_to_retrieve)const {
            FC_ASSERT(is_open(), "Wallet not open!");
            vector<WalletAccountEntry> delegate_entrys;
            const auto& account_entrys = list_my_accounts();
            
            for (const auto& account_entry : account_entrys) {
                if (!account_entry.is_delegate()) continue;
                
                if (delegates_to_retrieve & enabled_delegate_status && !account_entry.block_production_enabled) continue;
                
                if (delegates_to_retrieve & disabled_delegate_status && account_entry.block_production_enabled) continue;
                
                if (delegates_to_retrieve & active_delegate_status && !my->_blockchain->is_active_delegate(account_entry.id)) continue;
                
                if (delegates_to_retrieve & inactive_delegate_status && my->_blockchain->is_active_delegate(account_entry.id)) continue;
                
                delegate_entrys.push_back(account_entry);
            }
            
            return delegate_entrys;
        }
        
        optional<time_point_sec> Wallet::get_next_producible_block_timestamp(const vector<WalletAccountEntry>& delegate_entrys)const {
            try {
                if (!is_open() || is_locked()) return optional<time_point_sec>();
                
                vector<AccountIdType> delegate_ids;
                delegate_ids.reserve(delegate_entrys.size());
                
                for (const auto& delegate_entry : delegate_entrys)
                    delegate_ids.push_back(delegate_entry.id);
                    
                return my->_blockchain->get_next_producible_block_timestamp(delegate_ids);
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        fc::ecc::compact_signature Wallet::sign_hash(const string& signer, const fc::sha256& hash)const {
            try {
                auto key = PublicKeyType(signer);
                auto privkey = get_private_key(Address(key));
                return privkey.sign_compact(hash);
                
            } catch (...) {
                try {
                    auto addr = Address(signer);
                    auto privkey = get_private_key(addr);
                    return privkey.sign_compact(hash);
                    
                } catch (...) {
                    return get_active_private_key(signer).sign_compact(hash);
                }
            }
        }
        
        void Wallet::sign_block(SignedBlockHeader& header)const {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                const vector<AccountIdType>& active_delegate_ids = my->_blockchain->get_active_delegates();
                const AccountEntry delegate_entry = my->_blockchain->get_slot_signee(header.timestamp, active_delegate_ids);
                FC_ASSERT(delegate_entry.is_delegate());
                const PublicKeyType public_signing_key = delegate_entry.signing_key();
                const PrivateKeyType private_signing_key = get_private_key(Address(public_signing_key));
                FC_ASSERT(delegate_entry.delegate_info.valid());
                const uint32_t last_produced_block_num = delegate_entry.delegate_info->last_block_num_produced;
                const optional<SecretHashType>& prev_secret_hash = delegate_entry.delegate_info->next_secret_hash;
                
                if (prev_secret_hash.valid()) {
                    FC_ASSERT(!delegate_entry.delegate_info->signing_key_history.empty());
                    const map<uint32_t, PublicKeyType>& signing_key_history = delegate_entry.delegate_info->signing_key_history;
                    const uint32_t last_signing_key_change_block_num = signing_key_history.crbegin()->first;
                    
                    if (last_produced_block_num > last_signing_key_change_block_num) {
                        header.previous_secret = my->get_secret(last_produced_block_num, private_signing_key);
                        
                    } else {
                        // We need to use the old key to reveal the previous secret
                        FC_ASSERT(signing_key_history.size() >= 2);
                        auto iter = signing_key_history.crbegin();
                        ++iter;
                        const PublicKeyType& prev_public_signing_key = iter->second;
                        const PrivateKeyType prev_private_signing_key = get_private_key(Address(prev_public_signing_key));
                        header.previous_secret = my->get_secret(last_produced_block_num, prev_private_signing_key);
                    }
                    
                    FC_ASSERT(fc::ripemd160::hash(header.previous_secret) == *prev_secret_hash);
                }
                
                header.next_secret_hash = fc::ripemd160::hash(my->get_secret(header.block_num, private_signing_key));
                header.sign(private_signing_key);
                FC_ASSERT(header.validate_signee(public_signing_key));
            }
            
            FC_CAPTURE_AND_RETHROW((header))
        }
        
        std::shared_ptr<TransactionBuilder> Wallet::create_transaction_builder() {
            try {
                return std::make_shared<TransactionBuilder>(my.get());
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        std::shared_ptr<TransactionBuilder> Wallet::create_transaction_builder(const TransactionBuilder& old_builder) {
            try {
                auto builder = std::make_shared<TransactionBuilder>(old_builder, my.get());
                return builder;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        std::shared_ptr<TransactionBuilder> Wallet::create_transaction_builder_from_file(const string& old_builder_path) {
            try {
                auto path = old_builder_path;
                
                if (path == "") {
                    path = (get_data_directory() / "trx").string() + "/latest.trx";
                }
                
                auto old_builder = fc::json::from_file(path).as<TransactionBuilder>();
                auto builder = std::make_shared<TransactionBuilder>(old_builder, my.get());
                return builder;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        // TODO: Refactor publish_{slate|version} are exactly the same
        WalletTransactionEntry Wallet::publish_slate(
            const string& account_to_publish_under,
            const string& account_to_pay_with,
            bool sign) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                string paying_account = account_to_pay_with;
                
                if (paying_account.empty())
                    paying_account = account_to_publish_under;
                    
                if (!my->is_receive_account(paying_account))
                    FC_THROW_EXCEPTION(unknown_receive_account, "Unknown paying account!", ("paying_account", paying_account));
                    
                auto current_account = my->_blockchain->get_account_entry(account_to_publish_under);
                
                if (!current_account.valid())
                    FC_THROW_EXCEPTION(unknown_account, "Unknown publishing account!", ("account_to_publish_under", account_to_publish_under));
                    
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                const auto payer_public_key = get_owner_public_key(paying_account);
                const auto slate_id = my->set_delegate_slate(trx, vote_all);
                
                if (slate_id == 0)
                    FC_THROW_EXCEPTION(invalid_slate, "Cannot publish the null slate!");
                    
                fc::mutable_variant_object public_data;
                
                if (current_account->public_data.is_object())
                    public_data = current_account->public_data.get_object();
                    
                public_data["slate_id"] = slate_id;
                trx.update_account(current_account->id,
                                   current_account->delegate_pay_rate(),
                                   fc::variant_object(public_data),
                                   optional<PublicKeyType>());
                my->authorize_update(required_signatures, current_account);
                const auto required_fees = get_transaction_fee();
                
                if (current_account->is_delegate() && required_fees.amount < current_account->delegate_pay_balance()) {
                    // withdraw delegate pay...
                    trx.withdraw_pay(current_account->id, required_fees.amount);
                    required_signatures.insert(current_account->active_key());
                    
                } else {
                    my->withdraw_to_transaction(required_fees,
                                                paying_account,
                                                trx,
                                                required_signatures);
                }
                
                auto entry = LedgerEntry();
                entry.from_account = payer_public_key;
                entry.to_account = payer_public_key;
                entry.memo = "publish slate " + fc::variant(slate_id).as_string();
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_to_publish_under)(account_to_pay_with)(sign))
        }
        
        // TODO: Refactor publish_{slate|version} are exactly the same
        WalletTransactionEntry Wallet::publish_version(
            const string& account_to_publish_under,
            const string& account_to_pay_with,
            bool sign) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                string paying_account = account_to_pay_with;
                
                if (paying_account.empty())
                    paying_account = account_to_publish_under;
                    
                if (!my->is_receive_account(paying_account))
                    FC_THROW_EXCEPTION(unknown_receive_account, "Unknown paying account!", ("paying_account", paying_account));
                    
                auto current_account = my->_blockchain->get_account_entry(account_to_publish_under);
                
                if (!current_account.valid())
                    FC_THROW_EXCEPTION(unknown_account, "Unknown publishing account!", ("account_to_publish_under", account_to_publish_under));
                    
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                const auto payer_public_key = get_owner_public_key(paying_account);
                fc::mutable_variant_object public_data;
                
                if (current_account->public_data.is_object())
                    public_data = current_account->public_data.get_object();
                    
                const auto version = thinkyoung::client::version_info()["client_version"].as_string();
                public_data["version"] = version;
                trx.update_account(current_account->id,
                                   current_account->delegate_pay_rate(),
                                   fc::variant_object(public_data),
                                   optional<PublicKeyType>());
                my->authorize_update(required_signatures, current_account);
                const auto required_fees = get_transaction_fee();
                
                if (current_account->is_delegate() && required_fees.amount < current_account->delegate_pay_balance()) {
                    // withdraw delegate pay...
                    trx.withdraw_pay(current_account->id, required_fees.amount);
                    required_signatures.insert(current_account->active_key());
                    
                } else {
                    my->withdraw_to_transaction(required_fees,
                                                paying_account,
                                                trx,
                                                required_signatures);
                }
                
                auto entry = LedgerEntry();
                entry.from_account = payer_public_key;
                entry.to_account = payer_public_key;
                entry.memo = "publish version " + version;
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_to_publish_under)(account_to_pay_with)(sign))
        }
        
        WalletTransactionEntry Wallet::collect_account_balances(const string& account_name,
                const function<bool(const BalanceEntry&)> filter,
                const string& memo_message, bool sign) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                const oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(account_name);
                
                if (!account_entry.valid() || !account_entry->is_my_account)
                    FC_CAPTURE_AND_THROW(unknown_receive_account, (account_name));
                    
                AccountBalanceEntrySummaryType balance_entrys;
                const time_point_sec now = my->_blockchain->get_pending_state()->now();
                const auto scan_balance = [&](const BalanceIdType& id, const BalanceEntry& entry) {
                    if (!filter(entry)) return;
                    
                    const Asset balance = entry.get_spendable_balance(now);
                    
                    if (balance.amount == 0) return;
                    
                    const optional<Address> owner = entry.owner();
                    
                    if (!owner.valid()) return;
                    
                    const oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(*owner);
                    
                    if (!key_entry.valid() || !key_entry->has_private_key()) return;
                    
                    const oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                    
                    if (!account_entry.valid() || account_entry->name != account_name) return;
                    
                    balance_entrys[account_name].push_back(entry);
                };
                scan_balances(scan_balance);
                
                if (balance_entrys.find(account_name) == balance_entrys.end())
                    FC_CAPTURE_AND_THROW(insufficient_funds, (account_name));
                    
                SignedTransaction trx;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                unordered_set<Address> required_signatures;
                Asset total_balance;
                
                for (const BalanceEntry& entry : balance_entrys.at(account_name)) {
                    const Asset balance = entry.get_spendable_balance(my->_blockchain->get_pending_state()->now());
                    trx.withdraw(entry.id(), balance.amount);
                    const auto owner = entry.owner();
                    
                    if (!owner.valid()) continue;
                    
                    required_signatures.insert(*owner);
                    total_balance += balance;
                }
                
                trx.deposit_to_account(account_entry->active_key(),
                                       total_balance - get_transaction_fee(),
                                       get_private_key(account_entry->active_key()),
                                       memo_message,
                                       account_entry->active_key(),
                                       my->get_new_private_key(account_name),
                                       from_memo,
                                       account_entry->is_titan_account()
                                      );
                auto entry = LedgerEntry();
                entry.from_account = account_entry->owner_key;
                entry.to_account = account_entry->owner_key;
                entry.amount = total_balance - get_transaction_fee();
                entry.memo = memo_message;
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = get_transaction_fee();
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(sign))
        }
        
        TransactionBuilder Wallet::set_vote_info(
            const BalanceIdType& balance_id,
            const Address& voter_address,
            VoteStrategy strategy) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto builder = create_transaction_builder();
                const auto required_fees = get_transaction_fee();
                auto balance = my->_blockchain->get_balance_entry(balance_id);
                FC_ASSERT(balance.valid(), "No such balance!");
                SignedTransaction trx;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                trx.update_balance_vote(balance_id, voter_address);
                my->set_delegate_slate(trx, strategy);
                
                if (balance->restricted_owner == voter_address) { // not an owner update
                    builder->required_signatures.insert(voter_address);
                    
                } else {
                    const auto owner = balance->owner();
                    FC_ASSERT(owner.valid());
                    builder->required_signatures.insert(*owner);
                }
                
                auto entry = LedgerEntry();
                entry.memo = "Set balance vote info";
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                trans_entry.trx = trx;
                builder->transaction_entry = trans_entry;
                return *builder;
            }
            
            FC_CAPTURE_AND_RETHROW((balance_id)(voter_address)(strategy))
        }
        
        
        WalletTransactionEntry Wallet::update_signing_key(
            const string& authorizing_account_name,
            const string& delegate_name,
            const PublicKeyType& signing_key,
            bool sign
        ) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                TransactionBuilderPtr builder = create_transaction_builder();
                builder->update_signing_key(authorizing_account_name, delegate_name, signing_key);
                builder->finalize();
                
                if (sign) {
                    my->_dirty_accounts = true;
                    return builder->sign();
                }
                
                return builder->transaction_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((authorizing_account_name)(delegate_name)(signing_key)(sign))
        }
        
        void Wallet::repair_entrys(const optional<string>& collecting_account_name) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                ulog("Repairing wallet entrys. This may take a while...");
                my->_wallet_db.repair_entrys(my->_wallet_password);
                
                if (!collecting_account_name.valid())
                    return;
                    
                const oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(*collecting_account_name);
                FC_ASSERT(account_entry.valid(), "Cannot find a local account with that name!",
                          ("collecting_account_name", *collecting_account_name));
                map<string, vector<BalanceEntry>> items = get_spendable_account_balance_entrys();
                
                for (const auto& item : items) {
                    const auto& name = item.first;
                    const auto& entrys = item.second;
                    
                    if (name.find(ALP_ADDRESS_PREFIX) != 0)
                        continue;
                        
                    for (const auto& entry : entrys) {
                        const auto owner = entry.owner();
                        
                        if (!owner.valid()) continue;
                        
                        oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(*owner);
                        
                        if (key_entry.valid()) {
                            key_entry->account_address = account_entry->owner_address();
                            my->_wallet_db.store_key(*key_entry);
                        }
                    }
                }
                
                start_scan(0, -1);
            }
            
            FC_CAPTURE_AND_RETHROW((collecting_account_name))
        }
        
        uint32_t Wallet::regenerate_keys(const string& account_name, uint32_t num_keys_to_regenerate) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(num_keys_to_regenerate > 0);
                oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(account_name);
                FC_ASSERT(account_entry.valid());
                // Update local account entrys with latest global state
                my->scan_accounts();
                ulog("This may take a while...");
                uint32_t total_regenerated_key_count = 0;
                // Regenerate wallet child keys
                ulog("Regenerating wallet child keys and importing into account: ${name}", ("name", account_name));
                uint32_t key_index = 0;
                
                for (; key_index < num_keys_to_regenerate; ++key_index) {
                    fc::oexception regenerate_key_error;
                    
                    try {
                        const PrivateKeyType private_key = my->_wallet_db.get_wallet_child_key(my->_wallet_password, key_index);
                        import_private_key(private_key, account_name);
                        ++total_regenerated_key_count;
                        
                    } catch (const fc::exception& e) {
                        regenerate_key_error = e;
                    }
                    
                    if (regenerate_key_error.valid())
                        ulog("${e}", ("e", regenerate_key_error->to_detail_string()));
                }
                
                // Update wallet last used child key index
                my->_wallet_db.set_last_wallet_child_key_index(std::max(my->_wallet_db.get_last_wallet_child_key_index(), key_index - 1));
                // Regenerate v1 account child keys
                ulog("Regenerating type 1 account child keys for account: ${name}", ("name", account_name));
                uint32_t seq_num = 0;
                
                for (; seq_num < num_keys_to_regenerate; ++seq_num) {
                    fc::oexception regenerate_key_error;
                    
                    try {
                        const PrivateKeyType private_key = my->_wallet_db.get_account_child_key_v1(my->_wallet_password,
                                                           account_entry->owner_address(), seq_num);
                        import_private_key(private_key, account_name);
                        ++total_regenerated_key_count;
                        
                    } catch (const fc::exception& e) {
                        regenerate_key_error = e;
                    }
                    
                    if (regenerate_key_error.valid())
                        ulog("${e}", ("e", regenerate_key_error->to_detail_string()));
                }
                
                // Regenerate v2 account child keys
                const oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(Address(account_entry->active_key()));
                
                if (key_entry.valid() && key_entry->has_private_key()) {
                    ulog("Regenerating type 2 account child keys for account: ${name}", ("name", account_name));
                    const PrivateKeyType active_private_key = key_entry->decrypt_private_key(my->_wallet_password);
                    seq_num = 0;
                    
                    for (; seq_num < num_keys_to_regenerate; ++seq_num) {
                        fc::oexception regenerate_key_error;
                        
                        try {
                            const PrivateKeyType private_key = my->_wallet_db.get_account_child_key(active_private_key, seq_num);
                            import_private_key(private_key, account_name);
                            ++total_regenerated_key_count;
                            
                        } catch (const fc::exception& e) {
                            regenerate_key_error = e;
                        }
                        
                        if (regenerate_key_error.valid())
                            ulog("${e}", ("e", regenerate_key_error->to_detail_string()));
                    }
                }
                
                // Update account last used key sequence number
                account_entry->last_used_gen_sequence = std::max(account_entry->last_used_gen_sequence, seq_num - 1);
                my->_wallet_db.store_account(*account_entry);
                ulog("Successfully generated ${n} keys.", ("n", total_regenerated_key_count));
                my->_dirty_balances = true;
                my->_dirty_accounts = true;
                
                if (total_regenerated_key_count > 0)
                    start_scan(0, -1);
                    
                ulog("Key regeneration may leave the wallet in an inconsistent state.");
                ulog("It is recommended to create a new wallet and transfer all funds.");
                return total_regenerated_key_count;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        int32_t Wallet::recover_accounts(int32_t number_of_accounts, int32_t max_number_of_attempts) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            int attempts = 0;
            int recoveries = 0;
            uint32_t key_index = my->_wallet_db.get_last_wallet_child_key_index() + 1;
            
            while (recoveries < number_of_accounts && attempts++ < max_number_of_attempts) {
                const PrivateKeyType new_priv_key = my->_wallet_db.get_wallet_child_key(my->_wallet_password, key_index);
                fc::ecc::public_key new_pub_key = new_priv_key.get_public_key();
                auto recovered_account = my->_blockchain->get_account_entry(new_pub_key);
                
                if (recovered_account.valid()) {
                    my->_wallet_db.set_last_wallet_child_key_index(key_index);
                    import_private_key(new_priv_key, recovered_account->name, true);
                    ++recoveries;
                }
                
                ++key_index;
            }
            
            my->_dirty_accounts = true;
            
            if (recoveries)
                start_scan(0, -1);
                
            return recoveries;
        }
        
        // TODO: Rename to recover_titan_transaction_info
        WalletTransactionEntry Wallet::recover_transaction(const string& transaction_id_prefix, const string& recipient_account) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto transaction_entry = get_transaction(transaction_id_prefix);
                /* Only support standard transfers for now */
                FC_ASSERT(transaction_entry.ledger_entries.size() == 1);
                auto ledger_entry = transaction_entry.ledger_entries.front();
                
                /* In case the transaction was not saved in the entry */
                if (transaction_entry.trx.operations.empty()) {
                    const auto blockchain_transaction_entry = my->_blockchain->get_transaction(transaction_entry.entry_id, true);
                    FC_ASSERT(blockchain_transaction_entry.valid());
                    transaction_entry.trx = blockchain_transaction_entry->trx;
                }
                
                /* Only support a single deposit */
                DepositOperation deposit_op;
                bool has_deposit = false;
                
                for (const auto& op : transaction_entry.trx.operations) {
                    switch (OperationTypeEnum(op.type)) {
                        case deposit_op_type:
                            FC_ASSERT(!has_deposit);
                            deposit_op = op.as<DepositOperation>();
                            has_deposit = true;
                            break;
                            
                        default:
                            break;
                    }
                }
                
                FC_ASSERT(has_deposit);
                /* Only support standard withdraw by signature condition with memo */
                FC_ASSERT(WithdrawConditionTypes(deposit_op.condition.type) == withdraw_signature_type);
                const auto withdraw_condition = deposit_op.condition.as<WithdrawWithSignature>();
                FC_ASSERT(withdraw_condition.memo.valid());
                /* We had to have stored the one-time key */
                const auto key_entry = my->_wallet_db.lookup_key(withdraw_condition.memo->one_time_key);
                FC_ASSERT(key_entry.valid() && key_entry->has_private_key());
                const auto private_key = key_entry->decrypt_private_key(my->_wallet_password);
                /* Get shared secret and check memo decryption */
                bool found_recipient = false;
                PublicKeyType recipient_public_key;
                ExtendedMemoData memo;
                
                if (!recipient_account.empty()) {
                    recipient_public_key = get_owner_public_key(recipient_account);
                    const auto shared_secret = private_key.get_shared_secret(recipient_public_key);
                    memo = withdraw_condition.decrypt_memo_data(shared_secret);
                    found_recipient = true;
                    
                } else {
                    const auto check_account = [&](const AccountEntry& entry) {
                        try {
                            recipient_public_key = entry.owner_key;
                            // TODO: Need to check active keys as well as owner key
                            const auto shared_secret = private_key.get_shared_secret(recipient_public_key);
                            memo = withdraw_condition.decrypt_memo_data(shared_secret);
                            
                        } catch (...) {
                            return;
                        }
                        
                        found_recipient = true;
                        FC_ASSERT(false); /* Kill scanning since we found it */
                    };
                    
                    try {
                        my->_blockchain->scan_unordered_accounts(check_account);
                        
                    } catch (...) {
                    }
                }
                
                FC_ASSERT(found_recipient);
                /* Update ledger entry with recipient and memo info */
                ledger_entry.to_account = recipient_public_key;
                ledger_entry.memo = memo.get_message();
                transaction_entry.ledger_entries[0] = ledger_entry;
                my->_wallet_db.store_transaction(transaction_entry);
                return transaction_entry;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        optional<variant_object> Wallet::verify_titan_deposit(const string& transaction_id_prefix) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                
                // TODO: Separate this finding logic
                if (transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string(TransactionIdType()).size())
                    FC_THROW_EXCEPTION(invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix", transaction_id_prefix));
                    
                const TransactionIdType transaction_id = variant(transaction_id_prefix).as<TransactionIdType>();
                const oTransactionEntry transaction_entry = my->_blockchain->get_transaction(transaction_id, false);
                
                if (!transaction_entry.valid())
                    FC_THROW_EXCEPTION(transaction_not_found, "Transaction not found!", ("transaction_id_prefix", transaction_id_prefix));
                    
                /* Only support a single deposit */
                DepositOperation deposit_op;
                bool has_deposit = false;
                
                for (const auto& op : transaction_entry->trx.operations) {
                    switch (OperationTypeEnum(op.type)) {
                        case deposit_op_type:
                            FC_ASSERT(!has_deposit);
                            deposit_op = op.as<DepositOperation>();
                            has_deposit = true;
                            break;
                            
                        default:
                            break;
                    }
                }
                
                FC_ASSERT(has_deposit);
                /* Only support standard withdraw by signature condition with memo */
                FC_ASSERT(WithdrawConditionTypes(deposit_op.condition.type) == withdraw_signature_type);
                const WithdrawWithSignature withdraw_condition = deposit_op.condition.as<WithdrawWithSignature>();
                FC_ASSERT(withdraw_condition.memo.valid());
                oMessageStatus status;
                const map<PrivateKeyType, string> account_keys = my->_wallet_db.get_account_private_keys(my->_wallet_password);
                
                for (const auto& key_item : account_keys) {
                    const PrivateKeyType& key = key_item.first;
                    const string& account_name = key_item.second;
                    status = withdraw_condition.decrypt_memo_data(key);
                    
                    if (status.valid()) {
                        my->_wallet_db.cache_memo(*status, key, my->_wallet_password);
                        mutable_variant_object info;
                        info["from"] = variant();
                        info["to"] = account_name;
                        info["amount"] = Asset(deposit_op.amount, deposit_op.condition.asset_id);
                        info["memo"] = variant();
                        
                        if (status->has_valid_signature) {
                            const Address from_address(status->from);
                            const oAccountEntry chain_account_entry = my->_blockchain->get_account_entry(from_address);
                            const oWalletAccountEntry local_account_entry = my->_wallet_db.lookup_account(from_address);
                            
                            if (chain_account_entry.valid())
                                info["from"] = chain_account_entry->name;
                                
                            else if (local_account_entry.valid())
                                info["from"] = local_account_entry->name;
                        }
                        
                        const string memo = status->get_message();
                        
                        if (!memo.empty())
                            info["memo"] = memo;
                            
                        return variant_object(info);
                    }
                }
                
                return optional<variant_object>();
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        DelegatePaySalary Wallet::query_delegate_salary(const string& delegate_name) {
            FC_ASSERT(is_open());
            FC_ASSERT(is_unlocked());
            //FC_ASSERT(my->is_receive_account(delegate_name));
            auto asset_rec = my->_blockchain->get_asset_entry(AssetIdType(0));
            auto pending_state = my->_blockchain->get_pending_state();
            auto delegate_account_entry = pending_state->get_account_entry(delegate_name);
            FC_ASSERT(delegate_account_entry.valid());
            FC_ASSERT(delegate_account_entry->is_delegate());
            //auto required_fees = get_transaction_fee();
            DelegatePaySalary delepatbal;
            delepatbal.total_balance = delegate_account_entry->delegate_info->total_paid;
            delepatbal.pay_balance = delegate_account_entry->delegate_info->pay_balance;
            return delepatbal;
        }
        
        std::map<std::string, thinkyoung::blockchain::DelegatePaySalary> Wallet::query_delegate_salarys() {
            FC_ASSERT(is_open());
            FC_ASSERT(is_unlocked());
            std::map<std::string, thinkyoung::blockchain::DelegatePaySalary> Salary_Map;
            auto all_delegate_ids = my->_blockchain->get_delegates_by_vote(0, 99);
            auto asset_rec = my->_blockchain->get_asset_entry(AssetIdType(0));
            auto pending_state = my->_blockchain->get_pending_state();
            
            for (auto id : all_delegate_ids) {
                auto delegate_account_entry = pending_state->get_account_entry(id);
                FC_ASSERT(delegate_account_entry.valid());
                FC_ASSERT(delegate_account_entry->is_delegate());
                DelegatePaySalary delepatbal;
                delepatbal.total_balance = delegate_account_entry->delegate_info->total_paid;
                delepatbal.pay_balance = delegate_account_entry->delegate_info->pay_balance;
                Salary_Map[delegate_account_entry->name] = delepatbal;
            }
            
            return Salary_Map;
        }
        WalletTransactionEntry Wallet::withdraw_delegate_pay(
            const string& delegate_name,
            const string& real_amount_to_withdraw,
            const string& withdraw_to_account_name,
            bool sign) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(my->is_receive_account(delegate_name));
                auto asset_rec = my->_blockchain->get_asset_entry(AssetIdType(0));
                auto ipos = real_amount_to_withdraw.find(".");
                
                if (ipos != string::npos) {
                    string str = real_amount_to_withdraw.substr(ipos + 1);
                    int64_t precision_input = static_cast<int64_t>(pow(10, str.size()));
                    FC_ASSERT((static_cast<uint64_t>(precision_input) <= asset_rec->precision), "Precision is not correct");
                }
                
                double dAmountToWithdraw = std::stod(real_amount_to_withdraw);
                ShareType amount_to_withdraw((ShareType)(floor(dAmountToWithdraw * asset_rec->precision + 0.5)));
                auto delegate_account_entry = my->_blockchain->get_account_entry(delegate_name);
                FC_ASSERT(delegate_account_entry.valid());
                FC_ASSERT(delegate_account_entry->is_delegate());
                auto required_fees = get_transaction_fee();
                FC_ASSERT(delegate_account_entry->delegate_info->pay_balance >= (amount_to_withdraw + required_fees.amount), "",
                          ("delegate_account_entry", delegate_account_entry));
                SignedTransaction trx;
                unordered_set<Address> required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                oWalletKeyEntry delegate_key = my->_wallet_db.lookup_key(delegate_account_entry->active_key());
                FC_ASSERT(delegate_key && delegate_key->has_private_key());
                const auto delegate_private_key = delegate_key->decrypt_private_key(my->_wallet_password);
                const auto delegate_public_key = delegate_private_key.get_public_key();
                required_signatures.insert(delegate_public_key);
                const WalletAccountEntry receiver_account = get_account(withdraw_to_account_name);
                const string memo_message = "withdraw pay";
                trx.withdraw_pay(delegate_account_entry->id, amount_to_withdraw + required_fees.amount);
                trx.deposit(receiver_account.owner_address(), Asset(amount_to_withdraw, 0));
                //changed by CCW for deleting Titan Transfer
                //        trx.deposit_to_account( receiver_account.active_key(),
                //                                asset( amount_to_withdraw, 0 ),
                //                                delegate_private_key,
                //                                memo_message,
                //                                delegate_public_key,
                //                                my->get_new_private_key( delegate_name ),
                //                                from_memo,
                //                                receiver_account.is_titan_account()
                //                                );
                //my->set_delegate_slate(trx, strategy);
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                auto entry = LedgerEntry();
                entry.from_account = delegate_public_key;
                entry.amount = Asset(amount_to_withdraw, 0);
                entry.memo = memo_message;
                entry.to_account = receiver_account.owner_key;
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                trans_entry.extra_addresses.push_back(receiver_account.owner_address());
                trans_entry.trx = trx;
                //changed by CCW for deleting Titan Transfer
                //if (sign) my->sign_transaction(trx, required_signatures);
                //
                //        auto entry = ledger_entry();
                //        entry.from_account = delegate_public_key;
                //        entry.to_account = receiver_account.active_key(),
                //        entry.amount = asset( amount_to_withdraw );
                //        entry.memo = memo_message;
                //
                //        auto entry = wallet_transaction_entry();
                //        entry.ledger_entries.push_back( entry );
                //        entry.fee = required_fees;
                //
                //        if( sign )
                //            my->sign_transaction( trx, required_signatures );
                //
                //        entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((delegate_name)(real_amount_to_withdraw))
        }
        
        
        PublicKeyType Wallet::get_new_public_key(const string& account_name) {
            return my->get_new_public_key(account_name);
        }
        
        Address Wallet::create_new_address(const string& account_name, const string& label) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(my->is_receive_account(account_name));
            auto addr = my->get_new_address(account_name, label);
            return addr;
        }
        
        void Wallet::set_address_label(const Address& addr, const string& label) {
            FC_ASSERT(false, "This doesn't do anything right now.");
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            auto okey = my->_wallet_db.lookup_key(addr);
            FC_ASSERT(okey.valid(), "No such address.");
            //FC_ASSERT( okey->btc_data.valid(), "Trying to set a label for a TITAN address." );
            //okey->btc_data->label = label;
            my->_wallet_db.store_key(*okey);
        }
        
        string Wallet::get_address_label(const Address& addr) {
            FC_ASSERT(false, "This doesn't do anything right now.");
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            auto okey = my->_wallet_db.lookup_key(addr);
            //FC_ASSERT( okey.valid(), "No such address." );
            //FC_ASSERT( okey->btc_data.valid(), "This address has no label (it is a TITAN address)!" );
            //return okey->btc_data->label;
            return "";
        }
        
        void Wallet::set_address_group_label(const Address& addr, const string& group_label) {
            FC_ASSERT(false, "This doesn't do anything right now.");
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            auto okey = my->_wallet_db.lookup_key(addr);
            FC_ASSERT(okey.valid(), "No such address.");
            //FC_ASSERT( okey->btc_data.valid(), "Trying to set a group label for a TITAN address" );
            //okey->btc_data->group_label = group_label;
            my->_wallet_db.store_key(*okey);
        }
        
        string Wallet::get_address_group_label(const Address& addr) {
            FC_ASSERT(false, "This doesn't do anything right now.");
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            auto okey = my->_wallet_db.lookup_key(addr);
            FC_ASSERT(okey.valid(), "No such address.");
            //FC_ASSERT( okey->btc_data.valid(), "This address has no group label (it is a TITAN address)!" );
            return ""; //return okey->btc_data->group_label;
        }
        
        vector<Address> Wallet::get_addresses_for_group_label(const string& group_label) {
            FC_ASSERT(false, "This doesn't do anything right now.");
            vector<Address> addrs;
            
            for (auto item : my->_wallet_db.get_keys()) {
                auto key = item.second;
                //if( key.btc_data.valid() && key.btc_data->group_label == group_label )
                addrs.push_back(item.first);
            }
            
            return addrs;
        }
        
        ChainInterfacePtr Wallet::get_correct_state_ptr() const {
            if (my->_blockchain->get_is_in_sandbox())
                return my->_blockchain->get_sandbox_pending_state();
                
            return my->_blockchain;
        }
        
        WalletTransactionEntry Wallet::register_contract(const string& owner, const fc::path codefile, const string& asset_symbol, double init_limit, bool is_testing) {
            ChainInterfacePtr data_ptr = get_correct_state_ptr();
            string codefile_str = codefile.string();
            size_t pos;
            pos = codefile_str.find_last_of('.');
            
            if ((pos == string::npos) || codefile_str.substr(pos) != ".gpc") {
                FC_THROW_EXCEPTION(thinkyoung::blockchain::invalid_contract_filename, "contract bytecode file name should end with .gpc");
            }
            
            FC_ASSERT(init_limit > 0, "init_limit should greater than 0");
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(data_ptr->is_valid_symbol(asset_symbol), "Invalid asset symbol");
            FC_ASSERT(my->is_receive_account(owner), "Invalid account name");
            FC_ASSERT(asset_symbol == ALP_BLOCKCHAIN_SYMBOL, "asset_symbol must be ACT");
            const auto asset_rec = data_ptr->get_asset_entry(asset_symbol);
            FC_ASSERT(asset_rec.valid(), "Asset not exist!");
            FC_ASSERT(asset_rec->id == 0, "asset_symbol must be ACT");
            const auto asset_id = asset_rec->id;
            const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
            //ShareType amount_limit = init_limit * precision;
            //Asset asset_limit(amount_limit, asset_id);
            Asset asset_limit = to_asset(asset_rec->id, precision, init_limit);
            SignedTransaction     trx;
            unordered_set<Address> required_signatures;
            Code contract_code(codefile);
            PublicKeyType  owner_public_key = get_owner_public_key(owner);
            Address        owner_address = Address(owner_public_key);
            Asset register_fee = data_ptr->get_contract_register_fee(contract_code);
            Asset margin = data_ptr->get_default_margin(asset_id);
            Asset fee = get_transaction_fee(asset_limit.asset_id);
            map<BalanceIdType, ShareType> balances;
            
            if (!is_testing) {
                if (my->_blockchain->get_is_in_sandbox())
                    sandbox_get_enough_balances(owner, asset_limit + register_fee + fee + margin, balances, required_signatures);
                    
                else
                    get_enough_balances(owner, asset_limit + register_fee + fee + margin, balances, required_signatures);
                    
            } else
                required_signatures.insert(owner_address);
                
            ContractIdType contract_id = trx.register_contract(contract_code, owner_public_key, asset_limit, fee, balances); //插入合约注册op
            FC_ASSERT(register_fee.asset_id == 0, "register fee must be ACT");
            FC_ASSERT(margin.asset_id == 0, "register fee must be ACT");
            FC_ASSERT(fee.asset_id == 0, "register fee must be ACT");
            trx.expiration = blockchain::now() + get_transaction_expiration();
            my->sign_transaction(trx, required_signatures);
            //for scanning contract because upgrading contract
            my->_dirty_contracts = true;
            auto trans_entry = WalletTransactionEntry();
            trans_entry.fee = register_fee + fee;
            trans_entry.trx = trx;
            return trans_entry;
        }
        
        std::vector<thinkyoung::blockchain::Asset> Wallet::register_contract_testing(const string& owner, const fc::path codefile) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(my->is_receive_account(owner), "Invalid account name");
            Code contract_code(codefile);
            
            if (!contract_code.valid()) {
                FC_CAPTURE_AND_THROW(contract_code_invalid, (codefile));
            }
            
            Asset fee = get_transaction_fee(0);
            Asset margin = my->_blockchain->get_default_margin(0);
            Asset register_fee = my->_blockchain->get_contract_register_fee(contract_code);
            Asset asset_for_exec = my->_blockchain->get_amount(CONTRACT_TESTING_LIMIT_MAX);
            auto trans_entry = register_contract(owner,
                                                 codefile,
                                                 ALP_BLOCKCHAIN_SYMBOL,
                                                 (double)asset_for_exec.amount / ALP_BLOCKCHAIN_PRECISION,
                                                 true
                                                );
            SignedTransaction     trx;
            trx = trans_entry.trx;
            ChainInterfacePtr state_ptr = get_correct_state_ptr();
            PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(state_ptr);
            TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
            trx_eval_state->skipexec = false;
            trx_eval_state->evaluate_contract_testing = true;
            trx_eval_state->evaluate(trx);
            std::vector<thinkyoung::blockchain::Asset> asset_vec;
            asset_vec.emplace_back(fee);
            asset_vec.emplace_back(margin);
            asset_vec.emplace_back(register_fee);
            asset_vec.emplace_back(trx_eval_state->exec_cost);
            return asset_vec;
        }
        
        thinkyoung::wallet::WalletTransactionEntry Wallet::call_contract(const string caller, const ContractIdType contract, const string method, const string& arguments, const string& asset_symbol, double cost_limit, bool is_testing) {
            ChainInterfacePtr data_ptr = get_correct_state_ptr();
            
            if (arguments.length() > CONTRACT_PARAM_MAX_LEN)
                FC_CAPTURE_AND_THROW(contract_parameter_length_over_limit, ("the parameter length of contract function is over limit"));
                
            if (CallContractOperation::is_function_not_allow_call(method)) {
                FC_CAPTURE_AND_THROW(method_can_not_be_called_explicitly, (method)("method can't be called explicitly !"));
            }
            
            FC_ASSERT(cost_limit > 0, "cost_limit should greater than 0");
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(data_ptr->is_valid_symbol(asset_symbol), "Invalid asset symbol");
            FC_ASSERT(my->is_receive_account(caller), "Invalid account name");
            FC_ASSERT(asset_symbol == ALP_BLOCKCHAIN_SYMBOL, "asset_symbol must be ACT");
            const auto asset_rec = data_ptr->get_asset_entry(asset_symbol);
            FC_ASSERT(asset_rec.valid(), "Asset not exist!");
            FC_ASSERT(asset_rec->id == 0, "asset_symbol must be ACT");
            const auto asset_id = asset_rec->id;
            const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
            //ShareType amount_limit = cost_limit * precision;
            //Asset asset_limit(amount_limit, asset_id);
            Asset asset_limit = to_asset(asset_rec->id, precision, cost_limit);
            SignedTransaction     trx;
            unordered_set<Address> required_signatures;
            PublicKeyType  caller_public_key = get_owner_public_key(caller);
            Address        caller_address = Address(caller_public_key);
            Asset fee = get_transaction_fee(asset_limit.asset_id);
            map<BalanceIdType, ShareType> balances;
            
            if (!is_testing) {
                if (my->_blockchain->get_is_in_sandbox())
                    sandbox_get_enough_balances(caller, asset_limit + fee, balances, required_signatures);
                    
                else
                    get_enough_balances(caller, asset_limit + fee, balances, required_signatures);
                    
            } else
                required_signatures.insert(caller_address);
                
            trx.call_contract(contract, method, arguments, caller_public_key, asset_limit, fee, balances); //插入合约调用op
            FC_ASSERT(fee.asset_id == 0, "register fee must be ACT");
            trx.expiration = blockchain::now() + get_transaction_expiration();
            my->sign_transaction(trx, required_signatures);
            auto trans_entry = WalletTransactionEntry();
            trans_entry.fee = fee;
            trans_entry.trx = trx;
            trans_entry.entry_id = trx.id();
            return trans_entry;
        }
        
        std::vector<thinkyoung::blockchain::Asset> Wallet::call_contract_testing(const string caller, const ContractIdType contract, const string method, const string& arguments) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(my->is_receive_account(caller), "Invalid account name");
            Asset fee = get_transaction_fee(0);
            Asset asset_for_exec = my->_blockchain->get_amount(CONTRACT_TESTING_LIMIT_MAX);
            auto trans_entry = call_contract(caller,
                                             contract,
                                             method,
                                             arguments,
                                             ALP_BLOCKCHAIN_SYMBOL,
                                             (double)asset_for_exec.amount / ALP_BLOCKCHAIN_PRECISION,
                                             true);
            SignedTransaction     trx;
            trx = trans_entry.trx;
            ChainInterfacePtr state_ptr = get_correct_state_ptr();
            PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(state_ptr);
            TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
            trx_eval_state->skipexec = false;
            trx_eval_state->evaluate_contract_testing = true;
            trx_eval_state->evaluate(trx);
            std::vector<thinkyoung::blockchain::Asset> asset_vec;
            asset_vec.emplace_back(fee);
            asset_vec.emplace_back(trx_eval_state->exec_cost);
            return asset_vec;
        }
        std::vector<thinkyoung::blockchain::EventOperation> Wallet::call_contract_local_emit(const string caller, const ContractIdType contract, const string method, const string& arguments) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(my->is_receive_account(caller), "Invalid account name");
            Asset fee = get_transaction_fee(0);
            Asset asset_for_exec = my->_blockchain->get_amount(CONTRACT_TESTING_LIMIT_MAX);
            auto trans_entry = call_contract(caller,
                                             contract,
                                             method,
                                             arguments,
                                             ALP_BLOCKCHAIN_SYMBOL,
                                             (double)asset_for_exec.amount / ALP_BLOCKCHAIN_PRECISION,
                                             true);
            SignedTransaction trx;
            trx = trans_entry.trx;
            ChainInterfacePtr state_ptr = get_correct_state_ptr();
            PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(state_ptr);
            TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
            trx_eval_state->skipexec = false;
            trx_eval_state->evaluate_contract_testing = true;
            trx_eval_state->evaluate(trx);
            vector<EventOperation> ops;
            
            for (const auto& op : trx_eval_state->p_result_trx.operations) {
                if (op.type == OperationTypeEnum::event_op_type) {
                    ops.push_back(op.as<EventOperation>());
                }
            }
            
            return ops;
        }
        
        std::string Wallet::call_contract_offline(const string caller, const ContractIdType contract, const string method, const string& arguments) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            FC_ASSERT(my->is_receive_account(caller), "Invalid account name");
            oContractEntry entry = my->_blockchain->get_contract_entry(contract);
            
            if (!entry.valid()) {
                FC_CAPTURE_AND_THROW(contract_not_exist, (contract));
            }
            
            set<string>::iterator vsit = entry->code.offline_abi.begin();
            
            while (vsit != entry->code.offline_abi.end()) {
                if (method == *vsit)
                    break;
                    
                vsit++;
            }
            
            if (vsit == entry->code.offline_abi.end())
                FC_CAPTURE_AND_THROW(method_not_exist, (method));
                
            PublicKeyType  caller_public_key = get_owner_public_key(caller);
            Address        caller_address = Address(caller_public_key);
            PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(my->_blockchain);
            TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
            GluaStateValue statevalue;
            statevalue.pointer_value = trx_eval_state.get();
            lua::lib::GluaStateScope scope;
            lua::lib::add_global_string_variable(scope.L(), "caller", (((string)(caller_public_key)).c_str()));
            lua::lib::add_global_string_variable(scope.L(), "caller_address", ((string)(Address(caller_address))).c_str());
            lua::lib::set_lua_state_value(scope.L(), "evaluate_state", statevalue, GluaStateValueType::LUA_STATE_VALUE_POINTER);
            lua::api::global_glua_chain_api->clear_exceptions(scope.L());
            std::string result;
            scope.set_instructions_limit(CONTRACT_OFFLINE_LIMIT_MAX);
            scope.execute_contract_api_by_address(contract.AddressToString(AddressType::contract_address).c_str(), method.c_str(), arguments.c_str(), &result);
            int exception_code = lua::lib::get_lua_state_value(scope.L(), "exception_code").int_value;
            char* exception_msg = (char*)lua::lib::get_lua_state_value(scope.L(), "exception_msg").string_value;
            
            if (exception_code > 0) {
                thinkyoung::blockchain::contract_error con_err(32000, "exception", exception_msg);
                throw con_err;
            }
            
            return result;
        }
        
        void Wallet::get_enough_balances(const string& account_name, const Asset target, std::map<BalanceIdType, ShareType>& balances, unordered_set<Address>& required_signatures) {
            try {
                FC_ASSERT(!account_name.empty());
                auto amount_remaining = target;
                const AccountBalanceEntrySummaryType balance_entrys = get_spendable_account_balance_entrys(account_name);
                
                if (balance_entrys.find(account_name) == balance_entrys.end())
                    FC_CAPTURE_AND_THROW(insufficient_funds, (account_name)(target)(balance_entrys));
                    
                for (const auto& entry : balance_entrys.at(account_name)) {
                    const Asset balance = entry.get_spendable_balance(my->_blockchain->get_pending_state()->now());
                    
                    if (balance.amount <= 0 || balance.asset_id != amount_remaining.asset_id)
                        continue;
                        
                    const auto owner = entry.owner();
                    
                    if (!owner.valid())
                        continue;
                        
                    if (amount_remaining.amount > balance.amount) {
                        balances.insert(std::make_pair(entry.id(), balance.amount));
                        required_signatures.insert(*owner);
                        amount_remaining -= balance;
                        
                    } else {
                        balances.insert(std::make_pair(entry.id(), amount_remaining.amount));
                        required_signatures.insert(*owner);
                        return;
                    }
                }
                
                const string required = my->_blockchain->to_pretty_asset(target);
                const string available = my->_blockchain->to_pretty_asset(target - amount_remaining);
                FC_CAPTURE_AND_THROW(insufficient_funds, (required)(available)(balance_entrys));
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(target)(balances)(required_signatures))
        }
        
        AccountBalanceEntrySummaryType Wallet::sandbox_get_spendable_account_balance_entries(const string& account_name) {
            try {
                // set the balances state for sandbox
                PendingChainStatePtr sandbox_state = my->_blockchain->get_sandbox_pending_state();
                
                if (!account_name.empty() && !sandbox_state->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                map<string, vector<BalanceEntry>>  account_balances;
                const time_point_sec now = sandbox_state->now();
                
                for (auto iter = sandbox_state->_balance_id_to_entry.begin(); iter != sandbox_state->_balance_id_to_entry.end(); ++iter) {
                    auto entry = iter->second;
                    
                    if (entry.condition.type != withdraw_signature_type) continue;
                    
                    const Asset balance = entry.get_spendable_balance(now);
                    
                    if (balance.amount == 0) continue;
                    
                    const optional<Address> owner = entry.owner();
                    
                    if (!owner.valid()) continue;
                    
                    const oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(*owner);
                    
                    if (!key_entry.valid() || !key_entry->has_private_key()) continue;
                    
                    const oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                    const string name = account_entry.valid() ? account_entry->name : string(key_entry->public_key);
                    
                    if (!account_name.empty() && name != account_name) continue;
                    
                    account_balances[account_name].push_back(entry);
                }
                
                return account_balances;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::sandbox_get_enough_balances(const string& account_name, const Asset target, std::map<BalanceIdType, ShareType>& balances, unordered_set<Address>& required_signatures) {
            try {
                // set the balances state for sandbox
                PendingChainStatePtr sandbox_state = my->_blockchain->get_sandbox_pending_state();
                const AccountBalanceEntrySummaryType balance_entrys = sandbox_get_spendable_account_balance_entries(account_name);
                
                if (balance_entrys.find(account_name) == balance_entrys.end())
                    FC_CAPTURE_AND_THROW(insufficient_funds, (account_name)(target)(balance_entrys));
                    
                auto amount_remaining = target;
                
                for (const auto& entry : balance_entrys.at(account_name)) {
                    const Asset balance = entry.get_spendable_balance(sandbox_state->now());
                    
                    if (balance.amount <= 0 || balance.asset_id != amount_remaining.asset_id)
                        continue;
                        
                    const auto owner = entry.owner();
                    
                    if (!owner.valid())
                        continue;
                        
                    if (amount_remaining.amount > balance.amount) {
                        balances.insert(std::make_pair(entry.id(), balance.amount));
                        required_signatures.insert(*owner);
                        amount_remaining -= balance;
                        
                    } else {
                        balances.insert(std::make_pair(entry.id(), amount_remaining.amount));
                        required_signatures.insert(*owner);
                        return;
                    }
                }
                
                const string required = my->_blockchain->to_pretty_asset(target);
                const string available = my->_blockchain->to_pretty_asset(target - amount_remaining);
                FC_CAPTURE_AND_THROW(insufficient_funds, (required)(available)(balance_entrys));
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(target)(balances)(required_signatures))
        }
        
        vector<ScriptEntry> Wallet::list_scripts() {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                vector<ScriptEntry> res;
                auto it = my->script_id_to_script_entry_db.unordered_begin();
                auto end = my->script_id_to_script_entry_db.unordered_end();
                
                while (it != end) {
                    res.push_back(it->second);
                    it++;
                }
                
                std::sort(res.begin(), res.end());
                return res;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        thinkyoung::wallet::oScriptEntry Wallet::get_script_entry(const ScriptIdType& script_id) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto it = my->script_id_to_script_entry_db.unordered_find(script_id);
                
                if (it != my->script_id_to_script_entry_db.unordered_end()) {
                    return  it->second;
                }
                
                return oScriptEntry();
            }
            
            FC_CAPTURE_AND_RETHROW((script_id))
        }
        
        thinkyoung::blockchain::ScriptIdType Wallet::add_script(const fc::path& filename, const string& description/*=string("")*/) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                ScriptEntry script_entry(filename, description);
                my->script_id_to_script_entry_db.store(script_entry.id, script_entry);
                return  script_entry.id;
            }
            
            FC_CAPTURE_AND_RETHROW((filename)(description))
        }
        
        void Wallet::delete_script(const ScriptIdType& script_id) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                
                if (my->script_id_to_script_entry_db.unordered_find(script_id) == my->script_id_to_script_entry_db.unordered_end())
                    FC_CAPTURE_AND_THROW(script_not_found_in_db, (script_id));
                    
                my->script_id_to_script_entry_db.remove(script_id);
                
                for (auto it = my->contract_id_event_to_script_id_vector_db.unordered_begin(); it != my->contract_id_event_to_script_id_vector_db.unordered_end(); it++) {
                    ScriptRelationKey key = it->first;
                    vector<ScriptIdType> vec = it->second;
                    bool modified = false;
                    
                    for (auto vecit = vec.begin(); vecit!=vec.end();) {
                        if (*vecit == script_id) {
                            vecit = vec.erase(vecit);
                            modified = true;
                            
                        } else
                            vecit++;
                    }
                    
                    if (modified)
                        my->contract_id_event_to_script_id_vector_db.store(key, vec);
                }
            }
            
            FC_CAPTURE_AND_RETHROW((script_id))
        }
        void Wallet::disable_script(const ScriptIdType& script_id) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto it = my->script_id_to_script_entry_db.unordered_find(script_id);
                
                if (it != my->script_id_to_script_entry_db.unordered_end()) {
                    ScriptEntry entry = it->second;
                    
                    if (!entry.enable)
                        FC_CAPTURE_AND_THROW(script_has_been_disabled, (script_id));
                        
                    entry.enable = false;
                    my->script_id_to_script_entry_db.store(script_id, entry);
                    
                } else {
                    FC_THROW_EXCEPTION(script_not_found_in_db, "No such script exsited");
                }
            }
            
            FC_CAPTURE_AND_RETHROW((script_id))
        }
        
        void Wallet::enable_script(const ScriptIdType& script_id) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto it = my->script_id_to_script_entry_db.unordered_find(script_id);
                
                if (it != my->script_id_to_script_entry_db.unordered_end()) {
                    ScriptEntry entry = it->second;
                    entry.enable = true;
                    my->script_id_to_script_entry_db.store(script_id, entry);
                    
                } else {
                    FC_THROW_EXCEPTION(script_not_found_in_db, "No such script exsited");
                }
            }
            
            FC_CAPTURE_AND_RETHROW((script_id))
        }
        
        vector<ScriptIdType> Wallet::list_event_handler(const ContractIdType& contract_id, const std::string& event_type) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto contract_entry = my->_blockchain->get_contract_entry(contract_id);
                
                if (NOT contract_entry.valid())
                    FC_THROW_EXCEPTION(contract_not_exist, "contract id is not existed");
                    
                auto it = my->contract_id_event_to_script_id_vector_db.unordered_find(ScriptRelationKey(contract_id, event_type));
                
                if (it != my->contract_id_event_to_script_id_vector_db.unordered_end()) {
                    return  it->second;
                }
                
                return vector<ScriptIdType>();
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(event_type))
        }
        
        void Wallet::add_event_handler(const ContractIdType& contract_id, const std::string& event_type, const ScriptIdType& script_id, uint32_t index) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto contract_entry = my->_blockchain->get_contract_entry(contract_id);
                
                if (NOT contract_entry.valid())
                    FC_THROW_EXCEPTION(contract_not_exist, "contract id is not existed");
                    
                if (std::find(contract_entry->code.events.begin(), contract_entry->code.events.end(), event_type) == contract_entry->code.events.end())
                    FC_CAPTURE_AND_THROW(EventType_not_found, (event_type));
                    
                auto script_entry = get_script_entry(script_id);
                
                if (NOT script_entry.valid())
                    FC_THROW_EXCEPTION(script_not_found_in_db, "script id is not existed");
                    
                vector<ScriptIdType> script_id_vec;
                auto it = my->contract_id_event_to_script_id_vector_db.unordered_find(ScriptRelationKey(contract_id, event_type));
                
                if (it != my->contract_id_event_to_script_id_vector_db.unordered_end()) {
                    script_id_vec = it->second;
                    
                    for (const auto& id : script_id_vec) {
                        if (id == script_id)
                            FC_THROW_EXCEPTION(event_handler_existed, "Event handler existed");
                    }
                    
                    if (index > script_id_vec.size()) {
                        script_id_vec.push_back(script_id);
                        
                    } else {
                        script_id_vec.insert(script_id_vec.begin() + index, script_id);
                    }
                    
                } else {
                    script_id_vec.push_back(script_id);
                }
                
                my->contract_id_event_to_script_id_vector_db.store(ScriptRelationKey(contract_id, event_type), script_id_vec);
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(event_type)(script_id)(index))
        }
        
        void Wallet::delete_event_handler(const ContractIdType& contract_id, const std::string& event_type, const ScriptIdType& script_id) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto contract_entry = my->_blockchain->get_contract_entry(contract_id);
                
                if (NOT contract_entry.valid())
                    FC_THROW_EXCEPTION(contract_not_exist, "contract id is not existed");
                    
                auto script_entry = get_script_entry(script_id);
                
                if (NOT script_entry.valid())
                    FC_THROW_EXCEPTION(script_not_found_in_db, "script id is not existed");
                    
                bool get_delete = false;
                vector<ScriptIdType> new_script_id_vec;
                auto it = my->contract_id_event_to_script_id_vector_db.unordered_find(ScriptRelationKey(contract_id, event_type));
                
                if (it != my->contract_id_event_to_script_id_vector_db.unordered_end()) {
                    vector<ScriptIdType> script_id_vec(it->second);
                    
                    for (auto iter = script_id_vec.begin(); iter != script_id_vec.end(); ++iter) {
                        if (*iter != script_id) {
                            new_script_id_vec.push_back(*iter);
                            
                        } else {
                            get_delete = true;
                        }
                    }
                }
                
                if (get_delete == true) {
                    my->contract_id_event_to_script_id_vector_db.store(ScriptRelationKey(contract_id, event_type), new_script_id_vec);
                    
                } else {
                    FC_CAPTURE_AND_THROW(EventHandler_not_found, (contract_id)(event_type)(script_id));
                }
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(event_type)(script_id))
        }
        std::vector<std::string> Wallet::get_events_bound(const std::string& script_id) {
            FC_ASSERT(is_open(), "Wallet not open!");
            FC_ASSERT(is_unlocked(), "Wallet not unlock!");
            ScriptIdType id(script_id, AddressType::script_id);
            auto it = my->contract_id_event_to_script_id_vector_db.unordered_begin();
            std::vector<std::string> res;
            
            while(it!= my->contract_id_event_to_script_id_vector_db.unordered_end()) {
                for (auto script_id_it : it->second) {
                    if (script_id_it == id) {
                        res.push_back(it->first.contract_id.AddressToString(AddressType::contract_address) + "," + it->first.event_type);
                    }
                }
                
                it++;
            }
            
            return  res;
        }
        WalletTransactionEntry Wallet::transfer_asset_to_address(
            const string& real_amount_to_transfer,
            const string& amount_to_transfer_symbol,
            const string& from_account_name,
            const Address& to_address,
            const string& memo_message,
            VoteStrategy strategy,
            bool sign,
            const string& alp_account) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(my->_blockchain->is_valid_symbol(amount_to_transfer_symbol), "Invalid asset symbol");
                FC_ASSERT(my->is_receive_account(from_account_name), "Invalid account name");
                const auto asset_rec = my->_blockchain->get_asset_entry(amount_to_transfer_symbol);
                FC_ASSERT(asset_rec.valid(), "Asset not exist!");
                const auto asset_id = asset_rec->id;
                const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
                FC_ASSERT(utilities::isNumber(real_amount_to_transfer), "inputed amount is not a number");
                auto ipos = real_amount_to_transfer.find(".");
                
                if (ipos != string::npos) {
                    string str = real_amount_to_transfer.substr(ipos + 1);
                    int64_t precision_input = static_cast<int64_t>(pow(10, str.size()));
                    FC_ASSERT((precision_input <= precision), "Precision is not correct");
                }
                
                double dAmountToTransfer = std::stod(real_amount_to_transfer);
                ShareType amount_to_transfer = static_cast<ShareType>(floor(dAmountToTransfer * precision + 0.5));
                Asset asset_to_transfer(amount_to_transfer, asset_id);
                PrivateKeyType sender_private_key = get_active_private_key(from_account_name);
                PublicKeyType  sender_public_key = sender_private_key.get_public_key();
                Address          sender_account_address(sender_private_key.get_public_key());
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                
                if (alp_account != "") {
                    //trx.from_account = from_account_name;
                    trx.alp_account = alp_account;
                    trx.alp_inport_asset = asset_to_transfer;
                }
                
                //    if (memo_message != "")
                //    {
                //        trx.AddtionImessage(memo_message);
                //    }
                const auto required_fees = get_transaction_fee(asset_to_transfer.asset_id);
                const auto required_imessage_fee = get_transaction_imessage_fee(memo_message);
                
                if (required_fees.asset_id == asset_to_transfer.asset_id) {
                    my->withdraw_to_transaction(required_fees + asset_to_transfer + required_imessage_fee,
                                                from_account_name,
                                                trx,
                                                required_signatures);
                                                
                } else {
                    my->withdraw_to_transaction(asset_to_transfer,
                                                from_account_name,
                                                trx,
                                                required_signatures);
                    my->withdraw_to_transaction(required_fees + required_imessage_fee,
                                                from_account_name,
                                                trx,
                                                required_signatures);
                }
                
                trx.deposit(to_address, asset_to_transfer);
                trx.expiration = blockchain::now() + get_transaction_expiration();
                my->set_delegate_slate(trx, strategy);
                //       if( sign )
                //           my->sign_transaction( trx, required_signatures );
                auto entry = LedgerEntry();
                entry.from_account = sender_public_key;
                entry.amount = asset_to_transfer;
                
                if (memo_message != "") {
                    entry.memo = memo_message;
                    trx.AddtionImessage(memo_message);
                    //AddtionImessage(memo_message);
                    
                } else
                    entry.memo = "To: " + string(to_address).substr(0, 8) + "...";
                    
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                try {
                    auto account_rec = my->_blockchain->get_account_entry(to_address);
                    
                    if (account_rec.valid()) {
                        entry.to_account = account_rec->owner_key;
                        
                    } else {
                        auto acc_rec = get_account_for_address(to_address);
                        
                        if (acc_rec.valid()) {
                            entry.to_account = acc_rec->owner_key;
                        }
                    }
                    
                } catch (...) {
                }
                
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees + required_imessage_fee;
                trans_entry.extra_addresses.push_back(to_address);
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((real_amount_to_transfer)(amount_to_transfer_symbol)(from_account_name)(to_address)(memo_message))
        }
        
        // common account -> contract account  (contract balance)
        thinkyoung::wallet::WalletTransactionEntry Wallet::transfer_asset_to_contract(
            double real_amount_to_transfer,
            const string& amount_to_transfer_symbol,
            const string& from_account_name,
            const Address& to_contract_address,
            double exec_cost,
            bool sign,
            bool is_testing) {
            try {
                ChainInterfacePtr data_ptr = get_correct_state_ptr();
                FC_ASSERT(exec_cost >= 0, "exec_cost should greater or equal than 0");
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(amount_to_transfer_symbol == ALP_BLOCKCHAIN_SYMBOL, "Asset symbol should be ACT");
                FC_ASSERT(data_ptr->is_valid_symbol(amount_to_transfer_symbol), "Invalid asset symbol");
                FC_ASSERT(my->is_receive_account(from_account_name), "Invalid account name");
                const auto asset_rec = data_ptr->get_asset_entry(amount_to_transfer_symbol);
                FC_ASSERT(asset_rec.valid(), "Asset not exist!");
                const auto asset_id = asset_rec->id;
                FC_ASSERT(asset_id == 0, "Asset symbol should be ACT");
                const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
                //ShareType amount_to_transfer = real_amount_to_transfer * precision;
                //Asset asset_to_transfer(amount_to_transfer, asset_id);
                FC_ASSERT(real_amount_to_transfer>=1.0/precision, "transfer amount must bigger than 0");
                Asset asset_to_transfer = to_asset(asset_rec->id, precision, real_amount_to_transfer);
                //ShareType amount_for_exec = exec_cost * precision;
                //Asset asset_for_exec(amount_for_exec, asset_id);
                Asset asset_for_exec = to_asset(asset_rec->id, precision, exec_cost);
                PublicKeyType  sender_public_key = get_owner_public_key(from_account_name);
                Address        sender_account_address = Address(sender_public_key);
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                const auto required_fees = get_transaction_fee(asset_to_transfer.asset_id);
                map<BalanceIdType, ShareType> balances;
                
                if (!is_testing) {
                    if (my->_blockchain->get_is_in_sandbox())
                        sandbox_get_enough_balances(from_account_name, required_fees + asset_to_transfer + asset_for_exec, balances, required_signatures);
                        
                    else
                        get_enough_balances(from_account_name, required_fees + asset_to_transfer + asset_for_exec, balances, required_signatures);
                        
                } else
                    required_signatures.insert(sender_account_address);
                    
                my->transfer_to_contract_trx(trx, to_contract_address, asset_to_transfer, asset_for_exec, required_fees, sender_public_key, balances);
                //trx.deposit_to_contract(to_contract_address, asset_to_transfer);
                trx.expiration = blockchain::now() + get_transaction_expiration();
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                auto entry = LedgerEntry();
                entry.from_account = sender_public_key;
                entry.amount = asset_to_transfer;
                auto trans_entry = WalletTransactionEntry();
                trans_entry.entry_id = trx.id();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                trans_entry.extra_addresses.push_back(to_contract_address);
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((real_amount_to_transfer)(amount_to_transfer_symbol)(from_account_name)(to_contract_address))
        }
        
        std::vector<thinkyoung::blockchain::Asset> Wallet::transfer_asset_to_contract_testing(
            double real_amount_to_transfer,
            const string& amount_to_transfer_symbol,
            const string& from_account_name,
            const Address& to_contract_address,
            bool sign) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(amount_to_transfer_symbol == ALP_BLOCKCHAIN_SYMBOL, "Asset symbol should be ACT");
                FC_ASSERT(my->_blockchain->is_valid_symbol(amount_to_transfer_symbol), "Invalid asset symbol");
                FC_ASSERT(my->is_receive_account(from_account_name), "Invalid account name");
                const auto asset_rec = my->_blockchain->get_asset_entry(amount_to_transfer_symbol);
                FC_ASSERT(asset_rec.valid(), "Asset not exist!");
                const auto asset_id = asset_rec->id;
                FC_ASSERT(asset_id == 0, "Asset symbol should be ACT");
                const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
                ShareType amount_to_transfer = real_amount_to_transfer * precision;
                Asset asset_to_transfer(amount_to_transfer, asset_id);
                Asset asset_for_exec = my->_blockchain->get_amount(CONTRACT_TESTING_LIMIT_MAX);
                Asset required_fees = get_transaction_fee(asset_to_transfer.asset_id);
                SignedTransaction     trx;
                auto trans_entry = transfer_asset_to_contract(real_amount_to_transfer,
                                   amount_to_transfer_symbol,
                                   from_account_name,
                                   to_contract_address,
                                   (double)asset_for_exec.amount / ALP_BLOCKCHAIN_PRECISION,
                                   true,
                                   true);
                trx = trans_entry.trx;
                ChainInterfacePtr state_ptr = get_correct_state_ptr();
                PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(state_ptr);
                TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
                trx_eval_state->skipexec = false;
                trx_eval_state->evaluate_contract_testing = true;
                trx_eval_state->evaluate(trx);
                std::vector<thinkyoung::blockchain::Asset> asset_vec;
                asset_vec.emplace_back(required_fees);
                asset_vec.emplace_back(asset_to_transfer);
                asset_vec.emplace_back(trx_eval_state->exec_cost);
                return asset_vec;
            }
            
            FC_CAPTURE_AND_RETHROW((real_amount_to_transfer)(amount_to_transfer_symbol)(from_account_name)(to_contract_address))
        }
        
        
        std::vector<thinkyoung::blockchain::Asset> Wallet::upgrade_contract_testing(
            const Address& contract_id,
            const string& upgrader_name,
            const string& new_contract_name,
            const string& new_contract_desc,
            bool sign
        ) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                ChainInterfacePtr data_ptr = get_correct_state_ptr();
                oContractEntry contract_entry = data_ptr->get_contract_entry(contract_id);
                PublicKeyType  upgrader_owner_key = get_owner_public_key(upgrader_name);
                
                if (!contract_entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (contract_id));
                    
                if (NOT data_ptr->is_temporary_contract(contract_entry->level))
                    FC_CAPTURE_AND_THROW(contract_upgraded, (contract_id));
                    
                if (data_ptr->is_destroyed_contract(contract_entry->state))
                    FC_CAPTURE_AND_THROW(contract_destroyed, (contract_id));
                    
                if (upgrader_owner_key != contract_entry->owner)
                    FC_CAPTURE_AND_THROW(contract_no_permission, (upgrader_name));
                    
                if (NOT data_ptr->is_valid_contract_name(new_contract_name))
                    FC_CAPTURE_AND_THROW(contract_name_illegal, (new_contract_name));
                    
                if (NOT data_ptr->is_valid_contract_description(new_contract_desc))
                    FC_CAPTURE_AND_THROW(contract_desc_illegal, (new_contract_desc));
                    
                if (data_ptr->get_contract_entry(new_contract_name).valid())
                    FC_CAPTURE_AND_THROW(contract_name_in_use, (new_contract_name));
                    
                // check contract margin
                BalanceIdType margin_balance_id = data_ptr->get_balanceid(contract_entry->id, WithdrawBalanceTypes::withdraw_margin_type);
                oBalanceEntry margin_balance_entry = data_ptr->get_balance_entry(margin_balance_id);
                FC_ASSERT(margin_balance_entry.valid(), "invalid margin balance id");
                FC_ASSERT(margin_balance_entry->asset_id() == 0, "invalid margin balance asset type");
                
                if (margin_balance_entry->balance != ALP_DEFAULT_CONTRACT_MARGIN)
                    FC_CAPTURE_AND_THROW(invalid_margin_amount, (margin_balance_entry->balance));
                    
                Asset margin = Asset(ALP_DEFAULT_CONTRACT_MARGIN);
                Asset fee = get_transaction_fee(0);
                Asset asset_for_exec = my->_blockchain->get_amount(CONTRACT_TESTING_LIMIT_MAX);
                auto trans_entry = upgrade_contract(contract_id,
                                                    upgrader_name,
                                                    new_contract_name,
                                                    new_contract_desc,
                                                    ALP_BLOCKCHAIN_SYMBOL,
                                                    (double)asset_for_exec.amount / ALP_BLOCKCHAIN_PRECISION,
                                                    sign,
                                                    true
                                                   );
                SignedTransaction     trx = trans_entry.trx;
                ChainInterfacePtr state_ptr = get_correct_state_ptr();
                PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(state_ptr);
                TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
                trx_eval_state->skipexec = false;
                trx_eval_state->evaluate_contract_testing = true;
                trx_eval_state->evaluate(trx);
                std::vector<thinkyoung::blockchain::Asset> asset_vec;
                asset_vec.emplace_back(fee);
                asset_vec.emplace_back(trx_eval_state->exec_cost);
                asset_vec.emplace_back(margin);
                return asset_vec;
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(upgrader_name)(new_contract_name)(new_contract_desc))
        }
        
        WalletTransactionEntry Wallet::upgrade_contract(
            const Address& contract_id,
            const string& upgrader_name,
            const string& new_contract_name,
            const string& new_contract_desc,
            const std::string& asset_symbol,
            const double exec_limit,
            bool sign,
            bool is_testing) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                PublicKeyType  upgrader_owner_key = get_owner_public_key(upgrader_name);
                Address        upgrader_owner_address = Address(upgrader_owner_key);
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                ChainInterfacePtr data_ptr = get_correct_state_ptr();
                const auto required_fees = get_transaction_fee();
                oContractEntry contract_entry = data_ptr->get_contract_entry(contract_id);
                
                if (!contract_entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (contract_id));
                    
                if (NOT data_ptr->is_temporary_contract(contract_entry->level))
                    FC_CAPTURE_AND_THROW(contract_upgraded, (contract_id));
                    
                if (data_ptr->is_destroyed_contract(contract_entry->state))
                    FC_CAPTURE_AND_THROW(contract_destroyed, (contract_id));
                    
                if (upgrader_owner_key != contract_entry->owner)
                    FC_CAPTURE_AND_THROW(contract_no_permission, (upgrader_name));
                    
                if (NOT data_ptr->is_valid_contract_name(new_contract_name))
                    FC_CAPTURE_AND_THROW(contract_name_illegal, (new_contract_name));
                    
                if (NOT data_ptr->is_valid_contract_description(new_contract_desc))
                    FC_CAPTURE_AND_THROW(contract_desc_illegal, (new_contract_desc));
                    
                if (data_ptr->get_contract_entry(new_contract_name).valid())
                    FC_CAPTURE_AND_THROW(contract_name_in_use, (new_contract_name));
                    
                const auto asset_rec = data_ptr->get_asset_entry(asset_symbol);
                FC_ASSERT(asset_rec.valid(), "Asset not exist!");
                FC_ASSERT(asset_rec->id == 0, "asset_symbol must be ACT");
                const auto asset_id = asset_rec->id;
                const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
                //ShareType amount_limit = cost_limit * precision;
                //Asset asset_limit(amount_limit, asset_id);
                Asset asset_limit = to_asset(asset_rec->id, precision, exec_limit);
                map<BalanceIdType, ShareType> balances;
                
                if (!is_testing) {
                    if (my->_blockchain->get_is_in_sandbox())
                        sandbox_get_enough_balances(upgrader_name, required_fees + asset_limit, balances, required_signatures);
                        
                    else
                        get_enough_balances(upgrader_name, required_fees + asset_limit, balances, required_signatures);
                        
                } else
                    required_signatures.insert(upgrader_owner_address);
                    
                trx.upgrade_contract(contract_id, new_contract_name, new_contract_desc, asset_limit, required_fees, balances);
                BalanceIdType margin_balance_id = data_ptr->get_balanceid(contract_id,
                                                  WithdrawBalanceTypes::withdraw_margin_type);
                oBalanceEntry balance_entry = data_ptr->get_balance_entry(margin_balance_id);
                FC_ASSERT(balance_entry.valid(), "Can not get margin balance!");
                FC_ASSERT(balance_entry->asset_id() == 0, "margin balance asset is not ACT!");
                Asset margin_balance(balance_entry->balance, balance_entry->asset_id());
                trx.expiration = blockchain::now() + get_transaction_expiration();
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                //for scanning contract because upgrading contract
                my->_dirty_contracts = true;
                auto entry = LedgerEntry();
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees + margin_balance;
                trans_entry.extra_addresses.push_back(contract_id);
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(upgrader_name)(new_contract_name)(new_contract_desc))
        }
        
        std::vector<thinkyoung::blockchain::Asset> Wallet::destroy_contract_testing(
            const Address& contract_id,
            const string& destroyer_name,
            bool sign
        ) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                ChainInterfacePtr data_ptr = get_correct_state_ptr();
                const auto required_fees = get_transaction_fee();
                PublicKeyType  destroyer_owner_key = get_owner_public_key(destroyer_name);
                oContractEntry contract_entry = data_ptr->get_contract_entry(contract_id);
                
                if (!contract_entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (contract_id));
                    
                if (data_ptr->is_destroyed_contract(contract_entry->state))
                    FC_CAPTURE_AND_THROW(contract_destroyed, (contract_id));
                    
                if (NOT data_ptr->is_temporary_contract(contract_entry->level))
                    FC_CAPTURE_AND_THROW(permanent_contract, (contract_id));
                    
                if (destroyer_owner_key != contract_entry->owner)
                    FC_CAPTURE_AND_THROW(contract_no_permission, (destroyer_name));
                    
                Asset fee = get_transaction_fee(0);
                Asset asset_for_exec = my->_blockchain->get_amount(CONTRACT_TESTING_LIMIT_MAX);
                auto trans_entry = destroy_contract(contract_id,
                                                    destroyer_name,
                                                    ALP_BLOCKCHAIN_SYMBOL,
                                                    (double)asset_for_exec.amount / ALP_BLOCKCHAIN_PRECISION,
                                                    sign,
                                                    true
                                                   );
                SignedTransaction     trx = trans_entry.trx;
                ChainInterfacePtr state_ptr = get_correct_state_ptr();
                PendingChainStatePtr          pend_state = std::make_shared<PendingChainState>(state_ptr);
                TransactionEvaluationStatePtr trx_eval_state = std::make_shared<TransactionEvaluationState>(pend_state.get());
                trx_eval_state->skipexec = false;
                trx_eval_state->evaluate_contract_testing = true;
                trx_eval_state->evaluate(trx);
                std::vector<thinkyoung::blockchain::Asset> asset_vec;
                asset_vec.emplace_back(fee);
                asset_vec.emplace_back(trx_eval_state->exec_cost);
                return asset_vec;
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(destroyer_name))
        }
        
        
        WalletTransactionEntry Wallet::destroy_contract(
            const Address& contract_id,
            const string& destroyer_name,
            const std::string& asset_symbol,
            double exec_limit,
            bool sign,
            bool is_testing) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                PublicKeyType  destroyer_owner_key = get_owner_public_key(destroyer_name);
                Address        destroyer_owner_address = Address(destroyer_owner_key);
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                ChainInterfacePtr data_ptr = get_correct_state_ptr();
                const auto required_fees = get_transaction_fee();
                oContractEntry contract_entry = data_ptr->get_contract_entry(contract_id);
                
                if (!contract_entry.valid())
                    FC_CAPTURE_AND_THROW(contract_not_exist, (contract_id));
                    
                if (data_ptr->is_destroyed_contract(contract_entry->state))
                    FC_CAPTURE_AND_THROW(contract_destroyed, (contract_id));
                    
                if (NOT data_ptr->is_temporary_contract(contract_entry->level))
                    FC_CAPTURE_AND_THROW(permanent_contract, (contract_id));
                    
                if (destroyer_owner_key != contract_entry->owner)
                    FC_CAPTURE_AND_THROW(contract_no_permission, (destroyer_name));
                    
                const auto asset_rec = data_ptr->get_asset_entry(asset_symbol);
                FC_ASSERT(asset_rec.valid(), "Asset not exist!");
                FC_ASSERT(asset_rec->id == 0, "asset_symbol must be ACT");
                const auto asset_id = asset_rec->id;
                const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
                //ShareType amount_limit = cost_limit * precision;
                //Asset asset_limit(amount_limit, asset_id);
                Asset asset_limit = to_asset(asset_rec->id, precision, exec_limit);
                map<BalanceIdType, ShareType> balances;
                
                if (!is_testing) {
                    if (my->_blockchain->get_is_in_sandbox())
                        sandbox_get_enough_balances(destroyer_name, required_fees + asset_limit, balances, required_signatures);
                        
                    else
                        get_enough_balances(destroyer_name, required_fees + asset_limit, balances, required_signatures);
                        
                } else
                    required_signatures.insert(destroyer_owner_address);
                    
                trx.destroy_contract(contract_id, asset_limit, required_fees, balances);
                BalanceIdType margin_balance_id = data_ptr->get_balanceid(contract_id,
                                                  WithdrawBalanceTypes::withdraw_margin_type);
                oBalanceEntry balance_entry = data_ptr->get_balance_entry(margin_balance_id);
                FC_ASSERT(balance_entry.valid(), "Can not get margin balance!");
                FC_ASSERT(balance_entry->asset_id() == 0, "margin balance asset is not ACT!");
                FC_ASSERT(balance_entry->balance == ALP_DEFAULT_CONTRACT_MARGIN, "invalid margin balance amount");
                BalanceIdType contract_balance_id = data_ptr->get_balanceid(contract_id,
                                                    WithdrawBalanceTypes::withdraw_contract_type);
                balance_entry = data_ptr->get_balance_entry(contract_balance_id);
                
                if (balance_entry.valid()) {
                    FC_ASSERT(balance_entry->asset_id() == 0, "contract balance asset is not ACT!");
                }
                
                trx.expiration = blockchain::now() + get_transaction_expiration();
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                //for scanning contract because destroying contract
                my->_dirty_contracts = true;
                auto entry = LedgerEntry();
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                trans_entry.extra_addresses.push_back(contract_id);
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((contract_id)(destroyer_name))
        }
        
        
        WalletTransactionEntry Wallet::transfer_asset_to_many_address(
            const string& amount_to_transfer_symbol,
            const string& from_account_name,
            const std::unordered_map<Address, double>& to_address_amounts,
            const string& memo_message,
            bool sign) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(my->_blockchain->is_valid_symbol(amount_to_transfer_symbol));
                FC_ASSERT(my->is_receive_account(from_account_name));
                FC_ASSERT(to_address_amounts.size() > 0);
                auto asset_rec = my->_blockchain->get_asset_entry(amount_to_transfer_symbol);
                FC_ASSERT(asset_rec.valid());
                auto asset_id = asset_rec->id;
                PrivateKeyType sender_private_key = get_active_private_key(from_account_name);
                PublicKeyType  sender_public_key = sender_private_key.get_public_key();
                Address          sender_account_address(sender_private_key.get_public_key());
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                Asset total_asset_to_transfer(0, asset_id);
                auto required_fees = get_transaction_fee();
                vector<Address> to_addresses;
                
                for (const auto& address_amount : to_address_amounts) {
                    auto real_amount_to_transfer = address_amount.second;
                    ShareType amount_to_transfer((ShareType)(real_amount_to_transfer * asset_rec->precision));
                    Asset asset_to_transfer(amount_to_transfer, asset_id);
                    my->withdraw_to_transaction(asset_to_transfer,
                                                from_account_name,
                                                trx,
                                                required_signatures);
                    total_asset_to_transfer += asset_to_transfer;
                    trx.deposit(address_amount.first, asset_to_transfer);
                    to_addresses.push_back(address_amount.first);
                }
                
                my->withdraw_to_transaction(required_fees,
                                            from_account_name,
                                            trx,
                                            required_signatures);
                auto entry = LedgerEntry();
                entry.from_account = sender_public_key;
                entry.amount = total_asset_to_transfer;
                
                if (memo_message != "")
                    entry.memo = memo_message;
                    
                else
                    entry.memo = "Transfer to many addresses";
                    
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                trans_entry.extra_addresses = to_addresses;
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((amount_to_transfer_symbol)(from_account_name)(to_address_amounts)(memo_message))
        }
        
        WalletTransactionEntry Wallet::register_account(
            const string& account_to_register,
            const variant& public_data,
            uint8_t delegate_pay_rate,
            const string& pay_with_account_name,
            AccountType new_account_type,
            bool sign) {
            try {
                if (!my->_blockchain->is_valid_account_name(account_to_register))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_to_register", account_to_register));
                    
                if (delegate_pay_rate < 100) {
                    FC_THROW_EXCEPTION(invalid_delegate_pay_rate, "invalid_delegate_pay_rate", ("delegate_pay_rate", delegate_pay_rate));
                }
                
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                const auto registered_account = my->_blockchain->get_account_entry(account_to_register);
                
                if (registered_account.valid())
                    FC_THROW_EXCEPTION(duplicate_account_name, "This account name has already been registered!");
                    
                const auto payer_public_key = get_owner_public_key(pay_with_account_name);
                Address from_account_address(payer_public_key);
                const auto account_public_key = get_owner_public_key(account_to_register);
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                optional<AccountMetaInfo> meta_info = AccountMetaInfo(new_account_type);
                
                // TODO: This is a hack to register with different owner and active keys until the API is fixed
                try {
                    const WalletAccountEntry local_account = get_account(account_to_register);
                    trx.register_account(account_to_register,
                                         public_data,
                                         local_account.owner_key,
                                         local_account.active_key(),
                                         delegate_pay_rate <= 100 ? delegate_pay_rate : -1,
                                         meta_info);
                                         
                } catch (...) {
                    trx.register_account(account_to_register,
                                         public_data,
                                         account_public_key, // master
                                         account_public_key, // active
                                         delegate_pay_rate <= 100 ? delegate_pay_rate : -1,
                                         meta_info);
                }
                
                auto required_fees = get_transaction_fee();
                bool as_delegate = false;
                
                if (delegate_pay_rate <= 100) {
                    required_fees += Asset(my->_blockchain->get_delegate_registration_fee(delegate_pay_rate), 0);
                    as_delegate = true;
                    
                } else {
#if 0
                    required_fees += Asset(ALP_BLOCKCHAIN_REGISTER_ACCOUNT_FEE, 0);
#endif
                }
                
                my->withdraw_to_transaction(required_fees,
                                            pay_with_account_name,
                                            trx,
                                            required_signatures);
                auto entry = LedgerEntry();
                entry.from_account = payer_public_key;
                entry.to_account = account_public_key;
                entry.memo = "register " + account_to_register + (as_delegate ? " as a delegate" : "");
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_to_register)(public_data)(pay_with_account_name)(delegate_pay_rate))
        }
        
        WalletTransactionEntry Wallet::create_asset(
            const string& symbol,
            const string& asset_name,
            const string& description,
            const variant& data,
            const string& issuer_account_name,
            const string& max_share_supply,
            uint64_t precision,
            bool is_market_issued,
            bool sign) {
            try {
                FC_ASSERT(blockchain::is_power_of_ten(precision));
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(my->_blockchain->is_valid_symbol_name(symbol)); // valid length and characters
                FC_ASSERT(!my->_blockchain->is_valid_symbol(symbol)); // not yet registered
                SignedTransaction     trx;
                unordered_set<Address> required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                auto required_fees = get_transaction_fee();
                required_fees += Asset(my->_blockchain->get_asset_registration_fee(symbol.size()), 0);
                
                if (!my->_blockchain->is_valid_account_name(issuer_account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("issuer_account_name", issuer_account_name));
                    
                auto from_account_address = get_owner_public_key(issuer_account_name);
                auto oname_rec = my->_blockchain->get_account_entry(issuer_account_name);
                
                if (!oname_rec.valid())
                    FC_THROW_EXCEPTION(account_not_registered, "Assets can only be created by registered accounts", ("issuer_account_name", issuer_account_name));
                    
                my->withdraw_to_transaction(required_fees,
                                            issuer_account_name,
                                            trx,
                                            required_signatures);
                required_signatures.insert(oname_rec->active_key());
                //check this way to avoid overflow
                auto ipos = max_share_supply.find(".");
                
                if (ipos != string::npos) {
                    string str = max_share_supply.substr(ipos + 1);
                    int64_t precision_input = static_cast<int64_t>(pow(10, str.size()));
                    FC_ASSERT((static_cast<uint64_t>(precision_input) <= precision), "Precision is not correct");
                }
                
                double dAmountToCreate = std::stod(max_share_supply);
                ShareType max_share_supply_in_internal_units = floor(dAmountToCreate * precision + 0.5);
                FC_ASSERT(ALP_BLOCKCHAIN_MAX_SHARES > max_share_supply_in_internal_units);
                
                if (NOT is_market_issued) {
                    trx.create_asset(symbol, asset_name,
                                     description, data,
                                     oname_rec->id, max_share_supply_in_internal_units, precision);
                                     
                } else {
                    trx.create_asset(symbol, asset_name,
                                     description, data,
                                     AssetEntry::market_issuer_id, max_share_supply_in_internal_units, precision);
                }
                
                auto entry = LedgerEntry();
                entry.from_account = from_account_address;
                entry.to_account = from_account_address;
                entry.memo = "create " + symbol + " (" + asset_name + ")";
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((symbol)(asset_name)(description)(issuer_account_name))
        }
        
        WalletTransactionEntry Wallet::update_asset(
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
        ) {
            try {
                if (NOT is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (NOT is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                optional<AccountIdType> issuer_account_id;
                
                if (issuer_account_name != "") {
                    auto issuer_account = my->_blockchain->get_account_entry(issuer_account_name);
                    FC_ASSERT(issuer_account.valid());
                    issuer_account_id = issuer_account->id;
                }
                
                TransactionBuilderPtr builder = create_transaction_builder();
                builder->update_asset(symbol, name, description, public_data, maximum_share_supply, precision,
                                      issuer_fee, market_fee, flags, issuer_perms, issuer_account_id, required_sigs, authority);
                builder->finalize();
                
                if (sign)
                    return builder->sign();
                    
                return builder->transaction_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((symbol)(name)(description)(public_data)(precision)(issuer_fee)(restricted)(retractable)(required_sigs)(authority)(sign))
        }
        
        WalletTransactionEntry Wallet::issue_asset(
            const string& amount_to_issue,
            const string& symbol,
            const string& to_account_name,
            const string& memo_message,
            bool sign) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(my->_blockchain->is_valid_symbol(symbol));
                SignedTransaction         trx;
                unordered_set<Address>     required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                auto required_fees = get_transaction_fee();
                auto asset_entry = my->_blockchain->get_asset_entry(symbol);
                FC_ASSERT(asset_entry.valid(), "no such asset entry");
                auto issuer_account = my->_blockchain->get_account_entry(asset_entry->issuer_account_id);
                FC_ASSERT(issuer_account, "uh oh! no account for valid asset");
                auto authority = asset_entry->authority;
                auto ipos = amount_to_issue.find(".");
                
                if (ipos != string::npos) {
                    string str = amount_to_issue.substr(ipos + 1);
                    int64_t precision_input = static_cast<int64_t>(pow(10, str.size()));
                    FC_ASSERT((static_cast<uint32_t>(precision_input) <= asset_entry->precision), "Precision is not correct");
                }
                
                double dAmountToIssue = std::stod(amount_to_issue);
                Asset shares_to_issue(static_cast<ShareType>(floor(dAmountToIssue * asset_entry->precision + 0.5)), asset_entry->id);
                my->withdraw_to_transaction(required_fees,
                                            issuer_account->name,
                                            trx,
                                            required_signatures);
                trx.issue(shares_to_issue);
                
                for (auto owner : authority.owners)
                    required_signatures.insert(owner);
                    
                oWalletAccountEntry issuer = my->_wallet_db.lookup_account(asset_entry->issuer_account_id);
                FC_ASSERT(issuer.valid());
                oWalletKeyEntry issuer_key = my->_wallet_db.lookup_key(issuer->active_address());
                FC_ASSERT(issuer_key && issuer_key->has_private_key());
                auto sender_private_key = issuer_key->decrypt_private_key(my->_wallet_password);
                const WalletAccountEntry receiver_account = get_account(to_account_name);
                trx.deposit_to_account(receiver_account.active_key(),
                                       shares_to_issue,
                                       sender_private_key,
                                       memo_message,
                                       issuer->active_key(),
                                       my->get_new_private_key(issuer_account->name),
                                       from_memo,
                                       receiver_account.is_titan_account()
                                      );
                auto entry = LedgerEntry();
                entry.from_account = issuer->active_key();
                entry.to_account = receiver_account.active_key();
                entry.amount = shares_to_issue;
                entry.memo = "issue " + my->_blockchain->to_pretty_asset(shares_to_issue);
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                
                if (sign)
                    my->sign_transaction(trx, required_signatures);
                    
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        WalletTransactionEntry Wallet::issue_asset_to_addresses(
            const string& symbol,
            const map<string, ShareType>& addresses) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                FC_ASSERT(my->_blockchain->is_valid_symbol(symbol));
                SignedTransaction         trx;
                unordered_set<Address>     required_signatures;
                trx.expiration = blockchain::now() + get_transaction_expiration();
                auto required_fees = get_transaction_fee();
                auto asset_entry = my->_blockchain->get_asset_entry(symbol);
                FC_ASSERT(asset_entry.valid(), "no such asset entry");
                auto issuer_account = my->_blockchain->get_account_entry(asset_entry->issuer_account_id);
                FC_ASSERT(issuer_account, "uh oh! no account for valid asset");
                auto authority = asset_entry->authority;
                Asset shares_to_issue(0, asset_entry->id);
                
                for (auto pair : addresses) {
                    auto addr = Address(pair.first);
                    auto amount = Asset(pair.second, asset_entry->id);
                    trx.deposit(addr, amount);
                    shares_to_issue += amount;
                }
                
                my->withdraw_to_transaction(required_fees,
                                            issuer_account->name,
                                            trx,
                                            required_signatures);
                trx.issue(shares_to_issue);
                
                for (auto owner : authority.owners)
                    required_signatures.insert(owner);
                    
                //      required_signatures.insert( issuer_account->active_key() );
                oWalletAccountEntry issuer = my->_wallet_db.lookup_account(asset_entry->issuer_account_id);
                FC_ASSERT(issuer.valid());
                oWalletKeyEntry  issuer_key = my->_wallet_db.lookup_key(issuer->owner_address());
                FC_ASSERT(issuer_key && issuer_key->has_private_key());
                auto entry = LedgerEntry();
                entry.from_account = issuer->active_key();
                entry.amount = shares_to_issue;
                entry.memo = "issue to many addresses";
                auto trans_entry = WalletTransactionEntry();
                trans_entry.ledger_entries.push_back(entry);
                trans_entry.fee = required_fees;
                //    if( sign )
                my->sign_transaction(trx, required_signatures);
                trans_entry.trx = trx;
                return trans_entry;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::update_account_private_data(const string& account_to_update,
                const variant& private_data) {
            get_account(account_to_update); /* Just to check input */
            auto oacct = my->_wallet_db.lookup_account(account_to_update);
            FC_ASSERT(oacct.valid());
            oacct->private_data = private_data;
            my->_wallet_db.store_account(*oacct);
        }
        
        WalletTransactionEntry Wallet::update_registered_account(
            const string& account_to_update,
            const string& pay_from_account,
            optional<variant> public_data,
            uint8_t delegate_pay_rate,
            bool sign) {
            try {
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                
                if (delegate_pay_rate < 100) {
                    FC_THROW_EXCEPTION(invalid_delegate_pay_rate, "invalid_delegate_pay_rate", ("delegate_pay_rate", delegate_pay_rate));
                }
                
                auto account = get_account(account_to_update);
                oWalletAccountEntry payer;
                
                if (!pay_from_account.empty()) payer = get_account(pay_from_account);
                
                optional<uint8_t> pay;
                
                if (delegate_pay_rate <= 100)
                    pay = delegate_pay_rate;
                    
                TransactionBuilderPtr builder = create_transaction_builder();
                builder->update_account_registration(account, public_data, optional<PublicKeyType>(), pay, payer).
                finalize();
                
                if (sign)
                    return builder->sign();
                    
                return builder->transaction_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_to_update)(pay_from_account)(public_data)(sign))
        }
        
        WalletTransactionEntry Wallet::update_active_key(
            const std::string& account_to_update,
            const std::string& pay_from_account,
            const std::string& new_active_key,
            bool sign) {
            try {
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto account = get_account(account_to_update);
                oWalletAccountEntry payer;
                
                if (!pay_from_account.empty()) payer = get_account(pay_from_account);
                
                PublicKeyType new_public_key;
                
                if (new_active_key.empty()) {
                    new_public_key = my->get_new_public_key(account_to_update);
                    
                } else {
                    const optional<PrivateKeyType> new_private_key = utilities::wif_to_key(new_active_key);
                    FC_ASSERT(new_private_key.valid(), "Unable to parse new active key.");
                    new_public_key = import_private_key(*new_private_key, account_to_update, false);
                }
                
                TransactionBuilderPtr builder = create_transaction_builder();
                builder->update_account_registration(account, optional<variant>(), new_public_key, optional<ShareType>(), payer).
                finalize();
                
                if (sign) {
                    my->_dirty_accounts = true;
                    return builder->sign();
                }
                
                return builder->transaction_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_to_update)(pay_from_account)(sign))
        }
        
        WalletTransactionEntry Wallet::retract_account(
            const std::string& account_to_retract,
            const std::string& pay_from_account,
            bool sign) {
            try {
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto account = get_account(account_to_retract);
                oWalletAccountEntry payer;
                
                if (!pay_from_account.empty()) payer = get_account(pay_from_account);
                
                fc::ecc::public_key empty_pk;
                PublicKeyType new_public_key(empty_pk);
                TransactionBuilderPtr builder = create_transaction_builder();
                builder->update_account_registration(account, optional<variant>(), new_public_key, optional<ShareType>(), payer).
                finalize();
                
                if (sign)
                    return builder->sign();
                    
                return builder->transaction_entry;
            }
            
            FC_CAPTURE_AND_RETHROW((account_to_retract)(pay_from_account)(sign))
        }
        
        void Wallet::set_transaction_fee(const Asset& fee) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (fee.amount < ALP_DEFAULT_TRANSACTION_FEE || fee.asset_id != 0)
                    FC_THROW_EXCEPTION(invalid_fee, "Invalid transaction fee!", ("fee", fee));
                    
                my->_wallet_db.set_property(default_transaction_priority_fee, variant(fee));
            }
            
            FC_CAPTURE_AND_RETHROW((fee))
        }
        
        Asset Wallet::get_transaction_imessage_fee(const std::string & imessage)const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                Asset require_fee;
                auto max_soft_length = get_transaction_imessage_soft_max_length();
                
                if (imessage.size() > max_soft_length) {
                    FC_THROW_EXCEPTION(imessage_size_bigger_than_soft_max_lenth, "Invalid transaction imessage fee coefficient!", ("imessage_size", imessage.size()));
                }
                
                if (ALP_BLOCKCHAIN_MAX_FREE_MESSAGE_SIZE >= imessage.size()) {
                    return require_fee;
                }
                
                auto min_fee_coe = get_transaction_imessage_fee_coe();
                require_fee.amount = min_fee_coe * (imessage.size() - ALP_BLOCKCHAIN_MAX_FREE_MESSAGE_SIZE);
                //   require_fee.asset_id = 2;
                return require_fee;
            }
            
            FC_CAPTURE_AND_RETHROW((imessage.size()))
        }
        
        Asset Wallet::get_transaction_fee(const AssetIdType desired_fee_asset_id)const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                // TODO: support price conversion using price from blockchain
                Asset xts_fee(ALP_WALLET_DEFAULT_TRANSACTION_FEE, 0);
                
                try {
                    xts_fee = my->_wallet_db.get_property(default_transaction_priority_fee).as<Asset>();
                    
                } catch (...) {
                }
                
                /* //delete???
                if( desired_fee_asset_id != 0 )
                {
                const auto asset_rec = my->_blockchain->get_asset_entry( desired_fee_asset_id );
                FC_ASSERT( asset_rec.valid() );
                if( asset_rec->is_market_issued() )
                {
                auto median_price = my->_blockchain->get_median_delegate_price( desired_fee_asset_id, asset_id_type( 0 ) );
                if( median_price )
                {
                xts_fee += xts_fee + xts_fee;
                // fees paid in something other than XTS are discounted 50%
                auto alt_fees_paid = xts_fee * *median_price;
                return alt_fees_paid;
                }
                }
                }
                */
                return xts_fee;
            }
            
            FC_CAPTURE_AND_RETHROW((desired_fee_asset_id))
        }
        
        bool Wallet::asset_can_pay_fee(const AssetIdType desired_fee_asset_id) const {
            return get_transaction_fee(desired_fee_asset_id).asset_id == desired_fee_asset_id;
        }
        
        void Wallet::set_last_scanned_block_number(uint32_t block_num) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                my->_wallet_db.set_property(last_unlocked_scanned_block_number, fc::variant(block_num));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        void Wallet::set_last_scanned_block_number_for_alp(uint32_t block_num) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                my->_wallet_db.set_property(last_scanned_block_number_for_thinkyoung, fc::variant(block_num));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        uint32_t Wallet::get_last_scanned_block_number()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                try {
                    return my->_wallet_db.get_property(last_unlocked_scanned_block_number).as<uint32_t>();
                    
                } catch (...) {
                }
                
                return my->_blockchain->get_head_block_num();
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        uint32_t Wallet::get_last_scanned_block_number_for_alp()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                try {
                    return my->_wallet_db.get_property(last_scanned_block_number_for_thinkyoung).as<uint32_t>();
                    
                } catch (...) {
                }
                
                return my->_blockchain->get_head_block_num();
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        void Wallet::set_transaction_imessage_fee_coe(const ImessageIdType& coe) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (coe < ALP_BLOCKCHAIN_MIN_MESSAGE_FEE_COE) {
                    FC_THROW_EXCEPTION(invalid_transaction_imessage_fee_coe, "Invalid transaction imessage fee coefficient!", ("fee_coe", coe));
                }
                
                my->_wallet_db.set_property(transaction_min_imessage_fee_coe, fc::variant(coe));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        ImessageIdType Wallet::get_transaction_imessage_fee_coe()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                try {
                    return my->_wallet_db.get_property(transaction_min_imessage_fee_coe).as<ImessageIdType>();
                    
                } catch (...) {
                }
                
                return 0;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::set_transaction_imessage_soft_max_length(const ImessageLengthIdType& length) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (length < 0 || length > ALP_BLOCKCHAIN_MAX_MESSAGE_SIZE) {
                    FC_THROW_EXCEPTION(invalid_transaction_imessage_soft_length, "invalid imessage soft max length!", ("length", length));
                }
                
                my->_wallet_db.set_property(transaction_min_imessage_soft_length, fc::variant(length));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        ImessageLengthIdType Wallet::get_transaction_imessage_soft_max_length()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                try {
                    return my->_wallet_db.get_property(transaction_min_imessage_soft_length).as<ImessageIdType>();
                    
                } catch (...) {
                }
                
                return 0;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::set_transaction_expiration(uint32_t secs) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                
                if (secs > ALP_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC)
                    FC_THROW_EXCEPTION(invalid_expiration_time, "Invalid expiration time!", ("secs", secs));
                    
                my->_wallet_db.set_property(transaction_expiration_sec, fc::variant(secs));
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        uint32_t Wallet::get_transaction_expiration()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                return my->_wallet_db.get_property(transaction_expiration_sec).as<uint32_t>();
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        float Wallet::get_scan_progress()const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                return my->_scan_progress;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        string Wallet::get_key_label(const PublicKeyType& key)const {
            try {
                if (key == PublicKeyType())
                    return "ANONYMOUS";
                    
                auto account_entry = my->_wallet_db.lookup_account(key);
                
                if (account_entry.valid())
                    return account_entry->name;
                    
                const auto blockchain_account_entry = my->_blockchain->get_account_entry(key);
                
                if (blockchain_account_entry.valid())
                    return blockchain_account_entry->name;
                    
                const auto key_entry = my->_wallet_db.lookup_key(key);
                
                if (key_entry.valid()) {
                    if (key_entry->memo.valid())
                        return *key_entry->memo;
                        
                    account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                    
                    if (account_entry.valid())
                        return account_entry->name;
                }
                
                return string(key);
            }
            
            FC_CAPTURE_AND_RETHROW((key))
        }
        
        vector<string> Wallet::list() const {
            FC_ASSERT(is_enabled(), "Wallet is not enabled in this client!");
            vector<string> wallets;
            
            if (!fc::is_directory(get_data_directory()))
                return wallets;
                
            auto path = get_data_directory();
            fc::directory_iterator end_itr; // constructs terminator
            
            for (fc::directory_iterator itr(path); itr != end_itr; ++itr) {
                if (!itr->stem().string().empty() && fc::is_directory(*itr)) {
                    wallets.push_back((*itr).stem().string());
                }
            }
            
            std::sort(wallets.begin(), wallets.end());
            return wallets;
        }
        
        bool Wallet::is_sending_address(const Address& addr)const {
            try {
                return !is_receive_address(addr);
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        bool Wallet::is_receive_address(const Address& addr)const {
            try {
                auto key_rec = my->_wallet_db.lookup_key(addr);
                
                if (key_rec.valid())
                    return key_rec->has_private_key();
                    
                return false;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        vector<WalletAccountEntry> Wallet::list_accounts() const {
            try {
                const auto& accs = my->_wallet_db.get_accounts();
                vector<WalletAccountEntry> accounts;
                accounts.reserve(accs.size());
                
                for (const auto& item : accs)
                    accounts.push_back(item.second);
                    
                std::sort(accounts.begin(), accounts.end(),
                          [](const WalletAccountEntry& a, const WalletAccountEntry& b) -> bool
                { return a.name.compare(b.name) < 0; });
                return accounts;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        vector<AccountAddressData> Wallet::list_addresses() const {
            try {
                const auto& accs = my->_wallet_db.get_accounts();
                vector<AccountAddressData> receive_accounts;
                receive_accounts.reserve(accs.size());
                
                for (const auto& item : accs)
                    if (item.second.is_my_account) {
                        receive_accounts.push_back((item.second));
                    }
                    
                std::sort(receive_accounts.begin(), receive_accounts.end(),
                          [](const AccountAddressData& a, const AccountAddressData& b) -> bool
                { return a.name.compare(b.name) < 0; });
                return receive_accounts;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        vector<WalletAccountEntry> Wallet::list_my_accounts() const {
            try {
                const auto& accs = my->_wallet_db.get_accounts();
                vector<WalletAccountEntry> receive_accounts;
                receive_accounts.reserve(accs.size());
                
                for (const auto& item : accs)
                    if (item.second.is_my_account)
                        receive_accounts.push_back(item.second);
                        
                std::sort(receive_accounts.begin(), receive_accounts.end(),
                          [](const WalletAccountEntry& a, const WalletAccountEntry& b) -> bool
                { return a.name.compare(b.name) < 0; });
                return receive_accounts;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        
        vector<WalletAccountEntry> Wallet::list_favorite_accounts() const {
            try {
                const auto& accs = my->_wallet_db.get_accounts();
                vector<WalletAccountEntry> receive_accounts;
                receive_accounts.reserve(accs.size());
                
                for (const auto& item : accs) {
                    if (item.second.is_favorite) {
                        receive_accounts.push_back(item.second);
                    }
                }
                
                std::sort(receive_accounts.begin(), receive_accounts.end(),
                          [](const WalletAccountEntry& a, const WalletAccountEntry& b) -> bool
                { return a.name.compare(b.name) < 0; });
                return receive_accounts;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        vector<WalletAccountEntry> Wallet::list_unregistered_accounts() const {
            try {
                const auto& accs = my->_wallet_db.get_accounts();
                vector<WalletAccountEntry> receive_accounts;
                receive_accounts.reserve(accs.size());
                
                for (const auto& item : accs) {
                    if (item.second.id == 0) {
                        receive_accounts.push_back(item.second);
                    }
                }
                
                std::sort(receive_accounts.begin(), receive_accounts.end(),
                          [](const WalletAccountEntry& a, const WalletAccountEntry& b) -> bool
                { return a.name.compare(b.name) < 0; });
                return receive_accounts;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        vector<WalletTransactionEntry> Wallet::get_pending_transactions()const {
            return my->get_pending_transactions();
        }
        
        map<TransactionIdType, fc::exception> Wallet::get_pending_transaction_errors()const {
            try {
                map<TransactionIdType, fc::exception> transaction_errors;
                const auto& transaction_entrys = get_pending_transactions();
                const auto relay_fee = my->_blockchain->get_relay_fee();
                
                for (const auto& transaction_entry : transaction_entrys) {
                    FC_ASSERT(!transaction_entry.is_virtual && !transaction_entry.is_confirmed);
                    const auto error = my->_blockchain->get_transaction_error(transaction_entry.trx, relay_fee);
                    
                    if (!error.valid()) continue;
                    
                    transaction_errors[transaction_entry.trx.id()] = *error;
                }
                
                return transaction_errors;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        PrivateKeyType Wallet::get_active_private_key(const string& account_name)const {
            try {
                if (!my->_blockchain->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                auto opt_account = my->_wallet_db.lookup_account(account_name);
                FC_ASSERT(opt_account.valid(), "Unable to find account '${name}'",
                          ("name", account_name));
                auto opt_key = my->_wallet_db.lookup_key(opt_account->active_address());
                FC_ASSERT(opt_key.valid(), "Unable to find key for account '${name}",
                          ("name", account_name));
                FC_ASSERT(opt_key->has_private_key());
                return opt_key->decrypt_private_key(my->_wallet_password);
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        PublicKeyType Wallet::get_active_public_key(const string& account_name)const {
            try {
                if (!my->_blockchain->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                FC_ASSERT(my->is_unique_account(account_name));
                const auto registered_account = my->_blockchain->get_account_entry(account_name);
                
                if (registered_account.valid()) {
                    if (registered_account->is_retracted())
                        FC_CAPTURE_AND_THROW(account_retracted, (registered_account));
                        
                    return registered_account->active_key();
                }
                
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto opt_account = my->_wallet_db.lookup_account(account_name);
                FC_ASSERT(opt_account.valid(), "Unable to find account '${name}'",
                          ("name", account_name));
                return opt_account->active_key();
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        PublicKeyType Wallet::get_owner_public_key(const string& account_name)const {
            try {
                if (!my->_blockchain->is_valid_account_name(account_name))
                    FC_THROW_EXCEPTION(invalid_name, "Invalid account name!", ("account_name", account_name));
                    
                FC_ASSERT(my->is_unique_account(account_name));
                const auto registered_account = my->_blockchain->get_account_entry(account_name);
                
                if (registered_account.valid()) {
                    if (registered_account->is_retracted())
                        FC_CAPTURE_AND_THROW(account_retracted, (registered_account));
                        
                    return registered_account->owner_key;
                }
                
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto opt_account = my->_wallet_db.lookup_account(account_name);
                FC_ASSERT(opt_account.valid(), "Unable to find account '${name}'",
                          ("name", account_name));
                return opt_account->owner_key;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        
        vector<AccountEntry> Wallet::get_all_approved_accounts(const int8_t approval) {
            vector<AccountEntry> all_accounts;
            std::vector<AccountIdType> delegate_ids = my->_blockchain->get_all_delegates_by_vote();
            
            for (const auto& item : delegate_ids) {
                auto delegate_account = my->_blockchain->get_account_entry(item);
                int8_t account_approval = this->get_account_approval(delegate_account->name);
                
                if (approval == account_approval) {
                    all_accounts.push_back(*delegate_account);
                }
            }
            
            return all_accounts;
        }
        
        void Wallet::clear_account_approval(const string& account_name) {
            try {
                std::vector<AccountIdType> delegate_ids = my->_blockchain->get_all_delegates_by_vote();
                
                for (const auto& item : delegate_ids) {
                    auto delegate_account = my->_blockchain->get_account_entry(item);
                    int8_t approval = this->get_account_approval(delegate_account->name);
                    
                    if (approval > 0)
                        set_account_approval(delegate_account->name, 0);
                }
                
                //         vector<account_id_type> delegate_ids = _chain_db->get_delegates_by_vote(first, count);
                //         for (const auto& item : raw_votes)
                //         {
                //             auto delegate_account = pending_state->get_account_entry(item.first);
                //             int8_t approval = this->get_account_approval(delegate_account->name);
                //             if (approval>0)
                //              set_account_approval(delegate_account->name, 0);
                //         }
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        void Wallet::set_account_approval(const string& account_name, int8_t approval) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT((!account_name.empty()), "approve account name empty");
                const auto account_entry = my->_blockchain->get_account_entry(account_name);
                auto war = my->_wallet_db.lookup_account(account_name);
                
                if (!account_entry.valid() && !war.valid())
                    FC_THROW_EXCEPTION(unknown_account, "Unknown account name!", ("account_name", account_name));
                    
                if (war.valid()) {
                    war->approved = approval;
                    my->_wallet_db.store_account(*war);
                    return;
                }
                
                add_contact_account(account_name, account_entry->owner_key);
                set_account_approval(account_name, approval);
            }
            
            FC_CAPTURE_AND_RETHROW((account_name)(approval))
        }
        
        int8_t Wallet::get_account_approval(const string& account_name)const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto account_entry = my->_blockchain->get_account_entry(account_name);
                auto war = my->_wallet_db.lookup_account(account_name);
                
                if (!account_entry.valid() && !war.valid())
                    FC_THROW_EXCEPTION(unknown_account, "Unknown account name!", ("account_name", account_name));
                    
                if (!war.valid()) return 0;
                
                return war->approved;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        oWalletAccountEntry Wallet::get_account_for_address(Address addr_in_account)const {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto okey = my->_wallet_db.lookup_key(addr_in_account);
                
                if (!okey.valid()) return oWalletAccountEntry();
                
                return my->_wallet_db.lookup_account(okey->account_address);
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        vector<EscrowSummary>   Wallet::get_escrow_balances(const string& account_name) {
            vector<EscrowSummary> result;
            FC_ASSERT(is_open(), "Wallet not open!");
            
            if (!account_name.empty()) get_account(account_name); /* Just to check input */
            
            map<string, vector<BalanceEntry>> balance_entrys;
            const auto pending_state = my->_blockchain->get_pending_state();
            const auto scan_balance = [&](const BalanceEntry& entry) {
                // check to see if it is a withdraw by escrow entry
                if (entry.condition.type == withdraw_escrow_type) {
                    auto escrow_cond = entry.condition.as<withdraw_with_escrow>();
                    // lookup account for each key if known
                    // lookup transaction that created the balance entry in the local wallet
                    // if the sender or receiver is one of our accounts and isn't filtered by account_name
                    //    then add the escrow balance to the output.
                    const auto sender_key_entry = my->_wallet_db.lookup_key(escrow_cond.sender);
                    const auto receiver_key_entry = my->_wallet_db.lookup_key(escrow_cond.receiver);
                    /*
                    if( !((sender_key_entry && sender_key_entry->has_private_key()) ||
                    (receiver_key_entry && receiver_key_entry->has_private_key())) )
                    {
                    ilog( "no private key for sender nor receiver" );
                    return; // no private key for the sender nor receiver
                    }
                    */
                    EscrowSummary sum;
                    sum.balance_id = entry.id();
                    sum.balance = entry.get_spendable_balance(time_point_sec());
                    
                    if (sender_key_entry) {
                        const auto account_address = sender_key_entry->account_address;
                        const auto account_entry = my->_wallet_db.lookup_account(account_address);
                        
                        if (account_entry) {
                            const auto name = account_entry->name;
                            sum.sender_account_name = name;
                            
                        } else {
                            auto registered_account = my->_blockchain->get_account_entry(account_address);
                            
                            if (registered_account)
                                sum.sender_account_name = registered_account->name;
                                
                            else
                                sum.sender_account_name = string(escrow_cond.sender);
                        }
                        
                    } else {
                        sum.sender_account_name = string(escrow_cond.sender);
                    }
                    
                    if (receiver_key_entry) {
                        const auto account_address = receiver_key_entry->account_address;
                        const auto account_entry = my->_wallet_db.lookup_account(account_address);
                        const auto name = account_entry.valid() ? account_entry->name : string(account_address);
                        sum.receiver_account_name = name;
                        
                    } else {
                        sum.receiver_account_name = "UNKNOWN";
                    }
                    
                    auto agent_account = my->_blockchain->get_account_entry(escrow_cond.escrow);
                    
                    if (agent_account) {
                        if (agent_account->is_retracted())
                            FC_CAPTURE_AND_THROW(account_retracted, (agent_account));
                            
                        sum.escrow_agent_account_name = agent_account->name;
                        
                    } else {
                        sum.escrow_agent_account_name = string(escrow_cond.escrow);
                    }
                    
                    sum.agreement_digest = escrow_cond.agreement_digest;
                    
                    if (account_name.size()) {
                        if (sum.sender_account_name == account_name ||
                                sum.receiver_account_name == account_name) {
                            result.emplace_back(sum);
                            
                        } else {
                            wlog("skip ${s}", ("s", sum));
                        }
                        
                    } else result.emplace_back(sum);
                }
            };
            my->_blockchain->scan_balances(scan_balance);
            return result;
        }
        
        void Wallet::scan_balances(const function<void(const BalanceIdType&, const BalanceEntry&)> callback)const {
            for (const auto& item : my->_balance_entrys)
                callback(item.first, item.second);
        }
        
        AccountBalanceEntrySummaryType Wallet::get_spendable_account_balance_entrys(const string& account_name)const {
            try {
                map<string, vector<BalanceEntry>> balances;
                const time_point_sec now = my->_blockchain->get_pending_state()->now();
                const auto scan_balance = [&](const BalanceIdType& id, const BalanceEntry& entry) {
                    if (entry.condition.type != withdraw_signature_type) return;
                    
                    const Asset balance = entry.get_spendable_balance(now);
                    
                    if (balance.amount == 0) return;
                    
                    const optional<Address> owner = entry.owner();
                    
                    if (!owner.valid()) return;
                    
                    const oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(*owner);
                    
                    if (!key_entry.valid() || !key_entry->has_private_key()) return;
                    
                    const oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                    const string name = account_entry.valid() ? account_entry->name : string(key_entry->public_key);
                    
                    if (!account_name.empty() && name != account_name) return;
                    
                    balances[name].push_back(entry);
                };
                scan_balances(scan_balance);
                return balances;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        AccountBalanceSummaryType Wallet::get_spendable_account_balances(const string& account_name)const {
            try {
                map<string, map<AssetIdType, ShareType>> balances;
                const map<string, vector<BalanceEntry>> entrys = get_spendable_account_balance_entrys(account_name);
                const time_point_sec now = my->_blockchain->get_pending_state()->now();
                
                for (const auto& item : entrys) {
                    const string& name = item.first;
                    
                    for (const BalanceEntry& entry : item.second) {
                        const Asset balance = entry.get_spendable_balance(now);
                        balances[name][balance.asset_id] += balance.amount;
                    }
                }
                
                return balances;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        AccountBalanceIdSummaryType Wallet::get_account_balance_ids(const string& account_name)const {
            try {
                map<string, unordered_set<BalanceIdType>> balances;
                const auto scan_balance = [&](const BalanceIdType& id, const BalanceEntry& entry) {
                    const set<Address>& owners = entry.owners();
                    
                    for (const Address& owner : owners) {
                        const oWalletKeyEntry key_entry = my->_wallet_db.lookup_key(owner);
                        
                        if (!key_entry.valid() || !key_entry->has_private_key()) continue;
                        
                        const oWalletAccountEntry account_entry = my->_wallet_db.lookup_account(key_entry->account_address);
                        const string name = account_entry.valid() ? account_entry->name : string(key_entry->public_key);
                        
                        if (!account_name.empty() && name != account_name) continue;
                        
                        balances[name].insert(id);
                    }
                };
                scan_balances(scan_balance);
                return balances;
            }
            
            FC_CAPTURE_AND_RETHROW((account_name))
        }
        
        AccountVoteSummaryType Wallet::get_account_vote_summary(const string& account_name)const {
            try {
                const auto pending_state = my->_blockchain->get_pending_state();
                auto raw_votes = map<AccountIdType, int64_t>();
                auto result = AccountVoteSummaryType();
                const AccountBalanceEntrySummaryType items = get_spendable_account_balance_entrys(account_name);
                
                for (const auto& item : items) {
                    const auto& entrys = item.second;
                    
                    for (const auto& entry : entrys) {
                        const auto owner = entry.owner();
                        
                        if (!owner.valid()) continue;
                        
                        const auto okey_rec = my->_wallet_db.lookup_key(*owner);
                        
                        if (!okey_rec.valid() || !okey_rec->has_private_key()) continue;
                        
                        const auto oaccount_rec = my->_wallet_db.lookup_account(okey_rec->account_address);
                        
                        if (!oaccount_rec.valid()) FC_THROW_EXCEPTION(unknown_account, "Unknown account name!");
                        
                        if (!account_name.empty() && oaccount_rec->name != account_name) continue;
                        
                        const auto obalance = pending_state->get_balance_entry(entry.id());
                        
                        if (!obalance.valid()) continue;
                        
                        const auto balance = obalance->get_spendable_balance(pending_state->now());
                        
                        if (balance.amount <= 0 || balance.asset_id != 0) continue;
                        
                        const auto slate_id = obalance->slate_id();
                        
                        if (slate_id == 0) continue;
                        
                        const auto slate_entry = pending_state->get_slate_entry(slate_id);
                        
                        if (!slate_entry.valid()) FC_THROW_EXCEPTION(unknown_slate, "Unknown slate!");
                        
                        for (const AccountIdType delegate_id : slate_entry->slate) {
                            if (raw_votes.count(delegate_id) <= 0) raw_votes[delegate_id] = balance.amount;
                            
                            else raw_votes[delegate_id] += balance.amount;
                        }
                    }
                }
                
                for (const auto& item : raw_votes) {
                    auto delegate_account = pending_state->get_account_entry(item.first);
                    result[delegate_account->name] = item.second;
                }
                
                return result;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        variant Wallet::get_info()const {
            const time_point_sec now = blockchain::now();
            mutable_variant_object info;
            info["data_dir"] = fc::absolute(my->_data_directory);
            info["num_scanning_threads"] = my->_num_scanner_threads;
            const auto is_open = this->is_open();
            info["open"] = is_open;
            info["name"] = variant();
            info["automatic_backups"] = variant();
            info["transaction_scanning_enabled"] = variant();
            info["last_scanned_block_num"] = variant();
            info["last_scanned_block_timestamp"] = variant();
            info["transaction_fee"] = variant();
            info["transaction_expiration_secs"] = variant();
            info["unlocked"] = variant();
            info["unlocked_until"] = variant();
            info["unlocked_until_timestamp"] = variant();
            info["scan_progress"] = variant();
            info["version"] = variant();
            
            if (is_open) {
                info["name"] = my->_current_wallet_path.filename().string();
                info["automatic_backups"] = get_automatic_backups();
                info["transaction_scanning_enabled"] = get_transaction_scanning();
                const auto last_scanned_block_num = get_last_scanned_block_number();
                
                if (last_scanned_block_num > 0) {
                    info["last_scanned_block_num"] = last_scanned_block_num;
                    
                    try {
                        info["last_scanned_block_timestamp"] = my->_blockchain->get_block_header(last_scanned_block_num).timestamp;
                        
                    } catch (...) {
                    }
                }
                
                info["transaction_fee"] = get_transaction_fee();
                info["transaction_expiration_secs"] = get_transaction_expiration();
                info["unlocked"] = is_unlocked();
                const auto unlocked_until = this->unlocked_until();
                
                if (unlocked_until.valid()) {
                    info["unlocked_until"] = (*unlocked_until - now).to_seconds();
                    info["unlocked_until_timestamp"] = *unlocked_until;
                    info["scan_progress"] = get_scan_progress();
                }
                
                info["version"] = get_version();
            }
            
            return info;
        }
        
        PublicKeySummary Wallet::get_public_key_summary(const PublicKeyType& pubkey) const {
            PublicKeySummary summary;
            summary.hex = variant(fc::ecc::public_key_data(pubkey)).as_string();
            summary.native_pubkey = string(pubkey);
            summary.native_address = (string(Address(pubkey)) + INVALIDE_SUB_ADDRESS);
            summary.pts_normal_address = string(PtsAddress(pubkey, false, 56));
            summary.pts_compressed_address = string(PtsAddress(pubkey, true, 56));
            summary.btc_normal_address = string(PtsAddress(pubkey, false, 0));
            summary.btc_compressed_address = string(PtsAddress(pubkey, true, 0));
            return summary;
        }
        
        vector<PublicKeyType> Wallet::get_public_keys_in_account(const string& account_name)const {
            const auto account_rec = my->_wallet_db.lookup_account(account_name);
            
            if (!account_rec.valid())
                FC_THROW_EXCEPTION(unknown_account, "Unknown account name!");
                
            const auto account_address = Address(get_owner_public_key(account_name));
            vector<PublicKeyType> account_keys;
            const auto keys = my->_wallet_db.get_keys();
            
            for (const auto& key : keys) {
                if (key.second.account_address == account_address || key.first == account_address)
                    account_keys.push_back(key.second.public_key);
            }
            
            return account_keys;
        }
        
        
        
        void Wallet::write_latest_builder(const TransactionBuilder& builder,
                                          const string& alternate_path) {
            std::ofstream fs;
            
            if (alternate_path == "") {
                auto dir = (get_data_directory() / "trx").string();
                auto default_path = dir + "/latest.trx";
                
                if (!fc::exists(default_path))
                    fc::create_directories(dir);
                    
                fs.open(default_path);
                
            } else {
                if (fc::exists(alternate_path))
                    FC_THROW_EXCEPTION(file_already_exists, "That filename already exists!", ("filename", alternate_path));
                    
                fs.open(alternate_path);
            }
            
            fs << fc::json::to_pretty_string(builder);
            fs.close();
        }
        
        /*wallet_transaction_entry Wallet::asset_authorize_key( const string& paying_account_name,  //check????
                                                       const string& symbol,
                                                       const address& key,
                                                       const object_id_type meta, bool sign )
                                                       {
                                                       if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
                                                       if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );
                                                       auto payer_key = get_owner_public_key( paying_account_name );
        
                                                       transaction_builder_ptr builder = create_transaction_builder();
                                                       builder->asset_authorize_key( symbol, key, meta );
                                                       builder->deduct_balance( payer_key, asset() );
                                                       builder->finalize();
        
                                                       if( sign )
                                                       return builder->sign();
                                                       return builder->transaction_entry;
                                                       }*/
        
        void Wallet::initialize_transaction_creator(TransactionCreationState& c, const string& account_name) {
            c.pending_state._balance_id_to_entry = my->_balance_entrys;
            vector<PublicKeyType>  keys = get_public_keys_in_account(account_name);
            
            for (auto key : keys) c.add_known_key(key);
        }
        
        void Wallet::sign_transaction_creator(TransactionCreationState& c) {
        }
        
        vector<WalletContactEntry> Wallet::list_contacts()const {
            try {
                if (!is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (!is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                vector<WalletContactEntry> contacts;
                const auto& entrys = my->_wallet_db.get_contacts();
                contacts.reserve(entrys.size());
                
                for (const auto& item : entrys)
                    contacts.push_back(item.second);
                    
                std::sort(contacts.begin(), contacts.end(),
                          [](const WalletContactEntry& a, const WalletContactEntry& b) -> bool
                { return a.label.compare(b.label) < 0; });
                return contacts;
            }
            
            FC_CAPTURE_AND_RETHROW()
        }
        
        oWalletContactEntry Wallet::get_contact(const variant& data)const {
            try {
                if (!is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (!is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                return my->_wallet_db.lookup_contact(data);
            }
            
            FC_CAPTURE_AND_RETHROW((data))
        }
        
        oWalletContactEntry Wallet::get_contact(const string& label)const {
            try {
                if (!is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (!is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                return my->_wallet_db.lookup_contact(label);
            }
            
            FC_CAPTURE_AND_RETHROW((label))
        }
        
        WalletContactEntry Wallet::add_contact(const ContactData& contact) {
            try {
                if (!is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (!is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                return my->_wallet_db.store_contact(contact);
            }
            
            FC_CAPTURE_AND_RETHROW((contact))
        }
        
        oWalletContactEntry Wallet::remove_contact(const variant& data) {
            try {
                if (!is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (!is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                return my->_wallet_db.remove_contact(data);
            }
            
            FC_CAPTURE_AND_RETHROW((data))
        }
        
        oWalletContactEntry Wallet::remove_contact(const string& label) {
            try {
                if (!is_open()) FC_CAPTURE_AND_THROW(wallet_closed);
                
                if (!is_unlocked()) FC_CAPTURE_AND_THROW(wallet_locked);
                
                return my->_wallet_db.remove_contact(label);
            }
            
            FC_CAPTURE_AND_RETHROW((label))
        }
        
        void Wallet::import_script_db(const fc::path& src_path) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                fc::path db_path = get_data_directory() / get_wallet_name();
                
                if (db_path == src_path)
                    return;
                    
                if (!fc::exists(src_path))
                    FC_CAPTURE_AND_THROW(file_not_found, ("src_path is illegal"));
                    
                if (fc::exists(src_path / "script_id_to_script_entry_db")) {
                    db::fast_level_map <ScriptIdType, ScriptEntry> src_db;
                    src_db.open(src_path / "script_id_to_script_entry_db");
                    auto it = src_db.unordered_begin();
                    auto end = src_db.unordered_end();
                    
                    while (it != end) {
                        my->script_id_to_script_entry_db.store(it->first, it->second);
                        it++;
                    }
                    
                    src_db.close();
                }
                
                if (fc::exists(src_path / "contract_id_event_to_script_id_vector_db")) {
                    db::fast_level_map <ScriptRelationKey, vector<ScriptIdType>> src_db_relation;
                    src_db_relation.open(src_path / "contract_id_event_to_script_id_vector_db");
                    auto it_relation = src_db_relation.unordered_begin();
                    auto end_relation = src_db_relation.unordered_end();
                    
                    while (it_relation != end_relation) {
                        my->contract_id_event_to_script_id_vector_db.store(it_relation->first, it_relation->second);
                        it_relation++;
                    }
                    
                    src_db_relation.close();
                }
            }
            
            FC_CAPTURE_AND_RETHROW((src_path))
        }
        
        void Wallet::export_script_db(const fc::path & des_path) {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(is_unlocked(), "Wallet not unlock!");
                fc::path db_path = get_data_directory() / get_wallet_name();
                
                if (db_path == des_path)
                    return;
                    
                fc::remove_all(des_path / "script_id_to_script_entry_db");
                fc::remove_all(des_path / "contract_id_event_to_script_id_vector_db");
                db::fast_level_map <ScriptIdType, ScriptEntry> des_db;
                des_db.open(des_path / "script_id_to_script_entry_db");
                auto script_db_it = my->script_id_to_script_entry_db.unordered_begin();
                auto script_db_end = my->script_id_to_script_entry_db.unordered_end();
                
                while (script_db_it != script_db_end) {
                    des_db.store(script_db_it->first, script_db_it->second);
                    script_db_it++;
                }
                
                des_db.close();
                db::fast_level_map <ScriptRelationKey, vector<ScriptIdType>> des_db_relation;
                des_db_relation.open(des_path / "contract_id_event_to_script_id_vector_db");
                auto it_relation = my->contract_id_event_to_script_id_vector_db.unordered_begin();
                auto end_relation = my->contract_id_event_to_script_id_vector_db.unordered_end();
                
                while (it_relation != end_relation) {
                    des_db_relation.store(it_relation->first, it_relation->second);
                    it_relation++;
                }
                
                des_db_relation.close();
            }
            
            FC_CAPTURE_AND_RETHROW((des_path))
        }
        
    }
} // thinkyoung::wallet
