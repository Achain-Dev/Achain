#include <blockchain/Block.hpp>
#include <algorithm>

namespace thinkyoung {
    namespace blockchain {

        //BlockHeader
        DigestType BlockHeader::digest()const
        {
            fc::sha256::encoder enc;
            fc::raw::pack(enc, *this);
            return enc.result();
        }


        //SignedBlockHeader

		SignedBlockHeader::SignedBlockHeader(SignedBlockHeader_v2 sign_v2)
        {
        	(BlockHeader&)*this = sign_v2;

			delegate_signature = sign_v2.delegate_signature[0].signature;
        }
		
        BlockIdType SignedBlockHeader::id()const
        {
            fc::sha512::encoder enc;
            fc::raw::pack(enc, *this);
            return fc::ripemd160::hash(enc.result());
        }

        bool SignedBlockHeader::validate_signee(const fc::ecc::public_key& expected_signee, bool enforce_canonical)const
        {
            return fc::ecc::public_key(delegate_signature, digest(), enforce_canonical) == expected_signee;
        }

        PublicKeyType SignedBlockHeader::signee(bool enforce_canonical)const
        {
            return fc::ecc::public_key(delegate_signature, digest(), enforce_canonical);
        }

        void SignedBlockHeader::sign(const fc::ecc::private_key& signer)
        {
            try {
                delegate_signature = signer.sign_compact(digest());
            } FC_RETHROW_EXCEPTIONS(warn, "")
        }

        //SignedBlockHeader_v2

		SignedBlockHeader_v2::SignedBlockHeader_v2(SignedBlockHeader sign)
		{
			(BlockHeader&)(*this) = sign;
			this->delegate_signature.push_back(DeleSign(" ",sign.delegate_signature));
			this->delegate_pos = 0;
		}
		
        BlockIdType SignedBlockHeader_v2::id()const
        {
        	if (block_num < ALP_BLOCKCHAIN_V2_FORK_BLOCK_NUM)
        	{
        		SignedBlockHeader head_1(*this);

				return head_1.id();
        	}
            fc::sha512::encoder enc;
            fc::raw::pack(enc, *this);
            return fc::ripemd160::hash(enc.result());
        }

        // delegate sign
        bool SignedBlockHeader_v2::validate_signee(const fc::ecc::public_key& expected_signee, bool enforce_canonical)const
        {
            return fc::ecc::public_key(delegate_signature[0].signature, digest(), enforce_canonical) == expected_signee;
            //return fc::ecc::public_key(delegate_signature.begin()->signature, digest(), enforce_canonical) == expected_signee;
        }
        // delegate sign
        PublicKeyType SignedBlockHeader_v2::signee(uint32_t index, bool enforce_canonical)const
        {
            return fc::ecc::public_key(delegate_signature[index].signature, digest(), enforce_canonical);
        }

        // delegate sign
        //void SignedBlockHeader::sign(const fc::ecc::private_key& signer)
        void SignedBlockHeader_v2::sign(std::string dele_name, const fc::ecc::private_key& signer)
        {
            try {
                DeleSign dele_sign(dele_name, signer.sign_compact(digest()));
                    //delegate_signature[0] is the block maker
                delegate_signature.push_back(dele_sign);
                //delegate_signature.push_back(dele_sign);
            } FC_RETHROW_EXCEPTIONS(warn, "")
        }

        //FullBlock
        size_t FullBlock::block_size()const
        {
            fc::datastream<size_t> ds;
            fc::raw::pack(ds, *this);
            return ds.tellp();
        }

		FullBlock::FullBlock(const FullBlock_v2& block_data)
			:user_transactions(block_data.user_transactions)
        {
        	(BlockHeader&)*this = block_data;
            if (block_data.delegate_signature.size() > 0)
            {
                this->delegate_signature = block_data.delegate_signature[0].signature;
            }
        }

        DigestBlock::DigestBlock(const FullBlock& block_data)
        {
            (SignedBlockHeader&)*this = block_data;
            user_transaction_ids.reserve(block_data.user_transactions.size());
            for (const auto& item : block_data.user_transactions)
                user_transaction_ids.push_back(item.id());
        }

		DigestBlock::DigestBlock(const FullBlock_v2& block_data)
        {
            (BlockHeader&)*this = block_data;
            user_transaction_ids.reserve(block_data.user_transactions.size());
            for (const auto& item : block_data.user_transactions)
                user_transaction_ids.push_back(item.id());

			this->delegate_signature = block_data.delegate_signature[0].signature;
        }

        DigestBlock::DigestBlock(const DigestBlock_v2& block_data)
            :user_transaction_ids(block_data.user_transaction_ids)
        {
            (BlockHeader&)*this = block_data;
            this->delegate_signature = block_data.delegate_signature[0].signature;
        }

        DigestType DigestBlock::calculate_transaction_digest()const
        {
            fc::sha512::encoder enc;
            fc::raw::pack(enc, user_transaction_ids);
            return fc::sha256::hash(enc.result());
        }

        bool DigestBlock::validate_digest()const
        {
            return calculate_transaction_digest() == transaction_digest;
        }

        bool DigestBlock::validate_unique()const
        {
            std::unordered_set<TransactionIdType> trx_ids;
            for (const auto& id : user_transaction_ids)
                if (!trx_ids.insert(id).second) return false;
            return true;
        }

        ////v2
		size_t FullBlock_v2::block_size()const
        {
        	if (block_num < ALP_BLOCKCHAIN_V2_FORK_BLOCK_NUM)
        	{
        		FullBlock block_1(*this);

				return block_1.block_size();
        	}
			
            fc::datastream<size_t> ds;
            fc::raw::pack(ds, *this);
            return ds.tellp();
        }

		FullBlock_v2::FullBlock_v2(const FullBlock& block_data)
			:user_transactions(block_data.user_transactions)
		{
			(BlockHeader&)*this = block_data;
			this->delegate_signature.reserve(50);
            this->delegate_signature.push_back(DeleSign("", block_data.delegate_signature));
			this->delegate_pos = 0;
		}
		
        DigestBlock_v2::DigestBlock_v2(const FullBlock_v2& block_data)
        {
            (SignedBlockHeader_v2&)*this = block_data;
            user_transaction_ids.reserve(block_data.user_transactions.size());
            for (const auto& item : block_data.user_transactions)
                user_transaction_ids.push_back(item.id());
        }

        DigestType DigestBlock_v2::calculate_transaction_digest()const
        {
            fc::sha512::encoder enc;
            fc::raw::pack(enc, user_transaction_ids);
            return fc::sha256::hash(enc.result());
        }

        bool DigestBlock_v2::validate_digest()const
        {
            return calculate_transaction_digest() == transaction_digest;
        }

        bool DigestBlock_v2::validate_unique()const
        {
            std::unordered_set<TransactionIdType> trx_ids;
            for (const auto& id : user_transaction_ids)
                if (!trx_ids.insert(id).second) return false;
            return true;
        }

    }
} // thinkyoung::blockchain
