#ifndef ltypechecker_h
#define ltypechecker_h

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

#include <glua/glua_tokenparser.h>
#include <glua/lparsercombinator.h>
#include <glua/glua_debug_file.h>
#include <glua/glua_lutil.h>
#include <glua/ltype_checker_type_info.h>
#include <glua/glua_astparser.h>

namespace glua {
    namespace parser {

        std::string lua_type_info_to_str(const GluaTypeInfoEnum etype);

		template <typename T>
		class GluaNameTypePairT
		{
		public:
			std::string name;
			T type_info;
			std::string type_name;
			bool type_info_defined;
		public:
			inline GluaNameTypePairT(const std::string &_name, T _type_info, const std::string &_type_name, bool _type_info_defined=true)
				: name(_name), type_info(_type_info), type_name(_type_name), type_info_defined(_type_info_defined) {}
			virtual ~GluaNameTypePairT() {}
		};

		typedef GluaNameTypePairT<GluaTypeInfoP> GluaNameTypePair;

        // syntax tree of lua proto
        struct LuaProtoSTree;
        typedef std::shared_ptr<LuaProtoSTree> LuaProtoSTreeP;
        typedef struct LuaProtoSTree
        {
            std::vector<LuaProtoSTreeP>	sub_protos;
            MatchResult *mr;
            MatchResult *full_mr;
            std::vector<std::string> args;
            std::unordered_map<std::string, GluaTypeInfoP> localvars;
            std::unordered_map<std::string, bool> localvars_changable;
            std::unordered_map<std::string, bool> localvars_inited;
			std::unordered_map<std::string, GluaTypeInfoP> local_types; // 当前作用域下的类型列表

			std::vector<GluaNameTypePair> for_namelist;
			MatchResult *condition_mr;

			GluaTypeInfoP type_info;
			LuaProtoSTree();

			bool is_changable_localvar(const std::string &name);

			void add_arg(std::string arg_name, GluaTypeInfoP arg_type);

        } LuaProtoSTree;

		class GluaExtraBindingsType;

		typedef std::shared_ptr<GluaExtraBindingsType> GluaExtraBindingsTypeP;

		class GluaExtraBindingsType
		{
		private:
			std::unordered_map<std::string, GluaTypeInfoP> _variable_extra_bindings;
			std::unordered_map<std::string, GluaTypeInfoP> _type_extra_bindings;
		public:
			GluaExtraBindingsType();
			virtual ~GluaExtraBindingsType();
			void set_type(std::string name, GluaTypeInfoP value);
			void set_variable(std::string name, GluaTypeInfoP value);
			GluaTypeInfoP find_type(std::string name) const;
			GluaTypeInfoP find_variable(std::string name) const;
			void copy_to(GluaExtraBindingsTypeP other);
		};

        enum GluaTypeCheckerErrors
        {
            SIMPLE_ERROR = 0,
            IN_MATCH_RESULT_ERROR = 1,
            FUNCTION_CALL_ERROR = 2,
            BIN_EXP_ERROR = 3,
            UN_EXP_ERROR = 4,
            VARIABLE_TYPE_ERROR = 5,
            FUNCTION_CALL_WITH_WRONG_TYPE = 6,
            FUNCTION_NOT_FOUND = 7,
            TYPE_NOT_FOUND = 8,
            OPERATOR_NOT_FOUND = 9,
            USE_UNDEFINED_VARIABLE = 10,
            USE_NOT_INITED_VARIABLE = 11,
			RETURN_TOO_MORE_VALUES = 12,
			ACCESS_CONTRACT_PROPERTY_DISABLE = 13,
			CONTRACT_NOT_RETURN_CONTRACT_TYPE = 14,
			CONTRACT_NOT_ALLOW_DEFINE_NEW_VARIABLE = 15,
			CONTRACT_STORAGE_TYPE_ERROR = 16,
			TOO_MANY_LOCALVARS_IN_SCOPE = 17,
			USE_WRONG_SYMBOL_AS_VARIABLE_NAME = 18,
			CONTRACT_API_WRONG_ARGS = 19
        };

		// 外部注入的附加函数调用/类函数的关键字的检查函数，用来在type check时做附加检查
		typedef std::function<bool(GluaTypeInfoP, std::vector<GluaTypeInfoP>)> LuaFunctionCallExternalChecker;

		

        class GluaTypeChecker
        {
        private:
            ParserContext *_ctx;
            LuaProtoSTreeP _proto;
            std::string _error;
            std::vector<std::pair<int, std::string>> _errors;
            std::vector<LuaProtoSTreeP> _current_checking_proto_stack; // 当前正在检查的proto的栈,自顶向下递归检查
			std::shared_ptr<thinkyoung::lua::core::LuaDebugFileInfo> ldf; // 调试信息文件信息
			size_t _middle_inserted_code_lines; // 中间生成的代码+头部库代码的行数，通过这个来得到源文件和中间lua源文件的行号映射关系
			// std::vector<LuaFunctionCallExternalChecker> _functioncall_external_checkers; // 附加函数类型检查处理
			std::vector<std::string> _emit_events; // emit关键字抛出的event名称列表
			std::map<std::string, size_t> _created_global_variables; // 代码中新创建的全局变量列表(合约模式下未预先定义的全局变量不允许新建)
			bool _in_repl;
			std::vector<GluaTypeInfoP> _created_lua_type_info_pointers; // GluaTypeInfo类型的指针列表，内存池，析构时释放
			std::map<std::string, size_t> _imported_contracts; // 代码中import_contract的其他合约的合约名
			bool _open_record_call_syntax; // 是否开启__call语法
			bool is_checking_contract;
		public:
            GluaTypeChecker(ParserContext *ctx);
            ~GluaTypeChecker();

			void set_in_repl(bool in_repl);

			bool check_contract_syntax_tree_type(MatchResult *mr, GluaTypeInfoP *ret_contract_type, GluaTypeInfoP *ret_contract_storage_type_out);

            bool check_syntax_tree_type(MatchResult *mr, GluaTypeInfoP ret_type = nullptr, bool is_contract=false); // check types in all protos in the syntax tree

            inline std::string error() const{
                return _error;
            }
            inline std::vector<std::pair<int, std::string>> errors() const
            {
                return _errors;
            }
            inline bool has_error() const
            {
                return _errors.size() > 0;
            }
            // 从类型检查结果中反向生成源代码（类型检查过程中可能会对语法树做出修改）
            std::string dump() const;

			void dump_ldf_to_file(FILE *file) const;

            //  输出proto结构中的变量类型图
            std::string sprint_root_proto_var_type_infos() const;

			std::vector<std::string> get_emit_event_types() const;

			std::map<std::string, size_t> get_imported_contracts() const;

        private:
            GluaTypeChecker *set_error(int error_no, std::string error);
            GluaTypeChecker *set_error(std::pair<int, std::string> error);
            LuaProtoSTreeP current_checking_proto() const;
            LuaProtoSTreeP top_checking_proto() const;
            LuaProtoSTreeP first_checking_proto_has_var(std::string varname) const;

			// 报错没有开启record类型变量当成函数来使用的语法
			void set_error_of_not_support_record_call(MatchResult *mr);

			void add_created_global_variable(std::string varname, size_t linenumber);

			GluaTypeInfoP merge_union_types(GluaTypeInfoP type1,
				GluaTypeInfoP type2);

			void add_emit_event_type(std::string event_type_name);

			GluaTypeInfoP get_index_by_number_type(GluaTypeInfoP type_info);

			GluaTypeInfoP create_array(GluaTypeInfoP array_item_type = nullptr);

			bool can_visit_index_by_number(GluaTypeInfoP type_info);

			bool can_access_prop_of_type(MatchResult *mr, GluaTypeInfoP type_info, std::string prop_name, GluaTypeInfoEnum prop_name_type);

			// 根据类型信息的字符串表示，number, string, number (object), object (...) 等格式
			// 转为结构化信息，格式无法识别的，作为object类型
			GluaTypeInfoP of_type_str(std::string typestr, GluaExtraBindingsTypeP extra=nullptr);

			void init_global_variables_to_proto(LuaProtoSTreeP proto);

			GluaTypeInfoP create_lua_type_info(GluaTypeInfoEnum type = GluaTypeInfoEnum::LTI_OBJECT);

			// 构造函数proto的内容结构
			LuaProtoSTreeP make_proto_tree(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *funcname_mr, MatchResult *funcbody_mr, bool include_children_protos);
			LuaProtoSTreeP make_proto_tree_from_lambda(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *args_mr, MatchResult *body_mr, bool include_children_protos);

			LuaProtoSTreeP make_proto_tree_from_for_range_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *namelist_mr, MatchResult *explist_mr, MatchResult *body_mr, bool include_children_protos);
			LuaProtoSTreeP make_proto_tree_from_for_step_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *name_mr, MatchResult *explist_mr, MatchResult *body_mr, bool include_children_protos);
			LuaProtoSTreeP make_proto_tree_from_do_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *body_mr, bool include_children_protos);
			LuaProtoSTreeP make_proto_tree_from_while_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *condition_mr, MatchResult *body_mr, bool include_children_protos);
			LuaProtoSTreeP make_proto_tree_from_repeat_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
				MatchResult *condition_mr, MatchResult *body_mr, bool include_children_protos);
			LuaProtoSTreeP make_proto_tree_from_if_stat(LuaProtoSTreeP tree, MatchResult *full_mr,
				GluaIfStatNode *ast_node, bool include_children_protos);

			LuaProtoSTreeP find_proto(LuaProtoSTreeP parent_tree, MatchResult *mr, bool include_children_protos = true);
            bool build_proto(LuaProtoSTreeP tree, MatchResult *mr);

			void set_ldf_mapping(MatchResult *mr);


			// 获取本类型的祖先类型列表,按从最低级开始排列
			std::vector<GluaTypeInfoP> parent_types(GluaTypeInfoP type_info);

			// 获取能作为item_type1和item_type2中所有类型的最小declarative type(本身或者作为item_type的父类型)
			GluaTypeInfoP min_sharing_declarative_type(GluaTypeInfoP item_type1, GluaTypeInfoP item_type2);
                
			GluaTypeInfoP guess_exp_type(MatchResult *mr); // 猜测表达式类型
            // 检查表达式中的语法错误
            bool check_expr_error(MatchResult *mr,
                GluaTypeInfoP result_type = nullptr, GluaTypeInfoP ret_type = nullptr, MatchResult *parent_mr=nullptr);
			bool check_bin_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_un_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_local_def_stat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_functiondef_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_local_named_function_def_stat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_named_function_def_stat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_offline_named_function_def_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_tableconstructor_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_functioncall_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_suffixedexp_visit_prop_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_retstat_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_varlist_assign_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_record_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type, MatchResult *parent_mr);
			bool check_record_with_generic_state_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type, MatchResult *parent_mr);
			bool check_typedef_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_emit_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_lambda_value_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_lambda_expr_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_for_range_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_for_step_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_do_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_while_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_repeat_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
            bool check_break_stat_error(MatchResult *mr, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);
			bool check_if_stat_error(glua::parser::GluaIfStatNode *ast_node, GluaTypeInfoP result_type, GluaTypeInfoP ret_type);

			GluaTypeInfoP match_functioncall_exp_type(MatchResult *mr, GluaTypeInfoP func_type_info,
				bool is_constructor, GluaTypeInfoP constructor_ret_type, std::vector<GluaTypeInfoP> *pre_args=nullptr);

			// 每次用到Array<T>的时候，获取具体的T类型应用到Array<T>后的类型签名，没有就创建这个函数类型签名并返回
			GluaTypeInfoP get_or_create_array_type_constructor_info(MatchResult *mr, GluaTypeInfoP array_type);

            // 在本级以及上级作用域或者全局作用域中根据函数名称查找函数
            GluaTypeInfoP find_function_by_name(MatchResult *mr, std::string name,
				std::vector<GluaTypeInfoP> const &args_types, GluaExtraBindingsTypeP extra_bindings = nullptr, bool only_inited = true);
			// 查找操作符对应的函数签名。操作符可能函数重载，同一个操作符的函数签名可能因为参数类型不同而不同
			GluaTypeInfoP find_operator_func_by_name(MatchResult *mr, std::string operator_name,
				std::vector<GluaTypeInfoP> const &args_types, GluaExtraBindingsTypeP extra_bindings = nullptr, bool only_inited = true);
            // 从上下文和全局变量中查找变量信息
            GluaTypeInfoP find_info_by_varname(MatchResult *mr, std::string name,
				GluaExtraBindingsTypeP extra_bindings = nullptr, bool only_inited = true);
            // 从上下文类型和全局类型中查找类型
			GluaTypeInfoP find_type_by_name(MatchResult *mr, std::string name,
				GluaExtraBindingsTypeP extra_bindings=nullptr);
			// 检查变量是否已经初始化过值了
            bool check_var_inited(std::string name);
            // 在当前检查的proto中定义变量,如果已经存在并且类型不同，就并入union类型
            bool define_localvar_in_current_check_proto(MatchResult *mr, std::string name,
				GluaTypeInfoP type_info, bool is_new = false, bool changable = true, bool replace = false,
				bool inited = true, bool is_new_global_var=false);

			// 在当前检查的proto中定义类型，cover表示是否覆盖上层中已经存在的同名类型（如果存在的话）
			bool define_local_type_in_current_check_proto(MatchResult *mr, std::string name, GluaTypeInfoP type_info,
				bool cover = false);

            // 把proto反向转成源代码对应的tokens
            std::vector<GluaParserToken> dump_proto_tokens(LuaProtoSTree* tree) const;
            std::vector<GluaParserToken> dump_mr_tokens(MatchResult *mr) const;
            // 检查函数声明是否和函数调用时参数类型匹配
            bool match_function_args(GluaTypeInfoP declare_func_type, std::vector<MatchResult*> args_mrs);
            // 从mr中获取到namelist节点的内容
            std::vector<GluaNameTypePair> get_namelist_info_from_mr(MatchResult *mr, MatchResult *parlist_mr = nullptr, bool default_type_is_object=true);
            std::vector<GluaTypeInfoP> get_explist_from_mr(MatchResult *mr, bool *has_error =nullptr);
			std::vector<MatchResult *> get_varlist_from_mr(MatchResult *mr); // TODO: 给var_p定义一个类，包装var_p的信息
            std::vector<std::string> get_generic_list_from_mr(MatchResult *mr);
            std::vector<GluaTypeInfoP> get_generic_instances_from_mr(MatchResult *mr,
				GluaExtraBindingsTypeP extra_bindings = nullptr);

			// 检查是否是完整可用的类型（排除空类型，不完整的类型，没完全应用的泛型等）
			bool check_is_correct_type_to_use(MatchResult *mr, GluaTypeInfoP type_info, std::string type_name);

			// 从mr中获取到类型信息，extra_bindings附加命名空间
            GluaTypeInfoP get_type_from_mr(MatchResult *mr, GluaExtraBindingsTypeP extra_bindings = nullptr);
            // 进入静态分析类型信息的下一层proto进行分析
            void enter_proto_to_checking_type(MatchResult *mr, LuaProtoSTreeP proto);
			// 结束对当前proto的检查
			bool finish_check_current_checking_type(MatchResult *mr);
            // 泛型类型用具体类型实例化
            void replace_generic_by_instance(GluaTypeInfoP dest, std::vector<GluaTypeInfoP> generic_instances,
				GluaExtraBindingsTypeP extra_bindings = nullptr);

            // 自动产生自定义record类型的构造函数并注入语法树中
            void generate_record_constructor_code(MatchResult *mr, GluaTypeInfoP record, MatchResult *parent_mr=nullptr);

            std::pair<int, std::string> error_in_functioncall(MatchResult *mr, GluaTypeInfoP func_type_info, std::vector<GluaTypeInfoP> *real_args = nullptr) const;
            std::pair<int, std::string> error_of_wrong_type_call(MatchResult *mr, GluaParserToken token, std::string expected_types_str) const;
            std::pair<int, std::string> error_in_match_result(MatchResult *mr, std::string more = "", int error_no = GluaTypeCheckerErrors::IN_MATCH_RESULT_ERROR) const;
        };

    } // end of namespace glua::parser
}

#endif