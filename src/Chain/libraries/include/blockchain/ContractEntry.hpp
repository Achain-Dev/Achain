#pragma  once

#include <blockchain/Types.hpp>
#include <blockchain/StorageTypes.hpp>
#include <blockchain/WithdrawTypes.hpp>
#include <fc/optional.hpp>
#include <string>
#include <vector>
#include <set>
#include <boost/uuid/sha1.hpp>


#define PRINTABLE_CHAR(chr) \
if (chr >= 0 && chr <= 9)  \
    chr = chr + '0'; \
else \
    chr = chr + 'a' - 10; 


using namespace thinkyoung::blockchain;

namespace thinkyoung {
    namespace blockchain{

        enum ContractApiType
        {
            chain = 1,
            offline = 2,
            event = 3
        };

        enum ContractState
        {
            valid = 1,
            deleted = 2
        };

        enum ContractLevel
        {
            temp = 1,
            forever = 2
        };

        static std::string to_printable_hex(unsigned char chr)
        {
            unsigned char high = chr >> 4;
            unsigned char low = chr & 0x0F;
            char tmp[16];

            PRINTABLE_CHAR(high);
            PRINTABLE_CHAR(low);

            snprintf(tmp, sizeof(tmp), "%c%c", high, low);
            return string(tmp);
        }

        //the code-related detail of contract
        struct Code
        {
            std::set<std::string> abi;
            std::set<std::string> offline_abi;
            std::set<std::string> events;
            std::map<std::string, fc::enum_type<fc::unsigned_int, thinkyoung::blockchain::StorageValueTypes>> storage_properties;
            std::vector<ContractChar> byte_code;
            std::string code_hash;
            Code(const fc::path&);
            Code();
            void SetApis(char* module_apis[], int count, int api_type);
            bool valid() const;
			std::string GetHash() const ;
        };

        struct CodePrintAble
        {
            std::set<std::string> abi;
            std::set<std::string> offline_abi;
            std::set<std::string> events;
            std::map<std::string, std::string> printable_storage_properties;
            std::string printable_code;
            std::string code_hash;

            CodePrintAble(){}

            CodePrintAble(const Code& code) : abi(code.abi), offline_abi(code.offline_abi), events(code.events), code_hash(code.code_hash)
            {
                unsigned char* code_buf = (unsigned char*)malloc(code.byte_code.size());
                FC_ASSERT(code_buf, "malloc failed");

                for (int i = 0; i < (int)code.byte_code.size(); ++i)
                {
                    code_buf[i] = code.byte_code[i];
                    printable_code = printable_code + to_printable_hex(code_buf[i]);
                }

                for (const auto& storage_info : code.storage_properties)
                {
                    std::string storage_type = "unknown type";
                    auto iter = storage_type_map.find(storage_info.second);
                    if (iter != storage_type_map.end())
                        storage_type = iter->second;
                    
                    printable_storage_properties.insert(make_pair(storage_info.first, storage_type));
                }

                free(code_buf);
            }
        };

        //front declaration for typedef
        class ChainInterface;
        struct  ContractEntry;
        struct ContractStorageEntry;
        //use fc optional to hold the return value
        typedef fc::optional<ContractEntry> oContractEntry;
        typedef fc::optional<ContractStorageEntry> oContractStorage;
        typedef fc::optional<ContractIdType> oContractIdType;


        //contract information
        struct  ContractEntry
        {
            ContractName  contract_name; // contract name
            ContractIdType  id; //contract address
            fc::enum_type<fc::unsigned_int, ContractLevel> level = ContractLevel::temp; //the level of the contract
            PublicKeyType owner; //the owner of the contract
            fc::enum_type<fc::unsigned_int, ContractState> state = ContractState::valid; //contract state
            std::string description; //the description of contract
            Code code; // code-related of contract
            fc::optional<uint64_t>    reserved;
            TransactionIdType trx_id;
			fc::time_point register_time;
            //the address string, contain contract address prefix
            std::string id_to_base58()const;
		

            // database related functions
            static oContractEntry lookup(const ChainInterface&, const ContractIdType&);
            static oContractEntry lookup(const ChainInterface&, const ContractName&);
            static void store(ChainInterface&, const ContractIdType&, const ContractEntry&);
            static void remove(ChainInterface&, const ContractIdType&);

        };

        struct ContractEntryPrintable
        {
            ContractName  contract_name; // contract name
            string  id; //contract address
            fc::enum_type<fc::unsigned_int, ContractLevel> level = ContractLevel::temp; //the level of the contract
            PublicKeyType owner; //the owner of the contract
            Address owner_address;  //the owner address of the contract
            string owner_name;  //the owner name of the contract
            fc::enum_type<fc::unsigned_int, ContractState> state = ContractState::valid; //contract state
            std::string description; //the description of contract
            CodePrintAble code_printable; // code-related of contract
            fc::optional<uint64_t>    reserved;
            TransactionIdType trx_id;

            ContractEntryPrintable(){}

            ContractEntryPrintable(const ContractEntry& entry) : contract_name(entry.contract_name), id(entry.id.AddressToString(AddressType::contract_address)), level(entry.level),
                owner(entry.owner), owner_address(entry.owner), state(entry.state), description(entry.description), code_printable(entry.code), reserved(entry.reserved),
                trx_id(entry.trx_id) {}
        };

        //contract storage
        struct ContractStorageEntry
        {
            //std::vector<ContractChar> contract_storage;
            ContractIdType  id; //contract address
            std::map<std::string, StorageDataType> contract_storages;

            static oContractStorage lookup(const ChainInterface&, const ContractIdType&);
            static void store(ChainInterface&, const ContractIdType&, const ContractStorageEntry&);
            static void remove(ChainInterface&, const ContractIdType&);

        };
		struct  ResultTIdEntry;
		typedef fc::optional<ResultTIdEntry> oResultTIdEntry;
		struct ResultTIdEntry
		{
			TransactionIdType res;
			ResultTIdEntry();
			ResultTIdEntry(const TransactionIdType& id);
			static oResultTIdEntry lookup(const ChainInterface&, const TransactionIdType&);
			static void store(ChainInterface&, const TransactionIdType& , const ResultTIdEntry&);
			static void remove(ChainInterface&, const TransactionIdType&);
		};
		struct RequestIdEntry;
		typedef fc::optional<RequestIdEntry> oRequestIdEntry;
		struct RequestIdEntry
		{
			TransactionIdType req;
			RequestIdEntry();
			RequestIdEntry(const TransactionIdType& id);
			static oRequestIdEntry lookup(const ChainInterface&, const TransactionIdType&);
			static void store(ChainInterface&, const TransactionIdType&, const RequestIdEntry&);
			static void remove(ChainInterface&, const TransactionIdType&);

		};
		struct ContractTrxEntry;
		typedef fc::optional<ContractTrxEntry> oContractTrxEntry;
		struct ContractTrxEntry
		{
			TransactionIdType trx_id;
			ContractTrxEntry();
			ContractTrxEntry(const TransactionIdType& id);
			static oContractTrxEntry lookup(const ChainInterface&, const ContractIdType&);
			static void store(ChainInterface&, const ContractIdType&,const ContractTrxEntry&);
			static void remove(ChainInterface&, const ContractIdType&);
		};
		struct ContractinTrxEntry;
		typedef fc::optional<ContractinTrxEntry> oContractinTrxEntry;
		struct ContractinTrxEntry
		{
			ContractIdType contract_id;
			ContractinTrxEntry();
			ContractinTrxEntry(const ContractIdType& id);
			static oContractinTrxEntry lookup(const ChainInterface&, const TransactionIdType&);
			static void store(ChainInterface&, const TransactionIdType&, const ContractinTrxEntry&);
			static void remove(ChainInterface&, const TransactionIdType&);
		};
        class ContractDbInterface
        {

            friend struct ContractEntry;
            friend struct ContractStorageEntry;
			friend struct ResultTIdEntry;
			friend struct RequestIdEntry;
			friend struct ContractinTrxEntry;
			friend struct ContractTrxEntry;
            //lookup related
            virtual  oContractEntry  contract_lookup_by_id(const ContractIdType&)const = 0;
            virtual  oContractEntry  contract_lookup_by_name(const ContractName&)const = 0;
            virtual oContractStorage contractstorage_lookup_by_id(const ContractIdType&)const = 0;
			virtual oResultTIdEntry contract_lookup_resultid_by_reqestid(const TransactionIdType&)const = 0;
			virtual oRequestIdEntry contract_lookup_requestid_by_resultid(const TransactionIdType&)const = 0;
			virtual oContractinTrxEntry contract_lookup_contractid_by_trxid(const TransactionIdType&)const = 0;
			virtual oContractTrxEntry contract_lookup_trxid_by_contract_id(const ContractIdType&)const = 0;

			//insert related
            virtual void contract_insert_into_id_map(const ContractIdType&, const ContractEntry&) = 0;
            virtual void contract_insert_into_name_map(const ContractName&, const ContractIdType&) = 0;
            virtual void contractstorage_insert_into_id_map(const ContractIdType&, const ContractStorageEntry&) = 0;
			virtual void contract_store_resultid_by_reqestid(const TransactionIdType& req, const ResultTIdEntry& res) = 0;
			virtual void contract_store_requestid_by_resultid(const TransactionIdType& req, const RequestIdEntry& res) = 0;
			virtual void contract_store_contractid_by_trxid(const TransactionIdType& id, const ContractinTrxEntry& res) = 0;
			virtual void contract_store_trxid_by_contractid(const ContractIdType& id, const ContractTrxEntry & res) = 0;
			//erase related
            virtual void contract_erase_from_id_map(const ContractIdType&) = 0;
            virtual void contract_erase_from_name_map(const ContractName&) = 0;
            virtual void contractstorage_erase_from_id_map(const ContractIdType&) = 0;
			virtual void contract_erase_resultid_by_reqestid(const TransactionIdType& req) = 0;
			virtual void contract_erase_requestid_by_resultid(const TransactionIdType& req) = 0;
			virtual void contract_erase_trxid_by_contract_id(const ContractIdType&) = 0;
			virtual void contract_erase_contractid_by_trxid(const TransactionIdType& req) = 0;

        };
    }
}

FC_REFLECT_ENUM(thinkyoung::blockchain::ContractApiType,
    (chain)
    (offline)
    (event)
    )

    FC_REFLECT_ENUM(thinkyoung::blockchain::ContractState,
    (valid)
    (deleted)
    )

    FC_REFLECT_ENUM(thinkyoung::blockchain::ContractLevel,
    (temp)
    (forever)
    )


    FC_REFLECT(thinkyoung::blockchain::Code,
    (abi)
    (offline_abi)
    (events)
    (storage_properties)
    (byte_code)
    (code_hash)
    )

    FC_REFLECT(thinkyoung::blockchain::CodePrintAble,
    (abi)
    (offline_abi)
    (events)
    (printable_storage_properties)
    (printable_code)
    (code_hash)
    )


    FC_REFLECT(thinkyoung::blockchain::ContractEntry,
    (contract_name)
    (id)
    (level)
    (owner)
    (state)
    (description)
    (code)
    (reserved)
    (trx_id)
    )

    FC_REFLECT(thinkyoung::blockchain::ContractEntryPrintable,
    (contract_name)
    (id)
    (level)
    (owner)
    (owner_address)
    (owner_name)
    (state)
    (description)
    (code_printable)
    (reserved)
    (trx_id)
    )

    FC_REFLECT(thinkyoung::blockchain::ContractStorageEntry, (id)(contract_storages))
	FC_REFLECT(thinkyoung::blockchain::ResultTIdEntry, (res))
	FC_REFLECT(thinkyoung::blockchain::RequestIdEntry, (req))
	FC_REFLECT(thinkyoung::blockchain::ContractTrxEntry, (trx_id))
	FC_REFLECT(thinkyoung::blockchain::ContractinTrxEntry, (contract_id))
	