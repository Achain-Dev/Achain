#include <net/Config.hpp>
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

//hello msg
const LuaRpcMessageTypeEnum HelloMsgResultRpc::type = LuaRpcMessageTypeEnum::HELLO_MESSAGE_TYPE;

RpcClientMgr* RpcClientMgr::_s_rpc_mgr_ptr = nullptr;

RpcClientMgr::RpcClientMgr(Client* client)
    :_b_valid_flag(false),
     _rpc_client_ptr(std::make_shared<thinkyoung::net::StcpSocket>()){
    _client_ptr = client;
	_last_hello_message_received_time = fc::time_point::min();
}

RpcClientMgr::~RpcClientMgr() {
}



void RpcClientMgr::init(){

	_receive_msg_thread_ptr = std::make_shared<fc::thread>("server");
	_task_proc_thread_ptr = std::make_shared<fc::thread>("task_proc");

	//_task_handler_ptr = new TaskHandler();

}
void RpcClientMgr::start() {

	connect_to_server(); 
	
    _receive_msg_thread_ptr->async([&]() {
        this->read_loop();
    });
}

void RpcClientMgr::start_loop() {
	uint64_t interval = 0;
	interval = (fc::time_point::now() - _last_hello_message_received_time).to_seconds();

	//if the interval bigger than TIME_INTERVAL, the lvm maybe error, then restart the lvm
	if (interval > TIME_INTERVAL)
	{
		//TODO
		//start lvm
		_rpc_client_ptr->close();
		try{ 
			start(); 
		}
		catch (fc::exception& e)
		{
			//TODO
		}
	}

	fc::schedule([this](){ start_loop(); },
		fc::time_point::now() + fc::seconds(START_LOOP_TIME),
		"start_loop");

}

void RpcClientMgr::task_imp() {
    _task_proc_thread_ptr->schedule([this]() {
        process_task(this);
    },
    fc::time_point::now() + fc::seconds(DISPATCH_TASK_TIMESPAN),
    "process the task");
}

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
    bool reconnect = false;
    
    try {
        Message m;
        
        while (true) {
            uint64_t bytes_received = 0;
            uint64_t remaining_bytes_with_padding = 0;
            char buffer[BUFFER_SIZE];
            /*first: read msgHead, get data.size*/
            _rpc_client_ptr->read(buffer, BUFFER_SIZE);
            /*convert to MessageHeader*/
            memcpy((char*)&m, buffer, sizeof(MessageHeader));
			FC_ASSERT(m.size <= MAX_MESSAGE_SIZE, "", ("m.size", m.size)("MAX_MESSAGE_SIZE", MAX_MESSAGE_SIZE));
            /*remaining len of byte to read from socket*/
            remaining_bytes_with_padding = 16 * ((m.size - LEFTOVER + 15) / 16);
            m.data.resize(LEFTOVER + remaining_bytes_with_padding);
            std::copy(buffer + sizeof(MessageHeader), buffer + sizeof(buffer), m.data.begin());
            
            if (remaining_bytes_with_padding) {
                _rpc_client_ptr->read(&m.data[LEFTOVER], remaining_bytes_with_padding);
            }
            
            m.data.resize(m.size);
            //get task result pointer
            result_p = parse_to_result(m);
            insert_task(result_p);
        }
        
    } catch (const fc::eof_exception& e) {
        wlog("disconnected ${e}", ("e", e.to_detail_string()));
		reconnect = true;
        
    } catch (const fc::exception& e) {
        elog("disconnected ${er}", ("er", e.to_detail_string()));
		reconnect = true;
        
    } catch (const std::exception& e) {
        elog("disconnected ${er}", ("er", e.what()));
		reconnect = true;
        
    } catch (...) {
        elog("unexpected exception");
		reconnect = true;
    }
    
	if (reconnect)
	{
		reconnect_to_server();
	}
}

void RpcClientMgr::send_message(TaskBase* rpc_msg) {
    uint32_t size_of_message_and_header = 0;
    uint32_t size_with_padding = 0;

	Message m(generate_message(rpc_msg));
    //padding rpc data
    size_of_message_and_header = sizeof(MessageHeader) + m.size;
    //pad the message we send to a multiple of 16 bytes
    size_with_padding = 16 * ((size_of_message_and_header + 15) / 16);
    std::unique_ptr<char[]> padded_message(new char[size_with_padding]);
    memcpy(padded_message.get(), (char*)&m, sizeof(MessageHeader));
    memcpy(padded_message.get() + sizeof(MessageHeader), m.data.data(), m.size);
    
    //send response
    try {
        _rpc_client_ptr->write(padded_message.get(), size_with_padding);
        _rpc_client_ptr->flush();
        
    } catch (...) {
		_rpc_client_ptr->close();
    }
}

void RpcClientMgr::connect_to_server() {
	if (!_b_valid_flag)
		return;
	_rpc_client_ptr->connect_to(_end_point);
}

void RpcClientMgr::set_last_receive_time(){
	_last_hello_message_received_time = fc::time_point::now();
}


void RpcClientMgr::reconnect_to_server() {
	int times = 0;
	_rpc_client_ptr->close();

	while (times < RECONNECT_TIMES) {
		try {
			start();
			break;
		}
		catch (fc::exception& e) {
			times++;
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

//get rpc_mgr
RpcClientMgr* RpcClientMgr::get_rpc_mgr(){
	if (!_s_rpc_mgr_ptr){
		_s_rpc_mgr_ptr = new RpcClientMgr();
	}

	return _s_rpc_mgr_ptr;
}

void RpcClientMgr::execute_task(TaskBase* task_p){

	send_message(task_p);
}

Message RpcClientMgr::generate_message(TaskBase* task_p){

	FC_ASSERT(task_p != NULL);

	switch (task_p->task_type)
	{
		case COMPILE_TASK:
		{
			CompileTaskRpc task_rpc(*(CompileTask*)task_p);
			task_rpc.data.task_from = FROM_RPC;

			Message msg(task_rpc);

			return msg;
		}

		case REGISTER_TASK:
		{
			RegisterTaskRpc task_rpc(*(RegisterTask*)task_p);
			task_rpc.data.task_from = FROM_RPC;

			Message msg(task_rpc);

			return msg;
		}

		case UPGRADE_TASK:
		{
			UpgradeTaskRpc task_rpc(*(UpgradeTask*)task_p);
			task_rpc.data.task_from = FROM_RPC;

			Message msg(task_rpc);

			return msg;
		}

		case CALL_TASK:
		{
			CallTaskRpc task_rpc(*(CallTask*)task_p);
			task_rpc.data.task_from = FROM_RPC;

			Message msg(task_rpc);

			return msg;
		}

		case TRANSFER_TASK:
		{
			TransferTaskRpc task_rpc(*(TransferTask*)task_p);
			task_rpc.data.task_from = FROM_RPC;

			Message msg(task_rpc);

			return msg;
		}

		case DESTROY_TASK:
		{
			DestroyTaskRpc task_rpc(*(DestroyTask*)task_p);
			task_rpc.data.task_from = FROM_RPC;

			Message msg(task_rpc);

			return msg;
		}

		default:
		{
			return Message();
		}
	}
}