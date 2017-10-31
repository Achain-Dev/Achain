#pragma once
#include <fc/crypto/sha256.hpp>

namespace fc 
{
  void salsa20_encrypt( const fc::sha256& key, uint64_t iv, const char* plain, char* cipher, uint64_t len );
  void salsa20_decrypt( const fc::sha256& key, uint64_t iv, const char* cipher, char* plain, uint64_t len );
}
