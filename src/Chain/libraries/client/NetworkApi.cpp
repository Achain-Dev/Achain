#include <client/Client.hpp>
#include <client/ClientImpl.hpp>
#include <client/Messages.hpp>

namespace thinkyoung {
    namespace client {
        namespace detail {
        
            thinkyoung::blockchain::TransactionIdType detail::ClientImpl::network_broadcast_transactions(const std::vector<thinkyoung::blockchain::SignedTransaction>& transactions_to_broadcast) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                // ilog("broadcasting transaction: ${id} ", ("id", transaction_to_broadcast.id()));
                // p2p doesn't send messages back to the originator
                _p2p_node->broadcast(BatchTrxMessage(transactions_to_broadcast));
                return transactions_to_broadcast.begin()->id();
            }
            
            thinkyoung::blockchain::TransactionIdType detail::ClientImpl::network_broadcast_transaction(const thinkyoung::blockchain::SignedTransaction& transaction_to_broadcast) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                // ilog("broadcasting transaction: ${id} ", ("id", transaction_to_broadcast.id()));
                // p2p doesn't send messages back to the originator
                bool bContract = false;
                auto pending = blockchain_list_pending_transactions();
                
                //judge the transaction type
                for (auto operation : transaction_to_broadcast.operations) {
                    if (operation.type == register_contract_op_type || operation.type == upgrade_contract_op_type || operation.type == destroy_contract_op_type
                            || operation.type == call_contract_op_type || operation.type == transfer_contract_op_type || operation.type == withdraw_contract_op_type
                            || operation.type == deposit_contract_op_type) {
                        bContract = true;
                        break;
                    }
                }
                
                if (get_pending_contract_trx_size() > ALP_BLOCKCHAIN_LOCAL_CRITICAL_PENDING_QUEUE_SIZE && bContract) {
                    _local_pending_list.push_back(transaction_to_broadcast);
                    
                } else {
                    _p2p_node->broadcast(TrxMessage(transaction_to_broadcast));
                    on_new_transaction(transaction_to_broadcast);
                }
                
                return transaction_to_broadcast.id();
            }
            
            std::vector<fc::variant_object> detail::ClientImpl::network_get_peer_info(bool hide_firewalled_nodes)const {
                std::vector<fc::variant_object> results;
                std::vector<thinkyoung::net::PeerStatus> peer_statuses = _p2p_node->get_connected_peers();
                
                for (const thinkyoung::net::PeerStatus& peer_status : peer_statuses)
                    if (!hide_firewalled_nodes ||
                            peer_status.info["firewall_status"].as_string() == "not_firewalled")
                        results.push_back(peer_status.info);
                        
                return results;
            }

            
            void detail::ClientImpl::network_set_advanced_node_parameters(const fc::variant_object& params) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                _p2p_node->set_advanced_node_parameters(params);
            }
            
            fc::variant_object detail::ClientImpl::network_get_advanced_node_parameters() const {
                return _p2p_node->get_advanced_node_parameters();
            }
            
            void detail::ClientImpl::network_add_node(const string& node, const string& command) {
                if (command == "add")
                    _self->connect_to_peer(node);
                    
                else if (command == "block") {
                    std::basic_string <char> ip;
                    size_t pos;
                    
                    if ((pos = node.find(':')) != std::string::npos) {
                        _p2p_node->block_node(node.substr(0, pos).c_str(), true);
                        _p2p_node->add_node(fc::ip::endpoint::from_string(node), -1);
                        
                    } else {
                        _p2p_node->block_node(node, true);
                        _p2p_node->add_node(fc::ip::endpoint::from_string(node + ":0"), -1);
                    }
                    
                } else if (command == "unblock") {
                    std::basic_string <char> ip;
                    size_t pos;
                    
                    if ((pos = node.find(':')) != std::string::npos)
                        _p2p_node->block_node(node.substr(0, pos).c_str(), false);
                        
                    else
                        _p2p_node->block_node(node, false);
                        
                } else {
                    FC_THROW_EXCEPTION(fc::invalid_arg_exception, "unsupported command argument \"${command}\", valid commands are: \"add\"", ("command", command));
                }
            }
            
            uint32_t detail::ClientImpl::network_get_connection_count() const {
                return _p2p_node->get_connection_count();
            }
            
            thinkyoung::net::MessagePropagationData detail::ClientImpl::network_get_transaction_propagation_data(const TransactionIdType& transaction_id) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _p2p_node->get_transaction_propagation_data(transaction_id);
                FC_THROW_EXCEPTION(fc::invalid_operation_exception, "get_transaction_propagation_data only valid in p2p mode");
            }
            
            thinkyoung::net::MessagePropagationData detail::ClientImpl::network_get_block_propagation_data(const BlockIdType& block_id) {
                // set limit in  sandbox state
                if (_chain_db->get_is_in_sandbox())
                    FC_THROW_EXCEPTION(sandbox_command_forbidden, "in sandbox, this command is forbidden, you cannot call it!");
                    
                return _p2p_node->get_block_propagation_data(block_id);
                FC_THROW_EXCEPTION(fc::invalid_operation_exception, "get_block_propagation_data only valid in p2p mode");
            }
            
            fc::variant_object ClientImpl::network_get_info() const {
                return _p2p_node->network_get_info();
            }
            
            
            vector<thinkyoung::net::PotentialPeerEntry> ClientImpl::network_list_potential_peers()const {
                return _p2p_node->get_potential_peers();
            }
            
            fc::variant_object ClientImpl::network_get_upnp_info()const {
                fc::mutable_variant_object upnp_info;
                upnp_info["upnp_enabled"] = bool(_upnp_service);
                
                if (_upnp_service) {
                    upnp_info["external_ip"] = fc::string(_upnp_service->external_ip());
                    upnp_info["mapped_port"] = fc::variant(_upnp_service->mapped_port()).as_string();
                }
                
                return upnp_info;
            }
            
            std::vector<std::string> ClientImpl::network_get_blocked_ips()const {
                return _p2p_node->get_blocked_ips();
            }
            
        }
    }
} // namespace thinkyoung::client::detail
