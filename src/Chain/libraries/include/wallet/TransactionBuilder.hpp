#pragma once

#include <blockchain/Transaction.hpp>
#include <blockchain/Exceptions.hpp>
#include <wallet/WalletEntrys.hpp>

#include <vector>
#include <map>

namespace thinkyoung {
    namespace wallet {
        namespace detail { class WalletImpl; }

        enum VoteStrategy
        {
            vote_none = 0,
            vote_all = 1,
            vote_random = 2,
            vote_recommended = 3
        };

        /**
         * @brief The transaction_builder struct simplifies the process of creating arbitrarily complex transactions.
         *
         * The builder creates a transaction and allows an arbitrary number of operations to be added to this transaction by
         * subsequent calls to the Builder Functions. The builder is capable of dealing with operations from multiple
         * accounts, and the various accounts' credits and debits are not mixed. When the builder finalizes, it balances the
         * books by crediting or charging each account with only that account's outstanding balance.
         *
         * The same owner key is used for all market operations belonging to a single account. Different accounts have
         * different market owner keys. If a new market operation is added to the transaction, and that operation belongs to
         * an account which also has order cancellations or covers in this transaction, one of that account's cancellations
         * or covers is chosen arbitrarily and the new market operations are owned by that cancellation's owner key. If no
         * cancellations nor covers for that account exist at the time of building in the new market operation, a new key
         * pair is generated for the owning account and used as the owner key for subsequent market operations under that
         * account.
         *
         * The Builder Functions expect the wallet to be open and unlocked. Do not attempt to use a transaction_builder on a
         * wallet which is not unlocked. The functions make several other assumptions which must be validated before calling
         * the appropriate function. See the documentation for a given function to know exactly what assumptions it makes.
         * Calling a Builder Function with one of its assumptions being invalid yields undefined behavior.
         */
        struct TransactionBuilder {
            TransactionBuilder(detail::WalletImpl* wimpl = nullptr)
                : _wimpl(wimpl)
            {}

            TransactionBuilder(const TransactionBuilder& builder, detail::WalletImpl* wimpl = nullptr)
            {
                required_signatures = builder.required_signatures;

                outstanding_balances = builder.outstanding_balances;
                order_keys = builder.order_keys;
                transaction_entry = builder.transaction_entry;
                _wimpl = wimpl;
            }

            WalletTransactionEntry                                                    transaction_entry;
            std::unordered_set<blockchain::Address>                                      required_signatures;

            ///Map of <owning account address, asset ID> to that account's balance in that asset ID
            std::map<std::pair<blockchain::Address, AssetIdType>, ShareType>          outstanding_balances;

            ///Map of account address to key owning that account's market transactions
            std::map<blockchain::Address, PublicKeyType>                               order_keys;




            void  set_wallet_implementation(std::unique_ptr<detail::WalletImpl>& wimpl);

            /**
             * @brief Look up the market transaction owner key used for a particular account
             * @param account_address Account owner key address to look up
             * @param account_name Account name to generate new owner key for if necessary
             * @return The market transaction owner key used by the specified account in this transaction
             *
             * Gets the key which owns the market operations belonging to a particular account in this operation. If that
             * account has canceled or covered any orders in this transaction, the key owning the canceled order will be used
             * for the subsequent market operations. If no key yet exists for the specified account, a new key will be
             * generated to serve the purpose, and registered with the specified account's wallet.
             */
            PublicKeyType order_key_for_account(const blockchain::Address& account_address, const string& account_name);

            /**
             * \defgroup<charge_functions> Low-Level Balance Manipulation Functions
             *
             * These functions are used to manually tweak an account's balance in this transaction. This can be used to pay
             * additional fees or facilitate a public transfer between two accounts which is not validated by the normal
             * rules of the transfer functions. Generally these function should not be called directly, but they are exposed
             * in case they are useful.
             *
             * Note that calling these functions naively may result in a broken transaction, i.e. if credit_balance is called
             * without an opposing call to deduct_balance, then the transaction will attempt to pay more money to
             * account_to_credit than exists in the transaction, which will cause it to be rejected by the blockchain.
             */
            /// @{
            void deduct_balance(const blockchain::Address& account_to_charge, const blockchain::Asset& amount)
            {
                FC_ASSERT(amount.amount >= 0, "Don't deduct a negative amount. Call credit_balance instead.",
                    ("amount", amount));
                outstanding_balances[std::make_pair(account_to_charge, amount.asset_id)] -= amount.amount;
            }
            void credit_balance(const blockchain::Address& account_to_credit, const blockchain::Asset& amount)
            {
                FC_ASSERT(amount.amount >= 0, "Don't credit a negative amount. Call deduct_balance instead.",
                    ("amount", amount));
                outstanding_balances[std::make_pair(account_to_credit, amount.asset_id)] += amount.amount;
            }
            /// @}

            /**
             * \defgroup<builders> Builder Functions
             * These functions each add one operation to the transaction. They all return
             * a reference to the builder, so they can be chained together in standard
             * builder syntax:
             * @code
             * builder.cancel_market_order(id1)
             *        .cancel_market_order(id2)
             *        .submit_bid(account, quantity, symbol, price, quote_symbol);
             * @endcode
             */
            /// @{
            /**
             * @brief Register a new account on the blockchain
             * @param name Name of the newly registered account
             * @param public_data Public data for the new account
             * @param owner_key Owner key for the new account
             * @param active_key Active key for the new account. If unset, the owner key will be used
             * @param delegate_pay Delegate pay for the new account. If unset, account will not be a delegate
             * @param meta_info Extra information on registered account
             * @param paying_account Account to pay fees with
             */
            TransactionBuilder& register_account(const string& name,
                optional<variant> public_data,
                PublicKeyType owner_key,
                optional<PublicKeyType> active_key,
                optional<uint8_t> delegate_pay,
                optional<AccountMetaInfo> meta_info,
                optional<WalletAccountEntry> paying_account);
            /**
             * @brief Update a specified account on the blockchain
             * @param account The account to update
             * @param public_data The public data to set on the account
             * @param active_key The new active key to set
             * @param delegate_pay The pay this delegate requests
             * @param paying_accout The account to pay the extra fee; only required if delegate_pay is changed.
             *
             * If account is a delegate and his pay rate is reduced, paying_account must be set and is expected to be a
             * receive account. If paying_account is a delegate and his delegate pay balance is sufficient to cover the fee,
             * then the fee will be withdrawn from his pay. Otherwise, the fee will be charged to the balance for that account
             * in this transaction.
             */
            TransactionBuilder& update_account_registration(const WalletAccountEntry& account,
                optional<variant> public_data,
                optional<PublicKeyType> active_key,
                optional<uint8_t> delegate_pay,
                optional<WalletAccountEntry> paying_account);
            /**
             * @brief Transfer funds from payer to recipient
             * @param payer The account to charge
             * @param recipient The account to credit
             * @param amount The amount to credit
             * @param memo The memo to attach to the transaction notification. May be arbitrarily long
             * @param vote_method The method with which to select the delegate vote for the deposited asset
             * @param memo_sender If valid, the recipient will see the transaction as being from this sender instead of the
             * payer.
             *
             * payer is expected to be a receive account.
             * If set, memo_sender is expected to be a receive account.
             *
             * If recipient is a public account, a public deposit will be made to his active address; otherwise, a TITAN
             * transaction will be used.
             *
             * This method will create a transaction notice message, which will be completed after sign() is called.
             */
            TransactionBuilder& deposit_asset(const WalletAccountEntry& payer,
                const AccountEntry& recipient,
                const Asset& amount,
                const string& memo,
                fc::optional<string> memo_sender = fc::optional<string>(),
                const string& alp_account = "");

            /**
             * @brief Transfer funds from payer to a raw address
             * @param payer The account to charge
             * @param to_addr The raw address to credit
             * @param amount The amount to credit
             * @param memo A memo for your entrys
             * @param vote_method The method with which to select the delegate vote for the deposited asset
             *
             * This method will create a transaction notice message, which will be completed after sign() is called.
             * TODO can we send notices to raw addresses yet?
             */
            TransactionBuilder& deposit_asset_to_address(const WalletAccountEntry& payer,
                const Address& to_addr,
                const Asset& amount,
                const string& memo);

            TransactionBuilder& deposit_asset_with_escrow(const WalletAccountEntry& payer,
                const AccountEntry& recipient,
                const AccountEntry& escrow_agent,
                DigestType agreement,
                const Asset& amount,
                const string& memo,
                fc::optional<PublicKeyType> memo_sender = fc::optional<PublicKeyType>(),
                const string& alp_account = "");

            TransactionBuilder& release_escrow(const AccountEntry& payer,
                const Address& escrow_account,
                const Address& released_by_address,
                ShareType amount_to_sender,
                ShareType amount_to_receiver);

            TransactionBuilder& deposit_asset_to_multisig(const Asset& amount,
                const string& from_name,
                uint32_t m,
                const vector<Address>& addresses);

            TransactionBuilder& withdraw_from_balance(const BalanceIdType& from,
                const ShareType amount);

            TransactionBuilder& deposit_to_balance(const BalanceIdType& to,
                const Asset& amount);





            TransactionBuilder& update_signing_key(const string& authorizing_account_name,
                const string& delegate_name,
                const PublicKeyType& signing_key);

            TransactionBuilder& update_asset(const string& symbol,
                const optional<string>& name,
                const optional<string>& description,
                const optional<variant>& public_data,
                const optional<double>& maximum_share_supply,
                const optional<uint64_t>& precision,
                const ShareType issuer_fee,
                double market_fee,
                uint32_t flags,
                uint32_t issuer_perms,
                const optional<AccountIdType> issuer_account_id,
                uint32_t required_sigs,
                const vector<Address>& authority
                );

            /**
             * @brief Balance the books and pay the fees
             *
             * All balances are leveled to zero by withdrawing or depositing as necessary. A fee is deducted arbitrarily from
             * the available balances, preferring XTS, or withdrawn from an arbitrary account in the transaction if no
             * sufficiently positive balances are available. A single slate is chosen using vote_recommended for all deposits.
             *
             * This function should be called only once, at the end of the builder function calls. Calling it multiple times
             * may cause attempts to over-withdraw balances.
             */
            TransactionBuilder& finalize(const bool pay_fee = true, const VoteStrategy strategy = vote_none);
            /// @}

            /**
             * @brief Sign the final transaction, and return it.
             *
             *This function operates on best-effort; if the supplied wallet does not have the keys necessary to fill all
             *signature requirements, this function will apply all signatures for which keys are available and return the
             *partially signed transaction. To determine if all necessary signatures are present, use the is_signed() method.
             */
            WalletTransactionEntry& sign();
            bool is_signed() const
            {
                return required_signatures.size() == trx.signatures.size();
            }



        private:
            detail::WalletImpl* _wimpl;
            //Shorthand name for the signed_transaction
            SignedTransaction& trx = transaction_entry.trx;


            std::map<AssetIdType, ShareType> all_positive_balances() {
                std::map<AssetIdType, ShareType> balances;

                //outstanding_balance is pair<pair<account address, asset ID>, share_type>
                for (auto& outstanding_balance : outstanding_balances)
                    if (outstanding_balance.second > 0)
                        balances[outstanding_balance.first.second] += outstanding_balance.second;

                return balances;
            }
            void pay_fee();
            bool withdraw_fee();
        };

        typedef std::shared_ptr<TransactionBuilder> TransactionBuilderPtr;
    }
} //namespace thinkyoung::wallet

FC_REFLECT_ENUM(thinkyoung::wallet::VoteStrategy,
    (vote_none)
    (vote_all)
    (vote_random)
    (vote_recommended)
    )
    FC_REFLECT(thinkyoung::wallet::TransactionBuilder, (transaction_entry)(required_signatures)(outstanding_balances))
