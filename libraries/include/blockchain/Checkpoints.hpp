#pragma once
#include <blockchain/Types.hpp>

namespace thinkyoung {
    namespace blockchain {
        namespace detail{

            static std::map<uint32_t, thinkyoung::blockchain::BlockIdType> CHECKPOINT_BLOCKS
            {
                //{ 1246900, thinkyoung::blockchain::block_id_type( "b5214c1bc914ea5da6c1bb8f774d07f12b85dbe2" ) },
                //{ 1279500, thinkyoung::blockchain::block_id_type( "488313d2c1c85bdec117bac0379f7b17f7cfbed3" ) }
            };
            // Initialized in load_checkpoints()
            static uint32_t LAST_CHECKPOINT_BLOCK_NUM = 0;

        }
    }
} // thinkyoung::blockchain
