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
            BlockEntry(){}
            BlockEntry(const BlockEntry_v2& entry_v2)
                :DigestBlock(entry_v2),
                id(entry_v2.id), block_size(entry_v2.block_size),
                latency(entry_v2.latency), 
                signee_shares_issued(entry_v2.signee_shares_issued),
                signee_fees_collected(entry_v2.signee_fees_collected),
                signee_fees_destroyed(entry_v2.signee_fees_destroyed),
                random_seed(entry_v2.random_seed),
                processing_time(entry_v2.processing_time)
            {
            }
            BlockIdType       id;
            uint64_t            block_size = 0; /* Bytes */
            fc::microseconds    latency; /* Time between block timestamp and first push_block */

            ShareType          signee_shares_issued = 0;
            ShareType          signee_fees_collected = 0;
            ShareType          signee_fees_destroyed = 0;
            fc::ripemd160       random_seed;

            fc::microseconds    processing_time; /* Time taken for extend_chain to run */
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

