#pragma once

#include <cli/Cli.hpp>
#include <client/Notifier.hpp>
#include <db/LevelMap.hpp>
#include <db/Exception.hpp>
#include <net/ChainServer.hpp>
#include <net/Upnp.hpp>

#include <fc/log/appender.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/program_options.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/tee.hpp>

#include <fstream>
#include <iostream>

#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>
#include <blockchain/GluaChainApi.hpp>

namespace thinkyoung {
    namespace client {

        typedef boost::iostreams::tee_device<std::ostream, std::ofstream> TeeDevice;
        typedef boost::iostreams::stream<TeeDevice> TeeStream;

        namespace detail {
            using namespace boost;

            using namespace thinkyoung::wallet;
            using namespace thinkyoung::blockchain;

            // wrap the exception database in a class that logs the exception to the normal logging stream
            // in addition to just storing it
            class LoggingExceptionDb
            {
            public:
                typedef thinkyoung::db::LevelMap<fc::time_point, fc::exception> ExceptionLeveldbType;
            private:
                ExceptionLeveldbType _db;
            public:
                void open(const fc::path& filename)
                {
                    try
                    {
                        _db.open(filename);
                    }
                    catch (const thinkyoung::db::level_map_open_failure&)
                    {
                        fc::remove_all(filename);
                        _db.open(filename);
                    }
                }
                void store(const fc::exception& e)
                {
                    elog("storing error in database: ${e}", ("e", e));
                    _db.store(fc::time_point::now(), e);
                }
                ExceptionLeveldbType::iterator lower_bound(const fc::time_point time) const
                {
                    return _db.lower_bound(time);
                }
                ExceptionLeveldbType::iterator begin() const
                {
                    return _db.begin();
                }
                void remove(const fc::time_point key)
                {
                    _db.remove(key);
                }
            };

            class ClientImpl : public thinkyoung::net::NodeDelegate,
                public thinkyoung::api::CommonApi
            {
            public:
                class UserAppender : public fc::appender
                {
                public:
                    UserAppender(ClientImpl& c) :
                        _client_impl(c),
                        _thread(&fc::thread::current())
                    {}

                    virtual void log(const fc::log_message& m) override
                    {
                        if (!_thread->is_current())
                            return _thread->async([&](){ log(m); }, "user_appender::log").wait();

                        string format = m.get_format();
                        // lookup translation on format here

                        // perform variable substitution;
                        string message = format_string(format, m.get_data());


                        _history.emplace_back(message);
                        if (_client_impl._cli)
                            _client_impl._cli->display_status_message(message);
                        else
                            std::cout << message << "\n";

                        // call a callback to the client...

                        // we need an RPC call to fetch this log and display the
                        // current status.
                    }

                    vector<string> get_history()const
                    {
                        if (!_thread->is_current())
                            return _thread->async([&](){ return get_history(); }, "user_appender::get_history").wait();
                        return _history;
                    }

                    void clear_history()
                    {
                        if (!_thread->is_current())
                            return _thread->async([&](){ return clear_history(); }, "user_appender::clear_history").wait();
                        _history.clear();
                    }


                private:
                    // TODO: consider a deque and enforce maximum length?
                    vector<string>                       _history;
                    ClientImpl&                         _client_impl;
                    fc::thread*                          _thread;
                };

                ClientImpl(thinkyoung::client::Client* self, const std::string& user_agent) :
                    _self(self),
                    _user_agent(user_agent),
                    _last_sync_status_message_indicated_in_sync(true),
                    _last_sync_status_head_block(0),
                    _remaining_items_to_sync(0),
                    _sync_speed_accumulator(boost::accumulators::tag::rolling_window::window_size = 5),
                    _connection_count_notification_interval(fc::minutes(5)),
                    _connection_count_always_notify_threshold(5),
                    _connection_count_last_value_displayed(0),
                    _blockchain_synopsis_size_limit((unsigned)(log2(ALP_BLOCKCHAIN_BLOCKS_PER_YEAR * 20))),
                    _debug_last_wait_block(0)
                {
                    try
                    {
                        if (thinkyoung::lua::api::global_glua_chain_api == nullptr)
                            thinkyoung::lua::api::global_glua_chain_api = new thinkyoung::lua::api::GluaChainApi;

                        _user_appender = fc::shared_ptr<UserAppender>(new UserAppender(*this));
                        fc::logger::get("user").add_appender(_user_appender);
                        try
                        {
                            _rpc_server = std::make_shared<RpcServer>(self);
                        } FC_RETHROW_EXCEPTIONS(warn, "rpc server")
                            try
                        {
                            _chain_db = std::make_shared<ChainDatabase>();
                        } FC_RETHROW_EXCEPTIONS(warn, "chain_db")
                    } FC_RETHROW_EXCEPTIONS(warn, "")
                }

                virtual ~ClientImpl() override
                {
                    cancel_blocks_too_old_monitor_task();
                    cancel_rebroadcast_pending_loop();
                    if (_chain_downloader_future.valid() && !_chain_downloader_future.ready())
                        _chain_downloader_future.cancel_and_wait(__FUNCTION__);
                    _rpc_server.reset(); // this needs to shut down before the _p2p_node because several RPC requests will try to dereference _p2p_node.  Shutting down _rpc_server kills all active/pending requests
                    delete _cli;
                    _cli = nullptr;
                    _p2p_node.reset();
                }

                void start()
                {
                    std::exception_ptr unexpected_exception;
                    try
                    {
                        _cli->start();
                    }
                    catch (...)
                    {
                        unexpected_exception = std::current_exception();
                    }

                    if (unexpected_exception)
                    {
                        if (_notifier)
                            _notifier->notify_client_exiting_unexpectedly();
                        std::rethrow_exception(unexpected_exception);
                    }
                }
                /**
                * If the current thread is not created or did not start then start delegate loop
                *
                * @return void
                */
                void reschedule_delegate_loop();
                void start_delegate_loop();
                void cancel_delegate_loop();
                void delegate_loop();
                void set_target_connections(uint32_t target);
				thinkyoung::blockchain::TransactionIdType network_broadcast_transactions(const std::vector<thinkyoung::blockchain::SignedTransaction>& transactions_to_broadcast);
                void start_rebroadcast_pending_loop();
                void cancel_rebroadcast_pending_loop();
                void rebroadcast_pending_loop();
                fc::future<void> _rebroadcast_pending_loop_done;

                void start_fork_update_time_loop();
                void fork_update_time_loop();
                fc::future<void> _fork_update_loop_done;
                void configure_rpc_server(Config& cfg,
                    const program_options::variables_map& option_variables);
                void configure_chain_server(Config& cfg,
                    const program_options::variables_map& option_variables);

                BlockForkData on_new_block(const FullBlock& block,
                    const BlockIdType& block_id,
                    bool sync_mode);

                bool on_new_transaction(const SignedTransaction& trx);
                void blocks_too_old_monitor_task();
                void cancel_blocks_too_old_monitor_task();
                void set_client_debug_name(const string& name)
                {
                    _config.client_debug_name = name;
                    return;
                }

                /* Implement node_delegate */
                // @{
				virtual bool has_item(const thinkyoung::net::ItemId& id) override;
				fc::time_point_sec handle_ntp_time(bool need_update)override;
                virtual bool handle_message(const thinkyoung::net::Message&, bool sync_mode) override;
                virtual std::vector<thinkyoung::net::ItemHashType> get_item_ids(uint32_t item_type,
                    const vector<thinkyoung::net::ItemHashType>& blockchain_synopsis,
                    uint32_t& remaining_item_count,
                    uint32_t limit = 2000) override;
                virtual thinkyoung::net::Message get_item(const thinkyoung::net::ItemId& id) override;
                virtual fc::sha256 get_chain_id() const override
                {
                    FC_ASSERT(_chain_db != nullptr);
                    return _chain_db->get_chain_id();
                }
                virtual std::vector<thinkyoung::net::ItemHashType> get_blockchain_synopsis(uint32_t item_type,
                    const thinkyoung::net::ItemHashType& reference_point = thinkyoung::net::ItemHashType(),
                    uint32_t number_of_blocks_after_reference_point = 0) override;
                virtual void sync_status(uint32_t item_type, uint32_t item_count) override;
                virtual void connection_count_changed(uint32_t c) override;
                virtual uint32_t get_block_number(const thinkyoung::net::ItemHashType& block_id) override;
                virtual fc::time_point_sec get_block_time(const thinkyoung::net::ItemHashType& block_id) override;
                virtual fc::time_point_sec get_blockchain_now() override;
                virtual thinkyoung::net::ItemHashType get_head_block_id() const override;
                virtual uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const override;
                virtual void error_encountered(const std::string& message, const fc::oexception& error) override;
                /// @}

                thinkyoung::client::Client*                                    _self;
                std::string                                             _user_agent;
                thinkyoung::cli::Cli*                                          _cli = nullptr;

                std::unique_ptr<std::istream>                           _command_script_holder;
                std::ofstream                                           _console_log;
                std::unique_ptr<std::ostream>                           _output_stream;
                std::unique_ptr<TeeDevice>                              _tee_device;
                std::unique_ptr<TeeStream>                              _tee_stream;
                bool                                                    _enable_ulog = false;

                fc::path                                                _data_dir;

                fc::shared_ptr<UserAppender>                           _user_appender;
                bool                                                    _simulate_disconnect = false;
                fc::scoped_connection                                   _time_discontinuity_connection;

                thinkyoung::rpc::RpcServerPtr                                _rpc_server = nullptr;
                std::unique_ptr<thinkyoung::net::ChainServer>                 _chain_server = nullptr;
                std::unique_ptr<thinkyoung::net::UpnpService>                 _upnp_service = nullptr;
                ChainDatabasePtr                                      _chain_db = nullptr;
                unordered_map<TransactionIdType, SignedTransaction>  _pending_trxs;
                WalletPtr                                              _wallet = nullptr;
                //  std::shared_ptr<thinkyoung::mail::server>                      _mail_server = nullptr;
                // std::shared_ptr<thinkyoung::mail::client>                      _mail_client = nullptr;
                fc::time_point                                          _last_sync_status_message_time;
                bool                                                    _last_sync_status_message_indicated_in_sync;
                uint32_t                                                _last_sync_status_head_block;
                uint32_t                                                _remaining_items_to_sync;
                boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::rolling_mean> > _sync_speed_accumulator;

                // Chain downloader
                fc::future<void>                                        _chain_downloader_future;
                bool                                                    _chain_downloader_running = false;
                uint32_t                                                _chain_downloader_blocks_remaining = 0;

                fc::time_point                                          _last_connection_count_message_time;
                /** display messages about the connection count changing at most once every _connection_count_notification_interval */
                fc::microseconds                                        _connection_count_notification_interval;
                /** exception: if you have fewer than _connection_count_always_notify_threshold connections, notify any time the count changes */
                uint32_t                                                _connection_count_always_notify_threshold;
                uint32_t                                                _connection_count_last_value_displayed;

                Config                                                  _config;
                LoggingExceptionDb                                    _exception_db;

                // Delegate block production
                fc::future<void>                                        _delegate_loop_complete;
                bool                                                    _delegate_loop_first_run = true;
                DelegateConfig                                         _delegate_config;

                //start by assuming not syncing, network won't send us a msg if we start synced and stay synched.
                //at worst this means we might briefly sending some pending transactions while not synched.
                bool                                                    _sync_mode = false;

                RpcServerConfig                                       _tmp_rpc_config;

                thinkyoung::net::NodePtr                                      _p2p_node = nullptr;
                bool                                                    _simulated_network = false;

                AlpGntpNotifierPtr                                   _notifier;
                fc::future<void>                                        _blocks_too_old_monitor_done;

                const unsigned                                          _blockchain_synopsis_size_limit;

                fc::future<void>                                        _client_done;

                uint32_t                                                _debug_last_wait_block;
                uint32_t                                                _debug_stop_before_block_num;

                void wallet_http_callback(const string& url, const LedgerEntry& e);
                int save_code_to_file(const string& name, GluaModuleByteStream *stream, char* err_msg) const;
                Address get_contract_address(const string& contract, ChainInterfacePtr db=nullptr) const;


                ChainInterfacePtr sandbox_get_correct_state_ptr() const;
                Address sandbox_get_contract_address(const string& contract) const;
				fc::path compile_contract_helper(const fc::path& filename) const;

                boost::signals2::scoped_connection   _http_callback_signal_connection;

                //-------------------------------------------------- JSON-RPC Method Implementations
                // include all of the method overrides generated by the alp_api_generator
                // this file just contains a bunch of lines that look like:
                // virtual void some_method(const string& some_arg) override;
#include <rpc_stubs/CommonApiOverrides.ipp>//include auto-generated RPC API declarations
            };

            void compile_contract_callback(lua_State *L, std::list<void*>*args_list);

        }
    }
} // namespace thinkyoung::client::detail

