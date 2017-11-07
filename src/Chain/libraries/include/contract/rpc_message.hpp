#ifndef _RPC_MESSAGE_H_
#define _RPC_MESSAGE_H_

#include <stdint.h>
#include <string>

#include <fc/filesystem.hpp>
#include <fc/reflect/variant.hpp>
#include <contract/task.hpp>

enum LuaRpcMessageTypeEnum {
    COMPILE_MESSAGE_TYPE = 0,
    CALL_MESSAGE_TYPE,
    REGTISTER_MESSAGE_TYPE,
    UPGRADE_MESSAGE_TYPE,
    TRANSFER_MESSAGE_TYPE,
    DESTROY_MESSAGE_TYPE,
    MESSAGE_COUNT
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


FC_REFLECT_ENUM(LuaRpcMessageTypeEnum, (COMPILE_MESSAGE_TYPE)(CALL_MESSAGE_TYPE)(REGTISTER_MESSAGE_TYPE)
                (UPGRADE_MESSAGE_TYPE)(TRANSFER_MESSAGE_TYPE)(DESTROY_MESSAGE_TYPE))

//task
FC_REFLECT(CompileTaskRpc, (data))
FC_REFLECT(CallTaskRpc, (data))
FC_REFLECT(RegisterTaskRpc, (data))
FC_REFLECT(TransferTaskRpc, (data))
FC_REFLECT(UpgradeTaskRpc, (data))
FC_REFLECT(DestroyTaskRpc, (data))

//result
FC_REFLECT(CompileTaskResultRpc, (data))
FC_REFLECT(RegisterTaskResultRpc, (data))
FC_REFLECT(CallTaskResultRpc, (data))
FC_REFLECT(TransferTaskResultRpc, (data))
FC_REFLECT(UpgradeTaskResultRpc, (data))
FC_REFLECT(DestroyTaskResultRpc, (data))
#endif