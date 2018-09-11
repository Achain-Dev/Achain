#pragma once

#include <blockchain/Types.hpp>

namespace thinkyoung {
    namespace blockchain {
    
        struct SlateEntry;
        typedef optional<SlateEntry> oSlateEntry;
        
        class ChainInterface;
        struct SlateEntry {
            set<AccountIdType> slate;
            //vector<AccountIdType> duplicate_slate;
            
            SlateIdType id()const;
            void sanity_check(const ChainInterface&)const;
            static oSlateEntry lookup(const ChainInterface&, const SlateIdType);
            static void store(ChainInterface&, const SlateIdType, const SlateEntry&);
            static void remove(ChainInterface&, const SlateIdType);
        };
        
        class SlateDbInterface {
            friend struct SlateEntry;
            
            virtual oSlateEntry slate_lookup_by_id(const SlateIdType)const = 0;
            virtual void slate_insert_into_id_map(const SlateIdType, const SlateEntry&) = 0;
            virtual void slate_erase_from_id_map(const SlateIdType) = 0;
        };
        
    }
} // thinkyoung::blockchain

FC_REFLECT(thinkyoung::blockchain::SlateEntry, (slate))
