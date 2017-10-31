#pragma once
#include <blockchain/Block.hpp>
#include <client/Client.hpp>

namespace thinkyoung {
    namespace client {

        enum MessageTypeEnum
        {
            trx_message_type = 1000,
            block_message_type = 1001,
            batch_trx_message_type = 1002
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

    }
} // thinkyoung::client

FC_REFLECT_ENUM(thinkyoung::client::MessageTypeEnum, (trx_message_type)(block_message_type)(batch_trx_message_type))
FC_REFLECT(thinkyoung::client::TrxMessage, (trx))
FC_REFLECT(thinkyoung::client::BatchTrxMessage, (trx_vec))
FC_REFLECT(thinkyoung::client::BlockMessage, (block)(block_id))
