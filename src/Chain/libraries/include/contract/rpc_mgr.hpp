/*
author: saiy
date: 2017.11.1
rpc client
*/
#ifndef _RPC_CLIENT_MGR_H_
#define _RPC_CLIENT_MGR_H_

#include <client/Client.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/ip.hpp>
#include <net/StcpSocket.hpp>
#include <net/Message.hpp>
#include <mutex>
#include <memory>
#include <iostream>

struct TaskBase;
struct TaskImplResult;
class TaskHandler;
using thinkyoung::net::StcpSocketPtr;
using thinkyoung::client::Client;
using thinkyoung::net::Message;

class RpcClientMgr {
  public:
    static RpcClientMgr* get_rpc_mgr(Client* client = nullptr);
    static void   delete_rpc_mgr();
    
    void start_loop();
    void init();
    
    void set_endpoint(std::string& ip_addr, int port);
    
    void insert_connection(thinkyoung::net::StcpSocketPtr&);
    void connect_to_server();
    void send_message(TaskBase*);
    void set_last_receive_time();
    void execute_task(TaskBase*);
    Client* get_client();
    
  private:
    RpcClientMgr(Client* client = nullptr);
    virtual ~RpcClientMgr();
    
  private:
    void start();
    void read_loop();
    void insert_task(TaskImplResult*);
    void task_imp();
    void process_task(RpcClientMgr*);
    void reconnect_to_server();
    Message generate_message(TaskBase* task_p);
    
    TaskImplResult* parse_to_result(Message& msg);
    
    
  private:
    StcpSocketPtr _rpc_client_ptr;
    fc::ip::endpoint _end_point;
    std::shared_ptr<fc::thread> _receive_msg_thread_ptr;
    std::shared_ptr<fc::thread> _task_proc_thread_ptr;
    std::vector<StcpSocketPtr>      _rpc_connections;
    std::vector<TaskImplResult*>          _tasks;
    Client* _client_ptr;
    std::mutex              _task_mutex;
    fc::time_point _last_hello_message_received_time;
    bool _b_valid_flag;
    
    TaskHandler* _task_handler_ptr;
    
  private:
    static RpcClientMgr* _s_rpc_mgr_ptr;
};

#endif

