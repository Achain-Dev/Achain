/*
author: pli
date: 2017.10.17
contract task entity
*/

#ifndef _TASK_H_
#define _TASK_H_
#include <blockchain/ContractEntry.hpp>
#include <glua/thinkyoung_lua_lib.h>
#include <fc/filesystem.hpp>

#include <stdint.h>
#include <string>


using thinkyoung::blockchain::Code;

class RpcClientMgr;

#define DISPATCH_TASK_TIMESPAN 1
#define RECONNECT_TIMES 5
#define START_LOOP_TIME 30
#define TIME_INTERVAL 50

enum LUA_TASK_TYPE {
    COMPILE_TASK = 0,
    COMPILE_TASK_RESULT,
    COMPILE_SCRIPT_TASK,
    COMPILE_SCRIPT_RESULT,
    REGISTER_TASK,
    REGISTER_TASK_RESULT,
    UPGRADE_TASK,
    UPGRADE_TASK_RESULT,
    CALL_TASK,
    CALL_TASK_RESULT,
    CALL_OFFLINE_TASK,
    CALL_OFFLINE_TASK_RESULT,
    TRANSFER_TASK,
    TRANSFER_TASK_RESULT,
    DESTROY_TASK,
    DESTROY_TASK_RESULT,
    HANDLE_EVENTS_TASK,
    HANDLE_EVENTS_TASK_RESULT,
    LUA_REQUEST_TASK,
    LUA_REQUEST_RESULT_TASK,
    HELLO_MSG,
    TASK_COUNT
};

enum LUA_TASK_FROM {
    FROM_CLI = 0,
    FROM_RPC,
    FROM_LUA_TO_CHAIN,
    FROM_COUNT
};

enum LUA_REQUEST_METHOD {
    GET_STORED_CONTRACT_INFO_BY_ADDRESS = 0,
    GET_CONTRACT_ADDRESS_BY_NAME,
    CHECK_CONTRACT_EXIST_BY_ADDRESS,
    CHECK_CONTRACT_EXIST,
    OPEN_CONTRACT,
    OPEN_CONTRACT_BY_ADDRESS,
    GET_STORAGE_VALUE_FROM_THINKYOUNG,
    GET_CONTRACT_BALANCE_AMOUNT,
    GET_TRANSACTION_FEE,
    GET_CHAIN_NOW,
    GET_CHAIN_RANDOM,
    GET_TRANSACTION_ID,
    GET_HEADER_BLOCK_NUM,
    WAIT_FOR_FUTURE_RANDOM,
    GET_WAITED,
    COMMIT_STORAGE_CHANGES_TO_THINKYOUNG,
    TRANSFER_FROM_CONTRACT_TO_ADDRESS,
    TRANSFER_FROM_CONTRACT_TO_PUBLIC_ACCOUNT,
    EMIT,
    COUNT
};

struct TaskBase {
    TaskBase();
    uint32_t task_id;     //a random value,CLI or achain launch a request with a task_id
    LUA_TASK_TYPE task_type;   //LUA_TASK_TYPE
    LUA_TASK_FROM task_from;    //LUA_TASK_FORM_CLI/LUA_TASK_FORM_RPC
};

struct TaskImplResult : public TaskBase {
    TaskImplResult() {};
    virtual ~TaskImplResult() {};
    
    virtual  void  process_result(RpcClientMgr* msg_p = nullptr);
    virtual  void  func2() {};
  public:
    uint64_t      error_code;
    std::string   error_msg;
    std::string   json_string;
};

//hello msg
typedef struct TaskImplResult HelloMsgResult;

struct CompileTaskResult : public TaskImplResult {
    CompileTaskResult() {}
    CompileTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
    
    std::string  gpc_path_file;
};

struct RegisterTaskResult : public TaskImplResult {
    RegisterTaskResult() {}
    RegisterTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
    //TODO
};

struct CallTaskResult : public TaskImplResult {
    CallTaskResult() {}
    CallTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
    //TODO
};

struct TransferTaskResult : public TaskImplResult {
    TransferTaskResult() {}
    TransferTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
    //TODO
};

struct UpgradeTaskResult : public TaskImplResult {
    UpgradeTaskResult() {}
    UpgradeTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
    //TODO
};

struct DestroyTaskResult : public TaskImplResult {
    DestroyTaskResult() {}
    DestroyTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
    //TODO
};

struct CompileScriptTaskResult : TaskImplResult {
    CompileScriptTaskResult() {}
    CompileScriptTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
};

struct HandleEventsTaskResult : TaskImplResult {
    HandleEventsTaskResult() {}
    HandleEventsTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
};

struct CallContractOfflineTaskResult : TaskImplResult {
    CallContractOfflineTaskResult() {}
    CallContractOfflineTaskResult(TaskBase* task);
    
    void  process_result(RpcClientMgr* msg_p = nullptr);
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
    intptr_t                statevalue;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    Code                    contract_code;
};

struct UpgradeTask : public TaskBase {
    UpgradeTask() {
        task_type = UPGRADE_TASK;
    };
    intptr_t                statevalue;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    Code                    contract_code;
};

struct CallTask : public TaskBase {
    CallTask() {
        task_type = CALL_TASK;
    };
    intptr_t                statevalue;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    std::string             str_method;
    std::string             str_args;
    Code                    contract_code;
};

struct TransferTask : public TaskBase {
    TransferTask() {
        task_type = TRANSFER_TASK;
    };
    intptr_t                statevalue;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    std::string             str_args;
    Code                    contract_code;
};

struct DestroyTask : public TaskBase {
    DestroyTask() {
        task_type = DESTROY_TASK;
    };
    intptr_t               statevalue;
    int                    num_limit;
    std::string            str_caller;
    std::string            str_caller_address;
    std::string            str_contract_address;
    std::string            str_contract_id;
    Code                   contract_code;
};

struct CompileScriptTask : public TaskBase {
    CompileScriptTask() {
        task_type = COMPILE_SCRIPT_TASK;
    };
    
    bool use_contract;
    std::string path_file_name;
    bool use_type_check;
};

struct HandleEventsTask : public TaskBase {
    HandleEventsTask() {
        task_type = HANDLE_EVENTS_TASK;
    }
    
    std::string contract_id;
    std::string event_type;
    std::string event_param;
    bool is_truncated;
    Code script_code;
};

struct CallContractOfflineTask : public TaskBase {
    CallContractOfflineTask() {
        task_type = CALL_OFFLINE_TASK;
    }
    
    intptr_t                statevalue;
    int                     num_limit;
    std::string             str_caller;
    std::string             str_caller_address;
    std::string             str_contract_address;
    std::string             str_contract_id;
    std::string             str_method;
    std::string             str_args;
    Code                    contract_code;
};

//special task
struct LuaRequestTask : public TaskBase {
    LuaRequestTask() {
        task_type = LUA_REQUEST_TASK;
        task_from = FROM_LUA_TO_CHAIN;
    }
    
    LuaRequestTask(TaskBase* task);
    
    LUA_REQUEST_METHOD     method;
    intptr_t statevalue;
    std::vector<std::vector<char>> params;
};

struct LuaRequestTaskResult : public TaskBase {
    LuaRequestTaskResult() {
        task_type = LUA_REQUEST_RESULT_TASK;
        task_from = FROM_RPC;
    }
    
    LuaRequestTaskResult(TaskBase* task);
    
    LUA_REQUEST_METHOD     method;
    std::vector<char> ret;
    std::vector<std::vector<char>> params;
    int err_num;
};

FC_REFLECT_TYPENAME(LUA_TASK_TYPE)
FC_REFLECT_ENUM(LUA_TASK_TYPE,
                (COMPILE_TASK)
                (COMPILE_TASK_RESULT)
                (COMPILE_SCRIPT_TASK)
                (COMPILE_SCRIPT_RESULT)
                (REGISTER_TASK)
                (REGISTER_TASK_RESULT)
                (UPGRADE_TASK)
                (UPGRADE_TASK_RESULT)
                (CALL_TASK)
                (CALL_TASK_RESULT)
                (CALL_OFFLINE_TASK)
                (CALL_OFFLINE_TASK_RESULT)
                (TRANSFER_TASK)
                (TRANSFER_TASK_RESULT)
                (DESTROY_TASK)
                (DESTROY_TASK_RESULT)
                (HANDLE_EVENTS_TASK)
                (HANDLE_EVENTS_TASK_RESULT)
                (LUA_REQUEST_TASK)
                (LUA_REQUEST_RESULT_TASK)
                (HELLO_MSG)
               )

FC_REFLECT_TYPENAME(LUA_TASK_FROM)
FC_REFLECT_ENUM(LUA_TASK_FROM,
                (FROM_CLI)
                (FROM_RPC)
                (FROM_LUA_TO_CHAIN)
               )
FC_REFLECT_TYPENAME(LUA_REQUEST_METHOD)
FC_REFLECT_ENUM(LUA_REQUEST_METHOD,
                (GET_STORED_CONTRACT_INFO_BY_ADDRESS)
                (GET_CONTRACT_ADDRESS_BY_NAME)
                (CHECK_CONTRACT_EXIST_BY_ADDRESS)
                (CHECK_CONTRACT_EXIST)
                (OPEN_CONTRACT)
                (OPEN_CONTRACT_BY_ADDRESS)
                (GET_STORAGE_VALUE_FROM_THINKYOUNG)
                (GET_CONTRACT_BALANCE_AMOUNT)
                (GET_TRANSACTION_FEE)
                (GET_CHAIN_NOW)
                (GET_CHAIN_RANDOM)
                (GET_TRANSACTION_ID)
                (GET_HEADER_BLOCK_NUM)
                (WAIT_FOR_FUTURE_RANDOM)
                (GET_WAITED)
                (COMMIT_STORAGE_CHANGES_TO_THINKYOUNG)
                (TRANSFER_FROM_CONTRACT_TO_ADDRESS)
                (TRANSFER_FROM_CONTRACT_TO_PUBLIC_ACCOUNT)
                (EMIT)
               )

FC_REFLECT(TaskBase, (task_id)(task_type)(task_from))
FC_REFLECT_DERIVED(CompileTask, (TaskBase), (glua_path_file))

FC_REFLECT_DERIVED(CallTask, (TaskBase), (statevalue)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(str_method)(str_args)(contract_code))

FC_REFLECT_DERIVED(RegisterTask, (TaskBase), (gpc_code)(statevalue)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(TransferTask, (TaskBase), (statevalue)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(str_args)(contract_code))

FC_REFLECT_DERIVED(UpgradeTask, (TaskBase), (statevalue)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(DestroyTask, (TaskBase), (statevalue)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(contract_code))

FC_REFLECT_DERIVED(CompileScriptTask, (TaskBase), (path_file_name)(use_contract)
                   (use_type_check))
FC_REFLECT_DERIVED(HandleEventsTask, (TaskBase), (contract_id)(event_type)
                   (event_param)(is_truncated)(script_code))
FC_REFLECT_DERIVED(CallContractOfflineTask, (TaskBase), (statevalue)(num_limit)
                   (str_caller)(str_caller_address)(str_contract_address)
                   (str_contract_id)(str_method)(str_args)(contract_code))

FC_REFLECT_DERIVED(LuaRequestTask, (TaskBase), (method)(params))
FC_REFLECT_DERIVED(LuaRequestTaskResult, (TaskBase), (method)(result))
FC_REFLECT_DERIVED(TaskImplResult, (TaskBase), (error_code)(error_msg)(json_string))
FC_REFLECT_DERIVED(CompileTaskResult, (TaskImplResult), (gpc_path_file))
FC_REFLECT_DERIVED(RegisterTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(CallTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(TransferTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(UpgradeTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(DestroyTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(CompileScriptTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(HandleEventsTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(CallContractOfflineTaskResult, (TaskImplResult))

#endif
