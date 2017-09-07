#include <blockchain/ContractEntry.hpp>
#include <blockchain/ChainInterface.hpp>
#include <blockchain/Types.hpp>
#include <blockchain/Address.hpp>
#include <blockchain/Exceptions.hpp>

#include <fc/array.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <boost/uuid/sha1.hpp>

#include <utilities/CommonApi.hpp>

using namespace thinkyoung::blockchain;


#define INIT_API_FROM_FILE(dst_set, except_1, except_2, except_3)\
{\
read_count = thinkyoung::utilities::common_fread_int(f, &api_count); \
if (read_count != 1)\
{\
fclose(f); \
throw except_1(); \
}\
for (int i = 0; i < api_count; i++)\
{\
int api_len = 0; \
read_count = thinkyoung::utilities::common_fread_int(f, &api_len); \
if (read_count != 1)\
{\
fclose(f); \
throw except_2(); \
}\
api_buf = (char*)malloc(api_len + 1); \
if (api_buf == NULL) \
{ \
fclose(f); \
FC_ASSERT(api_buf == NULL, "malloc fail!"); \
}\
read_count = thinkyoung::utilities::common_fread_octets(f, api_buf, api_len); \
if (read_count != 1)\
{\
fclose(f); \
free(api_buf); \
throw except_3(); \
}\
api_buf[api_len] = '\0'; \
dst_set.insert(std::string(api_buf)); \
free(api_buf); \
}\
}

#define INIT_STORAGE_FROM_FILE(dst_map, except_1, except_2, except_3, except_4)\
{\
read_count = thinkyoung::utilities::common_fread_int(f, &storage_count); \
if (read_count != 1)\
{\
fclose(f); \
throw except_1(); \
}\
for (int i = 0; i < storage_count; i++)\
{\
int storage_name_len = 0; \
read_count = thinkyoung::utilities::common_fread_int(f, &storage_name_len); \
if (read_count != 1)\
{\
fclose(f); \
throw except_2(); \
}\
storage_buf = (char*)malloc(storage_name_len + 1); \
if (storage_buf == NULL) \
{ \
fclose(f); \
FC_ASSERT(storage_buf == NULL, "malloc fail!"); \
}\
read_count = thinkyoung::utilities::common_fread_octets(f, storage_buf, storage_name_len); \
if (read_count != 1)\
{\
fclose(f); \
free(storage_buf); \
throw except_3(); \
}\
storage_buf[storage_name_len] = '\0'; \
read_count = thinkyoung::utilities::common_fread_int(f, (int*)&storage_type); \
if (read_count != 1)\
{\
fclose(f); \
free(storage_buf); \
throw except_4(); \
}\
dst_map.insert(std::make_pair(std::string(storage_buf), storage_type)); \
free(storage_buf); \
}\
}


namespace thinkyoung {
    namespace blockchain{

        std::string ContractEntry::id_to_base58()const
        {
            fc::array<char, 24> bin_addr;
            memcpy((char*)&bin_addr, (char*)&(this->id.addr), sizeof(this->id.addr));
            auto checksum = fc::ripemd160::hash((char*)&(this->id.addr), sizeof(this->id.addr));
            memcpy(((char*)&bin_addr) + 20, (char*)&checksum._hash[0], 4);
            return CONTRACT_ADDRESS_PREFIX + fc::to_base58(bin_addr.data, sizeof(bin_addr));
        }

        oContractEntry ContractEntry::lookup(const ChainInterface& db, const ContractIdType& id)
        {
            try
            {
                return db.contract_lookup_by_id(id);

            }FC_CAPTURE_AND_RETHROW((id))

        }

        oContractEntry ContractEntry::lookup(const ChainInterface& db, const ContractName& name)
        {
            try
            {
                return db.contract_lookup_by_name(name);

            }FC_CAPTURE_AND_RETHROW((name))

        }

        void ContractEntry::store(ChainInterface& db, const ContractIdType& id, const ContractEntry& contract)
        {
            try
            {
                db.contract_insert_into_id_map(id, contract);
                if (db.is_valid_contract_name(contract.contract_name))
                {
                    db.contract_insert_into_name_map(contract.contract_name, id);
                }

            }FC_CAPTURE_AND_RETHROW((id)(contract))
        }

        void ContractEntry::remove(ChainInterface& db, const ContractIdType& id)
        {
            try
            {
                const oContractEntry info = db.lookup<ContractEntry>(id);

                if (info.valid())
                {
                    db.contract_erase_from_id_map(id);
                    //db.contract_erase_from_name_map(info->contract_name);
                }


            }FC_CAPTURE_AND_RETHROW((id))
        }


        oContractStorage ContractStorageEntry::lookup(const ChainInterface& db, const ContractIdType& id)
        {
            try
            {
                return db.contractstorage_lookup_by_id(id);
            }FC_CAPTURE_AND_RETHROW((id))

        }


        void ContractStorageEntry::store(ChainInterface& db, const ContractIdType& id, const ContractStorageEntry& storage)
        {
            try
            {
                db.contractstorage_insert_into_id_map(id, storage);
            }FC_CAPTURE_AND_RETHROW((id)(storage))


        }


        void ContractStorageEntry::remove(ChainInterface& db, const ContractIdType& id)
        {
            try
            {
                const oContractStorage storage = db.lookup<ContractStorageEntry>(id);

                if (storage.valid())
                {
                    db.contractstorage_erase_from_id_map(id);
                }
            }FC_CAPTURE_AND_RETHROW((id))

        }


        Code::Code(const fc::path& path)
        {
            if (!fc::exists(path))
                FC_THROW_EXCEPTION(fc::file_not_found_exception, "Script file not found!");

            string file = path.string();
            FILE* f = fopen(file.c_str(), "rb");
            fseek(f, 0, SEEK_END);
            int fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            unsigned int digest[5];
            int read_count = 0;
            for (int i = 0; i < 5; ++i)
            {
                read_count = thinkyoung::utilities::common_fread_int(f, (int*)&digest[i]);
                if (read_count != 1)
                {
                    fclose(f);
                    FC_THROW_EXCEPTION(thinkyoung::blockchain::read_verify_code_fail, "Read verify code fail!");
                }
            }

            int len = 0;
            read_count = thinkyoung::utilities::common_fread_int(f, &len);
            if (read_count != 1 || len < 0 || (len >= (fsize - ftell(f))))
            {
                fclose(f);
                FC_THROW_EXCEPTION(thinkyoung::blockchain::read_bytescode_len_fail, "Read bytescode len fail!");
            }

            byte_code.resize(len);
            read_count = thinkyoung::utilities::common_fread_octets(f, byte_code.data(), len);
            if (read_count != 1)
            {
                fclose(f);
                FC_THROW_EXCEPTION(thinkyoung::blockchain::read_bytescode_fail, "Read bytescode fail!");
            }

            boost::uuids::detail::sha1 sha;
            unsigned int check_digest[5];
            sha.process_bytes(byte_code.data(), byte_code.size());
            sha.get_digest(check_digest);
            if (memcmp((void*)digest, (void*)check_digest, sizeof(unsigned int) * 5))
            {
                fclose(f);
                FC_THROW_EXCEPTION(thinkyoung::blockchain::verify_bytescode_sha1_fail, "Verify bytescode SHA1 fail!");
            }

            for (int i = 0; i < 5; ++i)
            {
                unsigned char chr1 = (check_digest[i] & 0xFF000000) >> 24;
                unsigned char chr2 = (check_digest[i] & 0x00FF0000) >> 16;
                unsigned char chr3 = (check_digest[i] & 0x0000FF00) >> 8;
                unsigned char chr4 = (check_digest[i] & 0x000000FF);

                code_hash = code_hash + to_printable_hex(chr1) + to_printable_hex(chr2) +
                    to_printable_hex(chr3) + to_printable_hex(chr4);
            }

            int api_count = 0;
            char* api_buf = nullptr;

            INIT_API_FROM_FILE(abi, read_api_count_fail, read_api_len_fail, read_api_fail);
            INIT_API_FROM_FILE(offline_abi, read_offline_api_count_fail, read_offline_api_len_fail, read_offline_api_fail);
            INIT_API_FROM_FILE(events, read_events_count_fail, read_events_len_fail, read_events_fail);

            int storage_count = 0;
            char* storage_buf = nullptr;
            StorageValueTypes storage_type;

            INIT_STORAGE_FROM_FILE(storage_properties, read_storage_count_fail, read_storage_name_len_fail, read_storage_name_fail, read_storage_type_fail);

            fclose(f);
        }
        std::string Code::GetHash() const
        {
            std::string hashstr = "";
            boost::uuids::detail::sha1 sha;
            unsigned int check_digest[5];
            sha.process_bytes(byte_code.data(), byte_code.size());
            sha.get_digest(check_digest);
            for (int i = 0; i < 5; ++i)
            {
                unsigned char chr1 = (check_digest[i] & 0xFF000000) >> 24;
                unsigned char chr2 = (check_digest[i] & 0x00FF0000) >> 16;
                unsigned char chr3 = (check_digest[i] & 0x0000FF00) >> 8;
                unsigned char chr4 = (check_digest[i] & 0x000000FF);

                hashstr = hashstr + to_printable_hex(chr1) + to_printable_hex(chr2) +
                    to_printable_hex(chr3) + to_printable_hex(chr4);
            }
            return hashstr;
        }
        Code::Code()
        {

        }

        void Code::SetApis(char* module_apis[], int count, int api_type)
        {
            if (api_type == ContractApiType::chain)
            {
                abi.clear();
                for (int i = 0; i < count; i++)
                    abi.insert(module_apis[i]);
            }
            else if (api_type == ContractApiType::offline)
            {
                offline_abi.clear();
                for (int i = 0; i < count; i++)
                    offline_abi.insert(module_apis[i]);
            }
            else if (api_type == ContractApiType::event)
            {
                events.clear();
                for (int i = 0; i < count; i++)
                    events.insert(module_apis[i]);
            }
        }

        bool Code::valid() const
        {
            //FC_ASSERT(0);
            //return false;
            return true;
        }





        ResultTIdEntry::ResultTIdEntry()
        {
        }

        ResultTIdEntry::ResultTIdEntry(const TransactionIdType & id) :res(id)
        {
        }

        oResultTIdEntry ResultTIdEntry::lookup(const ChainInterface &db, const TransactionIdType &req)
        {
            try
            {
                return  db.contract_lookup_resultid_by_reqestid(req);
            }FC_CAPTURE_AND_RETHROW((req))
        }

        void ResultTIdEntry::store(ChainInterface &db, const TransactionIdType &req, const ResultTIdEntry &res)
        {
            try
            {
                db.contract_store_resultid_by_reqestid(req, res);
            }FC_CAPTURE_AND_RETHROW((req))
        }

        void ResultTIdEntry::remove(ChainInterface &db, const TransactionIdType &req)
        {
            try
            {
                db.contract_erase_resultid_by_reqestid(req);
            }FC_CAPTURE_AND_RETHROW((req))
        }

		RequestIdEntry::RequestIdEntry()
		{
		}

		RequestIdEntry::RequestIdEntry(const TransactionIdType & id):req(id)
		{

		}

		oRequestIdEntry RequestIdEntry::lookup(const ChainInterface &db, const TransactionIdType &res)
		{
			try
			{
				return  db.contract_lookup_requestid_by_resultid(res);
			}FC_CAPTURE_AND_RETHROW((res))
		}

		void RequestIdEntry::store(ChainInterface &db, const TransactionIdType &res, const RequestIdEntry &req)
		{
			try
			{
				db.contract_store_requestid_by_resultid(res, req);
			}FC_CAPTURE_AND_RETHROW((res))
		}

		void RequestIdEntry::remove(ChainInterface &db, const TransactionIdType &res)
		{
			try
			{
				db.contract_erase_requestid_by_resultid(res);
			}FC_CAPTURE_AND_RETHROW((res))
		}
		

		ContractTrxEntry::ContractTrxEntry()
		{
		}

		ContractTrxEntry::ContractTrxEntry(const TransactionIdType & id):trx_id(id)
		{
		}

		oContractTrxEntry ContractTrxEntry::lookup(const ChainInterface &db, const ContractIdType &id)
		{
			try
			{
				return db.contract_lookup_trxid_by_contract_id(id);
			}FC_CAPTURE_AND_RETHROW((id));
		}

		void ContractTrxEntry::store(ChainInterface &db, const ContractIdType &id, const ContractTrxEntry &trx_entry)
		{
			try {
				db.contract_store_trxid_by_contractid(id, trx_entry);
			}FC_CAPTURE_AND_RETHROW((id));
		}

		void ContractTrxEntry::remove( ChainInterface &db, const ContractIdType & id)
		{
			try{
				db.contract_erase_trxid_by_contract_id(id);
			}FC_CAPTURE_AND_RETHROW((id));
			
		}

		ContractinTrxEntry::ContractinTrxEntry()
		{
		}

		ContractinTrxEntry::ContractinTrxEntry(const ContractIdType & id):contract_id(id)
		{
		}

		oContractinTrxEntry ContractinTrxEntry::lookup(const ChainInterface &db, const TransactionIdType &id)
		{
			try
			{
				return db.contract_lookup_contractid_by_trxid(id);
			}FC_CAPTURE_AND_RETHROW((id));
		}

		void ContractinTrxEntry::store(ChainInterface &db, const TransactionIdType &id, const ContractinTrxEntry &contract_entry)
		{
			try
			{
				db.contract_store_contractid_by_trxid(id,contract_entry);
			}FC_CAPTURE_AND_RETHROW((id));
		}

		void ContractinTrxEntry::remove(ChainInterface &db, const TransactionIdType &id)
		{
			try
			{
				db.contract_erase_contractid_by_trxid(id);
			}FC_CAPTURE_AND_RETHROW((id));
		}

}
}