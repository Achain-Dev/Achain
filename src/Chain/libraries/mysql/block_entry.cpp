#include"blockchain/BlockEntry.hpp"
#include <sstream>

std::string BlockEntry::sqlstr_beging = "INSERT INTO block_entry VALUES ";
std::string BlockEntry::sqlstr_ending = "on duplicate key update block_num=values(block_num),block_timestamp=values(block_timestamp), processing_time=values(processing_time), sync_timestamp=values(sync_timestamp), last_update_timestamp=values(last_update_timestamp);";

std::string BlockEntry::compose_insert_sql()
{
    //std::string block_entry_sqlstr_beging = "INSERT INTO block_entry VALUES ";
    //std::string block_entry_sqlstr_ending = " on duplicate key update block_num=values(block_num),block_timestamp=values(block_timestamp), processing_time=values(processing_time), sync_timestamp=values(sync_timestamp), last_update_timestamp=values(last_update_timestamp); ";
    std::stringstream sqlss;
    //if (user_transaction_ids.size() == 0)
    //{
    //    return "NOEXECUTEMARK";
    //}
    sqlss << sqlstr_beging;
    sqlss << "(";
    sqlss << block_num << ",'";
    sqlss << id.str() << "','";  // blokc_id
    sqlss << previous.str() << "',";

    sqlss << "STR_TO_DATE('" << timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";

    sqlss << user_transaction_ids.size() << ",";
    sqlss << block_size << ",";
    sqlss << processing_time.count() << ",";   //microsecondd

    long unsigned  lat = latency.to_seconds() + 28800;
    sqlss << "TIMESTAMPADD(SECOND," << lat;
    sqlss << ",STR_TO_DATE('" + timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s')),";

    //sqlss << "STR_TO_DATE('" << syc_timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";

    sqlss << "now())";
    sqlss << sqlstr_ending;

    //write log
    //auto tmp_string = sqlss.str();
    return sqlss.str();
}

std::string BlockEntry::compose_insert_sql_value()
{
    std::stringstream sqlss;
    //if (user_transaction_ids.size() == 0)
    //{
    //    return "NOEXECUTEMARK";
    //}
    sqlss << "(";
    sqlss << block_num << ",'";
    sqlss << id.str() << "','";  // blokc_id
    sqlss << previous.str() << "',";

    sqlss << "STR_TO_DATE('" << timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";

    sqlss << user_transaction_ids.size() << ",";
    sqlss << block_size << ",";
    sqlss << processing_time.count() << ",";   //microsecond

    long unsigned  lat = latency.to_seconds() + 28800;  //8h  to Beijing timezone
    sqlss << "TIMESTAMPADD(SECOND," << lat;
    sqlss << ",STR_TO_DATE('" + timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s')),";

    //sqlss << "STR_TO_DATE('" << syc_timestamp.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";

    sqlss << "now())";
    return sqlss.str();
}