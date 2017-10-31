#include <blockchain/ExtendedAddress.hpp>
#include <blockchain/WithdrawTypes.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/algorithm/string.hpp>

namespace thinkyoung {
    namespace blockchain {

        const uint8_t WithdrawWithSignature::type = withdraw_signature_type;
        const uint8_t WithdrawWithMultisig::type = withdraw_multisig_type;
        const uint8_t withdraw_with_escrow::type = withdraw_escrow_type;

        MemoStatus::MemoStatus(const ExtendedMemoData& memo, bool valid_signature,
            const fc::ecc::private_key& opk)
            :ExtendedMemoData(memo), has_valid_signature(valid_signature), owner_private_key(opk)
        {
        }

        void MemoData::set_message(const std::string& message_str)
        {
            if (message_str.empty()) return;
            FC_ASSERT(message_str.size() <= sizeof(message));
            memcpy(message.data, message_str.c_str(), message_str.size());
        }

        void ExtendedMemoData::set_message(const std::string& message_str)
        {
            if (message_str.empty()) return;
            FC_ASSERT(message_str.size() <= sizeof(message) + sizeof(extra_memo));
            if (message_str.size() <= sizeof(message))
            {
                memcpy(message.data, message_str.c_str(), message_str.size());
            }
            else
            {
                memcpy(message.data, message_str.c_str(), sizeof(message));
                memcpy(extra_memo.data, message_str.c_str() + sizeof(message), message_str.size() - sizeof(message));
            }
        }

        std::string MemoData::get_message()const
        {
            // add .c_str() to auto-truncate at null byte
            return std::string((const char*)&message, sizeof(message)).c_str();
        }

        std::string ExtendedMemoData::get_message()const
        {
            // add .c_str() to auto-truncate at null byte
            return (std::string((const char*)&message, sizeof(message))
                + std::string((const char*)&extra_memo, sizeof(extra_memo))).c_str();
        }

        BalanceIdType WithdrawCondition::get_address()const
        {
            return Address(*this);
        }

        set<Address> WithdrawCondition::owners()const
        {
            switch (WithdrawConditionTypes(type))
            {
            case withdraw_signature_type:
                return set < Address > { this->as<WithdrawWithSignature>().owner };
            case withdraw_multisig_type:
                return this->as<WithdrawWithMultisig>().owners;
            case withdraw_escrow_type:
            {
                const auto escrow = this->as<withdraw_with_escrow>();
                return set < Address > { escrow.sender, escrow.receiver, escrow.escrow };
            }
            default:
                return set<Address>();
            }
        }

        optional<Address> WithdrawCondition::owner()const
        {
            switch (WithdrawConditionTypes(type))
            {
            case withdraw_signature_type:
                return this->as<WithdrawWithSignature>().owner;
            default:
                return optional<Address>();
            }
        }

        string WithdrawCondition::type_label()const
        {
            string label = string(this->type);
            label = label.substr(9);
            label = label.substr(0, label.find("_"));
            boost::to_upper(label);
            return label;
        }

        oMessageStatus WithdrawWithSignature::decrypt_memo_data(const fc::ecc::private_key& receiver_key, bool ignore_owner)const
        {
            try {
                try {
                    FC_ASSERT(memo.valid());
                    auto secret = receiver_key.get_shared_secret(memo->one_time_key);
                    ExtendedPrivateKey ext_receiver_key(receiver_key);

                    fc::ecc::private_key secret_private_key = ext_receiver_key.child(fc::sha256::hash(secret),
                        ExtendedPrivateKey::public_derivation);
                    auto secret_public_key = secret_private_key.get_public_key();

                    if (!ignore_owner && owner != Address(secret_public_key))
                        return oMessageStatus();

                    auto memo = decrypt_memo_data(secret);

                    bool has_valid_signature = false;
                    if (memo.memo_flags == from_memo && !(memo.from == PublicKeyType() && memo.from_signature == 0))
                    {
                        auto check_secret = secret_private_key.get_shared_secret(memo.from);
                        has_valid_signature = check_secret._hash[0] == memo.from_signature;
                    }
                    else
                    {
                        has_valid_signature = true;
                    }

                    return MemoStatus(memo, has_valid_signature, secret_private_key);
                }
                catch (const fc::aes_exception&)
                {
                    return oMessageStatus();
                }
            } FC_CAPTURE_AND_RETHROW((ignore_owner))
        }

        PublicKeyType WithdrawWithSignature::encrypt_memo_data(const fc::ecc::private_key& one_time_private_key,
            const fc::ecc::public_key& to_public_key,
            const fc::ecc::private_key& from_private_key,
            const std::string& memo_message,
            const fc::ecc::public_key& memo_pub_key,
            MemoFlagsEnum memo_type,
            bool use_stealth_address)
        {
            memo = TransferMemo();
            const auto secret = one_time_private_key.get_shared_secret(to_public_key);
            const auto ext_to_public_key = ExtendedPublicKey(to_public_key);
            const auto secret_ext_public_key = ext_to_public_key.child(fc::sha256::hash(secret));
            const auto secret_public_key = use_stealth_address ?
                secret_ext_public_key.get_pub_key() : to_public_key;
            owner = Address(secret_public_key);

            fc::sha512 check_secret;
            if (from_private_key.get_secret() != fc::ecc::private_key().get_secret())
                check_secret = from_private_key.get_shared_secret(secret_public_key);

            if (memo_message.size() <= ALP_BLOCKCHAIN_MAX_MEMO_SIZE)
            {
                MemoData memo_content;
                memo_content.set_message(memo_message);
                memo_content.from = memo_pub_key;
                memo_content.from_signature = check_secret._hash[0];
                memo_content.memo_flags = memo_type;
                memo->one_time_key = one_time_private_key.get_public_key();
                encrypt_memo_data(secret, memo_content);
            }
            else
            {
                ExtendedMemoData memo_content;
                memo_content.set_message(memo_message);
                memo_content.from = memo_pub_key;
                memo_content.from_signature = check_secret._hash[0];
                memo_content.memo_flags = memo_type;
                memo->one_time_key = one_time_private_key.get_public_key();
                encrypt_memo_data(secret, memo_content);
            }
            return secret_public_key;
        }

        ExtendedMemoData WithdrawWithSignature::decrypt_memo_data(const fc::sha512& secret)const
        {
            try {
                FC_ASSERT(memo.valid());
                if (memo->encrypted_memo_data.size() > (sizeof(MemoData)))
                {
                    return fc::raw::unpack<ExtendedMemoData>(fc::aes_decrypt(secret, memo->encrypted_memo_data));
                }
                else
                {
                    auto dmemo = fc::raw::unpack<MemoData>(fc::aes_decrypt(secret, memo->encrypted_memo_data));
                    ExtendedMemoData result;
                    result.from = dmemo.from;
                    result.from_signature = dmemo.from_signature;
                    result.message = dmemo.message;
                    result.memo_flags = dmemo.memo_flags;
                    return result;
                }
            } FC_RETHROW_EXCEPTIONS(warn, "")
        }

        void WithdrawWithSignature::encrypt_memo_data(const fc::sha512& secret,
            const MemoData& memo_content)
        {
            FC_ASSERT(memo.valid());
            memo->encrypted_memo_data = fc::aes_encrypt(secret, fc::raw::pack(memo_content));
        }
        void WithdrawWithSignature::encrypt_memo_data(const fc::sha512& secret,
            const ExtendedMemoData& memo_content)
        {
            FC_ASSERT(memo.valid());
            memo->encrypted_memo_data = fc::aes_encrypt(secret, fc::raw::pack(memo_content));
        }

        oMessageStatus withdraw_with_escrow::decrypt_memo_data(const fc::ecc::private_key& receiver_key, bool ignore_owner)const
        {
            try {
                try {
                    FC_ASSERT(memo.valid());
                    auto secret = receiver_key.get_shared_secret(memo->one_time_key);
                    ExtendedPrivateKey ext_receiver_key(receiver_key);

                    fc::ecc::private_key secret_private_key = ext_receiver_key.child(fc::sha256::hash(secret),
                        ExtendedPrivateKey::public_derivation);
                    auto secret_public_key = secret_private_key.get_public_key();

                    if (!ignore_owner
                        && sender != Address(secret_public_key)
                        && receiver != Address(secret_public_key)
                        && escrow != Address(secret_public_key))
                        return oMessageStatus();

                    auto memo = decrypt_memo_data(secret);

                    bool has_valid_signature = false;
                    if (memo.memo_flags == from_memo && !(memo.from == PublicKeyType() && memo.from_signature == 0))
                    {
                        auto check_secret = secret_private_key.get_shared_secret(memo.from);
                        has_valid_signature = check_secret._hash[0] == memo.from_signature;
                    }
                    else
                    {
                        has_valid_signature = true;
                    }

                    return MemoStatus(memo, has_valid_signature, secret_private_key);
                }
                catch (const fc::aes_exception&)
                {
                    return oMessageStatus();
                }
            } FC_CAPTURE_AND_RETHROW((ignore_owner))
        }

        PublicKeyType withdraw_with_escrow::encrypt_memo_data(
            const fc::ecc::private_key& one_time_private_key,
            const fc::ecc::public_key&  to_public_key,
            const fc::ecc::private_key& from_private_key,
            const std::string& memo_message,
            const fc::ecc::public_key&  memo_pub_key,
            MemoFlagsEnum memo_type)
        {
            memo = TransferMemo();
            const auto secret = one_time_private_key.get_shared_secret(to_public_key);
            const auto ext_to_public_key = ExtendedPublicKey(to_public_key);
            const auto secret_ext_public_key = ext_to_public_key.child(fc::sha256::hash(secret));
            const auto secret_public_key = secret_ext_public_key.get_pub_key();

            sender = Address(one_time_private_key.get_public_key());
            receiver = Address(secret_public_key);

            fc::sha512 check_secret;
            if (from_private_key.get_secret() != fc::ecc::private_key().get_secret())
                check_secret = from_private_key.get_shared_secret(secret_public_key);

            MemoData memo_content;
            memo_content.set_message(memo_message);
            memo_content.from = memo_pub_key;
            memo_content.from_signature = check_secret._hash[0];
            memo_content.memo_flags = memo_type;

            memo->one_time_key = one_time_private_key.get_public_key();

            encrypt_memo_data(secret, memo_content);
            return secret_public_key;
        }

        ExtendedMemoData withdraw_with_escrow::decrypt_memo_data(const fc::sha512& secret)const
        {
            try {
                FC_ASSERT(memo.valid());
                if (memo->encrypted_memo_data.size() > (sizeof(MemoData)))
                {
                    return fc::raw::unpack<ExtendedMemoData>(fc::aes_decrypt(secret, memo->encrypted_memo_data));
                }
                else
                {
                    auto dmemo = fc::raw::unpack<MemoData>(fc::aes_decrypt(secret, memo->encrypted_memo_data));
                    ExtendedMemoData result;
                    result.from = dmemo.from;
                    result.from_signature = dmemo.from_signature;
                    result.message = dmemo.message;
                    result.memo_flags = dmemo.memo_flags;
                    return result;
                }
            } FC_RETHROW_EXCEPTIONS(warn, "")
        }

        void withdraw_with_escrow::encrypt_memo_data(const fc::sha512& secret,
            const ExtendedMemoData& memo_content)
        {
            FC_ASSERT(memo.valid());
            memo->encrypted_memo_data = fc::aes_encrypt(secret, fc::raw::pack(memo_content));
        }

        void withdraw_with_escrow::encrypt_memo_data(const fc::sha512& secret,
            const MemoData& memo_content)
        {
            FC_ASSERT(memo.valid());
            memo->encrypted_memo_data = fc::aes_encrypt(secret, fc::raw::pack(memo_content));
        }

    }
} // thinkyoung::blockchain

namespace fc {
    void to_variant(const thinkyoung::blockchain::WithdrawCondition& var, variant& vo)
    {
        using namespace thinkyoung::blockchain;
        fc::mutable_variant_object obj;
        obj["asset_id"] = var.asset_id;
        obj["slate_id"] = var.slate_id;
        obj["type"] = var.type;
		obj["balance_type"] = var.balance_type;
        switch ((WithdrawConditionTypes)var.type)
        {
        case withdraw_null_type:
            obj["data"] = fc::variant();
            break;
        case withdraw_signature_type:
            obj["data"] = fc::raw::unpack<WithdrawWithSignature>(var.data);
            break;
        case withdraw_multisig_type:
            obj["data"] = fc::raw::unpack<WithdrawWithMultisig>(var.data);
            break;
        case withdraw_escrow_type:
            obj["data"] = fc::raw::unpack<withdraw_with_escrow>(var.data);
            break;
            // No default to force compiler warning
        }
        vo = std::move(obj);
    }

    void from_variant(const variant& var, thinkyoung::blockchain::WithdrawCondition& vo)
    {
        using namespace thinkyoung::blockchain;
        auto obj = var.get_object();
        from_variant(obj["asset_id"], vo.asset_id);
        try
        {
            from_variant(obj["slate_id"], vo.slate_id);
        }
        catch (const fc::key_not_found_exception&)
        {
            from_variant(obj["delegate_slate_id"], vo.slate_id);
        }
        from_variant(obj["type"], vo.type);
		from_variant(obj["balance_type"],vo.balance_type);
        switch ((WithdrawConditionTypes)vo.type)
        {
        case withdraw_null_type:
            return;
        case withdraw_signature_type:
            vo.data = fc::raw::pack(obj["data"].as<WithdrawWithSignature>());
            return;
        case withdraw_multisig_type:
            vo.data = fc::raw::pack(obj["data"].as<WithdrawWithMultisig>());
            return;
        case withdraw_escrow_type:
            vo.data = fc::raw::pack(obj["data"].as<withdraw_with_escrow>());
            return;
            // No default to force compiler warning
        }
        FC_ASSERT(false, "Invalid withdraw condition!");
    }

    void to_variant(const thinkyoung::blockchain::MemoData& var, variant& vo)
    {
        mutable_variant_object obj("from", var.from);
        obj("from_signature", var.from_signature)
            ("message", var.get_message())
            ("memo_flags", var.memo_flags);
        vo = std::move(obj);
    }

    void from_variant(const variant& var, thinkyoung::blockchain::MemoData& vo)
    {
        try {
            const variant_object& obj = var.get_object();
            if (obj.contains("from"))
                vo.from = obj["from"].as<thinkyoung::blockchain::PublicKeyType>();
            if (obj.contains("from_signature"))
                vo.from_signature = obj["from_signature"].as_int64();
            if (obj.contains("message"))
                vo.set_message(obj["message"].as_string());
            if (obj.contains("memo_flags"))
                vo.memo_flags = obj["memo_flags"].as<thinkyoung::blockchain::MemoFlagsEnum>();
        } FC_RETHROW_EXCEPTIONS(warn, "unable to convert variant to memo_data", ("variant", var))
    }

} // fc
