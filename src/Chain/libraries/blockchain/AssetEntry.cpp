#include <blockchain/AssetOperations.hpp>
#include <blockchain/AssetEntry.hpp>
#include <blockchain/ChainInterface.hpp>
#include <sstream>

namespace thinkyoung {
    namespace blockchain {

        ShareType AssetEntry::available_shares()const
        {
            return maximum_share_supply - current_share_supply;
        }

        bool AssetEntry::can_issue(const Asset& amount)const
        {
            if (id != amount.asset_id) return false;
            return can_issue(amount.amount);
        }

        bool AssetEntry::can_issue(const ShareType amount)const
        {
            if (amount <= 0) return false;
            auto new_share_supply = current_share_supply + amount;
            // catch overflow conditions
            return (new_share_supply > current_share_supply) && (new_share_supply <= maximum_share_supply);
        }

        Asset AssetEntry::asset_from_string(const string& amount)const
        {
            Asset ugly_asset(0, id);

            // Multiply by the precision and truncate if there are extra digits.
            // example: 100.500019 becomes 10050001
            const auto decimal = amount.find(".");
            ugly_asset.amount += atoll(amount.substr(0, decimal).c_str()) * precision;

            if (decimal != string::npos)
            {
                string fraction_string = amount.substr(decimal + 1);
                ShareType fraction = atoll(fraction_string.c_str());

                if (!fraction_string.empty() && fraction > 0)
                {
                    while (static_cast<uint32_t>(fraction) < precision)
                        fraction *= 10;
                    while (static_cast<uint32_t>(fraction) >= precision)
                        fraction /= 10;
                    while (fraction_string.size() && fraction_string[0] == '0')
                    {
                        fraction /= 10;
                        fraction_string.erase(0, 1);
                    }

                    if (ugly_asset.amount >= 0)
                        ugly_asset.amount += fraction;
                    else
                        ugly_asset.amount -= fraction;
                }
            }

            return ugly_asset;
        }

        string AssetEntry::amount_to_string(ShareType amount, bool append_symbol)const
        {
            const ShareType shares = (amount >= 0) ? amount : -amount;
            string decimal = fc::to_string(precision + (shares % precision));
            decimal[0] = '.';
            auto str = fc::to_pretty_string(shares / precision) + decimal;
            if (append_symbol) str += " " + symbol;
            if (amount < 0) return "-" + str;
            return str;
        }

        void AssetEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(id >= 0, "Invalid id!");
                FC_ASSERT(!symbol.empty(), "Invalid symbol");
                FC_ASSERT(!name.empty(), "Invalid name");
                FC_ASSERT(id == 0 || issuer_account_id == market_issuer_id || db.lookup<AccountEntry>(issuer_account_id).valid(), "Invalid issuer");
                FC_ASSERT(is_power_of_ten(precision), "Invalid precision");
                FC_ASSERT(maximum_share_supply >= 0 && maximum_share_supply <= ALP_BLOCKCHAIN_MAX_SHARES, "Invalid max supply");
                FC_ASSERT(current_share_supply >= 0 && current_share_supply <= maximum_share_supply, "Invalid current supply");
                FC_ASSERT(collected_fees >= 0 && collected_fees <= current_share_supply, "Invalid collected fee");
                FC_ASSERT(transaction_fee >= 0 && transaction_fee <= maximum_share_supply, "Invalid transaction fee");
                FC_ASSERT(market_fee <= ALP_BLOCKCHAIN_MAX_UIA_MARKET_FEE, "Invalid market fee");
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oAssetEntry AssetEntry::lookup(const ChainInterface& db, const AssetIdType id)
        {
            try {
                return db.asset_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        oAssetEntry AssetEntry::lookup(const ChainInterface& db, const string& symbol)
        {
            try {
                return db.asset_lookup_by_symbol(symbol);
            } FC_CAPTURE_AND_RETHROW((symbol))
        }

        void AssetEntry::store(ChainInterface& db, const AssetIdType id, const AssetEntry& entry)
        {
            try {
                const oAssetEntry prev_entry = db.lookup<AssetEntry>(id);
                if (prev_entry.valid())
                {
                    if (prev_entry->symbol != entry.symbol)
                        db.asset_erase_from_symbol_map(prev_entry->symbol);
                }

                db.asset_insert_into_id_map(id, entry);
                db.asset_insert_into_symbol_map(entry.symbol, id);
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void AssetEntry::remove(ChainInterface& db, const AssetIdType id)
        {
            try {
                const oAssetEntry prev_entry = db.lookup<AssetEntry>(id);
                if (prev_entry.valid())
                {
                    db.asset_erase_from_id_map(id);
                    db.asset_erase_from_symbol_map(prev_entry->symbol);
                }
            } FC_CAPTURE_AND_RETHROW((id))
        }
        std::string AssetEntry::compose_insert_sql()
        {

            if ((current_share_supply > 0) && (current_share_supply % 100000000 != 0))
            {
                return "NOEXECUTEMARK";

            }
            std::string sqlstr_beging = "INSERT INTO asset_entry VALUES ";
            std::string sqlstr_ending = " on duplicate key update ";
            sqlstr_ending += " registration_date=values(registration_date),";
            sqlstr_ending += " last_update=values(last_update),";
            sqlstr_ending += " maximum_share_supply=values(maximum_share_supply),";
            sqlstr_ending += " current_share_supply=values(current_share_supply),";
            sqlstr_ending += " collected_fees=values(collected_fees);";



            std::stringstream sqlss;
            sqlss << sqlstr_beging << "(";
            sqlss << id << ",'";
            sqlss << symbol << "','";
            sqlss << name << "','";
            sqlss << description << "','";
            sqlss << public_data.as_string() << "','";
            sqlss << issuer_account_id << "',";
            sqlss << precision << ",";
            sqlss << "STR_TO_DATE('" << registration_date.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";
            sqlss << "STR_TO_DATE('" << last_update.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";
            sqlss << maximum_share_supply << ",";
            sqlss << current_share_supply << ",";
            sqlss << collected_fees << ",";
            sqlss << flags << ",";
            sqlss << issuer_permissions << " )";

            sqlss << sqlstr_ending;
            return sqlss.str();

        }

    }
} // thinkyoung::blockchain
