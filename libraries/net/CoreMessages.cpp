#include <net/CoreMessages.hpp>

#include <client/Messages.hpp>

namespace thinkyoung {
    namespace net {

        const CoreMessageTypeEnum ItemIdsInventoryMessage::type = CoreMessageTypeEnum::item_ids_inventory_message_type;
        const CoreMessageTypeEnum BlockchainItemIdsInventoryMessage::type = CoreMessageTypeEnum::blockchain_item_ids_inventory_message_type;
        const CoreMessageTypeEnum FetchBlockchainItemIdsMessage::type = CoreMessageTypeEnum::fetch_blockchain_item_ids_message_type;
        const CoreMessageTypeEnum FetchItemsMessage::type = CoreMessageTypeEnum::fetch_items_message_type;
        const CoreMessageTypeEnum ItemNotAvailableMessage::type = CoreMessageTypeEnum::item_not_available_message_type;
        const CoreMessageTypeEnum HelloMessage::type = CoreMessageTypeEnum::hello_message_type;
        const CoreMessageTypeEnum ConnectionAcceptedMessage::type = CoreMessageTypeEnum::connection_accepted_message_type;
        const CoreMessageTypeEnum ConnectionRejectedMessage::type = CoreMessageTypeEnum::connection_rejected_message_type;
        const CoreMessageTypeEnum AddressRequestMessage::type = CoreMessageTypeEnum::address_request_message_type;
        const CoreMessageTypeEnum AddressMessage::type = CoreMessageTypeEnum::address_message_type;
        const CoreMessageTypeEnum ClosingConnectionMessage::type = CoreMessageTypeEnum::closing_connection_message_type;
        const CoreMessageTypeEnum CurrentTimeRequestMessage::type = CoreMessageTypeEnum::current_time_request_message_type;
        const CoreMessageTypeEnum CurrentTimeReplyMessage::type = CoreMessageTypeEnum::current_time_reply_message_type;
        const CoreMessageTypeEnum CheckFirewallMessage::type = CoreMessageTypeEnum::check_firewall_message_type;
        const CoreMessageTypeEnum CheckFirewallReplyMessage::type = CoreMessageTypeEnum::check_firewall_reply_message_type;
        const CoreMessageTypeEnum GetCurrentConnectionsRequestMessage::type = CoreMessageTypeEnum::get_current_connections_request_message_type;
        const CoreMessageTypeEnum GetCurrentConnectionsReplyMessage::type = CoreMessageTypeEnum::get_current_connections_reply_message_type;

    }
} // thinkyoung::client

static_assert((int)thinkyoung::net::block_message_type == (int)thinkyoung::client::block_message_type, "enum values don't match");
static_assert((int)thinkyoung::net::trx_message_type == (int)thinkyoung::client::trx_message_type, "enum values don't match");
