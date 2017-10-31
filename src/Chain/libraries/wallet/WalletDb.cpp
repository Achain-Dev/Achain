#include <blockchain/Time.hpp>
#include <db/LevelMap.hpp>
#include <wallet/Exceptions.hpp>
#include <wallet/WalletDb.hpp>

#include <fc/io/json.hpp>
#include <fstream>
#include <iostream>

namespace thinkyoung {
    namespace wallet {

        using namespace thinkyoung::blockchain;

        namespace detail {
            class WalletDbImpl
            {
            public:
                WalletDb*                                        self = nullptr;
                thinkyoung::db::LevelMap<int32_t, GenericWalletEntry> _entrys;

                void store_and_reload_generic_entry(const GenericWalletEntry& entry, const bool sync)
                {
                    try {
                        auto index = entry.get_wallet_entry_index();
                        FC_ASSERT(index != 0);
                        FC_ASSERT(_entrys.is_open());
#ifndef ALP_TEST_NETWORK
                        _entrys.store(index, entry, sync);
#else
                        _entrys.store(index, entry);
#endif
                        load_generic_entry(entry);
                    } FC_CAPTURE_AND_RETHROW((entry))
                }

                void load_generic_entry(const GenericWalletEntry& entry)
                {
                    try {
                        switch (WalletEntryTypeEnum(entry.type))
                        {
                        case property_entry_type:
                            load_property_entry(entry.as<WalletPropertyEntry>());
                            break;
                        case master_key_entry_type:
                            load_master_key_entry(entry.as<WalletMasterKeyEntry>());
                            break;
                        case account_entry_type:
                            load_account_entry(entry.as<WalletAccountEntry>());
                            break;
                        case key_entry_type:
                            load_key_entry(entry.as<WalletKeyEntry>());
                            break;
                        case contact_entry_type:
                            load_contact_entry(entry.as<WalletContactEntry>());
                            break;
                        case transaction_entry_type:
                            load_transaction_entry(entry.as<WalletTransactionEntry>());
                            break;
                        case setting_entry_type:
                            load_setting_entry(entry.as<WalletSettingEntry>());
                            break;
						case contract_entry_type:
							load_contract_entry(entry.as<WalletContractEntry>());
							break;

                        default:
                            elog("Unknown wallet entry type: ${type}", ("type", entry.type));
                            break;
                        }
                    } FC_CAPTURE_AND_RETHROW((entry))
                }

                void load_property_entry(const WalletPropertyEntry& property_rec)
                {
                    try {
                        self->properties[property_rec.key] = property_rec;
                    } FC_CAPTURE_AND_RETHROW((property_rec))
                }

                void load_master_key_entry(const WalletMasterKeyEntry& key)
                {
                    try {
                        self->wallet_master_key = key;
                    } FC_CAPTURE_AND_RETHROW((key))
                }

                void load_account_entry(const WalletAccountEntry& account_entry)
                {
                    try {
                        const int32_t& entry_index = account_entry.wallet_entry_index;
                        self->accounts[entry_index] = account_entry;

                        // Cache address map
                        self->address_to_account_wallet_entry_index[Address(account_entry.owner_key)] = entry_index;
                        for (const auto& item : account_entry.active_key_history)
                        {
                            const PublicKeyType& active_key = item.second;
                            if (active_key == PublicKeyType()) continue;
                            self->address_to_account_wallet_entry_index[Address(active_key)] = entry_index;
                        }
                        if (account_entry.is_delegate())
                        {
                            for (const auto& item : account_entry.delegate_info->signing_key_history)
                            {
                                const PublicKeyType& signing_key = item.second;
                                if (signing_key == PublicKeyType()) continue;
                                self->address_to_account_wallet_entry_index[Address(signing_key)] = entry_index;
                            }
                        }

                        // Cache name map
                        self->name_to_account_wallet_entry_index[account_entry.name] = entry_index;

                        // Cache id map
                        if (account_entry.id != 0)
                            self->account_id_to_wallet_entry_index[account_entry.id] = entry_index;
                    } FC_CAPTURE_AND_RETHROW((account_entry))
                }

				void load_contract_entry(const WalletContractEntry &contract_entry)
				{
					//store contract entry map
					self->contracts_of_wallet[Address(contract_entry.owner)].push_back(contract_entry);

					//store id account map
					self->id_to_entry_for_contract[contract_entry.id] = contract_entry;

				}

                void load_key_entry(const WalletKeyEntry& key_entry)
                {
                    try {
                        const Address key_address = key_entry.get_address();

                        self->keys[key_address] = key_entry;

                        // Cache address map
                        self->btc_to_alp_address[key_address] = key_address;
                        self->btc_to_alp_address[Address(PtsAddress(key_entry.public_key, false, 0))] = key_address; // Uncompressed BTC
                        self->btc_to_alp_address[Address(PtsAddress(key_entry.public_key, true, 0))] = key_address; // Compressed BTC
                        self->btc_to_alp_address[Address(PtsAddress(key_entry.public_key, false, 56))] = key_address; // Uncompressed PTS
                        self->btc_to_alp_address[Address(PtsAddress(key_entry.public_key, true, 56))] = key_address; // Compressed PTS
                    } FC_CAPTURE_AND_RETHROW((key_entry))
                }

                void load_contact_entry(const WalletContactEntry& entry)
                {
                    try {
                        const string data = entry.data.as_string();
                        self->contacts[data] = entry;
                        self->label_to_account_or_contact[entry.label] = data;
                    } FC_CAPTURE_AND_RETHROW((entry))
                }

                void load_transaction_entry(const WalletTransactionEntry& transaction_entry)
                {
                    try {
                        const TransactionIdType& entry_id = transaction_entry.entry_id;
                        self->transactions[entry_id] = transaction_entry;

                        // Cache id map
                        self->id_to_transaction_entry_index[entry_id] = entry_id;
                        const TransactionIdType transaction_id = transaction_entry.trx.id();
                        if (transaction_id != SignedTransaction().id())
                            self->id_to_transaction_entry_index[transaction_id] = entry_id;
                    } FC_CAPTURE_AND_RETHROW((transaction_entry))
                }

                void load_setting_entry(const WalletSettingEntry& rec)
                {
                    try {
                        self->settings[rec.name] = rec;
                    } FC_CAPTURE_AND_RETHROW((rec))
                }

                void remove_generic_wallet_entry(const GenericWalletEntry& rec)
                {
                    //从db中将钱包记录删除
                    try{
                        _entrys.remove(rec.get_wallet_entry_index(), true);
                    } FC_CAPTURE_AND_RETHROW((rec))
                }


            };

        } // namespace detail

        WalletDb::WalletDb()
            :my(new detail::WalletDbImpl())
        {
            my->self = this;
        }

        WalletDb::~WalletDb()
        {
        }

        void WalletDb::open(const fc::path& wallet_file)
        {
            try {
                try
                {
                    my->_entrys.open(wallet_file, true);
                    for (auto itr = my->_entrys.begin(); itr.valid(); ++itr)
                    {
                        auto entry = itr.value();
                        try
                        {
                            my->load_generic_entry(entry);
                            // Prevent hanging on large wallets
                            fc::usleep(fc::milliseconds(1));
                        }
                        catch (const fc::canceled_exception&)
                        {
                            throw;
                        }
                        catch (const fc::exception& e)
                        {
                            wlog("Error loading wallet entry:\n${r}\nReason: ${e}", ("e", e.to_detail_string())("r", entry));
                        }
                    }
                }
                catch (...)
                {
                    close();
                    throw;
                }
            } FC_RETHROW_EXCEPTIONS(warn, "Error opening wallet file ${file}", ("file", wallet_file))
        }

        void WalletDb::close()
        {
            my->_entrys.close();

            wallet_master_key.reset();

            accounts.clear();
            address_to_account_wallet_entry_index.clear();
            name_to_account_wallet_entry_index.clear();
            account_id_to_wallet_entry_index.clear();

            keys.clear();
            btc_to_alp_address.clear();

            transactions.clear();
            id_to_transaction_entry_index.clear();

            properties.clear();
            settings.clear();
			
			//clear contract db
			contracts_of_wallet.clear();
			id_to_entry_for_contract.clear();
        }

        bool WalletDb::is_open()const
        {
            return my->_entrys.is_open() && wallet_master_key.valid();
        }

        void WalletDb::store_and_reload_generic_entry(const GenericWalletEntry& entry, const bool sync)
        {
            FC_ASSERT(my->_entrys.is_open());
            my->store_and_reload_generic_entry(entry, sync);
        }

        int32_t WalletDb::new_wallet_entry_index()
        {
            auto next_rec_num = get_property(next_entry_number);
            int32_t next_rec_number = 2;
            if (next_rec_num.is_null())
            {
                bool getoldnum = false;
                for (auto iter = my->_entrys.begin(); iter.valid(); ++iter)
                {
                    GenericWalletEntry entry = iter.value();
                    if (entry.type == property_entry_type)
                    {
                        if ((entry.data.is_object()) && (entry.data.get_object().contains("key")))
                        {
                            if (entry.data.get_object()["key"].as<string>() == "next_record_number")
                            {
                                next_rec_number = entry.data.get_object()["value"].as<int32_t>();
                                ilog("im in the old movie\n");
                                getoldnum = true;
                                for (auto iter = my->_entrys.begin(); iter.valid();)
                                {
                                    GenericWalletEntry entry = iter.value();
                                    if (entry.type == transaction_entry_type)
                                    {
                                        auto key = iter.key();
                                        ++iter;
                                        my->_entrys.remove(key);
                                    }
                                    else
                                        ++iter;
                                }
                                transactions.clear();
                                id_to_transaction_entry_index.clear();
                                break;
                            }
                        }
                    }
                }
                if (!getoldnum)
                {
                next_rec_number = 2;
                }
            }
            else
            {
                next_rec_number = next_rec_num.as<int32_t>();
            }
            set_property(PropertyEnum::next_entry_number, next_rec_number + 1);
            return next_rec_number;
        }

        uint32_t WalletDb::get_last_wallet_child_key_index()const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                try
                {
                    return get_property(next_child_key_index).as<uint32_t>();
                }
                catch (...)
                {
                }
                return 0;
            } FC_CAPTURE_AND_RETHROW()
        }

        void WalletDb::set_last_wallet_child_key_index(uint32_t key_index)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                set_property(next_child_key_index, key_index);
            } FC_CAPTURE_AND_RETHROW((key_index))
        }

        PrivateKeyType WalletDb::get_wallet_child_key(const fc::sha512& password, uint32_t key_index)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const ExtendedPrivateKey master_private_key = wallet_master_key->decrypt_key(password);
                return master_private_key.child(key_index);
            } FC_CAPTURE_AND_RETHROW((key_index))
        }

        PublicKeyType WalletDb::generate_new_account(const fc::sha512& password, const string& account_name, const variant& private_data)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");

                oWalletAccountEntry account_entry = lookup_account(account_name);
                FC_ASSERT(!account_entry.valid(), "Wallet already contains an account with that name!");

                uint32_t key_index = get_last_wallet_child_key_index();
                PrivateKeyType owner_private_key, active_private_key;
                PublicKeyType owner_public_key, active_public_key;
                Address owner_address, active_address;
                while (true)
                {
                    ++key_index;
                    FC_ASSERT(key_index != 0, "Overflow!");

                    owner_private_key = get_wallet_child_key(password, key_index);
                    owner_public_key = owner_private_key.get_public_key();
                    owner_address = Address(owner_public_key);

                    account_entry = lookup_account(owner_address);
                    if (account_entry.valid()) continue;

                    oWalletKeyEntry key_entry = lookup_key(owner_address);
                    if (key_entry.valid() && key_entry->has_private_key()) continue;

                    active_private_key = get_account_child_key(owner_private_key, 0);
                    active_public_key = active_private_key.get_public_key();
                    active_address = Address(active_public_key);

                    account_entry = lookup_account(active_address);
                    if (account_entry.valid()) continue;

                    key_entry = lookup_key(active_address);
                    if (key_entry.valid() && key_entry->has_private_key()) continue;

                    break;
                }

                KeyData active_key;
                active_key.account_address = owner_address;
                active_key.public_key = active_public_key;
                active_key.encrypt_private_key(password, active_private_key);

                KeyData owner_key;
                owner_key.account_address = owner_address;
                owner_key.public_key = owner_public_key;
                owner_key.encrypt_private_key(password, owner_private_key);
                owner_key.gen_seq_number = key_index;

                AccountData account;
                account.name = account_name;
                account.owner_key = owner_public_key;
                account.set_active_key(blockchain::now(), active_public_key);
                account.last_update = blockchain::now();
                account.is_my_account = true;
                account.private_data = private_data;

                store_key(active_key);
                set_last_wallet_child_key_index(key_index);
                store_key(owner_key);
                store_account(account);

                return owner_public_key;
            } FC_CAPTURE_AND_RETHROW((account_name))
        }

        void WalletDb::remove_account(const string& accountname)
        {
            try{
                oWalletAccountEntry accRec = lookup_account(accountname);
                if (!accRec.valid())
                {
                    return;
                }
                const int32_t& entry_index = accRec->wallet_entry_index;

                accounts.erase(entry_index);

                address_to_account_wallet_entry_index.erase(Address(accRec->owner_key));
                for (const auto& item : accRec->active_key_history)
                {
                    const PublicKeyType& active_key = item.second;
                    address_to_account_wallet_entry_index.erase(Address(active_key));
                }
                if (accRec->is_delegate())
                {
                    for (const auto& item : accRec->delegate_info->signing_key_history)
                    {
                        const PublicKeyType& signing_key = item.second;
                        address_to_account_wallet_entry_index.erase(Address(signing_key));
                    }
                }


				//delete contract related db when delete account from wallet
				auto account_contract_entries = contracts_of_wallet[Address(accRec->owner_key)];
				for (auto con_entry : account_contract_entries)
				{
					id_to_entry_for_contract.erase(con_entry.id);

					GenericWalletEntry entry(con_entry);
					my->remove_generic_wallet_entry(entry);
				}

				contracts_of_wallet.erase(Address(accRec->owner_key));


                name_to_account_wallet_entry_index.erase(accRec->name);
                account_id_to_wallet_entry_index.erase(accRec->id);
                GenericWalletEntry entry(*accRec);
                my->remove_generic_wallet_entry(entry);
                //从当前运行时数据中将账号纪录删除
                address_to_account_wallet_entry_index.erase(Address(accRec->owner_key));
                name_to_account_wallet_entry_index.erase(accRec->name);
                account_id_to_wallet_entry_index.erase(accRec->id);
                btc_to_alp_address.erase(accRec->owner_address());
                btc_to_alp_address.erase(Address(PtsAddress(accRec->owner_key, false, 0)));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->owner_key, true, 0)));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->owner_key, false, 56)));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->owner_key, true, 56)));
                if (accRec->is_delegate())
                {
                    btc_to_alp_address.erase(accRec->signing_address());
                    btc_to_alp_address.erase(Address(PtsAddress(accRec->signing_key(), false, 0)));
                    btc_to_alp_address.erase(Address(PtsAddress(accRec->signing_key(), true, 0)));
                    btc_to_alp_address.erase(Address(PtsAddress(accRec->signing_key(), false, 56)));
                    btc_to_alp_address.erase(Address(PtsAddress(accRec->signing_key(), true, 56)));
                }
                btc_to_alp_address.erase(Address(accRec->active_key()));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->active_key(), false, 0)));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->active_key(), true, 0)));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->active_key(), false, 56)));
                btc_to_alp_address.erase(Address(PtsAddress(accRec->active_key(), true, 56)));


                //从db中将key删除,如果不删除,scan_accounts时会将与key相关的账号存入钱包
                oWalletKeyEntry tmpRec = lookup_key(accRec->owner_address());
                if (tmpRec.valid())
                {
                    my->remove_generic_wallet_entry(GenericWalletEntry(*tmpRec));
                }
                tmpRec = lookup_key(accRec->active_address());
                if (tmpRec.valid())
                {
                    my->remove_generic_wallet_entry(GenericWalletEntry(*tmpRec));
                }
                if (accRec->is_delegate())
                {
                    tmpRec = lookup_key(accRec->signing_address());
                    if (tmpRec.valid())
                    {
                        my->remove_generic_wallet_entry(GenericWalletEntry(*tmpRec));
                    }
                }

                auto keyit_owner = keys.find(accRec->owner_address());
                if (keyit_owner != keys.end())
                {
                    GenericWalletEntry rec(keyit_owner->second);
                    my->remove_generic_wallet_entry(rec);
                    keys.erase(accRec->owner_address());
                }
                auto keyit_activekey = keys.find(Address(accRec->active_key()));
                if (keyit_activekey != keys.end())
                {
                    GenericWalletEntry rec(keyit_activekey->second);
                    my->remove_generic_wallet_entry(rec);
                    keys.erase(accRec->active_address());
                }
                if (accRec->is_delegate())
                {
                    auto keyit_sign = keys.find(accRec->signing_address());
                    if (keyit_sign != keys.end())
                    {
                        GenericWalletEntry rec(keyit_sign->second);
                        my->remove_generic_wallet_entry(rec);
                    }
                    keys.erase(accRec->signing_address());
                    accounts.erase(accRec->wallet_entry_index);
                }
                //删除对应的交易记录
                unordered_map<TransactionIdType, WalletTransactionEntry>::iterator mapit = transactions.begin();
                unordered_map<TransactionIdType, WalletTransactionEntry>::iterator tmpit;
                while (mapit != transactions.end())
                {
                    tmpit = mapit;

                    GenericWalletEntry rec(tmpit->second);
                    my->remove_generic_wallet_entry(rec);
                    mapit = transactions.erase(tmpit);
                }
            }FC_CAPTURE_AND_RETHROW((accountname))
        }

        PrivateKeyType WalletDb::get_account_child_key(const PrivateKeyType& active_private_key, uint32_t seq_num)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const ExtendedPrivateKey extended_active_private_key = ExtendedPrivateKey(active_private_key);
                fc::sha256::encoder enc;
                fc::raw::pack(enc, seq_num);
                return extended_active_private_key.child(enc.result());
            } FC_CAPTURE_AND_RETHROW((seq_num))
        }

        // Deprecated but kept for key regeneration
        PrivateKeyType WalletDb::get_account_child_key_v1(const fc::sha512& password, const Address& account_address, uint32_t seq_num)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const ExtendedPrivateKey master_private_key = wallet_master_key->decrypt_key(password);
                fc::sha256::encoder enc;
                fc::raw::pack(enc, account_address);
                fc::raw::pack(enc, seq_num);
                return master_private_key.child(enc.result());
            } FC_CAPTURE_AND_RETHROW((account_address)(seq_num))
        }

        PrivateKeyType WalletDb::generate_new_account_child_key(const fc::sha512& password, const string& account_name)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");

                oWalletAccountEntry account_entry = lookup_account(account_name);
                FC_ASSERT(account_entry.valid(), "Account not found!");
                FC_ASSERT(!account_entry->is_retracted(), "Account has been retracted!");
                FC_ASSERT(account_entry->is_my_account, "Not my account!");

                const oWalletKeyEntry key_entry = lookup_key(Address(account_entry->active_key()));
                FC_ASSERT(key_entry.valid(), "Active key not found!");
                FC_ASSERT(key_entry->has_private_key(), "Active private key not found!");

                const PrivateKeyType active_private_key = key_entry->decrypt_private_key(password);
                uint32_t seq_num = account_entry->last_used_gen_sequence;
                PrivateKeyType account_child_private_key;
                PublicKeyType account_child_public_key;
                Address account_child_address;
                while (true)
                {
                    ++seq_num;
                    FC_ASSERT(seq_num != 0, "Overflow!");

                    account_child_private_key = get_account_child_key(active_private_key, seq_num);
                    account_child_public_key = account_child_private_key.get_public_key();
                    account_child_address = Address(account_child_public_key);

                    oWalletKeyEntry key_entry = lookup_key(account_child_address);
                    if (key_entry.valid() && key_entry->has_private_key()) continue;

                    break;
                }

                account_entry->last_used_gen_sequence = seq_num;

                KeyData key;
                key.account_address = account_entry->owner_address();
                key.public_key = account_child_public_key;
                key.encrypt_private_key(password, account_child_private_key);
                key.gen_seq_number = seq_num;

                store_account(*account_entry);
                store_key(key);

                return account_child_private_key;
            } FC_CAPTURE_AND_RETHROW((account_name))
        }

        void WalletDb::add_contact_account(const AccountEntry& blockchain_account_entry, const variant& private_data)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");

                oWalletAccountEntry entry = lookup_account(blockchain_account_entry.name);
                FC_ASSERT(!entry.valid(), "Wallet already contains an account with that name!");

                const PublicKeyType account_address = blockchain_account_entry.owner_key;
                oWalletKeyEntry key_entry = lookup_key(account_address);
                FC_ASSERT(!key_entry.valid(), "Wallet already contains that key");

                entry = WalletAccountEntry();
                AccountEntry& temp_entry = *entry;
                temp_entry = blockchain_account_entry;
                entry->private_data = private_data;

                store_account(*entry);
            } FC_CAPTURE_AND_RETHROW((blockchain_account_entry))
        }

        oWalletAccountEntry WalletDb::lookup_account(const Address& account_address)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto address_map_iter = address_to_account_wallet_entry_index.find(account_address);
                if (address_map_iter != address_to_account_wallet_entry_index.end())
                {
                    const int32_t& entry_index = address_map_iter->second;
                    const auto entry_iter = accounts.find(entry_index);
                    if (entry_iter != accounts.end())
                    {
                        const WalletAccountEntry& account_entry = entry_iter->second;
                        return account_entry;
                    }
                }
                return oWalletAccountEntry();
            } FC_CAPTURE_AND_RETHROW((account_address))
        }

        oWalletAccountEntry WalletDb::lookup_account(const string& account_name)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto name_map_iter = name_to_account_wallet_entry_index.find(account_name);
                if (name_map_iter != name_to_account_wallet_entry_index.end())
                {
                    const int32_t& entry_index = name_map_iter->second;
                    const auto entry_iter = accounts.find(entry_index);
                    if (entry_iter != accounts.end())
                    {
                        const WalletAccountEntry& account_entry = entry_iter->second;
                        return account_entry;
                    }
                }
                return oWalletAccountEntry();
            } FC_CAPTURE_AND_RETHROW((account_name))
        }

        oWalletAccountEntry WalletDb::lookup_account(const AccountIdType account_id)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto id_map_iter = account_id_to_wallet_entry_index.find(account_id);
                if (id_map_iter != account_id_to_wallet_entry_index.end())
                {
                    const int32_t& entry_index = id_map_iter->second;
                    const auto entry_iter = accounts.find(entry_index);
                    if (entry_iter != accounts.end())
                    {
                        const WalletAccountEntry& account_entry = entry_iter->second;
                        return account_entry;
                    }
                }
                return oWalletAccountEntry();
            } FC_CAPTURE_AND_RETHROW((account_id))
        }

        void WalletDb::store_account(const AccountData& account, fc::sha512 password)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(account.name != string());
                FC_ASSERT(account.owner_key != PublicKeyType());

                oWalletAccountEntry account_entry = lookup_account(account.owner_address());
                oWalletAccountEntry account_entry_cc = lookup_account(account.owner_address());
                if (!account_entry.valid())
                    account_entry = WalletAccountEntry();

                AccountData& temp = *account_entry;
                temp = account;

                store_and_reload_entry(*account_entry);

                set<PublicKeyType> account_public_keys;
                account_public_keys.insert(account_entry->owner_key);
                for (const auto& active_key_item : account_entry->active_key_history)
                {
                    const PublicKeyType& active_key = active_key_item.second;
                    if (active_key == PublicKeyType()) continue;
                    account_public_keys.insert(active_key);
                }
                if (account.is_delegate())
                {
                    for (const auto& item : account.delegate_info->signing_key_history)
                    {
                        const PublicKeyType& signing_key = item.second;
                        if (signing_key == PublicKeyType()) continue;
                        account_public_keys.insert(signing_key);
                    }
                }

                for (const PublicKeyType& account_public_key : account_public_keys)
                {
                    const Address account_address = Address(account_public_key);
                    oWalletKeyEntry key_entry = lookup_key(account_address);
                    if (!key_entry.valid())
                    {
                        KeyData key;
                        key.account_address = account_address;
                        key.public_key = account_public_key;
                        store_key(key);
                    }
                    else if (key_entry->has_private_key())
                    {
                        if (!account_entry->is_my_account)
                        {
                            account_entry->is_my_account = true;
                            store_and_reload_entry(*account_entry);
                        }

                        if (key_entry->account_address != account_entry->owner_address())
                        {
                            key_entry->account_address = account_entry->owner_address();
                            store_key(*key_entry);
                        }
                    }
                }
                for (const PublicKeyType& account_public_key : account_public_keys)
                {
                    oWalletKeyEntry key_entry = lookup_key(Address(account_public_key));
                    if ((!key_entry.valid() || !key_entry->has_private_key()) && password != fc::sha512())
                    {
                        auto owner_key_entry = lookup_key(account_entry_cc->owner_address());
                        if (owner_key_entry.valid() && owner_key_entry->has_private_key())
                        {
                            auto owner_key = owner_key_entry->decrypt_private_key(password);
                            PrivateKeyType active_private_key;
                            PublicKeyType active_public_key_temp;
                            active_private_key = get_account_child_key(owner_key, 0);
                            active_public_key_temp = active_private_key.get_public_key();
                            if (active_public_key_temp == account_public_key)
                            {
                                KeyData active_key;
                                active_key.account_address = Address(active_public_key_temp);
                                active_key.public_key = active_public_key_temp;
                                active_key.encrypt_private_key(password, active_private_key);
                                store_key(active_key);
                            }
                        }
                    }
                }
            } FC_CAPTURE_AND_RETHROW((account))
        }

        void WalletDb::store_account(const blockchain::AccountEntry& blockchain_account_entry, fc::sha512 password)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");

                oWalletAccountEntry account_entry = lookup_account(blockchain_account_entry.owner_address());
                if (!account_entry.valid())
                    account_entry = WalletAccountEntry();

                blockchain::AccountEntry& temp = *account_entry;
                temp = blockchain_account_entry;

                store_account(*account_entry, password);
            } FC_CAPTURE_AND_RETHROW((blockchain_account_entry))
        }
		
		oWalletContractEntry WalletDb::lookup_contract(const ContractIdType& contract_id)const
		{
			try
			{
				auto iter = id_to_entry_for_contract.find(contract_id);
				if (iter != id_to_entry_for_contract.end())
				{
					return iter->second;
				}
			
				return oWalletContractEntry();

			} FC_CAPTURE_AND_RETHROW((contract_id))

		}

		void WalletDb::store_contract(const blockchain::ContractEntry& blockchain_contract_entry)
		{
			try
			{
				FC_ASSERT(is_open(), "Wallet not open!");

				oWalletContractEntry contract_entry = lookup_contract(blockchain_contract_entry.id);

				if (contract_entry.valid())
					return;

				contract_entry = blockchain_contract_entry;

				store_and_reload_entry(*contract_entry);


				
			}FC_CAPTURE_AND_RETHROW((blockchain_contract_entry))
			
		}

		

        oWalletKeyEntry WalletDb::lookup_key(const Address& derived_address)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto address_map_iter = btc_to_alp_address.find(derived_address);
                if (address_map_iter != btc_to_alp_address.end())
                {
                    const Address& key_address = address_map_iter->second;
                    const auto entry_iter = keys.find(key_address);
                    if (entry_iter != keys.end())
                    {
                        const WalletKeyEntry& key_entry = entry_iter->second;
                        return key_entry;
                    }
                }
                return oWalletKeyEntry();
            } FC_CAPTURE_AND_RETHROW((derived_address))
        }

        void WalletDb::store_key(const KeyData& key)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(key.public_key != PublicKeyType());

                oWalletKeyEntry key_entry = lookup_key(key.get_address());
                if (!key_entry.valid())
                    key_entry = WalletKeyEntry();

                KeyData& temp = *key_entry;
                temp = key;

                store_and_reload_entry(*key_entry, true);

                if (key_entry->has_private_key())
                {
                    oWalletAccountEntry account_entry = lookup_account(key.public_key);
                    if (!account_entry.valid())
                        account_entry = lookup_account(key.account_address);

                    if (account_entry.valid())
                    {
                        if (key_entry->account_address != account_entry->owner_address())
                        {
                            key_entry->account_address = account_entry->owner_address();
                            store_and_reload_entry(*key_entry, true);
                        }

                        if (!account_entry->is_my_account)
                        {
                            account_entry->is_my_account = true;
                            store_account(*account_entry);
                        }
                    }
                }
            } FC_CAPTURE_AND_RETHROW((key))
        }

        void WalletDb::import_key(const fc::sha512& password, const string& account_name, const PrivateKeyType& private_key,
            bool move_existing)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");

                oWalletAccountEntry account_entry = lookup_account(account_name);
                FC_ASSERT(account_entry.valid(), "Account name not found!");

                const PublicKeyType public_key = private_key.get_public_key();

                oWalletKeyEntry key_entry = lookup_key(Address(public_key));
                if (!key_entry.valid())
                    key_entry = WalletKeyEntry();
                else if (!move_existing)
                    FC_ASSERT(key_entry->account_address == account_entry->owner_address());

                key_entry->account_address = account_entry->owner_address();
                key_entry->public_key = public_key;
                key_entry->encrypt_private_key(password, private_key);

                store_key(*key_entry);
            } FC_CAPTURE_AND_RETHROW((account_name)(move_existing))
        }

        oWalletContactEntry WalletDb::lookup_contact(const variant& data)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(!data.is_null());

                const auto iter = contacts.find(data.as_string());
                if (iter != contacts.end()) return iter->second;

                return oWalletContactEntry();
            } FC_CAPTURE_AND_RETHROW((data))
        }

        oWalletContactEntry WalletDb::lookup_contact(const string& label)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(!label.empty());

                const auto iter = label_to_account_or_contact.find(label);
                if (iter != label_to_account_or_contact.end()) return lookup_contact(variant(iter->second));

                return oWalletContactEntry();
            } FC_CAPTURE_AND_RETHROW((label))
        }

        WalletContactEntry WalletDb::store_contact(const ContactData& contact)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(!contact.data.is_null());
                FC_ASSERT(!contact.label.empty());

                // Check for label collision
                oWalletContactEntry contact_entry = lookup_contact(contact.label);
                if (contact_entry.valid() && contact_entry->data.as_string() != contact.data.as_string())
                    FC_CAPTURE_AND_THROW(label_already_in_use, );

                contact_entry = lookup_contact(contact.data);
                if (!contact_entry.valid())
                    contact_entry = WalletContactEntry();
                else if (contact_entry->label != contact.label)
                    label_to_account_or_contact.erase(contact_entry->label);

                ContactData& temp = *contact_entry;
                temp = contact;

                store_and_reload_entry(*contact_entry);
                return *contact_entry;
            } FC_CAPTURE_AND_RETHROW((contact))
        }

        oWalletContactEntry WalletDb::remove_contact(const variant& data)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(!data.is_null());

                oWalletContactEntry entry = lookup_contact(data);
                if (entry.valid())
                {
                    label_to_account_or_contact.erase(entry->label);
                    contacts.erase(data.as_string());
                    remove_item(entry->wallet_entry_index);
                }

                return entry;
            } FC_CAPTURE_AND_RETHROW((data))
        }

        oWalletContactEntry WalletDb::remove_contact(const string& label)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(!label.empty());

                oWalletContactEntry entry = lookup_contact(label);
                if (entry.valid())
                    return remove_contact(entry->data);

                return entry;
            } FC_CAPTURE_AND_RETHROW((label))
        }

        oWalletTransactionEntry WalletDb::lookup_transaction(const TransactionIdType& id)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const auto id_map_iter = id_to_transaction_entry_index.find(id);
                if (id_map_iter != id_to_transaction_entry_index.end())
                {
                    const TransactionIdType& entry_id = id_map_iter->second;
                    const auto entry_iter = transactions.find(entry_id);
                    if (entry_iter != transactions.end())
                    {
                        const WalletTransactionEntry& transaction_entry = entry_iter->second;
                        return transaction_entry;
                    }
                }
                return oWalletTransactionEntry();
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void WalletDb::store_transaction(const TransactionData& transaction)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(transaction.entry_id != TransactionIdType());
                FC_ASSERT(transaction.is_virtual || transaction.trx.id() != SignedTransaction().id());

                oWalletTransactionEntry transaction_entry = lookup_transaction(transaction.entry_id);
                if (!transaction_entry.valid())
                    transaction_entry = WalletTransactionEntry();

                TransactionData& temp = *transaction_entry;
                temp = transaction;

                store_and_reload_entry(*transaction_entry);
            } FC_CAPTURE_AND_RETHROW((transaction))
        }

        PrivateKeyType WalletDb::generate_new_one_time_key(const fc::sha512& password)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const PrivateKeyType one_time_private_key = PrivateKeyType::generate();

                const Address one_time_address = Address(one_time_private_key.get_public_key());
                const oWalletKeyEntry key_entry = lookup_key(one_time_address);
                FC_ASSERT(!key_entry.valid());

                KeyData key;
                key.public_key = one_time_private_key.get_public_key();
                key.encrypt_private_key(password, one_time_private_key);
                store_key(key);

                return one_time_private_key;
            } FC_CAPTURE_AND_RETHROW()
        }

        // Only returns private keys corresponding to owner and active keys
        map<PrivateKeyType, string> WalletDb::get_account_private_keys(const fc::sha512& password)const
        {
            try {
                map<PublicKeyType, string> public_keys;
                for (const auto& account_item : accounts)
                {
                    const auto& account = account_item.second;
                    public_keys[account.owner_key] = account.name;
                    for (const auto& active_key_item : account.active_key_history)
                    {
                        const auto& active_key = active_key_item.second;
                        if (active_key != PublicKeyType())
                            public_keys[active_key] = account.name;
                    }
                }

                map<PrivateKeyType, string> private_keys;
                for (const auto& public_key_item : public_keys)
                {
                    const auto& public_key = public_key_item.first;
                    const auto& account_name = public_key_item.second;

                    const auto key_entry = lookup_key(public_key);
                    if (!key_entry.valid() || !key_entry->has_private_key())
                        continue;

                    try
                    {
                        private_keys[key_entry->decrypt_private_key(password)] = account_name;
                    }
                    catch (const fc::exception& e)
                    {
                        elog("Error decrypting private key: ${e}", ("e", e.to_detail_string()));
                    }
                }
                return private_keys;
            } FC_CAPTURE_AND_RETHROW()
        }

        void WalletDb::repair_entrys(const fc::sha512& password)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");

                vector<GenericWalletEntry> entrys;
                for (auto iter = my->_entrys.begin(); iter.valid(); ++iter)
                    entrys.push_back(iter.value());


                // Repair account_data.is_my_account and key_data.account_address
                uint32_t count = 0;
                for (GenericWalletEntry& entry : entrys)
                {
                    try
                    {
                        if (WalletEntryTypeEnum(entry.type) == account_entry_type)
                        {
                            std::cout << "\rRepairing account entry " << std::to_string(++count) << std::flush;
                            WalletAccountEntry account_entry = entry.as<WalletAccountEntry>();
                            store_account(account_entry);
                        }
                    }
                    catch (...)
                    {
                    }
                }

                // Repair key_data.public_key when I have the private key and
                // repair key_data.account_address and account_data.is_my_account
                count = 0;
                for (GenericWalletEntry& entry : entrys)
                {
                    try
                    {
                        if (WalletEntryTypeEnum(entry.type) == key_entry_type)
                        {
                            std::cout << "\rRepairing key entry     " << std::to_string(++count) << std::flush;
                            WalletKeyEntry key_entry = entry.as<WalletKeyEntry>();
                            if (key_entry.has_private_key())
                            {
                                const PrivateKeyType private_key = key_entry.decrypt_private_key(password);
                                const PublicKeyType public_key = private_key.get_public_key();
                                if (key_entry.public_key != public_key)
                                {
                                    const Address key_address = key_entry.get_address();
                                    keys.erase(key_address);
                                    btc_to_alp_address.erase(key_address);
                                    btc_to_alp_address.erase(Address(PtsAddress(key_entry.public_key, false, 0)));
                                    btc_to_alp_address.erase(Address(PtsAddress(key_entry.public_key, true, 0)));
                                    btc_to_alp_address.erase(Address(PtsAddress(key_entry.public_key, false, 56)));
                                    btc_to_alp_address.erase(Address(PtsAddress(key_entry.public_key, true, 56)));

                                    key_entry.public_key = public_key;
                                    my->load_key_entry(key_entry);
                                }
                            }
                            store_key(key_entry);
                        }
                    }
                    catch (...)
                    {
                    }
                }

                // Repair transaction_data.entry_id
                count = 0;
                for (GenericWalletEntry& entry : entrys)
                {
                    try
                    {
                        if (WalletEntryTypeEnum(entry.type) == transaction_entry_type)
                        {
                            std::cout << "\rRepairing transaction entry     " << std::to_string(++count) << std::flush;
                            WalletTransactionEntry transaction_entry = entry.as<WalletTransactionEntry>();
                            if (transaction_entry.trx.id() != SignedTransaction().id())
                            {
                                const TransactionIdType entry_id = transaction_entry.trx.id();
                                if (transaction_entry.entry_id != entry_id)
                                {
                                    transactions.erase(transaction_entry.entry_id);
                                    id_to_transaction_entry_index.erase(transaction_entry.entry_id);

                                    transaction_entry.entry_id = entry_id;
                                    my->load_transaction_entry(transaction_entry);
                                }
                            }
                            store_transaction(transaction_entry);
                        }
                    }
                    catch (...)
                    {
                    }
                }
                std::cout << "\rWallet entrys repaired.                                  " << std::flush << "\n";
            } FC_CAPTURE_AND_RETHROW()
        }

        void WalletDb::set_property(PropertyEnum property_id, const variant& v)
        {
            WalletPropertyEntry property_entry;
            auto property_itr = properties.find(property_id);
            if (property_itr != properties.end())
            {
                property_entry = property_itr->second;
                property_entry.value = v;
            }
            else
            {
                if (property_id == PropertyEnum::next_entry_number)
                    property_entry = WalletPropertyEntry(WalletProperty(property_id, v), 1);
                else
                    property_entry = WalletPropertyEntry(WalletProperty(property_id, v), new_wallet_entry_index());
            }
            store_and_reload_entry(property_entry);
        }

        variant WalletDb::get_property(PropertyEnum property_id)const
        {
            auto property_itr = properties.find(property_id);
            if (property_itr != properties.end()) return property_itr->second.value;
            return variant();
        }

        string WalletDb::get_account_name(const Address& account_address)const
        {
            auto opt = lookup_account(account_address);
            if (opt) return opt->name;
            return "?";
        }

        void WalletDb::store_setting(const string& name, const variant& value)
        {
            auto orec = lookup_setting(name);
            if (orec.valid())
            {
                orec->value = value;
                settings[name] = *orec;
                store_and_reload_entry(*orec);
            }
            else
            {
                auto rec = WalletSettingEntry(Setting(name, value), new_wallet_entry_index());
                settings[name] = rec;
                store_and_reload_entry(rec);
            }
        }

        vector<WalletTransactionEntry> WalletDb::get_pending_transactions()const
        {
            vector<WalletTransactionEntry> transaction_entrys;
            for (const auto& item : transactions)
            {
                const auto& transaction_entry = item.second;
                if (!transaction_entry.is_virtual && !transaction_entry.is_confirmed)
                    transaction_entrys.push_back(transaction_entry);
            }
            return transaction_entrys;
        }

        void WalletDb::export_to_json(const path& filename)const
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                FC_ASSERT(!fc::exists(filename), "Filename to export to already exists!");

                const auto dir = fc::absolute(filename).parent_path();
                if (!fc::exists(dir))
                    fc::create_directories(dir);

                std::ofstream fs(filename.string());
                fs.write("[\n", 2);

                auto itr = my->_entrys.begin();
                while (itr.valid())
                {
                    auto str = fc::json::to_pretty_string(itr.value());
                    if ((++itr).valid()) str += ",";
                    str += "\n";
                    fs.write(str.c_str(), str.size());
                }

                fs.write("]", 1);
            } FC_CAPTURE_AND_RETHROW((filename))
        }

        void WalletDb::import_from_json(const path& filename)
        {
            try {
                FC_ASSERT(fc::exists(filename));
                FC_ASSERT(is_open(), "Wallet not open!");

                auto entrys = fc::json::from_file<std::vector<GenericWalletEntry>>(filename);
                for (const auto& entry : entrys)
                {
                    try
                    {
                        store_and_reload_generic_entry(entry);
                        // Prevent hanging on large wallets
                        fc::usleep(fc::milliseconds(1));
                    }
                    catch (const fc::canceled_exception&)
                    {
                        throw;
                    }
                    catch (const fc::exception& e)
                    {
                        elog("Error loading wallet entry:\n${r}\nReason: ${e}", ("e", e.to_detail_string())("r", entry));

                        switch (WalletEntryTypeEnum(entry.type))
                        {
                        case master_key_entry_type:
                        case key_entry_type:
                            throw;
                        default:
                            break;
                        }
                    }
                }
            } FC_CAPTURE_AND_RETHROW((filename))
        }

        bool WalletDb::has_private_key(const Address& addr)const
        {
            try {
                auto itr = keys.find(addr);
                if (itr != keys.end())
                {
                    return itr->second.has_private_key();
                }
                return false;
            } FC_CAPTURE_AND_RETHROW((addr))
        }

        void WalletDb::cache_memo(const MemoStatus& memo,
            const PrivateKeyType& account_key,
            const fc::sha512& password)
        {
            KeyData data;
            data.account_address = Address(account_key.get_public_key());
            data.public_key = memo.owner_private_key.get_public_key();
            data.encrypt_private_key(password, memo.owner_private_key);
            data.valid_from_signature = memo.has_valid_signature;
            //data.memo = memo_data( memo );
            store_key(data);
        }

        oWalletSettingEntry WalletDb::lookup_setting(const string& name)const
        {
            auto itr = settings.find(name);
            if (itr != settings.end())
            {
                return itr->second;
            }
            return oWalletSettingEntry();
        }

        void WalletDb::remove_contact_account(const string& account_name)
        {
            const oWalletAccountEntry account_entry = lookup_account(account_name);
            FC_ASSERT(account_entry.valid());
            FC_ASSERT(!account_entry->is_my_account);

            const int32_t& entry_index = account_entry->wallet_entry_index;

            accounts.erase(entry_index);

            address_to_account_wallet_entry_index.erase(Address(account_entry->owner_key));
            for (const auto& item : account_entry->active_key_history)
            {
                const PublicKeyType& active_key = item.second;
                address_to_account_wallet_entry_index.erase(Address(active_key));
            }
            if (account_entry->is_delegate())
            {
                for (const auto& item : account_entry->delegate_info->signing_key_history)
                {
                    const PublicKeyType& signing_key = item.second;
                    address_to_account_wallet_entry_index.erase(Address(signing_key));
                }
            }

            name_to_account_wallet_entry_index.erase(account_entry->name);
            account_id_to_wallet_entry_index.erase(account_entry->id);

            // TODO: Remove key entrys and indexes
        }

        void WalletDb::rename_account(const PublicKeyType &old_account_key,
            const string& new_account_name)
        {
            /* Precondition: check that new_account doesn't exist in wallet and that old_account does
             */
            FC_ASSERT(is_open(), "Wallet not open!");

            auto opt_old_acct = lookup_account(old_account_key);
            FC_ASSERT(opt_old_acct.valid());
            auto acct = *opt_old_acct;
            auto old_name = acct.name;
            acct.name = new_account_name;
            name_to_account_wallet_entry_index[acct.name] = acct.wallet_entry_index;
            if (name_to_account_wallet_entry_index[old_name] == acct.wallet_entry_index)
                //Only remove the old name from the map if it pointed to the entry we've just renamed
                name_to_account_wallet_entry_index.erase(old_name);
            accounts[acct.wallet_entry_index] = acct;
            address_to_account_wallet_entry_index[Address(acct.owner_key)] = acct.wallet_entry_index;
            for (const auto& time_key_pair : acct.active_key_history)
                address_to_account_wallet_entry_index[Address(time_key_pair.second)] = acct.wallet_entry_index;

            store_and_reload_entry(acct);
        }

        void WalletDb::remove_item(int32_t index)
        {
            try {
                try
                {
                    my->_entrys.remove(index);
                }
                catch (const fc::key_not_found_exception&)
                {
                    wlog("wallet_db tried to remove nonexistent index: ${i}", ("i", index));
                }
            } FC_CAPTURE_AND_RETHROW((index))
        }

        bool WalletDb::validate_password(const fc::sha512& password)const
        {
            try {
                FC_ASSERT(wallet_master_key);
                return wallet_master_key->validate_password(password);
            } FC_CAPTURE_AND_RETHROW()
        }

        void WalletDb::set_master_key(const ExtendedPrivateKey& extended_key,
            const fc::sha512& new_password)
        {
            try {
                MasterKey key;
                key.encrypt_key(new_password, extended_key);
                auto key_entry = WalletMasterKeyEntry(key, -1);
                store_and_reload_entry(key_entry, true);
            } FC_CAPTURE_AND_RETHROW()
        }

        void WalletDb::change_password(const fc::sha512& old_password,
            const fc::sha512& new_password)
        {
            try {
                FC_ASSERT(is_open(), "Wallet not open!");
                const ExtendedPrivateKey master_private_key = wallet_master_key->decrypt_key(old_password);
                set_master_key(master_private_key, new_password);

                for (auto key : keys)
                {
                    if (key.second.has_private_key())
                    {
                        auto priv_key = key.second.decrypt_private_key(old_password);
                        key.second.encrypt_private_key(new_password, priv_key);
                        store_and_reload_entry(key.second, true);
                    }
                }
            } FC_CAPTURE_AND_RETHROW()
        }

        void WalletDb::remove_transaction(const TransactionIdType& entry_id)
        {
            const auto rec = lookup_transaction(entry_id);
            if (!rec.valid()) return;
            remove_item(rec->wallet_entry_index);
            transactions.erase(entry_id);
        }

    }
} // thinkyoung::wallet
