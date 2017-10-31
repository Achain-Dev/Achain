#pragma once

#include <blockchain/Transaction.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct BlockHeader
        {
            DigestType  digest()const;

            BlockIdType        previous;
            uint32_t             block_num = 0;
            fc::time_point_sec   timestamp;
            DigestType          transaction_digest;
            /** used for random number generation on the blockchain */
            SecretHashType     next_secret_hash;
            SecretHashType     previous_secret;
        };

        struct SignedBlockHeader : public BlockHeader
        {
            BlockIdType    id()const;
            bool             validate_signee(const fc::ecc::public_key& expected_signee, bool enforce_canonical = false)const;
            PublicKeyType  signee(bool enforce_canonical = false)const;
            void             sign(const fc::ecc::private_key& signer);

            SignatureType delegate_signature;
        };

        struct FullBlock : public SignedBlockHeader
        {
            size_t               block_size()const;

            signed_transactions  user_transactions;
        };

        struct DigestBlock : public SignedBlockHeader
        {
            DigestBlock(){}
            DigestBlock(const FullBlock& block_data);

            DigestType                      calculate_transaction_digest()const;
            bool                             validate_digest()const;
            bool                             validate_unique()const;

            std::vector<TransactionIdType> user_transaction_ids;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::BlockHeader,
    (previous)(block_num)(timestamp)(transaction_digest)(next_secret_hash)(previous_secret))
    FC_REFLECT_DERIVED(thinkyoung::blockchain::SignedBlockHeader, (thinkyoung::blockchain::BlockHeader), (delegate_signature))
    FC_REFLECT_DERIVED(thinkyoung::blockchain::FullBlock, (thinkyoung::blockchain::SignedBlockHeader), (user_transactions))
    FC_REFLECT_DERIVED(thinkyoung::blockchain::DigestBlock, (thinkyoung::blockchain::SignedBlockHeader), (user_transaction_ids))
