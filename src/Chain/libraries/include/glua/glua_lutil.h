/**
* some utils functions
* @author
*/

#ifndef glua_lutil_h
#define glua_lutil_h

#include <glua/lprefix.h>

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
#include <list>
#include <vector>
#include <chrono>

struct lua_State;

namespace glua
{
	namespace util
	{
		char file_separator();

		char *file_separator_str();

		char *copy_str(const char *str);

		bool ends_with(const std::string &str, const std::string &suffix);

		bool starts_with(const std::string &str, const std::string &prefix);

		bool compare_string_list(std::list<std::string> &l1, std::list<std::string> &l2);

		void replace_all(std::string& str, const std::string& from, const std::string& to);

		/**
		* escape string to use in json
		*/
		std::string escape_string(std::string &str);

		std::vector<std::string> &string_split(const std::string &s, char delim, std::vector<std::string> &elems);

		std::vector<std::string> string_split(const std::string &s, char delim);

		std::string string_trim(std::string source);

		template <typename IterT>
		std::string string_join(IterT begin, IterT end, std::string sep)
		{
			std::stringstream ss;
			bool is_first = true;
			auto i = begin;
			while(i!=end)
			{
				if (!is_first)
					ss << sep;
				is_first = false;
				ss << *i;
				++i;
			}
			return ss.str();
		}

		template <typename T>
		bool vector_contains(std::vector<T> &col, T &item)
		{
			return std::find(col.begin(), col.end(), item) != col.end();
		}

		template <typename T>
		bool vector_contains(std::vector<T> col, const T &item)
		{
			return std::find(col.begin(), col.end(), item) != col.end();
		}

		template <typename ColType, typename ColType2>
		void append_range(ColType &col, ColType2 &items)
		{
			for(const auto &item : items)
			{
				col.push_back(item);
			}
		}

		template<typename TEMP,typename T>
        bool find(TEMP begin,TEMP end,T to_find)
        {
            TEMP in=begin;
            while (in!=end)
            {
                if(*in==to_find)
                {
                    return true;
                }
                in++;
            }
            return false;
        }

		// 获取字符串有几行
		size_t string_lines_count(const std::string &str);

        class TimeDiff
        {
        private:
          std::chrono::time_point<std::chrono::system_clock> _start_time;
          std::chrono::time_point<std::chrono::system_clock> _end_time;
        public:
          inline TimeDiff(bool start_now=true)
          {
            if (start_now)
              start();
          }
          inline void start()
          {
            _start_time = std::chrono::system_clock::now();
          }
          inline void end()
          {
            _end_time = std::chrono::system_clock::now();
          }
          inline std::chrono::duration<double> diff_time(bool end_time=true)
          {
            if (end_time)
              end();
            return _end_time - _start_time;
          }
          inline double diff_timestamp(bool end_time=true)
          {
            return diff_time(end_time).count();
          }
        };

	} // end namespace glua::util
} // end namespace glua

#endif