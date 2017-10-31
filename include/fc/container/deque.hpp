#pragma once

#include <deque>

namespace fc {
   namespace raw {

    template<typename Stream, typename T>
    inline void pack( Stream& s, const std::deque<T>& value ) {
      pack( s, unsigned_int((uint32_t)value.size()) );
      auto itr = value.begin();
      auto end = value.end();
      while( itr != end ) {
        fc::raw::pack( s, *itr );
        ++itr;
      }
    }

    template<typename Stream, typename T>
    inline void unpack( Stream& s, std::deque<T>& value ) {
      unsigned_int size; unpack( s, size );
      FC_ASSERT( size.value*sizeof(T) < MAX_ARRAY_ALLOC_SIZE );
      value.resize(size.value);
      auto itr = value.begin();
      auto end = value.end();
      while( itr != end ) {
        fc::raw::unpack( s, *itr );
        ++itr;
      }
    }

    } // namespace raw

   template<typename T>
   void to_variant( const std::deque<T>& var,  variant& vo )
   {
       std::vector<variant> vars;
       vars.reserve(var.size());
       std::transform(var.begin(), var.end(), std::back_inserter(vars), [](const T& t) { return variant(t); });
       vo = vars;
   }
   template<typename T>
   void from_variant( const variant& var,  std::deque<T>& vo )
   {
      const variants& vars = var.get_array();
      vo.clear();
      std::transform(vars.begin(), vars.end(), std::back_inserter(vo), [](const variant& t) { return t.template as<T>(); });
   }
} // namespace fc
