#include <contract/task_dispatcher.hpp>
#include <contract/rpc_mgr.hpp>
#include <blockchain/GluaChainApi.hpp>

TaskDispatcher* TaskDispatcher::_p_lua_task_dispatcher = nullptr;

TaskDispatcher::TaskDispatcher() {
    _on_lua_request_promise_ptr = fc::promise<void*>::ptr(new fc::promise<void*>("on_lua_request_promise"));
    _exec_lua_task_ptr = fc::promise<void*>::ptr(new fc::promise<void*>("exec_lua_task_promise"));
    
    if (thinkyoung::lua::api::global_glua_chain_api == nullptr) {
        thinkyoung::lua::api::global_glua_chain_api = new thinkyoung::lua::api::GluaChainApi;
    }
}

TaskDispatcher::~TaskDispatcher() {
}

TaskDispatcher* TaskDispatcher::get_lua_task_dispatcher() {
    if (!TaskDispatcher::_p_lua_task_dispatcher) {
        TaskDispatcher::_p_lua_task_dispatcher = new TaskDispatcher;
    }
    
    return TaskDispatcher::_p_lua_task_dispatcher;
}

void TaskDispatcher::delete_lua_task_dispatcher() {
    if (!TaskDispatcher::_p_lua_task_dispatcher) {
        delete TaskDispatcher::_p_lua_task_dispatcher;
        TaskDispatcher::_p_lua_task_dispatcher = nullptr;
    }
}

void TaskDispatcher::on_lua_request(LuaRequestTask& task) {
    RpcClientMgr* p_rpc_mgr = RpcClientMgr::get_rpc_mgr();
    TaskBase* task_base = nullptr;
    
    switch (task.method) {
        case GET_STORED_CONTRACT_INFO_BY_ADDRESS:
            break;
            
        case GET_CONTRACT_ADDRESS_BY_NAME:
            break;
            
        case CHECK_CONTRACT_EXIST_BY_ADDRESS:
            break;
            
        case  CHECK_CONTRACT_EXIST:
            break;
            
        case OPEN_CONTRACT:
            break;
            
        case OPEN_CONTRACT_BY_ADDRESS:
            break;
            
        case GET_STORAGE_VALUE_FROM_THINKYOUNG:
            break;
            
        case GET_CONTRACT_BALANCE_AMOUNT:
            break;
            
        case GET_TRANSACTION_FEE:
            break;
            
        case GET_CHAIN_NOW:
            break;
            
        case GET_CHAIN_RANDOM:
            break;
            
        case GET_TRANSACTION_ID:
            break;
            
        case GET_HEADER_BLOCK_NUM:
            break;
            
        case WAIT_FOR_FUTURE_RANDOM:
            break;
            
        case GET_WAITED:
            break;
            
        case COMMIT_STORAGE_CHANGES_TO_THINKYOUNG:
            break;
            
        case TRANSFER_FROM_CONTRACT_TO_ADDRESS:
            break;
            
        case TRANSFER_FROM_CONTRACT_TO_PUBLIC_ACCOUNT:
            break;
            
        case EMIT:
            break;
    }
    
    p_rpc_mgr->send_message(task_base);
    _on_lua_request_promise_ptr->wait();
}

void TaskDispatcher::exec_lua_task(TaskBase* task) {
    RpcClientMgr* p_rpc_mgr = RpcClientMgr::get_rpc_mgr();
    p_rpc_mgr->send_message(task);
    _exec_lua_task_ptr->wait();
}
