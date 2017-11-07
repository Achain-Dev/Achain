/*
author: saiy
date: 2017.11.1
rpc client
*/
#ifndef _RPC_CLIENT_MGR_H_
#define _RPC_CLIENT_MGR_H_

#include <fc/thread/thread.hpp>
#include <fc/network/ip.hpp>
#include <net/StcpSocket.hpp>
#include <net/Message.hpp>
#include <mutex>
#include <memory>
#include <iostream>

class Client;
struct TaskBase;
struct TaskImplResult;
using thinkyoung::net::StcpSocketPtr;

using thinkyoung::net::Message;

class RpcClientMgr {
  public:
    RpcClientMgr(Client* client = nullptr);
    virtual ~RpcClientMgr();
    
    void start();
    
    void set_endpoint(std::string& ip_addr, int port);
    
    void insert_connection(thinkyoung::net::StcpSocketPtr&);
    void connect_to_server();
    void send_message(TaskBase*);
    
  private:
    void read_loop();
    void insert_task(TaskImplResult*);
    void task_imp();
    void process_task();
    TaskImplResult* parse_to_result(Message& msg);
    
    //Message& generate_message(TaskBase* task);
    
    
  private:
    StcpSocketPtr _rpc_client_ptr;
    fc::ip::endpoint _end_point;
    std::shared_ptr<fc::thread> _receive_msg_thread_ptr;
    std::shared_ptr<fc::thread> _task_proc_thread_ptr;
    std::vector<StcpSocketPtr>      _rpc_connections;
    std::vector<TaskImplResult*>          _tasks;
    Client* _client_ptr;
    bool _b_valid_flag;
    std::mutex              _task_mutex;
    uint64_t _bytes_received;
    fc::time_point _connected_time;
    fc::time_point _last_message_received_time;
};

#endif

