#include "fc/log/logger.hpp"
#include "mysql/MysqlHand.h"
#include <assert.h>
#include <sstream>
#include <stdlib.h>
#include <time.h>


//#define myssqllogger "mysql"

MysqlHand * MysqlHand::_instance_ptr = nullptr;
std::vector<MysqlHand *> MysqlHand::_instance_ptr_vec(10, nullptr);
//static private member must be initialized before using;

MysqlHand::Garbo::~Garbo()
{
    if (MysqlHand::_instance_ptr)
        delete MysqlHand::_instance_ptr;

    for (auto itr : _instance_ptr_vec)
    {
        if (itr)
        {
            delete itr;
        }
    }
}

void MysqlHand::init_all_instance(int instance_num )
{
    for (int i = 0; i < instance_num; i++)
    {
        _instance_ptr_vec[i] = new MysqlHand(i + 1);
    }
}

MysqlHand::MysqlHand():
mysql_user("root")
, mysql_pswd("password")
{
    if (!connect_to_mysql())
    {
        assert(false);
    }
}

MysqlHand::MysqlHand(int user_seq):
mysql_pswd("password")
{
    std::stringstream  userss;
    userss << "user_";
    userss << user_seq;
    mysql_user = userss.str();

    if (!connect_to_mysql())
    {
        assert(false);
    }
}

MysqlHand * MysqlHand::get_instance()
{
    unsigned rand_seq;
    std::srand((unsigned)time(NULL));  
    rand_seq = std::rand() % 10;   // 随机获取一个实例
    assert(!_instance_ptr_vec[rand_seq]);
    return _instance_ptr_vec[rand_seq];

}

//MysqlHand * MysqlHand::get_instance( bool overload)
//{
//    if (!_instance_ptr)
//        _instance_ptr = new MysqlHand();
//    return _instance_ptr;
//}

MysqlHand::~MysqlHand()
{
    free_connect();
}

bool MysqlHand::connect_to_mysql()
{
    mysql_init(&myConnect);
    if (mysql_real_connect(&myConnect, mysql_host.c_str(), mysql_user.c_str(), mysql_pswd.c_str(),
        mysql_database.c_str(), mysql_port, NULL, 0))
    {
        fc_ilog(fc::logger::get("mysql"), " user     : ${u}", ("u", mysql_user));
        fc_ilog(fc::logger::get("mysql"), " host     : ${h}", ("h", mysql_host));
        fc_ilog(fc::logger::get("mysql"), " port     : ${p}", ("p", mysql_pswd));
        fc_ilog(fc::logger::get("mysql"), " database : ${d}", ("d", mysql_database));
        fc_ilog(fc::logger::get("mysql"), " Database connected successfully!");

    }
    else
    {
        fc_elog(fc::logger::get("mysql"), " Database connect failure ! Error info: ${err} ", ("err", mysql_error(&myConnect)));
    }

    //mysql_query(&myConnect, "set names gbk");
    //mysql_query(&myConnect, "select * from block_entry;");
    return true;
}

void MysqlHand::free_connect()
{
    if (result)
    {
        mysql_free_result(result);
    }
    if (&myConnect)
    {
        mysql_close(&myConnect);
    }
}

bool MysqlHand::run_insert_sql(std::string&  sql_str)
{

    if (sql_str == "NOEXECUTEMARK")
    {
        return true;   //jump
    }
    //std::cout << sql_str << std::endl;
    if (mysql_query(&myConnect, sql_str.c_str()))
    {
        fc_elog(fc::logger::get("mysql"), " Run insert sql failed ! Error info: ${err} ", ("err", mysql_error(&myConnect)));
        if (sql_str.size() < 1000)
        {
            fc_elog(fc::logger::get("mysql"), " The sqlstr : ${s} ", ("s", sql_str));
        }
        //mysql_sqlstate(&myConnect);
        return false;

    }
    else
    {
        return true;
    }
}

long MysqlHand::max_block_num()
{
    std::string querySqlStr;
    uint64_t maxBlockNum;
    querySqlStr = "SELECT MAX(block_num) FROM block_entry;";

    if (mysql_query(&myConnect, querySqlStr.c_str()))
    {
        fc_elog(fc::logger::get("mysql"), " Run max_block_num sql failed ! Error info: ${err} ", ("err", mysql_error(&myConnect)));
        fc_elog(fc::logger::get("mysql"), " The sqlstr : ${s} ", ("s", querySqlStr));
        //mysql_sqlstate(&myConnect);
        return -1;
    }

    if (!(result = mysql_store_result(&myConnect)))
    {
        fc_elog(fc::logger::get("mysql"), " Run max_block_num sql failed ! Error info: ${err} ", ("err", mysql_error(&myConnect)));
        fc_elog(fc::logger::get("mysql"), " The sqlstr : ${s} ", ("s", querySqlStr));
        return -1;
    }

    if (sql_row = mysql_fetch_row(result))
    {
        std::istringstream iStream;
        iStream.str(sql_row[0]);

        if (!iStream)
        {
            maxBlockNum = 0;
        }
        else
        {
            iStream >> maxBlockNum;
        }
    }


    if (result)
    {
        mysql_free_result(result);
    }

    return maxBlockNum;
}