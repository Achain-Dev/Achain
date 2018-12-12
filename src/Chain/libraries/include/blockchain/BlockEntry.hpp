#pragma once
#include <blockchain/Block.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct BlockEntry_v2 : public thinkyoung::blockchain::DigestBlock_v2
        {
            BlockIdType       id;
            uint64_t            block_size = 0; /* Bytes */
            fc::microseconds    latency; /* Time between block timestamp and first push_block */

            ShareType          signee_shares_issued = 0;
            ShareType          signee_fees_collected = 0;
            ShareType          signee_fees_destroyed = 0;
            fc::ripemd160       random_seed;

            fc::microseconds    processing_time; /* Time taken for extend_chain to run */
        };
        typedef optional<BlockEntry_v2> oBlockEntry_v2;

        struct BlockEntry : public thinkyoung::blockchain::DigestBlock
        {

            uint64_t            block_size = 0; /* Bytes */
            fc::microseconds    latency; /* Time between block timestamp and first push_block */

            ShareType           signee_shares_issued = 0;
            ShareType           signee_fees_collected = 0;
            ShareType           signee_fees_destroyed = 0;
            fc::ripemd160       random_seed;
            fc::time_point_sec  syc_timestamp;

            fc::microseconds    processing_time; /* Time taken for extend_chain to run */

            std::string  compose_insert_sql();
            std::string  compose_insert_sql_value();
        };


        typedef optional<BlockEntry> oBlockEntry;

		struct BlockEntrySigneeinfo
		{
			BlockEntrySigneeinfo(ShareType shares_issued, ShareType fees_collected, ShareType fees_destroyed)
				:signee_shares_issued(shares_issued), 
				signee_fees_collected(fees_collected),
				signee_fees_destroyed(fees_destroyed){}
				
			ShareType          signee_shares_issued = 0;
            ShareType          signee_fees_collected = 0;
            ShareType          signee_fees_destroyed = 0;
		};
    }
} // thinkyoung::blockchain

FC_REFLECT_DERIVED(thinkyoung::blockchain::BlockEntry,
    (thinkyoung::blockchain::DigestBlock),
    (id)
    (block_size)
    (latency)
    (signee_shares_issued)
    (signee_fees_collected)
    (signee_fees_destroyed)
    (random_seed)
    (processing_time)
    )
FC_REFLECT_DERIVED(thinkyoung::blockchain::BlockEntry_v2,
	(thinkyoung::blockchain::DigestBlock_v2),
	(id)
	(block_size)
	(latency)
	(signee_shares_issued)
	(signee_fees_collected)
	(signee_fees_destroyed)
	(random_seed)
	(processing_time)
	)

