
#include <api/GlobalApiLogger.hpp>

#if ALP_GLOBAL_API_LOG

#include <fc/io/json.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <api/CommonApi.hpp>
#include <client/ApiLogger.hpp>

namespace thinkyoung {
    namespace client {

        StreamApiLogger::StreamApiLogger(fc::ostream_ptr output)
        {
            this->output = output;
            this->is_first_item = true;
            this->is_closed = false;
            (*this->output) << "[\n";
            return;
        }

        StreamApiLogger::~StreamApiLogger()
        {
            this->disconnect();
            this->close();
            return;
        }

        void StreamApiLogger::close()
        {
            fc::scoped_lock<fc::mutex> lock(this->output_mutex);
            if (this->is_closed)
                return;
            (*this->output) << "\n]\n";
            this->output->close();
            return;
        }

        void StreamApiLogger::log_call_started(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args)
        {
            fc::scoped_lock<fc::mutex> lock(this->output_mutex);
            std::string cname = target->debug_get_client_name();

            FC_ASSERT(!this->is_closed);

            if (this->is_first_item)
                this->is_first_item = false;
            else
                (*this->output) << ",\n";
            (*this->output) << "    [\"s\", " << call_id << ", \"" << cname << "\", \"" << name << "\", ";
            fc::json::to_stream((*this->output), args);
            (*this->output) << "]";
            return;
        }

        void StreamApiLogger::log_call_finished(uint64_t call_id, const thinkyoung::api::CommonApi* target, const fc::string& name, const fc::variants& args, const fc::variant& result)
        {
            fc::scoped_lock<fc::mutex> lock(this->output_mutex);
            std::string cname = target->debug_get_client_name();

            FC_ASSERT(!this->is_closed);
            FC_ASSERT(!this->is_first_item);
            (*this->output) << ",\n    [\"f\", " << call_id << ", \"" << cname << "\", \"" << name << "\", ";
            fc::json::to_stream((*this->output), args);
            (*this->output) << ", ";
            fc::json::to_stream((*this->output), result);
            (*this->output) << "]";
            return;
        }

    }
}

#endif
