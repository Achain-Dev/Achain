#include <net/Config.hpp>
#include <lvm/LvmMgr.hpp>
#include <contract/rpc_mgr.hpp>
#include <contract/rpc_message.hpp>
#include <contract/task_dispatcher.hpp>
#include <iostream>
#include <mutex>

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
const LuaRpcMessageTypeEnum CompileScriptTaskRpc::type = LuaRpcMessageTypeEnum::COMPILE_SCRIPT_MESSAGE_TPYE;
const LuaRpcMessageTypeEnum HandleEventsTaskRpc::type = LuaRpcMessageTypeEnum::HANDLE_EVENTS_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CallContractOfflineTaskRpc::type = LuaRpcMessageTypeEnum::CALL_OFFLINE_MESSAGE_TYPE;

//result
const LuaRpcMessageTypeEnum CompileTaskResultRpc::type = LuaRpcMessageTypeEnum::COMPILE_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum RegisterTaskResultRpc::type = LuaRpcMessageTypeEnum::REGTISTER_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CallTaskResultRpc::type = LuaRpcMessageTypeEnum::CALL_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum UpgradeTaskResultRpc::type = LuaRpcMessageTypeEnum::UPGRADE_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum DestroyTaskResultRpc::type = LuaRpcMessageTypeEnum::DESTROY_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum TransferTaskResultRpc::type = LuaRpcMessageTypeEnum::TRANSFER_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum LuaRequestTaskResultRpc::type = LuaRpcMessageTypeEnum::LUA_REQUEST_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CompileScriptTaskResultRpc::type = LuaRpcMessageTypeEnum::COMPILE_SCRIPT_RESULT_MESSAGE_TPYE;
const LuaRpcMessageTypeEnum HandleEventsTaskResultRpc::type = LuaRpcMessageTypeEnum::HANDLE_EVENTS_RESULT_MESSAGE_TYPE;
const LuaRpcMessageTypeEnum CallContractOfflineTaskResultRpc::type = LuaRpcMessageTypeEnum::CALL_OFFLINE_RESULT_MESSAGE_TYPE;

//hello msg
const LuaRpcMessageTypeEnum HelloMsgResultRpc::type = LuaRpcMessageTypeEnum::HELLO_MESSAGE_TYPE;


RpcClientMgr* RpcClientMgr::_s_rpc_mgr_ptr = nullptr;

RpcClientMgr::RpcClientMgr(Client* client)
    :_b_valid_flag(false) {
    _client_ptr = client;
    _last_hello_message_received_time = fc::time_point::min();
    _rpc_client_ptr = std::make_shared<StcpSocket>();
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

Client* RpcClientMgr::get_client() {
    return _client_ptr;
}

void RpcClientMgr::init() {
    _socket_thread_ptr = std::make_shared<fc::thread>("socket client");
}

//srart socket
void RpcClientMgr::start() {
    //start socket first
    try {
        connect_to_server();
        
    } catch (...) {
        return;
    }
    
    _socket_thread_ptr->async([&]() {
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
        _rpc_client_ptr->close();
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
    if (!_b_valid_flag) {
        return;
    }
    
    _rpc_client_ptr->connect_to(_end_point);
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
    TaskBase* result_p = nullptr;
    std::shared_ptr<TaskBase> sp_result;
    static_assert(BUFFER_SIZE >= sizeof(MessageHeader), "insufficient buffer");
    
    try {
        Message m;
        
        while (true) {
            //read msg from async socket
            read_from_lvm(m);
            //receive response from lvm
            result_p = parse_msg(m);
            
            if (result_p == nullptr) {
                continue;
            }
            
            if (result_p->task_type != LUA_REQUEST_TASK && result_p->task_type != HELLO_MSG) {
                set_value(result_p);
                return;
            }
            
            sp_result.reset(result_p);
            
            if (result_p->task_type == LUA_REQUEST_TASK) {
                TaskDispatcher::get_lua_task_dispatcher()->on_lua_request(result_p);
                
            } else {
                ((HelloMsgResult*)result_p)->process_result(this);
            }
        }
        
    } catch (thinkyoung::blockchain::socket_read_error& e) {
        elog("socket read message error.");
    }
}

void RpcClientMgr::store_request(TaskBase* task, fc::promise<void*>::ptr prom) {
    ProcTaskRequest task_request;
    task_request.task = task;
    task_request.task_promise = prom;
    std::lock_guard<std::mutex>  auto_lock(_task_mutex);
    _tasks.push_back(task_request);
}

void RpcClientMgr::set_value(TaskBase* task_result) {
    FC_ASSERT(task_result);
    std::lock_guard<std::mutex>  auto_lock(_task_mutex);
    auto iter = std::find_if(begin(_tasks), end(_tasks), [=](ProcTaskRequest proc_task_request) {
        return (proc_task_request.task->task_id == task_result->task_id);
    });
    
    if (iter != _tasks.end()) {
        if (iter->task_promise && !iter->task_promise->canceled()) {
            iter->task_promise->set_value((TaskImplResult*)task_result);
        }
        
        _tasks.erase(iter);
    }
}

//async send msg, throw thinkyoung::blockchain::async_socket_error when exception
void RpcClientMgr::post_message(TaskBase* task_msg, fc::promise<void*>::ptr prom) {
    Message m(generate_message(task_msg));
    fc::sync_call(_socket_thread_ptr.get(), [&]() {
        try {
            send_to_lvm(m);
            
            //if msg from FROM_RPC, store the task and promise;when receive the result,promosi->set_value
            //if msg from FROM_LUA_TO_CHAIN, do not store,just send the msg to LVM
            if (task_msg->task_from == FROM_RPC) {
                store_request(task_msg, prom);
            }
            
        } catch (thinkyoung::blockchain::socket_send_error& e) {
            elog("async socket send message exception");
            FC_THROW_EXCEPTION(thinkyoung::blockchain::async_socket_error, \
                               "post msg error. ");
        };
    }, "post_message");
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
        
        case COMPILE_SCRIPT_TASK: {
            CompileScriptTaskRpc task_rpc(*(CompileScriptTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case HANDLE_EVENTS_TASK: {
            HandleEventsTaskRpc task_rpc(*(HandleEventsTask*)task_p);
            task_rpc.data.task_from = FROM_RPC;
            Message msg(task_rpc);
            return msg;
        }
        
        case CALL_OFFLINE_TASK: {
            CallContractOfflineTaskRpc task_rpc(*(CallContractOfflineTask*)task_p);
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
        
        case COMPILE_RESULT_MESSAGE_TYPE: {
            CompileTaskResultRpc compile_task(msg.as<CompileTaskResultRpc>());
            result_p = new CompileTaskResult(&compile_task.data);
            break;
        }
        
        case CALL_RESULT_MESSAGE_TYPE: {
            CallTaskResultRpc call_task(msg.as<CallTaskResultRpc>());
            result_p = new CallTaskResult(&call_task.data);
            break;
        }
        
        case REGTISTER_RESULT_MESSAGE_TYPE: {
            RegisterTaskResultRpc register_task(msg.as<RegisterTaskResultRpc>());
            result_p = new RegisterTaskResult(&register_task.data);
            break;
        }
        
        case UPGRADE_RESULT_MESSAGE_TYPE: {
            UpgradeTaskResultRpc upgrade_task(msg.as<UpgradeTaskResultRpc>());
            result_p = new RegisterTaskResult(&upgrade_task.data);
            break;
        }
        
        case TRANSFER_RESULT_MESSAGE_TYPE: {
            TransferTaskResultRpc transfer_task(msg.as<TransferTaskResultRpc>());
            result_p = new TransferTaskResult(&transfer_task.data);
            break;
        }
        
        case DESTROY_RESULT_MESSAGE_TYPE: {
            DestroyTaskResultRpc destroy_task(msg.as<DestroyTaskResultRpc>());
            result_p = new DestroyTaskResult(&destroy_task.data);
            break;
        }
        
        case COMPILE_SCRIPT_RESULT_MESSAGE_TPYE: {
            CompileScriptTaskResultRpc compile_script_task(msg.as<CompileScriptTaskResultRpc>());
            result_p = new CompileScriptTaskResult(&compile_script_task.data);
            break;
        }
        
        case HANDLE_EVENTS_RESULT_MESSAGE_TYPE: {
            HandleEventsTaskResultRpc handle_events_task(msg.as<HandleEventsTaskResultRpc>());
            result_p = new HandleEventsTaskResult(&handle_events_task.data);
            break;
        }
        
        case CALL_OFFLINE_RESULT_MESSAGE_TYPE: {
            CallContractOfflineTaskResultRpc call_offline_task(msg.as<CallContractOfflineTaskResultRpc>());
            result_p = new CallContractOfflineTaskResult(&call_offline_task.data);
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
