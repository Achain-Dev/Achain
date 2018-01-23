#pragma once

#include <blockchain/Types.hpp>
#include <fc/time.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct GenesisAsset
        {
            string      symbol;
            string      name;
            string      description;
            uint64_t    precision = 1;
        };

        struct GenesisDelegate
        {
            string          name;
            PublicKeyType owner;
        };

        struct GenesisBalance
        {
            string       raw_address;
            ShareType   balance = 0;
        };

        struct GenesisReserveBalances
        {
            fc::time_point_sec       start_time;
            uint32_t                 duration_days = 0;
            vector<GenesisBalance>  reserve_balances;
        };

        struct GenesisState
        {
            fc::time_point_sec       timestamp;
            vector<GenesisAsset>    market_assets;
            vector<GenesisDelegate> delegates;
            vector<GenesisBalance>  initial_balances;
            GenesisReserveBalances sharedrop_balances;
        };

    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::GenesisAsset, (symbol)(name)(description)(precision))
FC_REFLECT(thinkyoung::blockchain::GenesisDelegate, (name)(owner))
FC_REFLECT(thinkyoung::blockchain::GenesisBalance, (raw_address)(balance))
FC_REFLECT(thinkyoung::blockchain::GenesisReserveBalances, (start_time)(duration_days)(reserve_balances))
FC_REFLECT(thinkyoung::blockchain::GenesisState, (timestamp)(market_assets)(delegates)(initial_balances)(sharedrop_balances))
