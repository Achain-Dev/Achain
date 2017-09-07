#pragma once
#include <net/StcpSocket.hpp>
#include <net/Message.hpp>
#include <fc/exception/exception.hpp>
#include <blockchain/ChainDatabase.hpp>
#include <db/LevelMap.hpp>

using namespace thinkyoung::blockchain;
using namespace thinkyoung::net;

namespace thinkyoung {
    namespace net {

        namespace detail { class ChainConnectionImpl; }

        class ChainConnection;
        typedef std::shared_ptr<ChainConnection> ChainConnectionPtr;

        /**
         * @brief defines callback interface for chain_connections
         */
        class ChainConnectionDelegate
        {
        public:
            virtual ~ChainConnectionDelegate(){};
            virtual void on_connection_message(ChainConnection& c, const Message& m){};
            virtual void on_connection_disconnected(ChainConnection& c){}
        };



        /**
         *  Manages a connection to a remote p2p node. A connection
         *  processes a stream of messages that have a common header
         *  and ensures everything is properly encrypted.
         *
         *  A connection also allows arbitrary data to be attached to it
         *  for use by other protocols built at higher levels.
         */
        class ChainConnection : public std::enable_shared_from_this < ChainConnection >
        {
        public:
            ChainConnection(const StcpSocketPtr& c, ChainConnectionDelegate* d);
            ChainConnection(ChainConnectionDelegate* d);
            ~ChainConnection();

            StcpSocketPtr  get_socket()const;
            fc::ip::endpoint remote_endpoint()const;

            void send(const Message& m);

            void connect(const std::string& host_port);
            void connect(const fc::ip::endpoint& ep);
            void close();

            thinkyoung::blockchain::BlockIdType get_last_block_id()const;
            void                           set_last_block_id(const thinkyoung::blockchain::BlockIdType& t);

            void exec_sync_loop();
            void set_database(thinkyoung::blockchain::ChainDatabase*);

        private:
            std::unique_ptr<detail::ChainConnectionImpl> my;
        };

    }
} // thinkyoung::net   
