#pragma once

#include <net/Node.hpp>
#include <net/PeerDatabase.hpp>
#include <net/MessageOrientedConnection.hpp>
#include <net/StcpSocket.hpp>
#include <net/Config.hpp>
#include <client/Messages.hpp>

#include <boost/tuple/tuple.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <queue>
#include <boost/container/deque.hpp>

namespace thinkyoung {
    namespace net
    {
        struct FirewallCheckStateData
        {
            NodeIdType        expected_node_id;
            fc::ip::endpoint endpoint_to_test;

            // if we're coordinating a firewall check for another node, these are the helper
            // nodes we've already had do the test (if this structure is still relevant, that
            // that means they have all had indeterminate results
            std::set<NodeIdType> nodes_already_tested;

            // If we're a just a helper node, this is the node we report back to 
            // when we have a result
            NodeIdType        requesting_peer;
        };

        class PeerConnection;
        class PeerConnectionDelegate
        {
        public:
            virtual void on_message(PeerConnection* originating_peer,
                const Message& received_message) = 0;
            virtual void on_connection_closed(PeerConnection* originating_peer) = 0;
            virtual Message get_message_for_item(const ItemId& item) = 0;
        };

        class PeerConnection;
        typedef std::shared_ptr<PeerConnection> PeerConnectionPtr;
        class PeerConnection : public MessageOrientedConnectionDelegate,
            public std::enable_shared_from_this < PeerConnection >
        {
        public:
            enum class OurConnectionState
            {
                disconnected,
                just_connected, // if in this state, we have sent a hello_message
                connection_accepted, // remote side has sent us a connection_accepted, we're operating normally with them
                connection_rejected // remote side has sent us a connection_rejected, we may be exchanging address with them or may just be waiting for them to close
            };
            enum class TheirConnectionState
            {
                disconnected,
                just_connected, // we have not yet received a hello_message
                connection_accepted, // we have sent them a connection_accepted
                connection_rejected // we have sent them a connection_rejected
            };
            enum class ConnectionNegotiationStatus
            {
                disconnected,
                connecting,
                connected,
                accepting,
                accepted,
                hello_sent,
                peer_connection_accepted,
                peer_connection_rejected,
                negotiation_complete,
                closing,
                closed
            };
        private:
            PeerConnectionDelegate*      _node;
            fc::optional<fc::ip::endpoint> _remote_endpoint;
            MessageOrientedConnection    _message_connection;

            /* a base class for messages on the queue, to hide the fact that some
             * messages are complete messages and some are only hashes of messages.
             */
            struct QueuedMessage
            {
                fc::time_point enqueue_time;
                fc::time_point transmission_start_time;
                fc::time_point transmission_finish_time;

                QueuedMessage(fc::time_point enqueue_time = fc::time_point::now()) :
                    enqueue_time(enqueue_time)
                {}

                virtual Message get_message(PeerConnectionDelegate* node) = 0;
                /** returns roughly the number of bytes of memory the message is consuming while
                 * it is sitting on the queue
                 */
                virtual size_t get_size_in_queue() = 0;
                virtual ~QueuedMessage() {}
            };

            /* when you queue up a 'real_queued_message', a full copy of the message is
             * stored on the heap until it is sent
             */
            struct RealQueuedMessage : QueuedMessage
            {
                Message        message_to_send;
                size_t         message_send_time_field_offset;

                RealQueuedMessage(Message message_to_send,
                    size_t message_send_time_field_offset = (size_t)-1) :
                    message_to_send(std::move(message_to_send)),
                    message_send_time_field_offset(message_send_time_field_offset)
                {}

                Message get_message(PeerConnectionDelegate* node) override;
                size_t get_size_in_queue() override;
            };

            /* when you queue up a 'virtual_queued_message', we just queue up the hash of the
             * item we want to send.  When it reaches the top of the queue, we make a callback
             * to the node to generate the message.
             */
            struct VirtualQueuedMessage : QueuedMessage
            {
                ItemId item_to_send;

                VirtualQueuedMessage(ItemId item_to_send) :
                    item_to_send(std::move(item_to_send))
                {}

                Message get_message(PeerConnectionDelegate* node) override;
                size_t get_size_in_queue() override;
            };


            size_t _total_queued_messages_size;
            std::queue<std::unique_ptr<QueuedMessage>, std::list<std::unique_ptr<QueuedMessage> > > _queued_messages;
            fc::future<void> _send_queued_messages_done;
        public:
            fc::time_point connection_initiation_time;
            fc::time_point connection_closed_time;
            fc::time_point connection_terminated_time;
            PeerConnectionDirection direction;
            //connection_state state;
            FirewalledState is_firewalled;
            fc::microseconds clock_offset;
            fc::microseconds round_trip_delay;

            OurConnectionState our_state;
            bool they_have_requested_close;
            TheirConnectionState their_state;
            bool we_have_requested_close;

            ConnectionNegotiationStatus negotiation_status;
            fc::oexception connection_closed_error;

            fc::time_point get_connection_time()const { return _message_connection.get_connection_time(); }
            fc::time_point get_connection_terminated_time()const { return connection_terminated_time; }

            /// data about the peer node
            /// @{
            /** node_public_key from the hello message, zero-initialized before we get the hello */
            NodeIdType        node_public_key;
            /** the unique identifier we'll use to refer to the node with.  zero-initialized before
             * we receive the hello message, at which time it will be filled with either the "node_id"
             * from the user_data field of the hello, or if none is present it will be filled with a
             * copy of node_public_key */
            NodeIdType        node_id;
            uint32_t         core_protocol_version;
            std::string      user_agent;
            fc::optional<std::string> thinkyoung_git_revision_sha;
            fc::optional<fc::time_point_sec> thinkyoung_git_revision_unix_timestamp;
            fc::optional<std::string> fc_git_revision_sha;
            fc::optional<fc::time_point_sec> fc_git_revision_unix_timestamp;
            fc::optional<std::string> platform;
            fc::optional<uint32_t> bitness;

            // for inbound connections, these fields entry what the peer sent us in
            // its hello message.  For outbound, they entry what we sent the peer
            // in our hello message
            fc::ip::address inbound_address;
            uint16_t inbound_port;
            uint16_t outbound_port;
            /// @}

            typedef std::unordered_map<ItemId, fc::time_point> item_to_time_map_type;

            /// blockchain synchronization state data
            /// @{
            boost::container::deque<ItemHashType> ids_of_items_to_get; /// id of items in the blockchain that this peer has told us about
            std::set<ItemHashType> ids_of_items_being_processed; /// list of all items this peer has offered use that we've already handed to the client but the client hasn't finished processing
            uint32_t number_of_unfetched_item_ids; /// number of items in the blockchain that follow ids_of_items_to_get but the peer hasn't yet told us their ids
            bool peer_needs_sync_items_from_us;
            bool we_need_sync_items_from_peer;
            fc::optional<boost::tuple<ItemId, fc::time_point> > item_ids_requested_from_peer; /// we check this to detect a timed-out request and in busy()
            item_to_time_map_type sync_items_requested_from_peer; /// ids of blocks we've requested from this peer during sync.  fetch from another peer if this peer disconnects
            uint32_t last_block_number_delegate_has_seen; /// the number of the last block this peer has told us about that the delegate knows (ids_of_items_to_get[0] should be the id of block [this value + 1])
            ItemHashType last_block_delegate_has_seen; /// the hash of the last block  this peer has told us about that the peer knows
            fc::time_point_sec last_block_time_delegate_has_seen;
            bool inhibit_fetching_sync_blocks;
            /// @}

            /// non-synchronization state data
            /// @{
            struct TimestampedItemId
            {
                ItemId            item;
                fc::time_point_sec timestamp;
                TimestampedItemId(const ItemId& item, const fc::time_point_sec timestamp) :
                    item(item),
                    timestamp(timestamp)
                {}
            };
            struct timestamp_index{};
            typedef boost::multi_index_container < TimestampedItemId,
                boost::multi_index::indexed_by < boost::multi_index::hashed_unique<boost::multi_index::member<TimestampedItemId, ItemId, &TimestampedItemId::item>,
                std::hash<ItemId> >,
                boost::multi_index::ordered_non_unique < boost::multi_index::tag<timestamp_index>,
                boost::multi_index::member<TimestampedItemId, fc::time_point_sec, &TimestampedItemId::timestamp> > > > timestamped_items_set_type;
            timestamped_items_set_type inventory_peer_advertised_to_us;
            timestamped_items_set_type inventory_advertised_to_peer;

            item_to_time_map_type items_requested_from_peer;  /// items we've requested from this peer during normal operation.  fetch from another peer if this peer disconnects
            /// @}

            // if they're flooding us with transactions, we set this to avoid fetching for a few seconds to let the
            // blockchain catch up
            fc::time_point transaction_fetching_inhibited_until;

            uint32_t last_known_fork_block_number;

            fc::future<void> accept_or_connect_task_done;

            FirewallCheckStateData *firewall_check_state;
#ifndef NDEBUG
        private:
            fc::thread* _thread;
            unsigned _send_message_queue_tasks_running; // temporary debugging
#endif
        private:
            PeerConnection(PeerConnectionDelegate* delegate);
            void destroy();
        public:
            static PeerConnectionPtr make_shared(PeerConnectionDelegate* delegate); // use this instead of the constructor
            virtual ~PeerConnection();

            fc::tcp_socket& get_socket();
            void accept_connection();
            void connect_to(const fc::ip::endpoint& remote_endpoint, fc::optional<fc::ip::endpoint> local_endpoint = fc::optional<fc::ip::endpoint>());

            void on_message(MessageOrientedConnection* originating_connection, const Message& received_message) override;
            void on_connection_closed(MessageOrientedConnection* originating_connection) override;

            void send_queueable_message(std::unique_ptr<QueuedMessage>&& message_to_send);
            void send_message(const Message& message_to_send, size_t message_send_time_field_offset = (size_t)-1);
            void send_item(const ItemId& item_to_send);
            void close_connection();
            void destroy_connection();

            uint64_t get_total_bytes_sent() const;
            uint64_t get_total_bytes_received() const;

            fc::time_point get_last_message_sent_time() const;
            fc::time_point get_last_message_received_time() const;

            fc::optional<fc::ip::endpoint> get_remote_endpoint();
            fc::ip::endpoint get_local_endpoint();
            void set_remote_endpoint(fc::optional<fc::ip::endpoint> new_remote_endpoint);

            bool busy();
            bool idle();

            bool is_transaction_fetching_inhibited() const;
            fc::sha512 get_shared_secret() const;
            void clear_old_inventory();
            bool is_inventory_advertised_to_us_list_full_for_transactions() const;
            bool is_inventory_advertised_to_us_list_full() const;
            bool performing_firewall_check() const;
            fc::optional<fc::ip::endpoint> get_endpoint_for_connecting() const;
        private:
            void send_queued_messages_task();
            void accept_connection_task();
            void connect_to_task(const fc::ip::endpoint& remote_endpoint);
        };
        typedef std::shared_ptr<PeerConnection> PeerConnectionPtr;

    }
} // end namespace thinkyoung::net

// not sent over the wire, just reflected for logging
FC_REFLECT_ENUM(thinkyoung::net::PeerConnection::OurConnectionState, (disconnected)
    (just_connected)
    (connection_accepted)
    (connection_rejected))
    FC_REFLECT_ENUM(thinkyoung::net::PeerConnection::TheirConnectionState, (disconnected)
    (just_connected)
    (connection_accepted)
    (connection_rejected))
    FC_REFLECT_ENUM(thinkyoung::net::PeerConnection::ConnectionNegotiationStatus, (disconnected)
    (connecting)
    (connected)
    (accepting)
    (accepted)
    (hello_sent)
    (peer_connection_accepted)
    (peer_connection_rejected)
    (negotiation_complete)
    (closing)
    (closed))
