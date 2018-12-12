#pragma once
#include<iostream>
#include<string>
#include<memory>
#include"mysql.h"
#include<future>


class MysqlHand
{
private:
    MysqlHand();
    MysqlHand(int user_seq);
    static MysqlHand * _instance_ptr; // must be initialized before use
    static std::vector<MysqlHand *> _instance_ptr_vec; //change to muti-instance

    class Garbo//delete ptr
    {
        ~Garbo();
    };
    static Garbo garbo;
public:
    static void init_all_instance( int instance_num = 10 );

    static MysqlHand * get_instance();
    //static MysqlHand * get_instance(bool overload);

    ~MysqlHand();
    bool connect_to_mysql();
    void free_connect();
    bool run_insert_sql(std::string&  sqlstr);
    long max_block_num();
private:
    std::string mysql_user;                   //  username
    std::string mysql_pswd;              //  password
    int _instance_num;
    const std::string mysql_host = "localhost";        //  or"127.0.0.1"
    const std::string mysql_database = "actdb";        //  database
    unsigned int mysql_port = 3306;                    //  server port
    MYSQL myConnect;
    MYSQL_RES *result;
    MYSQL_ROW sql_row;
    //    MYSQL_FIELD *fd;

};

