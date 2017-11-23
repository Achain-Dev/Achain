#include <contract/task.hpp>
#include <contract/rpc_mgr.hpp>
#include <iostream>


TaskBase::TaskBase(){
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
}

CallTaskResult::CallTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
}

TransferTaskResult::TransferTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
}

UpgradeTaskResult::UpgradeTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
}

DestroyTaskResult::DestroyTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    //TODO
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