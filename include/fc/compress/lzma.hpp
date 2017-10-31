#pragma once

#include <fc/filesystem.hpp>
#include <vector>

namespace fc {

std::vector<char> lzma_compress( const std::vector<char>& in );
std::vector<char> lzma_decompress( const std::vector<char>& compressed );

void lzma_compress_file( const path& src_path,
                         const path& dst_path,
                         unsigned char level = 5,
                         unsigned int dict_size = (1 << 20) );

void lzma_decompress_file( const path& src_path,
                           const path& dst_path );

} // namespace fc
