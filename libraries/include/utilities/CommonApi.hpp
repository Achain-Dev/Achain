#pragma once

#include <cstdio>
#include <string>
namespace thinkyoung {
    namespace utilities {

        int common_fread_octets(FILE* fp, void* dst_stream, int len);
        int common_fread_int(FILE* fp, int* dst_int);

        int common_fwrite_stream(FILE* fp, const void* src_stream, int len);
        int common_fwrite_int(FILE* fp, const int* src_int);
		bool isNumber(std::string input);
    }
} // end namespace thinkyoung::utilities