#include <contract/task.hpp>
#include <contract/rpc_mgr.hpp>
#include <iostream>

CompileTaskResult::CompileTaskResult(TaskBase* task) {
	if (!task) {
		return;
	}

	CompileTaskResult* compileTask_p = (CompileTaskResult*)task;
	//memcpy(this, task, sizeof(TaskBase));
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
void TaskImplResult::process_result(RpcClientMgr* msg_p){
	if (msg_p)
	{
		msg_p->set_last_receive_time();
	}

	return;
}
void CompileTaskResult::process_result(RpcClientMgr* msg_p){
	CompileTaskResult* task = (CompileTaskResult*)(this);
	std::cout << "recieve response......." << std::endl;
	std::cout << "task_from: " << (task->task_from == 0 ? "FROM_CLI" : "FROM_RPC") << std::endl;
	std::cout << "task_id: " << task->task_id << std::endl;
	std::cout << "task_type: " << task->task_type << std::endl;
	std::cout << "error_code: " << task->error_code << std::endl;
	std::cout << "error_msg: " << task->error_msg << std::endl;

	return;
}

void RegisterTaskResult::process_result(RpcClientMgr* msg_p){

	//TODO
	return;
}

void CallTaskResult::process_result(RpcClientMgr* msg_p){

	//TODO
	return;
}

void TransferTaskResult::process_result(RpcClientMgr* msg_p){

	//TODO
	return;
}

void UpgradeTaskResult::process_result(RpcClientMgr* msg_p){

	//TODO
	return;
}

void DestroyTaskResult::process_result(RpcClientMgr* msg_p){

	//TODO
	return;
}
