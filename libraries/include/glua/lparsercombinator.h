#ifndef lparsercombinator_h
#define lparsercombinator_h

#include <glua/lprefix.h>
#include <string>
#include <list>
#include <memory>
#include <functional>

#include <glua/llimits.h>		  
#include <glua/lua.h>
#include <glua/thinkyoung_lua_api.h>
#include <glua/glua_tokenparser.h>
#include <glua/glua_lutil.h>

namespace glua {
    namespace parser {

		typedef struct Var
		{
			std::string name;
			bool is_middle_var;
			bool is_not_left_var;
			std::shared_ptr<Var> origin_var;
			size_t min_count;
			std::string var_type;
		} Var;

		inline std::string var_to_str(Var &v)
		{
			return v.name;
		}
		typedef std::shared_ptr<Var> VarP;

		VarP create_var(std::string &name);
		VarP create_final_var(std::string &name);
		VarP create_empty_var();

		struct RuleItem;

		typedef struct RuleItem
		{
			std::vector<std::shared_ptr<struct RuleItem>> items;
			std::string extra_info;
		} RuleItem;

		typedef std::shared_ptr<RuleItem> RuleItemP;

		struct MatchRule
		{
			std::vector<RuleItemP> items;
		};

		struct Rule
		{
			VarP dest_var;
			std::vector<RuleItemP> items;
		};

		class Input
		{
		private:
			GluaTokenParser *_token_parser;
			size_t _pos;
			size_t _end_skipped_size;
		public:
			inline Input() : _token_parser(nullptr), _pos(0), _end_skipped_size(0) {}
			inline Input(GluaTokenParser *token_parser)
				: _token_parser(token_parser), _pos(0), _end_skipped_size(0) {
				_token_parser->reset_position();
			}
			Input(const Input &other)
				: _token_parser(other._token_parser), _pos(other._pos), _end_skipped_size(other._end_skipped_size) {
				_token_parser->reset_position();
			}
			inline size_t pos() const {
				return _pos;
			}
			inline size_t end_skipped_size() const {
				return _end_skipped_size;
			}
			inline void inc_pos(int offset)
			{
				_pos += offset;
				_token_parser->reset_position(_pos);
			}
			inline GluaTokenParser *source() const {
				return _token_parser;
			}
			inline GluaParserToken prev_token(size_t skip_end_size = 0)
			{
				if(_pos<=1 || _token_parser->size()<_pos)
				{
					GluaParserToken token;
					token.linenumber = 0;
					token.position = 0;
					token.end_linenumber = 0;
					token.type = (TOKEN_RESERVED)0;
					return token;
				}
				return _token_parser->nth_token(_pos - 1);
			}
			inline bool can_read(size_t n = 1, size_t skip_end_size = 0) {
				if (n < 1)
					return true;
				if (nullptr == _token_parser)
					return false;
				return _pos + n <= source()->size() - _end_skipped_size - skip_end_size;
			}
			inline GluaParserToken read(size_t skip_end_size = 0) {
				if (can_read(1, skip_end_size))
				{
					_token_parser->reset_position(_pos);
					return source()->current();
				}
				GluaParserToken token;
				token.linenumber = 0;
				token.position = 0;
				token.end_linenumber = 0;
				token.type = (TOKEN_RESERVED)0;
				return token;
			}

			inline GluaParserToken lookahead(size_t skip_end_size=0)
			{
				if(can_read(2, skip_end_size))
				{
					_token_parser->reset_position(_pos + 1);
					auto result = source()->current();
					_token_parser->reset_position(_pos);
					return result;
				}
				GluaParserToken token;
				token.linenumber = 0;
				token.position = 0;
				token.end_linenumber = 0;
				token.type = (TOKEN_RESERVED)0;
				return token;
			}

			inline std::vector<GluaParserToken> lookahead_tokens(size_t n = 1, size_t skip_end_size=0)
			{
				// 能lookahead几个是几个
				std::vector<GluaParserToken> tokens;
				for(size_t i=0;i<n;++i)
				{
					// FIXME: 处理skip_end_size
					_token_parser->reset_position(_pos + i + 1);
					if (_token_parser->eof())
						break;
					auto token = source()->current();
					tokens.push_back(token);
				}
				_token_parser->reset_position(_pos);
				return tokens;
				/*
				if (can_read(n + 1, skip_end_size))
				{
					for (size_t i = 0; i < n; ++i)
					{
						_token_parser->reset_position(_pos + i + 1);
						auto token = source()->current();
						tokens.push_back(token);
					}
					_token_parser->reset_position(_pos);
					return tokens;
				}
				GluaParserToken token;
				token.linenumber = 0;
				token.position = 0;
				token.end_linenumber = 0;
				token.type = (TOKEN_RESERVED)0;
				tokens.push_back(token);
				return tokens;
				*/
			}

			/*
			* @param ignore_end_size 表示从input中读取时忽略最后的ignore_end_size
			*/
			inline Input skip_end()
			{
				Input input(*this);
				return input;
			}
			inline void reset_end_skipped(size_t new_end_skipped_size = 0)
			{
				_end_skipped_size = new_end_skipped_size;
			}

			// 获取position位置后的下一个token，或者EOF
			inline GluaParserToken token_after_position(size_t position)
			{
				for (const auto &item : *(this->source()->all_tokens()))
				{
					if (item.position > position)
						return item;
				}
				return glua::parser::eof_token();
			}
		};

		typedef enum ParserState { SUCCESS=0, FAILURE=1, ERROR=2 } ParserState;

		class ComplexMatchResult;
		class FinalMatchResult;

		class ParserFunctor;

		enum MatchResultBindingTypeEnum
		{
			NOT_BINDING=0, OBJECT=1, INT=2, NUMBER=3, BOOL=4, STRING=5, TABLE = 6, FUNCTION = 7, NIL = 100
		};

		class MatchResult
		{
		private:
			std::string _node_name;
			ParserFunctor *_parser;
			void *_binding;	// binding data, such as table, literal, function, etc.
			MatchResultBindingTypeEnum _binding_type; // binding data type 
			bool _hidden; // 在从语法树dump出源代码时，这个mr是否隐藏
			std::string _hidden_replace_string; // 如果这个mr是hidden的，那用这个字符串来作为mr的dump值
			bool _need_end_of_line; // 这个mr是否是当前行的最后一项（也就是下一个mr如果有的话必须不同行号)
		public:
			inline MatchResult()
				:_node_name(""), _parser(nullptr), _binding(nullptr),
				_binding_type(MatchResultBindingTypeEnum::NOT_BINDING), _hidden(false),
				_need_end_of_line(false)
			{}
			inline virtual ~MatchResult()
			{}
			inline virtual Input next_input() const {
				return Input();
			}
			inline virtual void set_input(Input &input) {}
			inline virtual bool is_final() const {
				return true;
			}
			inline virtual bool is_complex() const {
				return !is_final();
			}

			// 是否是AstNode类型或其子孙类型
			inline virtual bool is_ast_node_type() const
			{
				return false;
			}

			inline virtual MatchResult *set_hidden(bool hidden)
			{
				_hidden = hidden;
				return this;
			}

			inline virtual bool hidden() const
			{
				return _hidden;
			}

			inline virtual MatchResult *set_hidden_replace_string(std::string value)
			{
				_hidden_replace_string = value;
				return this;
			}

			inline virtual std::string hidden_replace_string() const
			{
				return _hidden_replace_string;
			}

			inline virtual MatchResult *set_need_end_of_line(bool value)
			{
				_need_end_of_line = value;
				return this;
			}

			inline virtual bool need_end_of_line() const
			{
				return _need_end_of_line;
			}

			// 隐藏mr导致源代码少了多少行，需要mr的开头是一行的第一个字符
			virtual size_t linenumber_after_end(MatchResult *parent_mr) const;

			inline virtual void set_node_name(std::string name)
			{
				_node_name = name;
			}
			inline virtual std::string node_name() const
			{
				return _node_name;
			}

			inline virtual ComplexMatchResult *as_complex() const
			{
				return (ComplexMatchResult*) this;
			}
			inline virtual FinalMatchResult *as_final() const
			{
				return (FinalMatchResult*) this;
			}
			inline virtual MatchResult *set_parser(ParserFunctor *parser)
			{
				_parser = parser;
				return this;
			}

			inline virtual ParserFunctor *parser() const
			{
				return _parser;
			}
			virtual MatchResult *invoke_parser_post_callback();
			inline virtual void *binding() const
			{
				return _binding;
			}
			inline virtual MatchResultBindingTypeEnum binding_type() const
			{
				return _binding_type;
			}
			inline virtual void set_binding(void *b)
			{
				this->_binding = b;
			}
			inline virtual void set_binding_type(MatchResultBindingTypeEnum t)
			{
				this->_binding_type = t;
			}
			virtual std::string str() const = 0;

			virtual GluaParserToken head_token() const = 0;
			virtual GluaParserToken last_token() const = 0;

			static bool isSameLine(MatchResult *mr1, MatchResult *mr2)
			{
				size_t line1 = mr1->last_token().linenumber;
				size_t line2 = mr2->head_token().linenumber;
				return line1 == line2;
			}
		};

		class FinalMatchResult : public MatchResult
		{
		public:
			FinalMatchResult(Input &input);
			glua::parser::GluaParserToken token;
			Input _next_input;
			virtual Input next_input() const;
			inline virtual bool is_final() const {
				return true;
			}
			inline virtual void set_input(Input &input)
			{
				_next_input = input;
			}
			inline virtual std::string str() const
			{
				return token.source_token.length() < 1 ? token.token : token.source_token;
			}
			virtual GluaParserToken head_token() const
			{
				return token;
			}
			virtual GluaParserToken last_token() const
			{
				return token;
			}
		};

		class ComplexMatchResult : public MatchResult
		{
		public:
			ComplexMatchResult(Input &input);
			std::vector<MatchResult*> items;
			Input _next_input;
			virtual Input next_input() const;
			inline virtual bool is_final() const {
				return false;
			}
			inline void add_item(MatchResult *r)
			{
				items.push_back(r);
			}
			inline void insert_item_at(size_t pos, MatchResult *r)
			{
				items.insert(items.begin() + pos, r);
			}
			inline MatchResult *get_item(size_t n)
			{
				return items.at(n);
			}
			inline size_t size() const
			{
				return items.size();
			}
			inline virtual void set_input(Input &input)
			{
				_next_input = input;
			}
			inline virtual MatchResult *invoke_parser_post_callback()
			{
				auto res = this->MatchResult::invoke_parser_post_callback();
				if (res != this)
					return res;
				for (auto i = items.begin(); i != items.end(); ++i)
				{
					auto *mr = *i;
					*i = mr->invoke_parser_post_callback();
				}
				return this;
			}
			inline virtual std::string str() const
			{
				std::stringstream ss;
				auto it = items.begin();
				while (it != items.end())
				{
					if (ss.str().length() > 0)
						ss << " ";
					ss << (*it)->str();
				}
				return ss.str();
			}
			virtual GluaParserToken head_token() const
			{
				if (items.size() > 0)
					return (items.at(0))->head_token();
				else
				{
					GluaParserToken token;
					token.token = "";
					token.position = 0;
					token.linenumber = 0;
					token.type = (TOKEN_RESERVED)-1;
					return token;
				}
			}

			virtual GluaParserToken last_token() const
			{
				if (items.size() > 0)
					return items.at(items.size() - 1)->last_token();
				else
				{
					GluaParserToken token;
					token.token = "";
					token.position = 0;
					token.linenumber = 0;
					token.type = (TOKEN_RESERVED)-1;
					return token;
				}
			}
		};

		class ParseResult
		{
		private:
			ParserState _state;
			MatchResult *_result;
		public:
			inline ParseResult()
			{

			}
			ParseResult(ParserState s, MatchResult *r);
			inline ParseResult(const ParseResult &other)
			{
				this->_state = other._state;
				this->_result = other._result;
			}
			inline ParserState state() const {
				return _state;
			}
			inline void set_state(ParserState s)
			{
				_state = s;
			}
			inline MatchResult *result() const {
				return _result;
			}
			inline void set_result(MatchResult *r)
			{
				_result = r;
			}
		};

		typedef std::function<MatchResult*(ParserFunctor *, MatchResult*)> ParserFunctorPostCallbackType;
		class ParserContext;

		class ParserFunctor
		{
		private:
			std::string _node_name;
			// post-callback called after match successfully, can use this to generate bytecode or do other thing
			ParserFunctorPostCallbackType _post_callback;
			bool _ignore_parsing_level_when_matched;
			std::function<bool(Input &, MatchResult*)> _result_requirement;
			ParserFunctor *_parent_parser;
		public:
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			inline ParserFunctor() : 
				_ignore_parsing_level_when_matched(false), _parent_parser(nullptr){}
			inline virtual ~ParserFunctor() {}

			inline virtual void finish()
			{}

			inline void set_parent_parser(ParserFunctor *parser)
			{
				if (this == parser)
					return;
				_parent_parser = parser;
			}

			inline ParserFunctor *parent_parser() const
			{
				return _parent_parser;
			}

			inline virtual bool is_fixed_length() const
			{
				return false;
			}

			inline virtual bool is_empty_parser() const
			{
				return false;
			}

			inline virtual bool is_cached() const {
				return false;
			}
			inline virtual ParserFunctor *head() {
				return this;
			}
			inline virtual std::vector<ParserFunctor*> tail() {
				return std::vector<ParserFunctor*>();
			}
			inline virtual bool is_union() const {
				return false;
			}
			inline virtual bool is_concat() const {
				return false;
			}
			inline virtual size_t size() const {
				return 1;
			}
			inline virtual size_t min_length() const {
				return 1;
			}
			inline virtual std::string str() const
			{
				const auto &lines = str_lines();
				return glua::util::string_join(lines.begin(), lines.end(), "\n");
			}
			// 转成多行字符串，每一项是转成最终字符串的每一行(这么做是为了方便增加缩进)
			virtual std::vector<std::string> str_lines() const = 0;

			inline virtual bool is_literal() const
			{
				return false;
			}
			ParserFunctor *set_result_requirement(std::function<bool(Input &, MatchResult*)> fn);
			// 判断接下来的token是否是types中某个type开头或者换行
			ParserFunctor *set_next_token_types_in_types_or_newline(std::vector<int> types);
			ParserFunctor *set_items_in_same_line();
			inline std::function<bool(Input &, MatchResult*)> result_requirement() const
			{
				return _result_requirement;
			}
			inline virtual ParserFunctor *set_ignore_parsing_level_when_matched(bool ignore_parsing_level_when_matched)
			{
				_ignore_parsing_level_when_matched = ignore_parsing_level_when_matched;
				return this;
			}
			inline virtual bool ignore_parsing_level_when_matched() const {
				return _ignore_parsing_level_when_matched;
			}
			inline virtual ParserFunctor *lookahead()
			{
				// look ahead word to optimize performance
				// only call this after the parser rules defined completely
				return this;
			}
			virtual void after_match(Input &input, ParseResult &pr);
			inline ParserFunctor *set_node_name(std::string name)
			{
				_node_name = name;
				return this;
			}
			inline ParserFunctor *as(std::string name)
			{
				return set_node_name(name);
			}
			inline std::string node_name() const
			{
				return _node_name;
			}
			inline ParserFunctor *post(ParserFunctorPostCallbackType callback)
			{
				this->_post_callback = callback;
				return this;
			}
			inline ParserFunctorPostCallbackType post_callback() const
			{
				return _post_callback;
			}
			inline MatchResult *invoke_post_callback(MatchResult *mr)
			{
				if (_post_callback)
					return _post_callback(this, mr);
				else
					return mr;
			}

			// 这个parserfunctor能接受参数中的tokens的数量
			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				return 0;
			}

			// 把parsers按first token分组，只考虑其中的concat parser和concat cached parsers, 但是可能有子orParser，
			// 里面也可能包含这些first token的parser
			static std::vector<ParserFunctor*> get_parsers_having_first_token(
				ParserContext *ctx, ParserFunctor *parsers_key, std::vector<ParserFunctor*> &parsers, std::vector<GluaParserToken> first_tokens);
		};

	#define PARSING_MAX_LEVEL_DEPTH 100 // max recursive parser function call depth

		typedef std::vector<ParserFunctor*> ParserListT;

		typedef std::vector<std::pair<std::vector<size_t>,
			std::vector<ParserFunctor*>>> ParsersHavingFirstTokenCacheT;

		class ParserContext
		{
		private:
			ParserFunctor *_lua_parser;
			size_t _parsing_level;
			std::vector<ParserFunctor*> _parser_functors;
			std::vector<MatchResult*> _match_results;
			ParserFunctor *_failure_parser;
			ParserFunctor *_error_parser;
			ParserFunctor *_empty_parser;
			ParserFunctor *_lua_symbol_parser;
			ParserFunctor *_string_parser;
			ParserFunctor *_number_parser;
			std::string _error;
			GluaParserToken _last_error_token; // 遇到某个concat规则匹配first 规则成功，follow规则失败时，用这个记录错误token
			GluaParserToken _last_second_error_token; // 倒数第二个_last_error_token，当_last_error_token被赋新值时旧值放入这里，这是考虑到一些相同first rule有多重follow，方便
			GluaParserToken _last_match_token; // 最后一个尝试匹配的token. 最终取错误语法所在token时，如果_last_error_token的position超过_last_match_token的position就用_last_error_token，否则用_last_match_token
			std::vector<std::string> _maybe_errors; // 可能的错误原因
			std::unordered_map<std::string, ParserFunctor*> _literal_parser_functors;
			std::unordered_map<int, ParserFunctor*> _token_type_parser_functors;
			std::unordered_map<std::string, ParserFunctor*> _named_parser_functors;
			std::unordered_map<ParserFunctor*, size_t> _parsers_min_length_cache;
			std::unordered_map<ParserFunctor*, std::shared_ptr<std::unordered_map<size_t, ParseResult>>> _parsers_parse_result_cache;

			size_t _inner_lib_code_lines = 0; // 内置库有多少行

			// 每个parser functor的执行次数，方便调试用
			std::unordered_map<std::string, size_t> _parser_functors_executed_count;

		public:
			std::unordered_map<ParserFunctor*, ParsersHavingFirstTokenCacheT> _get_parsers_having_first_token_cache;
		private:
			MatchResult *make_if_ast_node(ParserFunctor *pf, MatchResult *mr);
		public:
			inline GluaParserToken last_error_token() const {
				return _last_error_token;
			}
			inline ParserContext *set_last_error_token(GluaParserToken token)
			{
				if (token.position > _last_second_error_token.position)
					_last_second_error_token = _last_error_token;
				_last_error_token = token;
				return this;
			}
			inline GluaParserToken last_second_error_token() const {
				return _last_second_error_token;
			}
			inline GluaParserToken last_match_token() const
			{
				return _last_match_token;
			}
			inline ParserContext *set_last_match_token(GluaParserToken token)
			{
				_last_match_token = token;
				return this;
			}

			inline ParserContext *add_maybe_error(std::string error_str)
			{
				for(const auto &item : _maybe_errors)
				{
					if (item == error_str)
						return this;
				}
				_maybe_errors.push_back(error_str);
				return this;
			}

			inline void expect_token(Input &input, std::string token, bool optional=true, size_t skip_end_size=0)
			{
				auto cur_token = input.read(skip_end_size);
				if(!input.can_read(1, skip_end_size) || cur_token.token != token)
				{
					set_last_error_token(cur_token);
					auto error_str = std::string("expect ") + token + " but got " + cur_token.token + " in line " + std::to_string(cur_token.linenumber - 5); // FIXME
					if (!optional)
						set_error(error_str);
					else
						add_maybe_error(error_str);
				}
			}

			inline void expect_token_type(Input &input, TOKEN_RESERVED type, bool optional = true, size_t skip_end_size = 0)
			{
				auto cur_token = input.read(skip_end_size);
				if (!input.can_read(1, skip_end_size) || cur_token.type != type)
				{
					set_last_error_token(cur_token);
					auto error_str = std::string("expect token type ") + std::to_string(type) + " but got " + cur_token.token + " in line " + std::to_string(cur_token.linenumber - 5); // FIXME
					if (!optional)
						set_error(error_str);
					else
						add_maybe_error(error_str);
				}
			}

			std::string simple_token_error() const;

			std::string simple_token_error(GluaParserToken token, bool with_maybe_errors = true) const;

			inline ParserContext *set_inner_lib_code_lines(size_t lines_count)
			{
				_inner_lib_code_lines = lines_count;
				return this;
			}

			inline size_t inner_lib_code_lines() const
			{
				return _inner_lib_code_lines;
			}

			inline void incr_parser_functor_executed_count(std::string parser_functor_name)
			{
				if (_parser_functors_executed_count.find(parser_functor_name) == _parser_functors_executed_count.end())
					_parser_functors_executed_count[parser_functor_name] = 1;
				else
					_parser_functors_executed_count[parser_functor_name] += 1;
			}

			inline void add_parser_functor(ParserFunctor *fn)
			{
				_parser_functors.push_back(fn);
			} 
			inline void add_match_result(MatchResult *r)
			{
				_match_results.push_back(r);
			}

			inline ComplexMatchResult *create_complex_match_result(const Input &input)
			{
				auto *mr = new ComplexMatchResult(const_cast<Input&>(input));
				add_match_result(mr);
				return mr;
			}
			inline FinalMatchResult *create_final_match_result(const Input &input)
			{
				auto *mr = new FinalMatchResult(const_cast<Input&>(input));
				add_match_result(mr);
				return mr;
			}

			ParserContext();
			~ParserContext();

			bool parsing_level_overflow(Input *input);

			inline size_t parsing_level() const {
				return _parsing_level;
			}

			inline ParserContext *set_parsing_level(size_t level)
			{
				_parsing_level = level;
				return this;
			}

			inline ParserContext *inc_parsing_level()
			{
				++_parsing_level;
				return this;
			}
			inline ParserContext *dec_parsing_level()
			{
				if (_parsing_level > 0)
					--_parsing_level;
				return this;
			}
			ParserFunctor *concat_parsers(std::vector<ParserFunctor*> &parsers);
			ParserFunctor *union_parsers(std::vector<ParserFunctor*> &parsers);
			ParserFunctor *p_and(ParserFunctor *parser1, ParserFunctor *parser2,
				ParserFunctor *parser3 = nullptr, ParserFunctor *parser4 = nullptr, ParserFunctor *parser5 = nullptr,
				ParserFunctor *parser6 = nullptr, ParserFunctor *parser7 = nullptr, ParserFunctor *parser8 = nullptr,
				ParserFunctor *parser9 = nullptr, ParserFunctor *parser10 = nullptr, ParserFunctor *parser11 = nullptr,
				ParserFunctor *parser12 = nullptr, ParserFunctor *parser13 = nullptr, ParserFunctor *parser14 = nullptr,
				ParserFunctor *parser15 = nullptr, ParserFunctor *parser16 = nullptr, ParserFunctor *parser17 = nullptr);
			ParserFunctor *p_or(ParserFunctor *parser1, ParserFunctor *parser2,
				ParserFunctor *parser3 = nullptr, ParserFunctor *parser4 = nullptr, ParserFunctor *parser5 = nullptr,
				ParserFunctor *parser6 = nullptr, ParserFunctor *parser7 = nullptr, ParserFunctor *parser8 = nullptr,
				ParserFunctor *parser9 = nullptr, ParserFunctor *parser10 = nullptr, ParserFunctor *parser11 = nullptr,
				ParserFunctor *parser12 = nullptr, ParserFunctor *parser13 = nullptr, ParserFunctor *parser14 = nullptr,
				ParserFunctor *parser15 = nullptr, ParserFunctor *parser16 = nullptr, ParserFunctor *parser17 = nullptr,
				ParserFunctor *parser18 = nullptr, ParserFunctor *parser19 = nullptr, ParserFunctor *parser20 = nullptr,
				ParserFunctor *parser21 = nullptr, ParserFunctor *parser22 = nullptr, ParserFunctor *parser23 = nullptr);
			ParserFunctor *p_or(std::vector<ParserFunctor*> const parsers);
			// main_parser - parser1
			ParserFunctor *subtract(ParserFunctor *main_parser, ParserFunctor *parser1);
			ParserFunctor *subtract(std::vector<ParserFunctor*> const parsers);
			ParserFunctor *repeatStar(ParserFunctor *parser);
			ParserFunctor *repeatPlus(ParserFunctor *parser);
			ParserFunctor *repeatCount(ParserFunctor *parser, size_t n);
			ParserFunctor *optional_parser(ParserFunctor *parser);
			ParserFunctor *failure_parser() const;
			ParserFunctor *error_parser() const;
			ParserFunctor *empty_parser() const;
			ParserFunctor *lua_symbol_parser() const;
			ParserFunctor *literal(std::string &token_str);
			ParserFunctor *token_type_parser(int type);
			ParserFunctor *string_parser() const;
			ParserFunctor *number_parser() const;
			void finish();

			ParserFunctor *cache(std::string name, ParserFunctor *parser);

			ParserFunctor *get_cached(std::string name);

			ParserFunctor *named_parser(std::string name);

			inline FinalMatchResult *create_final_match_result(Input &input, ParserFunctor *parser)
			{
				FinalMatchResult *r = new FinalMatchResult(input);
				r->set_parser(parser);
				add_match_result(r);
				return r;
			}
			inline ComplexMatchResult *create_complex_match_result(const Input &input, ParserFunctor *parser)
			{
				ComplexMatchResult *r = new ComplexMatchResult(const_cast<Input&>(input));
				r->set_parser(parser);
				add_match_result(r);
				return r;
			}
			inline void set_error(std::string error_str, bool force_replace = false)
			{
				if (_error.length() < 1 || force_replace)
					_error = error_str;
			}
			inline std::string get_error() const
			{
				return _error;
			}
			inline void clear_caches()
			{
				_parsers_min_length_cache.clear();
				_parsers_parse_result_cache.clear();
			}
			inline void set_parser_min_length(ParserFunctor *parser, size_t min_length)
			{
				_parsers_min_length_cache.erase(parser);
				_parsers_min_length_cache[parser] = min_length;
			}
			inline int get_parser_min_length_cached(ParserFunctor *parser)
			{
				auto found = _parsers_min_length_cache.find(parser);
				if (found != _parsers_min_length_cache.end())
					return (int)found->second;
				else
					return -1;
			}
			size_t parser_min_length(ParserFunctor *parser);

			ParseResult parse_with_cache(ParserFunctor *parser, Input &input, size_t skip_end_size, bool optional);

			ParserFunctor *generate_glua_parser();

			ParseResult parse_syntax(Input &input);

		};

		class ParserContextParsingLevelScope
		{
		private:
			ParserContext *_ctx;
		public:
			inline ParserContextParsingLevelScope(ParserContext *ctx)
				: _ctx(ctx)
			{
				_ctx->inc_parsing_level();
			}

			inline ~ParserContextParsingLevelScope()
			{
				_ctx->dec_parsing_level();
			}
		};

		class ParserBuilder
		{
		private:
			ParserContext *_ctx;
			ParserFunctor *_result;
		public:
			inline ParserBuilder(ParserContext *ctx, ParserFunctor *initial)
				: _ctx(ctx), _result(initial) {}
			ParserBuilder *lua_and(ParserFunctor *other1, ParserFunctor *other2 = nullptr,
				ParserFunctor *other3 = nullptr, ParserFunctor *other4 = nullptr);
			ParserBuilder * lua_or (ParserFunctor *other1, ParserFunctor *other2 = nullptr,
				ParserFunctor *other3 = nullptr, ParserFunctor *other4 = nullptr);
			inline ParserFunctor *result() const
			{
				return _result;
			}
		};

		class FailureParserFunctor : public ParserFunctor
		{
			// always FAILURE parser functor
		private:
			ParserContext *_ctx;
		public:
			inline FailureParserFunctor(ParserContext *ctx) : _ctx(ctx){}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);

			inline virtual std::vector<std::string> str_lines() const
			{
				return std::vector<std::string>();
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				return 0;
			}
		};

		class ErrorParserFunctor : public ParserFunctor
		{
			// always ERROR parser functor
		private:
			ParserContext *_ctx;
		public:
			inline ErrorParserFunctor(ParserContext *ctx) : _ctx(ctx) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);

			inline virtual std::vector<std::string> str_lines() const
			{
				return std::vector<std::string>();
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				return 0;
			}
		};

		class EmptyParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
		public:
			inline EmptyParserFunctor(ParserContext *ctx) : _ctx(ctx){}
			inline virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional) {
				ComplexMatchResult *r = _ctx->create_complex_match_result(input, this);
				ParseResult pr(SUCCESS, r);
				return pr;
			}
			inline virtual size_t  min_length() const { return 0; }
			/*inline virtual std::string str() const
			{
				return "";
			}*/
			inline virtual std::vector<std::string> str_lines() const
			{
				return std::vector<std::string>();
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				return 0;
			}
			inline virtual bool is_fixed_length() const
			{
				return true;
			}
			inline virtual bool is_empty_parser() const
			{
				return true;
			}
		};

		class ConcatParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			std::vector<ParserFunctor*> _parsers;
		public:
			ConcatParserFunctor(ParserContext *ctx, std::vector<ParserFunctor*> &parsers);
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			inline virtual size_t size() const {
				return _parsers.size();
			}
			virtual ParserFunctor *head();
			virtual std::vector<ParserFunctor*> tail();
			inline virtual bool is_concat() const {
				return true;
			}
			inline const std::vector<ParserFunctor*> &parsers() const
			{
				return _parsers;
			}
			virtual size_t  min_length() const;
			// virtual std::string str() const;
			virtual std::vector<std::string> str_lines() const;

			inline virtual bool is_fixed_length() const
			{
				for(const auto &p : _parsers)
				{
					if (!p->is_fixed_length())
						return false;
				}
				return true;
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (size() < 1 || tokens.size()<1)
					return 0;
				// TODO: 如果能确认某次take失败(确认concat没有结束)，直接返回0
				// 能确认take失败的情况：前面几项都是单token parse
				auto remaining_tokens_count = tokens.size();
				bool end_fixed_length_token_parser = false; // 是否结束了从头开始连续的定长parser
				for (const auto &parser : _parsers)
				{
					if (!parser->is_fixed_length() && !end_fixed_length_token_parser)
						end_fixed_length_token_parser = true;
					std::vector<GluaParserToken> remaining_tokens(tokens.begin() + (tokens.size() - remaining_tokens_count), tokens.end());
					auto parser_result = parser->can_take_first_token(remaining_tokens);
					if (parser_result >= remaining_tokens_count)
						return (int) tokens.size();
					if (parser_result < 1)
					{
						if (!end_fixed_length_token_parser)
							return 0;
						return (int) (tokens.size() - remaining_tokens_count);
					}
					remaining_tokens_count -= parser_result;
				}
				return (int) (tokens.size() - remaining_tokens_count);
				//if (_parsers.size() < 2)
				//	return 1;
				//return 1 + _parsers[1]->can_take_first_token({ tokens[1] });
			}
		};

		// TODO: test
		// 减法的parser functor, a - b - c表示满足parser functor a但是不满足b且不满足c
		class SubtractionParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			ParserFunctor *_main_parser;
			std::vector<ParserFunctor*> _subtrac_parsers;
		public:
			inline SubtractionParserFunctor(ParserContext *ctx, ParserFunctor *_parser, std::vector<ParserFunctor*> subtrac_parsers={})
				: _ctx(ctx), _main_parser(_parser)
			{
				for(const auto & parser:  subtrac_parsers)
				{
					_subtrac_parsers.push_back(parser);
				}
			}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			inline virtual size_t  min_length() const { return _main_parser->min_length(); }
			// virtual std::string str() const;

			virtual std::vector<std::string> str_lines() const;

			inline virtual bool is_fixed_length() const
			{
				return _main_parser->is_fixed_length();
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (!_main_parser->can_take_first_token(tokens))
					return 0;
				for(const auto &p : _subtrac_parsers)
				{
					if (p->can_take_first_token(tokens)<tokens.size())
						return 0;
				}
				return static_cast<int>(tokens.size());
			}
		};

		class RepeatStarParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			ParserFunctor *_parser;
		public:
			inline RepeatStarParserFunctor(ParserContext *ctx, ParserFunctor *_parser)
				: _ctx(ctx), _parser(_parser) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			inline virtual size_t  min_length() const { return 0; }
			//inline virtual std::string str() const
			//{
			//	return _parser->str() + "*";
			//}

			virtual std::vector<std::string> str_lines() const;

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if(tokens.size()==1)
					return _parser->can_take_first_token(tokens);
				auto first_parser_result = _parser->can_take_first_token(tokens);
				if (first_parser_result > 0)
					return static_cast<int>(tokens.size());
				else
					return 0;
			}
		};

		class RepeatCountParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			ParserFunctor* _parser;
			size_t _num;
		public:
			inline RepeatCountParserFunctor(ParserContext *ctx, ParserFunctor* _parser, size_t num)
				: _ctx(ctx), _parser(_parser), _num(num) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			inline virtual size_t  min_length() const { return _num * _ctx->parser_min_length(_parser); }
			//inline virtual std::string str() const
			//{
			//	return _parser->str() + "{" + std::to_string(_num) + "}";
			//}

			virtual std::vector<std::string> str_lines() const;

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (_num < 1)
					return 0;
				if (tokens.size() < 1)
					return 0;
				if (tokens.size() == 1)
					return (int)(_num * _parser->can_take_first_token(tokens));
				auto first_parser_result = _parser->can_take_first_token(tokens);
				if (first_parser_result > 0)
					return static_cast<int>(tokens.size());
				else
					return 0;
			}

			inline virtual bool is_fixed_length() const
			{
				return _parser->is_fixed_length();
			}
		};

		class UnionParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			std::vector<ParserFunctor*> _parsers;
			std::unordered_map<std::string, std::shared_ptr<std::vector<ParserFunctor*>>> _parsers_started_with_final;
			std::vector<ParserFunctor*> _parsers_not_started_with_final;
			bool _lookahead_optimized;
		public:
			UnionParserFunctor(ParserContext *ctx, std::vector<ParserFunctor*> &parsers);
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			ParseResult try_parse_with_lookahead_optimized(Input &input, bool optional);
			virtual void finish();
			inline virtual bool is_union() const {
				return true;
			}
			inline void set_parsers(std::vector<ParserFunctor*> parsers)
			{
				_parsers = parsers;
			}

			inline const std::vector<ParserFunctor*> &parsers() const
			{
				return _parsers;
			}

			// 找到parser在union parser中_parsers中的索引，没找到返回负数(-1)
			inline int find_index_of_parser(ParserFunctor *parser) const
			{
				for(size_t i=0;i<_parsers.size();++i)
				{
					if (_parsers[i] == parser)
						return (int)i;
				}
				return -1;
			}

			/************************************************************************/
			/* parse direct left recur to rule with no direct left recur
				@return whether has direct left-recur sub rule
				*/
			/************************************************************************/
			bool remove_direct_left_recur_rule(std::vector<ParserFunctor*> &result_parsers);
			virtual size_t  min_length() const;
			//virtual std::string str() const;
			virtual std::vector<std::string> str_lines() const;
			virtual ParserFunctor *lookahead();

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				size_t max_accept_tokens_count = 0;
				for(const auto &p : _parsers)
				{
					auto res = p->can_take_first_token(tokens);
					if (res > max_accept_tokens_count)
						max_accept_tokens_count = res;
				}
				return static_cast<int>(max_accept_tokens_count);
			}

			inline virtual bool is_fixed_length() const
			{
				int min_length_last = -1;
				for(const auto &p : _parsers)
				{
					// 这里为了避免循环递归调用is_fixed_length(循环引用情况下)，暂时只考虑都是literal的情况
					if (!p->is_literal())
						return false;
				}
				return true;
			}

		};

		class LiteralParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			std::string _token_str;
		public:
			inline LiteralParserFunctor(ParserContext *ctx, std::string token_str)
				: _ctx(ctx), _token_str(token_str) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			/*inline virtual std::string str() const
			{
				return _token_str;
			}*/
			inline virtual std::vector<std::string> str_lines() const
			{
				return{ _token_str };
			}
			inline virtual bool is_literal() const
			{
				return true;
			}
			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (tokens.size() < 1)
					return 0;
				if (_token_str == tokens[0].token)
					return 1;
				else
					return 0;
			}

			inline virtual bool is_fixed_length() const
			{
				return true;
			}
		};

		class NumberParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
		public:
			inline NumberParserFunctor(ParserContext *ctx) : _ctx(ctx) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			/*inline virtual std::string str() const
			{
				return "<number>";
			}*/
			inline virtual std::vector<std::string> str_lines() const
			{
				return{ "<number>" };
			}
			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (tokens.size() < 1)
					return 0;
				return (tokens[0].type == TOKEN_RESERVED::LTK_FLT || tokens[0].type == TOKEN_RESERVED::LTK_INT) ? 1 : 0;
			}

			inline virtual bool is_fixed_length() const
			{
				return true;
			}
		};

		class StringParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
		public:
			inline StringParserFunctor(ParserContext *ctx) : _ctx(ctx) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			/*inline virtual std::string str() const
			{
				return "<str>";
			}*/
			inline virtual std::vector<std::string> str_lines() const
			{
				return{ "<str>" };
			}
			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (tokens.size() < 1)
					return 0;
				return tokens[0].type == TOKEN_RESERVED::LTK_STRING ? 1 : 0;
			}

			inline virtual bool is_fixed_length() const
			{
				return true;
			}
		};

		class LuaSymbolParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
		public:
			inline LuaSymbolParserFunctor(ParserContext *ctx) : _ctx(ctx) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			/*inline virtual std::string str() const
			{
				return "<symbol>";
			}*/

			inline virtual std::vector<std::string> str_lines() const
			{
				return{ "<symbol>" };
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (tokens.size() < 1)
					return 0;
				return tokens[0].type == TOKEN_RESERVED::LTK_NAME ? 1 : 0;
			}

			inline virtual bool is_fixed_length() const
			{
				return true;
			}
		};

		class CachedParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			std::string _name;
		public:
			inline CachedParserFunctor(ParserContext *ctx, std::string &name)
				: _ctx(ctx), _name(name) {}
			inline virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional)
			{
				auto *pf = _ctx->get_cached(_name);
				if (nullptr != pf && !pf->is_cached())
					return pf->tryParse(input, skip_end_size, optional);
				else
					return _ctx->failure_parser()->tryParse(input, 0, optional);
			}
			inline virtual bool is_cached() const {
				return true;
			}
			inline std::string name() const {
				return _name;
			}
			inline bool exist() const {
				return nullptr != _ctx->get_cached(name());
			}
			inline virtual bool is_concat() const {
				if (!exist())
					return false;
				auto cont = _ctx->get_cached(name());
				if (cont->is_cached())
					return false;
				return cont->is_concat();
			}
			inline virtual size_t  min_length() const
			{
				if (!exist())
					return 0;
				auto cont = _ctx->get_cached(name());
				if (cont->is_cached())
					return 0;
				return cont->min_length();
			}

			inline virtual std::vector<std::string> str_lines() const
			{
				return{ (std::string("<") + _name + ">") };
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (!exist())
					return 0;
				auto cont = _ctx->get_cached(name());
				if (cont->is_cached())
					return 0;
				return cont->can_take_first_token(tokens);
			}

			inline virtual bool is_fixed_length() const
			{
				if (!exist())
					return false;
				auto cont = _ctx->get_cached(name());
				if (cont->is_cached())
					return false;
				return cont->is_fixed_length();
			}
		};

		class LuaTokenTypeParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
			int _type;
		public:
			inline LuaTokenTypeParserFunctor(ParserContext *ctx, int type) : _ctx(ctx), _type(type) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);
			inline virtual std::vector<std::string> str_lines() const
			{
				return{ (std::string("<") + std::to_string(_type) + ">") };
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (tokens.size() < 1)
					return 0;
				return ((int)tokens[0].type) == _type ? 1 : 0;
			}
			inline virtual bool is_fixed_length() const
			{
				return true;
			}
		};

		/**
		 * type a[<...>] = { ... }
		 * type c[<...>] = b[<...>]
		 */
		class GluaTypeParserFunctor : public ParserFunctor
		{
		private:
			ParserContext *_ctx;
		public:
			inline GluaTypeParserFunctor(ParserContext *ctx) : _ctx(ctx) {}
			virtual ParseResult tryParse(Input &input, size_t skip_end_size, bool optional);

			inline virtual std::vector<std::string> str_lines() const
			{
				return{ (std::string("<") + "type statement" + ">") };
			}

			inline virtual int can_take_first_token(std::vector<GluaParserToken> tokens)
			{
				if (tokens.size() < 1)
					return 0;
				return tokens[0].type == TOKEN_RESERVED::LTK_TYPE;
			}
			inline virtual bool is_fixed_length() const
			{
				return false;
			}
		};

		// TODO: range, left-recursive remove, etc.

    } // end glua::parser
}

#endif