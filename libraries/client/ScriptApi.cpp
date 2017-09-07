#include <client/Client.hpp>
#include <client/ClientImpl.hpp>

#include <blockchain/Types.hpp>
#include <fc/reflect/variant.hpp>

#include <wallet/Wallet.hpp>

namespace thinkyoung {
    namespace client {
        namespace detail {

            string ClientImpl::add_script(const fc::path& path, const string& desc)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                return Address(_wallet->add_script(path, desc)).AddressToString(AddressType::script_id);
            }
            void ClientImpl::remove_script(const string& script_id)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                _wallet->delete_script(ScriptIdType(script_id,AddressType::script_id));
            }
            ScriptEntryPrintable ClientImpl::get_script_info(const string& script_id)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                auto res = _wallet->get_script_entry(ScriptIdType(script_id, AddressType::script_id));
                if (res.valid())
                {
                    ScriptEntryPrintable res_printable(*res);
                    return res_printable;
                }

                FC_CAPTURE_AND_THROW(script_not_found_in_db, (("no such script in database")));
            }
            vector<ScriptEntryPrintable> ClientImpl::list_scripts()
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                vector<ScriptEntryPrintable> res_printable_vec;

                for (auto res : _wallet->list_scripts())
                    res_printable_vec.push_back(ScriptEntryPrintable(res));

                return res_printable_vec;
            }
            void ClientImpl::disable_script(const string& id)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

				try {
                _wallet->disable_script(ScriptIdType(id, AddressType::script_id));
				}
				catch (const invalid_address&)
				{
					FC_CAPTURE_AND_THROW(script_id_not_valid, ("id"));
				}
			
            }
            void ClientImpl::enable_script(const string& id)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

				try {
					_wallet->enable_script(ScriptIdType(id, AddressType::script_id));
				}
				catch (const invalid_address&)
				{
					FC_CAPTURE_AND_THROW(script_id_not_valid, ("id"));
				}
            }
            void  ClientImpl::import_script_db(const fc::path& src_dir)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                _wallet->import_script_db(src_dir);
            }
            void  ClientImpl::export_script_db(const fc::path& dest_dir)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                _wallet->export_script_db(dest_dir);
            }

			std::vector<std::string> ClientImpl::list_event_handler(const string& contract_id_str, const std::string& event_type)
			{
				// set limit in  sandbox state
				if (_chain_db->get_is_in_sandbox())
					FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
				thinkyoung::blockchain::ContractIdType contract_id = get_contract_address(contract_id_str);
			
                auto idVec=_wallet->list_event_handler(contract_id, event_type);
                vector<string> res;
                for (auto id : idVec)
                {
                    res.push_back(id.AddressToString(script_id));
                }
                return res;
            }

            void ClientImpl::add_event_handler(const string& contract_id_str, const std::string& event_type, const string& script_id, uint32_t index)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                thinkyoung::blockchain::ContractIdType contract_id = thinkyoung::blockchain::Address(contract_id_str, thinkyoung::blockchain::AddressType::contract_address);
                _wallet->add_event_handler(contract_id, event_type, ScriptIdType(script_id, AddressType::script_id), index);
            }

            void ClientImpl::delete_event_handler(const string& contract_id_str, const std::string& event_type, const string& script_id)
            {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");

                thinkyoung::blockchain::ContractIdType contract_id = thinkyoung::blockchain::Address(contract_id_str, thinkyoung::blockchain::AddressType::contract_address);
                _wallet->delete_event_handler(contract_id, event_type, ScriptIdType(script_id, AddressType::script_id));
            }
			std::vector<std::string> ClientImpl::get_events_bound(const std::string& script_id)
			{
				return _wallet->get_events_bound(script_id);
			}
        }
    }
}