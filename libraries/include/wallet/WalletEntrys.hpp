/* NOTE: Avoid renaming any entry members because there will be no way to
 * unserialize existing downstream wallets */

#pragma once

#include <blockchain/AccountEntry.hpp>
#include <blockchain/BalanceEntry.hpp>
#include <blockchain/ExtendedAddress.hpp>
#include <blockchain/Transaction.hpp>
#include <blockchain/Types.hpp>
#include <blockchain/ContractEntry.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/optional.hpp>
#include <fc/reflect/variant.hpp>

namespace thinkyoung {
    namespace wallet {
        using namespace thinkyoung::blockchain;

        enum WalletEntryTypeEnum
        {
            master_key_entry_type = 0,
            account_entry_type = 1,
            key_entry_type = 2,
            transaction_entry_type = 3,
            contact_entry_type = 4,
            property_entry_type = 7,
            setting_entry_type = 9,
			contract_entry_type = 10
        };

        enum PropertyEnum
        {
            version,
            next_entry_number,
            next_child_key_index,
            automatic_backups,
            transaction_scanning,
            last_unlocked_scanned_block_number,
            default_transaction_priority_fee,
            transaction_expiration_sec,
            transaction_min_imessage_soft_length,
            transaction_min_imessage_fee_coe,
            last_scanned_block_number_for_thinkyoung
        };

        struct WalletProperty
        {
            WalletProperty(PropertyEnum k = next_entry_number,
                fc::variant v = fc::variant())
                :key(k), value(v){}

            fc::enum_type<int32_t, PropertyEnum> key;
            fc::variant                           value;
        };

        struct MasterKey
        {
            std::vector<char>              encrypted_key;
            fc::sha512                     checksum;

            bool                           validate_password(const fc::sha512& password)const;
            ExtendedPrivateKey           decrypt_key(const fc::sha512& password)const;
            void                           encrypt_key(const fc::sha512& password,
                const ExtendedPrivateKey& k);
        };

        struct AccountData : public thinkyoung::blockchain::AccountEntry
        {
            bool                             is_my_account = false;
            int8_t                           approved = 0;
            bool                             is_favorite = false;
            bool                             block_production_enabled = false;
            uint32_t                         last_used_gen_sequence = 0;
            variant                          private_data;
        };

        struct AccountAddressData :public AccountData
        {
            AccountAddressData(){}
            AccountAddressData(const AccountData & data) :AccountData(data)
            {
                owner_address = data.owner_address();
                active_address = data.active_address();
            }
            Address owner_address;
            Address active_address;
        };

        struct KeyData
        {
            Address                          account_address;
            PublicKeyType                  public_key;
            std::vector<char>                encrypted_private_key;
            bool                             valid_from_signature = false;
            optional<string>                 memo; // this memo is not used for anything.
            uint32_t                         gen_seq_number = 0;

            Address                          get_address()const { return Address(public_key); }
            bool                             has_private_key()const;
            void                             encrypt_private_key(const fc::sha512& password,
                const PrivateKeyType&);
            PrivateKeyType                 decrypt_private_key(const fc::sha512& password)const;
        };

        struct ContactData
        {
            enum class ContactTypeEnum : uint8_t
            {
                account_name = 0,
                public_key = 1,
                address = 2,
                btc_address = 3
            };

            ContactData() {}

            explicit ContactData(const string& name)
                : contact_type(ContactTypeEnum::account_name), data(variant(name)), label(name) {}

            explicit ContactData(const PublicKeyType& key)
                : contact_type(ContactTypeEnum::public_key), data(variant(key)), label(string(key)) {}

            explicit ContactData(const Address& addr)
                : contact_type(ContactTypeEnum::address), data(variant(addr)), label(string(addr)) {}

            explicit ContactData(const PtsAddress& addr)
                : contact_type(ContactTypeEnum::btc_address), data(variant(addr)), label(string(addr)) {}

            ContactData(const ChainInterface& db, const string& data, const string& label = "");

            ContactTypeEnum    contact_type;
            variant              data;
            string               label;
            bool                 favorite = false;
        };

        struct LedgerEntry;
        struct TransactionData
        {
            /*
             * entry_id
             * - non-virtual transactions: trx.id()
             * - virtual genesis claims: fc::ripemd160::hash( owner_account_name )
             * - virtual market bids: fc::ripemd160::hash( block_num + string( mtrx.bid_owner ) + string( mtrx.ask_owner ) )
             * - virtual market asks: fc::ripemd160::hash( block_num + string( mtrx.ask_owner ) + string( mtrx.bid_owner ) )
             */
            TransactionIdType       entry_id;
            uint32_t                  block_num = 0;
            bool                      is_virtual = false;
            bool                      is_confirmed = false;
            bool                      is_market = false;
            SignedTransaction        trx;
            vector<LedgerEntry>      ledger_entries;
            Asset                     fee;
            fc::time_point_sec        created_time;
            fc::time_point_sec        received_time;
            vector<Address>           extra_addresses;
        };

        struct LedgerEntry
        {
            optional<PublicKeyType> from_account;
            optional<PublicKeyType> to_account;
			optional<BalanceIdType> from_contract_balance;
			optional<BalanceIdType> to_contract_balance;
            Asset                     amount;
            string                    memo;
            optional<PublicKeyType> memo_from_account;
        };

        // don't use -- work in progress
        struct TransactionLedgerEntry
        {
            TransactionIdType              id;

            uint32_t                         block_num = uint32_t(-1);
            BlockIdType                    block_id;

            time_point_sec                   timestamp = time_point_sec(-1);

            // e.g. { name, INCOME-name, ISSUER-name, `snapshot address`, {ASK,BID,SHORT,MARGIN}-id, FEE }
            map<string, vector<Asset>>       delta_amounts;

            optional<TransactionIdType>    transaction_id;

            // only really useful for titan transfers
            map<uint16_t, string>            delta_labels;

            map<uint16_t, string>            operation_notes;

            bool is_confirmed()const { return block_num != uint32_t(-1); }
            bool is_virtual()const   { return !transaction_id.valid(); }

            friend bool operator < (const TransactionLedgerEntry& a, const TransactionLedgerEntry& b)
            {
                return std::tie(a.block_num, a.timestamp, a.id) < std::tie(b.block_num, b.timestamp, b.id);
            }
        };

        struct PrettyTransactionExperimental : TransactionLedgerEntry

        {
            vector<std::pair<string, Asset>>         inputs;
            vector<std::pair<string, Asset>>         outputs;
            mutable vector<std::pair<string, Asset>> balances;
            vector<string>                           notes;
        };

        /* Used to store GUI preferences and such */
        struct Setting
        {
            Setting(){};
            Setting(string name, variant value) :name(name), value(value){};
            string       name;
            variant      value;
        };

        template<WalletEntryTypeEnum entryType>
        struct BaseEntry
        {
            enum { type = entryType };

            BaseEntry(int32_t idx = 0) :wallet_entry_index(idx){}
            int32_t wallet_entry_index;
        };

        template<typename entryTypeName, WalletEntryTypeEnum entryTypeNumber>
        struct WalletEntry : public BaseEntry<entryTypeNumber>, public entryTypeName
        {
            WalletEntry(){}
            WalletEntry(const entryTypeName& rec, int32_t wallet_entry_index = 0)
                :BaseEntry<entryTypeNumber>(wallet_entry_index), entryTypeName(rec){}
        };

        typedef WalletEntry<WalletProperty, property_entry_type>       WalletPropertyEntry;
        typedef WalletEntry<MasterKey, master_key_entry_type>     WalletMasterKeyEntry;
        typedef WalletEntry<AccountData, account_entry_type>        WalletAccountEntry;
        typedef WalletEntry<KeyData, key_entry_type>            WalletKeyEntry;
        typedef WalletEntry<ContactData, contact_entry_type>        WalletContactEntry;
        typedef WalletEntry<TransactionData, transaction_entry_type>    WalletTransactionEntry;
        typedef WalletEntry<Setting, setting_entry_type>        WalletSettingEntry;
		typedef WalletEntry<ContractEntry, contract_entry_type> WalletContractEntry;

		typedef optional<WalletContractEntry>                             oWalletContractEntry;
        typedef optional<WalletPropertyEntry>                             oWalletPropertyEntry;
        typedef optional<WalletMasterKeyEntry>                           oWalletMasterKeyEntry;
        typedef optional<WalletAccountEntry>                              oWalletAccountEntry;
        typedef optional<WalletKeyEntry>                                  oWalletKeyEntry;
        typedef optional<WalletContactEntry>                              oWalletContactEntry;
        typedef optional<WalletTransactionEntry>                          oWalletTransactionEntry;
        typedef optional<WalletSettingEntry>                              oWalletSettingEntry;

        struct GenericWalletEntry
        {
            GenericWalletEntry() :type(0){}

            template<typename entryType>
            GenericWalletEntry(const entryType& rec)
                : type(int(entryType::type)), data(rec)
            { }

            template<typename entryType>
            entryType as()const;

            int32_t get_wallet_entry_index()const
            {
                try {
                    FC_ASSERT(data.is_object());
                    FC_ASSERT(data.get_object().contains("index"));
                    return data.get_object()["index"].as<int32_t>();
                } FC_RETHROW_EXCEPTIONS(warn, "")
            }

            fc::enum_type<uint8_t, WalletEntryTypeEnum>   type;
            fc::variant                                      data;
        };
        struct ScriptEntry
        {

            ScriptIdType id;				//脚本id
            bool enable;				//启用状态
            Code code;				//代码
            string description;			//备注
            fc::time_point_sec  register_time;
            ScriptEntry();
            ScriptEntry(fc::path filepath, string description = "");
            bool operator<(const ScriptEntry& entry) const;
        };
        typedef optional<ScriptEntry> oScriptEntry;

        struct ScriptEntryPrintable
        {
            string id;				//脚本id
            bool enable;				//启用状态
            CodePrintAble code_printable;			//代码
            string description;			//备注
            fc::time_point_sec  register_time;

            ScriptEntryPrintable(){}
            ScriptEntryPrintable(const ScriptEntry& entry) : id(entry.id.AddressToString(script_id)), enable(entry.enable), code_printable(entry.code),
                description(entry.description), register_time(entry.register_time)
            {
            }
        };
        typedef optional<ScriptEntryPrintable> oScriptEntryPrintable;

        class ScriptRelationKey
        {
        public:
            ContractIdType contract_id;
            std::string event_type;

            ScriptRelationKey(){}
            ScriptRelationKey(const ContractIdType& id, const std::string& type) : contract_id(id), event_type(type){}

            friend bool operator == (const ScriptRelationKey& key1, const ScriptRelationKey& key2)
            {
                return std::tie(key1.contract_id, key1.event_type) == std::tie(key2.contract_id, key2.event_type);
            }

            friend bool operator < (const ScriptRelationKey& key1, const ScriptRelationKey& key2)
            {
                return (key1.contract_id < key2.contract_id) ||
                    ((key1.contract_id == key2.contract_id) && (key1.event_type < key2.event_type));
            }
        };
    }
} // thinkyoung::wallet


namespace std
{
    template<>
    struct hash < thinkyoung::wallet::ScriptRelationKey >
    {
        size_t operator()(const thinkyoung::wallet::ScriptRelationKey& s)const
        {
            return  *((size_t*)&(s.contract_id));
        }
    };
}


FC_REFLECT_ENUM(thinkyoung::wallet::WalletEntryTypeEnum,
    (master_key_entry_type)
    (account_entry_type)
    (key_entry_type)
    (transaction_entry_type)
    (contact_entry_type)
    (property_entry_type)
    (setting_entry_type)
    )

    FC_REFLECT_ENUM(thinkyoung::wallet::PropertyEnum,
    (version)
    (next_entry_number)
    (next_child_key_index)
    (automatic_backups)
    (transaction_scanning)
    (last_unlocked_scanned_block_number)
    (default_transaction_priority_fee)
    (transaction_expiration_sec)
    (transaction_min_imessage_soft_length)
    (transaction_min_imessage_fee_coe)
(last_scanned_block_number_for_thinkyoung)
    )

    FC_REFLECT(thinkyoung::wallet::WalletProperty,
    (key)
    (value)
    )

    FC_REFLECT_DERIVED(thinkyoung::wallet::AccountData, (thinkyoung::blockchain::AccountEntry),
    (is_my_account)
    (approved)
    (is_favorite)
    (block_production_enabled)
    (last_used_gen_sequence)
    (private_data)
    )

    FC_REFLECT_DERIVED(thinkyoung::wallet::AccountAddressData, (thinkyoung::wallet::AccountData),
    (owner_address)
    (active_address)
    )

    FC_REFLECT(thinkyoung::wallet::MasterKey,
    (encrypted_key)
    (checksum)
    )

    FC_REFLECT(thinkyoung::wallet::KeyData,
    (account_address)
    (public_key)
    (encrypted_private_key)
    (valid_from_signature)
    (memo)
    (gen_seq_number)
    )

    FC_REFLECT_ENUM(thinkyoung::wallet::ContactData::ContactTypeEnum,
    (account_name)
    (public_key)
    (address)
    (btc_address)
    )

    FC_REFLECT(thinkyoung::wallet::ContactData,
    (contact_type)
    (data)
    (label)
    (favorite)
    )

    FC_REFLECT(thinkyoung::wallet::TransactionData,
    (entry_id)
    (block_num)
    (is_virtual)
    (is_confirmed)
    (is_market)
    (trx)
    (ledger_entries)
    (fee)
    (created_time)
    (received_time)
    (extra_addresses)
    )

    FC_REFLECT(thinkyoung::wallet::LedgerEntry,
    (from_account)
    (to_account)
    (amount)
    (memo)
	(from_contract_balance)
	(to_contract_balance)
    (memo_from_account)
    )

    // do not use -- see notes above
    FC_REFLECT(thinkyoung::wallet::TransactionLedgerEntry,
    (id)
    (block_num)
    (block_id)
    (timestamp)
    (delta_amounts)
    (transaction_id)
    (delta_labels)
    (operation_notes)
    )

    FC_REFLECT_DERIVED(thinkyoung::wallet::PrettyTransactionExperimental, (thinkyoung::wallet::TransactionLedgerEntry),
    (inputs)
    (outputs)
    (balances)
    (notes)
    )

    FC_REFLECT(thinkyoung::wallet::Setting,
    (name)
    (value)
    )

    FC_REFLECT(thinkyoung::wallet::GenericWalletEntry,
    (type)
    (data)
    )

    FC_REFLECT(thinkyoung::wallet::ScriptEntry,
    (id)
    (enable)
    (code)
    (description)
    (register_time)
    )

    FC_REFLECT(thinkyoung::wallet::ScriptEntryPrintable,
    (id)
    (enable)
    (code_printable)
    (description)
    (register_time)
    )

    FC_REFLECT(thinkyoung::wallet::ScriptRelationKey,
    (contract_id)
    (event_type)
    )


    /**
     *  Implement generic reflection for wallet entry types
     */
namespace fc {

    template<typename T, thinkyoung::wallet::WalletEntryTypeEnum N>
    struct get_typename < thinkyoung::wallet::WalletEntry<T, N> >
    {
        static const char* name()
        {
            static std::string _name = get_typename<T>::name() + std::string("Entry");
            return _name.c_str();
        }
    };

    template<typename Type, thinkyoung::wallet::WalletEntryTypeEnum N>
    struct reflector < thinkyoung::wallet::WalletEntry<Type, N> >
    {
        typedef thinkyoung::wallet::WalletEntry<Type, N>  type;
        typedef fc::true_type is_defined;
        typedef fc::false_type is_enum;
        enum member_count_enum {
            local_member_count = 1,
            total_member_count = local_member_count + reflector<Type>::total_member_count
        };

        template<typename Visitor>
        static void visit(const Visitor& visitor)
        {
            {
                typedef decltype(((thinkyoung::wallet::BaseEntry<N>*)nullptr)->wallet_entry_index) member_type;
                visitor.TEMPLATE operator() < member_type, thinkyoung::wallet::BaseEntry<N>, &thinkyoung::wallet::BaseEntry<N>::wallet_entry_index > ("index");
            }

            fc::reflector<Type>::visit(visitor);
        }
    };
} // namespace fc

namespace thinkyoung {
    namespace wallet {
        template<typename entryType>
        entryType GenericWalletEntry::as()const
        {
            FC_ASSERT((WalletEntryTypeEnum)type == entryType::type, "",
                ("type", type)
                ("WithdrawType", (WalletEntryTypeEnum)entryType::type));

            return data.as<entryType>();
        }
    }
}
