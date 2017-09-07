#pragma once

#include <blockchain/AccountEntry.hpp>
#include <blockchain/AssetEntry.hpp>
#include <blockchain/BlockEntry.hpp>
#include <blockchain/ContractEntry.hpp>
#include <blockchain/ImessageOperations.hpp>
#include <blockchain/Types.hpp>
#include <client/Client.hpp>
#include <wallet/Pretty.hpp>
#include <wallet/Wallet.hpp>

#include <fc/time.hpp>

#include <string>

namespace thinkyoung {
    namespace cli {

        using namespace thinkyoung::blockchain;
        using namespace thinkyoung::wallet;

        typedef thinkyoung::client::Client const * const ClientRawPtr;

        string pretty_line(int size, char c = '=');
        string pretty_shorten(const string& str, size_t max_size);
        string pretty_timestamp(const time_point_sec timestamp);
        string pretty_path(const path& file_path);
        string pretty_age(const time_point_sec timestamp, bool from_now = false, const string& suffix = string());
        string pretty_percent(double part, double whole, int precision = 2);
        string pretty_size(uint64_t bytes);

        string pretty_info(fc::mutable_variant_object info, ClientRawPtr client);
        /**
        * Format the result of get_blockchain_info to a human-readable String
        *
        * @param info result of get_blockchain_info
        * @param client pointer to client
        *
        * @return string
        */
        string pretty_blockchain_info(fc::mutable_variant_object info, ClientRawPtr client);
        string pretty_wallet_info(fc::mutable_variant_object info, ClientRawPtr client);
        string pretty_disk_usage(fc::mutable_variant_object usage);

        string pretty_delegate_list(const vector<AccountEntry>& delegate_entrys, ClientRawPtr client);

        string pretty_block_list(const vector<BlockEntry>& block_entrys, ClientRawPtr client);

        string pretty_transaction_list(const vector<PrettyTransaction>& transactions, ClientRawPtr client);
        string pretty_transaction_list_history(const vector<PrettyTransaction>& transactions, ClientRawPtr client);
        string pretty_experimental_transaction_list(const set<PrettyTransactionExperimental>& transactions, ClientRawPtr client);

        string pretty_asset_list(const vector<AssetEntry>& asset_entrys, ClientRawPtr client);

        string pretty_account(const oAccountEntry& entry, ClientRawPtr client);

        string pretty_balances(const AccountBalanceSummaryType& balances, ClientRawPtr client);

        string pretty_vote_summary(const AccountVoteSummaryType& votes, ClientRawPtr client);

        //string pretty_order_list( const vector<std::pair<order_id_type, market_order>>& order_items, cptr client );

    }
} // thinkyoung::cli
