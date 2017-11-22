#include <contract/task.hpp>
#include <contract/rpc_mgr.hpp>
#include <iostream>


TaskBase::TaskBase() {
    fc::time_point sec_now = fc::time_point::now();
    task_id = sec_now.sec_since_epoch();
}

CompileTaskResult::CompileTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    CompileTaskResult* compileTask_p = (CompileTaskResult*)task;
    this->task_from = task->task_from;
    this->task_id = task->task_id;
    this->task_type = task->task_type;
    this->error_code = compileTask_p->error_code;
    this->error_msg = compileTask_p->error_msg;
    this->gpc_path_file = compileTask_p->gpc_path_file;
}

RegisterTaskResult::RegisterTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
    RegisterTaskResult* registerTask_p = (RegisterTaskResult*)task;
    this->error_code = registerTask_p->error_code;
    this->error_msg = registerTask_p->error_msg;
    this->task_from = registerTask_p->task_from;
    this->task_id = registerTask_p->task_id;
    this->task_type = registerTask_p->task_type;
}

CallTaskResult::CallTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
    CallTaskResult* callTask_p = (CallTaskResult*)task;
    this->error_code = callTask_p->error_code;
    this->error_msg = callTask_p->error_msg;
    this->task_from = callTask_p->task_from;
    this->task_id = callTask_p->task_id;
    this->task_type = callTask_p->task_type;
}

TransferTaskResult::TransferTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
    TransferTaskResult* transferTask_p = (TransferTaskResult*)task;
    this->error_code = transferTask_p->error_code;
    this->error_msg = transferTask_p->error_msg;
    this->task_from = transferTask_p->task_from;
    this->task_id = transferTask_p->task_id;
    this->task_type = transferTask_p->task_type;
}

UpgradeTaskResult::UpgradeTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
    UpgradeTaskResult* upgradeTask_p = (UpgradeTaskResult*)task;
    this->error_code = upgradeTask_p->error_code;
    this->error_msg = upgradeTask_p->error_msg;
    this->task_from = upgradeTask_p->task_from;
    this->task_id = upgradeTask_p->task_id;
    this->task_type = upgradeTask_p->task_type;
}

DestroyTaskResult::DestroyTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
    DestroyTaskResult* destroyTask_p = (DestroyTaskResult*)task;
    this->error_code = destroyTask_p->error_code;
    this->error_msg = destroyTask_p->error_msg;
    this->task_from = destroyTask_p->task_from;
    this->task_id = destroyTask_p->task_id;
    this->task_type = destroyTask_p->task_type;
}

CompileScriptTaskResult::CompileScriptTaskResult(TaskBase* base) {
    if (!base) {
        return;
    }
    
    //TODO
    CompileScriptTaskResult* compileResult_p = (CompileScriptTaskResult*)base;
    this->error_code = compileResult_p->error_code;
    this->error_msg = compileResult_p->error_msg;
    this->task_from = compileResult_p->task_from;
    this->task_id = compileResult_p->task_id;
    this->task_type = compileResult_p->task_type;
}

HandleEventsTaskResult::HandleEventsTaskResult(TaskBase* base) {
    if (!base) {
        return;
    }
    
    //TODO
    HandleEventsTaskResult* handleEventTask_p = (HandleEventsTaskResult*)base;
    this->error_code = handleEventTask_p->error_code;
    this->error_msg = handleEventTask_p->error_msg;
    this->task_from = handleEventTask_p->task_from;
    this->task_id = handleEventTask_p->task_id;
    this->task_type = handleEventTask_p->task_type;
}

CallContractOfflineTaskResult::CallContractOfflineTaskResult(TaskBase* base) {
    if (!base) {
        return;
    }
    
    //TODO
    CallContractOfflineTaskResult* callContractOfflineResult_p = (CallContractOfflineTaskResult*)base;
    this->error_code = callContractOfflineResult_p->error_code;
    this->error_msg = callContractOfflineResult_p->error_msg;
    this->task_from = callContractOfflineResult_p->task_from;
    this->task_id = callContractOfflineResult_p->task_id;
    this->task_type = callContractOfflineResult_p->task_type;
}


LuaRequestTask::LuaRequestTask(TaskBase* task) {
    if (!task) {
        return;
    }
}

LuaRequestTaskResult::LuaRequestTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
}

void TaskImplResult::process_result(RpcClientMgr* msg_p) {
    if (msg_p) {
        msg_p->set_last_receive_time();
    }
    
    return;
}
void CompileTaskResult::process_result(RpcClientMgr* msg_p) {
    CompileTaskResult* task = (CompileTaskResult*)(this);
    std::cout << "recieve response......." << std::endl;
    std::cout << "task_from: " << (task->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
    std::cout << "task_id: " << task->task_id << std::endl;
    std::cout << "task_type: " << task->task_type << std::endl;
    std::cout << "error_code: " << task->error_code << std::endl;
    std::cout << "error_msg: " << task->error_msg << std::endl;
    return;
}

void RegisterTaskResult::process_result(RpcClientMgr* msg_p) {
    //TODO
    RegisterTaskResult* registerTask_p = (RegisterTaskResult*)this;
    std::cout << "register result......" << std::endl;
    std::cout << "task_from: " << (registerTask_p->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
    std::cout << "task_id: " << registerTask_p->task_id << std::endl;
    std::cout << "task_type: " << registerTask_p->task_type << std::endl;
    std::cout << "error_code: " << registerTask_p->error_code << std::endl;
    std::cout << "error_msg: " << registerTask_p->error_msg << std::endl;
    return;
}

void CallTaskResult::process_result(RpcClientMgr* msg_p) {
    //TODO
    CallTaskResult* callTask_p = (CallTaskResult*)this;
    std::cout << "register result......" << std::endl;
    std::cout << "task_from: " << (callTask_p->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
    std::cout << "task_id: " << callTask_p->task_id << std::endl;
    std::cout << "task_type: " << callTask_p->task_type << std::endl;
    std::cout << "error_code: " << callTask_p->error_code << std::endl;
    std::cout << "error_msg: " << callTask_p->error_msg << std::endl;
    return;
}

void TransferTaskResult::process_result(RpcClientMgr* msg_p) {
    //TODO
    TransferTaskResult* transferTask_p = (TransferTaskResult*)this;
    std::cout << "register result......" << std::endl;
    std::cout << "task_from: " << (transferTask_p->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
    std::cout << "task_id: " << transferTask_p->task_id << std::endl;
    std::cout << "task_type: " << transferTask_p->task_type << std::endl;
    std::cout << "error_code: " << transferTask_p->error_code << std::endl;
    std::cout << "error_msg: " << transferTask_p->error_msg << std::endl;
    return;
}

void UpgradeTaskResult::process_result(RpcClientMgr* msg_p) {
    //TODO
    UpgradeTaskResult* upgradeTask_p = (UpgradeTaskResult*)this;
    std::cout << "register result......" << std::endl;
    std::cout << "task_from: " << (upgradeTask_p->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
    std::cout << "task_id: " << upgradeTask_p->task_id << std::endl;
    std::cout << "task_type: " << upgradeTask_p->task_type << std::endl;
    std::cout << "error_code: " << upgradeTask_p->error_code << std::endl;
    std::cout << "error_msg: " << upgradeTask_p->error_msg << std::endl;
    return;
}

void DestroyTaskResult::process_result(RpcClientMgr* msg_p) {
    //TODO
    DestroyTaskResult* destroyTask_p = (DestroyTaskResult*)this;
    std::cout << "register result......" << std::endl;
    std::cout << "task_from: " << (destroyTask_p->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
    std::cout << "task_id: " << destroyTask_p->task_id << std::endl;
    std::cout << "task_type: " << destroyTask_p->task_type << std::endl;
    std::cout << "error_code: " << destroyTask_p->error_code << std::endl;
    std::cout << "error_msg: " << destroyTask_p->error_msg << std::endl;
    return;
}

void CompileScriptTaskResult::process_result(RpcClientMgr* msg_p) {
}

void HandleEventsTaskResult::process_result(RpcClientMgr* msp_p) {
}

void CallContractOfflineTaskResult::process_result(RpcClientMgr* msg_p) {
}
