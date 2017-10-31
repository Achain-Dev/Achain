#pragma once

#include <blockchain/Address.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>

#include <functional>
#define CON_INIT_INTERFACE "init"
#define CON_ON_DEPOSIT_INTERFACE "on_deposit"
#define CON_ON_DESTROY_INTERFACE "on_destroy"
#define CON_ON_UPGRADE_INTERFACE "on_upgrade"
namespace fc {
    class path;
    class microseconds;
    class time_point;
    class time_point_sec;
}

namespace thinkyoung {
    namespace blockchain {
        typedef std::string                        Imessage;
        typedef std::string                        FilePath;
        typedef fc::ripemd160                      BlockIdType;
        typedef fc::ripemd160                      TransactionIdType;
        typedef fc::ripemd160                      PublicKeyHashType;
        typedef fc::ripemd160                      SecretHashType;
        typedef fc::ripemd160                      OrderIdType;
        typedef Address                      ScriptIdType;
        typedef fc::sha256                         DigestType;
        typedef fc::ecc::compact_signature         SignatureType;
        typedef fc::ecc::private_key               PrivateKeyType;
        typedef Address                            BalanceIdType;
        typedef fc::signed_int                     AssetIdType;
        typedef fc::signed_int                     AccountIdType;
        typedef fc::signed_int                     FeedIdType;
        typedef int64_t                            ShareType;
        typedef uint64_t                           SlateIdType;
        typedef uint64_t                           ObjectIdType;
        typedef int64_t                            ImessageIdType;
        typedef int64_t                          ImessageLengthIdType;

        //type define for contract
        typedef std::string						  ContractName;
        typedef Address							  ContractIdType;
        typedef unsigned char                      ContractChar;

        using std::string;
        using std::function;
        using fc::variant;
        using fc::variant_object;
        using fc::mutable_variant_object;
        using fc::optional;
        using std::map;
        using std::unordered_map;
        using std::set;
        using std::unordered_set;
        using std::vector;
        using std::pair;
        using fc::path;
        using fc::sha512;
        using fc::sha256;
        using std::unique_ptr;
        using std::shared_ptr;
        using fc::time_point_sec;
        using fc::time_point;
        using fc::microseconds;
        using fc::unsigned_int;
        using fc::signed_int;

        struct PublicKeyType
        {
            struct BinaryKey
            {
                BinaryKey() :check(0){}
                uint32_t                 check;
                fc::ecc::public_key_data data;
            };

            fc::ecc::public_key_data key_data;

            PublicKeyType();
            PublicKeyType(const fc::ecc::public_key_data& data);
            PublicKeyType(const fc::ecc::public_key& pubkey);
            explicit PublicKeyType(const std::string& base58str);
            operator fc::ecc::public_key_data() const;
            operator fc::ecc::public_key() const;
            explicit operator std::string() const;
            friend bool operator == (const PublicKeyType& p1, const fc::ecc::public_key& p2);
            friend bool operator == (const PublicKeyType& p1, const PublicKeyType& p2);
            friend bool operator != (const PublicKeyType& p1, const PublicKeyType& p2);

            bool is_valid_v1(const std::string& base58str);
        };

        struct BlockchainSecurityState {
            enum alert_level_enum {
                green = 0,
                yellow = 1,
                red = 2,
                grey = 3
            };
            alert_level_enum    alert_level;
            uint32_t            estimated_confirmation_seconds;
            double              participation_rate;
        };

    }
} // thinkyoung::blockchain

namespace fc
{
    void to_variant(const thinkyoung::blockchain::PublicKeyType& var, fc::variant& vo);
    void from_variant(const fc::variant& var, thinkyoung::blockchain::PublicKeyType& vo);
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT(thinkyoung::blockchain::PublicKeyType, (key_data))
FC_REFLECT(thinkyoung::blockchain::PublicKeyType::BinaryKey, (data)(check));
FC_REFLECT_ENUM(thinkyoung::blockchain::BlockchainSecurityState::alert_level_enum, (green)(yellow)(red)(grey));
FC_REFLECT(thinkyoung::blockchain::BlockchainSecurityState, (alert_level)(estimated_confirmation_seconds)(participation_rate))
