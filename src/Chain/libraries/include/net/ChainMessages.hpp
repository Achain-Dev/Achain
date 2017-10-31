#pragma once
#include <fc/reflect/reflect.hpp>
#include <blockchain/Block.hpp>
#include <blockchain/Transaction.hpp>
#include <set>

/*
namespace thinkyoung { namespace net {

enum chain_message_type
{
subscribe_msg = 1,
block_msg     = 2,
trx_msg       = 3,
trx_err_msg   = 4

};

struct subscribe_message
{
static const chain_message_type type;
uint16_t                        version;
thinkyoung::blockchain::block_id_type  last_block;
};

struct block_message
{
static const chain_message_type type;
block_message(){}
block_message( const thinkyoung::blockchain::trx_block& blk )
:block_data(blk){}

thinkyoung::blockchain::trx_block             block_data;
};

struct trx_message
{
static const chain_message_type type;
trx_message(){}
trx_message( const thinkyoung::blockchain::signed_transaction& t ):signed_trx(t){}
thinkyoung::blockchain::signed_transaction    signed_trx;
};

struct trx_err_message
{
static const chain_message_type type;
thinkyoung::blockchain::signed_transaction    signed_trx;
std::string                            err;
};

} } // thinkyoung::net

FC_REFLECT_ENUM( thinkyoung::net::chain_message_type, (subscribe_msg)(block_msg)(trx_msg)(trx_err_msg) )
FC_REFLECT( thinkyoung::net::subscribe_message, (version)(last_block) )
FC_REFLECT( thinkyoung::net::block_message, (block_data) )
FC_REFLECT( thinkyoung::net::trx_message, (signed_trx) )
FC_REFLECT( thinkyoung::net::trx_err_message, (signed_trx)(err) )
*/
