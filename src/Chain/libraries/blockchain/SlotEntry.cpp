#include <blockchain/SlotEntry.hpp>
#include <blockchain/ChainInterface.hpp>
#include <sstream>

namespace thinkyoung {
    namespace blockchain {

        std::string SlotEntry::sqlstr_beging = "INSERT INTO slot_entry VALUES ";
        std::string SlotEntry::sqlstr_ending = " on duplicate key update delegate_id=values(delegate_id), slot_timestamp=values(slot_timestamp);";



        void SlotEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(db.lookup<AccountEntry>(index.delegate_id).valid(), "Slot sanity check failed!");
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oSlotEntry SlotEntry::lookup(const ChainInterface& db, const SlotIndex index)
        {
            try {
                return db.slot_lookup_by_index(index);
            } FC_CAPTURE_AND_RETHROW((index))
        }

        oSlotEntry SlotEntry::lookup(const ChainInterface& db, const time_point_sec timestamp)
        {
            try {
                if (!db.get_statistics_enabled()) return oSlotEntry();
                return db.slot_lookup_by_timestamp(timestamp);
            } FC_CAPTURE_AND_RETHROW((timestamp))
        }

        void SlotEntry::store(ChainInterface& db, const SlotIndex index, const SlotEntry& entry)
        {
            try {
                if (!db.get_statistics_enabled()) return;
                const oSlotEntry prev_entry = db.lookup<SlotEntry>(index);
                if (prev_entry.valid())
                {
                    if (prev_entry->index.timestamp != index.timestamp)
                        db.slot_erase_from_timestamp_map(prev_entry->index.timestamp);
                }

                db.slot_insert_into_index_map(index, entry);
                db.slot_insert_into_timestamp_map(index.timestamp, index.delegate_id);
            } FC_CAPTURE_AND_RETHROW((index)(entry))
        }

        void SlotEntry::remove(ChainInterface& db, const SlotIndex index)
        {
            try {
                if (!db.get_statistics_enabled()) return;
                const oSlotEntry prev_entry = db.lookup<SlotEntry>(index);
                if (prev_entry.valid())
                {
                    db.slot_erase_from_index_map(index);
                    db.slot_erase_from_timestamp_map(prev_entry->index.timestamp);
                }
            } FC_CAPTURE_AND_RETHROW((index))
        }
        std::string SlotEntry::compose_insert_sql()
        {
            std::stringstream sqlss;
            sqlss << sqlstr_beging << "('";
            sqlss << block_id->str() << "',";
            sqlss << index.delegate_id << ",";

            sqlss << "STR_TO_DATE('" << index.timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s') )";//expiration

            sqlss << sqlstr_ending;
            return sqlss.str();
        }
        std::string SlotEntry::compose_insert_sql_value()
        {
            std::stringstream sqlss;
            sqlss << "('";
            sqlss << block_id->str() << "',";
            sqlss << index.delegate_id << ",";

            sqlss << "STR_TO_DATE('" << index.timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s') )";//expiration
            return sqlss.str();
        }
    }
} // thinkyoung::blockchain
