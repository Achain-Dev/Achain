#include <blockchain/ChainInterface.hpp>
#include <blockchain/PropertyEntry.hpp>

namespace thinkyoung {
    namespace blockchain {

        void PropertyEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(!value.is_null());
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oPropertyEntry PropertyEntry::lookup(const ChainInterface& db, const PropertyIdType id)
        {
            try {
                return db.property_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void PropertyEntry::store(ChainInterface& db, const PropertyIdType id, const PropertyEntry& entry)
        {
            try {
                db.property_insert_into_id_map(id, entry);
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void PropertyEntry::remove(ChainInterface& db, const PropertyIdType id)
        {
            try {
                const oPropertyEntry prev_entry = db.lookup<PropertyEntry>(id);
                if (prev_entry.valid())
                    db.property_erase_from_id_map(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

    }
} // thinkyoung::blockchain
