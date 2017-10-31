#ifndef ltype_checker_type_info_h
#define ltype_checker_type_info_h

#include <glua/lprefix.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include <unordered_set>
#include <queue>
#include <memory>
#include <functional>
#include <algorithm>

#include <glua/llimits.h>
#include <glua/lstate.h>		  
#include <glua/lua.h>
#include <glua/thinkyoung_lua_api.h>
#include <glua/glua_tokenparser.h>
#include <glua/lparsercombinator.h>
#include <glua/exceptions.h>
#include <glua/glua_debug_file.h>
#include <glua/glua_lutil.h>

namespace glua {
	namespace parser {

		typedef ::GluaTypeInfoEnum GluaTypeInfoEnum;

		struct GluaTypeInfo;


		typedef GluaTypeInfo* GluaTypeInfoP;

		struct GluaTypeInfo
		{
			GluaTypeInfoEnum etype;

			// function type fields
			std::vector<std::string> arg_names;
			// 如果这里会出现循环引用，或者嵌套函数，考虑用object类型，或者在GluaTypeChecker中记录所有类型信息的对象，然后使用GluaTypeInfo *
			std::vector<GluaTypeInfoP> arg_types;
			std::vector<GluaTypeInfoP> ret_types;
			bool is_offline;
			bool declared;
			bool is_any_function;
			bool is_any_contract; // 作为合约使用，但是其他属性类似table
			bool is_stream_type; // 是否是二进制流类型，也是内置类型之一

			bool is_literal_token_value; // 是否是单token字面量值
			GluaParserToken literal_value_token; // 如果是单token表达式，这里存储字面值的token

			// end function type fields

			// record类型的各属性
			std::unordered_map<std::string, GluaTypeInfoP> record_props;
			std::unordered_map<std::string, std::string> record_default_values; // record属性的默认值
			std::string record_name; // record类型名称，如果用了typedef，这里就是新名称
			std::string record_origin_name; // record原始名称，如果用到泛型，那这就是泛型完全展开的类型名称，比如G1<G2<Person, string>, string, int>
			std::vector<GluaTypeInfoP> record_generics; // record类型中用到的泛型
			std::vector<GluaTypeInfoP> record_all_generics; // record类型创建时的所有泛型参数
			std::vector<GluaTypeInfoP> record_applied_generics; // record类型创建后被实例化的所有泛型参数

			// 泛型类型的属性
			std::string generic_name; // 泛型名称

			// 列表类型的属性
			GluaTypeInfoP array_item_type; // 列表类型中的每一项的类型

			// Map类型的属性
			GluaTypeInfoP map_item_type; // Map类型中的值类型
			bool is_literal_empty_table; // 是否空的字面量Array/Map类型

			// literal type的属性
			std::vector<GluaParserToken> literal_type_options; // literal type的可选的值

			std::unordered_set<GluaTypeInfoP> union_types; // may be any one of these types, not supported nested full info function now

			GluaTypeInfo(GluaTypeInfoEnum type_info_enum = GluaTypeInfoEnum::LTI_OBJECT);

			bool is_contract_type() const;

			bool is_function() const;

			bool is_int() const;

			bool is_number() const;

			bool is_bool() const;

			bool is_string() const;

			// 是literal type中的可以包含的每一项的类型
			bool is_literal_item_type() const;

			// TODO: 非__call的其他__开头的其他二参数的元方法，在使用在操作符上时要用重载函数来判断类型

			bool has_call_prop() const;

			bool may_be_callable() const;

			bool is_nil() const;

			bool is_undefined() const;

			bool is_union() const;

			bool is_record() const;

			bool is_array() const;

			bool is_map() const;
				
			bool is_table() const;

			// 狭义的table类型，etype是LTI_TABLE类型
			bool is_narrow_table() const;

			// 表现起来像table的类型，包括table, record, Map, Array
			bool is_like_table() const;
				
			bool is_generic() const;

			bool is_literal_type() const;

			// 判断函数中是否有...参数（是否参数不定长度)
			bool has_var_args() const;

			// 有元表函数(__开头的成员方法，会自动放入record类型的metatable中)
			bool has_meta_method() const;

			bool is_same_record(GluaTypeInfoP other) const;

			size_t min_args_count_require() const;

			// 检查literal type的类型是否匹配右值的类型
			bool match_literal_type(GluaTypeInfoP value_type) const;
			// 如果literal type的右值是单token的字面量，检查是否匹配
			bool match_literal_value(GluaParserToken value_token) const;

			// 检查literal type中是否有某个项是value_type类型
			bool contains_literal_item_type(GluaTypeInfoP value_type) const;

			// @param show_record_details 是否显示record类型的明细，这是为了避免递归死循环
			std::string str(const bool show_record_details = false) const;

			// 把合约的storage类型信息放入glua模块流中
			// @throws LuaException
			bool put_contract_storage_type_to_module_stream(GluaModuleByteStreamP stream);

			// 把合约的storage的APIs参数信息放入glua模块流中
			// @throws LuaException
			bool put_contract_apis_info_to_module_stream(GluaModuleByteStreamP stream);

		};

	} // end namespace glua::parser
}

#endif
