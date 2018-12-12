#include <blockchain/BalanceEntry.hpp>
#include <blockchain/ChainInterface.hpp>
#include <blockchain/Exceptions.hpp>
#include <sstream>

namespace thinkyoung {
    namespace blockchain {

        BalanceEntry::BalanceEntry(const Address& owner, const Asset& balance_arg, SlateIdType delegate_id)
        {
            balance = balance_arg.amount;
            condition = WithdrawCondition(WithdrawWithSignature(owner), balance_arg.asset_id, delegate_id);
        }

        set<Address> BalanceEntry::owners()const
        {
            return condition.owners();
        }

        optional<Address> BalanceEntry::owner()const
        {
            return condition.owner();
        }

        bool BalanceEntry::is_owner(const Address& addr)const
        {
            return this->owners().count(addr) > 0;
        }

        Asset BalanceEntry::get_spendable_balance(const time_point_sec at_time)const
        {
            switch (WithdrawConditionTypes(condition.type))
            {
            case withdraw_signature_type:
            case withdraw_escrow_type:
            case withdraw_multisig_type:
            {
                return Asset(balance, condition.asset_id);
            }
            default:
            {
                elog("balance_entry::get_spendable_balance() called on unsupported withdraw type!");
                return Asset();
            }
            }
            FC_ASSERT(false, "Should never get here!");
        }

        BalanceIdType BalanceEntry::get_multisig_balance_id(AssetIdType asset_id, uint32_t m, const vector<Address>& addrs)
        {
            try {
                WithdrawWithMultisig multisig_condition;
                multisig_condition.required = m;
                multisig_condition.owners = set<Address>(addrs.begin(), addrs.end());
                WithdrawCondition condition(multisig_condition, asset_id, 0);
                auto balance = BalanceEntry(condition);
                return balance.id();
            } FC_CAPTURE_AND_RETHROW((m)(addrs))
        }

        void BalanceEntry::sanity_check(const ChainInterface& db)const
        {
            try {
                FC_ASSERT(balance >= 0, "Invalid balance");
                FC_ASSERT(condition.asset_id == 0 || db.lookup<AssetEntry>(condition.asset_id).valid(), "Balance entry with invalid asset");
                FC_ASSERT(condition.slate_id == 0 || db.lookup<SlateEntry>(condition.slate_id).valid(), "Balance entry with invalid slate id");
                switch (WithdrawConditionTypes(condition.type))
                {
                case withdraw_signature_type:
                case withdraw_multisig_type:
                case withdraw_escrow_type:
                    break;
                default:
                    FC_CAPTURE_AND_THROW(invalid_withdraw_condition);
                }
            } FC_CAPTURE_AND_RETHROW((*this))
        }

        oBalanceEntry BalanceEntry::lookup(const ChainInterface& db, const BalanceIdType& id)
        {
            try {
                return db.balance_lookup_by_id(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }

        void BalanceEntry::store(ChainInterface& db, const BalanceIdType& id, const BalanceEntry& entry)
        {
            try {
                db.balance_insert_into_id_map(id, entry);
            } FC_CAPTURE_AND_RETHROW((id)(entry))
        }

        void BalanceEntry::remove(ChainInterface& db, const BalanceIdType& id)
        {
            try {
                const oBalanceEntry prev_entry = db.lookup<BalanceEntry>(id);
                if (prev_entry.valid())
                    db.balance_erase_from_id_map(id);
            } FC_CAPTURE_AND_RETHROW((id))
        }
        std::string BalanceEntry::compose_insert_sql()
        {
            std::string sqlstr_beging = "INSERT INTO balance_entry VALUES ";
            std::string sqlstr_ending = " on duplicate key update ";
            sqlstr_ending += " owner=values(owner),";
            sqlstr_ending += " balance=values(balance),";
            sqlstr_ending += " deposit_date=values(deposit_date),";
            sqlstr_ending += " last_update=values(last_update),";
            sqlstr_ending += " meta_data=values(meta_data);";

            std::stringstream sqlss;
            sqlss << sqlstr_beging << "('";
            sqlss << id().AddressToString() << "',";
            sqlss << condition.asset_id << ",";
            sqlss << condition.slate_id << ",'";
            sqlss << std::string(condition.type) << "','";       
            sqlss << std::string(condition.balance_type) << "','";
           
            sqlss << restricted_owner->AddressToString() << "',";
            sqlss << balance << ",";
            sqlss << "STR_TO_DATE('" << deposit_date.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),";
            sqlss << "STR_TO_DATE('" << last_update.to_iso_string() << "','%Y-%m-%d T %H:%i:%s'),'";
            sqlss << meta_data.as_string() << "')";
            sqlss << sqlstr_ending;
            return sqlss.str();
        }
    }
} // thinkyoung::blockchain
