/*
author: pli
date: 2017.10.17
contract task entity
*/

#ifndef _TASK_H_
#define _TASK_H_
#include <blockchain/ContractEntry.hpp>
#include <glua/thinkyoung_lua_lib.h>
#include <net/Message.hpp>
#include <fc/filesystem.hpp>

#include <stdint.h>
#include <string>


using thinkyoung::blockchain::Code;
using thinkyoung::net::Message;

#define DISPATCH_TASK_TIMESPAN 1

enum LUA_TASK_TYPE {
    COMPILE_TASK = 0,
    REGISTER_TASK,
    UPGRADE_TASK,
    CALL_TASK,
    TRANSFER_TASK,
    DESTROY_TASK,
    TASK_COUNT
};

enum LUA_TASK_FROM {
    FROM_CLI = 0,
    FROM_RPC,
    FROM_COUNT
};

struct TaskBase {
    uint32_t task_id;     //a random value,CLI or achain launch a request with a task_id
    uint16_t task_type;   //here,change LUA_TASK_TYPE to uint32_t, fit FC name
    uint8_t task_from;    //LUA_TASK_FORM_CLI¡¢LUA_TASK_FORM_RPC
};

struct TaskImplResult : public TaskBase {
    TaskImplResult() {};
    
    void   init_task_base(TaskBase* task);
    
    virtual  std::string  get_result_string();
    virtual  Message get_rpc_message() = 0;
  public:
    uint64_t      error_code;
    std::string   error_msg;
};

struct CompileTaskResult : public TaskImplResult {
    CompileTaskResult() {}
    CompileTaskResult(TaskBase* task);
    
    std::string  get_result_string();
    Message get_rpc_message();
    
    std::string  gpc_path_file;
};

struct RegisterTaskResult : public TaskImplResult {
    RegisterTaskResult() {}
    RegisterTaskResult(TaskBase* task);
    
    std::string  get_result_string();
    Message get_rpc_message();
    
    //TODO
};

struct CallTaskResult : public TaskImplResult {
    CallTaskResult() {}
    CallTaskResult(TaskBase* task);
    
    std::string  get_result_string();
    Message get_rpc_message();
    //TODO
};

struct TransferTaskResult : public TaskImplResult {
    TransferTaskResult() {}
    TransferTaskResult(TaskBase* task);
    
    std::string  get_result_string();
    Message get_rpc_message();
    //TODO
};

struct UpgradeTaskResult : public TaskImplResult {
    UpgradeTaskResult() {}
    UpgradeTaskResult(TaskBase* task);
    
    std::string  get_result_string();
    Message get_rpc_message();
    //TODO
};

struct DestroyTaskResult : public TaskImplResult {
    DestroyTaskResult() {}
    DestroyTaskResult(TaskBase* task);
    
    std::string  get_result_string();
    Message get_rpc_message();
    //TODO
};


//task
struct CompileTask : public TaskBase {
    CompileTask() {
        task_type = COMPILE_TASK;
    };
    fc::path glua_path_file;
};

struct RegisterTask : public TaskBase {
    RegisterTask() {
        task_type = REGISTER_TASK;
    };
    
    std::string             gpc_code;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    //GluaStateValue          statevalue;
    //
    Code                    contract_code;
    
};

struct UpgradeTask : public TaskBase {
    UpgradeTask() {
        task_type = UPGRADE_TASK;
    };
    std::string             gpc_code;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    //GluaStateValue          statevalue;
    Code                    contract_code;
    
};

struct CallTask : public TaskBase {
    CallTask() {
        task_type = CALL_TASK;
    };
    std::string             gpc_code;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    //GluaStateValue          statevalue;
    Code                    contract_code;
};

struct TransferTask : public TaskBase {
    TransferTask() {
        task_type = TRANSFER_TASK;
    };
    std::string             gpc_code;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    //GluaStateValue          statevalue;
    Code                    contract_code;
};

struct DestroyTask : public TaskBase {
    DestroyTask() {
        task_type = DESTROY_TASK;
    };
    std::string            gpc_code;
    int                    num_limit;
    std::string            str_caller;
    std::string            str_caller_address;
    std::string            str_contract_address;
    std::string            str_contract_id;
    //GluaStateValue          statevalue;
    Code                   contract_code;
};

FC_REFLECT_ENUM(LUA_TASK_TYPE,
                (COMPILE_TASK)
                (REGISTER_TASK)
                (UPGRADE_TASK)
                (CALL_TASK)
                (TRANSFER_TASK)
                (DESTROY_TASK)
               )

FC_REFLECT(TaskBase, (task_id)(task_type)(task_from))
FC_REFLECT_DERIVED(CompileTask, (TaskBase), (glua_path_file))
FC_REFLECT_DERIVED(CallTask, (TaskBase), (gpc_code)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))
FC_REFLECT_DERIVED(RegisterTask, (TaskBase), (gpc_code)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(TransferTask, (TaskBase), (gpc_code)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(UpgradeTask, (TaskBase), (gpc_code)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(DestroyTask, (TaskBase), (gpc_code)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(TaskImplResult, (TaskBase), (error_code)(error_msg))
FC_REFLECT_DERIVED(CompileTaskResult, (TaskImplResult), (gpc_path_file))
FC_REFLECT_DERIVED(RegisterTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(CallTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(TransferTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(UpgradeTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(DestroyTaskResult, (TaskImplResult))

#endif
