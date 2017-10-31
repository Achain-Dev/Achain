#pragma once

#include <api/CommonApi.hpp>
#include <blockchain/ChainDatabase.hpp>
#include <client/SeedNodes.hpp>
#include <net/Node.hpp>
#include <rpc/RpcClientApi.hpp>
#include <rpc_stubs/CommonApiClient.hpp>
#include <wallet/Wallet.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/thread/thread.hpp>

#include <boost/program_options.hpp>
#include <memory>

namespace thinkyoung {
    namespace rpc {
        class RpcServer;
        typedef std::shared_ptr<RpcServer> RpcServerPtr;
    }
}

namespace thinkyoung {
    namespace cli {
        class Cli;
    }
};

namespace thinkyoung {
    namespace client {

        using namespace thinkyoung::blockchain;
        using namespace thinkyoung::wallet;
        //    using thinkyoung::mail::mail_client_ptr;
        //    using thinkyoung::mail::mail_server_ptr;

        boost::program_options::variables_map parse_option_variables(int argc, char** argv);
        fc::path get_data_dir(const boost::program_options::variables_map& option_variables);
        fc::variant_object version_info();

        namespace detail { class ClientImpl; }

        using namespace thinkyoung::rpc;

        struct RpcServerConfig
        {
            RpcServerConfig()
                : enable(false),
                rpc_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
                httpd_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
                encrypted_rpc_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
                htdocs("./htdocs")
            {}

            bool             enable;
            bool             enable_cache = true;
            std::string      rpc_user;
            std::string      rpc_password;
            fc::ip::endpoint rpc_endpoint;
            fc::ip::endpoint httpd_endpoint;
            fc::ip::endpoint encrypted_rpc_endpoint;
            std::string      encrypted_rpc_wif_key;
            fc::path         htdocs;

            bool is_valid() const; /* Currently just checks if rpc port is set */
        };

        struct ChainServerConfig
        {
            ChainServerConfig()
                : enabled(false),
                listen_port(0)
            {}

            bool enabled;
            uint16_t listen_port;
        };

        struct Config
        {
            fc::logging_config  logging = fc::logging_config::default_config();
            bool                ignore_console = false;
            string              client_debug_name;

            RpcServerConfig   rpc;

            optional<fc::path>  genesis_config;
            bool                statistics_enabled = false;

            vector<string>      default_peers = SeedNodes;
            uint16_t            maximum_number_of_connections = ALP_NET_DEFAULT_MAX_CONNECTIONS;
            bool                use_upnp = true;

            vector<string>      chain_servers;
            ChainServerConfig chain_server;

            bool                wallet_enabled = true;
            ShareType          min_relay_fee = ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;
            string              wallet_callback_url;

            ShareType          light_network_fee = ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;
            ShareType          light_relay_fee = ALP_BLOCKCHAIN_DEFAULT_RELAY_FEE;
            /** relay account name is used to specify the name of the account that must be paid when
             * network_broadcast_transaction is called by a light weight client.  If it is unset then
             * no fee will be charged.  The fee charged by the light server will be the fee charged
             * light_relay_fee for allowing general network transactions to propagate.  In effect, light clients
             * pay 2x the fees, one to the relay_account_name and one to the network delegates.
             */
            string              relay_account_name;

            /** if this client provides faucet services, specify the account to pay from here */
            string              faucet_account_name;

            optional<string>    growl_notify_endpoint;
            optional<string>    growl_password;
            optional<string>    growl_alp_client_identifier;

        };


        /**
         * @class client
         * @brief integrates the network, wallet, and blockchain
         *
         */
        class Client : public thinkyoung::rpc_stubs::CommonApiClient,
            public std::enable_shared_from_this < Client >
        {
        public:
            Client(const std::string& user_agent);
            Client(const std::string& user_agent,
                thinkyoung::net::SimulatedNetworkPtr network_to_connect_to);

            void simulate_disconnect(bool state);

            virtual ~Client();

            void start_networking(std::function<void()> network_started_callback = std::function<void()>());
            void configure_from_command_line(int argc, char** argv);
            fc::future<void> start();
            void open(const path& data_dir,
                const path& wallet_dir,
                const optional<fc::path>& genesis_file_path = fc::optional<fc::path>(),
                const fc::optional<bool> statistics_enabled = fc::optional<bool>(),
                const std::function<void(float)> replay_status_callback = std::function<void(float)>());

            void init_cli();
            void set_daemon_mode(bool daemon_mode);
            void set_client_debug_name(const string& name);


            ChainDatabasePtr         get_chain()const;
            WalletPtr                 get_wallet()const;
            //         mail_client_ptr            get_mail_client()const;
            //         mail_server_ptr            get_mail_server()const;
            thinkyoung::rpc::RpcServerPtr   get_rpc_server()const;
            thinkyoung::net::NodePtr         get_node()const;
            fc::path                   get_data_dir()const;

            // returns true if the client is connected to the network
            bool                is_connected() const;
            thinkyoung::net::NodeIdType get_node_id() const;

            const Config& configure(const fc::path& configuration_directory);

            // functions for taking command-line parameters and passing them on to the p2p node
            void listen_on_port(uint16_t port_to_listen, bool wait_if_not_available);
            void accept_incoming_p2p_connections(bool accept);
            void listen_to_p2p_network();
            static fc::ip::endpoint string_to_endpoint(const std::string& remote_endpoint);
            void add_node(const string& remote_endpoint, int32_t oper_flag = 1);
            void connect_to_peer(const string& remote_endpoint);
            void connect_to_p2p_network();

            fc::ip::endpoint get_p2p_listening_endpoint() const;
            bool handle_message(const thinkyoung::net::Message&, bool sync_mode);
            void sync_status(uint32_t item_type, uint32_t item_count);
            DelegateConfig get_delegate_config();
        protected:
            virtual thinkyoung::api::CommonApi* get_impl() const override;

        private:
            unique_ptr<detail::ClientImpl> my;
        };

        typedef shared_ptr<Client> ClientPtr;

        /* Message broadcast on the network to notify all clients of some important information
          (security vulnerability, new version, that sort of thing) */
        class ClientNotification
        {
        public:
            fc::time_point_sec         timestamp;
            string                message;
            fc::ecc::compact_signature signature;

            //client_notification();
            fc::sha256 digest() const;
            void sign(const fc::ecc::private_key& key);
            fc::ecc::public_key signee() const;
        };
        typedef shared_ptr<ClientNotification> ClientNotificationPtr;

    }
} // thinkyoung::client

extern const std::string ALP_MESSAGE_MAGIC;

FC_REFLECT(thinkyoung::client::ClientNotification, (timestamp)(message)(signature))
FC_REFLECT(thinkyoung::client::RpcServerConfig, (enable)(enable_cache)(rpc_user)(rpc_password)(rpc_endpoint)(httpd_endpoint)
(encrypted_rpc_endpoint)(encrypted_rpc_wif_key)(htdocs))
FC_REFLECT(thinkyoung::client::ChainServerConfig, (enabled)(listen_port))
FC_REFLECT(thinkyoung::client::Config,
(logging)
(ignore_console)
(client_debug_name)
(rpc)
(genesis_config)
(statistics_enabled)
(default_peers)
(maximum_number_of_connections)
(use_upnp)
(chain_servers)
(chain_server)
(wallet_enabled)
(min_relay_fee)
(wallet_callback_url)
(light_network_fee)
(light_relay_fee)
(relay_account_name)
(faucet_account_name)
(growl_notify_endpoint)
(growl_password)
(growl_alp_client_identifier)
(rpc)
)

