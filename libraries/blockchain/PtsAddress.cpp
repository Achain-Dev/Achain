#include <blockchain/Exceptions.hpp>
#include <blockchain/PtsAddress.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <algorithm>

namespace thinkyoung {
    namespace blockchain {

        PtsAddress::PtsAddress()
        {
            memset(addr.data, 0, sizeof(addr.data));
        }

        PtsAddress::PtsAddress(const std::string& base58str)
        {
            std::vector<char> v = fc::from_base58(fc::string(base58str));
            if (v.size())
                memcpy(addr.data, v.data(), std::min<size_t>(v.size(), sizeof(addr)));

            if (!is_valid())
            {
                FC_THROW_EXCEPTION(invalid_pts_address, "invalid pts_address ${a}", ("a", base58str));
            }
        }

        PtsAddress::PtsAddress(const fc::ecc::public_key& pub, bool compressed, uint8_t version)
        {
            fc::sha256 sha2;
            if (compressed)
            {
                auto dat = pub.serialize();
                sha2 = fc::sha256::hash(dat.data, sizeof(dat));
            }
            else
            {
                auto dat = pub.serialize_ecc_point();
                sha2 = fc::sha256::hash(dat.data, sizeof(dat));
            }
            auto rep = fc::ripemd160::hash((char*)&sha2, sizeof(sha2));
            addr.data[0] = version;
            memcpy(addr.data + 1, (char*)&rep, sizeof(rep));
            auto check = fc::sha256::hash(addr.data, sizeof(rep) + 1);
            check = fc::sha256::hash(check); // double
            memcpy(addr.data + 1 + sizeof(rep), (char*)&check, 4);
        }

        /**
         *  Checks the address to verify it has a
         *  valid checksum
         */
        bool PtsAddress::is_valid()const
        {
            auto check = fc::sha256::hash(addr.data, sizeof(fc::ripemd160) + 1);
            check = fc::sha256::hash(check); // double
            return memcmp(addr.data + 1 + sizeof(fc::ripemd160), (char*)&check, 4) == 0;
        }

        PtsAddress::operator std::string()const
        {
            return fc::to_base58(addr.data, sizeof(addr));
        }

    }
} // namespace thinkyoung

namespace fc
{
    void to_variant(const thinkyoung::blockchain::PtsAddress& var, variant& vo)
    {
        vo = std::string(var);
    }
    void from_variant(const variant& var, thinkyoung::blockchain::PtsAddress& vo)
    {
        vo = thinkyoung::blockchain::PtsAddress(var.as_string());
    }
}
