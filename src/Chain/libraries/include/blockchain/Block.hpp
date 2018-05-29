#pragma once

#include <blockchain/Transaction.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct BlockHeader
        {
            BlockHeader(){}
            DigestType  digest()const;

            BlockIdType        previous;
            uint32_t             block_num = 0;
            fc::time_point_sec   timestamp;
            DigestType          transaction_digest;
            /** used for random number generation on the blockchain */
            SecretHashType     next_secret_hash;
            SecretHashType     previous_secret;
        };

        struct SignedBlockHeader_v2;
        struct SignedBlockHeader : public BlockHeader
        {
            SignedBlockHeader(){}
        	SignedBlockHeader(SignedBlockHeader_v2 sign_v2);
            BlockIdType    id()const;
            bool             validate_signee(const fc::ecc::public_key& expected_signee, bool enforce_canonical = false)const;
            PublicKeyType  signee(bool enforce_canonical = false)const;
            void             sign(const fc::ecc::private_key& signer);

            SignatureType delegate_signature;
        };

        struct SignedBlockHeader_v2 : public BlockHeader
        {
            SignedBlockHeader_v2(){}
			SignedBlockHeader_v2(SignedBlockHeader sign);
            BlockIdType    id()const;
            bool             validate_signee(const fc::ecc::public_key& expected_signee, bool enforce_canonical = false)const;
            PublicKeyType  signee(uint32_t index = 0, bool enforce_canonical = false)const;
            void             sign(std::string dele_name, const fc::ecc::private_key& signer);

            //block maker 's index at active_delegates
            uint32_t     delegate_pos;
            // delegate sign
            //SignatureType delegate_signature;
            //the delegate_signature[0] is the delegate who generate this block
            //delegate_signature[1] ... delegate_signature[n] are the delegates who sign this block
            DelegateSignatures   delegate_signature;
        };
        
        struct FullBlock_v2;
        struct FullBlock : public SignedBlockHeader
        {
            FullBlock(){}
            FullBlock(const FullBlock_v2& block_data);
            size_t               block_size()const;
            signed_transactions  user_transactions;
        };
        struct DigestBlock_v2;
        struct DigestBlock : public SignedBlockHeader
        {
            DigestBlock(){}
            DigestBlock(const FullBlock& block_data);

			DigestBlock(const FullBlock_v2& block_data);
            DigestBlock(const DigestBlock_v2& block_data);

            DigestType                      calculate_transaction_digest()const;
            bool                             validate_digest()const;
            bool                             validate_unique()const;

            std::vector<TransactionIdType> user_transaction_ids;
        };

        struct FullBlock_v2 : public SignedBlockHeader_v2
        {
            FullBlock_v2(){}
        	FullBlock_v2(const FullBlock& block_data);
            size_t               block_size()const;

            signed_transactions  user_transactions;
        };

        struct DigestBlock_v2 : public SignedBlockHeader_v2
        {
            DigestBlock_v2(){}
            DigestBlock_v2(const FullBlock_v2& block_data);

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

    FC_REFLECT_DERIVED(thinkyoung::blockchain::SignedBlockHeader_v2, (thinkyoung::blockchain::BlockHeader), (delegate_pos)(delegate_signature))
    FC_REFLECT_DERIVED(thinkyoung::blockchain::FullBlock_v2, (thinkyoung::blockchain::SignedBlockHeader_v2), (user_transactions))
    FC_REFLECT_DERIVED(thinkyoung::blockchain::DigestBlock_v2, (thinkyoung::blockchain::SignedBlockHeader_v2), (user_transaction_ids))