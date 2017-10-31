#include <blockchain/Config.hpp>
#include <blockchain/ExtendedAddress.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

namespace thinkyoung {
    namespace blockchain {

        ExtendedPublicKey::ExtendedPublicKey()
        {
        }

        ExtendedPublicKey::~ExtendedPublicKey(){}

        ExtendedPublicKey::ExtendedPublicKey(const fc::ecc::public_key& key, const fc::sha256& code)
            :pub_key(key), chain_code(code)
        {
        }

        ExtendedPublicKey ExtendedPublicKey::child(uint32_t child_idx)const
        {
            try {
                fc::sha512::encoder enc;
                fc::raw::pack(enc, pub_key);
                fc::raw::pack(enc, child_idx);
                fc::raw::pack(enc, chain_code);

                fc::sha512 ikey = enc.result();
                fc::sha256 ikey_left;
                fc::sha256 ikey_right;

                memcpy((char*)&ikey_left, (char*)&ikey, sizeof(ikey_left));
                memcpy((char*)&ikey_right, ((char*)&ikey) + sizeof(ikey_left), sizeof(ikey_right));

                ExtendedPublicKey child_key;
                child_key.chain_code = ikey_right;
                child_key.pub_key = pub_key.add(ikey_left);

                return child_key;
            } FC_RETHROW_EXCEPTIONS(warn, "child index ${child_idx}", ("child_idx", child_idx))
        }

        ExtendedPublicKey ExtendedPublicKey::child(const fc::sha256& child_idx)const
        {
            try {
                fc::sha512::encoder enc;
                fc::raw::pack(enc, pub_key);
                fc::raw::pack(enc, child_idx);
                fc::raw::pack(enc, chain_code);

                fc::sha512 ikey = enc.result();
                fc::sha256 ikey_left;
                fc::sha256 ikey_right;

                memcpy((char*)&ikey_left, (char*)&ikey, sizeof(ikey_left));
                memcpy((char*)&ikey_right, ((char*)&ikey) + sizeof(ikey_left), sizeof(ikey_right));

                ExtendedPublicKey child_key;
                child_key.chain_code = ikey_right;
                child_key.pub_key = pub_key.add(ikey_left);

                return child_key;
            } FC_RETHROW_EXCEPTIONS(warn, "child index ${child_idx}", ("child_idx", child_idx))
        }

        ExtendedPrivateKey::ExtendedPrivateKey(const fc::sha256& key, const fc::sha256& ccode)
            :priv_key(key), chain_code(ccode)
        {
        }

        ExtendedPrivateKey::ExtendedPrivateKey(const fc::sha512& seed)
        {
            memcpy((char*)&priv_key, (char*)&seed, sizeof(priv_key));
            //  memcpy( (char*)&chain_code, ((char*)&seed) + sizeof(priv_key), sizeof(priv_key) );
        }

        ExtendedPrivateKey::ExtendedPrivateKey()
        {
        }

        ExtendedPrivateKey ExtendedPrivateKey::child(uint32_t child_idx, DerivationType derivation)const
        {
            try {
                ExtendedPrivateKey child_key;

                fc::sha512::encoder enc;
                if (derivation == public_derivation)
                {
                    fc::raw::pack(enc, get_public_key());
                }
                else
                {
                    uint8_t pad = 0;
                    fc::raw::pack(enc, pad);
                    fc::raw::pack(enc, priv_key);
                }
                fc::raw::pack(enc, child_idx);
                fc::raw::pack(enc, chain_code);
                fc::sha512 ikey = enc.result();

                fc::sha256 ikey_left;
                fc::sha256 ikey_right;

                memcpy((char*)&ikey_left, (char*)&ikey, sizeof(ikey_left));
                memcpy((char*)&ikey_right, ((char*)&ikey) + sizeof(ikey_left), sizeof(ikey_right));

                child_key.priv_key = fc::ecc::private_key::generate_from_seed(priv_key, ikey_left).get_secret();
                child_key.chain_code = ikey_right;

                return child_key;
            } FC_RETHROW_EXCEPTIONS(warn, "child index ${child_idx}", ("child_idx", child_idx))
        }

        ExtendedPrivateKey ExtendedPrivateKey::child(const fc::sha256& child_idx, DerivationType derivation)const
        {
            try {
                ExtendedPrivateKey child_key;

                fc::sha512::encoder enc;
                if (derivation == public_derivation)
                {
                    fc::raw::pack(enc, get_public_key());
                }
                else
                {
                    uint8_t pad = 0;
                    fc::raw::pack(enc, pad);
                    fc::raw::pack(enc, priv_key);
                }
                fc::raw::pack(enc, child_idx);
                fc::raw::pack(enc, chain_code);
                fc::sha512 ikey = enc.result();

                fc::sha256 ikey_left;
                fc::sha256 ikey_right;

                memcpy((char*)&ikey_left, (char*)&ikey, sizeof(ikey_left));
                memcpy((char*)&ikey_right, ((char*)&ikey) + sizeof(ikey_left), sizeof(ikey_right));

                child_key.priv_key = fc::ecc::private_key::generate_from_seed(priv_key, ikey_left).get_secret();
                child_key.chain_code = ikey_right;

                return child_key;
            } FC_RETHROW_EXCEPTIONS(warn, "child index ${child_idx}", ("child_idx", child_idx))
        }

        ExtendedPrivateKey::operator fc::ecc::private_key()const
        {
            return fc::ecc::private_key::regenerate(priv_key);
        }

        fc::ecc::public_key ExtendedPrivateKey::get_public_key()const
        {
            return fc::ecc::private_key::regenerate(priv_key).get_public_key();
        }

        ExtendedAddress::ExtendedAddress()
        { }

        ExtendedAddress::ExtendedAddress(const std::string& base58str)
        {
            try {

                uint32_t checksum = 0;
                std::string prefix(ALP_ADDRESS_PREFIX);
                const size_t prefix_len = prefix.size();
                FC_ASSERT(base58str.size() > prefix_len, "Address must be longer than ALP_ADDRESS_PREFIX");
                FC_ASSERT(base58str.substr(0, prefix_len) == prefix, "ALP_ADDRESS_PREFIX must be in front of the address");

                std::vector<char> data = fc::from_base58(base58str.substr(prefix_len));
                FC_ASSERT(data.size() == (33 + 32 + 4));

                fc::datastream<const char*> ds(data.data(), data.size());
                fc::raw::unpack(ds, *this);
                fc::raw::unpack(ds, checksum);

                FC_ASSERT(checksum == fc::ripemd160::hash(data.data(), sizeof(*this))._hash[0]);

            } FC_RETHROW_EXCEPTIONS(warn, "decoding address ${address}", ("address", base58str))
        }

        ExtendedAddress::ExtendedAddress(const ExtendedPublicKey& pub)
            :addr(pub)
        {
        }

        bool ExtendedAddress::is_valid()const
        {
            return addr.pub_key.valid();
        }

        ExtendedAddress::operator std::string()const
        {
            std::vector<char> data = fc::raw::pack(*this);
            uint32_t checksum = fc::ripemd160::hash(data.data(), sizeof(*this))._hash[0];

            data.resize(data.size() + 4);
            memcpy(data.data() + data.size() - 4, (char*)&checksum, sizeof(checksum));

            return ALP_ADDRESS_PREFIX + fc::to_base58(data.data(), data.size());
        }

        ExtendedAddress::operator ExtendedPublicKey()const
        {
            return addr;
        }

    }
} // namespace thinkyoung::blockchain

namespace fc
{
    void to_variant(const thinkyoung::blockchain::ExtendedAddress& ext_addr, fc::variant& vo)
    {
        vo = std::string(ext_addr);
    }

    void from_variant(const fc::variant& var, thinkyoung::blockchain::ExtendedAddress& vo)
    {
        vo = thinkyoung::blockchain::ExtendedAddress(var.as_string());
    }
}
