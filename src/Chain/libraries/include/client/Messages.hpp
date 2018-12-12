#pragma once
#include <blockchain/Block.hpp>
#include <client/Client.hpp>

namespace thinkyoung {
    namespace client {

        enum MessageTypeEnum
        {
            trx_message_type = 1000,
            block_message_type = 1001,
            batch_trx_message_type = 1002,
            block_message_type_v2 = 1003,
            deles_node_info_message_type = 1004
        };

        struct TrxMessage
        {
            static const MessageTypeEnum type;

            thinkyoung::blockchain::SignedTransaction trx;
            TrxMessage() {}
            TrxMessage(thinkyoung::blockchain::SignedTransaction transaction) :
                trx(std::move(transaction))
            {}
        };

        struct BatchTrxMessage
        {
            static const MessageTypeEnum type;
            std::vector<thinkyoung::blockchain::SignedTransaction> trx_vec;
            BatchTrxMessage() {}
            BatchTrxMessage(std::vector<thinkyoung::blockchain::SignedTransaction> transactions) :
                trx_vec(std::move(transactions))
            {}
        };

        struct BlockMessage
        {
            static const MessageTypeEnum type;

            BlockMessage(){}
            BlockMessage(const thinkyoung::blockchain::FullBlock& blk)
                :block(blk), block_id(blk.id()){}			

            thinkyoung::blockchain::FullBlock    block;
            thinkyoung::blockchain::BlockIdType block_id;

        };

		struct BlockMessage_v2
        {
            static const MessageTypeEnum type;

            BlockMessage_v2(){}

            BlockMessage_v2(const BlockMessage& message_1)
                :block(message_1.block), block_id(message_1.block_id){}

			BlockMessage_v2(const thinkyoung::blockchain::FullBlock_v2& blk)
                :block(blk), block_id(blk.id()){}

            thinkyoung::blockchain::FullBlock_v2    block;
            thinkyoung::blockchain::BlockIdType block_id;

        };

        struct DelegatesNodeInfoMessage
        {
            static const MessageTypeEnum type;
            std::map<AccountIdType, thinkyoung::blockchain::DelegateNodeInfo> deleinfo_map;
            DelegatesNodeInfoMessage() {}
            DelegatesNodeInfoMessage(std::map<AccountIdType,thinkyoung::blockchain::DelegateNodeInfo> dele_node_info_map) :
                deleinfo_map(std::move(dele_node_info_map))
            {}
        };
    }
} // thinkyoung::client

FC_REFLECT_ENUM(thinkyoung::client::MessageTypeEnum, (trx_message_type)(block_message_type)(batch_trx_message_type)(block_message_type_v2))
FC_REFLECT(thinkyoung::client::TrxMessage, (trx))
FC_REFLECT(thinkyoung::client::BatchTrxMessage, (trx_vec))
FC_REFLECT(thinkyoung::client::BlockMessage, (block)(block_id))
FC_REFLECT(thinkyoung::client::BlockMessage_v2, (block)(block_id))
