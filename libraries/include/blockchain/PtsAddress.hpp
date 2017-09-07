#pragma once

#include <fc/array.hpp>
#include <string>

namespace fc { namespace ecc { class public_key; } }

namespace thinkyoung {
    namespace blockchain {

        /**
         *  Implements address stringification and validation from PTS
         */
        struct PtsAddress
        {
            PtsAddress(); ///< constructs empty / null address
            PtsAddress(const std::string& base58str);   ///< converts to binary, validates checksum
            PtsAddress(const fc::ecc::public_key& pub, bool compressed = true, uint8_t version = 56); ///< converts to binary

            uint8_t version()const { return addr.at(0); }
            bool is_valid()const;

            operator std::string()const; ///< converts to base58 + checksum

            fc::array<char, 25> addr; ///< binary representation of address
        };

        inline bool operator == (const PtsAddress& a, const PtsAddress& b) { return a.addr == b.addr; }
        inline bool operator != (const PtsAddress& a, const PtsAddress& b) { return a.addr != b.addr; }
        inline bool operator <  (const PtsAddress& a, const PtsAddress& b) { return a.addr < b.addr; }

    }
} // namespace thinkyoung::blockchain

namespace std
{
    template<>
    struct hash < thinkyoung::blockchain::PtsAddress >
    {
    public:
        size_t operator()(const thinkyoung::blockchain::PtsAddress &a) const
        {
            size_t s;
            memcpy((char*)&s, &a.addr.data[sizeof(a) - sizeof(s)], sizeof(s));
            return s;
        }
    };
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT(thinkyoung::blockchain::PtsAddress, (addr))

namespace fc
{
    void to_variant(const thinkyoung::blockchain::PtsAddress& var, fc::variant& vo);
    void from_variant(const fc::variant& var, thinkyoung::blockchain::PtsAddress& vo);
}
