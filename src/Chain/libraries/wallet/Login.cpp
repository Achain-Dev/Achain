#include <wallet/Url.hpp>
#include <wallet/Wallet.hpp>
#include <wallet/WalletImpl.hpp>

using namespace thinkyoung::wallet;
using namespace thinkyoung::wallet::detail;

void WalletImpl::login_map_cleaner_task()
{
    std::vector<PublicKeyType> expired_entrys;
    for (const auto& entry : _login_map)
        if (fc::time_point::now() - entry.second.insertion_time >= fc::seconds(_login_lifetime_seconds))
            expired_entrys.push_back(entry.first);
    ilog("Purging ${count} expired entrys from login map.", ("count", expired_entrys.size()));
    for (const auto& entry : expired_entrys)
        _login_map.erase(entry);

    if (!_login_map.empty())
        _login_map_cleaner_done = fc::schedule([this](){ login_map_cleaner_task(); },
        fc::time_point::now() + fc::seconds(_login_cleaner_interval_seconds),
        "login_map_cleaner_task");
}

std::string Wallet::login_start(const std::string& account_name)
{
    try {
        FC_ASSERT(is_open(), "Wallet not open!");
        FC_ASSERT(is_unlocked(), "Wallet not unlock!");

        auto key = my->_wallet_db.lookup_key(get_account(account_name).owner_address());
        FC_ASSERT(key.valid());
        FC_ASSERT(key->has_private_key());

        PrivateKeyType one_time_key = PrivateKeyType::generate();
        PublicKeyType one_time_public_key = one_time_key.get_public_key();
        my->_login_map[one_time_public_key] = { one_time_key, fc::time_point::now() };

        if (!my->_login_map_cleaner_done.valid() || my->_login_map_cleaner_done.ready())
            my->_login_map_cleaner_done = fc::schedule([this](){ my->login_map_cleaner_task(); },
            fc::time_point::now() + fc::seconds(my->_login_cleaner_interval_seconds),
            "login_map_cleaner_task");

        auto signature = key->decrypt_private_key(my->_wallet_password)
            .sign_compact(fc::sha256::hash((char*)&one_time_public_key,
            sizeof(one_time_public_key)));

        return CUSTOM_URL_SCHEME ":Login/" + variant(PublicKeyType(one_time_public_key)).as_string()
            + "/" + fc::variant(signature).as_string() + "/";
    } FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name))
}

fc::variant Wallet::login_finish(const PublicKeyType& server_key,
    const PublicKeyType& client_key,
    const fc::ecc::compact_signature& client_signature)
{
    try {
        FC_ASSERT(is_open(), "Wallet not open!");
        FC_ASSERT(is_unlocked(), "Wallet not unlock!");
        FC_ASSERT(my->_login_map.find(server_key) != my->_login_map.end(), "Login session has expired. Generate a new login URL and try again.");

        PrivateKeyType private_key = my->_login_map[server_key].key;
        my->_login_map.erase(server_key);
        auto secret = private_key.get_shared_secret(fc::ecc::public_key_data(client_key));
        auto user_account_key = fc::ecc::public_key(client_signature, fc::sha256::hash(secret.data(), sizeof(secret)));

        fc::mutable_variant_object result;
        result["user_account_key"] = PublicKeyType(user_account_key);
        result["shared_secret"] = secret;
        return result;
    } FC_RETHROW_EXCEPTIONS(warn, "", ("server_key", server_key)("client_key", client_key)("client_signature", client_signature))
}
