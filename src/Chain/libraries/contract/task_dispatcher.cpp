#include <contract/task_dispatcher.hpp>
#include <contract/rpc_mgr.hpp>
#include <blockchain/GluaChainApi.hpp>

#include <lvm/lvm_interface.h>

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

#define EVL_STATE_NIL (30000)
#define PARAMS_INVALID (31000)

void TaskDispatcher::on_lua_request(LuaRequestTask& task) {
    RpcClientMgr* p_rpc_mgr = RpcClientMgr::get_rpc_mgr();
    thinkyoung::blockchain::TransactionEvaluationState* trx_evl_state;
    //TODO make_shared
    // std::shared_ptr<LuaRequestTaskResult> result = std::make_shared<LuaRequestTaskResult>();
    LuaRequestTaskResult* result = new LuaRequestTaskResult;
    result->method = task.method;
    int par_size = task.params.size();
    
    if (par_size < 1) {
        result->err_num = EVL_STATE_NIL;
    }
    
    trx_evl_state = (thinkyoung::blockchain::TransactionEvaluationState*)(task.statevalue);
    lvm::api::LvmInterface lvm_req(trx_evl_state);
    
    switch (task.method) {
        case GET_STORED_CONTRACT_INFO_BY_ADDRESS:
            if (par_size < 2) {
                result->ret = fc::raw::pack<bool>(false));
                break;
            }
            
            {
                std::string address;
                address = fc::raw::unpack<std::string>(task.params[1]);
                lvm_req.get_stored_contract_info_by_address(address);
            }
            
            break;
            
        case GET_CONTRACT_ADDRESS_BY_NAME:
            if (par_size < 2) {
                result->ret_params.push_back(fc::raw::pack<bool>(false));
                break;
            }
            
            {
                std::string contract_name;
                contract_name = fc::raw::unpack<std::string>(task.params[1]);
                lvm_req.get_contract_address_by_name(contract_name);
            }
            
            break;
            
        case CHECK_CONTRACT_EXIST_BY_ADDRESS:
            if (par_size < 2) {
                result->ret_params.push_back(fc::raw::pack<bool>(false));
                break;
            }
            
            {
                std::string contract_address;
                contract_address = fc::raw::unpack<std::string>(task.params[1]);
                lvm_req.check_contract_exist_by_address(contract_address);
            }
            
            break;
            
        case  CHECK_CONTRACT_EXIST:
            if (par_size < 2) {
                result->ret_params.push_back(fc::raw::pack<bool>(false));
                break;
            }
            
            {
                std::string contract_name;
                contract_name = fc::raw::unpack<std::string>(task.params[1]);
                lvm_req.check_contract_exist(contract_name);
            }
            
            break;
            
        case OPEN_CONTRACT:
            if (par_size < 2) {
                result->ret_params.push_back(fc::raw::pack<bool>(false));
                break;
            }
            
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
    
DONE:
    p_rpc_mgr->send_message(result);
    _on_lua_request_promise_ptr->wait();
}

void TaskDispatcher::exec_lua_task(TaskBase* task) {
    RpcClientMgr* p_rpc_mgr = RpcClientMgr::get_rpc_mgr();
    p_rpc_mgr->send_message(task);
    _exec_lua_task_ptr->wait();
}
