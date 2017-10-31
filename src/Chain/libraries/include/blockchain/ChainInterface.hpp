#pragma once

#include <blockchain/AccountEntry.hpp>
#include <blockchain/AssetEntry.hpp>
#include <blockchain/BalanceEntry.hpp>
#include <blockchain/BlockEntry.hpp>

#include <blockchain/Condition.hpp>
#include <blockchain/ContractEntry.hpp>

#include <blockchain/PropertyEntry.hpp>
#include <blockchain/SlateEntry.hpp>
#include <blockchain/SlotEntry.hpp>
#include <blockchain/TransactionEntry.hpp>
#include <blockchain/Types.hpp>
#include <blockchain/WithdrawTypes.hpp>

namespace thinkyoung {
    namespace blockchain {

        class ChainInterface
            : public PropertyDbInterface,
            public AccountDbInterface,
            public AssetDbInterface,
            public SlateDbInterface,
            public BalanceDbInterface,
            public TransactionDbInterface,
            public SlotDbInterface,
            public  ContractDbInterface
        {
        public:
            virtual ~ChainInterface(){};

            virtual fc::time_point_sec now()const = 0;

            /**  is_valid_account_name
            * Determine  whether a string is a valid account name
            * @param  name  name to be checked
            *
            * @return bool
            */
            static bool                        is_valid_account_name(const string& name);
            /**  is_valid_symbol_name
            * Determine  whether a string is a valid asset symbol
            * @param  symbol  symbol to be checked
            *
            * @return bool
            */
            bool                               is_valid_symbol_name(const string& symbol)const;
            /**
            * Determine  whether a string is a valid asset symbol
            * @param  symbol  string
            *
            * @return bool
            */
            bool                               is_valid_symbol_name_v1(const string& symbol)const;
            /**
            * Determine  whether an asset associated with the specified assetentry is  fraudulent
            * @param  suspect_entry  assetentry accociatied with the asset
            *
            * @return bool
            */
            bool                               is_fraudulent_asset(const AssetEntry& suspect_entry)const;
            /**
            * Get time of genesis(registion of Asset ALP)
            *
            * @return time_point_sec
            */
            time_point_sec                     get_genesis_timestamp()const;

            /**  get_max_delegate_pay_issued_per_block
            *
            * get max fee that issued by dalegates generating a block
            *
            * @return ShareType
            */
            ShareType                         get_max_delegate_pay_issued_per_block()const;
            /**
            * Get fee for registing as a delegate
            *
            * @param pay_rate
            *
            * @return ShareType
            */
            ShareType                         get_delegate_registration_fee(uint8_t pay_rate)const;
            /**
            * Get fee for registing  an Asset
            *
            * @param pay_rate
            *
            * @return ShareType
            */
            ShareType                         get_asset_registration_fee(uint8_t symbol_length)const;
            ShareType                         get_imessage_need_fee(const string & imessage)const;
            ShareType                         get_delegate_registration_fee_v1(uint8_t pay_rate)const;
            ShareType                         get_asset_registration_fee_v1()const;
            ShareType                         get_delegate_pay_rate_v1()const;
            /**  get all active delegates and return all delegate ids
            *
            * @return vector<AccountIdType>
            */
            vector<AccountIdType>            get_active_delegates()const;
            /**  set_active_delegates
            * save a list of active delegates to database
            * @param  active_delegates  delegate to be save
            *
            * @return void
            */
            void                               set_active_delegates(const std::vector<AccountIdType>& active_delegates);
            /**  Check whether an account is a active delegate
            *
            * @param  id  id of the account that to be checked
            *
            * @return bool
            */
            bool                               is_active_delegate(const AccountIdType id)const;

            /**  converts an asset + asset_id to a more friendly representation using the symbol name
            *
            * @param  a  Asset to be formated
            *
            * @return string
            */
            string                             to_pretty_asset(const Asset& a)const;

            /**  Convert a price object to a double
            *
            * @param  price_to_pretty_print  Price
            *
            * @return double
            */
            double                             to_pretty_price_double(const Price& a)const;
            /**  Convert a price object to a string
            *
            * @param  price_to_pretty_print  Price
            *
            * @return string
            */
            string                             to_pretty_price(const Price& a)const;
            /**  to_ugly_asset
            * converts a numeric string + asset symbol to an asset
            * @param  amount  std::string
            * @param  symbol  std::string
            *
            * @return Asset
            */
            Asset                              to_ugly_asset(const string& amount, const string& symbol)const;
            /**  to_ugly_price
            * converts a numeric string and two asset symbols to a price
            * @param  price_string  std::string
            * @param  base_symbol  std::string
            * @param  quote_symbol  std::string
            * @param  do_precision_dance  bool
            *
            * @return Price
            */
            Price                              to_ugly_price(const string& price_string,
                const string& base_symbol,
                const string& quote_symbol,
                bool do_precision_dance = true)const;

            virtual SignedBlockHeader         get_block_header(const BlockIdType&)const = 0;
            /**  set_chain_id
            * Store id into property database as chain id
            * @param  id  DigestType
            *
            * @return void
            */
            void                               set_chain_id(const DigestType& id);
            /**
            * Get blockchain id from property database
            *
            * @return DigestType
            */
            DigestType                        get_chain_id()const;

            /**  set_statistics_enabled
            * store the input bool value into property database with the id statistics_enabled
            * @param  enabled  bool
            *
            * @return void
            */
            void                               set_statistics_enabled(const bool enabled);
            /**
            * Get the value from property database with id statistics_enabled
            *
            * @return bool
            */
            bool                               get_statistics_enabled()const;

            void								set_node_vm_enabled(const bool enabled);
            bool								get_node_vm_enabled()const;

            //virtual oprice                     get_active_feed_price( const asset_id_type quote_id,
            //const asset_id_type base_id = 0 )const = 0;

            /**  get_current_random_seed
            *
            * get currnet random seed from property database
            *
            * @return fc::ripemd160
            */
            fc::ripemd160                      get_current_random_seed()const;

            /**  set_required_confirmations
            * store the input uint64_t value into property database with the id confirmation_requirement
            *
            * @param  count  uint64_t
            *
            * @return void
            */
            void                               set_required_confirmations(uint64_t count);
            /**  get_required_confirmations
            *
            * get num of confiermations_required from property database
            *
            * @return uint64_t
            */
            uint64_t                           get_required_confirmations()const;



            /**
            * chech whether a transaction is known
            *
            * @param trx transaction to be checked
            *
            * @return bool
            */
            virtual bool                       is_known_transaction(const Transaction& trx)const = 0;
            /**
            * Get a transaction  according transaction id
            *
            * @param trx_id id of the target transaction
            *
            * @return bool
            */
            virtual oTransactionEntry        get_transaction(const TransactionIdType& trx_id,
                bool exact = true)const = 0;
            /**
            * Store a transaction into database
            *
            * @param trx_id id of the  transaction
            * @param entry transaction entry to be stored
            * @return bool
            */
            virtual void                       store_transaction(const TransactionIdType& id,
                const TransactionEntry&  entry) = 0;
            /**  last_asset_id
            *
            * get id of last asset
            *
            * @return AssetIdType
            */
            AssetIdType                      last_asset_id()const;
            /**  new_asset_id
            *
            * get an id which can be used as id of new asset
            *
            * @return AssetIdType
            */
            AssetIdType                      new_asset_id();

            /**  last_account_id
            *
            * get id of last account
            *
            * @return AccountIdType
            */
            AccountIdType                    last_account_id()const;
            /**  new_account_id
            *
            * Get an id which can be used as id of new account
            *
            * @return AccountIdType
            */
            AccountIdType                    new_account_id();
            ImessageLengthIdType get_imessage_min_length()const;
            void set_imessage_min_length(const ImessageLengthIdType& imessage_length);
            ImessageIdType get_imessage_fee_coefficient()const;
            void set_imessage_fee_coefficient(const ImessageIdType& fee_coe);
            SignedTransaction transfer_asset_from_contract(
                ShareType real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const Address& from_contract_address,
                const string& to_account_name,SignedTransaction& trx);
            SignedTransaction transfer_asset_from_contract(
                ShareType real_amount_to_transfer,
                const string& amount_to_transfer_symbol,
                const Address& from_contract_address,
                const Address& to_account_address,SignedTransaction& trx);
            /**  get_head_block_num
            *
            * Get the number of head block
            *
            * @return uint32_t
            */
            virtual uint32_t                   get_head_block_num()const = 0;


            virtual fc::time_point_sec     get_head_block_timestamp()const = 0;

            /**  get_property_entry
            * Get property entry from property database according property
            * @param  id  property_id_type
            *
            * @return oPropertyEntry
            */
            oPropertyEntry                   get_property_entry(const PropertyIdType id)const;
            /**  store_property_entry
            * Store value into property database with specified id
            * @param  id  property_id_type
            * @param  value  variant
            *
            * @return void
            */
            void                               store_property_entry(const PropertyIdType id, const variant& value);
            /**
            * Retrieves specified account entry according ID from db
            *
            * @param id id of specified account
            *
            * @return oAccountEntry
            */
            oAccountEntry                    get_account_entry(const AccountIdType id)const;
            /**
            * Retrieves specified account entry according name from db
            *
            * @param name name of specified account
            *
            * @return oAccountEntry
            */
            oAccountEntry                    get_account_entry(const string& name)const;
            /**
            * Retrive specified account entry according address from db
            *
            * @param address address of specified account
            *
            * @return oAccountEntry
            */
            oAccountEntry                    get_account_entry(const Address& addr)const;
            /**  store_account_entry
            * store account entry into database
            * @param  entry  AccountEntry to be stored
            *
            * @return void
            */
            void                               store_account_entry(const AccountEntry& entry);
            /**
            * Get specified asset entry from db
            *
            * @param id id of specified asset
            *
            * @return oAssetEntry
            */
            oAssetEntry                      get_asset_entry(const AssetIdType id)const;
            /**
            * Get specified asset entry from db
            *
            * @param symbol symbol of specified asset
            *
            * @return oAssetEntry
            */
            oAssetEntry                      get_asset_entry(const string& symbol)const;
            /**  store_asset_entry
            * Store asset entry into database
            * @param  entry  AssetEntry to be stored
            *
            * @return void
            */
            void                               store_asset_entry(const AssetEntry& entry);
            /**
            * Get slate entry from db according slate id
            *
            * @param id    slate id
            *
            * @return oSlateEntry
            */
            oSlateEntry                      get_slate_entry(const SlateIdType id)const;
            /**  store_slate_entry
            * store slate entry into database
            * @param  entry  SlateEntry to be stored
            *
            * @return void
            */
            void                               store_slate_entry(const SlateEntry& entry);
            /**  get_balance_entry
            * Get balance entry  from database according id
            *
            * @param  id  BalanceIdType
            *
            * @return oBalanceEntry
            */
            oBalanceEntry                    get_balance_entry(const BalanceIdType& id)const;
            /**  store_balance_entry
            * Store balance entry into balance entry
            * @param  entry  BalanceEntry
            *
            * @return void
            */
            void                               store_balance_entry(const BalanceEntry& entry);

            /**  get_slot_entry
            * Get slot entry from database according index
            * @param  index  index of slot entry
            *
            * @return oSlotEntry
            */
            oSlotEntry                       get_slot_entry(const SlotIndex index)const;
            /**  get slot entry by timestamp
            *
            * @param  timestamp  time_point_sec
            *
            * @return oSlotEntry
            */
            oSlotEntry                       get_slot_entry(const time_point_sec timestamp)const;
            /**  store_slot_entry
            * Store slot entry into database
            * @param  entry  SlotEntry
            *
            * @return void
            */
            void                               store_slot_entry(const SlotEntry& entry);

            int  get_limit(AssetIdType id, ShareType amount);
            Asset get_amount(ShareType limit, AssetIdType asset_id = 0);
            Asset  get_transaction_fee(const AssetIdType desired_fee_asset_id = 0)const;

            Asset                              get_contract_register_fee(const Code&) const;

            Asset  get_default_margin(const AssetIdType desired_fee_asset_id = 0)const;

            //uint32_t  get_transaction_expiration()const;

            BalanceIdType get_balanceid(const Address& contract_address,
                WithdrawBalanceTypes balance_type = WithdrawBalanceTypes::withdraw_common_type);


            void withdraw_from_contract(
                const Asset& amount_to_withdraw,
                const Address& from_contract_address,
                SignedTransaction& trx);


            oContractEntry                     get_contract_entry(const ContractIdType& id) const;

            oContractEntry                     get_contract_entry(const ContractName& name) const;

            void                               remove_contract_entry(const ContractIdType& id);

            void                               store_contract_entry(const ContractEntry& entry);

            oContractStorage                  get_contractstorage_entry(const ContractIdType& id) const;

            void                              remove_contractstorage_entry(const ContractIdType& id);

            void                              store_contractstorage_entry(const ContractStorageEntry& entry);

            bool                               is_destroyed_contract(const ContractState state) const;

            bool                               is_temporary_contract(const ContractLevel level) const;

            bool                               is_valid_contract_name(const string& name) const;

            bool                               is_valid_contract_description(const string& description) const;


            virtual BlockIdType               get_block_id(uint32_t block_num)const = 0;
            /**
            * call T::lookup to get an T type data from database according key
            *
            * @param key key used to find data
            *
            * @return  optional<T>
            */
            template<typename T, typename U>
            optional<T> lookup(const U& key)const
            {
                try {
                    return T::lookup(*this, key);
                } FC_CAPTURE_AND_RETHROW((key))
            }

            /**
            * call T::store to Store an T type data into database according key

            * @param key key used to find data
            * @param entry entry to be stored
            *
            * @return void
            */
            template<typename T, typename U>
            void store(const U& key, const T& entry)
            {
                try {
#ifdef ALP_TEST_NETWORK
                    //entry.sanity_check(*this);
#endif
                    T::store(*this, key, entry);
                } FC_CAPTURE_AND_RETHROW((key)(entry))
            }
            /**
            * call T::remove to remove an T type data from database according key
            *
            * @param key key used to find data
            *
            * @return void
            */
            template<typename T, typename U>
            void remove(const U& key)
            {
                try {
                    T::remove(*this, key);
                } FC_CAPTURE_AND_RETHROW((key))
            }

            // modified by sandbox contract
            /**  Is asset symbol valid
            *
            * @param  asset_symbol  string
            *
            * @return bool
            */
            bool                               is_valid_symbol(const string& asset_symbol)const;
        };
        typedef std::shared_ptr<ChainInterface> ChainInterfacePtr;

    }
} // thinkyoung::blockchain
