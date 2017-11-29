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
    
    if (this != compileTask_p) {
        task_from = task->task_from;
        task_id = task->task_id;
        task_type = task->task_type;
        error_code = compileTask_p->error_code;
        error_msg = compileTask_p->error_msg;
        gpc_path_file = compileTask_p->gpc_path_file;
    }
}

RegisterTaskResult::RegisterTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    RegisterTaskResult* registerTask_p = (RegisterTaskResult*)task;
    
    if (this != registerTask_p) {
        error_code = registerTask_p->error_code;
        error_msg = registerTask_p->error_msg;
        task_from = registerTask_p->task_from;
        task_id = registerTask_p->task_id;
        task_type = registerTask_p->task_type;
        execute_count = registerTask_p->execute_count;
    }
}

CallTaskResult::CallTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    CallTaskResult* callTask_p = (CallTaskResult*)task;
    
    if (this != callTask_p) {
        error_code = callTask_p->error_code;
        error_msg = callTask_p->error_msg;
        task_from = callTask_p->task_from;
        task_id = callTask_p->task_id;
        task_type = callTask_p->task_type;
    }
}

TransferTaskResult::TransferTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    TransferTaskResult* transferTask_p = (TransferTaskResult*)task;
    
    if (this != transferTask_p) {
        error_code = transferTask_p->error_code;
        error_msg = transferTask_p->error_msg;
        task_from = transferTask_p->task_from;
        task_id = transferTask_p->task_id;
        task_type = transferTask_p->task_type;
    }
}

UpgradeTaskResult::UpgradeTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    UpgradeTaskResult* upgradeTask_p = (UpgradeTaskResult*)task;
    
    if (this != upgradeTask_p) {
        error_code = upgradeTask_p->error_code;
        error_msg = upgradeTask_p->error_msg;
        task_from = upgradeTask_p->task_from;
        task_id = upgradeTask_p->task_id;
        task_type = upgradeTask_p->task_type;
    }
}

DestroyTaskResult::DestroyTaskResult(TaskBase* task) {
    if (!task) {
        return;
    }
    
    DestroyTaskResult* destroyTask_p = (DestroyTaskResult*)task;
    
    if (this != destroyTask_p) {
        error_code = destroyTask_p->error_code;
        error_msg = destroyTask_p->error_msg;
        task_from = destroyTask_p->task_from;
        task_id = destroyTask_p->task_id;
        task_type = destroyTask_p->task_type;
    }
}

CompileScriptTaskResult::CompileScriptTaskResult(TaskBase* base) {
    if (!base) {
        return;
    }
    
    CompileScriptTaskResult* compileResult_p = (CompileScriptTaskResult*)base;
    
    if (this != compileResult_p) {
        error_code = compileResult_p->error_code;
        error_msg = compileResult_p->error_msg;
        task_from = compileResult_p->task_from;
        task_id = compileResult_p->task_id;
        task_type = compileResult_p->task_type;
        script_path_file = compileResult_p->script_path_file;
    }
}

HandleEventsTaskResult::HandleEventsTaskResult(TaskBase* base) {
    if (!base) {
        return;
    }
    
    HandleEventsTaskResult* handleEventTask_p = (HandleEventsTaskResult*)base;
    
    if (this != handleEventTask_p) {
        error_code = handleEventTask_p->error_code;
        error_msg = handleEventTask_p->error_msg;
        task_from = handleEventTask_p->task_from;
        task_id = handleEventTask_p->task_id;
        task_type = handleEventTask_p->task_type;
    }
}

CallContractOfflineTaskResult::CallContractOfflineTaskResult(TaskBase* base) {
    if (!base) {
        return;
    }
    
    CallContractOfflineTaskResult* callContractOfflineResult_p = (CallContractOfflineTaskResult*)base;
    
    if (this != callContractOfflineResult_p) {
        error_code = callContractOfflineResult_p->error_code;
        error_msg = callContractOfflineResult_p->error_msg;
        task_from = callContractOfflineResult_p->task_from;
        task_id = callContractOfflineResult_p->task_id;
        task_type = callContractOfflineResult_p->task_type;
    }
}

void TaskImplResult::process_result(RpcClientMgr* msg_p) {
    if (msg_p) {
        msg_p->set_last_receive_time();
    }
    
    return;
}