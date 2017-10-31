#include <blockchain/Time.hpp>
#include <client/Client.hpp>
#include <client/ClientImpl.hpp>
#include <utilities/KeyConversion.hpp>
#include <wallet/Wallet.hpp>

#ifndef WIN32
#include <csignal>
#endif

namespace thinkyoung {
    namespace client {
        namespace detail {

            std::string ClientImpl::debug_get_client_name() const
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                return this->_config.client_debug_name;
            }

        }
    }
} // namespace thinkyoung::client::detail
