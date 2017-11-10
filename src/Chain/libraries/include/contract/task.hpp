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
#define START_LOOP_TIME 10
#define TIME_INTERVAL 15

enum LUA_TASK_TYPE {
    COMPILE_TASK = 0,
    REGISTER_TASK,
    UPGRADE_TASK,
    CALL_TASK,
    TRANSFER_TASK,
    DESTROY_TASK,
	HELLO_MSG = 100,
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
	virtual ~TaskImplResult() {};
    
	virtual  void  process_result(RpcClientMgr* msg_p = nullptr);
	virtual  void  func2() {};
  public:
    uint64_t      error_code;
    std::string   error_msg;
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

FC_REFLECT_ENUM(LUA_TASK_TYPE,
                (COMPILE_TASK)
                (REGISTER_TASK)
                (UPGRADE_TASK)
                (CALL_TASK)
                (TRANSFER_TASK)
                (DESTROY_TASK)
				(HELLO_MSG)
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

FC_REFLECT_DERIVED(TaskImplResult, (TaskBase), (error_code)(error_msg))
FC_REFLECT_DERIVED(CompileTaskResult, (TaskImplResult), (gpc_path_file))
FC_REFLECT_DERIVED(RegisterTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(CallTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(TransferTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(UpgradeTaskResult, (TaskImplResult))
FC_REFLECT_DERIVED(DestroyTaskResult, (TaskImplResult))

#endif
