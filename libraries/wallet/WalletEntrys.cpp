#include <blockchain/ChainInterface.hpp>
#include <wallet/Exceptions.hpp>
#include <blockchain/Exceptions.hpp>
#include <wallet/WalletEntrys.hpp>
#include <fc/crypto/aes.hpp>

namespace thinkyoung {
    namespace wallet {

        ExtendedPrivateKey MasterKey::decrypt_key(const fc::sha512& password)const
        {
            try {
                FC_ASSERT(password != fc::sha512());
                FC_ASSERT(encrypted_key.size());
                auto plain_text = fc::aes_decrypt(password, encrypted_key);
                return fc::raw::unpack<ExtendedPrivateKey>(plain_text);
            } FC_CAPTURE_AND_RETHROW()
        }

        bool MasterKey::validate_password(const fc::sha512& password)const
        {
            return checksum == fc::sha512::hash(password);
        }

        void MasterKey::encrypt_key(const fc::sha512& password,
            const ExtendedPrivateKey& k)
        {
            try {
                FC_ASSERT(password != fc::sha512());
                checksum = fc::sha512::hash(password);
                encrypted_key = fc::aes_encrypt(password, fc::raw::pack(k));

                // just to double check... we should find out if there is
                // a problem ASAP...
                ExtendedPrivateKey k_dec = decrypt_key(password);
                FC_ASSERT(k_dec == k);
            } FC_CAPTURE_AND_RETHROW()
        }

        bool KeyData::has_private_key()const
        {
            return encrypted_private_key.size() > 0;
        }

        void KeyData::encrypt_private_key(const fc::sha512& password,
            const PrivateKeyType& key_to_encrypt)
        {
            try {
                FC_ASSERT(password != fc::sha512());
                public_key = key_to_encrypt.get_public_key();
                encrypted_private_key = fc::aes_encrypt(password, fc::raw::pack(key_to_encrypt));

                // just to double check, we should find out if there is a problem ASAP
                FC_ASSERT(key_to_encrypt == decrypt_private_key(password));
            } FC_CAPTURE_AND_RETHROW()
        }

        PrivateKeyType KeyData::decrypt_private_key(const fc::sha512& password)const
        {
            try {
                FC_ASSERT(password != fc::sha512());
                const auto plain_text = fc::aes_decrypt(password, encrypted_private_key);
                return fc::raw::unpack<PrivateKeyType>(plain_text);
            } FC_CAPTURE_AND_RETHROW()
        }

        ContactData::ContactData(const ChainInterface& db, const string& data, const string& label)
        {
            try {
                ContactData entry;

                if (db.get_account_entry(data).valid())
                {
                    entry = ContactData(data);
                }
                else
                {
                    try
                    {
                        entry = ContactData(PublicKeyType(data));
                    }
                    catch (const fc::exception&)
                    {
                        try
                        {
                            entry = ContactData(Address(data));
                        }
                        catch (const fc::exception&)
                        {
                            try
                            {
                                entry = ContactData(PtsAddress(data));
                            }
                            catch (const fc::exception&)
                            {
                                FC_CAPTURE_AND_THROW(invalid_contact, (data));
                            }
                        }
                    }
                }

                if (!label.empty())
                    entry.label = label;

                *this = entry;
            } FC_CAPTURE_AND_RETHROW((data)(label))
        }

        ScriptEntry::ScriptEntry(fc::path filepath, string description /*= ""*/) : description(description), enable(true)
        {
            if (filepath.extension() != ".script")
                FC_CAPTURE_AND_THROW(script_file_name_invalid,(filepath));
            code = Code(filepath);
            fc::sha512::encoder enc;
            fc::raw::pack(enc, code);
            id.addr = fc::ripemd160::hash(enc.result());
            register_time = fc::time_point::now();
        }

        bool ScriptEntry::operator<(const ScriptEntry & entry) const
        {
            if (entry.register_time < register_time)
                return true;
            return false;
        }

        ScriptEntry::ScriptEntry()
        {

        }

    }
} // thinkyoung::wallet
