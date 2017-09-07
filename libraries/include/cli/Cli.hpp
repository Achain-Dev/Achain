#pragma once

#include <api/ApiMetadata.hpp>
#include <client/Client.hpp>
#include <rpc/RpcServer.hpp>

#include <fc/variant.hpp>

#include <boost/optional.hpp>

namespace thinkyoung {
    namespace rpc {
        class RpcServer;
        typedef std::shared_ptr<RpcServer> RpcServerPtr;
    }
}

#define CLI_PROMPT_SUFFIX ">>> "

namespace thinkyoung {
    namespace cli {

        using namespace thinkyoung::client;
        using namespace thinkyoung::rpc;
        using namespace thinkyoung::wallet;

        namespace detail { class CliImpl; }

        extern bool FILTER_OUTPUT_FOR_TESTS;

        class Cli
        {
        public:
            Cli(thinkyoung::client::Client* client,
                std::istream* command_script = nullptr,
                std::ostream* output_stream = nullptr);

            virtual ~Cli();
            void start();

            void set_input_stream_log(boost::optional<std::ostream&> input_stream_log);
            void set_daemon_mode(bool enable_daemon_mode);
            void display_status_message(const std::string& message);
            void process_commands(std::istream* input_stream);
            void enable_output(bool enable_output);
            void filter_output_for_tests(bool enable_flag);

            //Parse and execute a command line. Returns false if line is a quit command.
            bool execute_command_line(const std::string& line, std::ostream* output = nullptr);
            void confirm_and_broadcast(SignedTransaction& tx);

            // hooks to implement custom behavior for interactive command, if the default json-style behavior is undesirable
            virtual fc::variant parse_argument_of_known_type(fc::buffered_istream& argument_stream,
                const thinkyoung::api::MethodData& method_data,
                unsigned parameter_index);
            virtual fc::variants parse_unrecognized_interactive_command(fc::buffered_istream& argument_stream,
                const std::string& command);
            virtual fc::variants parse_recognized_interactive_command(fc::buffered_istream& argument_stream,
                const thinkyoung::api::MethodData& method_data);
            virtual fc::variants parse_interactive_command(fc::buffered_istream& argument_stream, const std::string& command);
            virtual fc::variant execute_interactive_command(const std::string& command, const fc::variants& arguments);
            virtual void format_and_print_result(const std::string& command,
                const fc::variants& arguments,
                const fc::variant& result);

        private:
            std::unique_ptr<detail::CliImpl> my;
        };
        typedef std::shared_ptr<Cli> CliPtr;

        string get_line(
            std::istream* input_stream,
            std::ostream* out,
            const string& prompt = CLI_PROMPT_SUFFIX,
            bool no_echo = false,
            fc::thread* cin_thread = nullptr,
            bool saved_out = false,
            std::ostream* input_stream_log = nullptr
            );

    }
} // thinkyoung::cli
