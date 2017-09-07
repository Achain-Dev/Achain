#include <glua/ltypechecker.h>
#include <cassert>
#include <glua/thinkyoung_lua_lib.h>
#include <glua/glua_lutil.h>
#include <glua/glua_astparser.h>
#include <boost/exception/diagnostic_information.hpp> 
#include <boost/exception_ptr.hpp> 

namespace glua {
    namespace parser {


		static std::vector<std::string> exclude_function_names = { "emit" };

        std::string lua_type_info_to_str(const GluaTypeInfoEnum etype)
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
            case GluaTypeInfoEnum::LTI_FUNCTION:
                return "function";
            case GluaTypeInfoEnum::LTI_UNION:
                return "union";
            case GluaTypeInfoEnum::LTI_RECORD:
                return "record";
            case GluaTypeInfoEnum::LTI_GENERIC:
                return "generic";
            case GluaTypeInfoEnum::LTI_ARRAY:
                return "array";
            case GluaTypeInfoEnum::LTI_UNDEFINED:
                return "undefined";
            default:
                return "object";
            }
        }

		LuaProtoSTree::LuaProtoSTree()
        {
			type_info = nullptr;
			mr = nullptr;
			full_mr = nullptr;
			condition_mr = nullptr;
        }

		bool LuaProtoSTree::is_changable_localvar(const std::string &name)
		{
			auto found = localvars_changable.find(name);
			if (found != localvars_changable.end())
				return found->second;
			else
				return true;
		}

		void LuaProtoSTree::add_arg(std::string arg_name, GluaTypeInfoP arg_type)
		{
			args.push_back(arg_name);
			type_info->arg_names.push_back(arg_name);
			type_info->arg_types.push_back(arg_type);
		}

		GluaExtraBindingsType::GluaExtraBindingsType() {}
		GluaExtraBindingsType::~GluaExtraBindingsType() {}
		void GluaExtraBindingsType::set_type(std::string name, GluaTypeInfoP value)
        {
			_type_extra_bindings[name] = value;
        }
		void GluaExtraBindingsType::set_variable(std::string name, GluaTypeInfoP value)
        {
			_variable_extra_bindings[name] = value;
        }
		GluaTypeInfoP GluaExtraBindingsType::find_type(std::string name) const
        {
			auto found = _type_extra_bindings.find(name);
			if (found != _type_extra_bindings.end())
				return found->second;
			else
				return nullptr;
        }
		GluaTypeInfoP GluaExtraBindingsType::find_variable(std::string name) const
        {
			auto found = _variable_extra_bindings.find(name);
			if (found != _variable_extra_bindings.end())
				return found->second;
			else
				return nullptr;
        }

		void GluaExtraBindingsType::copy_to(GluaExtraBindingsTypeP other)
        {
	        for(const auto &p : _variable_extra_bindings)
	        {
				other->_variable_extra_bindings[p.first] = p.second;
	        }
			for(const auto &p : _type_extra_bindings)
			{
				other->_type_extra_bindings[p.first] = p.second;
			}
        }


        GluaTypeChecker::GluaTypeChecker(ParserContext *ctx)
            : _ctx(ctx), _in_repl(false), _open_record_call_syntax(false), is_checking_contract(false)
        {
			_middle_inserted_code_lines = 0; // ctx->inner_lib_code_lines();
        }
        GluaTypeChecker::~GluaTypeChecker()
        {
			for(const auto &item : _created_lua_type_info_pointers)
			{
				delete item;
			}
        }

		void GluaTypeChecker::set_in_repl(bool in_repl)
        {
			_in_repl = in_repl;
        }

		GluaTypeInfoP GluaTypeChecker::get_index_by_number_type(GluaTypeInfoP type_info)
        {
			// 获得t[number]的结果类型
			if (type_info->etype == GluaTypeInfoEnum::LTI_OBJECT)
				return create_lua_type_info();
			else if (type_info->etype == GluaTypeInfoEnum::LTI_ARRAY)
				return type_info->array_item_type;
			else if (type_info->etype == GluaTypeInfoEnum::LTI_RECORD)
			{
				auto found = type_info->record_props.find("__index");
				if (found == type_info->record_props.end())
					return create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				auto t = found->second;
				if (t->is_any_function)
					return create_lua_type_info();
				else if (t->is_function())
				{
					if (t->arg_types.size() == 1 && t->arg_types.at(0)->is_number() && t->ret_types.size() > 0)
						return t->ret_types.at(0);
					return create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				}
				else
				{
					return create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				}
			}
			else if (type_info->etype == GluaTypeInfoEnum::LTI_UNION)
			{
				for (const auto &item : type_info->union_types)
				{
					auto t = get_index_by_number_type(item);
					if (!t->is_nil() && !t->is_undefined())
						return t;
				}
				return create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
			}
			else
				return create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
        }

		GluaTypeInfoP GluaTypeChecker::create_array(GluaTypeInfoP array_item_type)
        {
			auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_ARRAY);
			type_info->array_item_type = array_item_type ? array_item_type : create_lua_type_info();
			return type_info;
        }

		bool GluaTypeChecker::can_visit_index_by_number(GluaTypeInfoP type_info)
        {
			// 判断是否能用类似t[number]的方式访问（也就是当成数组来访问）
			// 判断是否是object或array或record但是有__index属性且类型为(int)=>any, 或union但是某个union_type满足条件
			auto t = get_index_by_number_type(type_info);
			return !t->is_nil() && !t->is_undefined();
        }

        static bool is_expr_node(MatchResult *mr)
        {
            return nullptr != mr && (mr->node_name() == "exp"
                || mr->node_name() == "simpleexp"
                || mr->node_name() == "bin_exp"
                || mr->node_name() == "un_exp"
				|| mr->node_name() == "lambda_value_expr"
				|| mr->node_name() == "lambda_expr");
        }

        static bool is_simpleexp_node(MatchResult *mr)
        {
            return nullptr != mr && (mr->node_name() == "simpleexp" || mr->node_name() == "tableconstructor");
        }

        std::pair<int, std::string> GluaTypeChecker::error_in_match_result(MatchResult *mr, std::string more, int error_no) const
        {
            auto head_token = mr->head_token();
            std::string err_str = std::string("type error in line " + std::to_string(head_token.linenumber - _ctx->inner_lib_code_lines() + 2) + " position ") + std::to_string(head_token.position) + " token " + head_token.token;
            if (more.length() > 0)
                err_str = err_str + ", " + more;
            return std::make_pair(error_no, err_str);
        }

        std::pair<int, std::string> GluaTypeChecker::error_in_functioncall(MatchResult *mr, GluaTypeInfoP func_type_info, std::vector<GluaTypeInfoP> *real_args) const
        {
            auto head_token = mr->head_token();
            std::stringstream ss;
            ss << "type error of function args in line " << std::to_string(head_token.linenumber - _ctx->inner_lib_code_lines() + 2)
                << " position " << std::to_string(head_token.position) << " token " << head_token.token;
            ss << ", function args types are: (";
            bool is_first = true;
            for (auto i = func_type_info->arg_types.begin(); i != func_type_info->arg_types.end(); ++i)
            {
                auto type = *i;
                if (!is_first)
                    ss << ", ";
                ss << type->str();
                is_first = false;
            }
            ss << ")";
            if (nullptr != real_args)
            {
                ss << ", using args are: (";
                bool has_started = false;
                for (const auto &item : *real_args)
                {
                    if (has_started)
                        ss << ", ";
                    ss << item->str();
                    has_started = true;
                }
                ss << ")";
            }
            auto error_str = ss.str();
            return std::make_pair(GluaTypeCheckerErrors::FUNCTION_CALL_ERROR, error_str);
        }

        std::pair<int, std::string> GluaTypeChecker::error_of_wrong_type_call(MatchResult *mr,
            GluaParserToken token, std::string expected_types_str) const
        {
            return error_in_match_result(mr, std::string("call ") + token.token + " with wrong type args at line "
                + ", expected " + expected_types_str, GluaTypeCheckerErrors::FUNCTION_CALL_WITH_WRONG_TYPE);
        }

        static void copy_lua_type_info(GluaTypeInfoP dest, GluaTypeInfoP src)
        {
            if (dest && src)
            {
                dest->etype = src->etype;
                dest->arg_names = src->arg_names;
                dest->arg_types = src->arg_types;
                dest->ret_types = src->ret_types;
                dest->union_types = src->union_types;
                dest->is_offline = src->is_offline;
				dest->is_literal_token_value = src->is_literal_token_value;
				dest->literal_value_token = src->literal_value_token;
                dest->declared = src->declared;
                dest->is_any_function = src->is_any_function;
                dest->record_name = src->record_name;
				dest->record_origin_name = src->record_origin_name;
                dest->record_props = src->record_props;
                dest->record_default_values = src->record_default_values;
                dest->record_generics = src->record_generics;
                dest->record_all_generics = src->record_all_generics;
                dest->record_applied_generics = src->record_applied_generics;
                dest->generic_name = src->generic_name;
                dest->array_item_type = src->array_item_type;
				dest->map_item_type = src->map_item_type;
				dest->literal_type_options = src->literal_type_options;
				dest->is_literal_empty_table = src->is_literal_empty_table;
            }
        }

        GluaTypeInfoP GluaTypeChecker::merge_union_types(GluaTypeInfoP type1,
            GluaTypeInfoP type2)
        {
            auto new_type_info = create_lua_type_info();
            if (type1->etype == GluaTypeInfoEnum::LTI_UNDEFINED)
            {
                copy_lua_type_info(new_type_info, type2);
            }
            else if (type1->etype == GluaTypeInfoEnum::LTI_UNION)
            {
				if(type1->union_types.size()<1 && type2->is_literal_type())
				{
					copy_lua_type_info(new_type_info, type2);
					copy_lua_type_info(type1, new_type_info);
				}
                else if (type2->etype != GluaTypeInfoEnum::LTI_UNION)
                {
                    new_type_info = type1;
                    new_type_info->etype = GluaTypeInfoEnum::LTI_UNION;
                    new_type_info->union_types.insert(type2);
                }
                else
                {
                    new_type_info = type1;
                    new_type_info->etype = GluaTypeInfoEnum::LTI_UNION;
                    for (auto ue = type2->union_types.begin(); ue != type2->union_types.end(); ++ue)
                    {
                        new_type_info->union_types.insert(*ue);
                    }
                }
            }
            else if (type2->etype == GluaTypeInfoEnum::LTI_UNION)
            {
				if (type2->union_types.size()<1 && type1->is_literal_type())
				{
					copy_lua_type_info(new_type_info, type1);
					copy_lua_type_info(type1, new_type_info);
				}
				else {
					new_type_info = type2;
					new_type_info->etype = GluaTypeInfoEnum::LTI_UNION;
					new_type_info->union_types.insert(type1);
				}
            }
			else if(type1->is_literal_type() && type2->is_literal_type())
			{
				new_type_info->etype = GluaTypeInfoEnum::LTI_LITERIAL_TYPE;
				for(const auto &item : type1->literal_type_options)
				{
					new_type_info->literal_type_options.push_back(item);
				}
				for(const auto &item : type2->literal_type_options)
				{
					bool exist = false;
					for(const auto &item1 : new_type_info->literal_type_options)
					{
						// TODO: 如果是数字，考虑数字的不同表示法
						if(item1.type == item.type && item1.token == item.token)
						{
							exist = true;
							break;
						}
					}
					if (!exist)
						new_type_info->literal_type_options.push_back(item);
				}
				copy_lua_type_info(type1, new_type_info);
			}
            else if (type1->etype == type2->etype)
            {
                new_type_info = type2;
            }
            else if (type1->etype == GluaTypeInfoEnum::LTI_RECORD && type2->etype == GluaTypeInfoEnum::LTI_TABLE)
            {
                new_type_info = type1;
            }
            else
            {
                new_type_info->etype = GluaTypeInfoEnum::LTI_UNION;
                new_type_info->union_types.insert(type1);
                new_type_info->union_types.insert(type2);
            }
            return new_type_info;
        }

		void GluaTypeChecker::add_emit_event_type(std::string event_type_name)
		{
			if (event_type_name.length()>0)
			{
				if (!glua::util::vector_contains(_emit_events, event_type_name))
					_emit_events.push_back(event_type_name);
			}
		}

        // @param type_up 是否允许real_type是declare_type父类型的时候类型验证通过
        static bool match_declare_type(GluaTypeInfoP declare_type, GluaTypeInfoP real_type, bool type_up = true)
        {
            if (declare_type == real_type)
                return true;
			if (real_type->is_nil()) // FIXME: 改成只有declare_type是允许nil的类型(union,nil,optional)等才能赋值, Map<T>[key]返回类型应该是T | nil
				return true;
            if (declare_type->etype != GluaTypeInfoEnum::LTI_OBJECT)
            {
                if (declare_type->etype == GluaTypeInfoEnum::LTI_NUMBER
                    && real_type->etype == GluaTypeInfoEnum::LTI_INT)
                {
                    return true;
                }
                if (type_up)
                {
                    if (declare_type->etype == GluaTypeInfoEnum::LTI_INT
                        && real_type->etype == GluaTypeInfoEnum::LTI_NUMBER)
                    {
						return false;
                    }
                }

				/*
				table, record, Map, Array之间的类型关系如表(两边类型完全一样肯定接受):
				左值类型              |    右值类型                               |   是否接受
				---------------------------------------------------------------
				table                          record                                true
				table                          Map<T>                                true
				table                          Array<T>                              true
				record                        table                                  true
				record                         Map<T>                                true
				record                         Array<T>                              false
				record						   其他类型的record						 false
				Map<T1>                   table                                      true
				Map<T1>                   record                                     false
				Map<T1>                   空Map<object>                              true
				Map<T1>                   Map<T2 where T2 extends T1>                true
				Map<T1>                   Map<T2 where T1 extends T2>                false
				Map<T1>                   Array<?>                                   false
				Array<T1>                   table                                    true
				Array<T1>                     空Array<object>                        true
				Array<T1>                   record                                   false
				Array<T1>                   Array<T2 where T2 extends T1>            true
				Array<T1>                   Array<T2 where T1 extends T2>            false
				Array<T1>                   Map<?>                                   false

				*/
				if(declare_type->is_like_table() && real_type->is_like_table())
				{
					if(declare_type->is_narrow_table())
					{
						return true;
					}
					else if(declare_type->is_record())
					{
						if (real_type->is_narrow_table() || real_type->is_map())
							return true;
						else if (real_type->is_array())
							return false;
						else if(real_type->is_record())
						{
							return declare_type->is_same_record(real_type);
						}
					}
					else if(declare_type->is_map())
					{
						if (real_type->is_narrow_table())
							return true;
						else if (real_type->is_record())
							return false;
						else if (real_type->is_map() && real_type->map_item_type->etype == GluaTypeInfoEnum::LTI_OBJECT && real_type->is_literal_empty_table)
							return true;
						else if (real_type->is_array())
							return false;
						else if(real_type->is_map())
						{
							return match_declare_type(declare_type->map_item_type, real_type->map_item_type, type_up);
						}
					}
					else if(declare_type->is_array())
					{
						if (real_type->is_narrow_table())
							return true;
						else if (real_type->is_array() && real_type->array_item_type->etype == GluaTypeInfoEnum::LTI_OBJECT && real_type->is_literal_empty_table)
							return true;
						else if (real_type->is_record())
							return false;
						else if (real_type->is_map())
							return false;
						else if (real_type->is_array())
							return match_declare_type(declare_type->array_item_type, real_type->array_item_type, type_up);
					}
				}

				// 编译期严格区分数组和哈希表
                if ((declare_type->etype == GluaTypeInfoEnum::LTI_TABLE 
					|| declare_type->etype == GluaTypeInfoEnum::LTI_MAP)
                    && (real_type->etype == GluaTypeInfoEnum::LTI_RECORD
                    || real_type->etype == GluaTypeInfoEnum::LTI_ARRAY
						|| real_type->etype == GluaTypeInfoEnum::LTI_MAP))
                {
                    return true;
                }
                // table和record可以互相转换，但是2个record如果具体的类型不一样，不能互相转换
                if ((declare_type->etype == GluaTypeInfoEnum::LTI_RECORD
                    || real_type->etype == GluaTypeInfoEnum::LTI_ARRAY
					|| real_type->etype == GluaTypeInfoEnum::LTI_MAP)
                    && (real_type->etype == GluaTypeInfoEnum::LTI_TABLE
						|| real_type->etype == GluaTypeInfoEnum::LTI_MAP))
                {
                    return true;
                }

                if (declare_type->is_union())
                {
                    if (real_type->is_union())
                    {
                        if (declare_type->union_types.size() != real_type->union_types.size())
                            return false;
                        for (const auto &item1 : declare_type->union_types)
                        {
                            bool found = false;
                            for (const auto &item2 : real_type->union_types)
                            {
                                if (match_declare_type(item1, item2))
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
                    else
                    {
                        for (const auto &item : declare_type->union_types)
                        {
                            if (match_declare_type(item, real_type))
                                return true;
                        }
                        return false;
                    }
                }


				if (declare_type->is_literal_type())
				{
					if (declare_type->match_literal_type(real_type))
						return true;
					// 如果是单token值，match literal token
					if (real_type->is_literal_token_value
						&& (real_type->is_literal_item_type()))
					{
						return declare_type->match_literal_value(real_type->literal_value_token);
					}
					else
						return false; // TODO: union的情况
				}
				if (real_type->is_literal_type())
				{
					// 声明类型不是literal type，而值类型是literal type的情况
					if (declare_type->is_literal_item_type())
					{
						return real_type->contains_literal_item_type(declare_type);
					}
					return false; // TODO: union的情况
				}


                if ((declare_type->etype == GluaTypeInfoEnum::LTI_TABLE
                    && (real_type->etype == GluaTypeInfoEnum::LTI_ARRAY
						|| real_type->etype == GluaTypeInfoEnum::LTI_MAP))
                    || ((declare_type->etype == GluaTypeInfoEnum::LTI_ARRAY
						|| declare_type->etype == GluaTypeInfoEnum::LTI_MAP)
                    && real_type->etype == GluaTypeInfoEnum::LTI_TABLE))
                    return true;

                if (declare_type->etype != real_type->etype)
                {
                    return false;
                }
                if (declare_type->etype == GluaTypeInfoEnum::LTI_RECORD)
                {
                    // 2个不同的record类型，不能匹配
					if (!declare_type->is_same_record(real_type))
						return false;
                    for (const auto & p : declare_type->record_props)
                    {
                        auto found = real_type->record_props.find(p.first);
                        if (found == real_type->record_props.end())
                            return false;
                        auto other = found->second;
                        if (!match_declare_type(p.second, other, true))
                            return false;
                    }
                    return true;
                }
                if (declare_type->etype == GluaTypeInfoEnum::LTI_ARRAY)
                {
                    // 允许父子类型转换
                    if (match_declare_type(declare_type->array_item_type, real_type->array_item_type, true))
                        return true;
                    else
                        return false;
                }
				if (declare_type->etype == GluaTypeInfoEnum::LTI_MAP)
				{
					// 允许父子类型转换
					if (match_declare_type(declare_type->map_item_type, real_type->map_item_type, true))
						return true;
					else
						return false;
				}
                if (declare_type->etype == GluaTypeInfoEnum::LTI_FUNCTION)
                {
                    if (declare_type->is_any_function)
                        return true;
                    if (declare_type->arg_types.size() != real_type->arg_types.size()
                        || declare_type->ret_types.size() != real_type->ret_types.size())
                        return false;
                    for (size_t i = 0; i < declare_type->arg_types.size(); ++i)
                    {
                        if (!match_declare_type(declare_type->arg_types.at(i), real_type->arg_types.at(i), true))
                            return false;
                    }
                    for (size_t i = 0; i < declare_type->ret_types.size(); ++i)
                    {
                        if (!match_declare_type(declare_type->ret_types.at(i), real_type->ret_types.at(i), true))
                            return false;
                    }
                    return true;
                }
            }
            return true;
        }

        bool GluaTypeChecker::match_function_args(GluaTypeInfoP declare_func_type, std::vector<MatchResult*> args_mrs)
        {
            size_t i = 0;
            for (; i < declare_func_type->arg_types.size(); ++i)
            {
                auto arg_declare_type = declare_func_type->arg_types.at(i);
                auto arg_real_type = guess_exp_type(args_mrs[i]);
                if (!match_declare_type(arg_declare_type, arg_real_type, true))
                {
                    return false;
                }
            }
            return true;
        }

        bool GluaTypeChecker::can_access_prop_of_type(MatchResult *mr, GluaTypeInfoP type_info, std::string prop_name, GluaTypeInfoEnum prop_name_type)
        {
			if (type_info->etype == GluaTypeInfoEnum::LTI_OBJECT
				|| type_info->etype == GluaTypeInfoEnum::LTI_TABLE
				|| type_info->etype == GluaTypeInfoEnum::LTI_MAP)
			{
				if(prop_name_type == GluaTypeInfoEnum::LTI_NIL || prop_name_type == GluaTypeInfoEnum::LTI_UNDEFINED)
				{
					set_error(error_in_match_result(mr, std::string("Can't access type ") + type_info->str() + "'s property by nil property"));
					return false;
				}
				return prop_name_type == GluaTypeInfoEnum::LTI_STRING || prop_name_type == GluaTypeInfoEnum::LTI_INT || prop_name_type == GluaTypeInfoEnum::LTI_NUMBER;
			}
			else if (type_info->etype == GluaTypeInfoEnum::LTI_UNION)
            {
                for (const auto &it : type_info->union_types)
                {
                    if (can_access_prop_of_type(mr, it, prop_name, prop_name_type))
                        return true;
                }
                return false;
            }
            else if (type_info->etype == GluaTypeInfoEnum::LTI_RECORD)
            {
				if(prop_name_type == GluaTypeInfoEnum::LTI_INT
					|| prop_name_type == GluaTypeInfoEnum::LTI_NUMBER)
				{
					// 判断是否有__index且参数类型是int/number
					return can_visit_index_by_number(type_info);
				}
                if (prop_name_type != GluaTypeInfoEnum::LTI_STRING
                    && prop_name_type != GluaTypeInfoEnum::LTI_OBJECT)
                    return false; // 字符串或者object外其他类型作为访问record的属性都应该报错
                for (const auto &it : type_info->record_props)
                {
                    if (it.first == prop_name)
                        return true;
                }
                return false;
            }
            else if (type_info->etype == GluaTypeInfoEnum::LTI_ARRAY)
            {
                return prop_name_type == GluaTypeInfoEnum::LTI_INT || prop_name_type == GluaTypeInfoEnum::LTI_NUMBER;
            }
            else
                return false;
        }

        void GluaTypeChecker::enter_proto_to_checking_type(MatchResult *mr, LuaProtoSTreeP proto)
        {
            _current_checking_proto_stack.push_back(proto);
            // 进入新proto时，要把局部变量放入进去
            if (proto->type_info)
            {
                size_t i = 0;
                for (; i < proto->args.size(); ++i)
                {
                    auto name = proto->args.at(i);
                    auto type_info = (i < proto->type_info->arg_types.size()) ? proto->type_info->arg_types.at(i) : create_lua_type_info();
                    define_localvar_in_current_check_proto(mr, name, type_info, true);
                }
            }
        }

		bool GluaTypeChecker::finish_check_current_checking_type(MatchResult *mr)
        {
	        if(_current_checking_proto_stack.size()>0)
	        {
				auto current_proto = _current_checking_proto_stack.at(_current_checking_proto_stack.size() - 1);
				if(current_proto)
				{
					if(current_proto->localvars.size()>LUA_FUNCTION_MAX_LOCALVARS_COUNT)
					{
						set_error(error_in_match_result(mr,
							std::string("too many local variables(") + std::to_string(current_proto->localvars.size())
							+ " variables), but limit count is " + std::to_string(LUA_FUNCTION_MAX_LOCALVARS_COUNT),
							GluaTypeCheckerErrors::TOO_MANY_LOCALVARS_IN_SCOPE));
						_current_checking_proto_stack.pop_back();
						return false;
					}
					_current_checking_proto_stack.pop_back();
					return true;
				}
				_current_checking_proto_stack.pop_back();
	        }
			return false;
        }

        void GluaTypeChecker::generate_record_constructor_code(MatchResult *mr, GluaTypeInfoP record, MatchResult *parent_mr)
        {
            // 生成record对应的构造函数
            // local function <record_name> (props) if(props) 依次检测record字段,没有设置值的用属性默认值  end  end

            if (record->is_record())
            {
                std::stringstream ss;
				auto use_metatable = record->has_meta_method();

				if (!_in_repl)
					ss << "local ";
                ss << "function " << record->record_name << " (props)\n";
                ss << "    props = props or {}\n";
                ss << "    if props then\n";
                for (auto const &item : record->record_default_values)
                {
                    ss << "        props['" << item.first << "'] = props['" << item.first << "'] or " << glua::util::string_trim(item.second) << "\n";
                }
                ss << "    end\n";

				// set metatable
				if (use_metatable)
				{
					// FIXME: a={};a.__index=a;setmetatable(a,a); print(a.b)会loop执行然后报错，可能占用过多的执行时间和资源
					ss << "    setmetatable(props, props)\n";
					_middle_inserted_code_lines += 1;
				}

                ss << "    return props\n";
				ss << "end\n";
                mr->set_hidden_replace_string(ss.str());
				auto lines_added = glua::util::string_lines_count(ss.str())-1;
				_middle_inserted_code_lines += lines_added;
                auto record_constructor_type = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
                record_constructor_type->ret_types.push_back(record);
                record_constructor_type->arg_names.push_back("...");
				record_constructor_type->arg_types.push_back(create_array());
				auto constructor_func_name = record->record_name + ":new";
                define_localvar_in_current_check_proto(mr, constructor_func_name, record_constructor_type, true, true, false, true);
				if (ldf)
					ldf->add_proto_name(constructor_func_name);

				// TODO: _middle_inserted_code_lines 要减去隐藏的mr导致的行数
				auto lines_need_to_remove = mr->linenumber_after_end(parent_mr);
				_middle_inserted_code_lines -= lines_need_to_remove;
            }
        }

		bool GluaTypeChecker::check_bin_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "bin_exp")
			{
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					if (cmr->size() > 2)
					{
						auto *op = cmr->get_item(1);
						auto *exp1 = cmr->get_item(0);
						auto *exp2 = cmr->get_item(2);
						auto exp1_type = guess_exp_type(exp1);
						auto exp2_type = guess_exp_type(exp2);
						std::vector<GluaTypeInfoP> args_types = { exp1_type , exp2_type };
						// 从proto中查找中缀函数来检查
						auto found_op_func = find_operator_func_by_name(mr, op->head_token().token, args_types);
						if (found_op_func->is_function())
						{
							if (found_op_func->arg_types.size() != 2)
							{
								set_error(GluaTypeCheckerErrors::BIN_EXP_ERROR, std::string("bin operand ") + op->head_token().token + " need 2 arguments");
								return false;
							}
							if (!match_function_args(found_op_func, { exp1, exp2 }))
							{
								set_error(error_of_wrong_type_call(cmr, op->head_token(), lua_type_info_to_str(found_op_func->arg_types.at(0)->etype)));
								if (nullptr != result_type)
								{
									result_type->etype = found_op_func->ret_types.size() < 1 ? GluaTypeInfoEnum::LTI_OBJECT : found_op_func->ret_types.at(0)->etype;
								}
								return false;
							}
							if (nullptr != result_type)
							{
								result_type->etype = found_op_func->ret_types.size() < 1 ? GluaTypeInfoEnum::LTI_OBJECT : found_op_func->ret_types.at(0)->etype;
							}
							return true;
						}
						else
						{
							set_error(GluaTypeCheckerErrors::OPERATOR_NOT_FOUND, std::string("Can't find bin op ") + op->head_token().token);
							return false;
						}
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_un_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "un_exp")
			{
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					if (cmr->items.size() > 1)
					{
						auto *op = cmr->get_item(0);
						auto *exp1 = cmr->get_item(1);
						auto exp1_type = guess_exp_type(exp1);
						std::vector<GluaTypeInfoP> args_types = { exp1_type };
						// 从proto中查找前缀函数来检查
						auto found_op_func = find_function_by_name(mr, op->head_token().token, args_types);
						if (found_op_func->is_function())
						{
							if (found_op_func->arg_types.size() != 1)
							{
								set_error(GluaTypeCheckerErrors::OPERATOR_NOT_FOUND, std::string("un_exp operand ") + op->head_token().token + " need 1 arguments");
								return false;
							}
							if (!match_function_args(found_op_func, { exp1 }))
							{
								set_error(error_of_wrong_type_call(cmr, op->head_token(), lua_type_info_to_str(found_op_func->arg_types.at(0)->etype)));
								if (nullptr != result_type)
								{
									result_type->etype = found_op_func->ret_types.size() < 1 ? GluaTypeInfoEnum::LTI_OBJECT : found_op_func->ret_types.at(0)->etype;
								}
								return false;
							}
							if (nullptr != result_type)
							{
								result_type->etype = found_op_func->ret_types.size() < 1 ? GluaTypeInfoEnum::LTI_OBJECT : found_op_func->ret_types.at(0)->etype;
							}
							return true;
						}
						else
						{
							set_error(GluaTypeCheckerErrors::OPERATOR_NOT_FOUND, std::string("Can't find un op ") + op->head_token().token);
							return false;
						}
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_local_def_stat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "local_def_stat")
			{
				auto *cmr = mr->as_complex();
				auto *local_mr = cmr->get_item(0);
				bool is_let = false;
				if (local_mr->is_final() && local_mr->node_name() == "var_declare_prefix")
				{
					is_let = local_mr->as_final()->token.token == "let";
					local_mr->as_final()->token.token = "local";
				}
				auto *namelist_mr = cmr->get_item(1);
				MatchResult *exprlist_mr = cmr->size() >= 4 ? cmr->get_item(3) : nullptr;
				if (namelist_mr->is_complex() && (nullptr == exprlist_mr || exprlist_mr->is_complex()))
				{
					size_t i = 0;
					auto namelist = get_namelist_info_from_mr(namelist_mr, nullptr, false);
					auto namelist_it = namelist.begin();
					auto exprlist = get_explist_from_mr(exprlist_mr);
					for (i = 0; i < namelist.size(); ++i)
					{
						auto p = &(namelist.at(i));
						auto name = p->name;
						auto declared_type_info = p->type_info;
						if(!declared_type_info->is_nil() && !declared_type_info->is_undefined())
							check_is_correct_type_to_use(namelist_mr, declared_type_info, p->type_name);
						auto value_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
						bool inited = false;
						if (exprlist_mr)
						{
							if (i < exprlist.size())
							{
								value_type_info = exprlist.at(i);
								inited = true;
							}
						}
						if (declared_type_info->is_nil())
						{
							set_error(error_in_match_result(namelist_mr, std::string("Can't find type ") + declared_type_info->str()));
							return false;
						}
						// 未声明类型(undefined)的，用推导类型
						if (!value_type_info->is_undefined() && exprlist_mr && !declared_type_info->is_undefined() && !match_declare_type(declared_type_info, value_type_info, true))
						{
							set_error(error_in_match_result(namelist_mr,
								std::string("declare variable ") + name + " type " + declared_type_info->str()
								+ " but got " + value_type_info->str()));
							return false;
						}
						if(value_type_info->is_undefined())
						{
							value_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
						}
						// 这里应该根据是否显式声明类型来判断变量是根据值类型还是声明类型来判断类型,但是根据是否是object声明也行
						define_localvar_in_current_check_proto(namelist_mr, name, 
							declared_type_info->is_undefined() ? value_type_info : declared_type_info, true, !is_let, false, inited);
						++namelist_it;
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_for_range_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
		{
			if (mr->node_name() == "for_range_stat")
			{
				auto *cmr = mr->as_complex();
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				if (proto_tree)
				{
					if (ldf)
					{
						ldf->add_proto_name(std::string("for_range"));
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
                bool result = true;
                // TODO: 检查in后面的类型，从而推导namelist应该的类型
                auto in_explist = cmr->get_item(3);

                printf("");
                auto in_exp_result_type = create_lua_type_info();
                if (!check_expr_error(in_explist, in_exp_result_type, nullptr, in_explist))
                {
                  return false;
                }
                // TODO: 如果in后面跟着的是pairs(a)或ipairs(a)，则namelist的类型可以进行约束
                auto first_value_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
                auto second_value_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
                if (in_explist->is_complex() && in_explist->as_complex()->size()>0)
                {
                  auto in_explist_cmr = in_explist->as_complex();
                  auto first_in_exp = in_explist_cmr->get_item(0);
                  if (first_in_exp->is_complex() && first_in_exp->as_complex()->size() >= 2)
                  {
                    auto first_in_exp_cmr = first_in_exp->as_complex();
                    auto iterator_func_mr = first_in_exp_cmr->get_item(0);
                    if (iterator_func_mr->is_final())
                    {
                      auto iterator_func_name = iterator_func_mr->as_final()->head_token().token;
                      if (iterator_func_name == "pairs" || iterator_func_name == "ipairs")
                      {
                        auto is_ipairs = iterator_func_name == "ipairs";
                        // 分析iterator函数的参数的类型，从而推导for key, value in iterator(collection)的value的类型
                        auto iterator_arg_type_info = guess_exp_type(first_in_exp_cmr->get_item(1));
                        if (iterator_arg_type_info->is_array())
                        {
                          first_value_type_info = this->find_type_by_name(mr, "int");
                          second_value_type_info = iterator_arg_type_info->array_item_type;
                        }
                        else if (iterator_arg_type_info->is_map())
                        {
                          first_value_type_info = this->find_type_by_name(mr, "string");
                          second_value_type_info = iterator_arg_type_info->map_item_type;
                        }
                      }
                    }
                  }
                }

				for(size_t i=0;i < proto_tree->for_namelist.size();i++)
				{
                    const auto &name_type_pair = proto_tree->for_namelist[i];
					// 如果变量已经定义，延续类型
					auto existed_var_info = find_info_by_varname(mr, name_type_pair.name, nullptr, false);
					auto range_var_info = name_type_pair.type_info;
					if(!existed_var_info->is_nil() && !existed_var_info->is_undefined())
					{
						if (!name_type_pair.type_info_defined)
							range_var_info = existed_var_info;
					}
                    GluaTypeInfoP value_type_info = nullptr;
                    if (i == 0)
                    {
                      if (!first_value_type_info->is_undefined())
                      {
                        value_type_info = first_value_type_info;
                      }
                    }
                    else if (i == 1)
                    {
                      if (!second_value_type_info->is_undefined())
                      {
                        value_type_info = second_value_type_info;
                      }
                    }
                    if (value_type_info && value_type_info->etype != GluaTypeInfoEnum::LTI_OBJECT)
                    {
                      // 能推导出in后的迭代器的每一项的类型，则要检查in前变量的类型
                      if (!match_declare_type(range_var_info, value_type_info, true))
                      {
                        result = false;
                        set_error(error_in_match_result(mr, std::string("variable expect ") + range_var_info->str() + " but got " + value_type_info->str()));
                      }
                      range_var_info = value_type_info;
                    }
					define_localvar_in_current_check_proto(mr, name_type_pair.name, range_var_info, true, true, false, true, false);
				}
				for (auto i = proto_tree->mr->as_complex()->items.begin(); i != proto_tree->mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, result_type, ret_type, proto_tree->mr))
					{
						result = false;
					}
				}
				if(result)
				{
					result = finish_check_current_checking_type(mr);
				}
				return result;
			}
			return false;
		}

		bool GluaTypeChecker::check_do_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
		{
			if (mr->node_name() == "do_stat")
			{
				auto *cmr = mr->as_complex();
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				if (proto_tree)
				{
					if (ldf)
					{
						ldf->add_proto_name(std::string("do_stat"));
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
				for (const auto &name_type_pair : proto_tree->for_namelist)
				{
					define_localvar_in_current_check_proto(mr, name_type_pair.name, name_type_pair.type_info, true, true, false, true, false);
				}
				bool result = true;
				for (auto i = proto_tree->mr->as_complex()->items.begin(); i != proto_tree->mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, result_type, ret_type, proto_tree->mr))
					{
						result = false;
					}
				}
				if (result)
				{
					result = finish_check_current_checking_type(mr);
				}
				return result;
			}
			return false;
		}

		bool GluaTypeChecker::check_while_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
		{
			if (mr->node_name() == "while_stat")
			{
				auto *cmr = mr->as_complex();
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				if (proto_tree)
				{
					if (ldf)
					{
						ldf->add_proto_name(std::string("while_stat"));
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
				for (const auto &name_type_pair : proto_tree->for_namelist)
				{
					define_localvar_in_current_check_proto(mr, name_type_pair.name, name_type_pair.type_info, true, true, false, true, false);
				}
				bool result = true;
				if (proto_tree->condition_mr 
					&& !check_expr_error(proto_tree->condition_mr, result_type, nullptr, proto_tree->condition_mr))
				{
					result = false;
				}
				for (auto i = proto_tree->mr->as_complex()->items.begin(); i != proto_tree->mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, result_type, ret_type, proto_tree->mr))
					{
						result = false;
					}
				}
				if (result)
				{
					result = finish_check_current_checking_type(mr);
				}
				return result;
			}
			return false;
		}

		bool GluaTypeChecker::check_repeat_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
		{
			if (mr->node_name() == "repeat_stat")
			{
				auto *cmr = mr->as_complex();
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				if (proto_tree)
				{
					if (ldf)
					{
						ldf->add_proto_name(std::string("repeat_stat"));
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
				for (const auto &name_type_pair : proto_tree->for_namelist)
				{
					define_localvar_in_current_check_proto(mr, name_type_pair.name, name_type_pair.type_info, true, true, false, true, false);
				}
				bool result = true;
				if (proto_tree->condition_mr
					&& !check_expr_error(proto_tree->condition_mr, result_type, nullptr, proto_tree->condition_mr))
				{
					result = false;
				}
				for (auto i = proto_tree->mr->as_complex()->items.begin(); i != proto_tree->mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, result_type, ret_type, proto_tree->mr))
					{
						result = false;
					}
				}
				if (result)
				{
					result = finish_check_current_checking_type(mr);
				}
				return result;
			}
			return false;
		}

        bool GluaTypeChecker::check_break_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
          if (mr->node_name() == "break_stat")
          {
            // TODO: check whether in loop
            return true;
          }
          return false;
        }


		bool GluaTypeChecker::check_if_stat_error(glua::parser::GluaIfStatNode *ast_node, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
		{
			auto parent_tree = current_checking_proto();
			auto proto_tree = find_proto(parent_tree, ast_node, false);
			if (proto_tree)
			{
				if (ldf)
				{
					ldf->add_proto_name(std::string("if_stat"));
				}
				enter_proto_to_checking_type(ast_node, proto_tree);
			}
			bool result = true;
			for (const auto &condition_info : ast_node->condition_mrs())
			{
				result = check_expr_error(condition_info.first, nullptr, nullptr, ast_node) && result; // 条件表达式
				result = check_expr_error(condition_info.second, result_type, ret_type, ast_node) && result; // 条件分支的代码块
			}
			if (ast_node->else_block_mr())
			{
				result = check_expr_error(ast_node->else_block_mr(), result_type, ret_type, ast_node) && result; // else分支的代码块
			}
			if (result && proto_tree)
			{
				result = finish_check_current_checking_type(ast_node);
			}
			return result;
		}


		bool GluaTypeChecker::check_for_step_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
		{
			if (mr->node_name() == "for_step_stat")
			{
				auto *cmr = mr->as_complex();
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				if (proto_tree)
				{
					if (ldf)
					{
						ldf->add_proto_name(std::string("for_step"));
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
				for (const auto &name_type_pair : proto_tree->for_namelist)
				{
					// 如果变量已经定义，延续类型
					auto existed_var_info = find_info_by_varname(mr, name_type_pair.name, nullptr, false);
					auto range_var_info = name_type_pair.type_info;
					if (!existed_var_info->is_nil() && !existed_var_info->is_undefined())
					{
						if (!name_type_pair.type_info_defined)
							range_var_info = existed_var_info;
					}
					define_localvar_in_current_check_proto(mr, name_type_pair.name, range_var_info, true, true, false, true, false);
				}
				bool result = true;
				for (auto i = proto_tree->mr->as_complex()->items.begin(); i != proto_tree->mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, result_type, ret_type, proto_tree->mr))
					{
						result = false;
					}
				}
				if (result)
				{
					result = finish_check_current_checking_type(mr);
				}
				return result;
			}
			return false;
		}

		bool GluaTypeChecker::check_functiondef_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "functiondef")
			{
				auto *cmr = mr->as_complex();
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				if (proto_tree)
				{
					if (ldf)
					{
						ldf->add_proto_name(std::string(""));
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
				auto ret_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				auto *funcbody_mr = cmr->get_item(1)->as_complex();
				if (!check_expr_error(funcbody_mr->get_item(1), nullptr, ret_type_info))
				{
					finish_check_current_checking_type(mr);
					return false;
				}
				if (ret_type_info->etype == GluaTypeInfoEnum::LTI_UNION && ret_type_info->union_types.size() < 1)
					copy_lua_type_info(ret_type_info, create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
				if (proto_tree)
				{
					bool result = finish_check_current_checking_type(mr);
					auto func_type = guess_exp_type(funcbody_mr);
					if (func_type->ret_types.size() == 1 && func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_OBJECT)
					{
						func_type->ret_types.clear();
						func_type->ret_types.push_back(ret_type_info);
					}
					if (func_type->ret_types.size() == 1
						&& func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_UNION
						&& func_type->ret_types.at(0)->union_types.size() == 1)
					{
						auto first_result_type = func_type->ret_types.at(0);
						func_type->ret_types.clear();
						func_type->ret_types.push_back(*(first_result_type->union_types.begin()));
					}
					if (nullptr != result_type)
						copy_lua_type_info(result_type, func_type);
					if (!result)
						return false;
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_local_named_function_def_stat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "local_named_function_def_stat")
			{
				auto *cmr = mr->as_complex();
				auto *local_mr = cmr->get_item(0);
				bool is_let = false;
				if (local_mr->is_final() && local_mr->node_name() == "var_declare_prefix")
				{
					is_let = local_mr->as_final()->token.token == "let";
					local_mr->as_final()->token.token = "local";
				}
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				std::string varname;
				if (proto_tree)
				{
					auto *funcname_mr = mr->as_complex()->get_item(2);
					if (funcname_mr->is_complex() && funcname_mr->as_complex()->size() == 1)
					{
						varname = funcname_mr->head_token().token;
					}
					else if (funcname_mr->is_final())
					{
						varname = funcname_mr->as_final()->token.token;
					}

					if (ldf)
					{
						ldf->add_proto_name(varname);
					}

					enter_proto_to_checking_type(mr, proto_tree);
				}
				auto ret_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				for (auto i = mr->as_complex()->items.begin(); i != mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, nullptr, ret_type_info))
					{
						finish_check_current_checking_type(mr);
						return false;
					}
				}
				if (ret_type_info->etype == GluaTypeInfoEnum::LTI_UNION && ret_type_info->union_types.size() < 1)
					copy_lua_type_info(ret_type_info, create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
				if (proto_tree)
				{
					bool result = finish_check_current_checking_type(mr);
					if (varname.length() > 0)
					{
						auto func_type = guess_exp_type(mr->as_complex()->get_item(3));
						if (func_type->ret_types.size() == 1 && func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_OBJECT)
						{
							func_type->ret_types.clear();
							func_type->ret_types.push_back(ret_type_info);
						}
						if (func_type->ret_types.size() == 1
							&& func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_UNION
							&& func_type->ret_types.at(0)->union_types.size() == 1)
						{
							auto first_result_type = func_type->ret_types.at(0);
							func_type->ret_types.clear();
							func_type->ret_types.push_back(*(first_result_type->union_types.begin()));
						}
						define_localvar_in_current_check_proto(mr, varname, func_type, true, !is_let);
					}
					if (!result)
						return false;
				}
			}
			return true;
        }

		void GluaTypeChecker::add_created_global_variable(std::string varname, size_t linenumber)
        {
			if (varname.length() < 1)
				return;
			if (_created_global_variables.find(varname) != _created_global_variables.end())
				return;
			_created_global_variables[varname] = linenumber;
        }

		bool GluaTypeChecker::check_named_function_def_stat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "named_function_def_stat")
			{
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				std::string varname;
				if (proto_tree)
				{
					// 如果function a.b(...) ... end中a是record类型，则不允许function a.b定义成员方法，只能function a:b
					auto *funcname_mr = mr->as_complex()->get_item(1);
					if (funcname_mr->node_name() == "funcname")
					{
						if (funcname_mr->is_complex() && funcname_mr->as_complex()->size() == 1)
						{
							varname = funcname_mr->head_token().token;
							add_created_global_variable(varname, funcname_mr->head_token().linenumber - _ctx->inner_lib_code_lines() + 2);
						}
						else if (funcname_mr->is_final())
						{
							varname = funcname_mr->as_final()->token.token;
							add_created_global_variable(varname, funcname_mr->head_token().linenumber - _ctx->inner_lib_code_lines() + 2);
						} else if(funcname_mr->is_complex() && funcname_mr->as_complex()->size()>1)
						{
							auto funcname_cmr = funcname_mr->as_complex();
							auto obj_name = funcname_cmr->get_item(0)->head_token().token;
							auto dot_token = funcname_cmr->get_item(1)->head_token().token;
							auto obj_type = find_info_by_varname(funcname_mr, obj_name);
							if(obj_type->is_record() && dot_token == ".")
							{
								set_error(error_in_match_result(funcname_mr, 
									std::string("should use '<varname>:<funcname>' to define record's member function, don't use '<varname>.<funcname>' when varname is record type")));
								return false;
							}
						}
					}

					if (ldf)
					{
						ldf->add_proto_name(varname);
					}

					enter_proto_to_checking_type(mr, proto_tree);
				}
				auto ret_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				for (auto i = mr->as_complex()->items.begin(); i != mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, nullptr, ret_type_info))
					{
						finish_check_current_checking_type(mr);
						return false;
					}
				}
				if (ret_type_info->etype == GluaTypeInfoEnum::LTI_UNION && ret_type_info->union_types.size() < 1)
					copy_lua_type_info(ret_type_info, create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
				if (proto_tree)
				{
					if (!finish_check_current_checking_type(mr))
						return false;
					if (varname.length() > 0)
					{
						auto func_type = guess_exp_type(mr->as_complex()->get_item(2));
						if (func_type->ret_types.size() == 1 && func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_OBJECT)
						{
							func_type->ret_types.clear();
							func_type->ret_types.push_back(ret_type_info);
						}
						if (func_type->ret_types.size() == 1
							&& func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_UNION
							&& func_type->ret_types.at(0)->union_types.size() == 1)
						{
							auto first_result_type = func_type->ret_types.at(0);
							func_type->ret_types.clear();
							func_type->ret_types.push_back(*(first_result_type->union_types.begin()));
						}
						define_localvar_in_current_check_proto(mr, varname, func_type, false);
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_offline_named_function_def_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "offline_named_function_def_stat")
			{
				auto parent_tree = current_checking_proto();
				auto proto_tree = find_proto(parent_tree, mr, false);
				std::string varname;
				if (proto_tree)
				{
					auto *funcname_mr = mr->as_complex()->get_item(2);
					if (funcname_mr->node_name() == "funcname")
					{
						if (funcname_mr->is_complex() && funcname_mr->as_complex()->size() == 1)
						{
							varname = funcname_mr->head_token().token;
						}
						else if (funcname_mr->is_final())
						{
							varname = funcname_mr->as_final()->token.token;
						}
					}
					if (ldf)
					{
						ldf->add_proto_name(varname);
					}
					enter_proto_to_checking_type(mr, proto_tree);
				}
				auto ret_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				for (auto i = mr->as_complex()->items.begin(); i != mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, nullptr, ret_type_info))
					{
						finish_check_current_checking_type(mr);
						return false;
					}
				}
				if (ret_type_info->etype == GluaTypeInfoEnum::LTI_UNION && ret_type_info->union_types.size() < 1)
					copy_lua_type_info(ret_type_info, create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));

				if (proto_tree)
				{
					if (!finish_check_current_checking_type(mr))
						return false;
					if (varname.length() > 0)
					{
						auto func_type = guess_exp_type(mr->as_complex()->get_item(3));
						if (func_type->ret_types.size() == 1 && func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_OBJECT)
						{
							func_type->ret_types.clear();
							func_type->ret_types.push_back(ret_type_info);
						}
						if (func_type->ret_types.size() == 1
							&& func_type->ret_types.at(0)->etype == GluaTypeInfoEnum::LTI_UNION
							&& func_type->ret_types.at(0)->union_types.size() == 1)
						{
							auto first_result_type = func_type->ret_types.at(0);
							func_type->ret_types.clear();
							func_type->ret_types.push_back(*(first_result_type->union_types.begin()));
						}
						define_localvar_in_current_check_proto(mr, varname, func_type, true);
					}
					if (result_type)
						copy_lua_type_info(result_type, proto_tree->type_info);
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_tableconstructor_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "tableconstructor")
			{
				// 把[...]换成{...}
				// 把{a: ...}换成{a=...}
				// 把{'a': ... } 换成{['a']=...}
				// 对于错误的字面量table语法要报错，[]中有非数组内容也要报错
				auto cmr = mr->as_complex();
				bool is_pure_array = cmr->get_item(0)->as_final()->token.token == "[";
				if(is_pure_array)
				{
					cmr->get_item(0)->set_hidden(true);
					cmr->get_item(0)->set_hidden_replace_string("{");
					cmr->get_item(cmr->size()-1)->set_hidden(true);
					cmr->get_item(cmr->size()-1)->set_hidden_replace_string("}");
				}

				bool result = true;
				// TODO: 对于[1, 2.5, 3]这类字面量，推导类型要推导为Array<number>，元素值类型要按最小共同父类型
				auto table_item_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
				std::vector<GluaTypeInfoP> value_item_types; // 各项的值类型，用来计算最小共同父类型
				bool is_empty = true;
				for (size_t i = 1;i<cmr->size()-1;++i)
				{
					auto *item = cmr->get_item(i);
					if(item->node_name()=="fieldlist")
					{
						auto *fieldlist_cmr = item->as_complex();
						bool has_array_part = false;
						bool has_hashmap_part = false;
						for(const auto &field_sep_value_pair_mr : fieldlist_cmr->items)
						{
							is_empty = false;
							auto field_mr = field_sep_value_pair_mr->as_complex()->get_item(0);
							GluaTypeInfoP field_value_type;

							if(field_mr->node_name()=="array_index_field")
							{
								if (is_pure_array)
								{
									set_error(error_in_match_result(mr, std::string("can't add hashmap part in array")));
									continue;
								}
								has_hashmap_part = true;
								auto *field_cmr = field_mr->as_complex();
								auto key_type = guess_exp_type(field_cmr->get_item(1));
								if(!key_type->is_string())
								{
									set_error(error_in_match_result(field_mr,
										std::string("only string can be table key")));
								}
								field_value_type = guess_exp_type(field_cmr->get_item(4));
							} 
							else if(field_mr->node_name() == "map_index_field")
							{
								if(is_pure_array)
								{
									set_error(error_in_match_result(mr, std::string("can't add hashmap part in array")));
									continue;
								}
								has_hashmap_part = true;
								auto *field_cmr = field_mr->as_complex();
								if(field_cmr->size()>=3 && field_cmr->get_item(1)->head_token().token==":")
								{
									field_cmr->get_item(1)->set_hidden(true);
									field_cmr->get_item(1)->set_hidden_replace_string("=");
								}
								field_value_type = guess_exp_type(field_cmr->get_item(2));
							}
							else if (field_mr->node_name() == "string_map_index_field")
							{
								if (is_pure_array)
								{
									set_error(error_in_match_result(mr, std::string("can't add hashmap part in array")));
									continue;
								}
								has_hashmap_part = true;
								auto *field_cmr = field_mr->as_complex();

								auto field_item0_mr = field_cmr->get_item(0);
								field_item0_mr->set_hidden(true);
								field_item0_mr->set_hidden_replace_string(std::string("[") + field_item0_mr->as_final()->token.source_token + "]");

								field_cmr->get_item(1)->set_hidden(true);
								field_cmr->get_item(1)->set_hidden_replace_string("=");

								field_value_type = guess_exp_type(field_cmr->get_item(2));
							}
							else
							{
								if(!is_pure_array)
								{
									set_error(error_in_match_result(mr, std::string("Can't put array part in Map")));
								}
								field_value_type = guess_exp_type(field_mr);
								has_array_part = true;
							}

							value_item_types.push_back(field_value_type);

							if(table_item_type_info->is_undefined())
							{
								table_item_type_info = field_value_type;
							}
							else
							{
								if(table_item_type_info->etype != field_value_type->etype
									&& table_item_type_info->etype != GluaTypeInfoEnum::LTI_OBJECT)
								{
									// 用最小共同父类型作为table_item_type_info
									table_item_type_info = min_sharing_declarative_type(table_item_type_info, field_value_type);
								}
							}
						}
						// 检查是否数组和哈希表部分混用了
						if(has_hashmap_part && has_array_part)
						{
							set_error(error_in_match_result(mr, std::string("Can't put array and hashmap items in one table together")));
						}
					}
					if (!check_expr_error(item))
					{
						result = false;
					}
				}
				if(table_item_type_info->is_undefined())
				{
					table_item_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
					is_empty = true;
				}
				GluaTypeInfoP type_info;
				if (is_pure_array)
				{
					type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_ARRAY);
					type_info->array_item_type = table_item_type_info;
				}
				else
				{
					type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_MAP);
					type_info->map_item_type = table_item_type_info;
				}
				if (is_empty)
					type_info->is_literal_empty_table = true;
				if (result_type)
				{
					copy_lua_type_info(result_type, type_info);
				}
				return result;
			}
			return true;
        }

		bool GluaTypeChecker::check_functioncall_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "functioncall")
			{
				// check type
				auto funccall_result_type = guess_exp_type(mr);
				bool result = true;
				for (auto i = mr->as_complex()->items.begin(); i != mr->as_complex()->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item))
					{
						result = false;
					}
				}
				if (result_type)
				{
					copy_lua_type_info(result_type, funccall_result_type);
				}
				return result;
			}
			return true;
        }

		bool GluaTypeChecker::check_suffixedexp_visit_prop_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "suffixedexp_visit_prop" || mr->node_name() == "var")
			{
				// 检查属性访问，判断符号类型是否可以访问对应属性
				bool result = true;
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					for (auto i = cmr->items.begin(); i != cmr->items.end(); ++i)
					{
						auto *item = *i;
						if (!check_expr_error(item, nullptr, ret_type))
						{
							result = false;
						}
					}
					if (nullptr != result_type)
						copy_lua_type_info(result_type, create_lua_type_info());
					auto *prefixexp_mr = cmr->get_item(0);
					auto obj_type_info = guess_exp_type(prefixexp_mr);
					auto *prop_mr = cmr->get_item(2);
					bool is_dot_visit = cmr->get_item(1)->head_token().token == ".";
					if (prop_mr->is_final()
						&& (prop_mr->as_final()->token.type == TOKEN_RESERVED::LTK_STRING
						|| prop_mr->as_final()->token.type == TOKEN_RESERVED::LTK_INT
						|| prop_mr->as_final()->token.type == TOKEN_RESERVED::LTK_NAME))
					{
						// 如果用字符串类型prop，而obj是数组，要报错
						// 如果obj是数组，索引不是int或int型name，要报错
						// 对于非数组的obj，要考虑对索引不是name和string的支持
						auto prop_token = prop_mr->head_token();
						auto prop_name = prop_token.type == TOKEN_RESERVED::LTK_NAME ? prop_token.token : prop_token.token;
						GluaTypeInfoEnum prop_name_type;
						if (prop_token.type == TOKEN_RESERVED::LTK_NAME)
							prop_name_type = is_dot_visit ? GluaTypeInfoEnum::LTI_STRING : find_info_by_varname(prop_mr, prop_name)->etype;
						else
							prop_name_type = guess_exp_type(prop_mr)->etype;
						if (obj_type_info->is_contract_type() && is_checking_contract)
						{
							if (obj_type_info != find_info_by_varname(mr, "self"))
							{
								if (glua::util::vector_contains(thinkyoung::lua::lib::contract_special_api_names, prop_name)
									|| prop_name == "storage")
								{
									set_error(error_in_match_result(mr, std::string("Can't access contract's ") + prop_name + " property", GluaTypeCheckerErrors::ACCESS_CONTRACT_PROPERTY_DISABLE));
									result = false;
								}
							}
							else
							{
								if (glua::util::vector_contains(thinkyoung::lua::lib::contract_special_api_names, prop_name))
								{
									set_error(error_in_match_result(mr, std::string("Can't access contract's ") + prop_name + " property", GluaTypeCheckerErrors::ACCESS_CONTRACT_PROPERTY_DISABLE));
									result = false;
								}
							}
						}else if (!can_access_prop_of_type(mr, obj_type_info, prop_name, prop_name_type))
						{
							set_error(error_in_match_result(prop_mr, std::string("type ") + obj_type_info->str() + " can't access property " + prop_name));
							result = false;
						}
							
						if (result && (obj_type_info->is_record()
							|| obj_type_info->is_array()
							|| obj_type_info->is_map()))
						{
							// 如果是访问record的属性，那这里类型可以访问
							// 需要考虑a.b.c这种情况的类型推导
							auto value_type = obj_type_info;
							auto *cur_cmr = cmr;
							std::string cur_prop_name = prop_name;
							bool cur_is_dot_visit = is_dot_visit;
							GluaTypeInfoEnum cur_prop_name_type = prop_name_type;
							size_t next_pos = cur_is_dot_visit ? 3 : 4;
							do
							{
								if (!value_type->is_record()
									&& !value_type->is_array()
									&& !value_type->is_map())
								{
									value_type = create_lua_type_info();
									break;
								}
								if (!can_access_prop_of_type(cur_cmr, value_type, cur_prop_name, cur_prop_name_type))
								{
									set_error(error_in_match_result(cur_cmr, std::string("Can't access ") + cur_prop_name + " of " + value_type->str()));
									if (nullptr != result_type)
										copy_lua_type_info(result_type, value_type);
									return result;
								}
								if (cur_prop_name_type == GluaTypeInfoEnum::LTI_INT || cur_prop_name_type == GluaTypeInfoEnum::LTI_NUMBER)
								{
									if (!can_visit_index_by_number(value_type))
									{
										set_error(error_in_match_result(prop_mr, std::string("type ") + value_type->str() + " can't access index " + cur_prop_name));
										result = false;
									}
									else
									{
										value_type = get_index_by_number_type(value_type);
									}
								}
								else if (value_type->is_record())
								{
									auto found = value_type->record_props.find(cur_prop_name);
									if (found == value_type->record_props.end())
									{
										set_error(error_in_match_result(cur_cmr, std::string("Can't access ") + cur_prop_name + " of " + value_type->str()));
										if (nullptr != result_type)
											copy_lua_type_info(result_type, value_type);
										return result;
									}
									value_type = found->second;
								}
								else if(value_type->is_map())
								{
									value_type = value_type->map_item_type;
								}
								else if (value_type->is_array())
								{
									value_type = value_type->array_item_type;
								}
								if (cmr->size() <= next_pos || !cmr->get_item(next_pos)->is_complex())
									break;
								cur_cmr = cmr->get_item(next_pos)->as_complex();
								next_pos += 1;
								cur_prop_name = cur_cmr->get_item(1)->head_token().token;
								cur_is_dot_visit = cur_cmr->get_item(0)->head_token().token == ".";
								if (cur_cmr->get_item(1)->head_token().type == TOKEN_RESERVED::LTK_NAME)
									cur_prop_name_type = cur_is_dot_visit ? GluaTypeInfoEnum::LTI_STRING : find_info_by_varname(cur_cmr, cur_prop_name)->etype;
								else
									cur_prop_name_type = guess_exp_type(cur_cmr->get_item(1))->etype;
							} while (true);
							if (result_type)
								copy_lua_type_info(result_type, value_type);
							return result;
						}
					}
				}
				return result;
			}
			return true;
        }

		bool GluaTypeChecker::check_retstat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "retstat")
			{
				auto *cmr = mr->as_complex();
				bool result = true;
				if (cmr->size() > 1 && cmr->get_item(1)->node_name() == "explist")
				{
					auto *explist_mr = cmr->get_item(1);
					auto ret_exp_type_info = create_lua_type_info();
					if (!check_expr_error(explist_mr, ret_exp_type_info, nullptr))
					{
						result = false;
					}
					if (result)
					{
						bool has_error = false;
						auto explist = get_explist_from_mr(explist_mr, &has_error);
						if (has_error)
							result = false;
						if (explist.size() > 1)
						{
							set_error(error_in_match_result(mr, "return too more values(can only accept 1 or 0 value)", GluaTypeCheckerErrors::RETURN_TOO_MORE_VALUES));
						}
					}
					if (ret_type)
					{
						auto copied = create_lua_type_info();
						copy_lua_type_info(copied, ret_type); // 拷贝一份再merge是为了避免嵌套类型时修改了自身
						auto merge_result = merge_union_types(copied, ret_exp_type_info);
						copy_lua_type_info(ret_type, merge_result);
					}
					if (result_type)
					{
						copy_lua_type_info(result_type, ret_exp_type_info);
					}
					// TODO: 标记当前scope已经return过了，不能重复return(要考虑if, for, while等控制结构的情况下可能多个return，所以控制结构语句要有独立scope)
				}
				else
				{
					if (result_type)
					{
						copy_lua_type_info(result_type, create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
					}
				}
				return result;

			}
			return true;
        }

		bool GluaTypeChecker::check_varlist_assign_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "varlist_assign_stat")
			{
				auto *cmr = mr->as_complex();
				auto *varlist_mr = cmr->get_item(0)->as_complex();
				auto *explist_mr = cmr->get_item(2)->as_complex();
				auto val_type_info = create_lua_type_info();
				bool result = true;
				for (auto i = explist_mr->items.begin(); i != explist_mr->items.end(); ++i)
				{
					auto *item = *i;
					if (!check_expr_error(item, val_type_info, nullptr))
					{
						result = false;
					}
				}
				auto explist = get_explist_from_mr(explist_mr);
				auto var_mr_list = get_varlist_from_mr(varlist_mr);
				assert(var_mr_list.size() > 0);
				auto min_size = var_mr_list.size() > explist.size() ? explist.size() : var_mr_list.size();
				for(size_t i=0;i <min_size;++i)
				{
					auto *var_mr = var_mr_list.at(i);
					if (var_mr->is_final())
					{
						auto varname = var_mr->as_final()->token.token;
						// 检查varname是否declare过了（可能declare了但是没有赋值），从而判断是否是新建全局变量
						auto exist_var_type = find_info_by_varname(var_mr, varname, nullptr, false);
						auto is_new_global_var = exist_var_type->is_undefined() || exist_var_type->is_nil();
						define_localvar_in_current_check_proto(mr, varname, explist.at(i), is_new_global_var, true, false, true, is_new_global_var);
						if(is_new_global_var)
						{
							add_created_global_variable(varname, var_mr->head_token().linenumber - _ctx->inner_lib_code_lines() + 2);
						}
					}
					else
					{
						// 如果是a.b[.c] = d 这样形式，并且左侧是访问record的字段，则要做类型检查
						auto left_type = guess_exp_type(var_mr);
						if (!match_declare_type(left_type, explist.at(i), true))
						{
							set_error(error_in_match_result(mr, std::string("assign statement type error, declare type is ")
								+ left_type->str() + " and value type is " 
								+ explist.at(i)->str()));
						}
						auto *var_cmr = var_mr->as_complex();
						if(var_cmr->size()==3 || (var_cmr->size()==4 && var_cmr->get_item(3)->is_final()))
						{
							// 如果是a.b或a['b']这样的形式
							auto first_var_token = var_cmr->get_item(0)->head_token();
							auto first_var_type = find_info_by_varname(var_cmr, first_var_token.token);
							if(first_var_type->is_contract_type() && is_checking_contract)
							{
								// 如果修改的是合约的id/name/storage属性，报错
								const auto &prop_token_to_change = var_cmr->get_item(2)->head_token();
								std::string prop_name_to_change;
								if(prop_token_to_change.type == TOKEN_RESERVED::LTK_NAME)
									prop_name_to_change = prop_token_to_change.token;
								else if(prop_token_to_change.type == TOKEN_RESERVED::LTK_STRING)
								{
									prop_name_to_change = prop_token_to_change.token;
								}
								if(prop_name_to_change == "id" || prop_name_to_change == "name" || prop_name_to_change == "storage")
								{
									set_error(error_in_match_result(mr, std::string("Can't change contract's ") + prop_name_to_change + " property"));
								}
							}
						}
					}
				}
				return result;
			}
			return true;
        }

		bool GluaTypeChecker::check_record_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type, MatchResult *parent_mr)
        {
			if (mr->node_name() == "record")
			{
				mr->set_hidden(true);
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					if ((cmr->size() == 6 || cmr->size() == 5) && cmr->get_item(1)->is_final())
					{
						auto record_name = cmr->get_item(1)->head_token().token;
						MatchResult *namelist_mr = cmr->size() == 6 ? cmr->get_item(4) : nullptr;
						// 从namelist_mr中获取各字段类型，并放入record类型中
						// 考虑到可能record中属性递归属于本record类型，所以解析record内容时，要先定义record，这里要注意可能的循环引用导致内存泄露
						auto record = create_lua_type_info(GluaTypeInfoEnum::LTI_RECORD);
						record->record_name = record_name;
						record->record_origin_name = record_name;
						define_localvar_in_current_check_proto(mr, std::string(GLUA_TYPE_NAMESPACE_PREFIX) + record_name, record);
						define_localvar_in_current_check_proto(mr, record_name, record); // 产生同名构造函数
						define_local_type_in_current_check_proto(mr, record_name, record);
						auto *cur_namelist_mr = namelist_mr;
						while(cur_namelist_mr) {
							if (cur_namelist_mr->is_complex() && cur_namelist_mr->as_complex()->size() > 0)
							{
								auto *name_mr = cur_namelist_mr->as_complex()->get_item(0);
								if(name_mr->is_final())
								{
									set_error(error_in_match_result(name_mr, std::string("when define record type, property's type declaration is required")));
								}
								if (!name_mr->is_complex() || name_mr->node_name() != "name_in_record_with_type_declare")
									break;
								auto prop_name = name_mr->as_complex()->get_item(0)->head_token().token;
								auto *type_declare_prefix_mr = name_mr->as_complex()->get_item(1);
								bool is_optional_colon = type_declare_prefix_mr->head_token().type == TOKEN_RESERVED::LTK_OPTIONAL_COLON;
								auto *item2 = name_mr->as_complex()->get_item(2);
								GluaTypeInfoP prop_type_info;
								if (item2->is_final())
								{
									auto prop_type_str = item2->as_final()->token.token;
									prop_type_info = find_info_by_varname(item2, prop_type_str);
								}
								else
								{
									prop_type_info = get_type_from_mr(name_mr->as_complex()->get_item(2), nullptr);
								}
								if (prop_type_info->is_nil() || prop_type_info->is_undefined())
								{
									set_error(error_in_match_result(mr, std::string("Can't use nil or undefined type in record definition")));
								}
								if(is_optional_colon)
								{
									if(!prop_type_info->is_union()
										&& !prop_type_info->is_nil()
										&& !prop_type_info->is_undefined())
									{
										auto new_prop_type = create_lua_type_info(GluaTypeInfoEnum::LTI_UNION);
										new_prop_type->union_types.insert(prop_type_info);
										new_prop_type->union_types.insert(create_lua_type_info(GluaTypeInfoEnum::LTI_NIL));
										prop_type_info = new_prop_type;
									}
								}
								record->record_props[prop_name] = prop_type_info;

								if (name_mr->node_name() == "name_in_record_with_type_declare" && name_mr->as_complex()->size() >= 5)
								{
									auto *item5 = name_mr->as_complex()->get_item(4);
									// record字段的默认值
									auto item_type = create_lua_type_info();
									check_expr_error(item5, item_type, nullptr);
									if (!match_declare_type(prop_type_info, item_type, true))
									{
										set_error(error_in_match_result(name_mr,
											std::string("default value type error of record ") + record_name + " property " + prop_name));
									}
									else
									{
										// TODO: 如果默认值是函数，则自动给加入self参数，并且语法分析时也要按照有self的函数来处理
										std::string default_value_code = "nil";
										auto token_parser = std::make_shared<GluaTokenParser>((lua_State *) nullptr);
										auto tokens = dump_mr_tokens(item5);
										std::vector<GluaParserToken> token_list;
										token_list.insert(token_list.end(), tokens.begin(), tokens.end());
										token_parser->replace_all_tokens(token_list);
										default_value_code = glua::util::string_trim(token_parser->dump());
										record->record_default_values[prop_name] = default_value_code;
									}
								}

								if (cur_namelist_mr->as_complex()->size() < 3)
									break;
								cur_namelist_mr = cur_namelist_mr->as_complex()->get_item(2);
							}
							else
								break;
						}

						generate_record_constructor_code(mr, record, parent_mr);
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_record_with_generic_state_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type, MatchResult *parent_mr)
        {
			if (mr->node_name() == "record_with_generic")
			{
				mr->set_hidden(true);
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					if ((cmr->size() == 9 || cmr->size() == 8) && cmr->get_item(1)->is_final())
					{
						auto record_name = cmr->get_item(1)->head_token().token;
						auto generic_list_mr = cmr->get_item(3)->as_complex();
						auto generic_names = get_generic_list_from_mr(generic_list_mr);
						MatchResult *namelist_mr = cmr->size() == 9 ? cmr->get_item(7) : nullptr;
						// 从namelist_mr中获取各字段类型，并放入record类型中
						// 考虑到可能record中属性递归属于本record类型，所以解析record内容时，要先定义record，这里要注意可能的循环引用导致内存泄露
						auto record = create_lua_type_info(GluaTypeInfoEnum::LTI_RECORD);
						record->record_name = record_name;
						record->record_origin_name = record_name;
						auto generic_name_type_map = std::make_shared<GluaExtraBindingsType>();
						for (const auto &generic_name : generic_names)
						{
							auto generic_type = create_lua_type_info(GluaTypeInfoEnum::LTI_GENERIC);
							generic_type->generic_name = generic_name;
							generic_name_type_map->set_variable(std::string(GLUA_TYPE_NAMESPACE_PREFIX) + generic_name, generic_type);
							generic_name_type_map->set_type(generic_name, generic_type);
							record->record_generics.push_back(generic_type);
						}
						record->record_all_generics = record->record_generics;
						define_localvar_in_current_check_proto(mr, std::string(GLUA_TYPE_NAMESPACE_PREFIX) + record_name, record);
						define_localvar_in_current_check_proto(mr, record_name, record); // 产生同名构造函数
						define_local_type_in_current_check_proto(mr, record_name, record);
						auto *cur_namelist_mr = namelist_mr;
						while(cur_namelist_mr) {
							if (cur_namelist_mr->is_complex() && cur_namelist_mr->as_complex()->size() > 0)
							{
								auto *name_mr = cur_namelist_mr->as_complex()->get_item(0);
								if (name_mr->is_final())
								{
									set_error(error_in_match_result(name_mr, std::string("when define record type, property's type declaration is required")));
								}
								if (!name_mr->is_complex() || name_mr->node_name() != "name_in_record_with_type_declare")
									break;
								auto prop_name = name_mr->as_complex()->get_item(0)->head_token().token;
								auto *item2 = name_mr->as_complex()->get_item(2);
								GluaTypeInfoP prop_type_info;
								if (item2->is_final())
								{
									auto prop_type_str = item2->as_final()->token.token;
									prop_type_info = generic_name_type_map->find_type(prop_type_str);
									if (!prop_type_info)
										prop_type_info = find_type_by_name(item2, prop_type_str);
								}
								else
								{
									prop_type_info = get_type_from_mr(name_mr->as_complex()->get_item(2), generic_name_type_map);
								}
								if (prop_type_info->is_nil() || prop_type_info->is_undefined())
								{
									set_error(error_in_match_result(mr, std::string("Can't use nil or undefined type in generic record type definition")));
								}
								record->record_props[prop_name] = prop_type_info;
								if (name_mr->as_complex()->size() >= 5)
								{
									auto *item5 = name_mr->as_complex()->get_item(4);
									// record字段的默认值
									auto item_type = create_lua_type_info();
									check_expr_error(item5, item_type, nullptr);
									if (!match_declare_type(prop_type_info, item_type, true))
									{
										set_error(error_in_match_result(name_mr,
											std::string("default value type error of record ") + record_name + " property " + prop_name));
									}
									else
									{
										// TODO: 如果默认值是函数，则自动给加入self参数，并且语法分析时也要按照有self的函数来处理
										std::string default_value_code = "nil";
										auto token_parser = std::make_shared<GluaTokenParser>((lua_State *) nullptr);
										auto tokens = dump_mr_tokens(item5);
										std::vector<GluaParserToken> token_list;
										token_list.insert(token_list.end(), tokens.begin(), tokens.end());
										token_parser->replace_all_tokens(token_list);
										default_value_code = glua::util::string_trim(token_parser->dump());
										record->record_default_values[prop_name] = default_value_code;
									}
								}
								if (cur_namelist_mr->as_complex()->size() < 3)
									break;
								cur_namelist_mr = cur_namelist_mr->as_complex()->get_item(2);
							}
							else
								break;
						}

						// 生成record对应的构造函数
						// local function <record_name> (props) if(props) 依次检测record字段,没有设置值的用属性默认值  end  end

						generate_record_constructor_code(mr, record, parent_mr);
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_typedef_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			if (mr->node_name() == "typedef")
			{
				mr->set_hidden(true);
				if (mr->is_complex())
				{
					auto *cmr = mr->as_complex();
					auto *new_type_mr = cmr->get_item(1);
					auto *old_type_mr = cmr->get_item(3);
					bool is_generic_type = new_type_mr->is_complex() && new_type_mr->as_complex()->size() == 4;
					std::string record_name;
					GluaTypeInfoP record;
					if (is_generic_type)
					{
						auto *new_type_cmr = new_type_mr->as_complex();
						record_name = new_type_cmr->get_item(0)->as_final()->token.token;
						auto generic_list_mr = new_type_cmr->get_item(2)->as_complex();
						auto generic_names = get_generic_list_from_mr(generic_list_mr);
						record = create_lua_type_info(GluaTypeInfoEnum::LTI_RECORD);
						record->record_name = record_name;
						auto extra_bindings = std::make_shared<GluaExtraBindingsType>();
						for (const auto &generic_name : generic_names)
						{
							auto generic_type = create_lua_type_info(GluaTypeInfoEnum::LTI_GENERIC);
							generic_type->generic_name = generic_name;
							extra_bindings->set_variable(std::string(GLUA_TYPE_NAMESPACE_PREFIX) + generic_name, generic_type);
							record->record_generics.push_back(generic_type);
							extra_bindings->set_type(generic_name, generic_type);
						}
						define_localvar_in_current_check_proto(mr, std::string(GLUA_TYPE_NAMESPACE_PREFIX) + record_name, record);
						define_localvar_in_current_check_proto(mr, record_name, record); // 产生同名构造函数
						define_local_type_in_current_check_proto(mr, record_name, record);
						auto type_info = get_type_from_mr(old_type_mr, extra_bindings);
						copy_lua_type_info(record, type_info);
						record->record_name = record_name;
						record->record_origin_name = type_info->record_origin_name;
					}
					else
					{
						record_name = new_type_mr->as_final()->token.token;
						record = create_lua_type_info(GluaTypeInfoEnum::LTI_RECORD);
						define_localvar_in_current_check_proto(mr, std::string(GLUA_TYPE_NAMESPACE_PREFIX) + record_name, record);
						define_localvar_in_current_check_proto(mr, record_name, record); // 产生同名构造函数
						define_local_type_in_current_check_proto(mr, record_name, record);
						auto old_type = get_type_from_mr(old_type_mr);
						copy_lua_type_info(record, old_type);
						record->record_name = record_name;
						record->record_origin_name = old_type->record_origin_name;
					}

					if (record->is_record())
					{
						std::stringstream ss;
						ss << "local " << record_name << " = " << old_type_mr->head_token().token << "\n";
						mr->set_hidden_replace_string(ss.str());
						auto record_constructor_type = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
						record_constructor_type->ret_types.push_back(record);
						record_constructor_type->arg_names.push_back("...");
						record_constructor_type->arg_types.push_back(create_array());
						define_localvar_in_current_check_proto(mr, record_name + ":new", record_constructor_type, true, true, false, true);
					}
				}
			}
			return true;
        }

		bool GluaTypeChecker::check_emit_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
	        if(mr->node_name() == "emit_stat")
	        {
				auto *cmr = mr->as_complex();
				auto *emit_mr = cmr->get_item(0);
				auto *event_type_mr = cmr->get_item(1);
				auto *arg_mr = cmr->get_item(3);
				event_type_mr->set_hidden(true);
				event_type_mr->set_hidden_replace_string("(\"" + event_type_mr->head_token().token + "\"");
				cmr->get_item(2)->set_hidden(true);
				cmr->get_item(2)->set_hidden_replace_string(",");
				auto emit_keyword_str = emit_mr->head_token().token;
				auto info = find_info_by_varname(mr, emit_keyword_str);
				std::string event_type_str(event_type_mr->head_token().token);
				// TODO: arg的类型要求是string类型，不能是其他类型比如number
				if(!info->is_undefined() && !info->is_nil())
				{
					set_error(error_in_match_result(mr, "Can't define variable emit, emit is a keyword"));
				}
				else if(event_type_str.length()>EMIT_EVENT_NAME_MAX_COUNT-1)
				{
					set_error(error_in_match_result(mr, std::string("emit event name must be at most ") + std::to_string(EMIT_EVENT_NAME_MAX_COUNT - 1) + " but got " + event_type_str));
				}
				else
				{
					auto arg_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
					if(check_expr_error(arg_mr, arg_type_info, nullptr))
					{
						add_emit_event_type(event_type_str);
					}
					if(arg_mr->is_final() && arg_mr->as_final()->token.type == TOKEN_RESERVED::LTK_NAME
						&& (arg_type_info->is_undefined() || arg_type_info->is_nil()))
					{
						set_error(error_in_match_result(mr, std::string("Can't find symbol " + arg_mr->head_token().token)));
					}
					else if(!arg_type_info->is_string())
					{
						set_error(error_in_match_result(mr, std::string("emit statement argument must be string, but got ") + arg_type_info->str()));
					}
				}
	        }
			return true;
        }

		bool GluaTypeChecker::check_lambda_value_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			// args => exp
			auto *cmr = mr->as_complex();
			auto *args_mr = cmr->get_item(0);
			// 转成functiondef的mr，然后当成functiondef处理
			auto *functiondef_mr = _ctx->create_complex_match_result(mr->next_input());
			auto *prefix_function_symbol_mr = _ctx->create_final_match_result(cmr->next_input());
			prefix_function_symbol_mr->token.token = "function";
			prefix_function_symbol_mr->token.linenumber = args_mr->head_token().linenumber;
			prefix_function_symbol_mr->token.position = args_mr->head_token().position;
			functiondef_mr->items.push_back(prefix_function_symbol_mr);
			auto *funcbody_mr = _ctx->create_complex_match_result(mr->next_input());
			funcbody_mr->set_node_name("funcbody");
			functiondef_mr->items.push_back(funcbody_mr);
			funcbody_mr->items.push_back(args_mr);
			auto *block_mr = _ctx->create_complex_match_result(mr->next_input());
			block_mr->set_node_name("block");
			auto *exp_mr = cmr->get_item(2);
			auto *retstat_mr = _ctx->create_complex_match_result(mr->next_input());
			retstat_mr->set_node_name("retstat");
			auto *return_symbol_mr = _ctx->create_final_match_result(mr->next_input());
			return_symbol_mr->token.token = "return";
			return_symbol_mr->token.linenumber = exp_mr->head_token().linenumber;
			return_symbol_mr->token.position = exp_mr->head_token().position;
			retstat_mr->items.push_back(return_symbol_mr);
			auto *explist_mr = _ctx->create_complex_match_result(mr->next_input());
			explist_mr->set_node_name("explist");
			explist_mr->items.push_back(exp_mr);
			retstat_mr->items.push_back(explist_mr);
			block_mr->items.push_back(retstat_mr);
			auto *end_symbol_mr = _ctx->create_final_match_result(exp_mr->next_input());
			end_symbol_mr->token.token = "; end;";
			end_symbol_mr->token.linenumber = exp_mr->last_token().linenumber;
			end_symbol_mr->token.position = exp_mr->last_token().position;
			funcbody_mr->items.push_back(block_mr);
			funcbody_mr->items.push_back(end_symbol_mr);
			functiondef_mr->set_node_name("functiondef");
			mr->set_node_name("block");
			cmr->items.clear();
			cmr->items.push_back(functiondef_mr);
			return check_expr_error(functiondef_mr, result_type, ret_type);
        }

		bool GluaTypeChecker::check_lambda_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type)
        {
			// args => do block end
			auto *cmr = mr->as_complex();
			auto *args_mr = cmr->get_item(0);
			// 转成functiondef的mr，然后当成functiondef处理
			auto *functiondef_mr = _ctx->create_complex_match_result(mr->next_input());
			auto *prefix_function_symbol_mr = _ctx->create_final_match_result(cmr->next_input());
			prefix_function_symbol_mr->token.token = "function";
			prefix_function_symbol_mr->token.linenumber = args_mr->head_token().linenumber;
			prefix_function_symbol_mr->token.position = args_mr->head_token().position;
			functiondef_mr->items.push_back(prefix_function_symbol_mr);
			auto *funcbody_mr = _ctx->create_complex_match_result(mr->next_input());
			funcbody_mr->set_node_name("funcbody");
			functiondef_mr->items.push_back(funcbody_mr);
			funcbody_mr->items.push_back(args_mr);
			auto *block_mr = cmr->get_item(3);
			auto *end_symbol_mr = cmr->get_item(4);
			funcbody_mr->items.push_back(block_mr);
			funcbody_mr->items.push_back(end_symbol_mr);
			functiondef_mr->set_node_name("functiondef");
			mr->set_node_name("block");
			cmr->items.clear();
			cmr->items.push_back(functiondef_mr);
			return check_expr_error(functiondef_mr, result_type, ret_type);
        }

		std::vector<GluaTypeInfoP> GluaTypeChecker::parent_types(GluaTypeInfoP type_info)
        {
			std::vector<GluaTypeInfoP> parents;
			auto obj_type = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
	        if(type_info->is_int())
	        {
				parents.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_NUMBER));
				parents.push_back(obj_type);
				return parents;
	        }
			else if(type_info->is_array() || type_info->is_map() || type_info->is_record())
			{
				parents.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_TABLE));
				parents.push_back(obj_type);
				return parents;
			}
			else
			{
				parents.push_back(obj_type);
				return parents;
			}
        }

		GluaTypeInfoP GluaTypeChecker::min_sharing_declarative_type(GluaTypeInfoP item_type1, GluaTypeInfoP item_type2)
        {
			if (item_type1 == item_type2)
				return item_type1;
			if (match_declare_type(item_type1, item_type2, true))
				return item_type1;
			if (match_declare_type(item_type2, item_type1, true))
				return item_type2;
			auto parents1 = parent_types(item_type1);
			for(const auto &p1 : parents1)
			{
				if (match_declare_type(p1, item_type2, true))
					return p1;
			}
			return create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
        }

		void GluaTypeChecker::set_ldf_mapping(MatchResult *mr)
        {
			if (ldf)
			{
				//if (mr->is_final())
				//{
				// TODO: 这里有BUG, 映射关系不对
				auto token = mr->head_token();
				if (token.linenumber < 100000 && token.linenumber>=7)
				{
					ldf->set_source_line_mapping(token.linenumber - 7, token.linenumber - 1 + _middle_inserted_code_lines);
				}
				//}
			}
        }

        bool GluaTypeChecker::check_expr_error(MatchResult *mr,
            GluaTypeInfoP result_typ, GluaTypeInfoP ret_type, MatchResult *parent_mr)
        {
			set_ldf_mapping(mr);
            if (mr->node_name() == "bin_exp")
            {
				return check_bin_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "un_exp")
            {
				return check_un_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "local_def_stat")
            {
				return check_local_def_stat_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "functiondef")
            {
				return check_functiondef_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "local_named_function_def_stat")
            {
				return check_local_named_function_def_stat_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "named_function_def_stat")
            {
				return check_named_function_def_stat_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "offline_named_function_def_stat")
            {
				return check_offline_named_function_def_stat_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "tableconstructor")
            {
				return check_tableconstructor_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "functioncall")
            {
				return check_functioncall_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "prefixexp")
            {
                if (result_typ)
                {
                    copy_lua_type_info(result_typ, guess_exp_type(mr));
                }
                return true;
            }
            else if (mr->node_name() == "simpleexp" && mr->is_final())
            {
                auto token = mr->as_final()->token;
                if (result_typ)
                {
                    copy_lua_type_info(result_typ, guess_exp_type(mr));
                }
                return true;
            }
            else if (mr->node_name() == "suffixedexp_visit_prop" || mr->node_name() == "var")
            {
				// TODO: 这里和suffixedexp_visit_prop的模式一样，考虑把repeat模式处理函数封装
				return check_suffixedexp_visit_prop_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "retstat")
            {
				return check_retstat_expr_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "varlist_assign_stat")
            {
				return check_varlist_assign_stat_error(mr, result_typ, ret_type);
            }
            else if (mr->node_name() == "record")
            {
				return check_record_stat_error(mr, result_typ, ret_type, parent_mr);
            }
            else if (mr->node_name() == "record_with_generic")
            {
				return check_record_with_generic_state_error(mr, result_typ, ret_type, parent_mr);
            }
            else if (mr->node_name() == "typedef")
            {
				return check_typedef_stat_error(mr, result_typ, ret_type);
            }
			else if(mr->node_name()=="emit_stat")
			{
				return check_emit_stat_error(mr, result_typ, ret_type);
			}
			else if(mr->node_name() == "lambda_value_expr")
			{
				return check_lambda_value_expr_error(mr, result_typ, ret_type);
			}
			else if(mr->node_name() == "lambda_expr")
			{
				return check_lambda_expr_error(mr, result_typ, ret_type);
			}
			else if(mr->node_name() == "for_range_stat")
			{
				return check_for_range_stat_error(mr, result_typ, ret_type);
			}
			else if(mr->node_name() == "for_step_stat")
			{
				return check_for_step_stat_error(mr, result_typ, ret_type);
			}
			else if(mr->node_name() == "do_stat")
			{
				return check_do_stat_error(mr, result_typ, ret_type);
			}
			else if (mr->node_name() == "while_stat")
			{
				return check_while_stat_error(mr, result_typ, ret_type);
			}
			else if (mr->node_name() == "repeat_stat")
			{
				return check_repeat_stat_error(mr, result_typ, ret_type);
			}
            else if (mr->node_name() == "break_stat")
            {
              return check_break_stat_error(mr, result_typ, ret_type);
            }
			else if(mr->is_ast_node_type())
			{
				auto *ast_node = (glua::parser::GluaAstNode*) mr;
				switch(ast_node->ast_node_type())
				{
				case GluaAstNodeType::LA_IF_STAT:
					return check_if_stat_error((glua::parser::GluaIfStatNode*) ast_node, result_typ, ret_type);
					break;
				default:
					set_error(error_in_match_result(mr, std::string("unknown ast node type ") + std::to_string(ast_node->ast_node_type()) + " in check_exp_type "));
				}
			}
            else if (mr->is_complex())
            {
                bool result = true;
                for (auto i = mr->as_complex()->items.begin(); i != mr->as_complex()->items.end(); ++i)
                {
                    auto *item = *i;
                    if (!check_expr_error(item, result_typ, ret_type, mr))
                    {
                        result = false;
                    }
                }
                return result;
            }
			// TODO: if, for, while, repeat until, etc. control flows also need type check
            return true;
        }


		/**
		 * 这只是将简易的类型签名的字符串转换成类型信息，不支持太复杂的类型签名，比如深度嵌套等，和本编程语言本身语法不一样
		 */
        GluaTypeInfoP GluaTypeChecker::of_type_str(std::string typestr, GluaExtraBindingsTypeP extra)
        {
            typestr = glua::util::string_trim(typestr);

            if (typestr == "object" || typestr == "...")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
            }
            else if (typestr == "void" || typestr == "nil")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_NIL);
            }
            else if (typestr == "bool" || typestr == "boolean")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_BOOL);
            }
            else if (typestr == "table")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_TABLE);
            }
            else if (typestr == "int" || typestr == "integer")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_INT);
            }
            else if (typestr == "number" || typestr == "float" || typestr == "double")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_NUMBER);
            }
            else if (typestr == "string")
            {
                return create_lua_type_info(GluaTypeInfoEnum::LTI_STRING);
            }
            else if (typestr == "array" || typestr == "Array")
            {
                auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_ARRAY);
                type_info->array_item_type = create_lua_type_info();
                return type_info;
            }
			else if(typestr == "map" || typestr == "Map")
			{
				auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_MAP);
				type_info->map_item_type = nullptr; // create_lua_type_info();
				return type_info;
			}
            else if (typestr == "function" || typestr == "Function")
            {
                auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
                type_info->is_any_function = true;
                type_info->arg_names.push_back("");
                type_info->arg_types.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
                type_info->ret_types.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
                return type_info;
            }
			else if(glua::util::starts_with(typestr, "("))
			{
				// 函数签名类型 (args...) => ret_type, 不支持嵌套括号
				auto args_str = typestr.substr(1, typestr.find_first_of(")")-1);
				auto ret_type_str = typestr.substr(typestr.find_last_of("=>") + 1);
				auto arg_type_strs = glua::util::string_split(args_str, ',');
				auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
				type_info->is_any_function = false;
				auto ret_type_info = of_type_str(ret_type_str, extra);
				type_info->ret_types.push_back(ret_type_info);
				for (auto i = arg_type_strs.begin(); i != arg_type_strs.end(); ++i)
				{
					std::string arg_type_str(*i);
					arg_type_str = glua::util::string_trim(arg_type_str);
					type_info->arg_names.push_back(arg_type_str == "..." ? "..." : "");
					type_info->arg_types.push_back(of_type_str(arg_type_str));
				}
				return type_info;
			}
			// record类型的识别, record{propName: typeName;...} 其中属性的typeName最多支持一层()，不支持嵌套record
			else if(glua::util::starts_with(typestr, "record"))
			{
				auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_RECORD);
				auto props_all_str = typestr.substr(typestr.find_first_of("{") + 1, typestr.find_last_of("}")- typestr.find_first_of("{") - 1);
				auto props_str_list = glua::util::string_split(props_all_str, ';');
				for(const auto &prop_pair_str : props_str_list)
				{
					std::string prop_name(prop_pair_str.substr(0, prop_pair_str.find_first_of(":")));
					if (prop_name.length() < 1)
						continue;
					prop_name = glua::util::string_trim(prop_name);
					auto prop_type_str = prop_pair_str.substr(prop_pair_str.find_first_of(":") + 1);
					prop_type_str = glua::util::string_trim(prop_type_str);
					if (prop_type_str.length() < 1)
						continue;
					auto prop_type = of_type_str(prop_type_str, extra);
					type_info->record_props[prop_name] = prop_type;
				}
				return type_info;
			}
            else
            {
                // TODO: 识别出union类型，比如 `string | integer|bool|function`
                auto left_xkh_idx = typestr.find_first_of('(');
                if (left_xkh_idx > 0 && left_xkh_idx < typestr.length() - 1)
                {
                    // function type
                    auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
                    type_info->is_any_function = false;
                    auto ret_type_str = typestr.substr(0, left_xkh_idx);
                    auto args_type_str = typestr.substr(left_xkh_idx + 1, typestr.length() - left_xkh_idx - 2);
                    auto ret_type_info = of_type_str(ret_type_str, extra);
                    auto arg_type_strs = glua::util::string_split(args_type_str, ',');
                    type_info->ret_types.push_back(ret_type_info);
                    for (auto i = arg_type_strs.begin(); i != arg_type_strs.end(); ++i)
                    {
                        type_info->arg_names.push_back((*i) == "..." ? "..." : "");
                        type_info->arg_types.push_back(of_type_str(*i, extra));
                    }
                    return type_info;
                }
                // unknown type
				if(extra)
				{
					auto type_info_in_extra = extra->find_type(typestr);
					if (!type_info_in_extra || type_info_in_extra->is_nil() || type_info_in_extra->is_undefined())
						return create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
					else
						return type_info_in_extra;
				}
                return create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
            }
        }

        void GluaTypeChecker::init_global_variables_to_proto(LuaProtoSTreeP proto)
        {
            auto *global_var_infos = thinkyoung::lua::lib::get_globalvar_type_infos();
			auto extra = std::make_shared<GluaExtraBindingsType>();
            for (auto i = global_var_infos->begin(); i != global_var_infos->end(); ++i)
            {
                auto varname = i->first;
                auto typestr = i->second;
                auto type_info = of_type_str(typestr, extra);
                proto->localvars[varname] = type_info;
                proto->localvars_changable[varname] = true; // 默认全局变量不能修改
				if(varname.length() > strlen(GLUA_TYPE_NAMESPACE_PREFIX) && varname.substr(0, strlen(GLUA_TYPE_NAMESPACE_PREFIX)) == GLUA_TYPE_NAMESPACE_PREFIX)
				{
					auto type_name = varname.substr(strlen(GLUA_TYPE_NAMESPACE_PREFIX));
					if (type_name == "Stream")
					{
						type_info->is_stream_type = true;
					}
					proto->local_types[type_name] = type_info;
					extra->set_type(type_name, type_info);
				}
				else
				{
					extra->set_variable(varname, type_info);
				}
            }
			/*
			添加Map的类型信息
			type Map<V> = {
				__index: (string) => V
			}
			*/
        }

		bool GluaTypeChecker::check_contract_syntax_tree_type(MatchResult *mr, GluaTypeInfoP *ret_contract_type, GluaTypeInfoP *ret_contract_storage_type_out)
        {
			this->is_checking_contract = true;
			struct _exit_scope
			{
				GluaTypeChecker *_that;
				inline _exit_scope(GluaTypeChecker *that)
				{
					_that = that;
				}
				inline ~_exit_scope()
				{
					_that->is_checking_contract = false;
				}
			} exit_scope(this);
			auto ret_type = create_lua_type_info(GluaTypeInfoEnum::LTI_UNDEFINED);
			if (!check_syntax_tree_type(mr, ret_type))
				return false;
			// 确保返回类型是合约类型
			if(!ret_type->is_record() || ret_type->record_props.find("storage")==ret_type->record_props.end())
			{
				set_error(GluaTypeCheckerErrors::CONTRACT_NOT_RETURN_CONTRACT_TYPE,
					std::string("contract must return contract type, but get ") + ret_type->str());
				return false;
			}
			// 确保storage的类型是record类型
			auto contract_storage_type = ret_type->record_props["storage"];
			if(!contract_storage_type->is_record())
			{
				set_error(GluaTypeCheckerErrors::CONTRACT_NOT_RETURN_CONTRACT_TYPE,
					std::string("contract storage's type must be record type, but get ") + ret_type->str());
				return false;
			}
			
			// TODO: 要把合约的storage类型返回
			if(ret_contract_storage_type_out)
			{
				*ret_contract_storage_type_out = contract_storage_type;
			}

			if(contract_storage_type->is_record())
			{
				for(const auto &item : contract_storage_type->record_props)
				{
					auto storage_prop_type = item.second;
					// contract的storage的类型的属性的类型，只能是int|number|string|bool|table|record|Array|Map
					if(storage_prop_type->etype != GluaTypeInfoEnum::LTI_INT
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_NUMBER
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_STRING
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_BOOL
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_TABLE
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_RECORD
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_ARRAY
						&& storage_prop_type->etype != GluaTypeInfoEnum::LTI_MAP)
					{
						set_error(GluaTypeCheckerErrors::CONTRACT_STORAGE_TYPE_ERROR,
							std::string("contract storage type ") + contract_storage_type->str() + " error, property "
								+ item.first + "'s type error, all storage record type's properties' types must be one of int/number/bool/string/table/record/Array/Map");
						return false;
					}
				}
			}

			for(const auto &api_type : ret_type->record_props)
			{
				const auto &api_prop_name = api_type.first;
				if(!api_type.second->is_function())
				{
					continue;
				}
				else if(glua::util::vector_contains(thinkyoung::lua::lib::contract_special_api_names, api_prop_name)
					&& !glua::util::vector_contains(thinkyoung::lua::lib::contract_int_argument_special_api_names, api_prop_name))
				{
					if (api_type.second->arg_types.size()>1)
					{
						set_error(GluaTypeCheckerErrors::CONTRACT_API_WRONG_ARGS,
							std::string("contract api ") + api_type.first + " expect no arguments except self");
					}
				}
				else if(glua::util::vector_contains(thinkyoung::lua::lib::contract_int_argument_special_api_names, api_prop_name))
				{
					if (api_type.second->arg_types.size()==2 && !api_type.second->arg_types[1]->is_int())
					{
						set_error(GluaTypeCheckerErrors::CONTRACT_API_WRONG_ARGS,
							std::string("contract api ") + api_type.first + "'s arg(except self)'s type must be int");
					}
					else if(api_type.second->arg_types.size()>2)
					{
						set_error(GluaTypeCheckerErrors::CONTRACT_API_WRONG_ARGS,
							std::string("contract api ") + api_type.first + " can only have one argument(except self)");
					}
				}
				else
				{
					// TODO: 改成调用合约API时自动参数类型转换
					if(api_type.second->arg_types.size() == 2 && !api_type.second->arg_types[1]->is_string())
					{
						set_error(GluaTypeCheckerErrors::CONTRACT_API_WRONG_ARGS,
							std::string("contract api ") + api_type.first + "'s first arg(except self)'s type must be string");
					} else if(api_type.second->arg_types.size()>2)
					{
						set_error(GluaTypeCheckerErrors::CONTRACT_API_WRONG_ARGS,
							std::string("contract api ") + api_type.first + " can only have one argument(except self)");
					}
				}
			}

			if(_created_global_variables.size()>0)
			{
				for(const auto &p : _created_global_variables)
				{
					set_error(GluaTypeCheckerErrors::CONTRACT_NOT_ALLOW_DEFINE_NEW_VARIABLE,
						std::string("line ") + std::to_string(p.second) + " token " + p.first + " , contract not allow define new variable");
				}
			}
			if (ret_contract_type)
				copy_lua_type_info(*ret_contract_type, ret_type);
			return true;
        }

        bool GluaTypeChecker::check_syntax_tree_type(MatchResult *mr, GluaTypeInfoP ret_type, bool is_contract)
        {
			if(is_contract)
			{
				this->is_checking_contract = true;
				struct _exit_scope
				{
					GluaTypeChecker *_that;
					inline _exit_scope(GluaTypeChecker *that)
					{
						_that = that;
					}
					inline ~_exit_scope()
					{
						_that->is_checking_contract = false;
					}
				} exit_scope(this);
			}
			ldf = std::make_shared<thinkyoung::lua::core::LuaDebugFileInfo>();
            this->_proto = std::make_shared<LuaProtoSTree>();
            _current_checking_proto_stack.clear();
            auto global_proto = std::make_shared<LuaProtoSTree>();
            // 把全局变量和函数的类型信息放入顶层proto
            init_global_variables_to_proto(global_proto);
            global_proto->sub_protos.push_back(_proto);
            _current_checking_proto_stack.push_back(global_proto);
            _current_checking_proto_stack.push_back(_proto);
            if (!build_proto(this->_proto, mr))
                return false;
			ldf->set_source_line_mapping(0, 0 + _middle_inserted_code_lines);
            // 递归检查，每次碰到一个stat，如果是增加修改var，放入当前proto中，如果是使用，检查类型，如果失败就报错
            // 当stat是包含proto时，递归进入这个proto，把proto压栈，并继续检查
			try {
				if (!check_expr_error(this->_proto->mr, nullptr, ret_type))
					return false;
				return true;
			} catch(...)
			{
				auto a = std::current_exception();
				try
				{
					std::rethrow_exception(a);
				}catch(const std::exception& e)
				{
					std::cerr << boost::diagnostic_information(e);
					std::rethrow_exception(a);
				}
				return false;
			}
        }

        GluaTypeChecker *GluaTypeChecker::set_error(int error_no, std::string error)
        {
            if (_error.length() < 1)
                _error = error;
			for(const auto &item : _errors)
			{
				if (item.first == error_no && item.second == error)
					return this;
			}
            _errors.push_back(std::make_pair(error_no, error));
            return this;
        }

        GluaTypeChecker *GluaTypeChecker::set_error(std::pair<int, std::string> error)
        {
            if (_error.length() < 1)
                _error = error.second;
			for (const auto &item : _errors)
			{
				if (item.first == error.first && item.second == error.second)
					return this;
			}
            _errors.push_back(std::make_pair(error.first, error.second));
            return this;
        }

		GluaTypeInfoP GluaTypeChecker::create_lua_type_info(GluaTypeInfoEnum type)
        {
			auto *p = new GluaTypeInfo(type);
			_created_lua_type_info_pointers.push_back(p);
			return p;
			// return std::make_shared<GluaTypeInfo>(type);
        }

        LuaProtoSTreeP GluaTypeChecker::current_checking_proto() const
        {
            if (_current_checking_proto_stack.size() > 0)
                return _current_checking_proto_stack.at(_current_checking_proto_stack.size() - 1);
            else
                return nullptr;
        }

        LuaProtoSTreeP GluaTypeChecker::top_checking_proto() const
        {
            if (_current_checking_proto_stack.size() > 0)
                return _current_checking_proto_stack.at(0);
            else
                return nullptr;
        }

        LuaProtoSTreeP GluaTypeChecker::first_checking_proto_has_var(std::string varname) const
        {
            for (auto i = _current_checking_proto_stack.rbegin(); i != _current_checking_proto_stack.rend(); ++i)
            {
                auto tree = *i;
                for (auto j = tree->localvars.begin(); j != tree->localvars.end(); ++j)
                {
                    if (j->first == varname)
                        return tree;
                }
            }
            return nullptr;
        }

		void GluaTypeChecker::set_error_of_not_support_record_call(MatchResult *mr)
        {
	        if(!_open_record_call_syntax)
	        {
				set_error(error_in_match_result(mr, std::string("now syntax of treat record as function is not supported")));
	        }
        }

        // @return ordered_map
        std::vector<GluaNameTypePair> GluaTypeChecker::get_namelist_info_from_mr(MatchResult *mr, MatchResult *parlist_mr, bool default_type_is_object)
        {
            std::vector<GluaNameTypePair> result;
            MatchResult *cur_mr = mr;
            while (cur_mr && (cur_mr->node_name() == "namelist" || cur_mr->node_name() == "namelist_in_record"))
            {
                auto *ccur_mr = cur_mr->as_complex();
                if (ccur_mr->get_item(0)->node_name() == "name_with_type_declare" || ccur_mr->get_item(0)->node_name() == "name_in_record_with_type_declare")
                {
                    auto *tname = ccur_mr->get_item(0)->as_complex();
					auto name_token = tname->get_item(0)->head_token();
                    auto name = name_token.token;
					if (name_token.type != TOKEN_RESERVED::LTK_NAME)
					{
						set_error(error_in_match_result(cur_mr, std::string("can't use keyword '") + name + "' as variable name", GluaTypeCheckerErrors::USE_WRONG_SYMBOL_AS_VARIABLE_NAME));
					}
					auto *type_declare_prefix_mr = tname->get_item(1);
					bool is_optional_colon = type_declare_prefix_mr->head_token().type == TOKEN_RESERVED::LTK_OPTIONAL_COLON;
                    tname->get_item(1)->set_hidden(true);
                    tname->get_item(2)->set_hidden(true);
					auto type_token = tname->get_item(2)->head_token();
                    auto type_symbol = type_token.token;
					GluaTypeInfoP type_info = get_type_from_mr(tname->get_item(2));
                    if (type_info->is_nil() || type_info->is_undefined())
                    {
                        set_error(error_in_match_result(cur_mr, std::string("type ") + type_symbol + " not found"));
                    }
					if(is_optional_colon)
					{
						if(!type_info->is_union() && !type_info->is_nil() && !type_info->is_undefined())
						{
							auto new_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_UNION);
							new_type_info->union_types.insert(type_info);
							new_type_info->union_types.insert(create_lua_type_info(GluaTypeInfoEnum::LTI_NIL));
							type_info = new_type_info;
						}
					}
                    result.push_back(GluaNameTypePair(name, type_info, type_symbol, true));
                }
                else if (ccur_mr->get_item(0)->is_final())
                {
					auto name_token = ccur_mr->get_item(0)->as_final()->token;
					auto name = name_token.token;
					if(name_token.type!=TOKEN_RESERVED::LTK_NAME)
					{
						set_error(error_in_match_result(cur_mr, std::string("can't use keyword '") + name + "' as variable name", GluaTypeCheckerErrors::USE_WRONG_SYMBOL_AS_VARIABLE_NAME));
					}
                    auto type_info = create_lua_type_info(default_type_is_object ? GluaTypeInfoEnum::LTI_OBJECT : GluaTypeInfoEnum::LTI_UNDEFINED);
					std::string type_name("object");
					result.push_back(GluaNameTypePair(name, type_info, type_name, false));
                }
                else {
                    set_error(error_in_match_result(cur_mr, std::string("unknown namelist")));
                }
                cur_mr = ccur_mr->size() >= 3 ? ccur_mr->get_item(2) : nullptr;
            }
            if (parlist_mr)
            {
                MatchResult *dots_mr = nullptr;
                if (mr && parlist_mr->is_complex() && parlist_mr->as_complex()->size() >= 3)
                {
                    dots_mr = parlist_mr->as_complex()->get_item(2);
                }
                else if (nullptr == mr && parlist_mr->is_final())
                {
                    dots_mr = parlist_mr->as_final();
                }
                    
                if (dots_mr && dots_mr->head_token().token == "...")
                {
					result.push_back(GluaNameTypePair(std::string("..."), create_array(), std::string("Array<object>")));
                }
                    
            }
            return result;
        }

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_lambda(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *args_mr, MatchResult *body_mr, bool include_children_protos)
        {
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
				tree->type_info->is_any_function = false;
			}
			std::string funcname = "";
			std::string assign_to_table_name = "";
			bool need_add_self = false;
			auto *funcbody_args_def_mr = args_mr->as_complex();
			if (funcbody_args_def_mr->items.size() > 2)
			{
				auto *parlist_mr = funcbody_args_def_mr->get_item(1);
				// find args from name list
				auto *namelist_mr = (parlist_mr->is_complex() && parlist_mr->as_complex()->size() > 0) ? parlist_mr->as_complex()->get_item(0) : nullptr;
				auto namelist = get_namelist_info_from_mr(namelist_mr, parlist_mr, true);
				for (const auto &p : namelist)
				{
					tree->add_arg(p.name, p.type_info);
				}
			}
			tree->mr = body_mr;
			tree->full_mr = full_mr;
			if (body_mr && include_children_protos)
			{
				if (body_mr->is_complex())
				{
					auto *funcbody_block_mr = body_mr->as_complex();
					for (auto i = funcbody_block_mr->items.begin(); i != funcbody_block_mr->items.end(); ++i)
					{
						auto *item = *i;
						auto sub_tree = find_proto(tree, item);
						if (sub_tree)
						{
							item->set_binding_type(MatchResultBindingTypeEnum::FUNCTION);
							item->set_binding(sub_tree.get());
						}
					}
				}
			}
			// TODO: add assign stat to parent block to replace origin code 
			// TODO: find return types
			// now use object as type, except inner defined functions
			tree->type_info->ret_types.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
			return tree;
        }

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_for_range_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *namelist_mr, MatchResult *explist_mr, MatchResult *body_mr, bool include_children_protos)
        {
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
				tree->type_info->is_any_function = false;
			}
			tree->full_mr = full_mr;
			tree->mr = body_mr;
			auto names = get_namelist_info_from_mr(namelist_mr, nullptr, true);
			auto exps_has_errors = false;
			auto exps = get_explist_from_mr(explist_mr, &exps_has_errors);
			if (exps_has_errors)
			{
				set_error(error_in_match_result(explist_mr, std::string("has error in for range statement's explist")));
				return tree;
			}
			if(exps.size()<1 || exps.size()>2)
			{
				set_error(error_in_match_result(explist_mr, std::string("for range statement's explist after `in` keyword must be 1 or 2 expressions")));
				return tree;
			}
			auto first_exp = exps[0];
			if(first_exp->etype != GluaTypeInfoEnum::LTI_OBJECT
				&& !first_exp->is_function())
			{
				set_error(error_in_match_result(explist_mr, std::string("for range statement's first expression after `in` keyword must be function or object type")));
				return tree;
			}
			tree->for_namelist = names;
			return tree;
        }

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_for_step_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *name_mr, MatchResult *explist_mr, MatchResult *body_mr, bool include_children_protos)
		{
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
				tree->type_info->is_any_function = false;
			}
			tree->full_mr = full_mr;
			tree->mr = body_mr;
			auto name = name_mr->as_final()->token.token;
			auto exps_has_errors = false;
			auto exps = get_explist_from_mr(explist_mr, &exps_has_errors);
			if (exps_has_errors)
			{
				set_error(error_in_match_result(explist_mr, std::string("has error in for range statement's explist")));
				return tree;
			}
			if (exps.size()<2 || exps.size()>3)
			{
				set_error(error_in_match_result(explist_mr, std::string("for step statement's explist after `=` keyword must be 2 or 3 expressions")));
				return tree;
			}
			auto num_type = create_lua_type_info(GluaTypeInfoEnum::LTI_NUMBER);
			if(!match_declare_type(num_type, exps[0]) || !match_declare_type(num_type, exps[1])
				|| (exps.size()>2 && !match_declare_type(num_type, exps[2])))
			{
				set_error(error_in_match_result(explist_mr, std::string("for step statement's expressions after `=` keyword must be int/number type")));
				return tree;
			}
			std::vector<GluaNameTypePair> names;
			std::string int_type_str("int");
			names.push_back(GluaNameTypePair(name, num_type, int_type_str));
			tree->for_namelist = names;
			return tree;
		}

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_do_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *body_mr, bool include_children_protos)
		{
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
				tree->type_info->is_any_function = false;
			}
			tree->full_mr = full_mr;
			tree->mr = body_mr;
			return tree;
		}

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_while_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *condition_mr, MatchResult *body_mr, bool include_children_protos)
		{
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
				tree->type_info->is_any_function = false;
			}
			tree->condition_mr = condition_mr;
			tree->full_mr = full_mr;
			tree->mr = body_mr;
			return tree;
		}

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_repeat_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *condition_mr, MatchResult *body_mr, bool include_children_protos)
		{
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
				tree->type_info->is_any_function = false;
			}
			tree->condition_mr = condition_mr;
			tree->full_mr = full_mr;
			tree->mr = body_mr;
			return tree;
		}

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree_from_if_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
			GluaIfStatNode *ast_node, bool include_children_protos)
		{
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
				tree->type_info->is_any_function = false;
			}
			tree->full_mr = full_mr;
			tree->mr = ast_node;
			return tree;
		}

		LuaProtoSTreeP GluaTypeChecker::make_proto_tree(LuaProtoSTreeP tree, MatchResult *full_mr,
			MatchResult *funcname_mr, MatchResult *funcbody_mr, bool include_children_protos)
        {
			if (!tree->type_info)
			{
				tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
				tree->type_info->is_any_function = false;
			}
			std::string funcname = "";
			std::string assign_to_table_name = "";
			bool need_add_self = false;
			bool anonymous = !funcname_mr;
			if (!anonymous)
			{
				bool wrong_funcname_type = false; // 是否用错误token(比如关键字，其他符合等)作为函数名
				if (funcname_mr->is_complex() && funcname_mr->as_complex()->size() > 2)
				{
					// table:funcname or table.funcname
					need_add_self = funcname_mr->as_complex()->get_item(1)->as_final()->token.type == ':';
					assign_to_table_name = funcname_mr->as_complex()->get_item(0)->as_final()->token.token;
					auto funcname_token = funcname_mr->as_complex()->get_item(2)->as_final()->token;
					funcname = funcname_token.token;
					if(funcname_token.type != TOKEN_RESERVED::LTK_NAME)
					{
						wrong_funcname_type = true;
					}
				}
				else if(funcname_mr->is_complex() && funcname_mr->as_complex()->size()>=2 && funcname_mr->as_complex()->get_item(1)->is_complex()
					&& funcname_mr->as_complex()->get_item(1)->as_complex()->size()>=2 && funcname_mr->as_complex()->get_item(1)->head_token().token==".")
				{
					auto second_name_token = funcname_mr->as_complex()->get_item(1)->as_complex()->get_item(1);
					funcname = second_name_token->as_final()->token.token;
					if(second_name_token->is_final() && second_name_token->as_final()->token.type != TOKEN_RESERVED::LTK_NAME)
					{
						wrong_funcname_type = true;
					}
				}
				else{
					auto funcname_token = funcname_mr->head_token();
					funcname = funcname_token.token;
					if (funcname_token.type != TOKEN_RESERVED::LTK_NAME)
					{
						wrong_funcname_type = true;
					}
				}
				if(wrong_funcname_type)
				{
					set_error(error_in_match_result(funcname_mr, std::string("can't use keyword '") + funcname + "' as function/property name", GluaTypeCheckerErrors::USE_WRONG_SYMBOL_AS_VARIABLE_NAME));
				}
			}
			auto *funcbody_args_def_mr = funcbody_mr->as_complex()->get_item(0)->as_complex();
			ComplexMatchResult *funcbody_block_mr = nullptr;
			if (funcbody_mr->as_complex()->size() > 2)
				funcbody_block_mr = funcbody_mr->as_complex()->get_item(1)->as_complex();
			if (!anonymous)
			{
				if (need_add_self)
				{
					tree->args.push_back("self");
					GluaTypeInfoP table_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_TABLE);
					if (assign_to_table_name.length() > 0)
					{
						table_type_info = find_info_by_varname(funcname_mr, assign_to_table_name);
						if (table_type_info->etype == GluaTypeInfoEnum::LTI_RECORD)
						{
							// 直接修改table对应的record类型，因为record类型目前只对当前代码文件其效果
							table_type_info->record_props[funcname] = tree->type_info;
						}
						else if (table_type_info->etype == GluaTypeInfoEnum::LTI_TABLE)
						{
							// 如果是table类型，可以考虑自动创建一个新table类型然后修改这个table类型的record_props，方便之后类型分析
							auto new_table_type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_TABLE);
							copy_lua_type_info(new_table_type_info, table_type_info);
							new_table_type_info->record_props[funcname] = tree->type_info;
							define_localvar_in_current_check_proto(funcname_mr, assign_to_table_name, new_table_type_info, false, true, true);
						}
					}
					tree->type_info->arg_names.push_back("self");
					tree->type_info->arg_types.push_back(table_type_info);
				}
			}
			if (funcbody_args_def_mr->items.size() > 2)
			{
				auto *parlist_mr = funcbody_args_def_mr->get_item(1);
				// find args from name list
				auto *namelist_mr = (parlist_mr->is_complex() && parlist_mr->as_complex()->size() > 0) ? parlist_mr->as_complex()->get_item(0) : nullptr;
				auto namelist = get_namelist_info_from_mr(namelist_mr, parlist_mr);
				for (const auto &p : namelist)
				{
					tree->add_arg(p.name, p.type_info);
				}
			}
			tree->mr = funcbody_block_mr;
			tree->full_mr = full_mr;
			if (funcbody_block_mr && include_children_protos)
			{
				for (auto i = funcbody_block_mr->items.begin(); i != funcbody_block_mr->items.end(); ++i)
				{
					auto *item = *i;
					auto sub_tree = find_proto(tree, item);
					if (sub_tree)
					{
						item->set_binding_type(MatchResultBindingTypeEnum::FUNCTION);
						item->set_binding(sub_tree.get());
					}
				}
			}
			// TODO: add assign stat to parent block to replace origin code 
			// TODO: find return types
			// now use object as type, except inner defined functions
			tree->type_info->ret_types.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));
			return tree;
        }

        LuaProtoSTreeP GluaTypeChecker::find_proto(LuaProtoSTreeP parent_tree, MatchResult *mr, bool include_children_protos)
        {
            if (mr->is_complex())
            {
                auto *cmr = mr->as_complex();
                if (cmr->node_name() == "named_function_def_stat")
                {
                    auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
                    if (!tree->type_info)
                    {
                        tree->type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
                        tree->type_info->is_any_function = false;
                    }
					auto *funcname_mr = cmr->get_item(1);
					auto *funcbody_mr = cmr->get_item(2)->as_complex();
					make_proto_tree(tree, cmr, funcname_mr, funcbody_mr, include_children_protos);
                    return tree;
                }
                else if (cmr->node_name() == "local_named_function_def_stat")
                {
                    auto *local_mr = cmr->get_item(0);
                    bool is_let = false;
                    if (local_mr->is_final() && local_mr->node_name() == "var_declare_prefix")
                    {
                        is_let = local_mr->as_final()->token.token == "let";
                        local_mr->as_final()->token.token = "local";
                    }
                    auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *funcname_mr = cmr->get_item(2);
					auto *funcbody_mr = cmr->get_item(3)->as_complex();
					return make_proto_tree(tree, cmr, funcname_mr, funcbody_mr, include_children_protos);;
                }
                else if (cmr->node_name() == "offline_named_function_def_stat")
                {
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *funcname_mr = cmr->get_item(2);
					auto *funcbody_mr = cmr->get_item(3)->as_complex();
					tree = make_proto_tree(tree, cmr, funcname_mr, funcbody_mr, include_children_protos);
					tree->type_info->is_offline = true;
					return tree;
                }
				else if(cmr->node_name() == "lambda_value_expr")
				{
					// args => exp
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *args_mr = cmr->get_item(0);
					auto *exp_mr = cmr->get_item(2);
					return make_proto_tree_from_lambda(tree, cmr, args_mr, exp_mr, include_children_protos);
				}
				else if(cmr->node_name() == "lambda_expr")
				{
					// args => do block end
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *args_mr = cmr->get_item(0);
					auto *block_mr = cmr->get_item(3);
					return make_proto_tree_from_lambda(tree, cmr, args_mr, block_mr, include_children_protos);
				}
                else if (cmr->node_name() == "functiondef")
                {
                    auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *funcbody_mr = cmr->get_item(1)->as_complex();
					return make_proto_tree(tree, cmr, nullptr, funcbody_mr, include_children_protos);
                }
				else if(cmr->node_name() == "for_range_stat")
				{
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *namelist_mr = cmr->get_item(1)->as_complex();
					auto *explist_mr = cmr->get_item(3)->as_complex();
					auto *block_mr = cmr->get_item(5);
					return make_proto_tree_from_for_range_stat(tree, cmr, namelist_mr, explist_mr, block_mr, include_children_protos);
				}
				else if (cmr->node_name() == "for_step_stat")
				{
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *name_mr = cmr->get_item(1);
					auto *explist_mr = cmr->get_item(3)->as_complex();
					auto *block_mr = cmr->get_item(5);
					return make_proto_tree_from_for_step_stat(tree, cmr, name_mr, explist_mr, block_mr, include_children_protos);
				}
				else if(cmr->node_name() == "do_stat")
				{
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *block_mr = cmr->get_item(1);
					return make_proto_tree_from_do_stat(tree, cmr, block_mr, include_children_protos);
				}
				else if(cmr->node_name() == "while_stat")
				{
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *condition_mr = cmr->get_item(1);
					auto *block_mr = cmr->get_item(3);
					return make_proto_tree_from_while_stat(tree, cmr, condition_mr, block_mr, include_children_protos);
				}
				else if (cmr->node_name() == "repeat_stat")
				{
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					auto *condition_mr = cmr->get_item(3);
					auto *block_mr = cmr->get_item(1);
					return make_proto_tree_from_repeat_stat(tree, cmr, condition_mr, block_mr, include_children_protos);
				}
				else if (mr->is_ast_node_type() && ((GluaAstNode*)mr)->ast_node_type() == GluaAstNodeType::LA_IF_STAT)
				{
					auto *ast_node = (GluaIfStatNode*) mr;
					auto tree = std::make_shared<LuaProtoSTree>();
					parent_tree->sub_protos.push_back(tree);
					return make_proto_tree_from_if_stat(tree, mr, ast_node, include_children_protos);
				}
                else
                {
                    // still in the function top scope, recur find sub protos
                    if (include_children_protos)
                    {
                        for (auto i = cmr->items.begin(); i != cmr->items.end(); ++i)
                        {
                            auto *item = *i;
                            auto sub_tree = find_proto(parent_tree, item);
                            if (sub_tree)
                            {
                                item->set_binding_type(MatchResultBindingTypeEnum::FUNCTION);
                                item->set_binding(sub_tree.get());
                            }
                        }
                    }
                    std::shared_ptr<LuaProtoSTree> tree;
                    return tree;
                }
            }
            else
            {
                std::shared_ptr<LuaProtoSTree> tree;
                return tree;
            }
        }

        bool GluaTypeChecker::build_proto(LuaProtoSTreeP tree, MatchResult *mr)
        {
            if (mr->is_final())
                return false;
            tree->mr = mr;
            tree->full_mr = mr;
            assert(!mr->is_final());
            return true;
        }

        std::vector<GluaTypeInfoP> GluaTypeChecker::get_explist_from_mr(MatchResult *mr, bool *has_error)
        {
            std::vector<GluaTypeInfoP> explist;
            if (mr && mr->node_name() == "explist")
            {
                auto *cmr = mr->as_complex();
                auto *cur_mr = cmr;
                do {
                    auto *item_mr = cur_mr->get_item(0);
                    if (item_mr->node_name() == "functiondef")
                    {
                        auto type_info = create_lua_type_info();
                        if(!check_expr_error(item_mr, type_info, nullptr))	// BUG here
                        {
							if (has_error)
								*has_error = true;
                        }
                        explist.push_back(type_info);
                    }
                    else
                        explist.push_back(guess_exp_type(item_mr));
                    if (cur_mr->size() >= 3)
                    {
                        cur_mr = cur_mr->get_item(2)->as_complex();
                    }
                    else
                        break;
                } while (true);
            }
            return explist;
        }

		std::vector<MatchResult *> GluaTypeChecker::get_varlist_from_mr(MatchResult *mr)
        {
			std::vector<MatchResult*> result;
			if(mr->node_name() == "varlist")
			{
				auto *cmr = mr->as_complex();
				result.push_back(cmr->get_item(0));
				if(cmr->size()>0)
				{
					for(size_t i=1;i<cmr->size();++i)
					{
						auto *item_cmr = cmr->get_item(i)->as_complex();
						result.push_back(item_cmr->get_item(1));
					}
				}
			}
			return result;
        }

        std::vector<std::string> GluaTypeChecker::get_generic_list_from_mr(MatchResult *mr)
        {
            std::vector<std::string> names;
            if (mr && mr->node_name() == "generic_list")
            {
                auto *cmr = mr->as_complex();
                size_t i = 0;
                auto *item = cmr->get_item(0);
                if (item->is_final())
                {
                    names.push_back(item->as_final()->token.token);
                }
                do
                {
                    ++i;
                    if (i >= cmr->size())
                        break;
                    item = cmr->get_item(i);
                    if (item->is_complex())
                    {
                        names.push_back(item->as_complex()->get_item(1)->as_final()->token.token);
                    }
                } while (i < cmr->size());
            }
            return names;
        }

        std::vector<GluaTypeInfoP> GluaTypeChecker::get_generic_instances_from_mr(MatchResult *mr, GluaExtraBindingsTypeP extra_bindings)
        {
            std::vector<GluaTypeInfoP> instances;
            if (mr && mr->node_name() == "generic_instance_list")
            {
                auto *cmr = mr->as_complex();
                size_t i = 0;
                auto *item = cmr->get_item(0);
                instances.push_back(get_type_from_mr(item, extra_bindings));
                do
                {
                    ++i;
                    if (i >= cmr->size())
                        break;
                    item = cmr->get_item(i);
                    if (item->is_complex())
                        instances.push_back(get_type_from_mr(item->as_complex()->get_item(1), extra_bindings));
                } while (i < cmr->size());
            }
            return instances;
        }

		bool GluaTypeChecker::check_is_correct_type_to_use(MatchResult *mr, GluaTypeInfoP type_info, std::string type_name)
        {
			if (type_info->is_nil() || type_info->is_undefined())
			{
				set_error(error_in_match_result(mr, std::string("Can't find type ") + type_name));
				return false;
			}
			if ((type_info->is_map() && !type_info->map_item_type)
				|| (type_info->is_array() && !type_info->array_item_type)
				|| (type_info->is_record() && type_info->record_generics.size() > 0))
			{
				set_error(error_in_match_result(mr, std::string("must fill type generic type ") + type_name + "'s type parameters"));
				return false;
			}
			return true;
        }

        GluaTypeInfoP GluaTypeChecker::get_type_from_mr(MatchResult *mr, GluaExtraBindingsTypeP extra_bindings)
        {
            auto type_info = create_lua_type_info();
            if (nullptr == mr)
            {
                set_error(GluaTypeCheckerErrors::TYPE_NOT_FOUND, "error of get type from empty mr");
                return type_info;
            }
			if (mr->node_name() == "literal_value_type")
			{
				auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_LITERIAL_TYPE);
				type_info->literal_type_options.push_back(mr->head_token());
				return type_info;
			}
            else if (mr->is_final())
            {
                return find_type_by_name(mr, mr->as_final()->token.token);
            }
            else if (mr->node_name() == "type")
            {
                auto *cmr = mr->as_complex();
                auto *item1 = cmr->get_item(0);
				cmr->get_item(1)->set_hidden(true);
				cmr->get_item(2)->set_hidden(true);
                auto *item3 = cmr->get_item(2);
                auto result = create_lua_type_info(GluaTypeInfoEnum::LTI_UNION);
                auto type1 = get_type_from_mr(item1);
                auto type2 = get_type_from_mr(item3);
                merge_union_types(result, type1);
                merge_union_types(result, type2);
                if (cmr->size() > 3)
                {
					// union type
                    for (size_t i = 3; i < cmr->size(); ++i)
                    {
                        auto *item = cmr->get_item(i);
						item->set_hidden(true);
                        assert(item->is_complex());
                        auto *citem = item->as_complex();
                        auto *simpletype_mr_in_item = citem->get_item(1);
                        auto sub_type = get_type_from_mr(simpletype_mr_in_item);
                        merge_union_types(result, sub_type);
                    }
                }
                return result;
            }
			// TODO: literal type | literal type | ... 还没处理
			else if(mr->node_name() == "literal_type")
			{
				// 不产生实际的运行时类型和构造函数，不能作为构造函数使用
				auto *cmr = mr->as_complex();
				assert(cmr->size()>=3);
				auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_LITERIAL_TYPE);
				auto *item1_mr = cmr->get_item(0)->as_final();
				auto *item3_mr = cmr->get_item(2)->as_final();
				type_info->literal_type_options.push_back(item1_mr->token);
				type_info->literal_type_options.push_back(item3_mr->token);
				if(cmr->size() > 3)
				{
					for(size_t i=3;i < cmr->size();++i)
					{
						auto *item_mr = cmr->get_item(i)->as_complex()->get_item(1)->as_final();
						type_info->literal_type_options.push_back(item_mr->token);
					}
				}
				return type_info;
			}
            else if (mr->node_name() == "simple_type")
            {
                auto *cmr = mr->as_complex();
                assert(cmr->size()>0);
				auto type_name = cmr->get_item(0)->head_token().token;
                auto record_info = find_type_by_name(cmr->get_item(0), type_name, extra_bindings);
                if (cmr->size() == 1)
                {
					check_is_correct_type_to_use(mr, record_info, type_name);
                    return record_info;
                }
                assert(cmr->size() == 4);
                cmr->get_item(1)->set_hidden(true);
                cmr->get_item(2)->set_hidden(true);
                cmr->get_item(3)->set_hidden(true);
                auto *generic_instance_list_mr = cmr->get_item(2);
                auto generic_instances = get_generic_instances_from_mr(generic_instance_list_mr, extra_bindings);
                if (record_info->record_generics.size() != generic_instances.size())
                {
                    set_error(error_in_match_result(mr, std::string("record need ")
                        + std::to_string(record_info->record_generics.size()) + " generic types but got " + std::to_string(generic_instances.size())));
                    return record_info;
				}
				else {
					// check_is_correct_type_to_use(mr, record_info);
				}
                auto instance_type = create_lua_type_info(GluaTypeInfoEnum::LTI_RECORD);
                copy_lua_type_info(instance_type, record_info);
                // 把record泛型中用到泛型的地方都用实例类替换,除了keeping_generic_names中的部分
                std::vector<GluaTypeInfoP> keeping_generic_types;
                for (size_t i = 0; i < instance_type->record_generics.size(); ++i)
                {
                    auto declare_generic = instance_type->record_generics.at(i);
                    auto real_type = generic_instances.at(i);
                    if (real_type->is_nil() || real_type->is_undefined())
                    {
                        set_error(error_in_match_result(mr, std::string("Can't use nil or undefined type in typedef")));
                    }
                    if (extra_bindings && real_type->etype == GluaTypeInfoEnum::LTI_GENERIC)
                    {
						auto to_add_generic_type = extra_bindings->find_type(real_type->generic_name);
						if(to_add_generic_type)
							keeping_generic_types.push_back(to_add_generic_type);
                    }
					if (real_type->etype != GluaTypeInfoEnum::LTI_GENERIC)
					{
						instance_type->record_applied_generics.push_back(real_type);
						// instance_type->record_applied_generics.push_back(declare_generic);
					}
                    for (auto &p : instance_type->record_props)
                    {
                        // 如果属性值是函数类型，则要递归替换
                        if (p.second->is_function() || p.second->is_union())
                        {
                            auto new_type = create_lua_type_info();
                            copy_lua_type_info(new_type, p.second);
                            auto extra_bindings_for_sub = std::make_shared<GluaExtraBindingsType>();
                            if (extra_bindings)
                            {
								extra_bindings->copy_to(extra_bindings_for_sub);
                            }
							extra_bindings_for_sub->set_type(declare_generic->generic_name, real_type);
                            replace_generic_by_instance(new_type, generic_instances, extra_bindings_for_sub); // FIXME: 函数中泛型替换有BUG
                            p.second = new_type;
                            continue;
                        }
                        if (p.second == declare_generic)
                        {
                            p.second = real_type;
                        }
                    }
                }
                // 替换新实例类的record_name
                if (instance_type->record_generics.size() > 0)
                {
                    std::stringstream ss;
                    if (instance_type->record_name.find('<') != std::string::npos)
                    {
                        // TODO: 如果旧的类名称类似G1<int, T1, string>，实例化后应该变成类似G1<int, string, string>这样的
                        // TODO: 泛型类创建时应该一开始就记录下一共有哪些泛型参数，每次实例化时记录实例化了哪些部分
                        // printf("");
                    }
                    ss << instance_type->record_name; // FIXME .substr(0, instance_type->record_name.find_first_of('<'));
                    ss << "<";
                    bool has_started = false;
                    for (const auto &item : generic_instances)
                    {
                        if (has_started)
                            ss << ", ";
                        ss << item->str(false);
                        has_started = true;
                    }
                    ss << ">";
                    instance_type->record_name = ss.str();
					instance_type->record_origin_name = record_info->record_origin_name;
                }
                instance_type->record_generics.clear();
                if (extra_bindings)
                    instance_type->record_generics = keeping_generic_types;
				if (!_open_record_call_syntax && instance_type->has_call_prop())
					set_error_of_not_support_record_call(mr);
                return instance_type;
            }
            else if (mr->node_name() == "array_type")
            {
                auto *cmr = mr->as_complex();
                auto *type_mr = cmr->get_item(2);
                auto sub_type = get_type_from_mr(type_mr, extra_bindings);
				assert(cmr->size() == 4);
				cmr->get_item(1)->set_hidden(true);
				cmr->get_item(2)->set_hidden(true);
				cmr->get_item(3)->set_hidden(true);
				check_is_correct_type_to_use(mr, sub_type, type_mr->head_token().token);
                auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_ARRAY);
                type_info->array_item_type = sub_type;
                return type_info;
            }
			else if (mr->node_name() == "map_type")
			{
				auto *cmr = mr->as_complex();
				auto *type_mr = cmr->get_item(2);
				auto sub_type = get_type_from_mr(type_mr, extra_bindings);
				assert(cmr->size() == 4);
				cmr->get_item(1)->set_hidden(true);
				cmr->get_item(2)->set_hidden(true);
				cmr->get_item(3)->set_hidden(true);
				check_is_correct_type_to_use(mr, sub_type, type_mr->head_token().token);
				auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_MAP);
				type_info->map_item_type = sub_type;
				return type_info;
			}
            else if (mr->node_name() == "function_type")
            {
                auto *cmr = mr->as_complex();
                bool has_args = cmr->size() > 4;
                auto *args_part_mr = has_args ? cmr->get_item(1) : nullptr;
                auto *ret_part_mr = has_args ? cmr->get_item(4) : cmr->get_item(3);
                std::vector<GluaTypeInfoP> arg_types;
                auto ret_type = create_lua_type_info();
                if (has_args)
                {
                    if (args_part_mr->is_final())
                    {
						auto arg_type = find_type_by_name(args_part_mr, args_part_mr->as_final()->token.token, extra_bindings);
						check_is_correct_type_to_use(args_part_mr, arg_type, args_part_mr->as_final()->token.token);
                        arg_types.push_back(arg_type);
                    }
                    else
                    {
                        auto *args_part_cmr = args_part_mr->as_complex();
						auto arg_type = find_type_by_name(args_part_cmr, args_part_cmr->get_item(0)->as_final()->token.token, extra_bindings);
						check_is_correct_type_to_use(args_part_mr, arg_type, args_part_cmr->get_item(0)->as_final()->token.token);
                        arg_types.push_back(arg_type);
                        size_t i = 1;
                        while (true)
                        {
                            if (i < args_part_cmr->size())
                            {
								auto arg_type_n = find_type_by_name(args_part_cmr->get_item(i), args_part_cmr->get_item(i)->as_complex()->get_item(1)->as_final()->token.token, extra_bindings);
								check_is_correct_type_to_use(args_part_cmr->get_item(i), arg_type_n, args_part_cmr->get_item(i)->as_complex()->get_item(1)->as_final()->token.token);
								arg_types.push_back(arg_type_n);
                                ++i;
                            }
                            else
                                break;
                        }
                    }
                }
                if (ret_part_mr->is_final())
                    ret_type = find_type_by_name(ret_part_mr, ret_part_mr->as_final()->token.token, extra_bindings);
                type_info->etype = GluaTypeInfoEnum::LTI_FUNCTION;
                type_info->is_any_function = false;
                type_info->arg_types = arg_types;
                type_info->ret_types.push_back(ret_type);
                return type_info;
            }
            else
            {
                set_error(error_in_match_result(mr, std::string("get type from unknown syntax")));
                return type_info;
            }
        }

		GluaTypeInfoP GluaTypeChecker::match_functioncall_exp_type(MatchResult *mr, GluaTypeInfoP func_type_info,
			bool is_constructor, GluaTypeInfoP constructor_ret_type, std::vector<GluaTypeInfoP> *pre_args)
        {
			auto *cmr = mr->as_complex();
			auto *last_item = cmr->get_item(cmr->size() - 1);
			// 如果不是function，但是有__call的元函数，也可以调用
			// check type
			if (!func_type_info->may_be_callable())
			{
				set_error(error_in_functioncall(mr, func_type_info));
				return create_lua_type_info();
			}
			if(!_open_record_call_syntax && func_type_info->has_call_prop())
			{
				set_error_of_not_support_record_call(mr);
			}
			if (func_type_info->arg_types.size() == 0 ||
				(func_type_info->arg_types.at(0)->etype != GluaTypeInfoEnum::LTI_OBJECT)) // FIXME
			{
				// <any> (object) 类型的函数，参数可能是不定参数，不检查
				if (last_item->is_complex())
				{
					auto *clast_item = last_item->as_complex();
					bool has_left_xkh = clast_item->size() >= 2 && clast_item->get_item(0)->is_final() && clast_item->get_item(0)->as_final()->token.type == '(';
					std::vector<GluaTypeInfoP> explist;
					if(pre_args)
					{
						for (const auto &item : *pre_args)
							explist.push_back(item);
					}
					if(last_item->node_name()=="tableconstructor")
					{
						auto arg_type = create_lua_type_info();
						if(!check_expr_error(last_item, arg_type, nullptr, mr))
						{
							set_error(error_in_match_result(last_item, std::string("")));
							return create_lua_type_info();
						}
						explist.push_back(arg_type);
					}
					else
					{
						if (has_left_xkh)
						{
							if (clast_item->size() > 2)
							{
								auto remaining_explist = get_explist_from_mr(clast_item->get_item(1));
								for (const auto &item : remaining_explist)
								{
									explist.push_back(item);
								}
							}
						}
						else
						{
							if (clast_item->size() > 0)
							{
								auto* arg1_mr = clast_item->get_item(0);
								explist.push_back(guess_exp_type(arg1_mr));
							}
						}
					}

					size_t args_count = explist.size();
					if (!func_type_info->is_any_function)
					{
						if (args_count <= 0)
						{
							if (func_type_info->min_args_count_require() > 0)
							{
								set_error(error_in_functioncall(mr, func_type_info, &explist));
								return create_lua_type_info();
							}
						}
						else if (args_count < func_type_info->min_args_count_require())
						{
							// 如果实参数量等于形参数量-1并且函数有不定长参数(...)，那么也接受这样类型
							set_error(error_in_functioncall(mr, func_type_info, &explist));
							return create_lua_type_info();
						}
						else if (args_count > func_type_info->arg_types.size()
							&& !func_type_info->has_var_args())
						{
							// 实参数量超过形参数量时，要检查函数形参最后一个是否是...，如果不是...，类型检查要报错
							set_error(error_in_functioncall(mr, func_type_info, &explist));
							return create_lua_type_info();
						}
						else
						{
							size_t i = 0;
							for (; i < func_type_info->min_args_count_require(); ++i)
							{
								auto arg_declare_type = func_type_info->arg_types.at(i);
								auto arg_real_type = i < explist.size() ? explist.at(i) : create_lua_type_info();
								if (!match_declare_type(arg_declare_type, arg_real_type, true))
								{
									set_error(error_in_functioncall(mr, func_type_info, &explist));
									return create_lua_type_info();
								}
							}
						}
					}
					else
					{
						return create_lua_type_info();
					}
				}
			}
			if (func_type_info->is_function())
			{
				if (is_constructor)
					return constructor_ret_type;
				if (func_type_info->ret_types.size() > 0)
				{
					return func_type_info->ret_types.at(0);
				}
			}
			return create_lua_type_info();
        }

		GluaTypeInfoP GluaTypeChecker::get_or_create_array_type_constructor_info(MatchResult *mr, GluaTypeInfoP array_type)
        {
			std::string name = array_type->str();
			auto type_info = find_info_by_varname(mr, name);
			if (type_info && !type_info->is_nil() && !type_info->is_undefined())
				return type_info;
			type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
			type_info->arg_types.push_back(create_lua_type_info());
			type_info->arg_names.push_back("...");
			type_info->ret_types.push_back(array_type);
			define_localvar_in_current_check_proto(mr, name, type_info);
			return type_info;
        }

        GluaTypeInfoP GluaTypeChecker::guess_exp_type(MatchResult *mr)
        {
            // 在当前和上层还有全局词法作用域中查找symbol对应的所有定义处，然后猜测类型
            if (is_simpleexp_node(mr))
            {
                if (mr->is_final())
                {
                    // 判断类型
                    // 如果是varname，从上下文（包括全局变量）中查询类型
                    auto *fmr = mr->as_final();
                    auto token = fmr->token;
					GluaTypeInfoP type_info = nullptr;
                    switch (token.type)
                    {
                    case TOKEN_RESERVED::LTK_INT:
						type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_INT);
						break;
                    case TOKEN_RESERVED::LTK_FLT:
						type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_NUMBER);
						break;
                    case TOKEN_RESERVED::LTK_NIL:
						type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_NIL);
						break;
                    case TOKEN_RESERVED::LTK_TRUE:
                    case TOKEN_RESERVED::LTK_FALSE:
						type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_BOOL);
						break;
                    case TOKEN_RESERVED::LTK_STRING:
						type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_STRING);
						break;
					case TOKEN_RESERVED::LTK_DOTS:
						type_info = find_info_by_varname(mr, token.token);
						break;
                    default:
                        break;
                    }
					if(type_info)
					{
						type_info->is_literal_token_value = true;
						type_info->literal_value_token = token;
						return type_info;
					}
                }
                else if (mr->node_name() == "tableconstructor")
                {
					auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_TABLE);
					if(!check_expr_error(mr, type_info))
					{
						set_error(error_in_match_result(mr, std::string("wrong table literal value format")));
						return create_lua_type_info(GluaTypeInfoEnum::LTI_TABLE);
					}
					return type_info;
                }
            }
            else if (mr->node_name() == "suffixedexp")
            {
                // printf("");
            }
            else if (mr->node_name() == "suffixedexp_visit_prop")
            {
                auto node_result = create_lua_type_info();
                check_expr_error(mr, node_result); // FIXME
                return node_result;
            }
            else if (mr->node_name() == "var")
            {
                auto node_result = create_lua_type_info();
                check_expr_error(mr, node_result); // FIXME
                return node_result;
            }
            else if (mr->node_name() == "prefixexp")
            {
                if (mr->is_final())
                {
                    auto *fmr = mr->as_final();
                    if (fmr->token.type == TOKEN_RESERVED::LTK_NAME)
                    {
                        return find_info_by_varname(fmr, fmr->token.token);
                    }
                }
				else
				{
					// ( exp )
					auto *cmr = mr->as_complex();
					if(cmr->size()==3
						&& cmr->get_item(0)->is_final() 
						&& cmr->get_item(0)->head_token().token=="("
						&& cmr->get_item(2)->is_final()
						&& cmr->get_item(2)->head_token().token == ")")
					{
						return guess_exp_type(cmr->get_item(1));
					}
				}
            }
            else if (mr->node_name() == "functioncall")
            {
                if (mr->is_complex())
                {
                    auto *cmr = mr->as_complex();
					// TODO: 重载函数的函数签名会随参数类型而不同
                    if (cmr->size() >= 2)
                    {
                        auto *item2 = cmr->get_item(1);
						auto *last_item = cmr->get_item(cmr->size() - 1);
						bool add_self_arg = false;
						// TODO: 处理a.b.c:d args的情况
                        if (item2->node_name() == "args" || item2->node_name() == "tableconstructor")
                        {
                            // prefixexp args

							check_expr_error(item2);

                            auto *item1 = cmr->get_item(0);
                            auto func_type_info = create_lua_type_info();
							auto constructor_ret_type = create_lua_type_info(); // 构造函数的返回类型
							bool is_function = false;
							bool is_constructor = false;

							if (item1->is_final())
								is_function = true;
							else if(item1->node_name()=="simple_type" || item1->node_name() == "array_type")
							{
								is_function = true; // 这里是调用构造函数
								is_constructor = true;
								check_expr_error(item1);
								constructor_ret_type = get_type_from_mr(item1);
								func_type_info = constructor_ret_type;
								if(!(func_type_info->is_record() || func_type_info->is_array()))
								{
									set_error(error_in_functioncall(mr, func_type_info));
									return create_lua_type_info();
								}
							} else
							{
								func_type_info = guess_exp_type(item1);
								is_function = func_type_info->is_function();
							}

                            if (is_function)
                            {
								auto funcname_token = item1->head_token();
								if (item1->is_final())
								{
									if(glua::util::vector_contains(exclude_function_names, funcname_token.token))
									{
										set_error(error_in_match_result(item1, std::string("Can't use keyword ") + funcname_token.token + " as function name", GluaTypeCheckerErrors::FUNCTION_NOT_FOUND));
										return create_lua_type_info();
									}

									func_type_info = find_info_by_varname(item1, funcname_token.token);
									if (!func_type_info)
									{
										set_error(error_in_match_result(item1, std::string("Can't find function ") + funcname_token.token, GluaTypeCheckerErrors::FUNCTION_NOT_FOUND));
										return create_lua_type_info();
									}

									// 优先考虑构造函数
									if(func_type_info->is_record() && (funcname_token.token == func_type_info->record_name || funcname_token.token == func_type_info->record_origin_name))
									{
										is_function = true; // 这里是调用构造函数
										is_constructor = true;
										check_expr_error(item1);
										constructor_ret_type = get_type_from_mr(item1);
										func_type_info = constructor_ret_type;
										if (!(func_type_info->is_record() || func_type_info->is_array()))
										{
											set_error(error_in_functioncall(mr, func_type_info));
											return create_lua_type_info();
										}
									}
									else if (!func_type_info->is_function() && func_type_info->may_be_callable() && func_type_info->has_call_prop())
									{
										func_type_info = func_type_info->record_props["__call"];
										if(!_open_record_call_syntax)
										{
											set_error_of_not_support_record_call(mr);
										}
									}
									else if(funcname_token.token == "import_contract")
									{
										if(item2->is_complex())
										{
											if (item2->as_complex()->size() >= 3)
											{
												_imported_contracts[item2->as_complex()->get_item(1)->head_token().token] = mr->head_token().linenumber - _ctx->inner_lib_code_lines() +2;
											}
											else if(item2->head_token().token != "(")
											{
												_imported_contracts[item2->head_token().token] = mr->head_token().linenumber - _ctx->inner_lib_code_lines() + 2;
											}
										} else
										{
											if (item2->head_token().token != "(")
											{
												_imported_contracts[item2->head_token().token] = mr->head_token().linenumber - _ctx->inner_lib_code_lines() + 2;
											}
										}
										// 返回一个满足任何api调用的特殊Contract类型
										auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT);
										type_info->is_any_contract = true;
										return type_info;
									}
								} else if(item1->node_name()=="simple_type")
								{
									assert(func_type_info); // FIXME
								}
                                if (func_type_info->is_record())
                                {
                                    // 用构造函数的声明替换
                                    func_type_info = find_info_by_varname(item1, funcname_token.token + ":new");
                                    if (!func_type_info->may_be_callable())
                                    {
                                        set_error(error_in_match_result(mr, std::string("Can't find constructor of record ") + funcname_token.token));
                                        return create_lua_type_info();
                                    }
                                } else if(func_type_info->is_array())
                                {
									func_type_info = get_or_create_array_type_constructor_info(item1, func_type_info);
                                }
                            }
                            else
                            {
                                if (!check_expr_error(item1, func_type_info))
                                {
                                    set_error(error_in_match_result(item1, std::string("function call of undefined type")));
                                    return create_lua_type_info();
                                }
                                if (func_type_info->etype == GluaTypeInfoEnum::LTI_OBJECT)
                                {
                                    func_type_info = create_lua_type_info();
                                    func_type_info->is_any_function = true;
                                    func_type_info->ret_types.push_back(create_lua_type_info());
                                }
                            }
								
							return match_functioncall_exp_type(mr, func_type_info,
									is_constructor, constructor_ret_type);
                        }
						else if(item2->is_final() && item2->as_final()->token.token == ":" && cmr->size()==4)
						{
							// a:b args
							add_self_arg = true;
							auto *item1 = cmr->get_item(0);
							item2 = cmr->get_item(2);
							auto *arg_item = cmr->get_item(3);
							auto funcname_type = guess_exp_type(item2);
							auto funcname_token = item2->as_final()->token;
							auto obj_to_find_func_type = guess_exp_type(item1);
							if(obj_to_find_func_type->is_contract_type())
							{
								if(glua::util::vector_contains(thinkyoung::lua::lib::contract_special_api_names, funcname_token.token))
								{
									set_error(error_in_match_result(mr, std::string("Can't call ") + funcname_token.token + " api of contract", GluaTypeCheckerErrors::ACCESS_CONTRACT_PROPERTY_DISABLE));
									return create_lua_type_info();
								}
							}

							std::vector<GluaTypeInfoP> pre_args;
							pre_args.push_back(obj_to_find_func_type);

							if(!(can_access_prop_of_type(mr, obj_to_find_func_type, funcname_token.token, GluaTypeInfoEnum::LTI_STRING) || obj_to_find_func_type->etype == GluaTypeInfoEnum::LTI_OBJECT))
							{
								set_error(error_in_match_result(mr, std::string("Can't access ") + funcname_token.token + " of type " + obj_to_find_func_type->str()));
								return create_lua_type_info();
							}
							auto func_type_info = create_lua_type_info();
							if (obj_to_find_func_type->is_record())
								func_type_info = obj_to_find_func_type->record_props[funcname_token.token];
							else
							{
								func_type_info->etype = GluaTypeInfoEnum::LTI_FUNCTION;
								func_type_info->is_any_function = true;
							}
							auto arg_type = guess_exp_type(arg_item);

							auto constructor_ret_type = create_lua_type_info();
							return match_functioncall_exp_type(mr, func_type_info,
								false, constructor_ret_type, &pre_args);
						}
                        else
                        {
                            // prefixexp : exp args
                            auto *item1 = cmr->get_item(0);
                            if (item1->is_final())
                            {
                                auto funcname_token = item1->as_final()->token;

								if (glua::util::vector_contains(exclude_function_names, funcname_token.token))
								{
									set_error(error_in_match_result(item1, std::string("Can't use keyword ") + funcname_token.token + " as function name", GluaTypeCheckerErrors::FUNCTION_NOT_FOUND));
									return create_lua_type_info();
								}

                                auto func_type_info = find_info_by_varname(item1, funcname_token.token);
                                if (!(item2->is_final() && item2->as_final()->token.type == TOKEN_RESERVED::LTK_STRING)
                                    && !(item2->node_name() == "tableconstructor"))
                                {
                                    set_error(error_in_functioncall(mr, func_type_info));
                                }
                                auto arg_type = guess_exp_type(item2);
                                if (func_type_info->arg_types.size() > 1)
                                {
                                    set_error(error_in_functioncall(mr, func_type_info));
                                    return create_lua_type_info();
                                }
                                auto arg_declare_type = func_type_info->arg_types.at(0);;
                                if (!match_declare_type(arg_declare_type, arg_type, true))
                                {
                                    set_error(error_in_functioncall(mr, func_type_info));
                                    return create_lua_type_info();
                                }
                                if (func_type_info->is_function())
                                {
                                    if (func_type_info->ret_types.size() > 0)
                                    {
                                        return func_type_info->ret_types.at(0);
                                    }
                                }
                            }
                            return create_lua_type_info();
                        }
                    }
                }
            }
            else if (is_expr_node(mr))
            {
                auto type_info = create_lua_type_info();
                if (!check_expr_error(mr, type_info))
                {
                    // set_error(error_in_match_result(mr));
                    return create_lua_type_info();
                }
                return type_info;
            }
            else if (mr->node_name() == "funcbody")
            {
                auto type_info = create_lua_type_info(GluaTypeInfoEnum::LTI_FUNCTION);
                type_info->is_any_function = false;
                auto *cmr = mr->as_complex();
                auto *args_def_mr = cmr->get_item(0)->as_complex();
                if (args_def_mr->node_name() == "funcbody_args_def")
                {
                    if (args_def_mr->size() > 2)
                    {
                        auto *parlist_mr = args_def_mr->get_item(1)->as_complex();
						auto *namelist_mr = (parlist_mr->is_complex() && parlist_mr->get_item(0)->node_name() == "namelist") ? parlist_mr->get_item(0) : nullptr;
                        auto namelist = get_namelist_info_from_mr(namelist_mr, parlist_mr, true);
                        for (const auto &p : namelist)
                        {
							type_info->arg_names.push_back(p.name);
                            type_info->arg_types.push_back(p.type_info);
                        }
                    }
                }
                type_info->ret_types.push_back(create_lua_type_info(GluaTypeInfoEnum::LTI_OBJECT));	// TODO: 根据return分支来判断返回类型
                /*
                if (!check_expr_error(mr, &type_info))
                {
                set_error(error_in_match_result(mr));
                return GluaTypeInfo();
                }
                */
                return type_info;
            }
            else if (mr->node_name() == "args")
            {
              if (mr->as_complex()->size() >= 3)
              {
                return guess_exp_type(mr->as_complex()->get_item(1));
              }
              else
                return create_lua_type_info();
            }
            else if (mr->node_name() == "explist")
            {
              if (mr->as_complex()->size() > 0)
              {
                return guess_exp_type(mr->as_complex()->get_item(0));
              }
              else
                return create_lua_type_info();
            }
            else
            {
                return create_lua_type_info();
            }
            return create_lua_type_info();
        }

        GluaTypeInfoP GluaTypeChecker::find_info_by_varname(MatchResult *mr, std::string name, GluaExtraBindingsTypeP extra_bindings, bool only_inited)
        {
            if (extra_bindings)
            {
                auto found_in_extra = extra_bindings->find_variable(name);
                if (found_in_extra)
                    return found_in_extra;
            }
            for (auto i = _current_checking_proto_stack.rbegin(); i != _current_checking_proto_stack.rend(); ++i)
            {
                auto proto = *i;
                auto found = proto->localvars.find(name);
                if (found != proto->localvars.end())
                {
                    if (only_inited)
                    {
                        auto inited_found = proto->localvars_inited.find(name);
                        if (inited_found != proto->localvars_inited.end() && !inited_found->second)
                        {
                            set_error(error_in_match_result(mr, std::string("use variable ") + name + " not inited", GluaTypeCheckerErrors::USE_NOT_INITED_VARIABLE));
                        }
                    }
                    return found->second;
                }
            }
            return create_lua_type_info(GluaTypeInfoEnum::LTI_NIL);
        }

		GluaTypeInfoP GluaTypeChecker::find_function_by_name(MatchResult *mr, std::string name,
			std::vector<GluaTypeInfoP> const &args_types, GluaExtraBindingsTypeP extra_bindings, bool only_inited)
		{
			// 函数重载特化的函数名
			std::stringstream spec_func_name_ss;
			spec_func_name_ss << name;
			for (const auto &arg_type : args_types)
			{
				spec_func_name_ss << "$" << arg_type->str();
			}
			auto spec_func_name = spec_func_name_ss.str();
			auto spec_func = find_info_by_varname(mr, spec_func_name, extra_bindings, only_inited);
			if (spec_func->is_function())
				return spec_func;
			auto not_spec_func_name = name; // 没有特化的函数名
			return find_info_by_varname(mr, not_spec_func_name);
		}

		GluaTypeInfoP GluaTypeChecker::find_operator_func_by_name(MatchResult *mr, std::string operator_name,
			std::vector<GluaTypeInfoP> const &args_types, GluaExtraBindingsTypeP extra_bindings, bool only_inited)
        {
			// 函数重载特化的函数名
			std::stringstream spec_func_name_ss;
			spec_func_name_ss << "(" << operator_name;
			for(const auto &arg_type : args_types)
			{
				spec_func_name_ss << "$" << arg_type->str();
			}
			spec_func_name_ss << ")";
			auto spec_func_name = spec_func_name_ss.str();
			auto spec_func = find_info_by_varname(mr, spec_func_name, extra_bindings, only_inited);
			if (spec_func->is_function())
				return spec_func;
			auto not_spec_func_name = std::string("(") + operator_name + ")"; // 没有特化的函数名
			return find_info_by_varname(mr, not_spec_func_name);
        }

		GluaTypeInfoP GluaTypeChecker::find_type_by_name(MatchResult *mr, std::string name, GluaExtraBindingsTypeP extra_bindings)
        {
			if (extra_bindings)
			{
				auto found_in_extra = extra_bindings->find_type(name);
				if (found_in_extra)
					return found_in_extra;
			}
			for (auto i = _current_checking_proto_stack.rbegin(); i != _current_checking_proto_stack.rend(); ++i)
			{
				auto proto = *i;
				auto found = proto->local_types.find(name);
				if (found != proto->local_types.end())
				{
					return found->second;
				}
			}
			return create_lua_type_info(GluaTypeInfoEnum::LTI_NIL);
        }

        bool GluaTypeChecker::check_var_inited(std::string name)
        {
            for (auto i = _current_checking_proto_stack.rbegin(); i != _current_checking_proto_stack.rend(); ++i)
            {
                auto proto = *i;
                auto found = proto->localvars_inited.find(name);
                if (found != proto->localvars_inited.end())
                    return found->second;
            }
            return false;
        }

        bool GluaTypeChecker::define_localvar_in_current_check_proto(MatchResult *mr, std::string name,
			GluaTypeInfoP type_info, bool is_new, bool changable, bool replace, bool inited, bool is_new_global_var)
        {
            // 本级和祖先级protos中查找变量
            // 如果已经存在并且类型不同，就并入union类型，如果是全局变量，不能改动
            if (name.length() < 1)
                return true;
			if(glua::util::vector_contains(exclude_function_names, name))
			{
				set_error(error_in_match_result(mr, std::string("Can't define variable of keyword name ") + name));
				return true;
			}

            auto proto = first_checking_proto_has_var(name);
            if (nullptr == proto || is_new)
            {
                auto current_proto = current_checking_proto();
				auto using_proto = is_new_global_var ? top_checking_proto() : current_proto;
                if (using_proto)
                {
					if(is_new)
					{
						if(using_proto->localvars.find(name) != using_proto->localvars.end())
						{
							set_error(error_in_match_result(mr, std::string("can't declare duplicate variable ") + name));
							return false;
						}
					}
					using_proto->localvars[name] = type_info;
					using_proto->localvars_changable[name] = changable;
					using_proto->localvars_inited[name] = inited;
                }
            }
            else
            {
				if(proto == top_checking_proto() && is_checking_contract)
				{
					set_error(error_in_match_result(mr, std::string("global variable ") + name + " can't be changed"));
					return false;
				}
                for (auto i = proto->localvars.begin(); i != proto->localvars.end(); ++i)
                {
                    if (i->first == name)
                    {
                        auto changable_old_found = proto->localvars_changable.find(name);
                        bool can_change = true;
                        if (changable_old_found != proto->localvars_changable.end())
                        {
                            auto changable_old = changable_old_found->second;
                            if (!changable_old)
                            {
                                can_change = false;
                            }
                        }
                        if (!can_change && proto->localvars_inited[name])
                        {
                            set_error(error_in_match_result(mr, std::string("changing variable ") + name + " that can't be changed"));
                            return false;
                        }
                        auto old_type = i->second;
                        if (!match_declare_type(old_type, type_info, true))
                        {
                            set_error(error_in_match_result(mr, std::string("variable declared as type ")
                                + lua_type_info_to_str(old_type->etype) + " but got " + lua_type_info_to_str(type_info->etype)));
                            return false;
                        }
						if (old_type->is_nil() || old_type->is_undefined() || old_type->is_union() || old_type->etype == GluaTypeInfoEnum::LTI_OBJECT)
						{
							auto new_type_info = merge_union_types(old_type, type_info);
							if (replace && new_type_info == old_type)
							{
								new_type_info = create_lua_type_info();
								copy_lua_type_info(new_type_info, type_info);
							}
							proto->localvars[name] = new_type_info;
							proto->localvars_inited[name] = proto->localvars_inited[name] || inited;
							return true;
						}
						else
						{
							// 很多情况下，比如var a: int = 1; a=a+1，不应该改变a的类型 
							proto->localvars[name] = old_type;
							proto->localvars_inited[name] = proto->localvars_inited[name] || inited;
							return true;
						}
                    }
                }
            }
            return true;
        }

		bool GluaTypeChecker::define_local_type_in_current_check_proto(MatchResult *mr, std::string name, GluaTypeInfoP type_info,
			bool cover)
        {
			if (name.length() < 1)
				return true;
			if (glua::util::vector_contains(exclude_function_names, name))
			{
				set_error(error_in_match_result(mr, std::string("Can't define type of keyword name ") + name));
				return true;
			}

			auto proto = first_checking_proto_has_var(name);
			if (!proto)
			{
				auto current_proto = current_checking_proto();
				if (current_proto)
				{
					current_proto->local_types[name] = type_info;
				}
			}
			else
			{
				// 这里也作为一个新局部类型
				if (!cover)
				{
					auto current_proto = current_checking_proto();
					if (current_proto)
					{
						current_proto->local_types[name] = type_info;
					}
				}
				else
				{
					// 覆盖
					proto->local_types[name] = type_info;
				}
			}
			return true;
        }

		// TODO: 生成lua代码，要同时生成一个.ldb文件，记录源文件和目标文件的各行的映射关系还有不同proto下不同符号的类型信息
        std::string GluaTypeChecker::dump() const
        {
			// TODO: 把ldb存到文件中
            auto dump_token_parser = std::make_shared<GluaTokenParser>((lua_State*) nullptr);
            auto tokens = dump_proto_tokens(_proto.get());
            std::vector<GluaParserToken> token_list(tokens.begin(), tokens.end());
            dump_token_parser->replace_all_tokens(token_list);
            return dump_token_parser->dump();
        }

		void GluaTypeChecker::dump_ldf_to_file(FILE *file) const
        {
			//if (ldf)
			//	ldf->serialize_to_file(file);
        }

        std::vector<GluaParserToken> GluaTypeChecker::dump_mr_tokens(MatchResult *mr)	const
        {
            std::vector<GluaParserToken> tokens;
            if (mr)
            {
				if(mr->is_ast_node_type())
				{
					auto ast_node = (GluaAstNode*)mr;
					return dump_mr_tokens(ast_node->dump_mr(_ctx));
				}
                else if (mr->is_complex())
                {
                    auto *cmr = mr->as_complex();
                    for (auto i = cmr->items.begin(); i != cmr->items.end(); ++i)
                    {
                        auto *item = *i;
                        if (item->hidden())
                        {
							if (item->hidden_replace_string().length() > 0)
							{
								auto replacement_token = literal_code_token(glua::util::string_trim(item->hidden_replace_string()));
								replacement_token.linenumber = item->head_token().linenumber;
								replacement_token.position = item->head_token().position;
								tokens.push_back(replacement_token);
							}
                            continue;
                        }
                        // 排除function部分，而用dump_proto_tokens来得出proto部分的tokens
                        std::vector<GluaParserToken> sub_tokens;
                        if (item->binding_type() == MatchResultBindingTypeEnum::FUNCTION
                            && nullptr != item->binding())
                        {
                            auto *sub_proto = (LuaProtoSTree *)item->binding();
                            sub_tokens = dump_proto_tokens(sub_proto);
                        }
                        else
                        {
                            sub_tokens = dump_mr_tokens(item);
                        }
                        for (auto j = sub_tokens.begin(); j != sub_tokens.end(); ++j)
                        {
                            tokens.push_back(*j);
                        }
                    }
                }
                else
                {
                    tokens.push_back(mr->as_final()->token);
                }
            }
            return tokens;
        }

        std::vector<GluaParserToken> GluaTypeChecker::dump_proto_tokens(LuaProtoSTree *tree) const
        {
            std::vector<GluaParserToken> tokens;
            // 因为proto的mr只包含了funcbody部分，所以使用记录整体部分的full_mr
            if (nullptr != tree->full_mr)
            {
                tokens = dump_mr_tokens(tree->full_mr);
            }
            return tokens;
        }

        static void sprint_proto_var_type_infos(LuaProtoSTreeP proto, std::stringstream &ss, size_t level)
        {
            if (proto)
            {
                bool has_started = false;
                for (auto i = proto->localvars.begin(); i != proto->localvars.end(); ++i)
                {
                    if (has_started)
                        ss << "\n";
                    for (size_t k = 0; k < level; ++k)
                    {
                        ss << "\t";
                    }
                    ss << i->first << " : " << i->second->str();
                    has_started = true;
                }
                for (auto i = proto->sub_protos.begin(); i != proto->sub_protos.end(); ++i)
                {
                    if (has_started)
                        ss << "\n";

                    for (size_t k = 0; k < level; ++k)
                    {
                        ss << "\t";
                    }
                    ss << "function (";
                    bool has_started2 = false;
                    for (auto j = (*i)->args.begin(); j != (*i)->args.end(); ++j)
                    {
                        if (has_started2)
                            ss << ",";
                        ss << *j;
                        has_started2 = true;
                    }
                    ss << ")\n";

                    sprint_proto_var_type_infos(*i, ss, level + 1);
                    has_started = true;
                }
            }
        }

        std::string GluaTypeChecker::sprint_root_proto_var_type_infos() const												{
            std::stringstream ss;
            size_t level = 0;
            auto top_proto = _current_checking_proto_stack.size() > 0 ? _current_checking_proto_stack.at(0) : _proto;
            if (top_proto)
            {
                sprint_proto_var_type_infos(top_proto, ss, level);
            }
            return ss.str();
        }

		std::vector<std::string> GluaTypeChecker::get_emit_event_types() const
		{
			return _emit_events;
		}

		std::map<std::string, size_t> GluaTypeChecker::get_imported_contracts() const
        {
			return _imported_contracts;
        }

        void GluaTypeChecker::replace_generic_by_instance(GluaTypeInfoP dest, std::vector<GluaTypeInfoP> generic_instances, GluaExtraBindingsTypeP extra_bindings)
        {
            if (!extra_bindings)
                return;
            if (dest->etype == GluaTypeInfoEnum::LTI_RECORD)
            {
                for (size_t i = 0; i < dest->record_generics.size(); ++i)
                {
                    auto declare_generic = dest->record_generics.at(i);
                    auto found_of_real_type = extra_bindings->find_type(declare_generic->generic_name);
                    if (!found_of_real_type)
                        continue;
                    auto real_type = found_of_real_type;
                    if (real_type->etype != GluaTypeInfoEnum::LTI_GENERIC)
                        dest->record_applied_generics.push_back(declare_generic);
                    for (auto &p : dest->record_props)
                    {
                        if (p.second == declare_generic)
                        {
                            p.second = real_type;
                            continue;
                        }
                        // TODO: 如果属性值是函数类型，则要递归替换
                        if (p.second->is_function())
                        {
                            // TODO: 泛型中的函数类型签名，如果函数类型签名里也有泛型参数, extra_bindings可能要变
                            auto new_type = create_lua_type_info();
                            copy_lua_type_info(new_type, p.second);
                            replace_generic_by_instance(new_type, generic_instances, extra_bindings);
                            p.second = new_type;
                        }
                    }
                }
            }
            else if (dest->etype == GluaTypeInfoEnum::LTI_UNION)
            {
                // 暂时不支持直接union字面量，所以暂时没有这个问题
				bool changed = false;
				std::unordered_set<GluaTypeInfoP> new_union_types;
				for(const auto &declare_type : dest->union_types)
				{
					if(declare_type->is_generic())
					{
						auto found_of_real_type = extra_bindings->find_type(declare_type->generic_name);
						if (!found_of_real_type)
							continue;
						auto real_type = found_of_real_type;
						// FIXME: 要先判断是否已经存在
						changed = true;
						new_union_types.insert(real_type);
					}
					else
					{
						auto new_type = create_lua_type_info();
						copy_lua_type_info(new_type, declare_type);
						replace_generic_by_instance(new_type, generic_instances, extra_bindings);
						// FIXME: 要先判断是否已经存在
						changed = true; // FIXME
						new_union_types.insert(new_type);
					}
				}
				dest->union_types = new_union_types;
            }
            else if (dest->etype == GluaTypeInfoEnum::LTI_FUNCTION)
            {
				auto item_type_processor = [this, &generic_instances, &extra_bindings](GluaTypeInfoP new_type)
				{
					if (new_type->is_function() || new_type->is_union() || new_type->is_record())
					{
						replace_generic_by_instance(new_type, generic_instances, extra_bindings);
					}
					else if (new_type->is_generic())
					{
						auto found_of_real_type = extra_bindings->find_type(new_type->generic_name);
						if (!found_of_real_type)
							return;
						auto real_type = found_of_real_type;
						copy_lua_type_info(new_type, real_type);
					}
				};
                for (auto it = dest->arg_types.begin(); it != dest->arg_types.end(); ++it)
                {
                    auto new_type = create_lua_type_info();
                    copy_lua_type_info(new_type, *it);
                    *it = new_type;
					item_type_processor(new_type);
                }
                for (auto it = dest->ret_types.begin(); it != dest->ret_types.end(); ++it)
                {
                    auto new_type = create_lua_type_info();
                    copy_lua_type_info(new_type, *it);
                    *it = new_type;
					item_type_processor(new_type);
                }
            }
        }

    } // end of namespace glua::parser
}