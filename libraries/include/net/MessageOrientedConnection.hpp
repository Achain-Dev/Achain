#pragma once
#include <fc/network/tcp_socket.hpp>
#include <net/Message.hpp>

namespace thinkyoung {
    namespace net {

        namespace detail { class MessageOrientedConnectionImpl; }

        class MessageOrientedConnection;

        /** receives incoming messages from a message_oriented_connection object */
        class MessageOrientedConnectionDelegate
        {
        public:
            virtual void on_message(MessageOrientedConnection* originating_connection, const Message& received_message) = 0;
            virtual void on_connection_closed(MessageOrientedConnection* originating_connection) = 0;
        };

        /** uses a secure socket to create a connection that reads and writes a stream of `fc::net::message` objects */
        class MessageOrientedConnection
        {
        public:
            MessageOrientedConnection(MessageOrientedConnectionDelegate* delegate = nullptr);
            ~MessageOrientedConnection();
            fc::tcp_socket& get_socket();
            void accept();
            void bind(const fc::ip::endpoint& local_endpoint);
            void connect_to(const fc::ip::endpoint& remote_endpoint);

            void send_message(const Message& message_to_send);
            void close_connection();
            void destroy_connection();

            uint64_t get_total_bytes_sent() const;
            uint64_t get_total_bytes_received() const;
            fc::time_point get_last_message_sent_time() const;
            fc::time_point get_last_message_received_time() const;
            fc::time_point get_connection_time() const;
            fc::sha512 get_shared_secret() const;
        private:
            std::unique_ptr<detail::MessageOrientedConnectionImpl> my;
        };
        typedef std::shared_ptr<MessageOrientedConnection> MessageOrientedConnectionPtr;

    }
} // thinkyoung::net
