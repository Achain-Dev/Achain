
#include <api/GlobalApiLogger.hpp>

#if ALP_GLOBAL_API_LOG

#include <atomic>
#include <iostream>

#include <fc/exception/exception.hpp>

namespace thinkyoung {
    namespace api {

        namespace detail
        {
            class GlobalApiLoggerImpl : public GlobalApiLogger
            {
            public:

                GlobalApiLoggerImpl();
                virtual ~GlobalApiLoggerImpl() override;

                virtual uint64_t log_call_started(const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args) override;
                virtual void log_call_finished(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args, const fc::variant& result) override;
                virtual bool obscure_passwords() const override;

                std::atomic_uint_fast64_t next_id;
            };

            GlobalApiLoggerImpl::GlobalApiLoggerImpl()
                : next_id(1)
            {
                return;
            }

            GlobalApiLoggerImpl::~GlobalApiLoggerImpl()
            {
                return;
            }

            uint64_t GlobalApiLoggerImpl::log_call_started(const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args)
            {
                if (name == "debug_get_client_name")
                    return 0;
                uint64_t call_id = (uint64_t)next_id.fetch_add(1);
                for (ApiLogger* logger : this->active_loggers)
                    logger->log_call_started(call_id, target, name, args);
                return call_id;
            }

            void GlobalApiLoggerImpl::log_call_finished(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args, const fc::variant& result)
            {
                if (name == "debug_get_client_name")
                    return;
                for (ApiLogger* logger : this->active_loggers)
                    logger->log_call_finished(call_id, target, name, args, result);
                return;
            }

            bool GlobalApiLoggerImpl::obscure_passwords() const
            {
                // TODO: Allow this to be overwritten
                return false;
            }

        }

        GlobalApiLogger* GlobalApiLogger::the_instance = new detail::GlobalApiLoggerImpl();

        GlobalApiLogger* GlobalApiLogger::get_instance()
        {
            return the_instance;
        }

        GlobalApiLogger::GlobalApiLogger()
        {
            return;
        }

        GlobalApiLogger::~GlobalApiLogger()
        {
            return;
        }

        ApiLogger::ApiLogger()
        {
            this->is_connected = false;
            return;
        }

        ApiLogger::~ApiLogger()
        {
            this->disconnect();
            return;
        }

        void ApiLogger::connect()
        {
            if (this->is_connected)
                return;

            GlobalApiLogger* g = GlobalApiLogger::get_instance();
            if (g == NULL)
                return;

            g->active_loggers.push_back(this);
            this->is_connected = true;
            return;
        }

        void ApiLogger::disconnect()
        {
            if (!this->is_connected)
                return;
            GlobalApiLogger* g = GlobalApiLogger::get_instance();
            if (g == NULL)
                return;

            size_t n = g->active_loggers.size();
            for (size_t i = 0; i < n; i++)
            {
                if (g->active_loggers[i] == this)
                {
                    g->active_loggers.erase(g->active_loggers.begin() + i);
                    break;
                }
            }

            n = g->active_loggers.size();
            for (size_t i = 0; i < n; i++)
            {
                FC_ASSERT(g->active_loggers[i] != this);
            }

            this->is_connected = false;

            return;
        }

    }
}

#endif
