#pragma once

#include <blockchain/Address.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/reflect/reflect.hpp>

namespace thinkyoung {
    namespace blockchain {

        /**
         *  Given an extended public key you can calculate the public key of all
         *  children keys, but not the corresponding private keys.
         *
         *  @note this only works for extended private keys that use public derivation
         */
        class ExtendedPublicKey
        {
        public:
            ExtendedPublicKey();
            virtual ~ExtendedPublicKey();

            ExtendedPublicKey(const fc::ecc::public_key& key, const fc::sha256& code = fc::sha256());

            ExtendedPublicKey child(uint32_t c)const;
            ExtendedPublicKey child(const fc::sha256& secret)const;

            fc::ecc::public_key get_pub_key()const { return pub_key; }
            operator Address()const { return Address(pub_key); }

            operator fc::ecc::public_key()const { return pub_key; }

            fc::ecc::public_key pub_key;
            fc::sha256          chain_code;

            inline friend bool operator < (const ExtendedPublicKey& a, const ExtendedPublicKey& b)
            {
                if (a.chain_code < b.chain_code) return true;
                if (a.chain_code > b.chain_code) return false;
                if (a.pub_key.serialize() < b.pub_key.serialize()) return true;
                return false;
            }
            inline friend bool operator == (const ExtendedPublicKey& a, const ExtendedPublicKey& b)
            {
                if (a.chain_code != b.chain_code) return false;
                if (a.pub_key.serialize() != b.pub_key.serialize()) return false;
                return true;
            }
            inline friend bool operator != (const ExtendedPublicKey& a, const ExtendedPublicKey& b)
            {
                return !(a == b);
            }
        };

        class ExtendedPrivateKey
        {
        public:
            enum DerivationType
            {
                public_derivation = 0,
                private_derivation = 1
            };
            ExtendedPrivateKey(const fc::sha512& seed);
            ExtendedPrivateKey(const fc::sha256& key, const fc::sha256& chain_code);
            ExtendedPrivateKey(const fc::ecc::private_key& k) :priv_key(k.get_secret()){}
            ExtendedPrivateKey();

            /** @param pub_derivation - if true, then extended_public_key can be used
             *      to calculate child keys, otherwise the extended_private_key is
             *      required to calculate all children.
             */
            ExtendedPrivateKey child(uint32_t c,
                DerivationType derivation = private_derivation)const;

            ExtendedPrivateKey child(const fc::sha256& secret,
                DerivationType derivation = private_derivation)const;

            operator fc::ecc::private_key()const;
            fc::ecc::public_key get_public_key()const;

            operator ExtendedPublicKey()const
            {
                return ExtendedPublicKey(fc::ecc::private_key::regenerate(priv_key).get_public_key(),
                    chain_code);
            }

            bool operator==(const ExtendedPrivateKey& k) const
            {
                return (this->priv_key == k.priv_key) &&
                    (this->chain_code == k.chain_code);
            }

            fc::ecc::private_key_secret  priv_key;
            fc::sha256                   chain_code;
        };

        /**
         *  @brief encapsulates an encoded, checksumed public key in
         *  binary form. It can be converted to base58 for display
         *  or input purposes and can also be constructed from an ecc
         *  public key.
         *
         *  An valid extended_address is 20 bytes with the following form:
         *
         *  First 3-bits are 0, followed by bits to 3-127 of sha256(pub_key), followed
         *  a 32 bit checksum calculated as the first 32 bits of the sha256 of
         *  the first 128 bits of the extended_address.
         *
         *  The result of this is an extended_address that can be 'verified' by
         *  looking at the first character (base58) and has a built-in
         *  checksum to prevent mixing up characters.
         *
         *  It is stored as 20 bytes.
         */
        struct ExtendedAddress
        {
            ExtendedAddress(); ///< constructs empty / null extended_address
            ExtendedAddress(const std::string& base58str);   ///< converts to binary, validates checksum
            ExtendedAddress(const ExtendedPublicKey& pub);

            bool is_valid()const;

            operator std::string()const; ///< converts to base58 + checksum
            operator ExtendedPublicKey()const; ///< converts to base58 + checksum

            ExtendedPublicKey addr;      ///< binary representation of extended_address
        };

        inline bool operator == (const ExtendedAddress& a, const ExtendedAddress& b) { return a.addr == b.addr; }
        inline bool operator != (const ExtendedAddress& a, const ExtendedAddress& b) { return a.addr != b.addr; }
        inline bool operator <  (const ExtendedAddress& a, const ExtendedAddress& b) { return a.addr < b.addr; }

    }
} // namespace thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::ExtendedPublicKey, (pub_key)(chain_code))
FC_REFLECT(thinkyoung::blockchain::ExtendedPrivateKey, (priv_key)(chain_code))
FC_REFLECT(thinkyoung::blockchain::ExtendedAddress, (addr))

namespace fc
{
    void to_variant(const thinkyoung::blockchain::ExtendedAddress& var, fc::variant& vo);
    void from_variant(const fc::variant& var, thinkyoung::blockchain::ExtendedAddress& vo);
}

namespace std
{
    template<>
    struct hash < thinkyoung::blockchain::ExtendedAddress >
    {
    public:
        size_t operator()(const thinkyoung::blockchain::ExtendedAddress &a) const
        {
            size_t s;
            fc::sha1::encoder enc;
            fc::raw::pack(enc, a);
            auto r = enc.result();

            memcpy((char*)&s, (char*)&r, sizeof(s));
            return s;
        }
    };
}
