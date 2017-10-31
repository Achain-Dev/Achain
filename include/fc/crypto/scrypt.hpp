#pragma once
#include <vector>

namespace fc {

   void scrypt_derive_key( const std::vector<unsigned char>& passphrase, const std::vector<unsigned char>& salt,
                           unsigned int n, unsigned int r, unsigned int p, std::vector<unsigned char>& key );

} // namespace fc
