#include <blockchain/Time.hpp>
#include <cli/Pretty.hpp>
#include <cli/locale.hpp>

#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace thinkyoung {
    namespace cli {

        bool FILTER_OUTPUT_FOR_TESTS = false;

        string pretty_line(int size, char c)
        {
            return string(size, c);
        }

        string pretty_shorten(const string& str, size_t max_size)
        {
            if (str.size() > max_size)
                return str.substr(0, max_size - 3) + "...";
            return str;
        }

        string pretty_timestamp(const time_point_sec timestamp)
        {
            if (FILTER_OUTPUT_FOR_TESTS)
                return "<d-ign>" + timestamp.to_iso_string() + "</d-ign>";
            return timestamp.to_iso_string();
        }

        string pretty_path(const path& file_path)
        {
            if (FILTER_OUTPUT_FOR_TESTS)
                return "<d-ign>" + file_path.generic_string() + "</d-ign>";
            return file_path.generic_string();
        }

        string pretty_age(const time_point_sec timestamp, bool from_now, const string& suffix)
        {
            string str;

            if (from_now)
            {
                const auto now = blockchain::now();
                if (suffix.empty())
                    str = fc::get_approximate_relative_time_string(timestamp, now);
                else
                    str = fc::get_approximate_relative_time_string(timestamp, now, " " + suffix);
            }
            else
                str = fc::get_approximate_relative_time_string(timestamp);
            if (FILTER_OUTPUT_FOR_TESTS)
                return "<d-ign>" + str + "</d-ign>";
            else
                return str;
        }

        string pretty_percent(double part, double whole, int precision)
        {
            if (part < 0 ||
                whole < 0 ||
                precision < 0 ||
                part > whole)
                return "? %";
            if (whole <= 0)
                return "N/A";
            double percent = 100 * part / whole;
            std::stringstream ss;
            ss << std::setprecision(precision) << std::fixed << percent << " %";
            return ss.str();
        }

        string pretty_size(uint64_t bytes)
        {
            static const vector<string> suffixes{ "B", "KiB", "MiB", "GiB" };
            uint8_t suffix_pos = 0;
            double count = bytes;
            while (count >= 1024 && suffix_pos < suffixes.size())
            {
                count /= 1024;
                ++suffix_pos;
            }
            uint64_t size = round(count);

            return std::to_string(size) + " " + suffixes.at(suffix_pos);
        }

        string pretty_info(fc::mutable_variant_object info, ClientRawPtr client)
        {
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            if (!info["blockchain_head_block_timestamp"].is_null())
            {
                const auto timestamp = info["blockchain_head_block_timestamp"].as<time_point_sec>();
                info["blockchain_head_block_timestamp"] = pretty_timestamp(timestamp);

                if (!info["blockchain_head_block_age"].is_null())
                    info["blockchain_head_block_age"] = pretty_age(timestamp, true, "old");
            }

            if (!info["blockchain_average_delegate_participation"].is_null())
            {
                const auto participation = info["blockchain_average_delegate_participation"].as<double>();
                info["blockchain_average_delegate_participation"] = pretty_percent(participation, 100);
            }

            if (!info["blockchain_share_supply"].is_null())
            {
                const auto share_supply = info["blockchain_share_supply"].as<ShareType>();
                info["blockchain_share_supply"] = client->get_chain()->to_pretty_asset(Asset(share_supply));
            }

            optional<time_point_sec> next_round_timestamp;
            if (!info["blockchain_next_round_timestamp"].is_null())
            {
                next_round_timestamp = info["blockchain_next_round_timestamp"].as<time_point_sec>();
                info["blockchain_next_round_timestamp"] = pretty_timestamp(*next_round_timestamp);

                if (!info["blockchain_next_round_time"].is_null())
                    info["blockchain_next_round_time"] = "at least " + pretty_age(*next_round_timestamp, true);
            }

            const auto data_dir = info["client_data_dir"].as<path>();
            info["client_data_dir"] = pretty_path(data_dir);

            if (!info["client_version"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                info["client_version"] = "<d-ign>" + info["client_version"].as_string() + "</d-ign>";

            if (!info["ntp_time"].is_null())
            {
                const auto ntp_time = info["ntp_time"].as<time_point_sec>();
                info["ntp_time"] = pretty_timestamp(ntp_time);

                if (!info["ntp_time_error"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                    info["ntp_time_error"] = "<d-ign>" + info["ntp_time_error"].as_string() + "</d-ign>";
            }

            if (!info["wallet_unlocked_until_timestamp"].is_null())
            {
                const auto unlocked_until_timestamp = info["wallet_unlocked_until_timestamp"].as<time_point_sec>();
                info["wallet_unlocked_until_timestamp"] = pretty_timestamp(unlocked_until_timestamp);

                if (!info["wallet_unlocked_until"].is_null())
                    info["wallet_unlocked_until"] = pretty_age(unlocked_until_timestamp, true);
            }

            if (!info["wallet_last_scanned_block_timestamp"].is_null())
            {
                const auto last_scanned_block_timestamp = info["wallet_last_scanned_block_timestamp"].as<time_point_sec>();
                info["wallet_last_scanned_block_timestamp"] = pretty_timestamp(last_scanned_block_timestamp);
            }

            if (!info["wallet_scan_progress"].is_null())
            {
                const auto scan_progress = info["wallet_scan_progress"].as<float>();
                info["wallet_scan_progress"] = pretty_percent(scan_progress, 1);
            }

            if (!info["wallet_next_block_production_timestamp"].is_null())
            {
                const auto next_block_timestamp = info["wallet_next_block_production_timestamp"].as<time_point_sec>();
                info["wallet_next_block_production_timestamp"] = pretty_timestamp(next_block_timestamp);
                if (!next_round_timestamp.valid() || next_block_timestamp < *next_round_timestamp)
                {
                    if (!info["wallet_next_block_production_time"].is_null())
                        info["wallet_next_block_production_time"] = pretty_age(next_block_timestamp, true);
                }
                else
                {
                    if (!info["wallet_next_block_production_time"].is_null())
                        info["wallet_next_block_production_time"] = "at least " + pretty_age(*next_round_timestamp, true);
                }
            }

            out << fc::json::to_pretty_string(info) << "\n";
            return out.str();
        }

        string pretty_blockchain_info(fc::mutable_variant_object info, ClientRawPtr client)
        {
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            if (!info["db_version"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                info["db_version"] = "<d-ign>" + info["db_version"].as_string() + "</d-ign>";

            const auto timestamp = info["genesis_timestamp"].as<time_point_sec>();
            info["genesis_timestamp"] = pretty_timestamp(timestamp);

            const auto min_fee = info["min_block_fee"].as<ShareType>();
            info["min_block_fee"] = client->get_chain()->to_pretty_asset(Asset(min_fee));

            const auto relay_fee = info["relay_fee"].as<ShareType>();
            info["relay_fee"] = client->get_chain()->to_pretty_asset(Asset(relay_fee));

            const auto max_delegate_pay_issued_per_block = info["max_delegate_pay_issued_per_block"].as<ShareType>();
            info["max_delegate_pay_issued_per_block"] = client->get_chain()->to_pretty_asset(Asset(max_delegate_pay_issued_per_block));

            const auto max_delegate_reg_fee = info["max_delegate_reg_fee"].as<ShareType>();
            info["max_delegate_reg_fee"] = client->get_chain()->to_pretty_asset(Asset(max_delegate_reg_fee));

            const auto short_symbol_asset_reg_fee = info["short_symbol_asset_reg_fee"].as<ShareType>();
            info["short_symbol_asset_reg_fee"] = client->get_chain()->to_pretty_asset(Asset(short_symbol_asset_reg_fee));

            const auto long_symbol_asset_reg_fee = info["long_symbol_asset_reg_fee"].as<ShareType>();
            info["long_symbol_asset_reg_fee"] = client->get_chain()->to_pretty_asset(Asset(long_symbol_asset_reg_fee));

            out << fc::json::to_pretty_string(info) << "\n";
            return out.str();
        }

        string pretty_wallet_info(fc::mutable_variant_object info, ClientRawPtr client)
        {
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            const auto data_dir = info["data_dir"].as<path>();
            info["data_dir"] = pretty_path(data_dir);

            if (!info["num_scanning_threads"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                info["num_scanning_threads"] = "<d-ign>" + info["num_scanning_threads"].as_string() + "</d-ign>";

            if (!info["unlocked_until_timestamp"].is_null())
            {
                const auto unlocked_until_timestamp = info["unlocked_until_timestamp"].as<time_point_sec>();
                info["unlocked_until_timestamp"] = pretty_timestamp(unlocked_until_timestamp);

                if (!info["unlocked_until"].is_null())
                    info["unlocked_until"] = pretty_age(unlocked_until_timestamp, true);
            }

            if (!info["last_scanned_block_timestamp"].is_null())
            {
                const auto last_scanned_block_timestamp = info["last_scanned_block_timestamp"].as<time_point_sec>();
                info["last_scanned_block_timestamp"] = pretty_timestamp(last_scanned_block_timestamp);
            }

            if (!info["transaction_fee"].is_null())
            {
                const auto transaction_fee = info["transaction_fee"].as<Asset>();
                info["transaction_fee"] = client->get_chain()->to_pretty_asset(transaction_fee);
            }

            if (!info["scan_progress"].is_null())
            {
                const auto scan_progress = info["scan_progress"].as<float>();
                info["scan_progress"] = pretty_percent(scan_progress, 1);
            }

            if (!info["version"].is_null() && FILTER_OUTPUT_FOR_TESTS)
                info["version"] = "<d-ign>" + info["version"].as_string() + "</d-ign>";

            out << fc::json::to_pretty_string(info) << "\n";
            return out.str();
        }

        string pretty_disk_usage(fc::mutable_variant_object usage)
        {
            std::stringstream out;
            out << std::left;

            const auto format_size = [](fc::mutable_variant_object& dict, const string& key)
            {
                if (!dict[key].is_null())
                {
                    const auto size = dict[key].as_uint64();
                    dict[key] = pretty_size(size);
                }
            };

            format_size(usage, "blockchain");
            format_size(usage, "dac_state");
            format_size(usage, "logs");
            format_size(usage, "mail_client");
            format_size(usage, "mail_server");
            format_size(usage, "network_peers");

            if (!usage["wallets"].is_null())
            {
                auto wallets = usage["wallets"].as<fc::mutable_variant_object>();
                for (const auto& item : wallets)
                    format_size(wallets, item.key());
                usage["wallets"] = wallets;
            }

            format_size(usage, "total");

            out << fc::json::to_pretty_string(usage) << "\n";
            return out.str();
        }

        string pretty_delegate_list(const vector<AccountEntry>& delegate_entrys, ClientRawPtr client)
        {
            if (delegate_entrys.empty()) return "No delegates found.\n";
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            out << std::setw(6) << "ID";
            out << std::setw(66) << "NAME (* next in line)";
            //out << std::setw( 32 ) << "NAME (* next in line)";
            out << std::setw(15) << "APPROVAL";
            out << std::setw(9) << "PRODUCED";
            out << std::setw(9) << "MISSED";
            out << std::setw(14) << "RELIABILITY";
            out << std::setw(9) << "PAY RATE";
            out << std::setw(20) << "PAY BALANCE";
            out << std::setw(12) << "LAST BLOCK";
            out << std::setw(12) << "VERSION";
            out << "\n";

            out << pretty_line(138);
            out << "\n";

            const auto current_slot_timestamp = blockchain::get_slot_start_time(blockchain::now());
            const auto head_block_timestamp = client->get_chain()->get_head_block().timestamp;
            const auto next_slot_time = std::max(current_slot_timestamp, head_block_timestamp + ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
            const auto& active_delegates = client->get_chain()->get_active_delegates();
            const auto next_slot_signee = client->get_chain()->get_slot_signee(next_slot_time, active_delegates);
            const auto next_signee_name = next_slot_signee.name;

            const auto asset_entry = client->get_chain()->get_asset_entry(AssetIdType());
            FC_ASSERT(asset_entry.valid());
            const auto share_supply = asset_entry->current_share_supply;

            for (const auto& delegate_entry : delegate_entrys)
            {
                out << std::setw(6) << delegate_entry.id;

                const auto delegate_name = delegate_entry.name;
                if (delegate_name != next_signee_name)
                    out << std::setw(66) << delegate_name;
                //out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );
                else
                    out << std::setw(66) << delegate_name + " *";
                //out << std::setw( 32 ) << pretty_shorten( delegate_name, 29 ) + " *";

                out << std::setw(15) << pretty_percent(delegate_entry.net_votes(), share_supply, 8);

                const auto num_produced = delegate_entry.delegate_info->blocks_produced;
                const auto num_missed = delegate_entry.delegate_info->blocks_missed;
                out << std::setw(9) << num_produced;
                out << std::setw(9) << num_missed;
                out << std::setw(14) << pretty_percent(num_produced, num_produced + num_missed, 2);

                out << std::setw(9) << pretty_percent(delegate_entry.delegate_info->pay_rate, 100, 0);
                const auto pay_balance = Asset(delegate_entry.delegate_info->pay_balance);
                out << std::setw(20) << client->get_chain()->to_pretty_asset(pay_balance);

                const auto last_block = delegate_entry.delegate_info->last_block_num_produced;
                out << std::setw(12) << (last_block > 0 ? std::to_string(last_block) : "NONE");

                string version;
                if (delegate_entry.public_data.is_object()
                    && delegate_entry.public_data.get_object().contains("version")
                    && delegate_entry.public_data.get_object()["version"].is_string())
                {
                    version = delegate_entry.public_data.get_object()["version"].as_string();
                }
                out << std::setw(12) << version;

                out << "\n";
            }

            return out.str();
        }

        string pretty_block_list(const vector<BlockEntry>& block_entrys, ClientRawPtr client)
        {
            if (block_entrys.empty()) return "No blocks found.\n";
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            out << std::setw(8) << "HEIGHT";
            out << std::setw(20) << "TIMESTAMP";
            out << std::setw(32) << "SIGNING DELEGATE";
            out << std::setw(8) << "# TXS";
            out << std::setw(8) << "SIZE";
            out << std::setw(8) << "LATENCY";
            out << std::setw(17) << "PROCESSING TIME";
            out << std::setw(40) << "RANDOM SEED";
            out << "\n";

            out << pretty_line(141);
            out << "\n";

            auto last_block_timestamp = block_entrys.front().timestamp;

            for (const auto& BlockEntry : block_entrys)
            {
                /* Print any missed slots */

                while (last_block_timestamp != BlockEntry.timestamp)
                {
                    last_block_timestamp -= ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
                    if (last_block_timestamp == BlockEntry.timestamp) break;

                    out << std::setw(8) << "MISSED";
                    out << std::setw(20) << pretty_timestamp(last_block_timestamp);

                    string delegate_name = "?";
                    const auto slot_entry = client->get_chain()->get_slot_entry(last_block_timestamp);
                    if (slot_entry.valid())
                    {
                        const auto delegate_entry = client->get_chain()->get_account_entry(slot_entry->index.delegate_id);
                        if (delegate_entry.valid()) delegate_name = delegate_entry->name;
                    }
                    out << std::setw(66) << delegate_name;
                    //out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );

                    out << std::setw(8) << "N/A";
                    out << std::setw(8) << "N/A";
                    out << std::setw(8) << "N/A";
                    out << std::setw(17) << "N/A";
                    out << std::setw(15) << "N/A";
                    out << '\n';
                }

                /* Print produced block */

                out << std::setw(8) << BlockEntry.block_num;
                out << std::setw(20) << pretty_timestamp(BlockEntry.timestamp);

                const auto& delegate_name = client->blockchain_get_block_signee(std::to_string(BlockEntry.block_num));

                out << std::setw(32);
                if (FILTER_OUTPUT_FOR_TESTS) out << "<d-ign>" << delegate_name << "</d-ign>";
                else
                    out << std::setw(66) << delegate_name;
                //out << pretty_shorten( delegate_name, 31 );

                out << std::setw(8) << BlockEntry.user_transaction_ids.size();
                out << std::setw(8) << BlockEntry.block_size;

                if (FILTER_OUTPUT_FOR_TESTS)
                {
                    out << std::setw(8) << "<d-ign>" << BlockEntry.latency.to_seconds() << "</d-ign>";
                    out << std::setw(17) << "<d-ign>" << BlockEntry.processing_time.count() / double(1000000) << "</d-ign>";
                    out << std::setw(40) << "<d-ign>" << std::string(BlockEntry.random_seed) << "</d-ign>";
                }
                else
                {
                    out << std::setw(8) << BlockEntry.latency.to_seconds();
                    out << std::setw(17) << BlockEntry.processing_time.count() / double(1000000);
                    out << std::setw(40) << std::string(BlockEntry.random_seed);
                }

                out << '\n';
            }

            return out.str();
        }
        string pretty_transaction_list_history(const vector<PrettyTransaction>& transactions, ClientRawPtr client)
        {
            if (transactions.empty()){
                return "No transaction found.\n";
            }
            FC_ASSERT(client != nullptr);
            const bool account_specified = !transactions.front().ledger_entries.empty()
                && transactions.front().ledger_entries.front().running_balances.size() == 1;
            std::stringstream out;
            out << std::left;
            out << std::setw(21) << "TIMESTAMP|";
            //out << std::setw(10) << "BLOCK";
            out << std::setw(73) << "FROM|";
            out << std::setw(73) << "TO|";
            out << std::setw(25) << "AMOUNT|";
            out << std::setw(21) << "FEE|";
            if (account_specified){
                out << std::setw(25) << "BALANCE|";
            }
            out << std::setw(44) << "MEMO";
            out << "\n";
            const auto line_size = !account_specified ? 270 : 294;
            //out << pretty_line(!any_group ? line_size : line_size + 2) << "\n";
            auto group = true;
            for (const auto& transaction : transactions)
            {
                size_t count = 0;
				vector<PrettyLedgerEntry>::const_reverse_iterator _citer = transaction.ledger_entries.rbegin();
               // vector<PrettyLedgerEntry>::const_iterator _citer = transaction.ledger_entries.begin();
                const auto is_pending = !transaction.is_virtual && !transaction.is_confirmed;
                out << std::setw(21) << pretty_timestamp(transaction.timestamp) << "|";
                // 		out << std::setw(10);
                // 		if (!is_pending)
                // 		{
                // 			out << transaction.block_num;
                // 		}
                // 		else if (transaction.error.valid())
                // 		{
                // 			auto name = string(transaction.error->name());
                // 			name = name.substr(0, name.find("_"));
                // 			boost::to_upper(name);
                // 			out << name.substr(0, 9);
                // 		}
                // 		else
                // 		{
                // 			out << "PENDING";
                // 		}
                out << std::setw(73) << _citer->from_account << "|";
                out << std::setw(73) << _citer->to_account << "|";
                //             out << std::setw( 20 ) << pretty_shorten( entry.from_account, 19 );
                //             out << std::setw( 20 ) << pretty_shorten( entry.to_account, 19 );
                out << std::setw(25) << client->get_chain()->to_pretty_asset(_citer->amount) << "|";

                out << std::setw(21);
                out << client->get_chain()->to_pretty_asset(transaction.fee) << "|";
                if (account_specified)
                {
                    out << std::setw(25);
                    if (!is_pending)
                    {
                        const string name = _citer->running_balances.begin()->first;
                        out << client->get_chain()->to_pretty_asset(_citer->running_balances.at(name).at(_citer->amount.asset_id)) << "|";
                    }
                    else
                    {
                        out << "N/A" << "|";
                    }
                }
                //out << std::setw(44) << pretty_shorten(_citer->memo, 43);
#ifdef WIN32
                string out_memo = UTF8ToGBK(_citer->memo);
#else
                string out_memo = _citer->memo;
#endif
                out << out_memo;
                // 		out << std::setw(8);
                // 		string str;
                // 		if (transaction.is_virtual)
                // 			str = "VIRTUAL";
                // 		else
                // 			str = string(transaction.trx_id).substr(0, 8);
                // 		if (FILTER_OUTPUT_FOR_TESTS)
                // 			out << "<d-ign>" << str << "</d-ign>";
                // 		else
                // 			out << str;
                out << "\n";
            }
            return out.str();
        }
        string pretty_transaction_list(const vector<PrettyTransaction>& transactions, ClientRawPtr client)
        {
            if (transactions.empty()) return "No transactions found.\n";
            FC_ASSERT(client != nullptr);

            const bool account_specified = !transactions.front().ledger_entries.empty()
                && transactions.front().ledger_entries.front().running_balances.size() == 1;

            auto any_group = false;
            for (const auto& transaction : transactions)
                any_group |= transaction.ledger_entries.size() > 1;

            std::stringstream out;
            out << std::left;

            if (any_group) out << " ";

            out << std::setw(20) << "TIMESTAMP";
            out << std::setw(10) << "BLOCK";
            out << std::setw(72) << "FROM";
            out << std::setw(72) << "TO";
            //     out << std::setw( 20 ) << "FROM";
            //     out << std::setw( 20 ) << "TO";
            out << std::setw(24) << "AMOUNT";
            out << std::setw(44) << "MEMO";
            if (account_specified) out << std::setw(24) << "BALANCE";
            out << std::setw(20) << "FEE";
            out << std::setw(8) << "ID";
            out << "\n";

            //const auto line_size = !account_specified ? 166 : 190;
            const auto line_size = !account_specified ? 270 : 294;
            out << pretty_line(!any_group ? line_size : line_size + 2) << "\n";

            auto group = true;
            for (const auto& transaction : transactions)
            {
                const auto prev_group = group;
                group = transaction.ledger_entries.size() > 1;
                if (group && !prev_group) out << pretty_line(line_size + 2, '-') << "\n";

                size_t count = 0;
                for (const auto& entry : transaction.ledger_entries)
                {
                    const auto is_pending = !transaction.is_virtual && !transaction.is_confirmed;

                    if (group) out << "|";
                    else if (any_group) out << " ";

                    ++count;
                    if (count == 1)
                    {
                        out << std::setw(20) << pretty_timestamp(transaction.timestamp);

                        out << std::setw(10);
                        if (!is_pending)
                        {
                            out << transaction.block_num;
                        }
                        else if (transaction.error.valid())
                        {
                            auto name = string(transaction.error->name());
                            name = name.substr(0, name.find("_"));
                            boost::to_upper(name);
                            out << name.substr(0, 9);
                        }
                        else
                        {
                            out << "PENDING";
                        }
                    }
                    else
                    {
                        out << std::setw(20) << "";
                        out << std::setw(10) << "";
                    }
                    out << std::setw(72) << entry.from_account;
                    out << std::setw(72) << entry.to_account;
                    //             out << std::setw( 20 ) << pretty_shorten( entry.from_account, 19 );
                    //             out << std::setw( 20 ) << pretty_shorten( entry.to_account, 19 );
                    out << std::setw(24) << client->get_chain()->to_pretty_asset(entry.amount);

                    //out << std::setw(44) << pretty_shorten(entry.memo, 43);
#ifdef WIN32
                    string out_memo = UTF8ToGBK(entry.memo);
#else
                    string out_memo = entry.memo;
#endif
                    out << out_memo;

                    if (account_specified)
                    {
                        out << std::setw(24);
                        if (!is_pending)
                        {
                            const string name = entry.running_balances.begin()->first;
                            out << client->get_chain()->to_pretty_asset(entry.running_balances.at(name).at(entry.amount.asset_id));
                        }
                        else
                        {
                            out << "N/A";
                        }
                    }

                    if (count == 1)
                    {
                        out << std::setw(20);
                        out << client->get_chain()->to_pretty_asset(transaction.fee);

                        out << std::setw(8);
                        string str;
                        if (transaction.is_virtual)
                            str = "VIRTUAL";
                        else
                            str = string(transaction.trx_id).substr(0, 8);
                        if (FILTER_OUTPUT_FOR_TESTS)
                            out << "<d-ign>" << str << "</d-ign>";
                        else
                            out << str;
                    }
                    else
                    {
                        out << std::setw(20) << "";
                        out << std::setw(8) << "";
                    }

                    if (group) out << "|";
                    out << "\n";
                }

                if (group) out << pretty_line(line_size + 2, '-') << "\n";
            }

            return out.str();
        }

        string pretty_experimental_transaction_list(const set<PrettyTransactionExperimental>& transactions, ClientRawPtr client)
        {
            if (transactions.empty()) return "No transactions found.\n";
            FC_ASSERT(client != nullptr);

            const vector<std::pair<string, int>> fields
            {
                { "timestamp", 20 },
                { "block", 10 },
                { "inputs", 40 },
                { "outputs", 40 },
                { "balances", 40 },
                { "notes", 32 },
                { "id", 8 }
            };

            std::stringstream out;
            out << std::left;

            map<string, int> field_widths;
            int total_width = 0;
            for (const auto& item : fields)
            {
                out << std::setw(item.second) << boost::to_upper_copy(item.first);
                field_widths[item.first] = item.second;
                total_width += item.second;
            }

            out << "\n";

            out << pretty_line(total_width) << "\n";

            for (const auto& transaction : transactions)
            {
                size_t line_count = 0;

                while (line_count < transaction.inputs.size()
                    || line_count < transaction.outputs.size()
                    || line_count < transaction.notes.size())
                {
                    out << std::setw(field_widths.at("timestamp"));
                    if (line_count == 0)
                        out << pretty_timestamp(transaction.timestamp);
                    else
                        out << "";

                    out << std::setw(field_widths.at("block"));
                    if (line_count == 0)
                    {
                        if (transaction.is_confirmed())
                        {
                            out << transaction.block_num;
                        }
                        /*
                        else if( transaction.error.valid() )
                        {
                        auto name = string( transaction.error->name() );
                        name = name.substr( 0, name.find( "_" ) );
                        boost::to_upper( name );
                        out << name.substr( 0, 9 );
                        }
                        */
                        else
                        {
                            out << "PENDING";
                        }
                    }
                    else
                    {
                        out << "";
                    }

                    out << std::setw(field_widths.at("inputs"));
                    if (line_count < transaction.inputs.size())
                    {
                        const auto& item = transaction.inputs.at(line_count);
                        const string& label = item.first;
                        const Asset& delta = item.second;
                        string input = " => " + client->get_chain()->to_pretty_asset(delta);
                        input = pretty_shorten(label, field_widths.at("inputs") - input.size() - 1) + input;
                        out << input;
                    }
                    else
                    {
                        out << "";
                    }

                    out << std::setw(field_widths.at("outputs"));
                    if (line_count < transaction.outputs.size())
                    {
                        const auto& item = transaction.outputs.at(line_count);
                        const string& label = item.first;
                        const Asset& delta = item.second;
                        string output = client->get_chain()->to_pretty_asset(delta) + " => ";
                        output = output + pretty_shorten(label, field_widths.at("outputs") - output.size() - 1);
                        out << output;
                    }
                    else
                    {
                        out << "";
                    }

                    out << std::setw(field_widths.at("balances"));
                    if (line_count < transaction.balances.size())
                    {
                        const auto& item = transaction.balances.at(line_count);
                        const string& label = item.first;
                        const Asset& delta = item.second;
                        string balance = " : " + client->get_chain()->to_pretty_asset(delta);
                        balance = pretty_shorten(label, field_widths.at("balances") - balance.size() - 1) + balance;
                        out << balance;
                    }
                    else
                    {
                        out << "";
                    }

                    out << std::setw(field_widths.at("notes"));
                    if (line_count < transaction.notes.size())
                    {
                        out << pretty_shorten(transaction.notes.at(line_count), field_widths.at("notes") - 1);
                    }
                    else
                    {
                        out << "";
                    }

                    out << std::setw(field_widths.at("id"));
                    if (line_count == 0)
                    {
                        if (FILTER_OUTPUT_FOR_TESTS)
                            out << "[redacted]";
                        else if (transaction.is_virtual())
                            out << "VIRTUAL";
                        else
                        {
                            if (FILTER_OUTPUT_FOR_TESTS)
                                out << "<d-ign>" << string(*transaction.transaction_id).substr(0, field_widths.at("id")) << "</d-ign>";
                            else
                                out << string(*transaction.transaction_id).substr(0, field_widths.at("id"));
                        }
                    }
                    else
                    {
                        out << "";
                    }

                    out << "\n";
                    ++line_count;
                }

                out << pretty_line(total_width, '-') << "\n";
            }

            return out.str();
        }

        string pretty_asset_list(const vector<AssetEntry>& asset_entrys, ClientRawPtr client)
        {
            if (asset_entrys.empty()) return "No assets found.\n";
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            out << std::setw(6) << "ID";
            out << std::setw(13) << "SYMBOL";
            out << std::setw(24) << "NAME";
            out << std::setw(48) << "DESCRIPTION";
            out << std::setw(32) << "ISSUER";
            out << std::setw(10) << "ISSUED";
            out << std::setw(28) << "SUPPLY";
            out << "\n";

            out << pretty_line(155);
            out << "\n";

            for (const auto& asset_entry : asset_entrys)
            {
                const auto asset_id = asset_entry.id;
                out << std::setw(6) << asset_id;

                out << std::setw(13) << asset_entry.symbol;
                out << std::setw(24) << pretty_shorten(asset_entry.name, 23);
                out << std::setw(48) << pretty_shorten(asset_entry.description, 47);

                const auto issuer_id = asset_entry.issuer_account_id;
                const auto supply = Asset(asset_entry.current_share_supply, asset_id);
                if (issuer_id == 0)
                {
                    out << std::setw(32) << "GENESIS";
                    out << std::setw(10) << "N/A";
                }
                else
                {
                    const auto account_entry = client->get_chain()->get_account_entry(issuer_id);
                    if (account_entry.valid())
                        out << std::setw(32) << pretty_shorten(account_entry->name, 31);
                    else
                        out << std::setw(32) << "";

                    const auto max_supply = Asset(asset_entry.maximum_share_supply, asset_id);
                    out << std::setw(10) << pretty_percent(supply.amount, max_supply.amount);
                }

                out << std::setw(28) << client->get_chain()->to_pretty_asset(supply);

                out << "\n";
            }

            return out.str();
        }

        string pretty_account(const oAccountEntry& entry, ClientRawPtr client)
        {
            if (!entry.valid()) return "No account found.\n";
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            out << "Name: " << entry->name << "\n";
            bool founder = entry->registration_date == client->get_chain()->get_genesis_timestamp();
            string registered = !founder ? pretty_timestamp(entry->registration_date) : "Genesis (Keyhotee Founder)";
            out << "Registered: " << registered << "\n";
            out << "Last Updated: " << pretty_age(entry->last_update) << "\n";
            out << "Owner Key: " << std::string(entry->owner_key) << "\n";

            out << "Active Key History:\n";
            for (const auto& key : entry->active_key_history)
            {
                out << "- " << std::string(key.second)
                    << ", last used " << pretty_age(key.first) << "\n";
            }

            if (entry->is_delegate())
            {
                const vector<AccountEntry> delegate_entrys = { *entry };
                out << "\n" << pretty_delegate_list(delegate_entrys, client) << "\n";
                out << "Block Signing Key: " << std::string(entry->signing_key()) << "\n";
            }
            else
            {
                out << "Not a delegate.\n";
            }

            if (!entry->public_data.is_null())
            {
                out << "Public Data:\n";
                out << fc::json::to_pretty_string(entry->public_data) << "\n";
            }

            return out.str();
        }

        string pretty_balances(const AccountBalanceSummaryType& balances, ClientRawPtr client)
        {
            if (balances.empty()) return "No balances found.\n";
            FC_ASSERT(client != nullptr);

            std::stringstream out;
            out << std::left;

            out << std::setw(66) << "ACCOUNT";
            //out << std::setw( 32 ) << "ACCOUNT";
            out << std::setw(28) << "BALANCE";
            out << "\n";

            out << pretty_line(60);
            out << "\n";

            for (const auto& item : balances)
            {
                const auto& account_name = item.first;

                bool first = true;
                for (const auto& asset_balance : item.second)
                {
                    if (first)
                    {
                        //out << std::setw( 32 ) << pretty_shorten( account_name, 31 );
                        out << std::setw(66) << account_name;
                        first = false;
                    }
                    else
                    {
                        out << std::setw(66) << "";
                        //out << std::setw( 32 ) << "";
                    }

                    const auto balance = Asset(asset_balance.second, asset_balance.first);
                    out << std::setw(28) << client->get_chain()->to_pretty_asset(balance);

                    out << "\n";
                }
            }

            return out.str();
        }

        string pretty_vote_summary(const AccountVoteSummaryType& votes, ClientRawPtr client)
        {
            if (votes.empty()) return "No votes found.\n";

            std::stringstream out;
            out << std::left;
            out << std::setw(66) << "DELEGATE";
            //out << std::setw( 32 ) << "DELEGATE";
            out << std::setw(24) << "VOTES";
            out << std::setw(8) << "APPROVAL";
            out << "\n";

            out << pretty_line(64);
            out << "\n";

            for (const auto& vote : votes)
            {
                const auto& delegate_name = vote.first;
                const auto votes_for = vote.second;

                //out << std::setw( 32 ) << pretty_shorten( delegate_name, 31 );
                out << std::setw(66) << delegate_name;
                out << std::setw(24) << client->get_chain()->to_pretty_asset(Asset(votes_for));
                out << std::setw(8) << std::to_string(client->get_wallet()->get_account_approval(delegate_name));

                out << "\n";
            }

            return out.str();
        }



    }
} // thinkyoung::cli
