/*
author: pli
date: 2017.11.16
contract task dispatcher
*/

#ifndef _TASK_DISPATCHER_H_
#define _TASK_DISPATCHER_H_

#include <blockchain/ContractOperations.hpp>
#include <blockchain/StorageOperations.hpp>
#include <fc/thread/future.hpp>
#include <contract/task.hpp>


class TaskDispatcher {
  public:
    static TaskDispatcher* get_lua_task_dispatcher();
    static void   delete_lua_task_dispatcher();

  public:
    void on_lua_request(TaskBase* task);
    TaskImplResult* exec_lua_task(TaskBase* task);
    void push_map_storage(const intptr_t statevalue, std::map<std::string, StorageDataType>& trx_storage);
    void pop_map_storage(const intptr_t statevalue);
    std::map<std::string, StorageDataType> get_map_storage(const intptr_t statevalue);

  private:
    TaskDispatcher();
    ~TaskDispatcher();

  public:
    fc::promise<void*>::ptr _exec_lua_task_ptr;
  private:
    static TaskDispatcher*  _p_lua_task_dispatcher;
    static std::map<intptr_t, std::map<std::string, StorageDataType>> _map_storages;
};

#endif
