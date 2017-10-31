#include <glua/ltype_checker_type_info.h>
#include <glua/ltypechecker.h>
#include <glua/exceptions.h>

namespace glua {
	namespace parser {

		GluaTypeInfo::GluaTypeInfo(GluaTypeInfoEnum type_info_enum)
			: etype(type_info_enum), is_offline(false), declared(false),
			is_any_function(false), is_any_contract(false), is_stream_type(false),
			is_literal_token_value(false), array_item_type(nullptr), 
			map_item_type(nullptr), is_literal_empty_table(false)
		{}

		bool GluaTypeInfo::is_contract_type() const
		{
			return is_any_contract || record_origin_name.find("Contract") != std::string::npos;
		}

		bool GluaTypeInfo::is_function() const {
			return etype == GluaTypeInfoEnum::LTI_FUNCTION;
		}

		bool GluaTypeInfo::is_int() const
		{
			return etype == GluaTypeInfoEnum::LTI_INT;
		}

		bool GluaTypeInfo::is_number() const
		{
			return etype == GluaTypeInfoEnum::LTI_INT || etype == GluaTypeInfoEnum::LTI_NUMBER;
		}

		bool GluaTypeInfo::is_bool() const
		{
			return etype == GluaTypeInfoEnum::LTI_BOOL || etype == GluaTypeInfoEnum::LTI_NIL;
		}

		bool GluaTypeInfo::is_string() const
		{
			return etype == GluaTypeInfoEnum::LTI_STRING;
		}

		bool GluaTypeInfo::is_literal_item_type() const
		{
			return is_string() || is_number() || is_bool() || is_nil();
		}

		bool GluaTypeInfo::has_call_prop() const
		{
			if (etype == GluaTypeInfoEnum::LTI_RECORD)
			{
				for (const auto &item : record_props)
				{
					if (item.first == "__call" && item.first.size() > 2 && item.second)
						return true;
				}
				return false;
			}
			return false;
		}

		bool GluaTypeInfo::may_be_callable() const
		{
			if (etype == GluaTypeInfoEnum::LTI_OBJECT
				|| etype == GluaTypeInfoEnum::LTI_TABLE
				|| etype == GluaTypeInfoEnum::LTI_FUNCTION)
				return true;
			if (etype == GluaTypeInfoEnum::LTI_UNION)
			{
				for (const auto &item : union_types)
				{
					if (item->may_be_callable())
						return true;
				}
				return false;
			}
			if (etype == GluaTypeInfoEnum::LTI_RECORD)
			{
				for (const auto &item : record_props)
				{
					if (item.first == "__call" && item.first.size() > 2 && item.second)
						return true;
				}
				return false;
			}
			return false;
		}

		bool GluaTypeInfo::is_nil() const
		{
			return etype == GluaTypeInfoEnum::LTI_NIL;
		}
			
		bool GluaTypeInfo::is_undefined() const
		{
			return etype == GluaTypeInfoEnum::LTI_UNDEFINED;
		}

		bool GluaTypeInfo::is_union() const
		{
			return etype == GluaTypeInfoEnum::LTI_UNION;
		}

		bool GluaTypeInfo::is_record() const
		{
			return etype == GluaTypeInfoEnum::LTI_RECORD;
		}
			
		bool GluaTypeInfo::is_array() const
		{
			return etype == GluaTypeInfoEnum::LTI_ARRAY;
		}

		bool GluaTypeInfo::is_map() const
		{
			return etype == GluaTypeInfoEnum::LTI_MAP;
		}
			
		bool GluaTypeInfo::is_table() const
		{
			return is_narrow_table() || is_map() || is_array();
		}

		bool GluaTypeInfo::is_narrow_table() const
		{
			return etype == GluaTypeInfoEnum::LTI_TABLE;
		}

		bool GluaTypeInfo::is_like_table() const
		{
			return is_narrow_table() || is_map() || is_array() || is_record();
		}
			
		bool GluaTypeInfo::is_generic() const
		{
			return etype == GluaTypeInfoEnum::LTI_GENERIC;
		}

		bool GluaTypeInfo::is_literal_type() const
		{
			return etype == GluaTypeInfoEnum::LTI_LITERIAL_TYPE;
		}

		bool GluaTypeInfo::has_var_args() const
		{
			return arg_names.size() > 0 && arg_names.at(arg_names.size() - 1) == "...";
		}

		bool GluaTypeInfo::has_meta_method() const
		{
			for(const auto &item : record_props)
			{
				if (item.first.length() > 2 && item.first[0] == '_' && item.first[1] == '_')
					return true;
			}
			return false;
		}

		bool GluaTypeInfo::is_same_record(GluaTypeInfoP other) const
		{
			if (this == other)
				return true;
			if (!this->is_record() || !other->is_record())
				return false;
			if ((this->record_name != other->record_name && this->record_origin_name != other->record_origin_name)
				|| this->record_props.size() != other->record_props.size()
				|| this->record_all_generics.size() != other->record_all_generics.size()
				|| this->record_generics.size() != other->record_generics.size()
				|| this->record_applied_generics.size() != other->record_applied_generics.size())
				return false;
			else
				return true;
		}

		size_t GluaTypeInfo::min_args_count_require() const
		{
			if (has_var_args())
				return arg_types.size() > 0 ? (arg_types.size() - 1) : 0;
			else
				return arg_types.size();
		}

		bool GluaTypeInfo::match_literal_type(GluaTypeInfoP value_type) const
		{
			if (etype != GluaTypeInfoEnum::LTI_LITERIAL_TYPE 
				|| value_type->etype != GluaTypeInfoEnum::LTI_LITERIAL_TYPE)
				return false;
			if (this == value_type)
				return true;
			// 声明类型的范围要超过或者等于值类型的范围
			if(this->literal_type_options.size() < value_type->literal_type_options.size())
				return false;
			for (const auto &value_token : value_type->literal_type_options)
			{
				bool found = false;
				for(const auto &item : this->literal_type_options)
				{
					// TODO: 如果是数字，考虑数字的不同表示法
					if (item.type == value_token.type
						&& item.token == value_token.token)
					{
						found = true;
						break;
					}
				}
				if (!found)
					return false;
			}
			return true;
		}

		bool GluaTypeInfo::match_literal_value(GluaParserToken value_token) const
		{
			if (!is_literal_type())
				return false;
			for(const auto &item : literal_type_options)
			{
				// TODO: 如果是数字，考虑数字的不同表示法
				if (item.type == value_token.type 
					&& item.token == value_token.token 
					&& item.source_token == value_token.source_token)
					return true;
			}
			return false;
		}

		bool GluaTypeInfo::contains_literal_item_type(GluaTypeInfoP value_type) const
		{
			if (!is_literal_type())
				return false;
			if (!value_type->is_literal_item_type())
				return false;
			for(const auto &item : literal_type_options)
			{
				if (value_type->is_string() && item.type == TOKEN_RESERVED::LTK_STRING)
					return true;
				if (value_type->is_number() && (item.type == TOKEN_RESERVED::LTK_INT || item.type == TOKEN_RESERVED::LTK_FLT))
					return true;
				if (value_type->is_bool() && (item.type == TOKEN_RESERVED::LTK_TRUE || item.type == TOKEN_RESERVED::LTK_FALSE))
					return true;
				if (value_type->is_nil() && item.type == TOKEN_RESERVED::LTK_NIL)
					return true;
			}
			return false;
		}

		bool GluaTypeInfo::put_contract_storage_type_to_module_stream(GluaModuleByteStreamP stream)
		{
			if (!stream || !this->is_record())
				throw glua::core::GluaException("empty stream or storage's type is not record");
			stream->contract_storage_properties.clear();
			for(const auto &p : this->record_props)
			{
				const auto &prop_type = p.second;
				if(prop_type->etype == GluaTypeInfoEnum::LTI_INT)
				{
					stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
				}
				else if(prop_type->etype == GluaTypeInfoEnum::LTI_NUMBER)
				{
					stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
				}
				else if(prop_type->is_bool())
				{
					stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_bool;
				}
				else if(prop_type->is_string())
				{
					stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
				}
				else if (prop_type->is_stream_type)
				{
					stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_stream;
				}
				else if(prop_type->is_narrow_table())
				{
					std::stringstream error_ss;
					error_ss << "Can't use type " << prop_type->str() << " in contract storage";
					const auto &error_str = error_ss.str();
					throw glua::core::GluaException(error_str.c_str());
					// stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_unkown_table;
				}
				else if(prop_type->is_map())
				{
					switch(prop_type->map_item_type->etype)
					{
					case GluaTypeInfoEnum::LTI_INT: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_int_table;
					} break;
					case GluaTypeInfoEnum::LTI_NUMBER: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_number_table;
					} break;
					case GluaTypeInfoEnum::LTI_BOOL: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_bool_table;
					} break;
					case GluaTypeInfoEnum::LTI_STRING: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_string_table;
					} break;
					case GluaTypeInfoEnum::LTI_STREAM: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_stream_table;
					} break;
					default: {
						if(prop_type->map_item_type->is_stream_type)
						{
							stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_stream_table;
						}
						else
						{
							std::stringstream error_ss;
							error_ss << "Can't use type " << prop_type->str() << " in contract storage";
							const auto &error_str = error_ss.str();
							throw glua::core::GluaException(error_str.c_str());
						}
					}
					}
				}
				else if(prop_type->is_array())
				{
					switch (prop_type->array_item_type->etype)
					{
					case GluaTypeInfoEnum::LTI_INT: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_int_array;
					} break;
					case GluaTypeInfoEnum::LTI_NUMBER: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_number_array;
					} break;
					case GluaTypeInfoEnum::LTI_BOOL: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_bool_array;
					} break;
					case GluaTypeInfoEnum::LTI_STRING: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_string_array;
					} break;
					case GluaTypeInfoEnum::LTI_STREAM: {
						stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_stream_array;
					} break;
					default: 
					{
						if(prop_type->array_item_type->is_stream_type)
						{
							stream->contract_storage_properties[p.first] = thinkyoung::blockchain::StorageValueTypes::storage_value_stream_array;

						}
						else
						{
							std::stringstream error_ss;
							error_ss << "Can't use type " << prop_type->str() << " in contract storage";
							const auto &error_str = error_ss.str();
							throw glua::core::GluaException(error_str.c_str());
						}
					}
					}
				}
				else
				{
					std::stringstream error_ss;
					error_ss << "Can't use type " << prop_type->str() << " in contract storage";
					const auto &error_str = error_ss.str();
					throw glua::core::GluaException(error_str.c_str());
				}
			}
			return true;
		}

		bool GluaTypeInfo::put_contract_apis_info_to_module_stream(GluaModuleByteStreamP stream)
		{
			for(const auto &api_type : record_props)
			{
				if (!api_type.second->is_function())
					continue;
				std::vector<GluaTypeInfoEnum> arg_types;
				for(size_t i=1;i<api_type.second->arg_types.size();++i) // 排除第一个参数self
				{
					arg_types.push_back(api_type.second->arg_types[i]->etype);
				}
				stream->contract_api_arg_types[api_type.first] = arg_types;
			}
			return true;
		}

		std::string GluaTypeInfo::str(const bool show_record_details) const
		{
			switch (etype)
			{
			case GluaTypeInfoEnum::LTI_OBJECT:
				return "object";
			case GluaTypeInfoEnum::LTI_NIL:
				return "nil";
			case GluaTypeInfoEnum::LTI_STRING:
				return "string";
			case GluaTypeInfoEnum::LTI_INT:
				return "int";
			case GluaTypeInfoEnum::LTI_NUMBER:
				return "number";
			case GluaTypeInfoEnum::LTI_BOOL:
				return "bool";
			case GluaTypeInfoEnum::LTI_TABLE:
				return "table";
			case GluaTypeInfoEnum::LTI_UNDEFINED:
				return "undefined";
			case GluaTypeInfoEnum::LTI_FUNCTION:
			{
				if (is_any_function)
					return "function";
				std::stringstream ss;
				ss << "function ";
				if (ret_types.size() > 0)
				{
					ss << ret_types.at(0)->str() << " ";
				}
				ss << "(";
				bool has_started = false;
				for (auto i = arg_types.begin(); i != arg_types.end(); ++i)
				{
					if (has_started)
						ss << ", ";
					ss << (*i)->str();
					has_started = true;
				}
				ss << ")";
				return ss.str();
			}
			case GluaTypeInfoEnum::LTI_UNION:
			{
				std::stringstream ss;
				for (auto i = union_types.begin(); i != union_types.end(); ++i)
				{
					if (ss.str().length() > 0)
						ss << "|";
					ss << lua_type_info_to_str((*i)->etype);
				}
				return ss.str();
			}
			case GluaTypeInfoEnum::LTI_ARRAY:
			{
				std::stringstream ss;
				ss << "Array<" << (array_item_type ? array_item_type->str() : "?") << ">";
				return ss.str();
			}
			case GluaTypeInfoEnum::LTI_MAP:
			{
				std::stringstream ss;
				ss << "Map<" << (map_item_type ? map_item_type->str() : "?") << ">";
				return ss.str();
			}
			case GluaTypeInfoEnum::LTI_LITERIAL_TYPE:
			{
				std::stringstream ss;
				bool is_first = true;
				for(const auto &item : literal_type_options)
				{
					if (!is_first)
						ss << " | ";
					ss << ((item.source_token.length() > 0) ? item.source_token : item.token);
					is_first = false;
				}
				return ss.str();
			}
			case GluaTypeInfoEnum::LTI_RECORD:
			{
				std::stringstream ss;
				ss << "record " << record_name;
				if (record_generics.size() > 0)
				{
					ss << "<";
					bool has_started = false;
					for (const auto &item : record_generics)
					{
						if (has_started)
							ss << ", ";
						ss << item->str();
						has_started = true;
					}
					ss << ">";
				}
				ss << "{";
				bool has_started = false;
				for (auto i = record_props.begin(); i != record_props.end(); ++i)
				{
					if (has_started)
						ss << ",";
					ss << i->first;
					if (show_record_details)
						ss << " : " << i->second->str(false);
					has_started = true;
				}
				ss << "}";
				return ss.str();
			}
			case GluaTypeInfoEnum::LTI_GENERIC:
			{
				return generic_name;
			}
			default:
				return lua_type_info_to_str(etype);
			}
		}

	}
}
