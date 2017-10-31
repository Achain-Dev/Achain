#pragma once

#include <blockchain/Types.hpp>

#include <fc/uint128.hpp>

namespace thinkyoung {
    namespace blockchain {

        struct Price;

        /**
         *  An asset is a 64-bit amount of shares, and an
         *  asset_id specifying the type of shares.
         */
        struct Asset
        {
            Asset() :amount(0), asset_id(0){}
            explicit Asset(ShareType a, AssetIdType u = 0)
                :amount(a), asset_id(u){}

            Asset& operator += (const Asset& o);
            Asset& operator -= (const Asset& o);
            Asset  operator /  (uint64_t constant)const
            {
                Asset tmp(*this);
                tmp.amount /= constant;
                return tmp;
            }
            Asset operator-()const { return Asset(-amount, asset_id); }

            operator std::string()const;

            ShareType     amount;
            AssetIdType  asset_id;
        };

        /**
         *  A price is the result of dividing 2 asset classes.  It is
         *  a 128-bit decimal fraction with denominator FC_REAL128_PRECISION
         *  together with units specifying the two asset ID's.
         *
         *  -1 is considered to be infinity.
         */
        struct Price
        {
            static const fc::uint128& one();
            static const fc::uint128& infinite();

            Price() {}
            Price(const fc::uint128_t& r, AssetIdType base, AssetIdType quote)
                :ratio(r), base_asset_id(base), quote_asset_id(quote){}

            Price(const std::string& s);
            Price(double a, AssetIdType quote, AssetIdType base);
            int set_ratio_from_string(const std::string& ratio_str);
            std::string ratio_string()const;
            operator std::string()const;
            explicit operator double()const;
            bool is_infinite() const;

            fc::uint128_t ratio; // This is a DECIMAL FRACTION with denominator equal to ALP_PRICE_PRECISIONI

            std::pair<AssetIdType, AssetIdType> asset_pair()const { return std::make_pair(quote_asset_id, base_asset_id); }

            AssetIdType base_asset_id;
            AssetIdType quote_asset_id;
        };
        typedef optional<Price> oprice;

        inline bool operator == (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return l.amount == r.amount;
        }
        inline bool operator != (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return l.amount != r.amount;
        }
        inline bool operator <  (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return l.amount < r.amount;
        }
        inline bool operator >  (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return l.amount > r.amount;
        }
        inline bool operator <= (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return l.asset_id == r.asset_id && l.amount <= r.amount;
        }
        inline bool operator >= (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return l.asset_id == r.asset_id && l.amount >= r.amount;
        }
        inline Asset operator +  (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return Asset(l) += r;
        }
        inline Asset operator -  (const Asset& l, const Asset& r) {
            FC_ASSERT(l.asset_id ==
                r.asset_id, "they are not same kind of asset."); return Asset(l) -= r;
        }

        inline bool operator == (const Price& l, const Price& r) { return l.ratio == r.ratio; }

        inline bool operator != (const Price& l, const Price& r) {
            return l.ratio != r.ratio ||
                l.base_asset_id != r.base_asset_id ||
                l.quote_asset_id != r.quote_asset_id;
        }

        Price operator *  (const Price& l, const Price& r);

        inline bool operator <  (const Price& l, const Price& r)
        {
            if (l.quote_asset_id < r.quote_asset_id) return true;
            if (l.quote_asset_id > r.quote_asset_id) return false;
            if (l.base_asset_id < r.base_asset_id) return true;
            if (l.base_asset_id > r.base_asset_id) return false;
            return l.ratio <  r.ratio;
        }
        inline bool operator >(const Price& l, const Price& r)
        {
            if (l.quote_asset_id > r.quote_asset_id) return true;
            if (l.quote_asset_id < r.quote_asset_id) return false;
            if (l.base_asset_id > r.base_asset_id) return true;
            if (l.base_asset_id < r.base_asset_id) return false;
            return l.ratio >  r.ratio;
        }
        inline bool operator <= (const Price& l, const Price& r) { return l.ratio <= r.ratio && l.asset_pair() == r.asset_pair(); }
        inline bool operator >= (const Price& l, const Price& r) { return l.ratio >= r.ratio && l.asset_pair() == r.asset_pair(); }

        /**
         *  A price will reorder the asset types such that the
         *  asset type with the lower enum value is always the
         *  denominator.  Therefore  thinkyoung/usd and  usd/thinkyoung will
         *  always result in a price measured in usd/thinkyoung because
         *  bitasset_id_type::bit_shares < bitasset_id_type::bit_usd.
         */
        Price operator / (const Asset& a, const Asset& b);
        Asset operator / (const Asset& a, const Price& b);

        /**
         *  Assuming a.type is either the numerator.type or denominator.type in
         *  the price equation, return the number of the other asset type that
         *  could be exchanged at price p.
         *
         *  ie:  p = 3 usd/thinkyoung & a = 4 thinkyoung then result = 12 usd
         *  ie:  p = 3 usd/thinkyoung & a = 4 usd then result = 1.333 thinkyoung
         */
        Asset operator * (const Asset& a, const Price& p);

        Asset to_asset(AssetIdType asset_id, uint64_t asset_precision, double double_amount);
    }
} // thinkyoung::blockchain

namespace fc
{
    //  void to_variant( const thinkyoung::blockchain::asset& var,  variant& vo );
    //  void from_variant( const variant& var,  thinkyoung::blockchain::asset& vo );
    void to_variant(const thinkyoung::blockchain::Price& var, variant& vo);
    void from_variant(const variant& var, thinkyoung::blockchain::Price& vo);
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT(thinkyoung::blockchain::Price, (ratio)(quote_asset_id)(base_asset_id));
FC_REFLECT(thinkyoung::blockchain::Asset, (amount)(asset_id));
