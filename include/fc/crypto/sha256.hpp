#pragma once
#include <fc/fwd.hpp>
#include <fc/string.hpp>
#include <fc/platform_independence.hpp>
#include <fc/io/raw_fwd.hpp>

namespace fc
{

class sha256 
{
  public:
    sha256();
    explicit sha256( const string& hex_str );
    explicit sha256( const char *data, size_t size );

    string str()const;
    operator string()const;

    char*    data()const;
    size_t data_size()const { return 256 / 8; }

    static sha256 hash( const char* d, uint32_t dlen );
    static sha256 hash( const string& );
    static sha256 hash( const sha256& );

    template<typename T>
    static sha256 hash( const T& t ) 
    { 
      sha256::encoder e; 
      fc::raw::pack(e,t);
      return e.result(); 
    } 

    class encoder 
    {
      public:
        encoder();
        ~encoder();

        void write( const char* d, uint32_t dlen );
        void put( char c ) { write( &c, 1 ); }
        void reset();
        sha256 result();

      private:
        struct      impl;
        fc::fwd<impl,112> my;
    };

    template<typename T>
    inline friend T& operator<<( T& ds, const sha256& ep ) {
      ds.write( ep.data(), sizeof(ep) );
      return ds;
    }

    template<typename T>
    inline friend T& operator>>( T& ds, sha256& ep ) {
      ds.read( ep.data(), sizeof(ep) );
      return ds;
    }
    friend sha256 operator << ( const sha256& h1, uint32_t i       );
    friend sha256 operator >> ( const sha256& h1, uint32_t i       );
    friend bool   operator == ( const sha256& h1, const sha256& h2 );
    friend bool   operator != ( const sha256& h1, const sha256& h2 );
    friend sha256 operator ^  ( const sha256& h1, const sha256& h2 );
    friend bool   operator >= ( const sha256& h1, const sha256& h2 );
    friend bool   operator >  ( const sha256& h1, const sha256& h2 ); 
    friend bool   operator <  ( const sha256& h1, const sha256& h2 ); 

    uint32_t pop_count()
    {
       return (uint32_t)(__builtin_popcountll(_hash[0]) +
                         __builtin_popcountll(_hash[1]) +
                         __builtin_popcountll(_hash[2]) +
                         __builtin_popcountll(_hash[3])); 
    }
                             
    uint64_t _hash[4]; 
};

  typedef sha256 uint256;

  class variant;
  void to_variant( const sha256& bi, variant& v );
  void from_variant( const variant& v, sha256& bi );

  uint64_t hash64(const char* buf, size_t len);    

} // fc
namespace std
{
    template<>
    struct hash<fc::sha256>
    {
       size_t operator()( const fc::sha256& s )const
       {
           return  *((size_t*)&s);
       }
    };
}
#include <fc/reflect/reflect.hpp>
FC_REFLECT_TYPENAME( fc::sha256 )
