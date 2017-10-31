#pragma once

#include <wallet/WalletEntrys.hpp>

namespace thinkyoung {
    namespace wallet {

        namespace detail { class WalletDbImpl; }

        class WalletDb
        {
        public:
            WalletDb();
            ~WalletDb();

            void open(const fc::path& wallet_file);
            void close();
            bool is_open()const;

            int32_t new_wallet_entry_index();

            void        set_property(PropertyEnum property_id, const fc::variant& v);
            fc::variant get_property(PropertyEnum property_id)const;

            // ********************************************************************
            // Most recently rewritten by Vikram

            // Wallet child keys
            uint32_t               get_last_wallet_child_key_index()const;
            void                   set_last_wallet_child_key_index(uint32_t key_index);
            PrivateKeyType       get_wallet_child_key(const fc::sha512& password, uint32_t key_index)const;
            PublicKeyType        generate_new_account(const fc::sha512& password, const string& account_name,
                const variant& private_data);

            void remove_account(const string& accountname);
            // Account child keys
            PrivateKeyType       get_account_child_key(const PrivateKeyType& active_private_key, uint32_t seq_num)const;
            PrivateKeyType       get_account_child_key_v1(const fc::sha512& password, const Address& account_address,
                uint32_t seq_num)const;
            PrivateKeyType       generate_new_account_child_key(const fc::sha512& password, const string& account_name);

            void                   add_contact_account(const AccountEntry& blockchain_account_entry, const variant& private_data);

            // Account getters and setters
            oWalletAccountEntry lookup_account(const Address& account_address)const;
            oWalletAccountEntry lookup_account(const string& account_name)const;
            oWalletAccountEntry lookup_account(const AccountIdType account_id)const;
            void                   store_account(const AccountData& account, fc::sha512 password = fc::sha512());
            void                   store_account(const blockchain::AccountEntry& blockchain_account_entry, fc::sha512 password = fc::sha512());

            // Key getters and setters
            oWalletKeyEntry     lookup_key(const Address& derived_address)const;
            void                   store_key(const KeyData& key);
            void                   import_key(const fc::sha512& password, const string& account_name,
                const PrivateKeyType& private_key, bool move_existing);

            // Contact getters and setters
            oWalletContactEntry lookup_contact(const variant& data)const;
            oWalletContactEntry lookup_contact(const string& label)const;
            WalletContactEntry  store_contact(const ContactData& contact);
            oWalletContactEntry remove_contact(const variant& data);
            oWalletContactEntry remove_contact(const string& label);

            // Transaction getters and setters
            oWalletTransactionEntry lookup_transaction(const TransactionIdType& id)const;
            void store_transaction(const TransactionData& transaction);

            // Non-deterministic and not linked to any account
            PrivateKeyType       generate_new_one_time_key(const fc::sha512& password);

            map<PrivateKeyType, string> get_account_private_keys(const fc::sha512& password)const;

            // Restore as many broken entry invariants as possible
            void                   repair_entrys(const fc::sha512& password);
            // ********************************************************************

            void cache_memo(const MemoStatus& memo,
                const PrivateKeyType& account_key,
                const fc::sha512& password);

            void remove_transaction(const TransactionIdType& entry_id);

            vector<WalletTransactionEntry> get_pending_transactions()const;

            string                        get_account_name(const Address& account_address)const;

            oWalletSettingEntry   lookup_setting(const string& name)const;
            void                     store_setting(const string& name, const variant& value);

            bool has_private_key(const Address& a)const;

            void remove_contact_account(const string& account_name);

            void rename_account(const PublicKeyType& old_account_key,
                const string& new_account_name);
            /**
            * Exports the current wallet to a JSON file.
            *
            * @param json_filename the full path and filename of JSON file to generate
            *
            * @return void
            */
            void export_to_json(const path& filename)const;
            void import_from_json(const path& filename);

            bool validate_password(const fc::sha512& password)const;

            void set_master_key(const ExtendedPrivateKey& key,
                const fc::sha512& new_password);

            void change_password(const fc::sha512& old_password,
                const fc::sha512& new_password);

            const unordered_map<TransactionIdType, WalletTransactionEntry>& get_transactions()const { return transactions; }
            const unordered_map<int32_t, WalletAccountEntry>& get_accounts()const { return accounts; }
            const unordered_map<Address, WalletKeyEntry>& get_keys()const { return keys; }
            const unordered_map<string, WalletContactEntry>& get_contacts()const { return contacts; }

			//thinkyoung3.0 contracts related
			const unordered_map<Address, vector<WalletContractEntry>>& get_contracts() const { return contracts_of_wallet; }
			const unordered_map<ContractIdType, WalletContractEntry>& get_id_contract_map() const { return id_to_entry_for_contract; }
			void store_contract(const blockchain::ContractEntry& blockchain_contract_entry);
			oWalletContractEntry lookup_contract(const ContractIdType& contract_id)const;

            unordered_map<TransactionIdType, TransactionLedgerEntry>   experimental_transactions;

        private:
            map<PropertyEnum, WalletPropertyEntry>                     properties;
            optional<WalletMasterKeyEntry>                             wallet_master_key;
            unordered_map<int32_t, WalletAccountEntry>                  accounts;
            unordered_map<Address, WalletKeyEntry>                      keys;
            unordered_map<string, WalletContactEntry>                   contacts;
            unordered_map<TransactionIdType, WalletTransactionEntry>  transactions;
            unordered_map<string, WalletSettingEntry>                   settings;

            // Caches to lookup accounts
            unordered_map<Address, int32_t>                                address_to_account_wallet_entry_index;
            unordered_map<string, int32_t>                                 name_to_account_wallet_entry_index;
            unordered_map<AccountIdType, int32_t>                        account_id_to_wallet_entry_index;

            // Cache to lookup keys
            unordered_map<Address, Address>                                btc_to_alp_address;

            // Cache to lookup accounts and contacts
            unordered_map<string, string>                                  label_to_account_or_contact;

            // Cache to lookup transactions
            unordered_map<TransactionIdType, TransactionIdType>        id_to_transaction_entry_index;

			// wallet contract entry db related
			unordered_map<Address, vector<WalletContractEntry>>                  contracts_of_wallet;
			unordered_map<ContractIdType, WalletContractEntry>                   id_to_entry_for_contract;

            void remove_item(int32_t index);

            template<typename T>
            void store_and_reload_entry(T& entry_to_store, const bool sync = false)
            {
                if (entry_to_store.wallet_entry_index == 0)
                    entry_to_store.wallet_entry_index = new_wallet_entry_index();
                store_and_reload_generic_entry(GenericWalletEntry(entry_to_store), sync);
            }

            void store_and_reload_generic_entry(const GenericWalletEntry& entry, const bool sync = false);

            friend class detail::WalletDbImpl;
            unique_ptr<detail::WalletDbImpl> my;
        };

    }
} // thinkyoung::wallet
