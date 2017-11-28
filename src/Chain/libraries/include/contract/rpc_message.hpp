#ifndef _RPC_MESSAGE_H_
#define _RPC_MESSAGE_H_

#include <stdint.h>
#include <string>

#include <fc/filesystem.hpp>
#include <fc/reflect/variant.hpp>
#include <contract/task.hpp>

enum LuaRpcMessageTypeEnum {
    COMPILE_MESSAGE_TYPE = 0,
    COMPILE_RESULT_MESSAGE_TYPE,
    COMPILE_SCRIPT_MESSAGE_TPYE,
    COMPILE_SCRIPT_RESULT_MESSAGE_TPYE,
    CALL_MESSAGE_TYPE,
    CALL_RESULT_MESSAGE_TYPE,
    REGTISTER_MESSAGE_TYPE,
    REGTISTER_RESULT_MESSAGE_TYPE,
    UPGRADE_MESSAGE_TYPE,
    UPGRADE_RESULT_MESSAGE_TYPE,
    TRANSFER_MESSAGE_TYPE,
    TRANSFER_RESULT_MESSAGE_TYPE,
    DESTROY_MESSAGE_TYPE,
    DESTROY_RESULT_MESSAGE_TYPE,
    CALL_OFFLINE_MESSAGE_TYPE,
    CALL_OFFLINE_RESULT_MESSAGE_TYPE,
    HANDLE_EVENTS_MESSAGE_TYPE,
    HANDLE_EVENTS_RESULT_MESSAGE_TYPE,
    LUA_REQUEST_MESSAGE_TYPE,
    LUA_REQUEST_RESULT_MESSAGE_TYPE,
    HELLO_MESSAGE_TYPE,
    MESSAGE_COUNT
};

//HELLO MSG
//hello msg, achain receive hello-msg from lvm only, not send hello-msg
struct HelloMsgResultRpc {
    static const LuaRpcMessageTypeEnum type;
    HelloMsgResult data;
    HelloMsgResultRpc() {}
    HelloMsgResultRpc(HelloMsgResult& para) :
        data(std::move(para))
    {}
};

//task:
struct CompileTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    CompileTask data;
    
    CompileTaskRpc() {}
    CompileTaskRpc(CompileTask& para) :
        data(std::move(para))
    {}
};

struct CallTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    CallTask data;
    
    CallTaskRpc() {}
    CallTaskRpc(CallTask& para) :
        data(std::move(para))
    {}
};

struct RegisterTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    RegisterTask data;
    
    RegisterTaskRpc() {}
    RegisterTaskRpc(RegisterTask& para) :
        data(std::move(para))
    {}
};

struct UpgradeTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    UpgradeTask data;
    
    UpgradeTaskRpc() {}
    UpgradeTaskRpc(UpgradeTask& para) :
        data(std::move(para))
    {}
};

struct TransferTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    TransferTask data;
    
    TransferTaskRpc() {}
    TransferTaskRpc(TransferTask& para) :
        data(std::move(para))
    {}
};

struct DestroyTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    DestroyTask data;
    
    DestroyTaskRpc() {}
    DestroyTaskRpc(DestroyTask& para) :
        data(std::move(para))
    {}
};

struct LuaRequestTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    LuaRequestTask data;
    
    LuaRequestTaskRpc() {}
    LuaRequestTaskRpc(LuaRequestTask& para) :
        data(std::move(para))
    {}
};

struct CompileScriptTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    CompileScriptTask data;
    
    CompileScriptTaskRpc() {}
    CompileScriptTaskRpc(CompileScriptTask& para) :
        data(std::move(para))
    {}
};

struct HandleEventsTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    HandleEventsTask data;
    
    HandleEventsTaskRpc() {}
    HandleEventsTaskRpc(HandleEventsTask& para) :
        data(std::move(para))
    {}
};

struct CallContractOfflineTaskRpc {
    static const LuaRpcMessageTypeEnum type;
    CallContractOfflineTask data;
    
    CallContractOfflineTaskRpc() {}
    CallContractOfflineTaskRpc(CallContractOfflineTask& para) :
        data(std::move(para))
    {}
};

//result:
struct CompileTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    CompileTaskResult data;
    
    CompileTaskResultRpc() {}
    CompileTaskResultRpc(CompileTaskResult& para) :
        data(std::move(para))
    {}
};

struct RegisterTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    RegisterTaskResult data;
    
    RegisterTaskResultRpc() {}
    RegisterTaskResultRpc(RegisterTaskResult& para) :
        data(std::move(para))
    {}
};

struct CallTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    CallTaskResult data;
    
    CallTaskResultRpc() {}
    CallTaskResultRpc(CallTaskResult& para) :
        data(std::move(para))
    {}
};

struct TransferTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    TransferTaskResult data;
    
    TransferTaskResultRpc() {}
    TransferTaskResultRpc(TransferTaskResult& para) :
        data(std::move(para))
    {}
};

struct UpgradeTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    UpgradeTaskResult data;
    
    UpgradeTaskResultRpc() {}
    UpgradeTaskResultRpc(UpgradeTaskResult& para) :
        data(std::move(para))
    {}
};

struct DestroyTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    DestroyTaskResult data;
    
    DestroyTaskResultRpc() {}
    DestroyTaskResultRpc(DestroyTaskResult& para) :
        data(std::move(para))
    {}
};

struct LuaRequestTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    LuaRequestTaskResult data;
    
    LuaRequestTaskResultRpc() {}
    LuaRequestTaskResultRpc(LuaRequestTaskResult& para) :
        data(std::move(para))
    {}
};

struct CompileScriptTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    CompileScriptTaskResult data;
    
    CompileScriptTaskResultRpc() {}
    CompileScriptTaskResultRpc(CompileScriptTaskResult& para) :
        data(std::move(para))
    {}
};

struct HandleEventsTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    HandleEventsTaskResult data;
    
    HandleEventsTaskResultRpc() {}
    HandleEventsTaskResultRpc(HandleEventsTaskResult& para) :
        data(std::move(para))
    {}
};

struct CallContractOfflineTaskResultRpc {
    static const LuaRpcMessageTypeEnum type;
    CallContractOfflineTaskResult data;
    
    CallContractOfflineTaskResultRpc() {}
    CallContractOfflineTaskResultRpc(CallContractOfflineTaskResult& para) :
        data(std::move(para))
    {}
};

FC_REFLECT_ENUM(LuaRpcMessageTypeEnum, (COMPILE_MESSAGE_TYPE)
                (COMPILE_RESULT_MESSAGE_TYPE)
                (COMPILE_SCRIPT_MESSAGE_TPYE)
                (COMPILE_SCRIPT_RESULT_MESSAGE_TPYE)
                (CALL_MESSAGE_TYPE)
                (CALL_RESULT_MESSAGE_TYPE)
                (REGTISTER_MESSAGE_TYPE)
                (REGTISTER_RESULT_MESSAGE_TYPE)
                (UPGRADE_MESSAGE_TYPE)
                (UPGRADE_RESULT_MESSAGE_TYPE)
                (TRANSFER_MESSAGE_TYPE)
                (TRANSFER_RESULT_MESSAGE_TYPE)
                (DESTROY_MESSAGE_TYPE)
                (DESTROY_RESULT_MESSAGE_TYPE)
                (CALL_OFFLINE_MESSAGE_TYPE)
                (CALL_OFFLINE_RESULT_MESSAGE_TYPE)
                (HANDLE_EVENTS_MESSAGE_TYPE)
                (HANDLE_EVENTS_RESULT_MESSAGE_TYPE)
                (LUA_REQUEST_MESSAGE_TYPE)
                (LUA_REQUEST_RESULT_MESSAGE_TYPE)
                (HELLO_MESSAGE_TYPE))

//task
FC_REFLECT(CompileTaskRpc, (data))
FC_REFLECT(CallTaskRpc, (data))
FC_REFLECT(RegisterTaskRpc, (data))
FC_REFLECT(TransferTaskRpc, (data))
FC_REFLECT(UpgradeTaskRpc, (data))
FC_REFLECT(DestroyTaskRpc, (data))
FC_REFLECT(LuaRequestTaskRpc, (data))
FC_REFLECT(CompileScriptTaskRpc, (data))
FC_REFLECT(HandleEventsTaskRpc, (data))
FC_REFLECT(CallContractOfflineTaskRpc, (data))

//result
FC_REFLECT(CompileTaskResultRpc, (data))
FC_REFLECT(RegisterTaskResultRpc, (data))
FC_REFLECT(CallTaskResultRpc, (data))
FC_REFLECT(TransferTaskResultRpc, (data))
FC_REFLECT(UpgradeTaskResultRpc, (data))
FC_REFLECT(DestroyTaskResultRpc, (data))
FC_REFLECT(LuaRequestTaskResultRpc, (data))
FC_REFLECT(CompileScriptTaskResultRpc, (data))
FC_REFLECT(HandleEventsTaskResultRpc, (data))
FC_REFLECT(CallContractOfflineTaskResultRpc, (data))

//hello msg
FC_REFLECT(HelloMsgResultRpc, (data))

#endif