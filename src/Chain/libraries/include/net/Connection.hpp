#pragma once
#include <net/StcpSocket.hpp>
#include <net/Message.hpp>
#include <fc/exception/exception.hpp>
#include <db/LevelMap.hpp>

namespace thinkyoung {
    namespace net {

        namespace detail { class ConnectionImpl; }

        class Connection;
        struct Message;
        struct MessageHeader;

        typedef std::shared_ptr<Connection> ConnectionPtr;

        /**
         * @brief defines callback interface for connections
         */
        class ConnectionDelegate
        {
        public:
            /** Called when given network connection has completed receiving a message and it is ready
                for further processing.
                */
            virtual void on_connection_message(Connection& c, const Message& m) = 0;
            /// Called when connection has been lost.
            virtual void on_connection_disconnected(Connection& c) = 0;

        protected:
            /// Only implementation is responsible for this object lifetime.
            virtual ~ConnectionDelegate() {}
        };



        /**
         *  Manages a connection to a remote p2p node. A connection
         *  processes a stream of messages that have a common header
         *  and ensures everything is properly encrypted.
         *
         *  A connection also allows arbitrary data to be attached to it
         *  for use by other protocols built at higher levels.
         */
        class Connection : public std::enable_shared_from_this < Connection >
        {
        public:
            Connection(const StcpSocketPtr& c, ConnectionDelegate* d);
            Connection(ConnectionDelegate* d);
            ~Connection();

            StcpSocketPtr  get_socket()const;
            fc::ip::endpoint remote_endpoint()const;

            void send(const Message& m);

            void connect(const std::string& host_port);
            void connect(const fc::ip::endpoint& ep);
            void close();

            void exec_sync_loop();

        private:
            std::unique_ptr<detail::ConnectionImpl> my;
        };


    }
} // thinkyoung:net
