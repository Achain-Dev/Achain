#include <glua/glua_debug_file.h>
#include <glua/exceptions.h>
#include <string>
#include <sstream>
#include <fstream>
#include <exception>
#include <boost/variant/variant.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

namespace thinkyoung
{
	namespace lua
	{
		namespace core
		{
			using boost::property_tree::ptree;
			using boost::property_tree::read_json;
			using boost::property_tree::write_json;
			namespace gle = thinkyoung::lua::exceptions;


			LuaDebugFileInfo::LuaDebugFileInfo()
			{
				
			}

			LuaDebugFileInfo::LuaDebugFileInfo(const LuaDebugFileInfo &other)
			{
				_source_line_mappings = other._source_line_mappings;
				_proto_names = other._proto_names;
			}

			std::string LuaDebugFileInfo::serialize() const
			{
				ptree pt;
				ptree pt_of_source_line_mappings;
				ptree pt_of_proto_names;
				for(const auto &item : _source_line_mappings)
				{
					pt_of_source_line_mappings.put(std::to_string(item.first), item.second);
				}
				for(const auto &item : _proto_names)
				{
					pt_of_proto_names.push_back(std::make_pair("", ptree(item)));
				}
				pt.put_child("source_line_mappings", pt_of_source_line_mappings);
				pt.put_child("proto_names", pt_of_proto_names);
				std::ostringstream buf;
				write_json(buf, pt, false);
				return buf.str();
			}

			void LuaDebugFileInfo::serialize_to_file(FILE* file) const
			{
				if(!file)
				{
					BOOST_THROW_EXCEPTION(gle::lua_debug_exception());
				}
				auto content = serialize();
				fwrite(content.c_str(), sizeof(char), content.length(), file);
			}


			LuaDebugFileInfo LuaDebugFileInfo::deserialize(const std::string& content)
			{
				ptree pt;               
				std::stringstream ss(content);
				try {
					read_json(ss, pt);      
				}
				catch (boost::property_tree::ptree_error & e) {
					BOOST_THROW_EXCEPTION(e);
				}
				LuaDebugFileInfo ldf;

				ptree pt_of_source_line_mappings = pt.get_child("source_line_mappings");
				ptree pt_of_proto_names = pt.get_child("proto_names");
																	
				for (auto it = pt_of_source_line_mappings.begin(); it != pt_of_source_line_mappings.end(); ++it) {
					size_t glua_line = std::stoi(it->first);
					size_t lua_line =std::stoi(it->second.data());
					ldf._source_line_mappings[glua_line] = lua_line;
				}
				for(auto it=pt_of_proto_names.begin();it!=pt_of_proto_names.end();++it)
				{
					std::string proto_name = it->second.data();
					ldf._proto_names.push_back(proto_name);
				}
				return ldf;
			}

			LuaDebugFileInfo LuaDebugFileInfo::deserialize_from_file(FILE* file)
			{
				if (!file)
					BOOST_THROW_EXCEPTION(gle::lua_debug_exception());
				long loc=ftell(file);
				fseek(file,0,SEEK_END);
				long fsize=ftell(file);
				rewind(file);
				char* strbuf=(char*)malloc(fsize+1);
				memset(strbuf, 0x0, sizeof(char) * (fsize + 1));
				if(!strbuf)
					BOOST_THROW_EXCEPTION(gle::lua_debug_exception());
				int real_size = (int) fread(strbuf,1,fsize,file);
				strbuf[real_size]='\0';
				std::string strbuf_str(strbuf);
				free(strbuf);
				return deserialize(strbuf_str);
			}

			void LuaDebugFileInfo::set_source_line_mapping(size_t glua_line, size_t lua_line)
			{
				_source_line_mappings[glua_line] = lua_line;
			}

			void LuaDebugFileInfo::add_proto_name(const std::string &proto_name)
			{
				_proto_names.push_back(proto_name);
			}

			size_t LuaDebugFileInfo::find_glua_line_by_lua_line(size_t lua_line)
			{
				for(const auto &item : _source_line_mappings)
				{
					if (item.second >= lua_line)
						return item.first;
				}
				return 0;
			}

			size_t LuaDebugFileInfo::find_lua_line_by_glua_line(size_t glua_line)
			{
				for(const auto &item : _source_line_mappings)
				{
					if (item.first >= glua_line)
						return item.second;
				}
				return 0;
			}

		}
	}
}