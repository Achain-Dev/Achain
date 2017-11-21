#include <net/Config.hpp>
#include <lvm/LvmMgr.hpp>
#include <contract/rpc_mgr.hpp>
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
const LuaRpcMessageTypeEnum LuaRequestTaskRpc::type = LuaRpcMessageTypeEnum::LUA_REQUEST_MESSAGE_TYPE;

//result
const LuaRpcMessageTypeEnum CompileTaskResultRpc::type = LuaRpcMessageTypeEnum::COMPILE_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum RegisterTaskResultRpc::type = LuaRpcMessageTypeEnum::REGTISTER_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CallTaskResultRpc::type = LuaRpcMessageTypeEnum::CALL_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum UpgradeTaskResultRpc::type = LuaRpcMessageTypeEnum::UPGRADE_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum DestroyTaskResultRpc::type = LuaRpcMessageTypeEnum::DESTROY_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum TransferTaskResultRpc::type = LuaRpcMessageTypeEnum::TRANSFER_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum LuaRequestTaskResultRpc::type = LuaRpcMessageTypeEnum::LUA_REQUEST_RESULT_MESSAGE_TYPE;

//hello msg
const LuaRpcMessageTypeEnum HelloMsgResultRpc::type = LuaRpcMessageTypeEnum::HELLO_MESSAGE_TYPE;


RpcClientMgr* RpcClientMgr::_s_rpc_mgr_ptr = nullptr;

RpcClientMgr::RpcClientMgr(Client* client)
    :_b_valid_flag(false) {
    _client_ptr = client;
    _last_hello_message_received_time = fc::time_point::min();
}

RpcClientMgr::~RpcClientMgr() {
    close_rpc_client();
    _socket_thread_ptr->quit();
    delete_rpc_mgr();
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
    _socket_thread_ptr = std::make_shared<fc::thread>("socket client");
}

//srart socket
void RpcClientMgr::start() {
    //start socket first
    _socket_thread_ptr->async([&]() {
        try {
            connect_to_server();
            
        } catch (...) {
            return;
        }
        
        this->read_loop();
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
        start();
    }
    
    fc::schedule([this]() {
        start_loop();
    }, fc::time_point::now() + fc::seconds(START_LOOP_TIME),
    "start_loop");
}

void RpcClientMgr::set_endpoint(std::string& ip_addr, int port) {
    fc::ip::address ip(ip_addr);
    _end_point = fc::ip::endpoint(ip, port);
    _b_valid_flag = true;
    return;
}

void RpcClientMgr::connect_to_server() {
    if (!_b_valid_flag)
        return;
        
    _rpc_client_ptr->connect_to(_end_point);
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
void RpcClientMgr::read_from_lvm(Message& m) {
    uint64_t remaining_bytes_with_padding = 0;
    char buffer[BUFFER_SIZE];
    int leftover = BUFFER_SIZE - sizeof(MessageHeader);
    
    try {
        /*first: read msgHead, get data.size*/
        _rpc_client_ptr->read(buffer, BUFFER_SIZE);
        /*convert to MessageHeader*/
        memcpy((char*)&m, buffer, sizeof(MessageHeader));
        FC_ASSERT(m.size <= MAX_MESSAGE_SIZE, "", ("m.size", m.size)("MAX_MESSAGE_SIZE", MAX_MESSAGE_SIZE));
        /*remaining len of byte to read from socket*/
        remaining_bytes_with_padding = 16 * ((m.size - leftover + 15) / 16);
        m.data.resize(leftover + remaining_bytes_with_padding);
        std::copy(buffer + sizeof(MessageHeader), buffer + sizeof(buffer), m.data.begin());
        
        if (remaining_bytes_with_padding) {
            _rpc_client_ptr->read(&m.data[leftover], remaining_bytes_with_padding);
        }
        
        m.data.resize(m.size);
        
    } catch (...) {
        FC_THROW_EXCEPTION(thinkyoung::blockchain::socket_read_error, \
                           "socket read error. ");
    }
    
    return;
}

//send msg to lvm
void RpcClientMgr::send_to_lvm(Message& m) {
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
        _rpc_client_ptr->write(padded_message.get(), size_with_padding);
        _rpc_client_ptr->flush();
        
    } catch (...) {
        elog("send message exception");
        FC_THROW_EXCEPTION(thinkyoung::blockchain::socket_send_error, \
                           "socket send error. ");
    }
}

//socket receive msg loop
void RpcClientMgr::read_loop() {
    TaskBase* result_p = NULL;
    static_assert(BUFFER_SIZE >= sizeof(MessageHeader), "insufficient buffer");
    
    try {
        Message m;
        
        while (true) {
            //read msg from async socket
            read_from_lvm(m);
            //receive response from lvm
            result_p = parse_msg(m);
            set_value(result_p);
        }
        
    } catch (thinkyoung::blockchain::socket_read_error& e) {
        elog("socket read message error.");
        reconnect_to_server();
    }
}

void RpcClientMgr::store_request(TaskBase* task, fc::promise<void*>::ptr& prom) {
    ProcTaskRequest task_request;
    _task_mutex.lock();
    task_request.task = task;
    task_request.task_promise = prom;
    _tasks.push_back(task_request);
    _task_mutex.unlock();
}

void RpcClientMgr::set_value(TaskBase* task_result) {
    FC_ASSERT(task_result);
    _task_mutex.lock();
    std::vector<ProcTaskRequest>::iterator iter = _tasks.begin();
    
    for (; iter != _tasks.end(); iter++) {
        if (iter->task->task_id == task_result->task_id) {
            break;
        }
    }
    
    if ((iter != _tasks.end()) && (!iter->task_promise->canceled())) {
        iter->task_promise->set_value((TaskImplResult*)task_result);
    }
    
    _tasks.erase(iter);
    _task_mutex.unlock();
}

//async send msg, throw thinkyoung::blockchain::async_socket_error when exception
void RpcClientMgr::post_message(TaskBase* task_msg, fc::promise<void*>::ptr& prom) {
    Message m(generate_message(task_msg));
    
    try {
        send_to_lvm(m);
        
        //if msg from FROM_RPC, store the task and promise;when receive the result,promosi->set_value
        //if msg from FROM_LUA_TO_CHAIN, do not store,just send the msg to LVM
        if (task_msg->task_from == FROM_RPC) {
            store_request(task_msg, prom);
        }
        
    } catch (thinkyoung::blockchain::socket_send_error& e) {
        elog("async socket send message exception");
        reconnect_to_server();
        FC_THROW_EXCEPTION(thinkyoung::blockchain::async_socket_error, \
                           "post msg error. ");
    }
}

void RpcClientMgr::close_rpc_client() {
    _rpc_client_ptr->close();
}

void RpcClientMgr::set_last_receive_time() {
    _last_hello_message_received_time = fc::time_point::now();
}

Message RpcClientMgr::generate_message(TaskBase* task_p) {
    FC_ASSERT(task_p);
    
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
        
        case LUA_REQUEST_RESULT_TASK: {
            LuaRequestTaskResultRpc task_rpc(*(LuaRequestTaskResult*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        default: {
            return Message();
        }
    }
}
TaskBase* RpcClientMgr::parse_msg(Message& msg) {
    TaskBase* result_p = NULL;
    
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
        
        case LUA_REQUEST_MESSAGE_TYPE: {
            LuaRequestTaskRpc request_task(msg.as<LuaRequestTaskRpc>());
            result_p = new LuaRequestTask(&request_task.data);
            break;
        }
        
        default: {
            //TODO
            result_p = nullptr;
        }
    }
    
    return result_p;
}