#include <blockchain/ChainInterface.hpp>
#include <blockchain/SlateEntry.hpp>

#include <fc/crypto/sha256.hpp>
#include <fc/io/raw.hpp>

namespace thinkyoung {
    namespace blockchain {

        SlateIdType SlateEntry::id()const
        {
            if (slate.empty()) return 0;
            fc::sha256::encoder enc;
            fc::raw::pack(enc, slate);
            return enc.result()._hash[0];
        }

        void SlateEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(!slate.empty());
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oSlateEntry SlateEntry::lookup(const ChainInterface& db, const SlateIdType id)
        {
            try {
                return db.slate_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void SlateEntry::store(ChainInterface& db, const SlateIdType id, const SlateEntry& entry)
        {
            try {
                db.slate_insert_into_id_map(id, entry);
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void SlateEntry::remove(ChainInterface& db, const SlateIdType id)
        {
            try {
                const oSlateEntry prev_entry = db.lookup<SlateEntry>(id);
                if (prev_entry.valid())
                    db.slate_erase_from_id_map(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

    }
} // thinkyoung::blockchain
