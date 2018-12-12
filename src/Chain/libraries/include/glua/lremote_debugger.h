#ifndef lremote_debugger_h
#define lremote_debugger_h

#include <boost/asio.hpp>
#include <glua/lprefix.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>

#include <boost/asio/ip/tcp.hpp>

namespace glua
{
	namespace debugger
	{
#define LREMOTE_DEBUGGER_WEB_PORT 6547

		typedef boost::asio::io_service IoService;
		typedef boost::asio::ip::tcp TCP;
		typedef boost::asio::ip::tcp::socket TcpSocket;

		// varname => pair(value-json-string, is-upvalue)
		typedef std::unordered_map<std::string, std::pair<std::string, bool>> LuaDebuggerInfoList;

		struct LRemoteDebuggerCommandInfo
		{
			// ��ѡ: status, resume, stop, add, remove, remove_line_all, remove_all, ���ַ���
			std::string operation;
			std::string filename;
			std::vector<size_t> linenumbers;
		};

		class LRemoteDebugger
		{
		private:
			int _port;
			std::string _address;
			bool _running;
			bool _loop_running;
			bool _pausing_lvm;
			IoService *_io_service;
			std::shared_ptr<TCP::acceptor> _acceptor;
			std::unordered_map<std::string, std::vector<int>> _debugger_source_lines;
			LuaDebuggerInfoList _last_lvm_debugger_status; // lvm���е��ϵ�ʱ������������Ϣ��������

			LRemoteDebuggerCommandInfo read_next_command(std::shared_ptr<TcpSocket> socket);

			std::string process_cmd_info(LRemoteDebuggerCommandInfo cmd_info, bool send_response=true);
			void start_accept();
			void handle_accept(std::shared_ptr<TcpSocket> socket,
				const boost::system::error_code& error);
			void process_connection(std::shared_ptr<TcpSocket> socket);
			void process_connection_msg_loop(std::shared_ptr<TcpSocket> socket);
			void process_connection_msg_response_loop(std::shared_ptr<TcpSocket> socket);
			void start_sync();
			void init_from_file();
		public:
			LRemoteDebugger(std::string address="0.0.0.0", int port= LREMOTE_DEBUGGER_WEB_PORT);
			~LRemoteDebugger();

			// TODO: �Ѷϵ�Ľ��ͨ��socket���ͻ�ȥ
			// TODO: �����첽��ȡ������Ϣ
			void start_async();

			void shutdown();

			bool is_running() const;

			bool is_pausing_lvm() const;

			void set_pausing_lvm(bool pausing);

			std::unordered_map<std::string, std::vector<int>> debugger_source_lines() const;
			LuaDebuggerInfoList last_lvm_debugger_status() const;

			void set_last_lvm_debugger_status(std::string varname, std::string value, bool is_upvalue);

			void clear_last_lvm_debugger_status();
		};

	}
}

#endif
