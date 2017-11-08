#include <contract/rpc_mgr.hpp>
#include <contract/rpc_message.hpp>
#include <iostream>

using thinkyoung::net::Message;
using thinkyoung::net::MessageHeader;


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

RpcClientMgr::RpcClientMgr(Client* client)
    :_receive_msg_thread_ptr(std::make_shared<fc::thread>("server")),
     _b_valid_flag(false),
     _rpc_client_ptr(std::make_shared<thinkyoung::net::StcpSocket>()),
     _task_proc_thread_ptr(std::make_shared<fc::thread>("task_proc")) {
    _client_ptr = client;
}

RpcClientMgr::~RpcClientMgr() {
}

void RpcClientMgr::start() {
    connect_to_server();
    _receive_msg_thread_ptr->async([&]() {
        this->read_loop();
    });
}

void RpcClientMgr::task_imp() {
    _task_proc_thread_ptr->schedule([this]() {
        std::cout << "receive task" << std::endl;
        process_task();
    },
    fc::time_point::now() + fc::seconds(DISPATCH_TASK_TIMESPAN),
    "process the task");
}

void RpcClientMgr::process_task() {
    _task_mutex.lock();
    std::vector<TaskImplResult*>::iterator iter = _tasks.begin();
    
    while (iter != _tasks.end()) {
        if ((*iter)->task_type == COMPILE_TASK) {
            std::cout << "recieve response......." << std::endl;
            std::cout << "task_from: " << ((*iter)->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
            std::cout << "task_id: " << (*iter)->task_id << std::endl;
            std::cout << "task_type: " << (*iter)->task_type << std::endl;
            CompileTaskResult* task = (CompileTaskResult*)(*iter);
            std::cout << "error_code: " << task->error_code << std::endl;
            std::cout << "error_msg: " << task->error_msg << std::endl;
            iter = _tasks.erase(iter);
            delete task;
        }
    }
    
    _task_mutex.unlock();
}

void RpcClientMgr::insert_task(TaskImplResult* task) {
    _task_mutex.lock();
    _tasks.push_back(task);
    std::vector<TaskImplResult*>::iterator iter = _tasks.begin();
    CompileTaskResult* result = (CompileTaskResult*)(*iter);
    _task_mutex.unlock();
    task_imp();
}

void RpcClientMgr::set_endpoint(std::string& ip_addr, int port) {
    fc::ip::address ip(ip_addr);
    _end_point = fc::ip::endpoint(ip, port);
    _b_valid_flag = true;
    return;
}


void RpcClientMgr::read_loop() {
    TaskImplResult* result_p = NULL;
    const int BUFFER_SIZE = 16;
    const int LEFTOVER = BUFFER_SIZE - sizeof(MessageHeader);
    static_assert(BUFFER_SIZE >= sizeof(MessageHeader), "insufficient buffer");
    _connected_time = fc::time_point::now();
    fc::oexception exception_to_rethrow;
    bool call_on_connection_closed = false;
    
    try {
        Message m;
        
        while (true) {
            uint64_t bytes_received = 0;
            uint64_t remaining_bytes_with_padding = 0;
            char buffer[BUFFER_SIZE];
            /*first: read msgHead, get data.size*/
            _rpc_client_ptr->read(buffer, BUFFER_SIZE);
            _bytes_received += BUFFER_SIZE;
            /*convert to MessageHeader*/
            memcpy((char*)&m, buffer, sizeof(MessageHeader));
            FC_ASSERT(m.size <= 1000000, "", ("m.size", m.size)("MAX_MESSAGE_SIZE", 1000000));
            /*remaining len of byte to read from socket*/
            remaining_bytes_with_padding = 16 * ((m.size - LEFTOVER + 15) / 16);
            m.data.resize(LEFTOVER + remaining_bytes_with_padding);
            std::copy(buffer + sizeof(MessageHeader), buffer + sizeof(buffer), m.data.begin());
            
            if (remaining_bytes_with_padding) {
                _rpc_client_ptr->read(&m.data[LEFTOVER], remaining_bytes_with_padding);
                _bytes_received += remaining_bytes_with_padding;
            }
            
            m.data.resize(m.size); // truncate off the padding bytes
            //get task result pointer
            result_p = parse_to_result(m);
            insert_task(result_p);
            _last_message_received_time = fc::time_point::now();
        }
        
    } catch (const fc::canceled_exception& e) {
        wlog("caught a canceled_exception in read_loop.  this should mean we're in the process of deleting this object already, so there's no need to notify the delegate: ${e}", ("e", e.to_detail_string()));
        throw;
        
    } catch (const fc::eof_exception& e) {
        wlog("disconnected ${e}", ("e", e.to_detail_string()));
        call_on_connection_closed = true;
        
    } catch (const fc::exception& e) {
        elog("disconnected ${er}", ("er", e.to_detail_string()));
        call_on_connection_closed = true;
        exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", e.to_detail_string())));
        
    } catch (const std::exception& e) {
        elog("disconnected ${er}", ("er", e.what()));
        call_on_connection_closed = true;
        exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", e.what())));
        
    } catch (...) {
        elog("unexpected exception");
        call_on_connection_closed = true;
        exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", fc::except_str())));
    }
    
    if (exception_to_rethrow)
        throw *exception_to_rethrow;
}

void RpcClientMgr::send_message(TaskBase* rpc_msg) {
    uint32_t size_of_message_and_header = 0;
    uint32_t size_with_padding = 0;
    CompileTaskRpc rpc(*(CompileTask*)rpc_msg);
    Message m(rpc);
    //padding rpc data
    size_of_message_and_header = sizeof(thinkyoung::net::MessageHeader) + m.size;
    //pad the message we send to a multiple of 16 bytes
    size_with_padding = 16 * ((size_of_message_and_header + 15) / 16);
    std::unique_ptr<char[]> padded_message(new char[size_with_padding]);
    memcpy(padded_message.get(), (char*)&m, sizeof(MessageHeader));
    memcpy(padded_message.get() + sizeof(MessageHeader), m.data.data(), m.size);
    
    //send response
    try {
        _rpc_client_ptr->write(padded_message.get(), size_with_padding);
        _rpc_client_ptr->flush();
        
    } catch (fc::exception& er) {
        //TODO
    } catch (const std::exception& e) {
        //TODO
    } catch (...) {
        //TODO
    }
}

void RpcClientMgr::connect_to_server() {
	if (!_b_valid_flag)
		return;
	_rpc_client_ptr->connect_to(_end_point);
}

std::string TaskImplResult::get_result_string() {
    return nullptr;
}


std::string CompileTaskResult::get_result_string() {
    return nullptr;
}

void CompileTaskResult::get_rpc_message(){
}


CompileTaskResult::CompileTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    CompileTaskResult* compileTask_p = (CompileTaskResult*)task;
    //memcpy(this, task, sizeof(TaskBase));
    this->task_from = task->task_from;
    this->task_id = task->task_id;
    this->task_type = task->task_type;
    this->error_code = compileTask_p->error_code;
    this->error_msg = compileTask_p->error_msg;
    this->gpc_path_file = compileTask_p->gpc_path_file;
}


TaskImplResult* RpcClientMgr::parse_to_result(Message& msg) {
    TaskImplResult* result_p = NULL;
    
    switch (msg.msg_type) {
        case COMPILE_MESSAGE_TYPE: {
            CompileTaskResultRpc compile_task(msg.as<CompileTaskResultRpc>());
            result_p = new CompileTaskResult(&compile_task.data);
            break;
        }
        
        case CALL_MESSAGE_TYPE: {
            break;
        }
        
        case REGTISTER_MESSAGE_TYPE: {
            break;
        }
        
        case UPGRADE_MESSAGE_TYPE: {
            break;
        }
        
        case TRANSFER_MESSAGE_TYPE: {
            break;
        }
        
        case DESTROY_MESSAGE_TYPE: {
            break;
        }
        
        default: {
        }
    }
    
    return result_p;
}