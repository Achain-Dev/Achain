#include <glua/lprefix.h>

#include <string>
#include <algorithm>
#include <sstream>

#include <glua/glua_lutil.h>
#include <glua/lstate.h>


namespace glua
{
	namespace util
	{
		inline char file_separator()
		{
#ifdef _WIN32
			return '\\';
#else
			return '/';
#endif
		}

		static char *FILE_SEPRATOR_STR = nullptr;

		char *file_separator_str()
		{
			if (FILE_SEPRATOR_STR)
				return FILE_SEPRATOR_STR;
			FILE_SEPRATOR_STR = (char*)calloc(2, sizeof(char));
			FILE_SEPRATOR_STR[0] = file_separator();
			FILE_SEPRATOR_STR[1] = '\0';
			return FILE_SEPRATOR_STR;
		}

		char *copy_str(const char *str)
		{
			if (!str)
				return nullptr;
			char *copied = (char*)malloc(strlen(str) + 1);
			strcpy(copied, str);
			return copied;
		}

		bool ends_with(const std::string &str, const std::string &suffix)
		{
			return str.size() >= suffix.size() &&
				str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
		}

		bool starts_with(const std::string &str, const std::string &prefix)
		{
			return str.size() >= prefix.size() &&
				str.compare(0, prefix.size(), prefix) == 0;
		}

		bool compare_string_list(std::list<std::string> &l1, std::list<std::string> &l2)
		{
			if (l1.size() != l2.size())
				return false;
			std::list<std::string>::iterator it1 = l1.begin();
			std::list<std::string>::iterator it2 = l2.begin();
			std::list<std::string>::iterator end1 = l1.end();
			while (it1 != end1)
			{
				if (*it1 != *it2) {
					return false;
				}
				++it1;
				++it2;
			}
			return true;
		}

		void replace_all(std::string& str, const std::string& from, const std::string& to) {
			size_t start_pos = 0;
			while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
				size_t end_pos = start_pos + from.length();
				str.replace(start_pos, end_pos, to);
				start_pos += to.length();
			}
		}

		std::string escape_string(std::string &str)
		{
			std::string result(str);
			glua::util::replace_all(result, "\\", "\\\\");
			glua::util::replace_all(result, "\"", "\\\"");
			return result;
		}

		std::vector<std::string> &string_split(const std::string &s, char delim, std::vector<std::string> &elems)
		{
			std::stringstream ss(s);
			std::string item;
			while (std::getline(ss, item, delim)) {
				elems.push_back(item);
			}
			return elems;
		}

		std::vector<std::string> string_split(const std::string &s, char delim) {
			std::vector<std::string> elems;
			string_split(s, delim, elems);
			return elems;
		}

		std::string string_trim(std::string source)
		{
			while (source.length() > 0 && (source[0] == ' ' || source[0] == '\t' || source[0] == '\n' || source[0] == '\r'))
				source.erase(0, source.find_first_of(source[0]) + 1);
			while (source.length() > 0 && (source[source.length() - 1] == ' ' || source[source.length() - 1] == '\t' || source[source.length() - 1] == '\n' || source[source.length()-1] == '\r'))
				source.erase(source.find_last_not_of(source[source.length() - 1]) + 1);
			return source;
		}

		size_t string_lines_count(const std::string &str)
		{
			return std::count(str.begin(), str.end(), '\n') + 1;
		}

	} // end namespace glua::util
} // end namespace glua