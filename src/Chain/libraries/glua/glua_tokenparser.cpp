#include <glua/glua_tokenparser.h>
#include <sstream>
#include <glua/glua_lutil.h>
#include <glua/exceptions.h>

using thinkyoung::lua::api::global_glua_chain_api;

namespace glua
{
    namespace parser
    {

        static std::map<std::string, enum TOKEN_RESERVED> symbol_name_token_type_mapping
        {
            { "if", LTK_IF },
            { "else", LTK_ELSE },
            { "elseif", LTK_ELSEIF },
            { "while", LTK_WHILE },
            { "until", LTK_UNTIL },
            { "repeat", LTK_REPEAT },
            { "end", LTK_END },
            { "function", LTK_FUNCTION },
            { "true", LTK_TRUE },
            { "false", LTK_FALSE },
            { "then", LTK_THEN },
            { "return", LTK_RETURN },
            { "and", LTK_AND },
            { "or", LTK_OR },
            { "not", LTK_NOT },
            { "nil", LTK_NIL },
            { "local", LTK_LOCAL },
			{ "let", LTK_LET },
            { "offline", LTK_OFFLINE },
			{ "default", LTK_DEFAULT },
			{ "type", LTK_TYPE },
            { "in", LTK_IN },
            { "goto", LTK_GOTO },
            { "for", LTK_FOR },
            { "do", LTK_DO },
            { "break", LTK_BREAK }
        };

        static enum TOKEN_RESERVED get_token_type_by_symbol_name(std::string token)
        {
            auto found = symbol_name_token_type_mapping.find(token);
            if (found != symbol_name_token_type_mapping.end())
                return found->second;
            else
                return LTK_NAME;
        }

        GluaTokenParser::GluaTokenParser(lua_State *L)
        {
            _L = L;
            _pos = 0;
            _parsing_linenumber = 1;
            _parsing_pos = 0;
            _cycle_count = 0;
            _parsing_buff = new int[LUA_TOKEN_MAX_LENGTH];
            _reset_parse_buff();
        }

        GluaTokenParser::~GluaTokenParser()
        {
            delete[] _parsing_buff;
        }

        void GluaTokenParser::_reset_parse_buff()
        {
            memset(_parsing_buff, 0x0, sizeof(_parsing_buff));
            _parsing_buff_len = 0;
        }

        int GluaTokenParser::_save_to_buff(int c)
        {
            if (_parsing_buff_len >= LUA_TOKEN_MAX_LENGTH - 1)
                return c;
            _parsing_buff[_parsing_buff_len++] = c;
            return c;
        }

        int GluaTokenParser::_save_to_buff_and_next(int c)
        {
            if (_parsing_buff_len >= LUA_TOKEN_MAX_LENGTH - 1)
                return c;
            _parsing_buff[_parsing_buff_len++] = c;
            ++_parsing_pos;
            return c;
        }

        int GluaTokenParser::_skip_sep(std::string &code)
        {
            int count = 0;
            int s = code[_parsing_pos];
            lua_assert(s == '[' || s == ']');
            _save_to_buff_and_next(s);
            while (_parsing_pos < code.length() && code[_parsing_pos] == '=') {
                _save_to_buff_and_next(code[_parsing_pos]);
                count++;
            }
            return (_parsing_pos < code.length() && code[_parsing_pos] == s) ? count : (-count) - 1;
        }

        static bool is_new_line_start(int c)
        {
            return  c == '\n' || c == '\r';
        }

        static inline bool is_base16_digit(int c)
        {
            return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
        }

        static inline bool is_digit(int c)
        {
            return '0' <= c && c <= '9';
        }

        static inline bool is_alpha(int c)
        {
            return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
        }

        static inline bool is_big_alpha(int c)
        {
            return 'A' <= c && c <= 'Z';
        }

        static inline bool is_little_alpha(int c)
        {
            return 'a' <= c && c <= 'z';
        }

        static inline bool is_space(int c)
        {
            return c == '\n' || c == '\r' || c == ' ' || c == '\f' || c == '\t' || c == '\v';
        }

        int GluaTokenParser::_current_char(std::string &code)
        {
            return _parsing_pos < code.length() ? code[_parsing_pos] : EOF_TOKEN_CHAR;
        }

        void GluaTokenParser::_inc_line(std::string &code)
        {
            if (_parsing_pos >= code.length())
                return;
            int cur = code[_parsing_pos];
            bool iswinnewline = _parsing_pos < (code.length() - 1)
                && ((cur == '\n' && code[_parsing_pos + 1] == '\r')
                || (cur == '\r' && code[_parsing_pos + 1] == '\n'));
            if (iswinnewline)
            {
                ++_parsing_linenumber;
                _parsing_pos += 2;
            }
            else
            {
                ++_parsing_linenumber;
                _parsing_pos += 1;
            }
        }

        void GluaTokenParser::_push_new_token(int type, size_t line)
        {
            _parsing_buff[_parsing_buff_len] = '\0';
            char buff[LUA_TOKEN_MAX_LENGTH];
            for (size_t i = 0; i < _parsing_buff_len; ++i)
            {
                buff[i] = (char)_parsing_buff[i];
            }
            buff[_parsing_buff_len] = '\0';
            std::string token_str(buff);
            GluaParserToken token;
            token.token = token_str;
            auto enum_type = static_cast<enum TOKEN_RESERVED>(type);
            if (enum_type == LTK_NAME)
                enum_type = get_token_type_by_symbol_name(token_str);
            token.type = enum_type;
            token.linenumber = line > 0 ? line : _parsing_linenumber;
            token.position = _parsing_pos - token_str.length();
			token.end_linenumber = token.linenumber + glua::util::string_lines_count(token_str) - 1;
            _tokens.push_back(token);
            _reset_parse_buff();
        }

        GluaParserToken GluaTokenParser::_last_token() const
        {
            return *(std::next(_tokens.begin(), _tokens.size() - 1));
        }

        GluaParserToken GluaTokenParser::_single_char_token(std::string &code, int type)
        {
            _save_to_buff_and_next(_current_char(code));
            _push_new_token(type);
            return _last_token();
        }

        GluaParserToken GluaTokenParser::_double_chars_token(std::string &code, int type)
        {
            _save_to_buff_and_next(_current_char(code));
            _save_to_buff_and_next(_current_char(code));
            _push_new_token(type);
            return _last_token();
        }

        GluaParserToken GluaTokenParser::_three_chars_token(std::string &code, int type)
        {
            _save_to_buff_and_next(_current_char(code));
            _save_to_buff_and_next(_current_char(code));
            _save_to_buff_and_next(_current_char(code));
            _push_new_token(type);
            return _last_token();
        }

        int GluaTokenParser::_next_token_char(std::string &code) const
        {
            return (_parsing_pos + 1 < code.length()) ? code[_parsing_pos + 1] : EOF_TOKEN_CHAR;
        }

        static inline int hexchar_to_base10(int c)
        {
            return is_digit(c) ? (c - '0') : (is_big_alpha(c) ? (c - 'A' + 10) : (c - 'a' + 10));
        }

        void GluaTokenParser::_loadhexaesc(std::string &code, std::stringstream *ss)
        {
            --_parsing_buff_len;
            int c1 = _current_char(code);
            ss && *ss << (char)c1;
            ++_parsing_pos;
            int c2 = _current_char(code);
            ss && *ss << (char)c2;
            ++_parsing_pos;
            if (!is_base16_digit(c1) || !is_base16_digit(c2))
            {
				THROW_PERROR("hex wrong format");
                return;
            }
            int r1 = hexchar_to_base10(c1);
            int r2 = hexchar_to_base10(c2);
            int d = (r1 << 4) + r2;
            char s[4];
            snprintf(s, 4, "%d", d);
            for (size_t i = 0; i < strlen(s); ++i)
            {
                _save_to_buff(s[i]);
            }
        }

        void GluaTokenParser::_loadutf8esc(std::string &code, std::stringstream *ss)
        {
            // char buff[8];
            unsigned long r;
            int i = 4;  // chars to be removed: '\', 'u', '{', and first digit 
            --_parsing_buff_len;
            if (_current_char(code) != '{')
            {
                THROW_PERROR("in utf8 literal, need '{' after '\\u'");
                return;
            }
            ++_parsing_pos;
            r = hexchar_to_base10(_current_char(code));  // must have at least one digit 
            while (is_base16_digit(_current_char(code)) && (!ss || *ss << (char)_current_char(code)) && _save_to_buff_and_next(hexchar_to_base10(_current_char(code)))) {
                i++;
                r = (r << 4) + hexchar_to_base10(_current_char(code));
                if (r > 0x10FFFF)
                {
                    THROW_PERROR("UTF-8 value too large");
                    return;
                }
            }
            if (_current_char(code) != '}')
            {
                THROW_PERROR("missing '}' in UTF-8 value");
                return;
            }
            ++_parsing_pos;	// skip }
        }

        void GluaTokenParser::_loaddecesc(std::string &code, std::stringstream *ss)
        {
            int i;
            int r = 0; // result accumulator
            for (i = 0; i < 3 && is_digit(_current_char(code)); ++i) // read up to 3 digits
            {
                ss && *ss << (char)_current_char(code);
                r = 10 * r + _current_char(code) - '0';
                _save_to_buff_and_next(_current_char(code) - '0');
            }
            if (r > 0xff)
            {
                THROW_PERROR("decimal escape too large");
                return;
            }
        }

        bool GluaTokenParser::_check_next2(std::string &code, const char *str)
        {
            ++_parsing_pos;
            int n1 = _current_char(code);
            ++_parsing_pos;
            int n2 = _current_char(code);
            _parsing_pos = _parsing_pos - 2;
            return n1 == str[0] && n2 == str[1];
        }

        void GluaTokenParser::_read_numeral(std::string &code, std::stringstream *ss)
        {
            const char *expo = "Ee";
            int first = _current_char(code);
            ss && *ss << (char)first;
            lua_assert(lisdigit(first));
            _save_to_buff_and_next(first);
            if (first == '0' && _check_next2(code, "xX"))  // hexadecimal?
                expo = "Pp";
            bool is_float = false;
            bool dotexist = false;
            for (;;) {
                ss && *ss << (char)_current_char(code);
                if (_check_next2(code, expo))  // exponent part?
                    _check_next2(code, "-+");  // optional exponent sign 
                if (is_base16_digit(_current_char(code)))
                {
                    _save_to_buff_and_next(_current_char(code));
                    ss && *ss << (char)_current_char(code);
                }
                else if (_current_char(code) == '.')
                {
                    is_float = true;
                    _save_to_buff_and_next(_current_char(code));
                    if (dotexist)
                    {
                        THROW_PERROR("number format error");
                    }
                    dotexist = true;
                    ss && *ss << (char)_current_char(code);
                }
                else break;
            }
            _save_to_buff('\0');
            // TODO: check number format
            if (!is_float) {
                return _push_new_token(LTK_INT);
            }
            else {
                return _push_new_token(LTK_FLT);
            }
        }

        void GluaTokenParser::_read_string(std::string &code, int delimiter)
        {
            std::stringstream literal_str;
            literal_str << (char)_current_char(code);
            _save_to_buff_and_next(_current_char(code));
            while (_current_char(code) != delimiter) {
                if (_current_char(code) != '\\' || _next_token_char(code) != 'z')
                    literal_str << (char)_current_char(code);
                switch (_current_char(code)) {
                case EOF_TOKEN_CHAR:
					THROW_PERROR("unfinished string");
                    return;
                case '\n':
                case '\r':
                    THROW_PERROR("unfinished string");
                    return;
                case '\\': {  /* escape sequences */
                    int c;  /* final character to be saved */
                    _save_to_buff_and_next(_current_char(code));  /* keep '\\' for error messages */
                    switch (_current_char(code)) {
                    case 'a': c = '\a'; goto read_save;
                    case 'b': c = '\b'; goto read_save;
                    case 'f': c = '\f'; goto read_save;
                    case 'n': c = '\n'; goto read_save;
                    case 'r': c = '\r'; goto read_save;
                    case 't': c = '\t'; goto read_save;
                    case 'v': c = '\v'; goto read_save;
                    case 'x': literal_str << 'x'; ++_parsing_pos; _loadhexaesc(code, &literal_str); goto no_save;
                    case 'u': literal_str << "u{"; ++_parsing_pos; _loadutf8esc(code, &literal_str); literal_str << "}"; goto no_save;
                    case '\n': case '\r':
                    {
                        _inc_line(code); c = '\n'; goto only_save;
                    }
                    case '\\': case '\"': case '\'':
                        c = _current_char(code); goto read_save;
                    case EOZ: goto no_save;  // will raise an error next loop 
                    case 'z': {  // zap following span of spaces 
                        --_parsing_buff_len; // remove '\\'
                        ++_parsing_pos;
                        while (is_space(_current_char(code))) {
                            if (is_new_line_start(_current_char(code)))
                                _inc_line(code);
                            else
                                ++_parsing_pos;
                        }
                        goto no_save;
                    }

                    default: {
                        if (!is_digit(_current_char(code)))
                        {
                            THROW_PERROR("invalid escape sequence");
                            ++_parsing_pos;
                            return;
                        }
                        _loaddecesc(code, &literal_str);  // digital escape '\ddd'
                        goto no_save;
                    }
                    }
                read_save:
                    literal_str << (char)_current_char(code);
                    _save_to_buff_and_next(_current_char(code));
                    /* go through */
                only_save:
                    if (_parsing_buff_len > 0)
                        --_parsing_buff_len;
                    _save_to_buff(c);
                    // go through
                no_save: break;
                }
                default:
                    _save_to_buff_and_next(_current_char(code));
                }
            }
            literal_str << (char)_current_char(code);
            _save_to_buff_and_next(_current_char(code));  // skip delimiter
            _push_new_token(LTK_STRING);
            auto token = _last_token();
            token.token = token.token.substr(1, token.token.length() - 2);
            token.source_token = literal_str.str();
            _tokens.pop_back();
            _tokens.push_back(token);
        }

#define MAX_TOKEN_PARSER_CYCLES_COUNT 10086
        static bool end_cycle(size_t cycle_count)
        {
            return cycle_count > MAX_TOKEN_PARSER_CYCLES_COUNT;
        }

        void GluaTokenParser::_read_long_string(std::string &code, bool is_string, int sep)
        {
            ++_cycle_count;
            if (end_cycle(_cycle_count))
                return;
            auto line = _parsing_linenumber; // initial linenumber, the string may over-lines
            auto what = is_string ? "string" : "comment";
            _save_to_buff_and_next(_current_char(code));
            if (_parsing_pos < code.length() && is_new_line_start(_current_char(code)))
            {
                _inc_line(code);
            }
            while (_parsing_pos < code.length())
            {
                if (_parsing_buff_len >= LUA_TOKEN_MAX_LENGTH - 1)
                {
					if(!is_string)
					{
						// is too long comment
						_parsing_buff_len = 0;
					}
					else 
					{
                        global_glua_chain_api->throw_exception(_L, THINKYOUNG_API_COMPILE_ERROR, std::string("too long token").c_str());
						return;
					}
                }
                switch (_current_char(code))
                {
                case EOF_TOKEN_CHAR:
                    global_glua_chain_api->throw_exception(_L, THINKYOUNG_API_COMPILE_ERROR, "undefined long %s (starting at line %d)", what, line);
                    return;
                case ']':
                {
                    if (!is_string && code.length() > _parsing_pos + 1 && code[_parsing_pos + 1] == ']'
						)
                    {
                        _save_to_buff_and_next(_current_char(code));
                        _save_to_buff_and_next(_current_char(code));
                        goto endloop;
                    }
                    if (_skip_sep(code) == sep)
                    {
                        _save_to_buff_and_next(_current_char(code));
                        goto endloop;
                    }
                    else if (!is_string)
                    {
                        ++_parsing_pos;	// ignore any thing in comment
                    }
                    else
                    {
                        _save_to_buff_and_next(_current_char(code));  // ?
                    }
                    break;
                }
                case '\n': case '\r':
                    _save_to_buff(_current_char(code));
                    _inc_line(code);
                    break;
                default:
                    _save_to_buff_and_next(_current_char(code));
                }
            }
        endloop:
            if (is_string)
                _push_new_token(LTK_STRING, line);
            else
                _reset_parse_buff();
        }

        GluaParserToken GluaTokenParser::_parse_next_token(std::string &code)
        {
            ++_cycle_count;
            if (end_cycle(_cycle_count))
                return eof_token();
            while (_parsing_pos < code.length())
            {
                int cur = code[_parsing_pos];
                int sep;
                int nextchar;
                switch (_current_char(code))
                {
                case '\n': case '\r':
                {
                    _inc_line(code);
                }  break;
                case ' ': case '\f': case '\t': case '\v':
                {
                    ++_parsing_pos;
                }  break;
                case '-':
                {
                    ++_parsing_pos;
                    if (_parsing_pos >= code.length() || (_parsing_pos < code.length() && code[_parsing_pos] != '-'))
                    {
                        GluaParserToken token;
                        token.linenumber = _parsing_linenumber;
                        token.position = _parsing_pos - 1;
                        char codestr[2];
                        codestr[0] = (char) cur;
                        codestr[1] = '\0';
                        token.token = std::string(codestr);
						token.end_linenumber = token.linenumber + glua::util::string_lines_count(token.token) - 1;
                        token.type = (TOKEN_RESERVED) cur;
                        _tokens.push_back(token);
                        return token;
                    }
                    // else is a comment
                    ++_parsing_pos;
                    if (_parsing_pos < code.length() && code[_parsing_pos] == '[')
                    {
                        // long comment
                        int sep = _skip_sep(code);
                        if (sep >= 0)
                        {
                            _read_long_string(code, false, sep);
                            _reset_parse_buff();
                            break;
                        }
                    }
                    // short comment
                    while (!is_new_line_start(_current_char(code)) && _current_char(code) != EOF_TOKEN_CHAR)
                        ++_parsing_pos;
                }  break;
                case '[':
                    // long string 
                    sep = _skip_sep(code);
                    if (sep >= 0)
                    {
                        _read_long_string(code, true, sep);
                        return _last_token();
                    }
                    else if (sep != -1)
                    {
                        THROW_PERROR("invalid long string delimiter");
                        return eof_token();
                    }
                    else
                    {
                        // _save_to_buff(_current_char(code));
                        --_parsing_pos;
                        _push_new_token(_current_char(code));
                        ++_parsing_pos;
                        return _last_token();
                    }
                    break;
				case '?':
					if(_parsing_pos + 1 < code.length() && code[_parsing_pos+1]==':')
					{
						// ?:
						_save_to_buff_and_next(_current_char(code));
						_save_to_buff_and_next(_current_char(code));
						_push_new_token(LTK_OPTIONAL_COLON);
						return _last_token();
					}
					else {
						// ?
						_save_to_buff_and_next(_current_char(code));
						_push_new_token('?');
						return _last_token();
					}
                case '=':
                    if (_parsing_pos + 1 < code.length() && code[_parsing_pos + 1] == '=')
                    {
                        // ==
                        _save_to_buff_and_next(_current_char(code));
                        _save_to_buff_and_next(_current_char(code));
                        _push_new_token(LTK_EQ);
                        return _last_token();
                    }
                    else if (_parsing_pos + 1 < code.length() && code[_parsing_pos + 1] == '>')
                    {
                        // =>
                        _save_to_buff_and_next(_current_char(code));
                        _save_to_buff_and_next(_current_char(code));
                        _push_new_token(LTK_FUNCTION_ARROW);
                        return _last_token();
                    }
                    else
                    {
                        // =
                        _save_to_buff_and_next(_current_char(code));
                        _push_new_token('=');
                        return _last_token();
                    }
                    break;
                case '<':
                    nextchar = _next_token_char(code);
                    if (nextchar == '=')
                    {
                        return _double_chars_token(code, LTK_LE);
                    }
                    else if (nextchar == '<')
                    {
                        return _double_chars_token(code, LTK_SHL);
                    }
                    else
                    {
                        return _single_char_token(code, '<');
                    }
                    break;
                case '>':
                    nextchar = _next_token_char(code);
                    if (nextchar == '=')
                    {
                        return _double_chars_token(code, LTK_GE);
                    }
                    else if (nextchar == '>')
                    {
                        return _double_chars_token(code, LTK_SHR);
                    }
                    else
                    {
                        return _single_char_token(code, '>');
                    }
                    break;
                case '/':
                    nextchar = _next_token_char(code);
                    if (nextchar == '/')
                    {
                        return _double_chars_token(code, LTK_IDIV);
                    }
                    else
                    {
                        return _single_char_token(code, '/');
                    }
                    break;
                case '~':
                    nextchar = _next_token_char(code);
                    if (nextchar == '=')
                        return _double_chars_token(code, LTK_NE);
                    else
                        return _single_char_token(code, '~');
                    break;
                case ':': {
                    nextchar = _next_token_char(code);
                    if (nextchar == ':')
                        return _double_chars_token(code, LTK_DBCOLON);
                    else
                        return _single_char_token(code, ':');
                }
                case '"': case '\'': {  // short literal strings
                    _read_string(code, _current_char(code));
                    return _last_token();
                }
                case '.': {  // '.', '..', '...', or number
                    int curchar = _current_char(code);
                    nextchar = _next_token_char(code);
                    if (nextchar == '.')
                    {
                        ++_parsing_pos;
                        int nextnextchar = _next_token_char(code);
                        --_parsing_pos;
                        if (nextnextchar == '.')
                            return _three_chars_token(code, LTK_DOTS); // ...
                        else
                            return _double_chars_token(code, LTK_CONCAT); // ..
                    }
                    else if (!is_digit(curchar))
                        return _single_char_token(code, '.');
                    else
                    {
                        _read_numeral(code);
                        return _last_token();
                    }
                }
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9': {
                    _read_numeral(code);
                    return _last_token();
                }
                case EOF_TOKEN_CHAR: {
                    _push_new_token(LTK_EOS);
                    return _last_token();
                }
                default: {
                  int c = _current_char(code);
                  if (c < 0)
                  {
                    // FIXME: 单独出现的特殊字符暂时忽略，之后要忽略不可见字符但是对于中文字符要输出
                    ++_parsing_pos;
                    continue;
                  }
                    if (is_alpha(c) || c == '_') {  // identifier or reserved word?
                        do {
                            _save_to_buff_and_next(_current_char(code));
                        } while (is_alpha(_current_char(code)) || _current_char(code) == '_' || is_digit(_current_char(code)));
                        _push_new_token(LTK_NAME); // TODO: if is reserved word, use other type
                        return _last_token();
                    }
                    else {  // single-char tokens (+ - / ...) 
                        _save_to_buff_and_next(c);
                        _push_new_token(c);
                        return _last_token();
                    }
                }
                }
            }
            return eof_token();
        }

        void GluaTokenParser::parse(const std::string &code)
        {
            _parsing_linenumber = 1;
            _parsing_pos = 0;
            size_t cycle_count = 0;
			while (_parsing_pos < code.length() && cycle_count < 1000000)
			{
				++cycle_count;
				auto token = _parse_next_token(const_cast<std::string&>(code));
				if (is_eof_token(token))
					break;
			}
        }

        void GluaTokenParser::goback()
        {
            if (_pos > 0)
                --_pos;
        }

        bool GluaTokenParser::next()
        {
            if (!hasNext())
                return false;
            ++_pos;
            return true;
        }

        GluaParserToken GluaTokenParser::current() const
        {
            if (eof())
                return eof_token();
            return *std::next(_tokens.begin(), _pos);
        }

        GluaParserToken GluaTokenParser::lookahead() const
        {
            if (!hasNext())
                return eof_token();
            return *std::next(_tokens.begin(), _pos + 1);
        }

        std::string GluaTokenParser::dump() const
        {
			// TODO: 把生成ldf文件的处理放到这里来
            std::stringstream ss;
            size_t lastline = 1;
			size_t lasttoken_lines_count = 0;
            for (auto it = _tokens.begin(); it != _tokens.end(); ++it)
            {
				if (it->linenumber != lastline && it->linenumber > 1) {
					int lines_count_to_insert = static_cast<int>(it->linenumber) - static_cast<int>(lastline) - static_cast<int>(lasttoken_lines_count) + 1;
					if (lines_count_to_insert < 1)
						lines_count_to_insert = 1;
					for(int k =0;k<lines_count_to_insert;++k)
						ss << "\n";
				}
                else if (it->type != '.' && it->type != ':' && it->type != '(' && it->type != ')'
                    && it->type != '[' && it->type != ']')
                    ss << " ";
                lastline = it->linenumber;
                switch (it->type)
                {
                case LTK_STRING:
                {
                    auto lstr = it->token;
                    if (it->source_token.length() > 0)
                        lstr = it->source_token;
                    bool is_long_string = it->source_token.length() < 1;
                    ss << lstr;
					lasttoken_lines_count = glua::util::string_lines_count(lstr);
                }
                break;
                default:
				{
					ss << it->token;
					lasttoken_lines_count = glua::util::string_lines_count(it->token);
				}
                }
            }
            return ss.str();
        }

    }
}