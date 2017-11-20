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
#include <net/Message.hpp>
#include <net/StcpSocket.hpp>
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
    
    void init();
    void start_loop();
    void set_endpoint(std::string& ip_addr, int port);
    void send_message(TaskBase*);
    void set_last_receive_time();
    void post_message(TaskBase*);
    Client* get_client();
    
  private:
    RpcClientMgr(Client* client = nullptr);
    virtual ~RpcClientMgr();
    
  private:
    void start();
    void read_loop(SocketMode emode);
    void insert_task(TaskImplResult*);
    void task_imp();
    void process_task(RpcClientMgr*);
    void reconnect_to_server();
    Message generate_message(TaskBase* task_p);
    void send_to_lvm(Message& m, StcpSocketPtr& sock_ptr);
    void read_from_lvm(StcpSocketPtr& sock, Message& m);
    TaskImplResult* parse_to_result(Message& msg);
    
    void close_rpc_client();
    void insert_connection(StcpSocketPtr&);
    void connect_to_server(SocketMode emode);
    StcpSocketPtr get_connection(SocketMode emode);
    
  private:
    //StcpSocketPtr _rpc_client_ptr[MODE_COUNT];
    std::vector<StcpSocketPtr>      _rpc_connections;
    fc::ip::endpoint _end_point;
    std::shared_ptr<fc::thread> _async_thread_ptr;
    std::shared_ptr<fc::thread> _sync_thread_ptr;
    std::shared_ptr<fc::thread> _task_proc_thread_ptr;
    
    std::vector<TaskImplResult*>          _tasks;
    Client* _client_ptr;
    std::mutex              _task_mutex;
    fc::time_point _last_hello_message_received_time;
    bool _b_valid_flag;
    
  private:
    static RpcClientMgr* _s_rpc_mgr_ptr;
};

#endif

