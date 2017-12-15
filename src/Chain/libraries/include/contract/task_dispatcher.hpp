/*
author: pli
date: 2017.11.16
contract task dispatcher
*/

#ifndef _TASK_DISPATCHER_H_
#define _TASK_DISPATCHER_H_

#include <fc/thread/future.hpp>
#include <contract/task.hpp>

class TaskDispatcher {
  public:
    static TaskDispatcher* get_lua_task_dispatcher();
    static void   delete_lua_task_dispatcher();
    
  public:
    void on_lua_request(TaskBase* task);
    TaskImplResult* exec_lua_task(TaskBase* task);
    
    
  private:
    TaskDispatcher();
    ~TaskDispatcher();
    static TaskDispatcher*  _p_lua_task_dispatcher;
    fc::promise<void*>::ptr _exec_lua_task_ptr;
};

#endif
