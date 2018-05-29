#pragma once
#include<iostream>
#include<string>
#include<memory>
#include"mysql.h"


class MysqlHandSingleton
{
private:
    MysqlHandSingleton();
    static MysqlHandSingleton * _instance_ptr; // must be initialized before use

    class Garbo//delete ptr
    {
        ~Garbo();
    };
    static Garbo garbo;
public:

    static MysqlHandSingleton * get_instance();

    ~MysqlHandSingleton();
    bool connect_to_mysql();
    void free_connect();
    bool run_insert_sql(std::string&  sqlstr);
    long max_block_num();
private:
    const std::string mysql_user = "root";             //  username
    const std::string mysql_pswd = "password";         //  password
    const std::string mysql_host = "localhost";        //  or"127.0.0.1"
    const std::string mysql_database = "actdb";     //  database
    unsigned int mysql_port = 3306;                    //  server port

    MYSQL myConnect;
    MYSQL_RES *result;
    MYSQL_ROW sql_row;
    //    MYSQL_FIELD *fd;

};

