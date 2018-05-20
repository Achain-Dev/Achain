#include"blockchain/BlockEntry.hpp"
#include <sstream>


std::string BlockEntry::compose_insert_sql()
{

	std::string block_entry_sqlstr_beging = "INSERT INTO block_entry ( block_num, block_id, pre_block_id, block_timestamp, num_trxs, block_size, processing_time, sync_timestamp, last_update_timestamp ) VALUES ";
	std::string block_entry_sqlstr_ending = " on duplicate key update block_timestamp =values(block_timestamp), processing_time = values(processing_time), sync_timestamp =values(sync_timestamp), last_update_timestamp=values(last_update_timestamp) ; ";
	std::stringstream sqlss;

	sqlss << block_entry_sqlstr_beging;
	sqlss << "(";
	sqlss << block_num << ",'";
	sqlss << id.str() << "','";  // blokc_id
	sqlss << previous.str() << "',";

	sqlss << "STR_TO_DATE('" << timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";
	
	sqlss << user_transaction_ids.size() << ",";
	sqlss << block_size << ",";
	sqlss << processing_time.count() << ",";   //microsecondd

	sqlss << "TIMESTAMPADD(SECOND," << latency.to_seconds();
	sqlss << ",STR_TO_DATE('" + timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s')),";
	sqlss << "now())";
	sqlss << block_entry_sqlstr_ending;

	//write log
	auto tmp_string = sqlss.str();
	return sqlss.str();
}