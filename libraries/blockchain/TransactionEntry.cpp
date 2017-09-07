#include <blockchain/ChainInterface.hpp>
#include <blockchain/TransactionEntry.hpp>

namespace thinkyoung {
    namespace blockchain {

        void TransactionEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(!trx.reserved.valid());
                FC_ASSERT(!trx.operations.empty());
                FC_ASSERT(!trx.signatures.empty());
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oTransactionEntry TransactionEntry::lookup(const ChainInterface& db, const TransactionIdType& id)
        {
            try {
                return db.transaction_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void TransactionEntry::store(ChainInterface& db, const TransactionIdType& id, const TransactionEntry& entry)
        {
            try {
                db.transaction_insert_into_id_map(id, entry);
                db.transaction_insert_into_unique_set(entry.trx);
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void TransactionEntry::remove(ChainInterface& db, const TransactionIdType& id)
        {
            try {
                const oTransactionEntry prev_entry = db.lookup<TransactionEntry>(id);
                if (prev_entry.valid())
                {
                    db.transaction_erase_from_id_map(id);
                    db.transaction_erase_from_unique_set(prev_entry->trx);
                }
            } FC_CAPTURE_AND_RETHROW((id))
        }

    }
} // thinkyoung::blockchain
