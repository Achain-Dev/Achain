#include <blockchain/Address.hpp>
#include <blockchain/WithdrawTypes.hpp>
#include <blockchain/Exceptions.hpp>
#include <fc/crypto/base58.hpp>
#include <algorithm>

namespace thinkyoung {
    namespace blockchain {
        Address::Address(){}

        Address::Address(const std::string& base58str, const AddressType& address_type)
        {
            std::string prefix;
            if (address_type == AddressType::alp_address)
                prefix = ALP_ADDRESS_PREFIX;
            else if (address_type == AddressType::contract_address)
                prefix = CONTRACT_ADDRESS_PREFIX;
            else if (address_type == script_id)
                prefix = SCRIPT_ID_PREFIX;
            if(!is_valid(base58str, prefix))
				FC_CAPTURE_AND_THROW(invalid_address,  ( base58str));

            std::vector<char> v = fc::from_base58(base58str.substr(prefix.size()));
            memcpy((char*)addr._hash, v.data(), std::min<size_t>(v.size() - 4, sizeof(addr)));
        }

        Address::Address(const WithdrawCondition& condition)
        {
            fc::sha512::encoder enc;
            fc::raw::pack(enc, condition);
            addr = fc::ripemd160::hash(enc.result());
        }

        bool Address::is_valid(const std::string& base58str, const std::string& prefix)
        {
            const size_t prefix_len = prefix.size();
            if (base58str.size() <= prefix_len)
                return false;
            if (base58str.substr(0, prefix_len) != prefix)
                return false;
            std::vector<char> v;
            try
            {
                v = fc::from_base58(base58str.substr(prefix_len));
            }
            catch (const fc::parse_error_exception&)
            {
                return false;
            }
            if (v.size() != sizeof(fc::ripemd160) + 4)
                return false;
            const fc::ripemd160 checksum = fc::ripemd160::hash(v.data(), static_cast<uint32_t>(v.size() - 4));
            if (memcmp(v.data() + 20, (char*)checksum._hash, 4) != 0)
                return false;
            return true;
        }

        Address::Address(const fc::ecc::public_key& pub)
        {
            auto dat = pub.serialize();
            addr = fc::ripemd160::hash(fc::sha512::hash(dat.data, sizeof(dat)));
        }

        Address::Address(const PtsAddress& ptsaddr)
        {
            addr = fc::ripemd160::hash((char*)&ptsaddr, sizeof(ptsaddr));
        }

        Address::Address(const fc::ecc::public_key_data& pub)
        {
            addr = fc::ripemd160::hash(fc::sha512::hash(pub.data, sizeof(pub)));
        }

        Address::Address(const thinkyoung::blockchain::PublicKeyType& pub)
        {
            addr = fc::ripemd160::hash(fc::sha512::hash(pub.key_data.data, sizeof(pub.key_data)));
        }

     //   Address::Address(const PublicKeyType& pubkey, const TransactionIdType& trxid)
     //   {
     //       fc::sha512::encoder enc;
     //       fc::raw::pack(enc, pubkey);
     //       fc::raw::pack(trxid);
     //       addr = fc::ripemd160::hash(enc.result());
     //   }

        std::string Address::AddressToString(const AddressType& address_type)const
        {
            std::string prefix;
            if (address_type == alp_address)
                prefix = ALP_ADDRESS_PREFIX;
            else if (address_type == contract_address)
                prefix = CONTRACT_ADDRESS_PREFIX;
            else if(address_type == script_id)
                prefix = SCRIPT_ID_PREFIX;
            fc::array<char, 24> bin_addr;
            memcpy((char*)&bin_addr, (char*)&addr, sizeof(addr));
            auto checksum = fc::ripemd160::hash((char*)&addr, sizeof(addr));
            memcpy(((char*)&bin_addr) + 20, (char*)&checksum._hash[0], 4);
            return prefix + fc::to_base58(bin_addr.data, sizeof(bin_addr));
        }

        Address::operator std::string()const
        {
            fc::array<char, 24> bin_addr;
            memcpy((char*)&bin_addr, (char*)&addr, sizeof(addr));
            auto checksum = fc::ripemd160::hash((char*)&addr, sizeof(addr));
            memcpy(((char*)&bin_addr) + 20, (char*)&checksum._hash[0], 4);
            return ALP_ADDRESS_PREFIX + fc::to_base58(bin_addr.data, sizeof(bin_addr));
        }

    }
} // namespace thinkyoung::blockchain

namespace fc
{
    void to_variant(const thinkyoung::blockchain::Address& var, variant& vo)
    {
        vo = std::string(var);
    }
    void from_variant(const variant& var, thinkyoung::blockchain::Address& vo)
    {
        vo = thinkyoung::blockchain::Address(var.as_string());
    }
}
