#pragma once

#include <net/Config.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/network/ip.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/time.hpp>
#include <fc/variant_object.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/enum_type.hpp>


#include <vector>

namespace thinkyoung {
    namespace net {

        typedef fc::ecc::public_key_data NodeIdType;
        typedef fc::ripemd160 ItemHashType;
        struct ItemId
        {
            uint32_t      item_type;
            ItemHashType   item_hash;

            ItemId() {}
            ItemId(uint32_t type, const ItemHashType& hash) :
                item_type(type),
                item_hash(hash)
            {}
            bool operator==(const ItemId& other) const
            {
                return item_type == other.item_type &&
                    item_hash == other.item_hash;
            }
        };

        enum CoreMessageTypeEnum
        {
            trx_message_type = 1000,
            block_message_type = 1001,
            batch_trx_message_type = 1002,
            core_message_type_first = 5000,
            item_ids_inventory_message_type = 5001,
            blockchain_item_ids_inventory_message_type = 5002,
            fetch_blockchain_item_ids_message_type = 5003,
            fetch_items_message_type = 5004,
            item_not_available_message_type = 5005,
            hello_message_type = 5006,
            connection_accepted_message_type = 5007,
            connection_rejected_message_type = 5008,
            address_request_message_type = 5009,
            address_message_type = 5010,
            closing_connection_message_type = 5011,
            current_time_request_message_type = 5012,
            current_time_reply_message_type = 5013,
            check_firewall_message_type = 5014,
            check_firewall_reply_message_type = 5015,
            get_current_connections_request_message_type = 5016,
            get_current_connections_reply_message_type = 5017,
            core_message_type_last = 5099
        };

        const uint32_t core_protocol_version = ALP_NET_PROTOCOL_VERSION;

        struct ItemIdsInventoryMessage
        {
            static const CoreMessageTypeEnum type;

            uint32_t item_type;
            std::vector<ItemHashType> item_hashes_available;

            ItemIdsInventoryMessage() {}
            ItemIdsInventoryMessage(uint32_t item_type, const std::vector<ItemHashType>& item_hashes_available) :
                item_type(item_type),
                item_hashes_available(item_hashes_available)
            {}
        };

        struct BlockchainItemIdsInventoryMessage
        {
            static const CoreMessageTypeEnum type;

            uint32_t total_remaining_item_count;
            uint32_t item_type;
            std::vector<ItemHashType> item_hashes_available;

            BlockchainItemIdsInventoryMessage() {}
            BlockchainItemIdsInventoryMessage(uint32_t total_remaining_item_count,
                uint32_t item_type,
                const std::vector<ItemHashType>& item_hashes_available) :
                total_remaining_item_count(total_remaining_item_count),
                item_type(item_type),
                item_hashes_available(item_hashes_available)
            {}
        };

        struct FetchBlockchainItemIdsMessage
        {
            static const CoreMessageTypeEnum type;

            uint32_t item_type;
            std::vector<ItemHashType> blockchain_synopsis;

            FetchBlockchainItemIdsMessage() {}
            FetchBlockchainItemIdsMessage(uint32_t item_type, const std::vector<ItemHashType>& blockchain_synopsis) :
                item_type(item_type),
                blockchain_synopsis(blockchain_synopsis)
            {}
        };

        struct FetchItemsMessage
        {
            static const CoreMessageTypeEnum type;

            uint32_t item_type;
            std::vector<ItemHashType> items_to_fetch;

            FetchItemsMessage() {}
            FetchItemsMessage(uint32_t item_type, const std::vector<ItemHashType>& items_to_fetch) :
                item_type(item_type),
                items_to_fetch(items_to_fetch)
            {}
        };

        struct ItemNotAvailableMessage
        {
            static const CoreMessageTypeEnum type;

            ItemId requested_item;

            ItemNotAvailableMessage() {}
            ItemNotAvailableMessage(const ItemId& requested_item) :
                requested_item(requested_item)
            {}
        };

        struct HelloMessage
        {
            static const CoreMessageTypeEnum type;

            std::string                user_agent;
            uint32_t                   core_protocol_version;
            fc::ip::address            inbound_address;
            uint16_t                   inbound_port;
            uint16_t                   outbound_port;
            NodeIdType                  node_public_key;
            fc::ecc::compact_signature signed_shared_secret;
            fc::sha256                 chain_id;
            fc::variant_object         user_data;

            HelloMessage() {}
            HelloMessage(const std::string& user_agent,
                uint32_t core_protocol_version,
                const fc::ip::address& inbound_address,
                uint16_t inbound_port,
                uint16_t outbound_port,
                const NodeIdType& node_public_key,
                const fc::ecc::compact_signature& signed_shared_secret,
                const fc::sha256& chain_id_arg,
                const fc::variant_object& user_data) :
                user_agent(user_agent),
                core_protocol_version(core_protocol_version),
                inbound_address(inbound_address),
                inbound_port(inbound_port),
                outbound_port(outbound_port),
                node_public_key(node_public_key),
                signed_shared_secret(signed_shared_secret),
                chain_id(chain_id_arg),
                user_data(user_data)
            {}
        };

        struct ConnectionAcceptedMessage
        {
            static const CoreMessageTypeEnum type;

            ConnectionAcceptedMessage() {}
        };

        enum class RejectionReasonCode {
            unspecified,
            different_chain,
            already_connected,
            connected_to_self,
            not_accepting_connections,
            blocked,
            invalid_hello_message,
            client_too_old
        };

        struct ConnectionRejectedMessage
        {
            static const CoreMessageTypeEnum type;

            std::string                                   user_agent;
            uint32_t                                      core_protocol_version;
            fc::ip::endpoint                              remote_endpoint;
            std::string                                   reason_string;
            fc::enum_type<uint8_t, RejectionReasonCode> reason_code;

            ConnectionRejectedMessage() {}
            ConnectionRejectedMessage(const std::string& user_agent, uint32_t core_protocol_version,
                const fc::ip::endpoint& remote_endpoint, RejectionReasonCode reason_code,
                const std::string& reason_string) :
                user_agent(user_agent),
                core_protocol_version(core_protocol_version),
                remote_endpoint(remote_endpoint),
                reason_string(reason_string),
                reason_code(reason_code)
            {}
        };

        struct AddressRequestMessage
        {
            static const CoreMessageTypeEnum type;

            AddressRequestMessage() {}
        };

        enum class PeerConnectionDirection { unknown, inbound, outbound };
        enum class FirewalledState { unknown, firewalled, not_firewalled };

        struct AddressInfo
        {
            fc::ip::endpoint          remote_endpoint;
            fc::time_point_sec        last_seen_time;
            fc::microseconds          latency;
            NodeIdType                 node_id;
            fc::enum_type<uint8_t, PeerConnectionDirection> direction;
            fc::enum_type<uint8_t, FirewalledState> firewalled;

            AddressInfo() {}
            AddressInfo(const fc::ip::endpoint& remote_endpoint,
                const fc::time_point_sec last_seen_time,
                const fc::microseconds latency,
                const NodeIdType& node_id,
                PeerConnectionDirection direction,
                FirewalledState firewalled) :
                remote_endpoint(remote_endpoint),
                last_seen_time(last_seen_time),
                latency(latency),
                node_id(node_id),
                direction(direction),
                firewalled(firewalled)
            {}
        };

        struct AddressMessage
        {
            static const CoreMessageTypeEnum type;

            std::vector<AddressInfo> addresses;
        };

        struct ClosingConnectionMessage
        {
            static const CoreMessageTypeEnum type;

            std::string        reason_for_closing;
            bool               closing_due_to_error;
            fc::oexception     error;

            ClosingConnectionMessage() : closing_due_to_error(false) {}
            ClosingConnectionMessage(const std::string& reason_for_closing,
                bool closing_due_to_error = false,
                const fc::oexception& error = fc::oexception()) :
                reason_for_closing(reason_for_closing),
                closing_due_to_error(closing_due_to_error),
                error(error)
            {}
        };

        struct CurrentTimeRequestMessage
        {
            static const CoreMessageTypeEnum type;
            fc::time_point request_sent_time;

            CurrentTimeRequestMessage(){}
            CurrentTimeRequestMessage(const fc::time_point request_sent_time) :
                request_sent_time(request_sent_time)
            {}
        };

        struct CurrentTimeReplyMessage
        {
            static const CoreMessageTypeEnum type;
            fc::time_point request_sent_time;
            fc::time_point request_received_time;
            fc::time_point reply_transmitted_time;

            CurrentTimeReplyMessage(){}
            CurrentTimeReplyMessage(const fc::time_point request_sent_time,
                const fc::time_point request_received_time,
                const fc::time_point reply_transmitted_time = fc::time_point()) :
                request_sent_time(request_sent_time),
                request_received_time(request_received_time),
                reply_transmitted_time(reply_transmitted_time)
            {}
        };

        struct CheckFirewallMessage
        {
            static const CoreMessageTypeEnum type;
            NodeIdType node_id;
            fc::ip::endpoint endpoint_to_check;
        };

        enum class FirewallCheckResult
        {
            unable_to_check,
            unable_to_connect,
            connection_successful
        };

        struct CheckFirewallReplyMessage
        {
            static const CoreMessageTypeEnum type;
            NodeIdType node_id;
            fc::ip::endpoint endpoint_checked;
            fc::enum_type<uint8_t, FirewallCheckResult> result;
        };

        struct GetCurrentConnectionsRequestMessage
        {
            static const CoreMessageTypeEnum type;
        };

        struct CurrentConnectionData
        {
            uint32_t           connection_duration; // in seconds
            fc::ip::endpoint   remote_endpoint;
            NodeIdType          node_id;
            fc::microseconds   clock_offset;
            fc::microseconds   round_trip_delay;
            fc::enum_type<uint8_t, PeerConnectionDirection> connection_direction;
            fc::enum_type<uint8_t, FirewalledState> firewalled;
            fc::variant_object user_data;
        };

        struct GetCurrentConnectionsReplyMessage
        {
            static const CoreMessageTypeEnum type;
            uint32_t upload_rate_one_minute;
            uint32_t download_rate_one_minute;
            uint32_t upload_rate_fifteen_minutes;
            uint32_t download_rate_fifteen_minutes;
            uint32_t upload_rate_one_hour;
            uint32_t download_rate_one_hour;
            std::vector<CurrentConnectionData> current_connections;
        };


    }
} // thinkyoung::client

FC_REFLECT_ENUM(thinkyoung::net::CoreMessageTypeEnum,
    (trx_message_type)
    (block_message_type)
    (batch_trx_message_type)
    (core_message_type_first)
    (item_ids_inventory_message_type)
    (blockchain_item_ids_inventory_message_type)
    (fetch_blockchain_item_ids_message_type)
    (fetch_items_message_type)
    (item_not_available_message_type)
    (hello_message_type)
    (connection_accepted_message_type)
    (connection_rejected_message_type)
    (address_request_message_type)
    (address_message_type)
    (closing_connection_message_type)
    (current_time_request_message_type)
    (current_time_reply_message_type)
    (check_firewall_message_type)
    (check_firewall_reply_message_type)
    (get_current_connections_request_message_type)
    (get_current_connections_reply_message_type)
    (core_message_type_last))
    FC_REFLECT(thinkyoung::net::ItemId, (item_type)
    (item_hash))
    FC_REFLECT(thinkyoung::net::ItemIdsInventoryMessage, (item_type)
    (item_hashes_available))
    FC_REFLECT(thinkyoung::net::BlockchainItemIdsInventoryMessage, (total_remaining_item_count)
    (item_type)
    (item_hashes_available))
    FC_REFLECT(thinkyoung::net::FetchBlockchainItemIdsMessage, (item_type)
    (blockchain_synopsis))
    FC_REFLECT(thinkyoung::net::FetchItemsMessage, (item_type)
    (items_to_fetch))
    FC_REFLECT(thinkyoung::net::ItemNotAvailableMessage, (requested_item))
    FC_REFLECT(thinkyoung::net::HelloMessage, (user_agent)
    (core_protocol_version)
    (inbound_address)
    (inbound_port)
    (outbound_port)
    (node_public_key)
    (signed_shared_secret)
    (chain_id)
    (user_data))

    FC_REFLECT_EMPTY(thinkyoung::net::ConnectionAcceptedMessage)
    FC_REFLECT_ENUM(thinkyoung::net::RejectionReasonCode, (unspecified)
    (different_chain)
    (already_connected)
    (connected_to_self)
    (not_accepting_connections)
    (blocked)
    (invalid_hello_message)
    (client_too_old))
    FC_REFLECT(thinkyoung::net::ConnectionRejectedMessage, (user_agent)
    (core_protocol_version)
    (remote_endpoint)
    (reason_code)
    (reason_string))
    FC_REFLECT_EMPTY(thinkyoung::net::AddressRequestMessage)
    FC_REFLECT(thinkyoung::net::AddressInfo, (remote_endpoint)
    (last_seen_time)
    (latency)
    (node_id)
    (direction)
    (firewalled))
    FC_REFLECT(thinkyoung::net::AddressMessage, (addresses))
    FC_REFLECT(thinkyoung::net::ClosingConnectionMessage, (reason_for_closing)
    (closing_due_to_error)
    (error))
    FC_REFLECT_ENUM(thinkyoung::net::PeerConnectionDirection, (unknown)
    (inbound)
    (outbound))
    FC_REFLECT_ENUM(thinkyoung::net::FirewalledState, (unknown)
    (firewalled)
    (not_firewalled))

    FC_REFLECT(thinkyoung::net::CurrentTimeRequestMessage, (request_sent_time))
    FC_REFLECT(thinkyoung::net::CurrentTimeReplyMessage, (request_sent_time)
    (request_received_time)
    (reply_transmitted_time))
    FC_REFLECT_ENUM(thinkyoung::net::FirewallCheckResult, (unable_to_check)
    (unable_to_connect)
    (connection_successful))
    FC_REFLECT(thinkyoung::net::CheckFirewallMessage, (node_id)(endpoint_to_check))
    FC_REFLECT(thinkyoung::net::CheckFirewallReplyMessage, (node_id)(endpoint_checked)(result))
    FC_REFLECT_EMPTY(thinkyoung::net::GetCurrentConnectionsRequestMessage)
    FC_REFLECT(thinkyoung::net::CurrentConnectionData, (connection_duration)
    (remote_endpoint)
    (node_id)
    (clock_offset)
    (round_trip_delay)
    (connection_direction)
    (firewalled)
    (user_data))
    FC_REFLECT(thinkyoung::net::GetCurrentConnectionsReplyMessage, (upload_rate_one_minute)
    (download_rate_one_minute)
    (upload_rate_fifteen_minutes)
    (download_rate_fifteen_minutes)
    (upload_rate_one_hour)
    (download_rate_one_hour)
    (current_connections))

#include <unordered_map>
#include <fc/crypto/city.hpp>
namespace std
{
    template<>
    struct hash < thinkyoung::net::ItemId >
    {
        size_t operator()(const thinkyoung::net::ItemId& item_to_hash) const
        {
            return fc::city_hash_size_t((char*)&item_to_hash, sizeof(item_to_hash));
        }
    };
}
