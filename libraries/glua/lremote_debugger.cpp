#include <glua/lremote_debugger.h>

#include <memory>
#include <fstream>
#include <glua/glua_lutil.h>
#include <boost/bind/bind.hpp>
#include <boost/variant/variant.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

namespace glua
{
	namespace debugger
	{
		LRemoteDebugger::LRemoteDebugger(std::string address, int port)
			: _address(address), _port(port), _io_service(nullptr), _running(false), _loop_running(false), _pausing_lvm(false)
		{
			init_from_file();
		}

		LRemoteDebugger::~LRemoteDebugger()
		{
			shutdown();
		}

		void LRemoteDebugger::handle_accept(std::shared_ptr<TcpSocket> socket,
			const boost::system::error_code& error)
		{
			process_connection(socket);
			if (!_running)
			{
				_loop_running = false;
				return;
			}
			start_accept();
		}

		using boost::property_tree::ptree;
		using boost::property_tree::read_json;
		using boost::property_tree::write_json;

		void LRemoteDebugger::process_connection(std::shared_ptr<TcpSocket> socket)
		{
			std::cout << "processing new connection" << std::endl;
			std::thread t1(std::bind(&LRemoteDebugger::process_connection_msg_loop, this, socket));
			t1.detach();
			std::thread t2(std::bind(&LRemoteDebugger::process_connection_msg_response_loop, this, socket));
			t2.detach();
		}

		void LRemoteDebugger::process_connection_msg_loop(std::shared_ptr<TcpSocket> socket)
		{
			while(_running)
			{
				auto cmd_info = read_next_command(socket);
				auto res = process_cmd_info(cmd_info);
				boost::system::error_code ignored_error;
				boost::asio::write(*socket, boost::asio::buffer(res), ignored_error);
			}
		}

		void LRemoteDebugger::process_connection_msg_response_loop(std::shared_ptr<TcpSocket> socket)
		{
			while(_running)
			{
				if(_last_lvm_debugger_status.size()>0)
				{
					ptree pt;
					for(const auto &item : _last_lvm_debugger_status)
					{
						ptree pt_of_var_item_value;
						pt_of_var_item_value.put("value", item.second.first);
						pt_of_var_item_value.put("is_upvalue", item.second.second);
						pt.put_child(item.first, pt_of_var_item_value);
					}
					std::ostringstream buf;
					write_json(buf, pt, false);
					auto jsonvalue = buf.str();
					_last_lvm_debugger_status.clear();
					boost::system::error_code ignored_error;
					boost::asio::write(*socket, boost::asio::buffer(jsonvalue), ignored_error);
				}
			}
		}

		void LRemoteDebugger::start_accept()
		{
			auto socket = std::make_shared<TCP::socket>(*_io_service);
			_acceptor->async_accept(*socket,
				boost::bind(&LRemoteDebugger::handle_accept, this, socket,
					boost::asio::placeholders::error));
		}

		// 初始化时从本地缓存中读取调试信息配置
		void LRemoteDebugger::init_from_file()
		{
			std::string init_debugger_info_filename = "init_debugger_info.db";
			std::ifstream in(init_debugger_info_filename, std::ios::in | std::ios::binary);
			if(in.is_open())
			{
				while(true)
				{
					std::string command_str;
					std::getline(in, command_str);
					command_str = glua::util::string_trim(command_str);
					if (glua::util::ends_with(command_str, std::string(";")))
					{
						command_str = command_str.substr(0, command_str.length() - 1);
					}
					if (command_str.length() < 1)
						break;
					LRemoteDebuggerCommandInfo cmd_info;
					std::vector<std::string> str_items;
					glua::util::string_split(command_str, ',', str_items);
					if(str_items.size()>0)
						cmd_info.operation = str_items[0];
					if (str_items.size() > 1)
						cmd_info.filename = str_items[1];
					for (size_t i = 2; i<str_items.size(); ++i)
					{
						int linenumber = std::atoi(str_items[i].c_str());
						cmd_info.linenumbers.push_back(static_cast<size_t>(linenumber));
					}
					process_cmd_info(cmd_info, false);
					if (in.eof())
						break;
				}
				in.close();
			}
		}

		void LRemoteDebugger::start_sync()
		{
			_io_service = new IoService();
			boost::asio::ip::tcp::resolver resolver(*_io_service);
			boost::asio::ip::tcp::resolver::query query(_address, std::to_string(_port));
			boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
			_acceptor = std::make_shared<TCP::acceptor>(*_io_service, endpoint);

			start_accept();

			_io_service->run();
			_running = true;
			_loop_running = true;
			_pausing_lvm = true;
			/*
			auto *socket = new TCP::socket(*_io_service);
			_acceptor->accept(*socket);
			while(_running)
			{
				auto cmd_info = read_next_command(socket);
				process_cmd_info(cmd_info);
			}
			_loop_running = false;
			delete socket;
			*/
		}

		void LRemoteDebugger::start_async()
		{
			std::thread t(std::bind(&LRemoteDebugger::start_sync, this));
			t.detach();
		}

		void LRemoteDebugger::shutdown()
		{
			_running = false;
			while(_loop_running)
			{ }
			if(_io_service)
			{
				_io_service->stop();
				delete _io_service;
				_io_service = nullptr;
			}
		}

		std::unordered_map<std::string, std::vector<int>> LRemoteDebugger::debugger_source_lines() const
		{
			return _debugger_source_lines;
		}

		LuaDebuggerInfoList LRemoteDebugger::last_lvm_debugger_status() const
		{
			return _last_lvm_debugger_status;
		}

		void LRemoteDebugger::set_last_lvm_debugger_status(std::string varname, std::string value, bool is_upvalue)
		{
			_last_lvm_debugger_status[varname] = std::make_pair(value, is_upvalue);
		}

		void LRemoteDebugger::clear_last_lvm_debugger_status()
		{
			_last_lvm_debugger_status.clear();
		}

		// 每条命令用英文分号;结尾, 每条命令内部是"op, @文件名,用逗号分隔的行号列表",
		LRemoteDebuggerCommandInfo LRemoteDebugger::read_next_command(std::shared_ptr<TcpSocket> socket)
		{
			std::vector<char> end = { ';' };
			boost::system::error_code ignored_error;
			std::vector<char> all_buf;
			while (true)
			{
				std::vector<char> buf(1);
				auto len = boost::asio::read(*socket,
					boost::asio::buffer(buf), boost::asio::transfer_exactly(1), ignored_error);
				if (len<1)
				{
					break;
				}
				else
				{
					for (size_t i = 0; i<len; ++i)
					{
						all_buf.push_back(buf[i]);
					}
					if (all_buf.size() >= end.size())
					{
						bool ended = true;
						for (size_t i = 0; i<end.size(); ++i)
						{
							if (end[i] != all_buf[all_buf.size() + i - end.size()])
							{
								ended = false;
								break;
							}
						}
						if (ended)
							break;
					}
				}
			}
			all_buf.push_back('\0');
			std::string command_str(all_buf.data());
			LRemoteDebuggerCommandInfo cmd_info;
			std::vector<std::string> str_items;
			glua::util::string_split(command_str, ',', str_items);
			if (str_items.size() < 1)
				return cmd_info; // 这个命令没有指定op
			cmd_info.operation = str_items[0];
			if (str_items.size() > 1)
				cmd_info.filename = str_items[1];
			for(size_t i=2;i<str_items.size();++i)
			{
				int linenumber = std::atoi(str_items[i].c_str());
				if (linenumber < 1)
					continue;
				cmd_info.linenumbers.push_back(static_cast<size_t>(linenumber));
			}
			return cmd_info;
		}

		bool LRemoteDebugger::is_running() const
		{
			return _running;
		}

		bool LRemoteDebugger::is_pausing_lvm() const
		{
			return is_running() && _pausing_lvm;
		}

		void LRemoteDebugger::set_pausing_lvm(bool pausing)
		{
			_pausing_lvm = pausing;
		}

		std::string LRemoteDebugger::process_cmd_info(LRemoteDebuggerCommandInfo cmd_info, bool send_response)
		{
			if (cmd_info.operation.length() < 1)
				return "empty command";
			if(cmd_info.operation=="add")
			{
				if (cmd_info.filename.length() < 1)
					return "empty add command";
				auto filename_found = _debugger_source_lines.find(cmd_info.filename);
				if (filename_found == _debugger_source_lines.end())
				{
					std::vector<int> linenumbers;
					for (size_t line : cmd_info.linenumbers)
					{
						int line_int = (int)line;
						linenumbers.push_back(line_int);
					}
					_debugger_source_lines[cmd_info.filename] = linenumbers;
					return "add successfully";
				}
				auto linenumbers = filename_found->second;
				for(size_t line : cmd_info.linenumbers)
				{
					int line_int = static_cast<int>(line);
					if(!glua::util::vector_contains(linenumbers, line_int))
					{
						linenumbers.push_back(line_int);
					}
				}
				return "add successfully";
			} else if(cmd_info.operation == "remove")
			{
				if (cmd_info.filename.length() < 1)
					return "empty filename";
				auto filename_found = _debugger_source_lines.find(cmd_info.filename);
				if (filename_found == _debugger_source_lines.end())
					return "filename not found of remove cmd";
				auto linenumbers = filename_found->second;
				for (size_t line : cmd_info.linenumbers)
				{
					int line_int = static_cast<int>(line);
					if (glua::util::vector_contains(linenumbers, line_int))
					{
						std::remove(linenumbers.begin(), linenumbers.end(), line_int);
					}
				}
				return "remove successfully";
			} else if(cmd_info.operation == "remove_line_all")
			{
				if (cmd_info.filename.length() < 1)
					return "empty remove_line_all cmd";
				auto filename_found = _debugger_source_lines.find(cmd_info.filename);
				if (filename_found == _debugger_source_lines.end())
					return "filename to remove lines not found";
				auto linenumbers = filename_found->second;
				linenumbers.clear();
				return "remove_line_all";
			} else if(cmd_info.operation == "remove_all")
			{
				_debugger_source_lines.clear();
				return "remove all successfully";
			} else if(cmd_info.operation == "status")
			{
				// TODO
				// TODO: send back debugging status, whether the lvm pausing
				ptree pt;
				for (const auto &item : _last_lvm_debugger_status)
				{
					ptree pt_of_var_item_value;
					pt_of_var_item_value.put("value", item.second.first);
					pt_of_var_item_value.put("is_upvalue", item.second.second);
					pt.put_child(item.first, pt_of_var_item_value);
				}
				std::ostringstream buf;
				write_json(buf, pt, false);
				auto jsonvalue = buf.str();
				return jsonvalue;
			}
			else if(cmd_info.operation == "resume")
			{
				// TODO
				_pausing_lvm = false;
				return "resume successfully";
			} else if(cmd_info.operation == "stop")
			{
				_pausing_lvm = true;
				shutdown();
				return "stop successfully";
			} else
			{
				return std::string("unknown command ") + cmd_info.operation;
			}
		}

	}
}
