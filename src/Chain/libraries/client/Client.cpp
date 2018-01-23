/*
#include "fc/network/ip.hpp"
#include <iostream>

int main()
{
int a = 0;
fc::ip::address add("192.168.10.109");
bool re = add.is_private_address();
std::cout <<"re="<< re;
std::cin >> a;
}

*/



#define DEFAULT_LOGGER "client"



#include <client/Client.hpp>
#include <client/ClientImpl.hpp>
#include <client/Messages.hpp>

#include <db/LevelMap.hpp>
#include <utilities/GitRevision.hpp>

#include <blockchain/ChainDatabase.hpp>
#include <blockchain/Exceptions.hpp>
#include <blockchain/Time.hpp>
#include <blockchain/TransactionEvaluationState.hpp>

#include <blockchain/Checkpoints.hpp>

#include <net/ChainDownloader.hpp>
#include <net/Exceptions.hpp>

#include <api/CommonApi.hpp>
#include <rpc/RpcClient.hpp>
#include <rpc/RpcServer.hpp>

//#include <thinkyoung/mail/server.hpp>


#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>

#include <fc/filesystem.hpp>
#include <fc/git_revision.hpp>
#include <fc/reflect/variant.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
#include <fc/thread/thread.hpp>

#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/hex.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/algorithm/reverse.hpp>
#include <boost/version.hpp>

#include <openssl/opensslv.h>

#include <algorithm>
#include <iomanip>
#include <set>

#include <blockchain/ForkBlocks.hpp>
#include <cli/locale.hpp>
#include <blockchain/api_extern.hpp>
#include <glua/thinkyoung_lua_api.h>

using namespace boost;
using std::string;


#define FREE_LUA_MODULE(p_lua_module) \
delete (p_lua_module);



#define INVOCATION_COUNTER(name) \
   static unsigned total_ ## name ## _counter = 0; \
   static unsigned active_ ## name ## _counter = 0; \
   struct name ## _invocation_logger { \
   unsigned *total; \
   unsigned *active; \
   name ## _invocation_logger(unsigned *total, unsigned *active) : \
   total(total), active(active) \
{ \
   ++*total; \
   ++*active; \
   fprintf(stderr,"NEWDEBUG: Entering %s, now %d total calls, %d active calls", #name, *total, *active); \
   } \
   ~name ## _invocation_logger() \
{ \
   --*active; \
   fprintf(stderr,"NEWDEBUG: Leaving %s, now %d total calls, %d active calls", #name, *total, *active); \
   } \
   } invocation_logger(&total_ ## name ## _counter, &active_ ## name ## _counter)

namespace thinkyoung {
    namespace client {

        const string ALP_MESSAGE_MAGIC = "Act Signed Message:\n";
        Client* g_client = nullptr;
        bool g_client_quit = false;

        fc::logging_config create_default_logging_config(const fc::path&, bool enable_ulog);
        fc::path get_data_dir(const program_options::variables_map& option_variables);
        Config load_config(const fc::path& datadir, const bool enable_ulog, const fc::optional<bool> statistics_enabled);
        void load_and_configure_chain_database(const fc::path& datadir,
            const program_options::variables_map& option_variables);

        program_options::variables_map parse_option_variables(int argc, char** argv)
        {
            // parse command-line options
            program_options::options_description option_config("Usage");
            option_config.add_options()
                ("help", "Display this help message and exit")
                ("version", "Print version information and exit")

                ("data-dir", program_options::value<FilePath>(), "Set client data directory")
                ("wallet-dir", program_options::value<string>(), "Set Wallet db directory")
                ("genesis-config", program_options::value<string>(),
                "Generate a genesis state with the given JSON file instead of using the built-in "
                "genesis block (only accepted when the blockchain is empty)")

                ("rebuild-index", "Same as --resync-blockchain, except it preserves the raw blockchain data rather "
                "than downloading a new copy")
                ("resync-blockchain", "Delete our copy of the blockchain at startup and download a "
                "fresh copy of the entire blockchain from the network")
                ("statistics-enabled",
                "Index additional blockchain statistics; requires a rebuild or resync if blocks have already been applied")

                ("p2p-port", program_options::value<string>(), "Set network port to listen on (prepend 'r' to enable SO_REUSEADDR)")
                ("accept-incoming-connections", program_options::value<bool>()->default_value(true), "Set to false to reject incoming p2p connections and only establish outbound connections")
                ("upnp", program_options::value<bool>()->default_value(true), "Enable UPNP")

                ("max-connections", program_options::value<uint16_t>(),
                "Set the maximum number of peers this node will accept at any one time")
                ("total-bandwidth-limit", program_options::value<uint32_t>()->default_value(1000000),
                "Limit total bandwidth to this many bytes per second")
                ("min-delegate-connection-count", program_options::value<uint32_t>(),
                "Override the default minimum connection count needed to produce a block")

                ("clear-peer-database", "Erase all information in the peer database")
                ("connect-to", program_options::value<std::vector<string> >(), "Set a remote host to connect to")
                ("disable-default-peers", "Disable automatic connection to default peers")
                ("disable-peer-advertising", "Don't let any peers know which other nodes we're connected to")

                ("server", "Enable JSON-RPC server")
                ("daemon", "Run in daemon mode with no CLI and start JSON-RPC server")

                ("rpcuser", program_options::value<string>(), "Set username for JSON-RPC")
                ("rpcpassword", program_options::value<string>(), "Set password for JSON-RPC")
                ("rpcendpoint", program_options::value<string>(), "Set interface/port to listen for JSON-RPC connections")
                ("rpcport", program_options::value<uint16_t>(), "Set port to listen for JSON-RPC connections")
                ("httpdendpoint", program_options::value<string>(), "Set interface/port to listen for HTTP JSON-RPC connections")
                ("httpport", program_options::value<uint16_t>(), "Set port to listen for HTTP JSON-RPC connections")

                ("chain-server-port", program_options::value<uint16_t>(), "Run a chain server on this port")

                ("input-log", program_options::value< vector<string> >(), "Set log file with CLI commands to execute at startup")
                ("log-commands", "Log all command input and output")
                ("ulog", program_options::value<bool>()->default_value(true), "Enable CLI user logging")

                ("stop-before-block", program_options::value<uint32_t>(), "stop before given block number")

                ("growl", program_options::value<std::string>()->implicit_value("127.0.0.1"), "Send notifications about potential problems to Growl")
                ("growl-password", program_options::value<std::string>(), "Password for authenticating to a Growl server")
                ("growl-identifier", program_options::value<std::string>(), "A name displayed in growl messages to identify this alp_client instance")
                ;

            program_options::variables_map option_variables;
            try
            {
                program_options::store(program_options::command_line_parser(argc, argv).
                    options(option_config).run(), option_variables);
                program_options::notify(option_variables);
            }
            catch (program_options::error& cmdline_error)
            {
                std::cerr << "Error: " << cmdline_error.what() << "\n";
                std::cerr << option_config << "\n";
                exit(1);
            }

            if (option_variables.count("help"))
            {
                std::cout << option_config << "\n";
                exit(0);
            }
            else if (option_variables.count("version"))
            {
                std::cout << fc::json::to_pretty_string(thinkyoung::client::version_info()) << "\n";
                exit(0);
            }

            return option_variables;
        }

        string extract_commands_from_log_stream(std::istream& log_stream)
        {
            string command_list;
            string line;
            while (std::getline(log_stream, line))
            {
                //if line begins with a prompt, add to input buffer
                size_t prompt_position = line.find(CLI_PROMPT_SUFFIX);
                if (prompt_position != string::npos)
                {
                    size_t command_start_position = prompt_position + strlen(CLI_PROMPT_SUFFIX);
                    command_list += line.substr(command_start_position);
                    command_list += "\n";
                }
            }
            return command_list;
        }

        string extract_commands_from_log_file(fc::path test_file)
        {
            if (!fc::exists(test_file))
                FC_THROW(("Unable to input-log-file: \"" + test_file.string() + "\" not found!").c_str());
            else
                ulog("Extracting commands from input log file: ${log}", ("log", test_file.string()));
            boost::filesystem::ifstream test_input(test_file);
            return extract_commands_from_log_stream(test_input);
        }

        fc::logging_config create_default_logging_config(const fc::path& data_dir, bool enable_ulog)
        {
            fc::logging_config cfg;
            fc::path log_dir("logs");

            fc::file_appender::config ac;
            ac.filename = log_dir / "default" / "default.log";
            ac.flush = true;
            ac.rotate = true;
            ac.rotation_interval = fc::hours(1);
            ac.rotation_limit = fc::days(1);
            // ac.rotation_compression = false;

            std::cout << "Logging to file: " << (data_dir / ac.filename).preferred_string() << "\n";

            fc::file_appender::config ac_rpc;
            ac_rpc.filename = log_dir / "rpc" / "rpc.log";
            ac_rpc.flush = true;
            ac_rpc.rotate = true;
            ac_rpc.rotation_interval = fc::hours(1);
            ac_rpc.rotation_limit = fc::days(1);
            // ac_rpc.rotation_compression = false;

            std::cout << "Logging RPC to file: " << (data_dir / ac_rpc.filename).preferred_string() << "\n";

            fc::file_appender::config ac_blockchain;
            ac_blockchain.filename = log_dir / "blockchain" / "blockchain.log";
            ac_blockchain.flush = true;
            ac_blockchain.rotate = true;
            ac_blockchain.rotation_interval = fc::hours(1);
            ac_blockchain.rotation_limit = fc::days(1);
            //  ac_blockchain.rotation_compression = false;

            std::cout << "Logging blockchain to file: " << (data_dir / ac_blockchain.filename).preferred_string() << "\n";

            fc::file_appender::config ac_p2p;
            ac_p2p.filename = log_dir / "p2p" / "p2p.log";
#ifdef NDEBUG
            ac_p2p.flush = false;
#else // NDEBUG
            ac_p2p.flush = true;
#endif // NDEBUG
            ac_p2p.rotate = true;
            ac_p2p.rotation_interval = fc::hours(1);
            ac_p2p.rotation_limit = fc::days(1);
            // ac_p2p.rotation_compression = false;

            std::cout << "Logging P2P to file: " << (data_dir / ac_p2p.filename).preferred_string() << "\n";

            fc::variants  c{
                fc::mutable_variant_object("level", "debug")("color", "green"),
                fc::mutable_variant_object("level", "warn")("color", "brown"),
                fc::mutable_variant_object("level", "error")("color", "red") };

            cfg.appenders.push_back(
                fc::appender_config("stderr", "console",
                fc::mutable_variant_object()
                ("stream", "std_error")
                ("level_colors", c)
                ));

            cfg.appenders.push_back(fc::appender_config("default", "file", fc::variant(ac)));
            cfg.appenders.push_back(fc::appender_config("rpc", "file", fc::variant(ac_rpc)));
            cfg.appenders.push_back(fc::appender_config("blockchain", "file", fc::variant(ac_blockchain)));
            cfg.appenders.push_back(fc::appender_config("p2p", "file", fc::variant(ac_p2p)));

            fc::logger_config dlc;
#ifdef ALP_TEST_NETWORK
            dlc.level = fc::log_level::debug;
#else
            dlc.level = fc::log_level::warn;
#endif
            dlc.name = "default";
            dlc.appenders.push_back("default");
            dlc.appenders.push_back("p2p");
            // dlc.appenders.push_back("stderr");

            fc::logger_config dlc_client;
#ifdef ALP_TEST_NETWORK
            dlc_client.level = fc::log_level::debug;
#else
            dlc_client.level = fc::log_level::warn;
#endif
            dlc_client.name = "client";
            dlc_client.appenders.push_back("default");
            dlc_client.appenders.push_back("p2p");
            // dlc.appenders.push_back("stderr");

            fc::logger_config dlc_rpc;
#ifdef ALP_TEST_NETWORK
            dlc_rpc.level = fc::log_level::debug;
#else
            dlc_rpc.level = fc::log_level::warn;
#endif
            dlc_rpc.name = "rpc";
            dlc_rpc.appenders.push_back("rpc");

            fc::logger_config dlc_blockchain;
#ifdef ALP_TEST_NETWORK
            dlc_blockchain.level = fc::log_level::debug;
#else
            dlc_blockchain.level = fc::log_level::warn;
#endif
            dlc_blockchain.name = "blockchain";
            dlc_blockchain.appenders.push_back("blockchain");

            fc::logger_config dlc_p2p;
#ifdef ALP_TEST_NETWORK
            dlc_p2p.level = fc::log_level::debug;
#else
            dlc_p2p.level = fc::log_level::warn;
#endif
            dlc_p2p.name = "p2p";
            dlc_p2p.appenders.push_back("p2p");

            fc::logger_config dlc_user;
            if (enable_ulog) dlc_user.level = fc::log_level::debug;
            else dlc_user.level = fc::log_level::off;
            dlc_user.name = "user";
            dlc_user.appenders.push_back("user");

            cfg.loggers.push_back(dlc);
            cfg.loggers.push_back(dlc_client);
            cfg.loggers.push_back(dlc_rpc);
            cfg.loggers.push_back(dlc_p2p);
            cfg.loggers.push_back(dlc_user);
            cfg.loggers.push_back(dlc_blockchain);

            return cfg;
        }

        fc::path get_data_dir(const program_options::variables_map& option_variables)
        {
            try {
                fc::path datadir;
                if (option_variables.count("data-dir"))
                {
#ifdef WIN32
                    datadir = fc::path(option_variables["data-dir"].as<string>());
#else 
                    datadir = fc::path(option_variables["data-dir"].as<string>().c_str());
#endif
                }
                else
                {
                    const auto get_os_specific_dir_name = [&](string dir_name) -> string
                    {
#ifdef WIN32
#elif defined( __APPLE__ )
#else
                        std::string::iterator end_pos = std::remove(dir_name.begin(), dir_name.end(), ' ');
                        dir_name.erase(end_pos, dir_name.end());
                        dir_name = "." + dir_name;
#endif

#ifdef ALP_TEST_NETWORK
                        //dir_name += "-Test" + std::to_string(ALP_TEST_NETWORK_VERSION);
                        dir_name += "-Test";
#endif
                        return dir_name;
                    };

                    datadir = fc::app_path() / get_os_specific_dir_name(ALP_BLOCKCHAIN_NAME);
                }
                return datadir;

            } FC_RETHROW_EXCEPTIONS(warn, "error loading config")
        }
        fc::path get_wallet_dir(const program_options::variables_map& option_variables, const fc::path& default_path)
        {
            try {
                fc::path wallet_dir;
                if (option_variables.count("wallet-dir"))
                {
                    wallet_dir = fc::path(option_variables["wallet-dir"].as<string>().c_str());
                }
                else
                {
                    return default_path;
                }
                return wallet_dir;

            } FC_RETHROW_EXCEPTIONS(warn, "error loading config")
        }
        void load_and_configure_chain_database(const fc::path& datadir,
            const program_options::variables_map& option_variables)
        {
            try {
                if (option_variables.count("resync-blockchain"))
                {
                    std::cout << "Deleting old copy of the blockchain in: " << (datadir / "chain").preferred_string() << "\n";
                    try
                    {
                        fc::remove_all(datadir / "chain");
                    }
                    catch (const fc::exception& e)
                    {
                        std::cout << "Error while deleting old copy of the blockchain: " << e.what() << "\n";
                        std::cout << "You may need to manually delete your blockchain and relaunch the client\n";
                    }
                }
                else if (option_variables.count("rebuild-index"))
                {
                    try
                    {
                        fc::remove_all(datadir / "chain/index");
                    }
                    catch (const fc::exception& e)
                    {
                        std::cout << "Error while deleting database index: " << e.what() << "\n";
                    }
                }
                else
                {
                    std::cout << "Loading blockchain from: " << (datadir / "chain").preferred_string() << "\n";
                }
            } FC_RETHROW_EXCEPTIONS(warn, "unable to open blockchain from ${data_dir}", ("data_dir", datadir / "chain"))
        }

        Config load_config(const fc::path& datadir, const bool enable_ulog, const fc::optional<bool> statistics_enabled)
        {
            try {
                fc::path config_file = datadir / "config.json";
                Config cfg;
                if (fc::exists(config_file))
                {
                    std::cout << "Loading config from file: " << config_file.preferred_string() << "\n";
                    const auto default_peers = cfg.default_peers;
                    cfg = fc::json::from_file(config_file).as<Config>();

                    int merged_peer_count = 0;
                    for (const auto& peer : default_peers)
                    {
                        if (std::find(cfg.default_peers.begin(), cfg.default_peers.end(), peer) == cfg.default_peers.end())
                        {
                            ++merged_peer_count;
                            cfg.default_peers.push_back(peer);
                        }
                    }
                    if (merged_peer_count > 0)
                        std::cout << "Merged " << merged_peer_count << " default peers into config.\n";
                }
                else
                {
                    std::cerr << "Creating default config file at: " << config_file.preferred_string() << "\n";
                    cfg.logging = create_default_logging_config(datadir, enable_ulog);
                }

                if (statistics_enabled.valid())
                    cfg.statistics_enabled = *statistics_enabled;

                fc::json::save_to_file(cfg, config_file);

                // the logging_config may contain relative paths.  If it does, expand those to full
                // paths, relative to the data_dir
                for (fc::appender_config& appender : cfg.logging.appenders)
                {
                    if (appender.type == "file")
                    {
                        try
                        {
                            fc::file_appender::config file_appender_config = appender.args.as<fc::file_appender::config>();
                            if (file_appender_config.filename.is_relative())
                            {
                                file_appender_config.filename = fc::absolute(datadir / file_appender_config.filename);
                                appender.args = fc::variant(file_appender_config);
                            }
                        }
                        catch (const fc::exception& e)
                        {
                            wlog("Unexpected exception processing logging config: ${e}", ("e", e));
                        }
                    }
                }

                std::srand(std::time(0));
                std::random_shuffle(cfg.default_peers.begin(), cfg.default_peers.end());
                return cfg;
            } FC_RETHROW_EXCEPTIONS(warn, "unable to load config file ${cfg}", ("cfg", datadir / "config.json"))
        }

        void load_checkpoints(const fc::path& data_dir)
        {
            try {
                const fc::path checkpoint_file = data_dir / "checkpoints.json";

                decltype(thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS) external_checkpoints;
                fc::oexception file_exception;
                if (fc::exists(checkpoint_file))
                {
                    try
                    {
                        external_checkpoints = fc::json::from_file(checkpoint_file).as<decltype(external_checkpoints)>();
                    }
                    catch (const fc::exception& e)
                    {
                        file_exception = e;
                    }
                }

                if (!external_checkpoints.empty())
                {
                    decltype(thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS) external_checkpoints;
                    if (thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS.empty() || external_checkpoints.crbegin()->first >= thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS.crbegin()->first)
                    {
                        ulog("Using blockchain checkpoints from file: ${x}", ("x", checkpoint_file.preferred_string()));
                        thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS = external_checkpoints;
                        return;
                    }
                }

                if (!file_exception.valid())
                {
                    fc::remove_all(checkpoint_file);
                    fc::json::save_to_file(thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS, checkpoint_file);
                }
                else
                {
                    ulog("Error loading blockchain checkpoints from file: ${x}", ("x", checkpoint_file.preferred_string()));
                }

                if (!thinkyoung::blockchain::detail::CHECKPOINT_BLOCKS.empty())
                    ulog("Using built-in blockchain checkpoints");
            } FC_CAPTURE_AND_RETHROW((data_dir))
        }

        namespace detail
        {
            //should this function be moved to rpc server eventually? probably...
            void ClientImpl::configure_rpc_server(Config& cfg,
                const program_options::variables_map& option_variables)
            {
                if (option_variables.count("server") || option_variables.count("daemon") || cfg.rpc.enable)
                {
                    // the user wants us to launch the RPC server.
                    // First, override any config parameters they
                    // thinkyoung::rpc::rpc_server::config rpc_config(cfg.rpc);
                    if (option_variables.count("rpcuser"))
                        cfg.rpc.rpc_user = option_variables["rpcuser"].as<string>();
                    if (option_variables.count("rpcpassword"))
                        cfg.rpc.rpc_password = option_variables["rpcpassword"].as<string>();
                    if (option_variables.count("rpcendpoint"))
                        cfg.rpc.rpc_endpoint = fc::ip::endpoint::from_string(option_variables["rpcendpoint"].as<string>());
                    if (option_variables.count("rpcport"))
                        cfg.rpc.rpc_endpoint.set_port(option_variables["rpcport"].as<uint16_t>());
                    if (option_variables.count("httpdendpoint"))
                        cfg.rpc.httpd_endpoint = fc::ip::endpoint::from_string(option_variables["httpdendpoint"].as<string>());
                    if (option_variables.count("httpport"))
                        cfg.rpc.httpd_endpoint.set_port(option_variables["httpport"].as<uint16_t>());

                    if (cfg.rpc.rpc_user.empty() ||
                        cfg.rpc.rpc_password.empty())
                    {
                        std::cout << "Error starting RPC server\n";
                        std::cout << "You specified " << (option_variables.count("server") ? "--server" : "--daemon") << " on the command line,\n";
                        std::cout << "but did not provide a username or password to authenticate RPC connections.\n";
                        std::cout << "You can provide these by using --rpcuser=username and --rpcpassword=password on the\n";
                        std::cout << "command line, or by setting the \"rpc_user\" and \"rpc_password\" properties in the\n";
                        std::cout << "config file.\n";
                        exit(1);
                    }

                    // launch the RPC servers
                    bool rpc_success = _rpc_server->configure_rpc(cfg.rpc);
                    rpc_success &= _rpc_server->configure_http(cfg.rpc);
                    if (!cfg.rpc.encrypted_rpc_wif_key.empty())
                        rpc_success &= _rpc_server->configure_encrypted_rpc(cfg.rpc);

                    // this shouldn't fail due to the above checks, but just to be safe...
                    if (!rpc_success)
                        std::cerr << "Error starting rpc server\n\n";

                    fc::optional<fc::ip::endpoint> actual_rpc_endpoint = _rpc_server->get_rpc_endpoint();
                    if (actual_rpc_endpoint)
                    {
                        std::cout << "Starting JSON RPC server on port " << actual_rpc_endpoint->port();
                        if (actual_rpc_endpoint->get_address() == fc::ip::address("127.0.0.1"))
                            std::cout << " (localhost only)";
                        std::cout << "\n";
                    }

                    fc::optional<fc::ip::endpoint> actual_httpd_endpoint = _rpc_server->get_httpd_endpoint();
                    if (actual_httpd_endpoint)
                    {
                        std::cout << "Starting HTTP JSON RPC server on port " << actual_httpd_endpoint->port();
                        if (actual_httpd_endpoint->get_address() == fc::ip::address("127.0.0.1"))
                            std::cout << " (localhost only)";
                        std::cout << "\n";
                    }
                }
                else
                {
                    std::cout << "Not starting RPC server, use --server to enable the RPC interface\n";
                }
            }

            void ClientImpl::configure_chain_server(Config& cfg, const program_options::variables_map& option_variables)
            {
                if (option_variables.count("chain-server-port"))
                {
                    cfg.chain_server.listen_port = option_variables["chain-server-port"].as<uint16_t>();
                    cfg.chain_server.enabled = true;
                }
            }

            // Call this whenever a change occurs that may enable block production by the client
            void ClientImpl::reschedule_delegate_loop()
            {
                if (!_delegate_loop_complete.valid() || _delegate_loop_complete.ready())
                    start_delegate_loop();
            }

            void ClientImpl::start_delegate_loop()
            {
                const fc::path config_file = _data_dir / "delegate_config.json";

                fc::oexception file_exception;
                if (fc::exists(config_file))
                {
                    try
                    {
                        _delegate_config = fc::json::from_file(config_file).as<decltype(_delegate_config)>();
                    }
                    catch (const fc::exception& e)
                    {
                        file_exception = e;
                    }
                }

                if (file_exception.valid())
                    ulog("Error loading delegate config from file: ${x}", ("x", config_file.preferred_string()));

                if (!_time_discontinuity_connection.connected())
                    _time_discontinuity_connection = thinkyoung::blockchain::time_discontinuity_signal.connect([=](){ reschedule_delegate_loop(); });
                _delegate_loop_complete = fc::async([=](){ delegate_loop(); }, "delegate_loop");
            }

            void ClientImpl::cancel_delegate_loop()
            {
                try
                {
                    ilog("Canceling delegate loop...");
                    _delegate_loop_complete.cancel_and_wait(__FUNCTION__);
                    ilog("Delegate loop canceled");
                }
                catch (const fc::exception& e)
                {
                    wlog("Unexpected exception thrown from delegate_loop(): ${e}", ("e", e.to_detail_string()));
                }

                try
                {
                    const fc::path config_file = _data_dir / "delegate_config.json";
                    if (fc::exists(config_file))
                    {
                        fc::remove_all(config_file);
                        fc::json::save_to_file(_delegate_config, config_file);
                    }
                }
                catch (...)
                {
                }
            }
            bool ClientImpl::wallet_account_delete(const std::string& account_name)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                return _wallet->delete_account(account_name);
            }
            void ClientImpl::delegate_loop()
            {
                if (!_wallet->is_open() || _wallet->is_locked())
                    return;

                vector<WalletAccountEntry> enabled_delegates = _wallet->get_my_delegates(enabled_delegate_status);
                if (enabled_delegates.empty())
                    return;

                const auto now = blockchain::now();
                ilog("Starting delegate loop at time: ${t}", ("t", now));

                _chain_db->_verify_transaction_signatures = true;
                if (_delegate_loop_first_run)
                {
                    set_target_connections(ALP_NET_DELEGATE_DESIRED_CONNECTIONS);
                    _delegate_loop_first_run = false;
                }
				_chain_db->generating_block = false;
                const auto next_block_time = _wallet->get_next_producible_block_timestamp(enabled_delegates);
                if (next_block_time.valid())
                {
                    _wallet->_generating_block = true;
                    _chain_db->generating_block = true;
                    ilog("Producing block at time: ${t}", ("t", *next_block_time));

                    if (*next_block_time <= now)
                    {
                        try
                        {
                            _delegate_config.validate();

                            FC_ASSERT(network_get_connection_count() >= _delegate_config.network_min_connection_count,
                                "Client must have ${count} connections before you may produce blocks!",
                                ("count", _delegate_config.network_min_connection_count));
                            FC_ASSERT(_wallet->is_unlocked(), "Wallet must be unlocked to produce blocks!");
                            FC_ASSERT((now - *next_block_time) < fc::seconds(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC),
                                "You missed your slot at time: ${t}!", ("t", *next_block_time));

                            FullBlock next_block = _chain_db->generate_block(*next_block_time, _delegate_config);
                            _wallet->sign_block(next_block);

                            on_new_block(next_block, next_block.id(), false);
                            _p2p_node->broadcast(BlockMessage(next_block));

                            ilog("Produced block #${n}!", ("n", next_block.block_num));
                        }
                        catch (const fc::canceled_exception&)
                        {
                            throw;
                        }
                        catch (const fc::exception& e)
                        {
                            _exception_db.store(e);
                        }
                    }
                }

                uint32_t slot_number = blockchain::get_slot_number(now);
                time_point_sec next_slot_time = blockchain::get_slot_start_time(slot_number + 1);
                ilog("Rescheduling delegate loop for time: ${t}", ("t", next_slot_time));

                time_point scheduled_time = next_slot_time;
                if (blockchain::ntp_time().valid())
                    scheduled_time -= blockchain::ntp_error();

                /* Don't reschedule immediately in case we are in simulation */
                const auto system_now = time_point::now();
                if (scheduled_time <= system_now)
                    scheduled_time = system_now + fc::seconds(1);

                if (!_delegate_loop_complete.canceled())
                    _delegate_loop_complete = fc::schedule([=](){ delegate_loop(); }, scheduled_time, "delegate_loop");
            }

            void ClientImpl::set_target_connections(uint32_t target)
            {
                auto params = fc::mutable_variant_object();
                params["desired_number_of_connections"] = target;
                network_set_advanced_node_parameters(params);
            }

            void ClientImpl::start_rebroadcast_pending_loop()
            {
                if (!_rebroadcast_pending_loop_done.valid() || _rebroadcast_pending_loop_done.ready())
                    _rebroadcast_pending_loop_done = fc::schedule([=](){ rebroadcast_pending_loop(); },
                    fc::time_point::now() + fc::seconds((int64_t)(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 1.3)),
                    "rebroadcast_pending");
            }
            void ClientImpl::start_fork_update_time_loop()
            {
                if (!_fork_update_loop_done.valid() || _fork_update_loop_done.ready())
                    _fork_update_loop_done = fc::schedule([=](){ fork_update_time_loop(); },
                    fc::time_point::now() + fc::seconds((int64_t)(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 6)),
                    "rebroadcast_pending");
            }
            void ClientImpl::fork_update_time_loop()
            {
                uint32_t fork_num = _chain_db->get_fork_list_num();
                uint32_t fork_pre_num = _chain_db->get_forkdb_num();
                if (fork_num != fork_pre_num)
                {
                    blockchain::update_ntp_time();
                    _chain_db->set_forkdb_num(fork_num);
                }
            }
            thinkyoung::blockchain::PublicKeyType ClientImpl::wallet_get_account_owner_publickey(const std::string& account_name)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                return _wallet->get_owner_public_key(account_name);
            }
            void ClientImpl::cancel_rebroadcast_pending_loop()
            {
                try
                {
                    _rebroadcast_pending_loop_done.cancel_and_wait(__FUNCTION__);
                }
                catch (const fc::exception& e)
                {
                    wlog("Unexpected error from rebroadcast_pending(): ${e}", ("e", e));
                }
            }

            void ClientImpl::rebroadcast_pending_loop()
            {
#ifndef NDEBUG
                static bool currently_running = false;
                struct checker {
                    bool& var;
                    //Log instead of crashing. Failing this test is normal behavior in the tests.
                    checker(bool& var) : var(var) { if (var) elog("Checker failure!"); var = true; }
                    ~checker() { var = false; }
                } _checker(currently_running);
#endif // !NDEBUG
                if (_sync_mode)
                {
                    wlog("skip rebroadcast_pending while syncing");
                }
                else
                {
                    try
                    {
                        //auto pending = blockchain_list_pending_transactions();
                        auto pending = blockchain_list_pending_transactions();
                        wlog("rebroadcasting ${trx_count}", ("trx_count", pending.size()));
                        //for (auto trx : pending)
                        //{
                        //    network_broadcast_transaction(trx.second);
                        //}
                        auto iter = pending.begin();
                        std::vector<thinkyoung::blockchain::SignedTransaction> temp_vec;
                        
                        while (iter != pending.end())
                        {
                            if (temp_vec.size() < 1000) {
                                temp_vec.push_back(iter->second);
                            }
                            else {
                                network_broadcast_transactions(temp_vec);
                                temp_vec.clear();
                            }
                            ++iter;
                        }
                        if (!temp_vec.empty()) {
                            network_broadcast_transactions(temp_vec);
                            temp_vec.clear();
                        }
                    }
                    catch (const fc::canceled_exception&)
                    {
                        throw;
                    }
                    catch (const fc::exception& e)
                    {
                        wlog("error rebroadcasting transacation: ${e}", ("e", e.to_detail_string()));
                    }
                }
                if (!_rebroadcast_pending_loop_done.canceled())
                    _rebroadcast_pending_loop_done = fc::schedule([=](){ rebroadcast_pending_loop(); },
                    fc::time_point::now() + fc::seconds((int64_t)(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 1.3)),
                    "rebroadcast_pending");
            }

            ///////////////////////////////////////////////////////
            // Implement chain_client_delegate                   //
            ///////////////////////////////////////////////////////
            BlockForkData ClientImpl::on_new_block(const FullBlock& block,
                const BlockIdType& block_id,
                bool sync_mode)
            {
                try
                {
                    // delay until we want to accept the block
                    while ((this->_debug_stop_before_block_num >= 1) &&
                        (block.block_num >= this->_debug_stop_before_block_num))
                    {
                        fc::usleep(fc::microseconds(1000000));
                    }

                    _sync_mode = sync_mode;
                    if (sync_mode && _remaining_items_to_sync > 0)
                        --_remaining_items_to_sync;
                    try
                    {
                        FC_ASSERT(!_simulate_disconnect);
                        ilog("Received a new block from the p2p network, current head block is ${num}, "
                            "new block is ${block}, current head block is ${num}",
                            ("num", _chain_db->get_head_block_num())("block", block)("num", _chain_db->get_head_block_num()));
                        fc::optional<BlockForkData> fork_data = _chain_db->get_block_fork_data(block_id);

                        if (fork_data && fork_data->is_known)
                        {
                            if (sync_mode && !fork_data->is_linked)
                                FC_THROW_EXCEPTION(thinkyoung::blockchain::unlinkable_block,
                                "The blockchain already has this block, but it isn't linked");
                            ilog("The block we just received is one I've already seen, ignoring it");
                            return *fork_data;
                        }
                        else
                        {
                            BlockForkData result = _chain_db->push_block(block);
                            if (sync_mode && !result.is_linked)
                                FC_THROW_EXCEPTION(thinkyoung::blockchain::unlinkable_block, "The blockchain accepted this block, but it isn't linked");
                            ilog("After push_block, current head block is ${num}", ("num", _chain_db->get_head_block_num()));

                            fc::time_point_sec now = blockchain::now();
                            fc::time_point_sec head_block_timestamp = _chain_db->now();
                            if (_cli
                                && result.is_included
                                && (now - head_block_timestamp) > fc::minutes(5)
                                && _last_sync_status_message_time < (now - fc::seconds(30)))
                            {
                                std::ostringstream message;
                                message << "--- syncing with p2p network, our last block is "
                                    << fc::get_approximate_relative_time_string(head_block_timestamp, now, " old");
                                ulog(message.str());
                                uint32_t current_head_block_num = _chain_db->get_head_block_num();
                                if (_last_sync_status_message_time >(now - fc::seconds(60)) &&
                                    _last_sync_status_head_block != 0 &&
                                    current_head_block_num > _last_sync_status_head_block)
                                {
                                    uint32_t seconds_since_last_status_message = (uint32_t)((fc::time_point(now) - _last_sync_status_message_time).count() / fc::seconds(1).count());
                                    uint32_t blocks_since_last_status_message = current_head_block_num - _last_sync_status_head_block;
                                    double current_sync_speed_in_blocks_per_sec = (double)blocks_since_last_status_message / seconds_since_last_status_message;
                                    _sync_speed_accumulator(current_sync_speed_in_blocks_per_sec);
                                    double average_sync_speed = boost::accumulators::rolling_mean(_sync_speed_accumulator);
                                    std::ostringstream speed_message;
                                    if (average_sync_speed > 0)
                                    {
                                        double remaining_seconds_to_sync = _remaining_items_to_sync / average_sync_speed;

                                        speed_message << "--- currently syncing at ";
                                        if (average_sync_speed >= 10.)
                                            speed_message << (int)average_sync_speed << " blocks/sec, ";
                                        else if (average_sync_speed >= 0.1)
                                            speed_message << std::setprecision(2) << average_sync_speed << " blocks/sec, ";
                                        else
                                            speed_message << (int)(1. / average_sync_speed) << " sec/block, ";
                                        speed_message << fc::get_approximate_relative_time_string(fc::time_point::now(), fc::time_point::now() + fc::seconds((int64_t)remaining_seconds_to_sync), "") << " remaining";
                                    }
                                    else
                                        speed_message << "--- currently syncing at an imperceptible rate";
                                    ulog(speed_message.str());
                                }
                                _last_sync_status_message_time = now;
                                _last_sync_status_head_block = current_head_block_num;
                                _last_sync_status_message_indicated_in_sync = false;
                            }

                            return result;
                        }
                    } FC_RETHROW_EXCEPTIONS(warn, "Error pushing block ${block_number} - ${block_id}",
                        ("block_id", block.id())
                        ("block_number", block.block_num)
                        ("block", block));
                }
                catch (const fc::exception& e)
                {
                    _exception_db.store(e);
                    throw;
                }
            }

            bool ClientImpl::on_new_transaction(const SignedTransaction& trx)
            {
                try {
                    // throws exception if invalid trx, don't override limits
                    return !!_chain_db->store_pending_transaction(trx, false);
                }
                catch (const duplicate_transaction&)
                {
                    throw;
                }
                catch (const fc::exception& e)
                {
                    _exception_db.store(e);
                    throw;
                }
            }

            ///////////////////////////////////////////////////////
            // Implement node_delegate                           //
            ///////////////////////////////////////////////////////

            //client supply this method for node , item could be block or trx
            bool ClientImpl::has_item(const thinkyoung::net::ItemId& id)
            {
                if (id.item_type == block_message_type)
                {
                    return _chain_db->is_known_block(id.item_hash);
                }

                if (id.item_type == trx_message_type)
                {
                    //return _chain_db->is_known_transaction( id.item_hash );
                    // TODO: the performance of get_transaction is much slower than is_known_transaction,
                    // but we do not have enough information to call is_known_transaction because it depends
                    // upon the transaction digest + expiration date and we only have the trx id.
                    return _chain_db->get_transaction(id.item_hash).valid();
                }
                return false;
            }
			fc::time_point_sec ClientImpl::handle_ntp_time(bool need_update)
			{
				if (need_update)
				{
					blockchain::update_ntp_time();
				}
				return blockchain::now();
			}
            /** called when a peer system sends us an new unseen item
            *
            * If this message contains a block:
            * - we check our list of signatures_without_a_block to see if it is signed .
            *   - If it is signed:
            *       verify that it is a potential next block.  If it is, push to blockchain, if not, throw
            *   - if it's not signed:
            *       verify that it is a potential next block.  if it is, add it to our list of unapproved_blocks.  If not , throw
            * If this is a transaction:
            * - verify that it's a valid transaction.  throw if not.
            *   add it to uncommitted_transactions
            *   if we're mining, we'll do some transaction validation and add it to our merkle tree
            * - if it's a signature.  check the signature, throw if invalid
            *   - if it signs one of our unapproved_blocks.
            *     then push that block to the blockchain and clear any other unapproved_blocks
            *     else add to signatures_without_a_block
            *
            * @param message the message from a peer
            * @note this should throw if the message is invalid, will cause the originating peer to be disconnected
            */
            bool ClientImpl::handle_message(const thinkyoung::net::Message& message_to_handle, bool sync_mode)
            {
                if (g_client_quit)
                    return true;
                
                try
                {
                    switch (message_to_handle.msg_type)
                    {
                    case block_message_type:
                    {
                        BlockMessage block_message_to_handle(message_to_handle.as<BlockMessage>());
                        if (block_message_to_handle.block.block_size() > ALP_BLOCKCHAIN_MAX_BLOCK_SIZE)
                        {
                            wlog("block message ${hash} size ${size}, block size ${blocksize} is out of the max block size range",
                                ("hash", message_to_handle.id())
                                ("size", message_to_handle.size)
                                ("blocksize", block_message_to_handle.block.block_size()));
                            return false;
                        }

                        ilog("CLIENT: just received block ${id}", ("id", block_message_to_handle.block.id()));
                        thinkyoung::blockchain::BlockIdType old_head_block = _chain_db->get_head_block_id();
                        BlockForkData fork_data = on_new_block(block_message_to_handle.block, block_message_to_handle.block_id, sync_mode);
                        return fork_data.is_included ^ (block_message_to_handle.block.previous == old_head_block);  // TODO is this right?
                    }
                    case trx_message_type:
                    {
                        TrxMessage trx_message_to_handle(message_to_handle.as<TrxMessage>());
                        if (trx_message_to_handle.trx.data_size() > ALP_BLOCKCHAIN_MAX_TRX_SIZE)
                        {
                            wlog("block message ${hash} size ${size}, trx size $(trxsize) is out of the max transaction size range",
                                ("hash", message_to_handle.id())
                                ("size", message_to_handle.size)
                                ("trxsize", trx_message_to_handle.trx.data_size()));
                            return false;
                        }

                        ilog("CLIENT: just received transaction ${id}", ("id", trx_message_to_handle.trx.id()));
                        return on_new_transaction(trx_message_to_handle.trx);
                    }
                    case batch_trx_message_type:
                    {
                        BatchTrxMessage trx_message_to_handle(message_to_handle.as<BatchTrxMessage>());
                        //ilog("CLIENT: just received transaction ${id}", ("id", trx_message_to_handle.trx.id()));
                        bool result;
                        for (auto trx : trx_message_to_handle.trx_vec)
                        {
                            result &= on_new_transaction(trx);
                        }
                        return result;
                    }
                    }
                    return false;
                }
                catch (const thinkyoung::blockchain::insufficient_relay_fee& original_exception)
                {
                    FC_THROW_EXCEPTION(thinkyoung::net::insufficient_relay_fee, "Insufficient relay fee; do not propagate!",
                        ("original_exception", original_exception.to_detail_string()));
                }
                catch (const thinkyoung::blockchain::block_older_than_undo_history& original_exception)
                {
                    FC_THROW_EXCEPTION(thinkyoung::net::block_older_than_undo_history, "Block is older than undo history, stop fetching blocks!",
                        ("original_exception", original_exception.to_detail_string()));
                }
            }

            /**
            *  Get the hash of all blocks after from_id
            */
            std::vector<thinkyoung::net::ItemHashType> ClientImpl::get_item_ids(uint32_t item_type,
                const std::vector<thinkyoung::net::ItemHashType>& blockchain_synopsis,
                uint32_t& remaining_item_count,
                uint32_t limit /* = 2000 */)
            {
                // limit = 20; // for testing
                FC_ASSERT(item_type == thinkyoung::client::block_message_type);

                // assume anything longer than our limit is an attacker (limit is currently ~26 items)
                if (blockchain_synopsis.size() > _blockchain_synopsis_size_limit)
                    FC_THROW("Peer provided unreasonably long blockchain synopsis during sync (actual length: ${size}, limit: ${blockchain_synopsis_size_limit})",
                    ("size", blockchain_synopsis.size())
                    ("blockchain_synopsis_size_limit", _blockchain_synopsis_size_limit));

                uint32_t last_seen_block_num = 1;
                thinkyoung::net::ItemHashType last_seen_block_hash;
                for (const thinkyoung::net::ItemHashType& item_hash : boost::adaptors::reverse(blockchain_synopsis))
                {
                    try
                    {
                        uint32_t block_num = _chain_db->get_block_num(item_hash);
                        if (_chain_db->is_included_block(item_hash))
                        {
                            last_seen_block_num = block_num;
                            last_seen_block_hash = item_hash;
                            break;
                        }
                    }
                    catch (fc::key_not_found_exception&)
                    {
                    }
                }

                std::vector<thinkyoung::net::ItemHashType> hashes_to_return;
                uint32_t head_block_num = _chain_db->get_head_block_num();
                if (head_block_num == 0)
                {
                    remaining_item_count = 0;
                    return hashes_to_return; // we have no blocks
                }

                if (last_seen_block_num > head_block_num)
                {
                    // We were getting this condition during testing when one of the blocks is invalid because
                    // its timestamp was in the future.  It was accepted in to the database, but never linked to
                    // the chain.  We've fixed the test and it doesn't seem likely that this would happen in a
                    // production environment.

                    //wlog("last_seen_block_num(${last_seen}) > head_block_num(${head})", ("last_seen", last_seen_block_num)("head", head_block_num));
                    //wlog("last_seen_block(${last_seen}) > head_block(${head})", ("last_seen", last_seen_block_hash)("head", _chain_db->get_head_block_id()));
                    //int num = rand() % 100;
                    //fc::path dot_filename(std::string("E:\\fork") + boost::lexical_cast<std::string>(num) + ".dot");
                    //_chain_db->export_fork_graph(dot_filename);
                    //wlog("Graph written to file ${dot_filename}", ("dot_filename", dot_filename));

                    assert(false);
                    // and work around it
                    last_seen_block_num = head_block_num;
                }

                remaining_item_count = head_block_num - last_seen_block_num + 1;
                uint32_t items_to_get_this_iteration = std::min(limit, remaining_item_count);
                hashes_to_return.reserve(items_to_get_this_iteration);
                for (uint32_t i = 0; i < items_to_get_this_iteration; ++i)
                {
                    BlockIdType block_id;
                    bool block_id_not_found = false;
                    try
                    {
                        block_id = _chain_db->get_block_id(last_seen_block_num);
                        //assert(_chain_db->get_block(last_seen_block_num).id() == block_id);  // expensive assert, remove once we're sure
                    }
                    catch (const fc::key_not_found_exception&)
                    {
                        block_id_not_found = true;
                    }

                    if (block_id_not_found)
                    {
                        ilog("chain_database::get_block_id failed to return the id for block number ${last_seen_block_num} even though chain_database::get_block_num() provided its block number",
                            ("last_seen_block_num", last_seen_block_num));
                        ulog("Error: your chain database is in an inconsistent state.  Please shut down and relaunch using --rebuild-index or --resync-blockchain to repair the database");
                        assert(!"I assume this can never happen");
                        // our database doesn't make sense, so just act as if we have no blocks so the remote node doesn't try to sync with us
                        remaining_item_count = 0;
                        hashes_to_return.clear();
                        return hashes_to_return;
                    }
                    hashes_to_return.push_back(block_id);
                    ++last_seen_block_num;
                }
                remaining_item_count -= items_to_get_this_iteration;
                return hashes_to_return;
            }

            std::vector<thinkyoung::net::ItemHashType> ClientImpl::get_blockchain_synopsis(uint32_t item_type,
                const thinkyoung::net::ItemHashType& reference_point /* = thinkyoung::net::item_hash_t() */,
                uint32_t number_of_blocks_after_reference_point /* = 0 */)
            {
                FC_ASSERT(item_type == thinkyoung::client::block_message_type);
                std::vector<thinkyoung::net::ItemHashType> synopsis;
                uint32_t high_block_num = 0;
                uint32_t non_fork_high_block_num = 0;
                std::vector<BlockIdType> fork_history;

                if (reference_point != thinkyoung::net::ItemHashType())
                {
                    // the node is asking for a summary of the block chain up to a specified
                    // block, which may or may not be on a fork
                    // for now, assume it's not on a fork
                    try
                    {
                        if (_chain_db->is_included_block(reference_point))
                        {
                            // block is a block we know about and is on the main chain
                            uint32_t reference_point_block_num = _chain_db->get_block_num(reference_point);
                            assert(reference_point_block_num > 0);
                            high_block_num = reference_point_block_num;
                            non_fork_high_block_num = high_block_num;
                        }
                        else
                        {
                            // block is a block we know about, but it is on a fork
                            try
                            {
                                fork_history = _chain_db->get_fork_history(reference_point);
                                assert(fork_history.size() >= 2);
                                assert(fork_history.front() == reference_point);
                                BlockIdType last_non_fork_block = fork_history.back();
                                fork_history.pop_back();
                                boost::reverse(fork_history);
                                try
                                {
                                    if (last_non_fork_block == BlockIdType())
                                        non_fork_high_block_num = 0;
                                    else
                                        non_fork_high_block_num = _chain_db->get_block_num(last_non_fork_block);
                                }
                                catch (const fc::key_not_found_exception&)
                                {
                                    assert(!"get_fork_history() returned a history that doesn't link to the main chain");
                                }
                                high_block_num = non_fork_high_block_num + fork_history.size();
                                assert(high_block_num == _chain_db->get_block_header(fork_history.back()).block_num);
                            }
                            catch (const fc::exception& e)
                            {
                                // unable to get fork history for some reason.  maybe not linked?
                                // we can't return a synopsis of its chain
                                elog("Unable to construct a blockchain synopsis for reference hash ${hash}: ${exception}", ("hash", reference_point)("exception", e));
                                throw; //FC_RETHROW_EXCEPTIONS( e ); //throw;
                                //return synopsis;
                            }
                        }
                    }
                    catch (const fc::key_not_found_exception&)
                    {
                        assert(false); // the logic in the p2p networking code shouldn't call this with a reference_point that we've never seen
                        // we've never seen this block
                        return synopsis;
                    }
                }
                else
                {
                    // no reference point specified, summarize the whole block chain
                    high_block_num = _chain_db->get_head_block_num();
                    non_fork_high_block_num = high_block_num;
                    if (high_block_num == 0)
                        return synopsis; // we have no blocks
                }

                uint32_t true_high_block_num = high_block_num + number_of_blocks_after_reference_point;
                uint32_t low_block_num = 1;
                do
                {
                    // for each block in the synopsis, figure out where to pull the block id from.
                    // if it's <= non_fork_high_block_num, we grab it from the main blockchain;
                    // if it's not, we pull it from the fork history
                    if (low_block_num <= non_fork_high_block_num)
                        synopsis.push_back(_chain_db->get_block(low_block_num).id());
                    else
                        synopsis.push_back(fork_history[low_block_num - non_fork_high_block_num - 1]);
                    low_block_num += ((true_high_block_num - low_block_num + 2) / 2);
                } while (low_block_num <= high_block_num);

                return synopsis;
            }


            //client supply item for on_fetch_items_message from other peer.  item could be block or trx
            thinkyoung::net::Message ClientImpl::get_item(const thinkyoung::net::ItemId& id)
            {
                if (id.item_type == block_message_type)
                {
                    //   uint32_t block_number = _chain_db->get_block_num(id.item_hash);
                    thinkyoung::client::BlockMessage block_message_to_send(_chain_db->get_block(id.item_hash));
                    FC_ASSERT(id.item_hash == block_message_to_send.block_id); //.id());
                    //   block_message_to_send.signature = block_message_to_send.block.delegate_signature;
                    return block_message_to_send;
                }

                if (id.item_type == trx_message_type)
                {
                    TrxMessage trx_message_to_send;
                    auto iter = _pending_trxs.find(id.item_hash);
                    if (iter != _pending_trxs.end())
                        trx_message_to_send.trx = iter->second;
                }

                FC_THROW_EXCEPTION(fc::key_not_found_exception, "I don't have the item you're looking for");
            }
            fc::path ClientImpl::compile_script(const fc::path& filename) const
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                try{
                    if (!fc::exists(filename))
                        FC_THROW_EXCEPTION(fc::file_not_found_exception, "Script file not found!");

                    char err_msg[LUA_EXCEPTION_MULTILINE_STRNG_MAX_LENGTH] = "\0";
                    string filename_str = filename.string();
                    string out_filename;

                    size_t pos;
                    pos = filename_str.find_last_of('.');
                    if ((pos != string::npos) && (filename_str.substr(pos) == ".lua"|| filename_str.substr(pos) == ".glua"))
                    {
                        out_filename = filename_str.substr(0, pos) + ".script";
                    }
                    else
                    {
                        FC_THROW_EXCEPTION(thinkyoung::blockchain::invalid_script_source_filename, "script source file name should end with .lua or .glua");
                    }
					auto p_lua_module = std::make_shared<GluaModuleByteStream>();
                    FC_ASSERT(p_lua_module, "Alloc memory for GluaModuleByteStream failed!");
                    lua::lib::GluaStateScope sco(false);
                    if (NOT lua::lib::compilefile_to_stream(sco.L(), filename.generic_string().c_str(), p_lua_module.get(), err_msg, USE_TYPE_CHECK))
                    {
                        FC_THROW_EXCEPTION(compile_script_fail, err_msg);
                    }
                    if (save_code_to_file(out_filename, p_lua_module.get(), err_msg) < 0)
                    {
                        FC_THROW_EXCEPTION(thinkyoung::blockchain::save_bytecode_to_scriptfile_fail, err_msg);
                    }
                    return fc::path(out_filename);
                }FC_CAPTURE_AND_RETHROW((filename))
            }
            void ClientImpl::sync_status(uint32_t item_type, uint32_t item_count)
            {
                const bool in_sync = item_count == 0;
                _remaining_items_to_sync = item_count;

                fc::time_point now = fc::time_point::now();
                if (_cli)
                {
                    if (in_sync && !_last_sync_status_message_indicated_in_sync)
                    {
                        ulog("--- in sync with p2p network");
                        _last_sync_status_message_time = now;
                        _last_sync_status_message_indicated_in_sync = true;
                        _last_sync_status_head_block = 0;
                    }
                    else if (!in_sync &&
                        item_count >= 100 && // if we're only a few blocks out of sync, don't bother the user about it
                        _last_sync_status_message_indicated_in_sync &&
                        _last_sync_status_message_time < now - fc::seconds(30))
                    {
                        std::ostringstream message;
                        message << "--- syncing with p2p network, " << item_count << " blocks left to fetch";
                        ulog(message.str());
                        _last_sync_status_message_time = now;
                        _last_sync_status_message_indicated_in_sync = false;
                        _last_sync_status_head_block = _chain_db->get_head_block_num();
                    }
                }
            }

            void ClientImpl::connection_count_changed(uint32_t new_connection_count)
            {
                fc::time_point now(fc::time_point::now());
                if (new_connection_count != _connection_count_last_value_displayed &&
                    (new_connection_count < _connection_count_always_notify_threshold ||
                    now > _last_connection_count_message_time + _connection_count_notification_interval))
                {
                    _last_connection_count_message_time = now;
                    _connection_count_last_value_displayed = new_connection_count;
                    std::ostringstream message;
                    message << "--- there are now " << new_connection_count << " active connections to the p2p network";
                    ulog(message.str());
                }
                if (_notifier)
                    _notifier->notify_connection_count_changed(new_connection_count);
            }

            uint32_t ClientImpl::get_block_number(const thinkyoung::net::ItemHashType& block_id)
            {
                return _chain_db->get_block_num(block_id);
            }

            fc::time_point_sec ClientImpl::get_block_time(const thinkyoung::net::ItemHashType& block_id)
            {
                if (block_id == thinkyoung::net::ItemHashType())
                {
                    // then the question the net is really asking is, what is the timestamp of the genesis block?
                    return _chain_db->get_genesis_timestamp();
                }
                // else they're asking about a specific block
                try
                {
                    return _chain_db->get_block_header(block_id).timestamp;
                }
                catch (const fc::canceled_exception&)
                {
                    throw;
                }
                catch (const fc::exception&)
                {
                    return fc::time_point_sec::min();
                }
            }

            fc::time_point_sec ClientImpl::get_blockchain_now()
            {
                ASSERT_TASK_NOT_PREEMPTED();
                return thinkyoung::blockchain::nonblocking_now();
            }

            thinkyoung::net::ItemHashType ClientImpl::get_head_block_id() const
            {
                return _chain_db->get_head_block_id();
            }

            uint32_t ClientImpl::estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const
            {
                return thinkyoung::blockchain::estimate_last_known_fork_from_git_revision_timestamp(unix_timestamp);
            }

            void ClientImpl::error_encountered(const std::string& message, const fc::oexception& error)
            {
                if (error)
                    _exception_db.store(*error);
                else
                    _exception_db.store(fc::exception(FC_LOG_MESSAGE(error, message.c_str())));
                ulog(message);
            }

            void ClientImpl::blocks_too_old_monitor_task()
            {
                // if we have no connections, don't warn about the head block too old --
                //   we should already be warning about no connections
                // if we're syncing, don't warn, we wouldn't be syncing if the head block weren't old
                if (_chain_db->get_head_block().timestamp < thinkyoung::blockchain::now() - fc::seconds(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 2) &&
                    !_sync_mode &&
                    _p2p_node->get_connection_count() > 0 &&
                    _notifier)
                    _notifier->notify_head_block_too_old(_chain_db->get_head_block().timestamp);

                if (!_blocks_too_old_monitor_done.canceled())
                    _blocks_too_old_monitor_done = fc::schedule([=]() { blocks_too_old_monitor_task(); },
                    fc::time_point::now() + fc::seconds(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC),
                    "block_monitor_task");
            }

            void ClientImpl::cancel_blocks_too_old_monitor_task()
            {
                try
                {
                    _blocks_too_old_monitor_done.cancel_and_wait(__FUNCTION__);
                }
                catch (const fc::exception& e)
                {
                    wlog("Unexpected exception thrown while canceling blocks_too_old_monitor(): ${e}", ("e", e.to_detail_string()));
                }
            }

        } // end namespace detail

        Client::Client(const std::string& user_agent)
            :my(new detail::ClientImpl(this, user_agent))
        {
        }

        Client::Client(const std::string& user_agent,
            thinkyoung::net::SimulatedNetworkPtr network_to_connect_to)
            : my(new detail::ClientImpl(this, user_agent))
        {
            network_to_connect_to->add_node_delegate(my.get());
            my->_p2p_node = network_to_connect_to;
            my->_simulated_network = true;
        }

        void Client::simulate_disconnect(bool state)
        {
            my->_simulate_disconnect = state;
        }

        void Client::open(const path& data_dir, const path& wallet_dir, const optional<fc::path>& genesis_file_path /*= fc::optional<fc::path>()*/,
            const fc::optional<bool> statistics_enabled,
            const std::function<void(float)> replay_status_callback)
        {
            try {
                my->_config = load_config(data_dir, my->_enable_ulog, statistics_enabled);

                fc::configure_logging(my->_config.logging);
                // re-register the _user_appender which was overwritten by configure_logging()
                fc::logger::get("user").add_appender(my->_user_appender);

                my->_exception_db.open(data_dir / "exceptions");

                bool attempt_to_recover_database = false;
                try
                {
                    if (my->_config.statistics_enabled) ulog("Additional blockchain statistics enabled");
                    my->_chain_db->open(data_dir / "chain", genesis_file_path, my->_config.statistics_enabled, replay_status_callback);
                }
                catch (const db::level_map_open_failure& e)
                {
                    if (e.to_string().find("Corruption") != string::npos)
                    {
                        elog("Chain database corrupted. Deleting it and attempting to recover.");
                        ulog("Chain database corrupted. Deleting it and attempting to recover.");
                        attempt_to_recover_database = true;
                    }
                    else
                    {
                        throw;
                    }
                }

                if (attempt_to_recover_database)
                {
                    fc::remove_all(data_dir / "chain");
                    my->_chain_db->open(data_dir / "chain", genesis_file_path, my->_config.statistics_enabled, replay_status_callback);
                }

                my->_wallet = std::make_shared<thinkyoung::wallet::Wallet>(my->_chain_db, my->_config.wallet_enabled);
                my->_wallet->set_data_directory(wallet_dir / "wallets");

                /*  //delete?
                if (my->_config.mail_server_enabled)
                {
                my->_mail_server = std::make_shared<thinkyoung::mail::server>();
                my->_mail_server->open(data_dir / "mail");
                }
                my->_mail_client = std::make_shared<thinkyoung::mail::client>(my->_wallet, my->_chain_db);
                my->_mail_client->open(data_dir / "mail_client");
                */

                //if we are using a simulated network, _p2p_node will already be set by client's constructor
                if (!my->_p2p_node)
                    my->_p2p_node = std::make_shared<thinkyoung::net::Node>(my->_user_agent);
                my->_p2p_node->set_node_delegate(my.get());
                my->start_rebroadcast_pending_loop();
                std::map<uint32_t, std::vector<ForkEntry>> forks = my->blockchain_list_forks();
                if (!forks.empty())
                {
                    uint32_t fork_num = forks.rbegin()->first;
                    my->_chain_db->set_forkdb_num(fork_num);
                }
            } FC_CAPTURE_AND_RETHROW((data_dir))
        }

        Client::~Client()
        {
            if (my->_notifier)
                my->_notifier->client_is_shutting_down();
            my->cancel_delegate_loop();
            try
            {
                my->_client_done.cancel_and_wait();
            }
            catch (...)
            {
            }
        }

        WalletPtr Client::get_wallet()const { return my->_wallet; }
        /*  //delete?
        mail_client_ptr Client::get_mail_client()const { return my->_mail_client; }
        mail_server_ptr Client::get_mail_server()const { return my->_mail_server; }
        */
        ChainDatabasePtr Client::get_chain()const { return my->_chain_db; }
        thinkyoung::rpc::RpcServerPtr Client::get_rpc_server()const { return my->_rpc_server; }
        thinkyoung::net::NodePtr Client::get_node()const { return my->_p2p_node; }

        fc::variant_object version_info()
        {
            string client_version(thinkyoung::utilities::git_revision_description);
            const size_t pos = client_version.find('/');
            if (pos != string::npos && client_version.size() > pos)
                client_version = client_version.substr(pos + 1);
#ifdef ALP_TEST_NETWORK
            //client_version += "-testnet-" + std::to_string(ALP_TEST_NETWORK_VERSION);
            client_version += "-testnet";
#endif

            fc::mutable_variant_object info;
            info["blockchain_name"] = ALP_BLOCKCHAIN_NAME;
            info["blockchain_description"] = ALP_BLOCKCHAIN_DESCRIPTION;
            info["client_version"] = client_version;
            info["alp_revision"] = thinkyoung::utilities::git_revision_sha;
            info["alp_revision_age"] = fc::get_approximate_relative_time_string(fc::time_point_sec(thinkyoung::utilities::git_revision_unix_timestamp));
            info["fc_revision"] = fc::git_revision_sha;
            info["fc_revision_age"] = fc::get_approximate_relative_time_string(fc::time_point_sec(fc::git_revision_unix_timestamp));
            info["compile_date"] = "compiled on " __DATE__ " at " __TIME__;
            info["boost_version"] = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
            info["openssl_version"] = OPENSSL_VERSION_TEXT;

            std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
            std::string os = "osx";
#elif defined(__linux__)
            std::string os = "linux";
#elif defined(_MSC_VER)
            std::string os = "win32";
#else
            std::string os = "other";
#endif
            info["build"] = os + " " + bitness;

#if defined(ALP_CLIENT_JENKINS_BUILD_NUMBER)
            info["jenkins_build_number"] = ALP_CLIENT_JENKINS_BUILD_NUMBER;
#endif
#if defined(ALP_CLIENT_JENKINS_BUILD_URL)
            info["jenkins_build_url"] = ALP_CLIENT_JENKINS_BUILD_URL;
#endif

            return info;
        }

        void Client::start_networking(std::function<void()> network_started_callback)
        {
            //Start chain_downloader if there are chain_servers to connect to; otherwise, just start p2p immediately
            if (!my->_config.chain_servers.empty())
            {
                thinkyoung::net::ChainDownloader* chain_downloader = new thinkyoung::net::ChainDownloader();
                for (const auto& server : my->_config.chain_servers)
                    chain_downloader->add_chain_server(fc::ip::endpoint::from_string(server));
                my->_chain_downloader_running = true;
                my->_chain_downloader_future = chain_downloader->get_all_blocks([this](const FullBlock& new_block, uint32_t blocks_left) {
                    my->_chain_db->push_block(new_block);
                    my->_chain_downloader_blocks_remaining = blocks_left;
                }, my->_chain_db->get_head_block_num() + 1);
                my->_chain_downloader_future.on_complete([this, chain_downloader, network_started_callback](const fc::exception_ptr& e) {
                    if (e && e->code() == fc::canceled_exception::code_value)
                    {
                        delete chain_downloader;
                        return;
                    }
                    if (e)
                        elog("chain_downloader failed with exception: ${e}", ("e", e->to_detail_string()));
                    my->_chain_downloader_running = false;
                    delete chain_downloader;
                    connect_to_p2p_network();
                    if (network_started_callback) network_started_callback();
                });
            }
            else
            {
                connect_to_p2p_network();
                if (network_started_callback) network_started_callback();
            }
        }

        //RPC server and CLI configuration rules:
        //if daemon mode requested
        //  start RPC server only (no CLI input)
        //else
        //  start RPC server if requested
        //  start CLI
        //  if input log
        //    cli.processs_commands in input log
        //    wait till finished
        //  set input stream to cin
        //  cli.process_commands from cin
        //  wait till finished
        void Client::configure_from_command_line(int argc, char** argv)
        {
            if (argc == 0 && argv == nullptr)
            {
                my->_cli = new thinkyoung::cli::Cli(this, nullptr, &std::cout);
                return;
            }
            g_client = this;
            // parse command-line options
            auto option_variables = parse_option_variables(argc, argv);

            fc::path datadir = thinkyoung::client::get_data_dir(option_variables);
            fc::path walletDir = thinkyoung::client::get_wallet_dir(option_variables, datadir);
            if (!fc::exists(datadir))
            {
                std::cout << "Creating new data directory " << datadir.preferred_string() << "\n";
                fc::create_directories(datadir);
#ifndef WIN32
                int perm = 0700;
                std::cout << "Setting UNIX permissions on new data directory to " << std::oct << perm << std::dec << "\n";
                fc::chmod(datadir, perm);
#else
                std::cout << "Note: new data directory is readable by all user accounts on non-UNIX OS\n";
#endif
            }
            if (!fc::exists(walletDir))
            {
                std::cout << "Creating new wallet directory " << walletDir.preferred_string() << "\n";
                fc::create_directories(walletDir);
#ifndef WIN32
                int perm = 0700;
                std::cout << "Setting UNIX permissions on new data directory to " << std::oct << perm << std::dec << "\n";
                fc::chmod(walletDir, perm);
#else
                std::cout << "Note: new wallet directory is readable by all user accounts on non-UNIX OS\n";
#endif
            }

            // this just clears the database if the command line says
            // TODO: rename it to smething better
            load_and_configure_chain_database(datadir, option_variables);

            fc::optional<fc::path> genesis_file_path;
            if (option_variables.count("genesis-config"))
                genesis_file_path = option_variables["genesis-config"].as<string>();

            my->_enable_ulog = option_variables["ulog"].as<bool>();
            fc::optional<bool> statistics_enabled;
            if (option_variables.count("statistics-enabled") > 0) statistics_enabled = true;
            this->open(datadir, walletDir, genesis_file_path, statistics_enabled);

            if (option_variables.count("min-delegate-connection-count"))
                my->_delegate_config.network_min_connection_count = option_variables["min-delegate-connection-count"].as<uint32_t>();

            this->configure(datadir);

            if (option_variables.count("max-connections"))
            {
                my->_config.maximum_number_of_connections = option_variables["max-connections"].as<uint16_t>();
                fc::mutable_variant_object params;
                params["maximum_number_of_connections"] = my->_config.maximum_number_of_connections;
                this->network_set_advanced_node_parameters(params);
            }

            my->configure_rpc_server(my->_config, option_variables);
            my->configure_chain_server(my->_config, option_variables);

            uint16_t p2p_port = 0;
            if (option_variables.count("p2p-port"))
            {
                string str_port = option_variables["p2p-port"].as<string>();
                bool p2p_wait_if_not_available = true;
                if (str_port[0] == 'r')
                {
                    str_port = str_port.substr(1);
                    p2p_wait_if_not_available = false;
                }
                p2p_port = (uint16_t)std::stoul(str_port);
                listen_on_port(p2p_port, p2p_wait_if_not_available);
            }
            accept_incoming_p2p_connections(option_variables["accept-incoming-connections"].as<bool>());

            // else we use the default set in thinkyoung::net::node

            //initialize cli
            if (option_variables.count("daemon") || my->_config.ignore_console)
            {
                std::cout << "Running in daemon mode, ignoring console\n";
                my->_cli = new thinkyoung::cli::Cli(this, nullptr, &std::cout);
                my->_cli->set_daemon_mode(true);
            }
            else //we will accept input from the console
            {
                //if user wants us to execute a command script log for the CLI,
                //  extract the commands and put them in a temporary input stream to pass to the CLI

                if (option_variables.count("input-log"))
                {
                    std::vector<string> input_logs = option_variables["input-log"].as< std::vector<string> >();
                    string input_commands;
                    for (const auto& input_log : input_logs)
                        input_commands += extract_commands_from_log_file(input_log);
                    my->_command_script_holder.reset(new std::stringstream(input_commands));
                }

                const fc::path console_log_file = datadir / "console.log";
                if (option_variables.count("log-commands") <= 0)
                {
                    /* Remove any console logs for security */
                    fc::remove_all(console_log_file);
                    /* Don't create a log file, just output to console */
                    my->_cli = new thinkyoung::cli::Cli(this, my->_command_script_holder.get(), &std::cout);
                }
                else
                {
                    /* Tee cli output to the console and a log file */
                    ulog("Logging commands to: ${file}", ("file", console_log_file.string()));
                    my->_console_log.open(console_log_file.string());
                    my->_tee_device.reset(new TeeDevice(std::cout, my->_console_log));;
                    my->_tee_stream.reset(new TeeStream(*my->_tee_device.get()));

                    my->_cli = new thinkyoung::cli::Cli(this, my->_command_script_holder.get(), my->_tee_stream.get());
                    /* Echo command input to the log file */
                    my->_cli->set_input_stream_log(my->_console_log);
                }
            } //end else we will accept input from the console

            if (option_variables.count("stop-before-block"))
                my->_debug_stop_before_block_num = option_variables["stop-before-block"].as<uint32_t>();

            // start listening.  this just finds a port and binds it, it doesn't start
            // accepting connections until connect_to_p2p_network()
            listen_to_p2p_network();

            if (option_variables["upnp"].as<bool>())
            {
                ulog("Attempting to map P2P port ${port} with UPNP...", ("port", get_p2p_listening_endpoint().port()));
                my->_upnp_service = std::unique_ptr<thinkyoung::net::UpnpService>(new thinkyoung::net::UpnpService);
                my->_upnp_service->map_port(get_p2p_listening_endpoint().port());
                fc::usleep(fc::seconds(3));
            }

            if (option_variables.count("total-bandwidth-limit"))
                get_node()->set_total_bandwidth_limit(option_variables["total-bandwidth-limit"].as<uint32_t>(),
                option_variables["total-bandwidth-limit"].as<uint32_t>());

            if (option_variables.count("disable-peer-advertising"))
                get_node()->disable_peer_advertising();

            if (option_variables.count("clear-peer-database"))
            {
                ulog("Erasing old peer database");
                get_node()->clear_peer_database();
            }

            if (option_variables.count("growl") || my->_config.growl_notify_endpoint)
            {
                std::string host_to_notify;
                if (option_variables.count("growl"))
                    host_to_notify = option_variables["growl"].as<std::string>();
                else
                    host_to_notify = *my->_config.growl_notify_endpoint;

                uint16_t port_to_notify = 23053;
                std::string::size_type colon_pos = host_to_notify.find(':');
                if (colon_pos != std::string::npos)
                {
                    port_to_notify = boost::lexical_cast<uint16_t>(host_to_notify.substr(colon_pos + 1));
                    host_to_notify = host_to_notify.substr(0, colon_pos);
                }

                fc::optional<std::string> growl_password;
                if (option_variables.count("growl-password"))
                    growl_password = option_variables["growl-password"].as<std::string>();
                else
                    growl_password = my->_config.growl_password;

                std::string alp_instance_identifier = "Alp";
                if (option_variables.count("growl-identifier"))
                    alp_instance_identifier = option_variables["growl-identifier"].as<std::string>();
                else if (my->_config.growl_alp_client_identifier)
                    alp_instance_identifier = *my->_config.growl_alp_client_identifier;
                my->_notifier = std::make_shared<AlpGntpNotifier>(host_to_notify, port_to_notify, alp_instance_identifier, growl_password);
                my->_blocks_too_old_monitor_done = fc::schedule([=]() { my->blocks_too_old_monitor_task(); },
                    fc::time_point::now() + fc::seconds(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC),
                    "block_monitor_task");
            }

            start_networking([=]{
                fc::ip::endpoint actual_p2p_endpoint = this->get_p2p_listening_endpoint();
                std::ostringstream port_stream;
                if (actual_p2p_endpoint.get_address() == fc::ip::address())
                    port_stream << "port " << actual_p2p_endpoint.port();
                else
                    port_stream << (string)actual_p2p_endpoint;

                if (option_variables.count("log-commands") <= 0) /* Was breaking regression tests */
                {
                    if (option_variables["accept-incoming-connections"].as<bool>())
                    {
                        ulog("Listening for P2P connections on ${port}", ("port", port_stream.str()));
                        if (option_variables.count("p2p-port"))
                        {
                            if (p2p_port != 0 && p2p_port != actual_p2p_endpoint.port())
                                ulog(" (unable to bind to the desired port ${p2p_port} )", ("p2p_port", p2p_port));
                        }
                    }
                    else
                        ulog("Not accepting incoming P2P connections");
                }

                if (option_variables.count("connect-to"))
                {
                    std::vector<string> hosts = option_variables["connect-to"].as<std::vector<string>>();
                    for (auto peer : hosts)
                        this->connect_to_peer(peer);
                }
                else if (!option_variables.count("disable-default-peers"))
                {
                    for (string default_peer : my->_config.default_peers)
                    {
                        try
                        {
                            this->add_node(default_peer);
                        }
                        catch (net::endpoint_in_blacklist)
                        {
                        }
                    }
                }
            });

            if (my->_config.chain_server.enabled)
            {
                my->_chain_server = std::unique_ptr<thinkyoung::net::ChainServer>(
                    new thinkyoung::net::ChainServer(my->_chain_db,
                    my->_config.chain_server.listen_port));
                ulog("Starting a chain server on port ${port}", ("port", my->_chain_server->get_listening_port()));
            }

            my->_chain_db->set_relay_fee(my->_config.min_relay_fee);
            my->_chain_db->repair_database();
        } //configure_from_command_line

        fc::future<void> Client::start()
        {
            my->_client_done = fc::async([=](){ my->start(); }, "Client::start");
            return my->_client_done;
        }

        bool Client::is_connected() const
        {
            return my->_p2p_node->is_connected();
        }

        thinkyoung::net::NodeIdType Client::get_node_id() const
        {
            return my->_p2p_node->get_node_id();
        }

        void Client::listen_on_port(uint16_t port_to_listen, bool wait_if_not_available)
        {
            my->_p2p_node->listen_on_port(port_to_listen, wait_if_not_available);
        }

        void Client::accept_incoming_p2p_connections(bool accept)
        {
            my->_p2p_node->accept_incoming_connections(accept);
        }

        const Config& Client::configure(const fc::path& configuration_directory)
        {
            my->_data_dir = configuration_directory;

            if (!my->_simulated_network)
                my->_p2p_node->load_configuration(my->_data_dir);

            return my->_config;
        }

        void Client::init_cli()
        {
            if (!my->_cli)
                my->_cli = new thinkyoung::cli::Cli(this, nullptr, &std::cout);
        }

        void Client::set_daemon_mode(bool daemon_mode)
        {
            init_cli();
            my->_cli->set_daemon_mode(daemon_mode);
        }

        fc::path Client::get_data_dir()const
        {
            return my->_data_dir;
        }

        /* static */ fc::ip::endpoint Client::string_to_endpoint(const std::string& remote_endpoint)
        {
            try
            {
                ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
                // first, try and parse the endpoint as a numeric_ipv4_address:port that doesn't need DNS lookup
                return fc::ip::endpoint::from_string(remote_endpoint);
            }
            catch (...)
            {
            }

            // couldn't parse as a numeric ip address, try resolving as a DNS name.  This can yield, so don't
            // do it in the catch block above
            string::size_type colon_pos = remote_endpoint.find(':');
            try
            {
                uint16_t port = boost::lexical_cast<uint16_t>(remote_endpoint.substr(colon_pos + 1, remote_endpoint.size()));

                string hostname = remote_endpoint.substr(0, colon_pos);
                std::vector<fc::ip::endpoint> endpoints = fc::resolve(hostname, port);
                if (endpoints.empty())
                    FC_THROW_EXCEPTION(fc::unknown_host_exception, "The host name can not be resolved: ${hostname}", ("hostname", hostname));
                return endpoints.back();
            }
            catch (const boost::bad_lexical_cast&)
            {
                FC_THROW("Bad port: ${port}", ("port", remote_endpoint.substr(colon_pos + 1, remote_endpoint.size())));
            }
        }

        void Client::add_node(const string& remote_endpoint, int32_t oper_flag)
        {
            fc::ip::endpoint endpoint;
            fc::oexception string_to_endpoint_error;
            try
            {
                endpoint = string_to_endpoint(remote_endpoint);
            }
            catch (const fc::exception& e)
            {
                string_to_endpoint_error = e;
            }
            if (string_to_endpoint_error)
            {
                ulog("Unable to add peer ${remote_endpoint}: ${error}",
                    ("remote_endpoint", remote_endpoint)("error", string_to_endpoint_error->to_string()));
                return;
            }

            try
            {
                ulog("Adding peer ${peer} to peer database", ("peer", endpoint));
                my->_p2p_node->add_node(endpoint, oper_flag);
            }
            catch (const thinkyoung::net::already_connected_to_requested_peer&)
            {
            }
        }
        void Client::connect_to_peer(const string& remote_endpoint)
        {
            fc::ip::endpoint endpoint;
            fc::oexception string_to_endpoint_error;
            try
            {
                endpoint = string_to_endpoint(remote_endpoint);
            }
            catch (const fc::exception& e)
            {
                string_to_endpoint_error = e;
            }
            if (string_to_endpoint_error)
            {
                ulog("Unable to initiate connection to peer ${remote_endpoint}: ${error}",
                    ("remote_endpoint", remote_endpoint)("error", string_to_endpoint_error->to_string()));
                return;
            }

            try
            {
                ulog("Attempting to connect to peer ${peer}", ("peer", endpoint));
                my->_p2p_node->connect_to_endpoint(endpoint);
            }
            catch (const thinkyoung::net::already_connected_to_requested_peer&)
            {
            }
        }

        void Client::listen_to_p2p_network()
        {
            my->_p2p_node->listen_to_p2p_network();
        }

        void Client::connect_to_p2p_network()
        {
            thinkyoung::net::ItemId head_item_id;
            head_item_id.item_type = thinkyoung::client::block_message_type;
            uint32_t last_block_num = my->_chain_db->get_head_block_num();
            if (last_block_num == (uint32_t)-1)
                head_item_id.item_hash = thinkyoung::net::ItemHashType();
            else
                head_item_id.item_hash = my->_chain_db->get_head_block_id();
            my->_p2p_node->sync_from(head_item_id, thinkyoung::blockchain::get_list_of_fork_block_numbers());
            my->_p2p_node->connect_to_p2p_network();
        }

        fc::ip::endpoint Client::get_p2p_listening_endpoint() const
        {
            return my->_p2p_node->get_actual_listening_endpoint();
        }

        bool Client::handle_message(const thinkyoung::net::Message& message, bool sync_mode)
        {
            return my->handle_message(message, sync_mode);
        }
        void Client::sync_status(uint32_t item_type, uint32_t item_count)
        {
            my->sync_status(item_type, item_count);
        }

        fc::sha256 ClientNotification::digest()const
        {
            fc::sha256::encoder enc;
            fc::raw::pack(enc, *this);
            return enc.result();
        }

        void ClientNotification::sign(const fc::ecc::private_key& key)
        {
            signature = key.sign_compact(digest());
        }

        fc::ecc::public_key ClientNotification::signee() const
        {
            return fc::ecc::public_key(signature, digest());
        }

        void Client::set_client_debug_name(const string& name)
        {
            return my->set_client_debug_name(name);
        }

        DelegateConfig Client::get_delegate_config()
        {
            return my->_delegate_config;
        }
        /**
        * Detail Implementation
        */
        namespace detail  {

            //This function is here instead of in debug_api.cpp because it needs load_config() which is local to client.cpp
            // 			void client_impl::debug_update_logging_config()  //why????
            // 			{
            //    config temp_config = load_config( _data_dir, _enable_ulog, _config.statistics_enabled );
            // 				fc::configure_logging(temp_config.logging);
            // 				// re-register the _user_appender which was overwritten by configure_logging()
            // 				fc::logger::get("user").add_appender(_user_appender);
            // 			}

            //This function is here insetad of in general_api.cpp because it needs extract_commands_from_log_file()
            void ClientImpl::execute_script(const fc::path& script_filename) const
            {
                if (_cli)
                {
                    if (!fc::exists(script_filename))
                        FC_THROW_EXCEPTION(fc::file_not_found_exception, "Script file not found!");
                    string input_commands = extract_commands_from_log_file(script_filename);
                    std::stringstream input_stream(input_commands);
                    _cli->process_commands(&input_stream);
                    _cli->process_commands(&std::cin);
                }
            }
        } // namespace detail
        ///////////////////////////////////////////////////////////////////////////////////////////////


        thinkyoung::api::CommonApi* Client::get_impl() const
        {
            return my.get();
        }

        bool RpcServerConfig::is_valid() const
        {
#ifndef _WIN32
            if (rpc_user.empty())
                return false;
            if (rpc_password.empty())
                return false;
#endif
            return true;
        }

    }
} // thinkyoung::client
