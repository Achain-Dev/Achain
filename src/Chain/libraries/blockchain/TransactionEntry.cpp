#include <blockchain/ChainInterface.hpp>
#include <blockchain/TransactionEntry.hpp>
#include <sstream>
//#include <mysql/MysqlHand.h>

namespace thinkyoung {
    namespace blockchain {

        void TransactionEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(!trx.reserved.valid());
                FC_ASSERT(!trx.operations.empty());
                FC_ASSERT(!trx.signatures.empty());
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oTransactionEntry TransactionEntry::lookup(const ChainInterface& db, const TransactionIdType& id)
        {
            try {
                return db.transaction_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void TransactionEntry::store(ChainInterface& db, const TransactionIdType& id, const TransactionEntry& entry)
        {
            try {
                db.transaction_insert_into_id_map(id, entry);
                db.transaction_insert_into_unique_set(entry.trx);
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void TransactionEntry::remove(ChainInterface& db, const TransactionIdType& id)
        {
            try {
                const oTransactionEntry prev_entry = db.lookup<TransactionEntry>(id);
                if (prev_entry.valid())
                {
                    db.transaction_erase_from_id_map(id);
                    db.transaction_erase_from_unique_set(prev_entry->trx);
                }
            } FC_CAPTURE_AND_RETHROW((id))
        }
        std::string TransactionEntry::compose_insert_sql()
          {
              std::string trx_entry_sqlstr_beging = "INSERT INTO trx_entry VALUES ";
              std::string trx_entry_sqlstr_ending = " on duplicate key update ";
              trx_entry_sqlstr_ending += " trx_type=values(trx_type),";
              trx_entry_sqlstr_ending += " deposit_balance_id=values(deposit_balance_id),";
              trx_entry_sqlstr_ending += " deposit_amount=values(deposit_amount),";
              trx_entry_sqlstr_ending += " withdraw_balance_id=values(withdraw_balance_id),";
              trx_entry_sqlstr_ending += " withdraw_amount=values(withdraw_amount),";
              trx_entry_sqlstr_ending += " balance_diff=values(balance_diff),";
              trx_entry_sqlstr_ending += " alp_account=values(alp_account),";
              trx_entry_sqlstr_ending += " expiration=values(expiration),";
              trx_entry_sqlstr_ending += " trx_amount=values(trx_amount),";
              trx_entry_sqlstr_ending += " required_fee=values(required_fee),";
              trx_entry_sqlstr_ending += " transaction_fee=values(transaction_fee),";
              trx_entry_sqlstr_ending += " commission_charge=values(commission_charge),";
              trx_entry_sqlstr_ending += " op_count=values(op_count),";
              trx_entry_sqlstr_ending += " operations=values(operations),";
              trx_entry_sqlstr_ending += " contract_id=values(contract_id),";
              trx_entry_sqlstr_ending += " contract_method=values(contract_method),";
              trx_entry_sqlstr_ending += " contract_arg=values(contract_arg),";
              trx_entry_sqlstr_ending += " event_type=values(event_type),";
              trx_entry_sqlstr_ending += " event_arg=values(event_arg),";
              trx_entry_sqlstr_ending += " message_length=values(message_length),";
              trx_entry_sqlstr_ending += " block_num=values(block_num),";
              trx_entry_sqlstr_ending += " trx_num=values(trx_num),";
              trx_entry_sqlstr_ending += " last_update_timestamp=values(last_update_timestamp);";
              std::stringstream sqlss;
              
              set_trx_type();
              sqlss << trx_entry_sqlstr_beging << "('";
              sqlss << trx.id().str() << "',";                              //trx_id   
              sqlss << static_cast<uint32_t>(trx_type) << ",'";       //trx_type 
              
              if (deposit_balance_id != Address())
              {
                  sqlss << deposit_balance_id.AddressToString() << "',";  //deposit_balance_id 
              }
              else
              {
                  sqlss << "',";  
              }

              int64_t deposit_amount = 0;
              for (auto depst : deposits)
              {
                  deposit_amount += depst.second;
              }
              sqlss << deposit_amount << ",'";                //deposit_amount

              if (withdraw_balance_id != Address())
              {
                  sqlss << withdraw_balance_id.AddressToString() << "',";  //withdraw_balance_id 
              }
              else
              {
                  sqlss << "',";   
              }

              int64_t withdraw_amount = 0;
              for (auto wthd : withdraws)
              {
                  withdraw_amount += wthd.second;
              }
              sqlss << withdraw_amount << ",";

              int64_t balance_diff = 0;
              for (auto bal : balance)
              {
                  balance_diff += bal.second;
              }

              sqlss << balance_diff << ",'";  // balance diff
              sqlss << trx.alp_account << "',"; // alp_account
              sqlss << "STR_TO_DATE('" << trx.expiration.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";//expiration
              sqlss << trx_amount << ","; //trx_amount
              sqlss << asset_id << ",";  //asset_id
              sqlss << required_fees.amount << ",";  //required_fees
              sqlss << transaction_fee.amount << ","; //transaction_fee
              sqlss << commission_charge.amount << ","; //commission_charge
              sqlss << operations.size() << ",'";        //op_count
              std::string  ops_str;
              std::stringstream  opsss;
              for (auto op : operations)
              {
                  opsss << op.type << ",";
              }

              sqlss << opsss.str()<<"','"; //operations
              if (contract_id != Address())
              {
                  sqlss << contract_id.AddressToString() << "','";  //contract_id
              }
              else
              {
                  sqlss <<  "','";  //contract_id    
              }

              sqlss << contract_method << "','";
              sqlss << contract_args << "','";
              sqlss << event_type << "','";
              sqlss << event_args << "',";
              sqlss << imessage_length << ",";
              sqlss << chain_location.block_num << ",";
              sqlss << chain_location.trx_num << ",";
              sqlss << "now())";
              sqlss << trx_entry_sqlstr_ending;

              return sqlss.str();

          }

        std::string TransactionEntry::compose_delete_sql()
        {
            std::stringstream sqlss;


            return sqlss.str();
        }
    }
} // thinkyoung::blockchain
