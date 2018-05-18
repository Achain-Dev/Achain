#include "mysql/MysqlHand.h"
#include <assert.h>
#include <sstream>


MysqlHandSingleton * MysqlHandSingleton::_instance_ptr = nullptr;
//static private member must be initialized before using;

MysqlHandSingleton::Garbo::~Garbo()
{
    {
        if (MysqlHandSingleton::_instance_ptr)
            delete MysqlHandSingleton::_instance_ptr;
    }
}

MysqlHandSingleton::MysqlHandSingleton()
{
    if (!connect_to_mysql())
    {
        assert(false);
    }
}

MysqlHandSingleton * MysqlHandSingleton::get_instance()
{
    if (!_instance_ptr)
        _instance_ptr = new MysqlHandSingleton();
    return _instance_ptr;

}


MysqlHandSingleton::~MysqlHandSingleton()
{
    free_connect();
}

bool MysqlHandSingleton::connect_to_mysql()
{
    mysql_init(&myConnect);
    if (mysql_real_connect(&myConnect, mysql_host.c_str(), mysql_user.c_str(), mysql_pswd.c_str(),
        mysql_database.c_str(), mysql_port, NULL, 0))
    {
        std::cout << " user     :  " << mysql_user << std::endl;
        std::cout << " host     :  " << mysql_host << std::endl;
        std::cout << " port     :  " << mysql_port << std::endl;
        std::cout << " database :  " << mysql_database << std::endl;
        std::cout << " Database connected successfully!" << std::endl;
    }
    else
    {
        std::cout << "Database connect failure !" << mysql_error(&myConnect) << std::endl;
    }

    //mysql_query(&myConnect, "set names gbk");
    //mysql_query(&myConnect, "select * from block_entry;");
    return true;
}

void MysqlHandSingleton::free_connect()
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

bool MysqlHandSingleton::run_insert_sql(std::string&  sql_str)
{


    //std::cout << sql_str << std::endl;
    if (mysql_query(&myConnect, sql_str.c_str()))
    {
        std::cout << "Insert into block_entry failed, " << mysql_error(&myConnect) << std::endl;
        if (sql_str.size() < 1000)
        {
            std::cout << "The sqlstr :  " << sql_str << std::endl;
        }
        //mysql_sqlstate(&myConnect);
        return false;

    }
    else
    {
        return true;
    }
}

long MysqlHandSingleton::max_block_num()
{
    std::string querySqlStr;
    uint64_t maxBlockNum;
    querySqlStr = "SELECT MAX(block_num) FROM block_entry;";

    if (mysql_query(&myConnect, querySqlStr.c_str()))
    {
        std::cout << "Insert into block_entry failed, " << mysql_error(&myConnect) << std::endl;
        std::cout << "The sqlstr :  " << querySqlStr << std::endl;
        //mysql_sqlstate(&myConnect);
        return -1;
    }

    if (!(result = mysql_store_result(&myConnect)))
    {
        std::cout << "Couldn't get the query result, " << mysql_error(&myConnect) << std::endl;
        std::cout << "The sqlstr :  " << querySqlStr << std::endl;
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