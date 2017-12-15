#include <contract/task_dispatcher.hpp>
#include <contract/rpc_mgr.hpp>
#include <blockchain/GluaChainApi.hpp>
#include <blockchain/StorageTypes.hpp>

#include <lvm/lvm_interface.h>

#include <memory>

TaskDispatcher* TaskDispatcher::_p_lua_task_dispatcher = nullptr;

TaskDispatcher::TaskDispatcher() {
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

void TaskDispatcher::on_lua_request(TaskBase* task) {
    LuaRequestTask* plua_request = (LuaRequestTask*)task;
    RpcClientMgr* p_rpc_mgr = RpcClientMgr::get_rpc_mgr();
    thinkyoung::blockchain::TransactionEvaluationState* trx_evl_state;
    auto result = std::make_shared<LuaRequestTaskResult>();
    result->task_id = plua_request->task_id;
    result->method = plua_request->method;
    int par_size = plua_request->params.size();
    void* eval_state_ptr = (void*)plua_request->statevalue;
    trx_evl_state = (thinkyoung::blockchain::TransactionEvaluationState*)(eval_state_ptr);
    lvm::api::LvmInterface lvm_req(trx_evl_state);
    
    switch (result->method) {
        case GET_STORED_CONTRACT_INFO_BY_ADDRESS:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string address;
                address = fc::raw::unpack<std::string>(plua_request->params[0]);
                lvm_req.get_stored_contract_info_by_address(address);
            }
            
            break;
            
        case GET_CONTRACT_ADDRESS_BY_NAME:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_name;
                contract_name = fc::raw::unpack<std::string>(plua_request->params[0]);
                lvm_req.get_contract_address_by_name(contract_name);
            }
            
            break;
            
        case CHECK_CONTRACT_EXIST_BY_ADDRESS:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_address;
                contract_address = fc::raw::unpack<std::string>(plua_request->params[0]);
                lvm_req.check_contract_exist_by_address(contract_address);
            }
            
            break;
            
        case  CHECK_CONTRACT_EXIST:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_name;
                contract_name = fc::raw::unpack<std::string>(plua_request->params[0]);
                lvm_req.check_contract_exist(contract_name);
            }
            
            break;
            
        case OPEN_CONTRACT:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_name;
                contract_name = fc::raw::unpack<std::string>(plua_request->params[0]);
                lvm_req.open_contract(contract_name);
            }
            
            break;
            
        case OPEN_CONTRACT_BY_ADDRESS:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_address;
                contract_address = fc::raw::unpack<std::string>(plua_request->params[0]);
                lvm_req.open_contract_by_address(contract_address);
            }
            
            break;
            
        case GET_STORAGE_VALUE_FROM_THINKYOUNG:
            if (par_size < 2) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_name;
                std::string storage_name;
                contract_name = fc::raw::unpack<std::string>(plua_request->params[0]);
                storage_name = fc::raw::unpack<std::string>(plua_request->params[1]);
                lvm_req.get_storage_value_from_thinkyoung_by_address(contract_name, storage_name);
            }
            
            break;
            
        case GET_CONTRACT_BALANCE_AMOUNT:
            if (par_size < 2) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_address;
                std::string asset_sym;
                contract_address = fc::raw::unpack<std::string>(plua_request->params[0]);
                asset_sym = fc::raw::unpack<std::string>(plua_request->params[1]);
                lvm_req.get_contract_balance_amount(contract_address, asset_sym);
            }
            
            break;
            
        case GET_TRANSACTION_FEE:
            lvm_req.get_transaction_fee();
            break;
            
        case GET_CHAIN_NOW:
            lvm_req.get_chain_now();
            break;
            
        case GET_CHAIN_RANDOM:
            lvm_req.get_chain_random();
            break;
            
        case GET_TRANSACTION_ID:
            lvm_req.get_transaction_id();
            break;
            
        case GET_HEADER_BLOCK_NUM:
            lvm_req.get_header_block_num();
            break;
            
        case WAIT_FOR_FUTURE_RANDOM:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                int next;
                next = fc::raw::unpack<int>(plua_request->params[0]);
                lvm_req.wait_for_future_random(next);
            }
            
            break;
            
        case GET_WAITED:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                uint32_t next;
                next = fc::raw::unpack<uint32_t>(plua_request->params[0]);
                lvm_req.get_waited(next);
            }
            
            break;
            
        case COMMIT_STORAGE_CHANGES_TO_THINKYOUNG:
            if (par_size < 1) {
                result->err_num = -1;
                break;
            }
            
            {
                lvm::api::AllStorageDataChange  contract_change_item;
                contract_change_item = fc::raw::unpack<lvm::api::AllStorageDataChange>(plua_request->params[0]);
                lvm_req.commit_storage_changes_to_thinkyoung(contract_change_item);
            }
            
            break;
            
        case TRANSFER_FROM_CONTRACT_TO_ADDRESS:
            if (par_size < 4) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_addr, to_address, asset_type;
                int64_t amount;
                contract_addr = fc::raw::unpack<std::string>(plua_request->params[0]);
                to_address = fc::raw::unpack<std::string>(plua_request->params[1]);
                asset_type = fc::raw::unpack<std::string>(plua_request->params[2]);
                amount = fc::raw::unpack<int64_t>(plua_request->params[3]);
                lvm_req.transfer_from_contract_to_address(contract_addr, to_address, asset_type, amount);
            }
            
            break;
            
        case TRANSFER_FROM_CONTRACT_TO_PUBLIC_ACCOUNT:
            if (par_size < 4) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_addr, to_address_name, asset_type;
                int64_t amount;
                contract_addr = fc::raw::unpack<std::string>(plua_request->params[0]);
                to_address_name = fc::raw::unpack<std::string>(plua_request->params[1]);
                asset_type = fc::raw::unpack<std::string>(plua_request->params[2]);
                amount = fc::raw::unpack<int64_t>(plua_request->params[3]);
                lvm_req.transfer_from_contract_to_public_account(contract_addr, to_address_name, asset_type, amount);
            }
            
            break;
            
        case EMIT:
            if (par_size < 3) {
                result->err_num = -1;
                break;
            }
            
            {
                std::string contract_id, event_name, event_param;
                contract_id = fc::raw::unpack<std::string>(plua_request->params[0]);
                event_name = fc::raw::unpack<std::string>(plua_request->params[1]);
                event_param = fc::raw::unpack<std::string>(plua_request->params[2]);
                lvm_req.emit(contract_id, event_name, event_param);
            }
            
            break;
    }
    
    result->params = lvm_req.result;
    
    if (result->err_num != -1) {
        result->err_num = lvm_req.err_num;
    }
    
    p_rpc_mgr->post_message(result.get(), nullptr);
}

TaskImplResult* TaskDispatcher::exec_lua_task(TaskBase* task) {
    RpcClientMgr* p_rpc_mgr = RpcClientMgr::get_rpc_mgr();
    _exec_lua_task_ptr = fc::promise<void*>::ptr(new fc::promise<void*>("exec_lua_task_promise"));
    p_rpc_mgr->post_message(task, _exec_lua_task_ptr);
    TaskImplResult* impl_result = (TaskImplResult*)(void *)_exec_lua_task_ptr->wait();
    _exec_lua_task_ptr.reset();
    return impl_result;
}
