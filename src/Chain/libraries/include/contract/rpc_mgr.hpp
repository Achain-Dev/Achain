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
    Client* get_client();
    
    void init();
    void start_loop();
    void set_endpoint(std::string& ip_addr, int port);
    void set_last_receive_time();
    void post_message(TaskBase*, fc::promise<void*>::ptr);

    struct ProcTaskRequest
    {
        TaskBase* task;
        fc::promise<void*>::ptr task_promise;
    };
    
  private:
    RpcClientMgr(Client* client = nullptr);
    virtual ~RpcClientMgr();
    
  private:
    void start();
    void read_loop();
    void reconnect_to_server();
    Message generate_message(TaskBase*);
    void send_to_lvm(Message&);
    void read_from_lvm(Message&);
    TaskBase* parse_msg(Message&);
    void close_rpc_client();
    void connect_to_server();
    void store_request(TaskBase*, fc::promise<void*>::ptr&);
    void set_value(TaskBase*);
    
  private:
    StcpSocketPtr _rpc_client_ptr;
    fc::ip::endpoint _end_point;
    std::shared_ptr<fc::thread> _socket_thread_ptr;
    Client* _client_ptr;
    std::vector<ProcTaskRequest>  _tasks;
    std::mutex              _task_mutex;
    fc::time_point _last_hello_message_received_time;
    bool _b_valid_flag;
    
  private:
    static RpcClientMgr* _s_rpc_mgr_ptr;
};

#endif

