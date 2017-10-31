#include <stdint.h>
#include <utilities/CommonApi.hpp> 
#include <string>
namespace thinkyoung {
    namespace utilities {

        int common_fread_octets(FILE* fp, void* dst_stream, int len)
        {
            return (int)fread(dst_stream, len, 1, fp);
        }

        int common_fread_int(FILE* fp, int* dst_int)
        {
            int ret;
            unsigned char uc4, uc3, uc2, uc1;

            ret = (int)fread(&uc4, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;
            ret = (int)fread(&uc3, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;
            ret = (int)fread(&uc2, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;
            ret = (int)fread(&uc1, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;

            *dst_int = (uc4 << 24) + (uc3 << 16) + (uc2 << 8) + uc1;

            return 1;
        }

        int common_fwrite_stream(FILE* fp, const void* src_stream, int len)
        {
            return (int)fwrite(src_stream, len, 1, fp);
        }

        int common_fwrite_int(FILE* fp, const int* src_int)
        {
            int ret;
            unsigned char uc4, uc3, uc2, uc1;
            uc4 = ((*src_int) & 0xFF000000) >> 24;
            uc3 = ((*src_int) & 0x00FF0000) >> 16;
            uc2 = ((*src_int) & 0x0000FF00) >> 8;
            uc1 = (*src_int) & 0x000000FF;

            ret = (int)fwrite(&uc4, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;
            ret = (int)fwrite(&uc3, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;
            ret = (int)fwrite(&uc2, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;
            ret = (int)fwrite(&uc1, sizeof(unsigned char), 1, fp);
            if (ret != 1)
                return ret;

            return 1;
        }
		bool isNumber(std::string input)
		{
			const char* p = input.c_str();
			int num = input.length();
			bool gotpoint = false;
			for (int i = 0; i < num; i++)
			{
				char c = p[i];
				if (c >= '0'&&c <= '9')
					continue;
				if (c == '.'&&gotpoint == false)
					gotpoint = true;
				else
					return false;
			}
			return true;
		}
    }

} // end namespace thinkyoung::utilities