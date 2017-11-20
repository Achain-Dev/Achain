#include <net/Config.hpp>
#include <lvm/LvmMgr.hpp>
#include <contract/rpc_mgr.hpp>
//#include <net/StcpSocket.hpp>
#include <contract/rpc_message.hpp>
#include <iostream>

using thinkyoung::net::Message;
using thinkyoung::net::MessageHeader;
using thinkyoung::net::StcpSocketPtr;
using thinkyoung::net::StcpSocket;

const int BUFFER_SIZE = 16;


//task
const LuaRpcMessageTypeEnum CompileTaskRpc::type = LuaRpcMessageTypeEnum::COMPILE_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CallTaskRpc::type = LuaRpcMessageTypeEnum::CALL_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum RegisterTaskRpc::type = LuaRpcMessageTypeEnum::REGTISTER_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum UpgradeTaskRpc::type = LuaRpcMessageTypeEnum::UPGRADE_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum TransferTaskRpc::type = LuaRpcMessageTypeEnum::TRANSFER_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum DestroyTaskRpc::type = LuaRpcMessageTypeEnum::DESTROY_MESSAGE_TYPE;

//result
const LuaRpcMessageTypeEnum CompileTaskResultRpc::type = LuaRpcMessageTypeEnum::COMPILE_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum RegisterTaskResultRpc::type = LuaRpcMessageTypeEnum::REGTISTER_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CallTaskResultRpc::type = LuaRpcMessageTypeEnum::CALL_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum UpgradeTaskResultRpc::type = LuaRpcMessageTypeEnum::UPGRADE_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum DestroyTaskResultRpc::type = LuaRpcMessageTypeEnum::DESTROY_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum TransferTaskResultRpc::type = LuaRpcMessageTypeEnum::TRANSFER_MESSAGE_TYPE;

//hello msg
const LuaRpcMessageTypeEnum HelloMsgResultRpc::type = LuaRpcMessageTypeEnum::HELLO_MESSAGE_TYPE;

RpcClientMgr* RpcClientMgr::_s_rpc_mgr_ptr = nullptr;

RpcClientMgr::RpcClientMgr(Client* client)
    :_b_valid_flag(false) {
    _client_ptr = client;
    _last_hello_message_received_time = fc::time_point::min();
    
    for (int iter = ASYNC_MODE; iter < MODE_COUNT; iter++) {
        _rpc_connections.push_back(std::make_shared<StcpSocket>());
    }
}

RpcClientMgr::~RpcClientMgr() {
    close_rpc_client();
    _async_thread_ptr->quit();
    _sync_thread_ptr->quit();
    _task_proc_thread_ptr->quit();
}

//get rpc_mgr
RpcClientMgr* RpcClientMgr::get_rpc_mgr(Client* client) {
    if (!RpcClientMgr::_s_rpc_mgr_ptr) {
        RpcClientMgr::_s_rpc_mgr_ptr = new RpcClientMgr(client);
    }
    
    return RpcClientMgr::_s_rpc_mgr_ptr;
}

void RpcClientMgr::delete_rpc_mgr() {
    if (RpcClientMgr::_s_rpc_mgr_ptr) {
        delete RpcClientMgr::_s_rpc_mgr_ptr;
    }
}

void RpcClientMgr::init() {
    _async_thread_ptr = std::make_shared<fc::thread>("async");
    _sync_thread_ptr = std::make_shared<fc::thread>("sync");
    _task_proc_thread_ptr = std::make_shared<fc::thread>("task_proc");
}

//srart socket
void RpcClientMgr::start() {
    //start async socket first
    connect_to_server(ASYNC_MODE);
    _async_thread_ptr->async([&]() {
        this->read_loop(ASYNC_MODE);
    });
    //then start sync scoket,just connect to server
    connect_to_server(SYNC_MODE);
    _sync_thread_ptr->async([&]() {
        this->read_loop(SYNC_MODE);
    });
}

//async socket start loop
void RpcClientMgr::start_loop() {
    uint64_t interval = 0;
    interval = (fc::time_point::now() - _last_hello_message_received_time).to_seconds();
    
    //if the interval bigger than TIME_INTERVAL, the lvm maybe error, then restart the lvm
    if (interval > TIME_INTERVAL) {
        //start lvm
        FC_ASSERT(_client_ptr);
        thinkyoung::lvm::LvmMgrPtr lvm_mgr = _client_ptr->get_lvm_mgr();
        FC_ASSERT(lvm_mgr);
        lvm_mgr->run_lvm();
        
        try {
            start();
            
        } catch (fc::exception& e) {
            //TODO
        }
    }
    
    fc::schedule([this]() {
        start_loop();
    }, fc::time_point::now() + fc::seconds(99999999),
    "start_loop");
    //START_LOOP_TIME
}

void RpcClientMgr::set_endpoint(std::string& ip_addr, int port) {
    fc::ip::address ip(ip_addr);
    _end_point = fc::ip::endpoint(ip, port);
    _b_valid_flag = true;
    return;
}

void RpcClientMgr::connect_to_server(SocketMode emode) {
    StcpSocketPtr tmp = NULL;
    
    if (!_b_valid_flag)
        return;
        
    tmp = get_connection(emode);
    
    if (tmp) {
        tmp->connect_to(_end_point);
    }
}

void RpcClientMgr::reconnect_to_server() {
    int times = 0;
    
    while (times < RECONNECT_TIMES) {
        try {
            start();
            break;
            
        } catch (fc::exception& e) {
            times++;
        }
    }
}

//receive msg
void RpcClientMgr::read_from_lvm(StcpSocketPtr& sock, Message& m) {
    uint64_t remaining_bytes_with_padding = 0;
    char buffer[BUFFER_SIZE];
    int leftover = BUFFER_SIZE - sizeof(MessageHeader);
    
    try {
        /*first: read msgHead, get data.size*/
        sock->read(buffer, BUFFER_SIZE);
        /*convert to MessageHeader*/
        memcpy((char*)&m, buffer, sizeof(MessageHeader));
        FC_ASSERT(m.size <= MAX_MESSAGE_SIZE, "", ("m.size", m.size)("MAX_MESSAGE_SIZE", MAX_MESSAGE_SIZE));
        /*remaining len of byte to read from socket*/
        remaining_bytes_with_padding = 16 * ((m.size - leftover + 15) / 16);
        m.data.resize(leftover + remaining_bytes_with_padding);
        std::copy(buffer + sizeof(MessageHeader), buffer + sizeof(buffer), m.data.begin());
        
        if (remaining_bytes_with_padding) {
            sock->read(&m.data[leftover], remaining_bytes_with_padding);
        }
        
        m.data.resize(m.size);
        
    } catch (...) {
        FC_THROW_EXCEPTION(thinkyoung::blockchain::socket_read_error, \
                           "socket read error. ");
    }
    
    return;
}

//send msg to lvm
void RpcClientMgr::send_to_lvm(Message& m, StcpSocketPtr& sock_ptr) {
    uint32_t size_of_message_and_header = 0;
    uint32_t size_with_padding = 0;
    size_of_message_and_header = sizeof(MessageHeader) + m.size;
    //pad the message we send to a multiple of 16 bytes
    size_with_padding = 16 * ((size_of_message_and_header + 15) / 16);
    std::unique_ptr<char[]> padded_message(new char[size_with_padding]);
    memcpy(padded_message.get(), (char*)&m, sizeof(MessageHeader));
    memcpy(padded_message.get() + sizeof(MessageHeader), m.data.data(), m.size);
    
    //send
    try {
        sock_ptr->write(padded_message.get(), size_with_padding);
        sock_ptr->flush();
        
    } catch (...) {
        elog("send message exception");
        FC_THROW_EXCEPTION(thinkyoung::blockchain::socket_send_error, \
                           "socket send error. ");
    }
}

//async socket receive msg loop
void RpcClientMgr::read_loop(SocketMode emode) {
    TaskImplResult* result_p = NULL;
    StcpSocketPtr tmp = NULL;
    const int BUFFER_SIZE = 16;
    static_assert(BUFFER_SIZE >= sizeof(MessageHeader), "insufficient buffer");
    
    try {
        Message m;
        tmp = get_connection(emode);
        
        while (true) {
            //read msg from async socket
            read_from_lvm(tmp, m);
            
            if (ASYNC_MODE == emode) {
                //receive response from lvm,insert the result to _tasks
                result_p = parse_to_result(m);
                insert_task(result_p);
            }
            
            //process sync msg
            else {
                //receive request from lvm, process request, then send result to lvm
                //TODO:process msg,then send result to lvm
                //send_to_lvm();
            }
        }
    } catch (thinkyoung::blockchain::socket_read_error& e) {
        elog("socket read message error.");
        reconnect_to_server();
    }
}

//async send msg, throw thinkyoung::blockchain::async_socket_error when exception
//this interface called by broadcast msg : evaluate contract, this process is async
void RpcClientMgr::post_message(TaskBase* task_msg) {
    Message m(generate_message(task_msg));
    
    try {
        send_to_lvm(m, get_connection(ASYNC_MODE));
        
    } catch (thinkyoung::blockchain::socket_send_error& e) {
        elog("async socket send message exception");
        reconnect_to_server();
        FC_THROW_EXCEPTION(thinkyoung::blockchain::async_socket_error, \
                           "post msg error. ");
    }
}

//sync rpc send msg;throw thinkyoung::blockchain::sync_socket_error when exception
//this interface called by CLI/RPC on demand,this process is sync
void RpcClientMgr::send_message(TaskBase* rpc_msg) {
    uint32_t size_of_message_and_header = 0;
    uint32_t size_with_padding = 0;
    StcpSocketPtr tmp = NULL;
    Message m(generate_message(rpc_msg));
    Message rec_m;
    
    //send response
    try {
        tmp = get_connection(SYNC_MODE);
        FC_ASSERT(tmp);
        send_to_lvm(m, tmp);
        read_from_lvm(tmp, rec_m);
        
    } catch (thinkyoung::blockchain::socket_send_error& e) {
        elog("sync socket send message exception");
        reconnect_to_server();
        FC_THROW_EXCEPTION(thinkyoung::blockchain::sync_socket_error, \
                           "send msg error. ");
                           
    } catch (thinkyoung::blockchain::socket_read_error& e) {
        elog("sync socket read message exception");
        reconnect_to_server();
        FC_THROW_EXCEPTION(thinkyoung::blockchain::sync_socket_error, \
                           "reeive msg error. ");
    }
    
    //TODO : result from lvm,then return it to CLI OR RPC
    return;
}

StcpSocketPtr RpcClientMgr::get_connection(SocketMode emode) {
    FC_ASSERT(emode < MODE_COUNT);
    StcpSocketPtr tmp = NULL;
    
    try {
        tmp = _rpc_connections.at(uint32_t(emode));
        
    } catch (std::exception& e) {
        tmp = NULL;
    }
    
    return tmp;
}

void RpcClientMgr::close_rpc_client() {
    std::vector<StcpSocketPtr>::iterator iter = _rpc_connections.begin();
    
    for (; iter != _rpc_connections.end(); iter++) {
        (*iter)->close();
    }
}

void RpcClientMgr::set_last_receive_time() {
    _last_hello_message_received_time = fc::time_point::now();
}

/////process task
void RpcClientMgr::insert_task(TaskImplResult* task) {
    _task_mutex.lock();
    _tasks.push_back(task);
    std::vector<TaskImplResult*>::iterator iter = _tasks.begin();
    CompileTaskResult* result = (CompileTaskResult*)(*iter);
    _task_mutex.unlock();
    task_imp();
}

void RpcClientMgr::task_imp() {
    _task_proc_thread_ptr->schedule([this]() {
        process_task(this);
    },
    fc::time_point::now() + fc::seconds(DISPATCH_TASK_TIMESPAN),
    "process the task");
}

//process result received from lvm
void RpcClientMgr::process_task(RpcClientMgr* msg_p) {
    TaskImplResult* ptask = nullptr;
    _task_mutex.lock();
    std::vector<TaskImplResult*>::iterator iter = _tasks.begin();
    
    while (iter != _tasks.end()) {
        ptask = (*iter);
        (*iter)->process_result(msg_p);
        iter = _tasks.erase(iter);
        delete ptask;
    }
    
    _task_mutex.unlock();
}
Client* RpcClientMgr::get_client() {
    return _client_ptr;
};

Message RpcClientMgr::generate_message(TaskBase* task_p) {
    FC_ASSERT(task_p != NULL);
    
    switch (task_p->task_type) {
        case COMPILE_TASK: {
            CompileTaskRpc task_rpc(*(CompileTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case REGISTER_TASK: {
            RegisterTaskRpc task_rpc(*(RegisterTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case UPGRADE_TASK: {
            UpgradeTaskRpc task_rpc(*(UpgradeTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case CALL_TASK: {
            CallTaskRpc task_rpc(*(CallTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case TRANSFER_TASK: {
            TransferTaskRpc task_rpc(*(TransferTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case DESTROY_TASK: {
            DestroyTaskRpc task_rpc(*(DestroyTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        default: {
            return Message();
        }
    }
}

TaskImplResult* RpcClientMgr::parse_to_result(Message& msg) {
    TaskImplResult* result_p = NULL;
    
    switch (msg.msg_type) {
        case HELLO_MESSAGE_TYPE: {
            HelloMsgResultRpc hello_msg(msg.as<HelloMsgResultRpc>());
            result_p = new HelloMsgResult();
            result_p->task_type = hello_msg.data.task_type;
            break;
        }
        
        case COMPILE_MESSAGE_TYPE: {
            CompileTaskResultRpc compile_task(msg.as<CompileTaskResultRpc>());
            result_p = new CompileTaskResult(&compile_task.data);
            break;
        }
        
        case CALL_MESSAGE_TYPE: {
            CallTaskResultRpc call_task(msg.as<CallTaskResultRpc>());
            result_p = new CallTaskResult(&call_task.data);
            break;
        }
        
        case REGTISTER_MESSAGE_TYPE: {
            RegisterTaskResultRpc register_task(msg.as<RegisterTaskResultRpc>());
            result_p = new RegisterTaskResult(&register_task.data);
            break;
        }
        
        case UPGRADE_MESSAGE_TYPE: {
            UpgradeTaskResultRpc upgrade_task(msg.as<UpgradeTaskResultRpc>());
            result_p = new RegisterTaskResult(&upgrade_task.data);
            break;
        }
        
        case TRANSFER_MESSAGE_TYPE: {
            TransferTaskResultRpc transfer_task(msg.as<TransferTaskResultRpc>());
            result_p = new TransferTaskResult(&transfer_task.data);
            break;
        }
        
        case DESTROY_MESSAGE_TYPE: {
            DestroyTaskResultRpc destroy_task(msg.as<DestroyTaskResultRpc>());
            result_p = new DestroyTaskResult(&destroy_task.data);
            break;
        }
        
        default: {
            //TODO
            result_p = nullptr;
        }
    }
    
    return result_p;
}
