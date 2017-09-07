#pragma once
#include <blockchain/Types.hpp>
#include <fc/time.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct SlotIndex
        {
            AccountIdType delegate_id;
            time_point_sec  timestamp;

            SlotIndex() {}
            SlotIndex(const AccountIdType id, const time_point_sec t)
                : delegate_id(id), timestamp(t) {}

            friend bool operator < (const SlotIndex& a, const SlotIndex& b)
            {
                // Reverse because newer slot entrys are what people care about
                return std::tie(a.delegate_id, a.timestamp) > std::tie(b.delegate_id, b.timestamp);
            }

            friend bool operator == (const SlotIndex& a, const SlotIndex& b)
            {
                return std::tie(a.delegate_id, a.timestamp) == std::tie(b.delegate_id, b.timestamp);
            }
        };

        struct SlotEntry;
        typedef fc::optional<SlotEntry> oSlotEntry;

        class ChainInterface;
        struct SlotEntry
        {
            SlotEntry(){}
            SlotEntry(const time_point_sec t, const AccountIdType d, const optional<BlockIdType>& b = optional<BlockIdType>())
                : index(d, t), block_id(b) {}

            SlotIndex              index;
            optional<BlockIdType> block_id;

            void sanity_check(const ChainInterface&)const;
            static oSlotEntry lookup(const ChainInterface&, const SlotIndex);
            static oSlotEntry lookup(const ChainInterface&, const time_point_sec);
            static void store(ChainInterface&, const SlotIndex, const SlotEntry&);
            static void remove(ChainInterface&, const SlotIndex);
        };

        class SlotDbInterface
        {
            friend struct SlotEntry;

            virtual oSlotEntry slot_lookup_by_index(const SlotIndex)const = 0;
            virtual oSlotEntry slot_lookup_by_timestamp(const time_point_sec)const = 0;

            virtual void slot_insert_into_index_map(const SlotIndex, const SlotEntry&) = 0;
            virtual void slot_insert_into_timestamp_map(const time_point_sec, const AccountIdType) = 0;

            virtual void slot_erase_from_index_map(const SlotIndex) = 0;
            virtual void slot_erase_from_timestamp_map(const time_point_sec) = 0;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::SlotIndex,
    (delegate_id)
    (timestamp)
    )

    FC_REFLECT(thinkyoung::blockchain::SlotEntry,
    (index)
    (block_id)
    )
