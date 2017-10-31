#pragma once

#include <blockchain/Types.hpp>

namespace thinkyoung {
    namespace blockchain {

        enum class PropertyIdType : uint8_t
        {
            database_version = 0,
            chain_id = 1,
            last_asset_id = 2,
            last_account_id = 3,
            active_delegate_list_id = 4,
            last_random_seed_id = 5,
            statistics_enabled = 6,
            /**
            *  N = num delegates
            *  Initial condition = 2N
            *  Every time a block is produced subtract 1
            *  Every time a block is missed add 2
            *  Maximum value is 2N, Min value is 0
            *
            *  Defines how many blocks you must wait to
            *  be 'confirmed' assuming that at least
            *  60% of the blocks in the last 2 rounds
            *  are present. Less than 60% and you
            *  are on the minority chain.
            */
            confirmation_requirement = 7,
            dirty_markets = 8,
            node_vm_enabled = 9
        };

        struct PropertyEntry;
        typedef fc::optional<PropertyEntry> oPropertyEntry;

        class ChainInterface;
        struct PropertyEntry
        {
            PropertyIdType    id;
            variant             value;

            void sanity_check(const ChainInterface&)const;
            static oPropertyEntry lookup(const ChainInterface&, const PropertyIdType);
            static void store(ChainInterface&, const PropertyIdType, const PropertyEntry&);
            static void remove(ChainInterface&, const PropertyIdType);
        };

        class PropertyDbInterface
        {
            friend struct PropertyEntry;

            virtual oPropertyEntry property_lookup_by_id(const PropertyIdType)const = 0;
            virtual void property_insert_into_id_map(const PropertyIdType, const PropertyEntry&) = 0;
            virtual void property_erase_from_id_map(const PropertyIdType) = 0;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT_TYPENAME(thinkyoung::blockchain::PropertyIdType)
FC_REFLECT_ENUM(thinkyoung::blockchain::PropertyIdType,
(database_version)
(chain_id)
(last_asset_id)
(last_account_id)
(active_delegate_list_id)
(last_random_seed_id)
(statistics_enabled)
(confirmation_requirement)
(dirty_markets)
(node_vm_enabled)
);
FC_REFLECT(thinkyoung::blockchain::PropertyEntry,
    (id)
    (value)
    );
