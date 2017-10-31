
#pragma once

#if ALP_GLOBAL_API_LOG

#include <fc/variant.hpp>

namespace thinkyoung {
    namespace api {

        class CommonApi;

        // individual api_logger objects are called from singleton global_api_logger
        class ApiLogger
        {
        public:

            ApiLogger();
            virtual ~ApiLogger();

            virtual void log_call_started(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args) = 0;
            virtual void log_call_finished(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args, const fc::variant& result) = 0;

            virtual void connect();
            virtual void disconnect();

            bool is_connected;
        };

        class GlobalApiLogger
        {
        public:

            GlobalApiLogger();
            virtual ~GlobalApiLogger();

            virtual uint64_t log_call_started(const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args) = 0;
            virtual void log_call_finished(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args, const fc::variant& result) = 0;
            virtual bool obscure_passwords() const = 0;

            static GlobalApiLogger* get_instance();

            static GlobalApiLogger* the_instance;

            std::vector< ApiLogger* > active_loggers;
        };

    }
} // end namespace thinkyoung::api

#endif
