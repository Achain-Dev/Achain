#include <client/Messages.hpp>
namespace thinkyoung {
    namespace client {
        const MessageTypeEnum TrxMessage::type = MessageTypeEnum::trx_message_type;
        const MessageTypeEnum BlockMessage::type = MessageTypeEnum::block_message_type;
        const MessageTypeEnum BatchTrxMessage::type = MessageTypeEnum::batch_trx_message_type;
        const MessageTypeEnum BlockMessage_v2::type = MessageTypeEnum::block_message_type_v2;
        const MessageTypeEnum DelegatesNodeInfoMessage::type = MessageTypeEnum::deles_node_info_message_type;

    }
} // thinkyoung::client
