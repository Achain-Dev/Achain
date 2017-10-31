#include <blockchain/Types.hpp>
#include <cli/PrintResult.hpp>
#include <rpc/RpcServer.hpp>
#include <wallet/Url.hpp>
#include <cli/locale.hpp>

#include <boost/range/algorithm/max_element.hpp>
#include <boost/range/algorithm/min_element.hpp>


namespace thinkyoung {
    namespace cli {

        PrintResult::PrintResult()
        {
            initialize();
        }

        void PrintResult::format_and_print(const string& method_name, const fc::variants& arguments, const fc::variant& result,
            ClientRawPtr client, std::ostream& out)const
        {
            try
            {
                TCommandToFunction::const_iterator iter = command_to_function.find(method_name);

                if (iter != command_to_function.end())
                    (iter->second)(out, arguments, result, client);
                else
                {
                    // there was no custom handler for this particular command, see if the return type
                    // is one we know how to pretty-print
                    string result_type;

                    const thinkyoung::api::MethodData& method_data = client->get_rpc_server()->get_method_data(method_name);
                    result_type = method_data.return_type;

                    if (result_type == "asset")
                    {
                        out << string(result.as<thinkyoung::blockchain::Asset>()) << "\n";
                    }
                    else if (result_type == "address")
                    {
                        out << (string)result.as<thinkyoung::blockchain::Address>() << "\n";
                    }
                    else if (result_type == "null" || result_type == "void")
                    {
                        out << "OK\n";
                    }
                    else
                    {
                        out << fc::json::to_pretty_string(result) << "\n";
                    }
                }
            }
            catch (const fc::key_not_found_exception&)
            {
                elog(" KEY NOT FOUND ");
                out << "key not found \n";
            }
            catch (const fc::exception& e)
            {
                elog("Exception from pretty printer: ${e}", ("e", e.to_detail_string()));
            }
            catch (...)
            {
                out << "unexpected exception \n";
            }
        }

        void PrintResult::initialize()
        {
            command_to_function["help"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                string help_string = result.as<string>();
                out << help_string << "\n";
            };

            command_to_function["wallet_account_vote_summary"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& votes = result.as<AccountVoteSummaryType>();
                out << pretty_vote_summary(votes, client);
            };

            command_to_function["debug_list_errors"] = &f_debug_list_errors;


            command_to_function["get_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& info = result.as<variant_object>();
                out << pretty_info(info, client);
            };

            command_to_function["blockchain_get_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& config = result.as<variant_object>();
                out << pretty_blockchain_info(config, client);
            };

            command_to_function["wallet_get_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& info = result.as<variant_object>();
                out << pretty_wallet_info(info, client);
            };

            command_to_function["wallet_account_transaction_history"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& transactions = result.as<vector<PrettyTransaction>>();
                //out << pretty_transaction_list( transactions, client );
                out << pretty_transaction_list_history(transactions, client);
            };
            command_to_function["wallet_transaction_history_splite"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& transactions = result.as<vector<PrettyTransaction>>();
                //out << pretty_transaction_list( transactions, client );
                out << pretty_transaction_list_history(transactions, client);
            };

            command_to_function["wallet_transaction_history_experimental"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& transactions = result.as<set<PrettyTransactionExperimental>>();
                out << pretty_experimental_transaction_list(transactions, client);
            };



            command_to_function["wallet_list_my_accounts"] = &f_wallet_list_my_accounts;

            command_to_function["wallet_list_accounts"] = &f_wallet_list_accounts;
            command_to_function["wallet_list_unregistered_accounts"] = &f_wallet_list_accounts;
            command_to_function["wallet_list_favorite_accounts"] = &f_wallet_list_accounts;

            command_to_function["wallet_account_balance"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& balances = result.as<AccountBalanceSummaryType>();
                out << pretty_balances(balances, client);
            };

			command_to_function["sandbox_account_balance"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
			{
				const auto& balances = result.as<AccountBalanceSummaryType>();
				out << pretty_balances(balances, client);
			};

            command_to_function["wallet_account_yield"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& yield = result.as<AccountBalanceSummaryType>();
                out << pretty_balances(yield, client);
            };

            command_to_function["wallet_transfer"] = &f_wallet_transfer;
            command_to_function["wallet_transfer_from"] = &f_wallet_transfer;
            command_to_function["wallet_get_transaction"] = &f_wallet_transfer;
            command_to_function["wallet_account_register"] = &f_wallet_transfer;
            command_to_function["wallet_account_retract"] = &f_wallet_transfer;
            command_to_function["wallet_account_update_registration"] = &f_wallet_transfer;
            command_to_function["wallet_account_update_active_key"] = &f_wallet_transfer;
            command_to_function["wallet_asset_create"] = &f_wallet_transfer;
            command_to_function["wallet_asset_issue"] = &f_wallet_transfer;
            command_to_function["wallet_delegate_withdraw_pay"] = &f_wallet_transfer;

            command_to_function["wallet_publish_version"] = &f_wallet_transfer;
            command_to_function["wallet_publish_slate"] = &f_wallet_transfer;

            command_to_function["wallet_scan_transaction"] = &f_wallet_transfer;
            command_to_function["wallet_recover_transaction"] = &f_wallet_transfer;

            command_to_function["wallet_transfer_to_address"] = &f_wallet_transfer_2;
            command_to_function["wallet_transfer_to_public_account"] = &f_wallet_transfer_2;

            command_to_function["wallet_list"] = &f_wallet_list;

            command_to_function["network_get_usage_stats"] = &f_network_get_usage_stats;

            command_to_function["blockchain_list_delegates"] = &f_blockchain_list_delegates;
            command_to_function["blockchain_list_active_delegates"] = &f_blockchain_list_delegates;

            command_to_function["get_contract_info"] = &f_get_contract_info;
            command_to_function["sandbox_get_contract_info"] = &f_get_contract_info;

            command_to_function["blockchain_list_blocks"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& block_entrys = result.as<vector<BlockEntry>>();
                out << pretty_block_list(block_entrys, client);
            };

            command_to_function["blockchain_list_accounts"] = &f_blockchain_list_accounts;

            command_to_function["blockchain_list_assets"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << pretty_asset_list(result.as<vector<AssetEntry>>(), client);
            };

            command_to_function["blockchain_get_account"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << pretty_account(result.as<oAccountEntry>(), client);
            };

            command_to_function["blockchain_list_forks"] = &f_blockchain_list_forks;

            command_to_function["blockchain_list_pending_transactions"] = &f_blockchain_list_pending_transactions;

            //_command_to_function["blockchain_market_order_book"] = &f_blockchain_market_order_book;

            //_command_to_function["blockchain_market_order_history"] = &f_blockchain_market_order_history;

            command_to_function["blockchain_get_pretty_transaction"] = &f_blockchain_get_pretty_transaction;

            command_to_function["blockchain_get_transaction"] = &f_blockchain_get_transaction;

            command_to_function["blockchain_get_block_transactions"] = &f_blockchain_get_block_transactions;

            command_to_function["blockchain_unclaimed_genesis"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << client->get_chain()->to_pretty_asset(result.as<Asset>()) << "\n";
            };

            command_to_function["blockchain_calculate_supply"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << client->get_chain()->to_pretty_asset(result.as<Asset>()) << "\n";
            };

            command_to_function["blockchain_calculate_debt"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << client->get_chain()->to_pretty_asset(result.as<Asset>()) << "\n";
            };

            command_to_function["network_list_potential_peers"] = &f_network_list_potential_peers;

            command_to_function["wallet_set_transaction_fee"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << client->get_chain()->to_pretty_asset(result.as<Asset>()) << "\n";
            };

            command_to_function["wallet_get_transaction_fee"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                out << client->get_chain()->to_pretty_asset(result.as<Asset>()) << "\n";
            };


            command_to_function["disk_usage"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& usage = result.as<fc::mutable_variant_object>();
                out << pretty_disk_usage(usage);
            };

#define blank4 "    "

            command_to_function["blockchain_get_block"] = &f_blockchain_get_block;
            command_to_function["list_scripts"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& res = result.as<vector<ScriptEntryPrintable>>();
                out << "Number" << blank4 << "RegisterTime" << blank4 << "ID" << blank4 << "Status" << blank4 << "Description\n";
                int i = 0;
                for (auto it : res)
                {
                    string desc;
#ifdef WIN32
                    desc = UTF8ToGBK(it.description);
#else
                    desc = it.description;
#endif
                    out << i << blank4 << it.register_time.to_iso_string() << blank4 << it.id << blank4 << (it.enable ? "enabled" : "disabled") << blank4 << desc << "\n";
                    i++;
                }
            };
            command_to_function["get_script_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                auto res = result.as<thinkyoung::wallet::ScriptEntryPrintable>();
                string res_str = "";
                string res_desc = "";
                res_str = res_str + "id:" + res.id + "\n";
#ifdef WIN32
                res_desc = UTF8ToGBK(res.description);
#else
                res_desc = res.description;
#endif
                res_str = res_str + "description:" + res_desc + "\n";
                res_str = res_str + "enabled:" + (res.enable ? "yes" : "no") + "\n";
                res_str = res_str + "register_time:" + res.register_time.to_iso_string() + "\n";
                out << res_str;
            };
            command_to_function["list_event_handler"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                const auto& res = result.as<vector<std::string>>();
                out << "Number" << blank4 << "ID\n";
                int i = 0;
                for (auto it : res)
                {
                    out << i << blank4 << it << "\n";
                    i++;
                }
            };
            auto print_fc_path = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
            {
                auto res = result.as<fc::path>();
                string filepath_str = res.string();
                out << filepath_str << "\n";
            };
            command_to_function["compile_contract"] = print_fc_path;
            command_to_function["compile_script"] = print_fc_path;
            command_to_function["sandbox_compile_contract"] = print_fc_path;
        }


        void PrintResult::f_debug_list_errors(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto error_map = result.as<map<fc::time_point, fc::exception> >();
            if (error_map.empty())
                out << "No errors.\n";
            else
                for (const auto& item : error_map)
                {
                    (out) << string(item.first) << " (" << fc::get_approximate_relative_time_string(item.first) << " )\n";
                    (out) << item.second.to_detail_string();
                    (out) << "\n";
                }
        }

        void PrintResult::f_wallet_list_my_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto accts = result.as<vector<WalletAccountEntry>>();
            out << std::setw(66) << std::left << "NAME (* delegate)";
            //out << std::setw(35) << std::left << "NAME (* delegate)";
            out << std::setw(64) << "KEY";
            out << std::setw(22) << "REGISTERED";
            out << std::setw(15) << "FAVORITE";
            out << std::setw(15) << "APPROVAL";
            out << std::setw(25) << "BLOCK PRODUCTION ENABLED";
            out << "\n";

            for (const auto& acct : accts)
            {
                if (acct.is_delegate())
                {
                    out << std::setw(66) << acct.name + " *";
                    //out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
                }
                else
                {
                    out << std::setw(66) << acct.name;
                    //out << std::setw(35) << pretty_shorten(acct.name, 34);
                }

                out << std::setw(64) << string(acct.active_key());

                if (acct.id == 0)
                {
                    out << std::setw(22) << "NO";
                }
                else
                {
                    out << std::setw(22) << pretty_timestamp(acct.registration_date);
                }

                if (acct.is_favorite)
                    out << std::setw(15) << "YES";
                else
                    out << std::setw(15) << "NO";

                out << std::setw(15) << std::to_string(acct.approved);
                if (acct.is_delegate())
                    out << std::setw(25) << (acct.block_production_enabled ? "YES" : "NO");
                else
                    out << std::setw(25) << "N/A";
                out << "\n";
            }
        }

        void PrintResult::f_wallet_list_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto accts = result.as<vector<WalletAccountEntry>>();
            out << std::setw(66) << std::left << "NAME (* delegate)";
            //out << std::setw(35) << std::left << "NAME (* delegate)";
            out << std::setw(64) << "KEY";
            out << std::setw(22) << "REGISTERED";
            out << std::setw(15) << "FAVORITE";
            out << std::setw(15) << "APPROVAL";
            out << "\n";

            for (const auto& acct : accts)
            {
                if (acct.is_delegate())
                {
                    out << std::setw(66) << acct.name + " *";
                    //out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
                }
                else
                {
                    out << std::setw(66) << acct.name;
                    //out << std::setw(35) << pretty_shorten(acct.name, 34);
                }


                out << std::setw(64) << string(acct.active_key());

                if (acct.id == 0)
                    out << std::setw(22) << "NO";
                else
                    out << std::setw(22) << pretty_timestamp(acct.registration_date);

                if (acct.is_favorite)
                    out << std::setw(15) << "YES";
                else
                    out << std::setw(15) << "NO";

                out << std::setw(10) << std::to_string(acct.approved);
                out << "\n";
            }
        }

        void PrintResult::f_wallet_transfer(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            const auto& entry = result.as<WalletTransactionEntry>();
            auto pretty = client->get_wallet()->to_pretty_trx(entry);

            const std::vector<PrettyTransaction> transactions = { pretty };
            out << pretty_transaction_list(transactions, client);
        }

        void PrintResult::f_wallet_transfer_2(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto entry = result.as<WalletTransactionEntry>();

#ifdef WIN32
            for (int i = 0; i < entry.trx.operations.size(); ++i)
                if (entry.trx.operations[i].type == thinkyoung::blockchain::OperationTypeEnum::imessage_memo_op_type)
                {
                    ImessageMemoOperation memo_op = entry.trx.operations[i].as<ImessageMemoOperation>();
                    memo_op.imessage = UTF8ToGBK(memo_op.imessage);
                    entry.trx.operations[i] = memo_op;
                }

            for (int j = 0; j < entry.ledger_entries.size(); ++j)
                entry.ledger_entries[j].memo = UTF8ToGBK(entry.ledger_entries[j].memo);
#endif 

            out << fc::json::to_pretty_string(entry);
        }

        void PrintResult::f_blockchain_get_pretty_transaction(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto pretty = result.as<PrettyTransaction>();
#ifdef WIN32
            for (int i = 0; i < pretty.ledger_entries.size(); ++i)
                pretty.ledger_entries[i].memo = UTF8ToGBK(pretty.ledger_entries[i].memo);
#endif
            out << fc::json::to_pretty_string(pretty);
        }

        void PrintResult::f_blockchain_get_transaction(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto trx_pair = result.as<pair<TransactionIdType, TransactionEntry>>();
#ifdef WIN32
            for (int i = 0; i < trx_pair.second.trx.operations.size(); ++i)
                if (trx_pair.second.trx.operations[i].type == thinkyoung::blockchain::OperationTypeEnum::imessage_memo_op_type)
                {
                    ImessageMemoOperation memo_op = trx_pair.second.trx.operations[i].as<ImessageMemoOperation>();
                    memo_op.imessage = UTF8ToGBK(memo_op.imessage);
                    trx_pair.second.trx.operations[i] = memo_op;
                }
#endif
            out << fc::json::to_pretty_string(trx_pair);
        }

        void PrintResult::f_blockchain_get_block_transactions(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto trx_map = result.as<map<TransactionIdType, TransactionEntry>>();
#ifdef WIN32
            for (auto iter = trx_map.begin(); iter != trx_map.end(); ++iter)
                for (int i = 0; i < iter->second.trx.operations.size(); ++i)
                    if (iter->second.trx.operations[i].type == thinkyoung::blockchain::OperationTypeEnum::imessage_memo_op_type)
                    {
                        ImessageMemoOperation memo_op = iter->second.trx.operations[i].as<ImessageMemoOperation>();
                        memo_op.imessage = UTF8ToGBK(memo_op.imessage);
                        iter->second.trx.operations[i] = memo_op;
                    }
#endif
            out << fc::json::to_pretty_string(trx_map);
        }

        void PrintResult::f_wallet_list(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto wallets = result.as<vector<string>>();
            if (wallets.empty())
                out << "No wallets found.\n";
            else
                for (const auto& wallet : wallets)
                    out << wallet << "\n";
        }

        void PrintResult::f_network_get_usage_stats(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto stats = result.get_object();

            std::vector<uint32_t> usage_by_second = stats["usage_by_second"].as<std::vector<uint32_t> >();
            if (!usage_by_second.empty())
            {
                out << "last minute:\n";
                print_network_usage_graph(out, usage_by_second);
                out << "\n";
            }
            std::vector<uint32_t> usage_by_minute = stats["usage_by_minute"].as<std::vector<uint32_t> >();
            if (!usage_by_minute.empty())
            {
                out << "last hour:\n";
                print_network_usage_graph(out, usage_by_minute);
                out << "\n";
            }
            std::vector<uint32_t> usage_by_hour = stats["usage_by_hour"].as<std::vector<uint32_t> >();
            if (!usage_by_hour.empty())
            {
                out << "by hour:\n";
                print_network_usage_graph(out, usage_by_hour);
                out << "\n";
            }
        }

        void PrintResult::print_network_usage_graph(std::ostream& out, const std::vector<uint32_t>& usage_data)
        {
            uint32_t max_value = *boost::max_element(usage_data);
            uint32_t min_value = *boost::min_element(usage_data);
            const unsigned num_rows = 10;
            for (unsigned row = 0; row < num_rows; ++row)
            {
                uint32_t threshold = min_value + (max_value - min_value) / (num_rows - 1) * (num_rows - row - 1);
                for (unsigned column = 0; column < usage_data.size(); ++column)
                    out << (usage_data[column] >= threshold ? "*" : " ");
                out << " " << threshold << " bytes/sec\n";
            }
            out << "\n";
        }

        void PrintResult::f_blockchain_list_delegates(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            const auto& delegate_entrys = result.as<vector<AccountEntry>>();
            out << pretty_delegate_list(delegate_entrys, client);
        }

        void PrintResult::f_get_contract_info(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto contract_info = result.as<ContractEntryPrintable>();
#ifdef WIN32
            contract_info.description = UTF8ToGBK(contract_info.description);
#endif
            out << fc::json::to_pretty_string(contract_info);
        }

        void PrintResult::f_blockchain_list_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            string start = "";
            int32_t count = 25; // In CLI this is a more sane default
            if (arguments.size() > 0)
                start = arguments[0].as_string();
            if (arguments.size() > 1)
                count = arguments[1].as<int32_t>();
            print_registered_account_list(out, result.as<vector<AccountEntry>>(), count, client);
        }

        void PrintResult::print_registered_account_list(std::ostream& out, const vector<AccountEntry>& account_entrys, int32_t count, ClientRawPtr client)
        {
            out << std::setw(35) << std::left << "NAME (* delegate)";
            out << std::setw(64) << "KEY";
            out << std::setw(22) << "REGISTERED";
            out << std::setw(15) << "VOTES FOR";
            out << std::setw(15) << "APPROVAL";

            out << '\n';
            for (int i = 0; i < 151; ++i)
                out << '-';
            out << '\n';

            int32_t counter = 0;
            for (const auto& acct : account_entrys)
            {
                if (acct.is_delegate())
                {
                    out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
                }
                else
                {
                    out << std::setw(35) << pretty_shorten(acct.name, 34);
                }

                out << std::setw(64) << string(acct.active_key());
                out << std::setw(22) << pretty_timestamp(acct.registration_date);

                if (acct.is_delegate())
                {
                    out << std::setw(15) << acct.delegate_info->votes_for;

                    try
                    {
                        out << std::setw(15) << std::to_string(client->get_wallet()->get_account_approval(acct.name));
                    }
                    catch (...)
                    {
                        out << std::setw(15) << false;
                    }
                }
                else
                {
                    out << std::setw(15) << "N/A";
                    out << std::setw(15) << "N/A";
                }

                out << "\n";

                /* Count is positive b/c CLI overrides default -1 arg */
                if (counter >= count)
                {
                    out << "... Use \"blockchain_list_accounts <start_name> <count>\" to see more.\n";
                    return;
                }
                counter++;
            }
        }

        void PrintResult::f_blockchain_list_forks(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            std::map<uint32_t, std::vector<ForkEntry>> forks = result.as<std::map<uint32_t, std::vector<ForkEntry>>>();
            std::map<BlockIdType, std::string> invalid_reasons; //Your reasons are invalid.

            if (forks.empty())
                out << "No forks.\n";
            else
            {
                out << std::setw(15) << "FORKED BLOCK"
                    << std::setw(30) << "FORKING BLOCK ID"
                    << std::setw(30) << "SIGNING DELEGATE"
                    << std::setw(15) << "TXN COUNT"
                    << std::setw(10) << "SIZE"
                    << std::setw(20) << "TIMESTAMP"
                    << std::setw(10) << "LATENCY"
                    << std::setw(8) << "VALID"
                    << std::setw(20) << "IN CURRENT CHAIN"
                    << "\n" << std::string(158, '-') << "\n";

                for (const auto& fork : forks)
                {
                    out << std::setw(15) << fork.first << "\n";

                    for (const auto& tine : fork.second)
                    {
                        out << std::setw(45) << fc::variant(tine.block_id).as_string();

                        auto delegate_entry = client->get_chain()->get_account_entry(tine.signing_delegate);
                        if (delegate_entry.valid() && delegate_entry->name.size() < 29)
                            out << std::setw(30) << delegate_entry->name;
                        else if (tine.signing_delegate > 0)
                            out << std::setw(30) << std::string("Delegate ID ") + fc::variant(tine.signing_delegate).as_string();
                        else
                            out << std::setw(30) << "Unknown Delegate";

                        out << std::setw(15) << tine.transaction_count
                            << std::setw(10) << tine.size
                            << std::setw(20) << pretty_timestamp(tine.timestamp)
                            << std::setw(10) << tine.latency.to_seconds()
                            << std::setw(8);

                        if (tine.is_valid.valid()) {
                            if (*tine.is_valid) {
                                out << "YES";
                            }
                            else {
                                out << "NO";
                                if (tine.invalid_reason.valid())
                                    invalid_reasons[tine.block_id] = tine.invalid_reason->to_detail_string();
                                else
                                    invalid_reasons[tine.block_id] = "No reason given.";
                            }
                        }
                        else
                            out << "N/A";

                        out << std::setw(20);
                        if (tine.is_current_fork)
                            out << "YES";
                        else
                            out << "NO";

                        out << "\n";
                    }
                }

                if (invalid_reasons.size() > 0) {
                    out << "REASONS FOR INVALID BLOCKS\n";

                    for (const auto& excuse : invalid_reasons)
                        out << fc::variant(excuse.first).as_string() << ": " << excuse.second << "\n";
                }
            }
        }

        void PrintResult::f_blockchain_list_pending_transactions(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto transactions = result.as<vector<std::pair<thinkyoung::blockchain::TransactionIdType, thinkyoung::blockchain::SignedTransaction>>>();

            if (transactions.empty())
            {
                out << "No pending transactions.\n";
            }
            else
            {
                out << std::setw(15) << "TXN ID"
                    << std::setw(20) << "EXPIRES"
                    << std::setw(10) << "SIZE"
                    << std::setw(25) << "OPERATION COUNT"
                    << std::setw(25) << "SIGNATURE COUNT"
                    << "\n" << std::string(100, '-') << "\n";

                for (const auto& transaction : transactions)
                {
                    if (FILTER_OUTPUT_FOR_TESTS)
                    {
                        out << std::setw(15) << "<d-ign>" << string(transaction.second.id()).substr(0, 8) << "</d-ign>";
                        out << std::setw(20) << "<d-ign>" << string(transaction.second.expiration) << "</d-ign>";
                    }
                    else
                    {
                        out << std::setw(15) << string(transaction.second.id()).substr(0, 8);
                        out << std::setw(20) << string(transaction.second.expiration);
                    }

                    out << std::setw(10) << transaction.second.data_size()
                        << std::setw(25) << transaction.second.operations.size()
                        << std::setw(25) << transaction.second.signatures.size()
                        << "\n";
                }
            }
        }


        void PrintResult::f_network_list_potential_peers(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto peers = result.as<std::vector<net::PotentialPeerEntry>>();
            out << std::setw(25) << "ENDPOINT";
            out << std::setw(25) << "LAST SEEN";
            out << std::setw(25) << "LAST CONNECT ATTEMPT";
            out << std::setw(30) << "SUCCESSFUL CONNECT ATTEMPTS";
            out << std::setw(30) << "FAILED CONNECT ATTEMPTS";
            out << std::setw(35) << "LAST CONNECTION DISPOSITION";
            out << std::setw(30) << "LAST ERROR";

            out << "\n";
            for (const auto& peer : peers)
            {
                out << std::setw(25) << string(peer.endpoint);
                out << std::setw(25) << pretty_timestamp(peer.last_seen_time);
                out << std::setw(25) << pretty_timestamp(peer.last_connection_attempt_time);
                out << std::setw(30) << peer.number_of_successful_connection_attempts;
                out << std::setw(30) << peer.number_of_failed_connection_attempts;
                out << std::setw(35) << string(peer.last_connection_disposition);
                out << std::setw(30) << (peer.last_error ? peer.last_error->to_detail_string() : "none");

                out << "\n";
            }
        }


        void PrintResult::f_blockchain_get_block(std::ostream& out, const fc::variants& arguments, const fc::variant& result, ClientRawPtr client)
        {
            auto block = result.as<fc::mutable_variant_object>();
            if (!block["previous"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                block["previous"] = "<d-ign>" + block["previous"].as_string() + "</d-ign>";
            if (!block["next_secret_hash"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                block["next_secret_hash"] = "<d-ign>" + block["next_secret_hash"].as_string() + "</d-ign>";
            if (!block["delegate_signature"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                block["delegate_signature"] = "<d-ign>" + block["delegate_signature"].as_string() + "</d-ign>";
            if (!block["random_seed"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                block["random_seed"] = "<d-ign>" + block["random_seed"].as_string() + "</d-ign>";
            if (!block["processing_time"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                block["processing_time"] = "<d-ign>" + block["processing_time"].as_string() + "</d-ign>";
            out << fc::json::to_pretty_string(block) << "\n";
        }
    }
}
