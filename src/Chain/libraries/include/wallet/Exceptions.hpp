#pragma once

#include <fc/exception/exception.hpp>

namespace thinkyoung {
    namespace wallet {

        FC_DECLARE_EXCEPTION(wallet_exception, 20000, "wallet error");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_password, thinkyoung::wallet::wallet_exception, 20001, "invalid password");
        FC_DECLARE_DERIVED_EXCEPTION(wallet_locked, thinkyoung::wallet::wallet_exception, 20002, "wallet locked");
        FC_DECLARE_DERIVED_EXCEPTION(wallet_already_exists, thinkyoung::wallet::wallet_exception, 20003, "wallet already exists");
        FC_DECLARE_DERIVED_EXCEPTION(no_such_wallet, thinkyoung::wallet::wallet_exception, 20004, "wallet does not exist");
        FC_DECLARE_DERIVED_EXCEPTION(unknown_receive_account, thinkyoung::wallet::wallet_exception, 20005, "unknown receive account");
        FC_DECLARE_DERIVED_EXCEPTION(unknown_account, thinkyoung::wallet::wallet_exception, 20006, "unknown account");
        FC_DECLARE_DERIVED_EXCEPTION(wallet_closed, thinkyoung::wallet::wallet_exception, 20007, "wallet closed");
        FC_DECLARE_DERIVED_EXCEPTION(negative_bid, thinkyoung::wallet::wallet_exception, 20008, "negative bid");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_price, thinkyoung::wallet::wallet_exception, 20009, "invalid price");
        FC_DECLARE_DERIVED_EXCEPTION(insufficient_funds, thinkyoung::wallet::wallet_exception, 20010, "insufficient funds");
        FC_DECLARE_DERIVED_EXCEPTION(unknown_market_order, thinkyoung::wallet::wallet_exception, 20011, "unknown market order");
        FC_DECLARE_DERIVED_EXCEPTION(fee_greater_than_amount, thinkyoung::wallet::wallet_exception, 20012, "fee greater than amount");
        FC_DECLARE_DERIVED_EXCEPTION(unknown_address, thinkyoung::wallet::wallet_exception, 20013, "unknown address");
        FC_DECLARE_DERIVED_EXCEPTION(brain_key_too_short, thinkyoung::wallet::wallet_exception, 20014, "brain key is too short");
        FC_DECLARE_DERIVED_EXCEPTION(password_too_short, thinkyoung::wallet::wallet_exception, 20015, "password too short");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_timestamp, thinkyoung::wallet::wallet_exception, 20016, "invalid timestamp");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_name, thinkyoung::wallet::wallet_exception, 20017, "invalid account name");
        FC_DECLARE_DERIVED_EXCEPTION(file_already_exists, thinkyoung::wallet::wallet_exception, 20018, "file already exists");
        FC_DECLARE_DERIVED_EXCEPTION(file_not_found, thinkyoung::wallet::wallet_exception, 20019, "file not found");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_timeout, thinkyoung::wallet::wallet_exception, 20020, "invalid timeout");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_operation, thinkyoung::wallet::wallet_exception, 20021, "invalid operation");
        FC_DECLARE_DERIVED_EXCEPTION(transaction_not_found, thinkyoung::wallet::wallet_exception, 20022, "transaction not found");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_transaction_id, thinkyoung::wallet::wallet_exception, 20023, "invalid transaction id");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_asset_symbol, thinkyoung::wallet::wallet_exception, 20024, "invalid asset symbol");
        FC_DECLARE_DERIVED_EXCEPTION(duplicate_account_name, thinkyoung::wallet::wallet_exception, 20025, "duplicate account name");
        FC_DECLARE_DERIVED_EXCEPTION(not_contact_account, thinkyoung::wallet::wallet_exception, 20026, "not contact account");
        FC_DECLARE_DERIVED_EXCEPTION(memo_too_long, thinkyoung::wallet::wallet_exception, 20027, "memo too long");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_pay_rate, thinkyoung::wallet::wallet_exception, 20028, "invalid pay rate");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_wallet_name, thinkyoung::wallet::wallet_exception, 20029, "invalid wallet name");
        FC_DECLARE_DERIVED_EXCEPTION(unsupported_version, thinkyoung::wallet::wallet_exception, 20030, "unsupported version");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_fee, thinkyoung::wallet::wallet_exception, 20031, "invalid fee");
        FC_DECLARE_DERIVED_EXCEPTION(key_already_registered, thinkyoung::wallet::wallet_exception, 20032, "key belongs to other account");
        FC_DECLARE_DERIVED_EXCEPTION(double_cover, thinkyoung::wallet::wallet_exception, 20033, "double cover");
        FC_DECLARE_DERIVED_EXCEPTION(stupid_order, thinkyoung::wallet::wallet_exception, 20034, "stupid order");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_expiration_time, thinkyoung::wallet::wallet_exception, 20035, "invalid expiration time");
        FC_DECLARE_DERIVED_EXCEPTION(bad_collateral_amount, thinkyoung::wallet::wallet_exception, 20036, "bad collateral amount");
        FC_DECLARE_DERIVED_EXCEPTION(unknown_slate, thinkyoung::wallet::wallet_exception, 20037, "unknown slate");
        FC_DECLARE_DERIVED_EXCEPTION(duplicate_key, thinkyoung::wallet::wallet_exception, 20038, "duplicate key");
        FC_DECLARE_DERIVED_EXCEPTION(unauthorized_child_account, thinkyoung::wallet::wallet_exception, 20039, "unauthorized child account");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_slate, thinkyoung::wallet::wallet_exception, 20040, "invalid slate");
        FC_DECLARE_DERIVED_EXCEPTION(private_key_not_found, thinkyoung::wallet::wallet_exception, 20041, "private key not found");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_cancel, thinkyoung::wallet::wallet_exception, 20042, "invalid cancel");
        FC_DECLARE_DERIVED_EXCEPTION(account_not_registered, thinkyoung::wallet::wallet_exception, 20043, "account not registered");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_contact, thinkyoung::wallet::wallet_exception, 20044, "invalid contact");
        FC_DECLARE_DERIVED_EXCEPTION(label_already_in_use, thinkyoung::wallet::wallet_exception, 20045, "label already in use");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_delegate_pay_rate, thinkyoung::wallet::wallet_exception, 20046, "invalid_delegate_pay_rate");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_transaction_imessage_fee_coe, thinkyoung::wallet::wallet_exception, 20047, "invalid_transaction_fee_coe");
        FC_DECLARE_DERIVED_EXCEPTION(invalid_transaction_imessage_soft_length, thinkyoung::wallet::wallet_exception, 20048, "invalid_transaction_soft_max length");
        FC_DECLARE_DERIVED_EXCEPTION(imessage_size_bigger_than_soft_max_lenth, thinkyoung::wallet::wallet_exception, 20049, "imessage size bigger than soft_max_lenth");
        FC_DECLARE_DERIVED_EXCEPTION(filename_not_regular, thinkyoung::wallet::wallet_exception, 20050, "Filename isn't a regular name!");
    }
} // thinkyoung::wallet
