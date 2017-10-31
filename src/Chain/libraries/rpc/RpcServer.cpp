#define DEFAULT_LOGGER "rpc"


#include <fc/exception/exception.hpp>
#include <wallet/Exceptions.hpp>
#include <rpc/Exceptions.hpp>
#include <rpc/RpcServer.hpp>
#include <blockchain/Config.hpp>
#include <blockchain/Time.hpp>


#include <utilities/GitRevision.hpp>
#include <utilities/KeyConversion.hpp>
#include <utilities/PaddingOstream.hpp>
#include <net/StcpSocket.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fc/interprocess/file_mapping.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/crypto/digest.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/git_revision.hpp>
#include <fc/io/sstream.hpp>

#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <iomanip>
#include <limits>
#include <sstream>


#include <fc/exception/exception.hpp>
#include <fc/io/iostream.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>
#include <fc/string.hpp>
//#include <utfcpp/utf8.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/filesystem/fstream.hpp>
#include <rpc_stubs/CommonApiRpcServer.hpp>
#include <boost/filesystem/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/iostream.hpp>
#include <fc/io/buffered_iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/sstream.hpp>
#include <fc/log/logger.hpp>



namespace thinkyoung {
    namespace rpc {

        using namespace client;

        namespace detail
        {
            class RpcServerImpl : public thinkyoung::rpc_stubs::CommonApiRpcServer
            {
            public:
                RpcServerConfig                                 _config;
                thinkyoung::client::Client*                              _client;
                std::shared_ptr<fc::http::server>                 _httpd;
                std::shared_ptr<fc::tcp_server>                   _tcp_serv;
                std::shared_ptr<fc::tcp_server>                   _stcp_serv;
                fc::future<void>                                  _accept_loop_complete;
                fc::future<void>                                  _encrypted_accept_loop_complete;
                RpcServer*                                       _self;
                fc::shared_ptr<fc::promise<void>>                 _on_quit_promise;
                fc::thread*                                       _thread;
                HttpCallbackType                                _http_file_callback;
                std::unordered_set<fc::rpc::json_connection_ptr>  _open_json_connections;
                fc::mutex                                         _rpc_mutex; // locked to prevent executing two rpc calls at once

                bool                                              _cache_enabled = true;
                fc::time_point                                    _last_cache_clear_time;
                std::unordered_map<string, string>                 _call_cache;

                typedef std::map<std::string, thinkyoung::api::MethodData> MethodMapType;
                MethodMapType _method_map;

                /** the map of alias and method name */
                std::map<std::string, std::string>     _alias_map;

                /** the set of connections that have successfully logged in */
                std::unordered_set<fc::rpc::json_connection*> _authenticated_connection_set;

                RpcServerImpl(thinkyoung::client::Client* client) :
                    _client(client),
                    _on_quit_promise(new fc::promise<void>("rpc_quit")),
                    _self(nullptr),
                    _thread(nullptr)
                {}

                void shutdown_rpc_server();

                virtual thinkyoung::api::CommonApi* get_client() const override;
                virtual void verify_json_connection_is_authenticated(fc::rpc::json_connection* json_connection) const override;
                virtual void verify_wallet_is_open() const override;
                virtual void verify_wallet_is_unlocked() const override;
                virtual void verify_sandbox_is_open() const override;
                virtual void verify_connected_to_network() const override;
                virtual void store_method_metadata(const thinkyoung::api::MethodData& method_metadata);

                std::string help(const std::string& command_name) const;

                std::string make_short_description(const thinkyoung::api::MethodData& method_data, bool show_decription = true) const
                {
                    std::string help_string;
                    std::stringstream short_description;
                    short_description << std::setw(100) << std::left;
                    help_string = method_data.name + " ";
                    for (const thinkyoung::api::ParameterData& parameter : method_data.parameters)
                    {
                        if (parameter.classification == thinkyoung::api::required_positional)
                            help_string += std::string("<") + parameter.name + std::string("> ");
                        else if (parameter.classification == thinkyoung::api::optional_named)
                            help_string += std::string("{") + parameter.name + std::string("} ");
                        else if (parameter.classification == thinkyoung::api::required_positional_hidden)
                            continue;
                        else
                            help_string += std::string("[") + parameter.name + std::string("] ");
                    }
                    short_description << help_string;
                    if (show_decription)
                    {
                        short_description << "  " << method_data.description << "\n";
                    }
                    else
                    {
                        short_description << "\n";
                    }
                    help_string = short_description.str();
                    return help_string;
                }

                void add_content_type_header(const fc::string& path, const fc::http::server::response& s) {
                    static map<string, string> mime_types
                    { { "png", "image/png" },
                    { "jpg", "image/jpeg" },
                    { "svg", "image/svg+xml" },
                    { "css", "text/css" },
                    { "html", "text/html" },
                    { "js", "application/javascript" },
                    { "json", "application/json" },
                    { "xml", "application/xml" },
                    { "woff", "application/font-woff" },
                    { "ttf", "pplication/x-font-ttf" }
                    };

                    if (path == "/rpc") {
                        s.add_header("Content-Type", "application/json");
                        s.add_header("Cache-Control", "no-cache, no-store, must-revalidate");
                        s.add_header("Pragma", "no-cache");
                        s.add_header("Expires", "0");
                        return;
                    }

                    auto pos = path.rfind('.');
                    if (pos != std::string::npos) {
                        auto extension = path.substr(pos + 1, std::string::npos);
                        auto mime_type = mime_types.find(extension);
                        if (mime_type != mime_types.end())
                            s.add_header("Content-Type", mime_type->second);
                    }
                }

                void handle_request(const fc::http::request& r, const fc::http::server::response& s)
                {
                    fc::time_point begin_time = fc::time_point::now();
                    // WARNING: logging RPC calls can capture passwords and private keys
                    //fc_ilog( fc::logger::get("rpc"), "Started ${path} ${method} at ${time}", ("path",r.path)("method",r.method)("time",begin_time));
                    // dlog( "${r}", ("r",r.path) );
                    fc::http::reply::status_code status = fc::http::reply::OK;

                    s.add_header("Connection", "close");

                    fc::oexception internal_server_error;
                    bool invalid_request_error = false;

                    try {
                        if (_config.rpc_user.size())
                        {
                            //     dlog( "CHECKING AUTH" );
                            auto auth_value = r.get_header("Authorization");
                            std::string username, password;
                            if (auth_value.size())
                            {
                                auto auth_b64 = auth_value.substr(6);
                                auto userpass = fc::base64_decode(auth_b64);
                                auto split = userpass.find(':');
                                username = userpass.substr(0, split);
                                password = userpass.substr(split + 1);
                            }

                            //    ilog( "\n\n\n  username: ${user}  password ${password}", ("path",r.path)("user",username)("password", password));
                            if (_config.rpc_user != username ||
                                _config.rpc_password != password)
                            {
                                //fc_ilog( fc::logger::get("rpc"), "Unauthorized ${path}, username: ${user}", ("path",r.path)("user",username));
                                // WARNING: logging RPC calls can capture passwords and private keys
                                //      elog( "\n\n\n Unauthorized ${path}, username: ${user}  password ${password}", ("path",r.path)("user",username)("password", password));
                                s.add_header("WWW-Authenticate", "Basic realm=\"thinkyoung wallet\"");
                                std::string message = "Unauthorized";

                                s.set_status(fc::http::reply::NotAuthorized);
                                s.set_length(message.size());
                                s.write(message.c_str(), message.size());
                                //elog( "UNAUTHORIZED ${r} ${cfg}  ${user} : ${provided}", ("r",r.path)("cfg",_config)("provided",password)("user",username)  );
                                return;
                            }
                            else
                            {
                                //wlog( "NOT CHECKING AUTH" );
                            }
                        }
                        //ilog( "rpc_user.size: ${config}", ("config",_config.rpc_user.size() ) );

                        // wlog( "providing data anyway???" );

                        fc::string path = r.path;
                        auto pos = path.find('?');
                        if (pos != std::string::npos) path.resize(pos);

                        pos = path.find("..");
                        FC_ASSERT(pos == std::string::npos);

                        if (path == "/") path = "/index.html";
                        add_content_type_header(path, s);

                        auto filename = _config.htdocs / path.substr(1, std::string::npos);
                        if (r.path == fc::path("/rpc"))
                        {
                            // WARNING: logging RPC calls can capture passwords and private keys
                            //  dlog( "RPC ${r}", ("r",r.path) );
                            status = handle_http_rpc(r, s);
                        }
                        else if (fc::path(r.path).parent_path() == fc::path("/safebot"))
                        {
                            // WARNING: logging RPC calls can capture passwords and private keys
                            //  dlog( "RPC ${r}", ("r",r.path) );
                            status = handle_http_rpc(r, s);
                        }
                        else if (_http_file_callback)
                        {
                            _http_file_callback(path, s);
                        }
                        else if (fc::exists(filename))
                        {
                            FC_ASSERT(!fc::is_directory(filename));
                            uint64_t file_size_64 = fc::file_size(filename);
                            FC_ASSERT(file_size_64 <= std::numeric_limits<size_t>::max());
                            size_t file_size = (size_t)file_size_64;
                            FC_ASSERT(file_size != 0);

                            fc::file_mapping fm(filename.generic_string().c_str(), fc::read_only);
                            fc::mapped_region mr(fm, fc::read_only, 0, file_size);
                            fc_ilog(fc::logger::get("rpc"), "Processing ${path}, size: ${size}", ("path", r.path)("size", file_size));
                            s.set_status(fc::http::reply::OK);
                            s.set_length(file_size);
                            s.write((const char*)mr.get_address(), mr.get_size());
                        }
                        else
                        {
                            fc_ilog(fc::logger::get("rpc"), "Not found ${path} (${file})", ("path", r.path)("file", filename));
                            filename = _config.htdocs / "404.html";
                            FC_ASSERT(!fc::is_directory(filename));
                            uint64_t file_size_64 = fc::file_size(filename);
                            FC_ASSERT(file_size_64 <= std::numeric_limits<size_t>::max());
                            size_t file_size = (size_t)file_size_64;
                            FC_ASSERT(file_size != 0);

                            fc::file_mapping fm(filename.generic_string().c_str(), fc::read_only);
                            fc::mapped_region mr(fm, fc::read_only, 0, file_size);
                            s.set_status(fc::http::reply::NotFound);
                            s.set_length(file_size);
                            s.write((const char*)mr.get_address(), mr.get_size());
                            status = fc::http::reply::NotFound;
                        }
                    }
                    catch (const fc::canceled_exception&)
                    {
                        throw;
                    }
                    catch (const fc::exception& e)
                    {
                        internal_server_error = e;
                    }
                    catch (...)
                    {
                        invalid_request_error = true;
                    }

                    if (internal_server_error)
                    {
                        std::string message = "Internal Server Error\n";
                        message += internal_server_error->to_detail_string();
                        fc_ilog(fc::logger::get("rpc"), "Internal Server Error ${path} - ${msg}", ("path", r.path)("msg", message));
                        elog("Internal Server Error ${path} - ${msg}", ("path", r.path)("msg", message));
                        s.set_length(message.size());
                        s.set_status(fc::http::reply::InternalServerError);
                        s.write(message.c_str(), message.size());
                        elog("${e}", ("e", internal_server_error->to_detail_string()));
                        status = fc::http::reply::InternalServerError;
                    }
                    else if (invalid_request_error)
                    {
                        std::string message = "Invalid RPC Request\n";
                        fc_ilog(fc::logger::get("rpc"), "Invalid RPC Request ${path}", ("path", r.path));
                        elog("Invalid RPC Request ${path}", ("path", r.path));
                        s.set_length(message.size());
                        s.set_status(fc::http::reply::BadRequest);
                        s.write(message.c_str(), message.size());
                        status = fc::http::reply::BadRequest;
                    }

                    fc::time_point end_time = fc::time_point::now();
                    fc_ilog(fc::logger::get("rpc"), "Completed ${path} ${status} in ${ms}ms", ("path", r.path)("status", (int)status)("ms", (end_time - begin_time).count() / 1000));
                }


                template<typename T>
                bool skip_white_space_v1(T& in)
                {
                    bool skipped = false;
                    while (true)
                    {
                        switch (in.peek())
                        {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r':
                            skipped = true;
                            in.get();
                            break;
                        default:
                            return skipped;
                        }
                    }
                }
                template<typename T>
                char parseEscape_v1(T& in)
                {
                    if (in.peek() == '\\')
                    {
                        try {
                            in.get();
                            switch (in.peek())
                            {
                            case 't':
                                in.get();
                                return '\t';
                            case 'n':
                                in.get();
                                return '\n';
                            case 'r':
                                in.get();
                                return '\r';
                            case '\\':
                                in.get();
                                return '\\';
                            default:
                                return in.get();
                            }
                        }
                        catch (...){};
                    }

                }
                template<typename T>
                fc::string stringFromToken_v1(T& in)
                {
                    fc::stringstream token;
                    try
                    {
                        char c = in.peek();

                        while (true)
                        {
                            switch (c = in.peek())
                            {
                            case '\\':
                                token << parseEscape_v1(in);
                                break;
                            case '\t':
                            case ' ':
                            case '\0':
                            case '\n':
                                in.get();
                                return token.str();
                            default:
                                if (isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':' || c == '/')
                                {
                                    token << c;
                                    in.get();
                                }
                                else return token.str();
                            }
                        }
                        return token.str();
                    }
                    catch (const fc::eof_exception&)
                    {
                        return token.str();
                    }
                    catch (const std::ios_base::failure&)
                    {
                        return token.str();
                    }

                    FC_RETHROW_EXCEPTIONS(warn, "while parsing token '${token}'",
                        ("token", token.str()));
                }

                template<typename T, bool strict>
                variant_object objectFromStream_v1(T& in)
                {
                    mutable_variant_object obj;
                    try
                    {
                        //char c = in.peek();

                        in.get();
                        skip_white_space_v1(in);
                        while (in.peek() != '}')
                        {
                            if (in.peek() == ',')
                            {
                                in.get();
                                continue;
                            }
                            if (skip_white_space_v1(in)) continue;
                            string key = stringFromStream_v1<T, strict>(in);
                            skip_white_space_v1(in);
                            if (in.peek() != ':')
                            {

                            }
                            in.get();
                            auto val = variant_from_stream_v1<T, strict>(in);

                            obj(std::move(key), std::move(val));
                            skip_white_space_v1(in);
                        }
                        if (in.peek() == '}')
                        {
                            in.get();
                            return obj;
                        }

                    }
                    catch (const fc::eof_exception&)
                    {

                    }
                    catch (const std::ios_base::failure&)
                    {

                    } FC_RETHROW_EXCEPTIONS(warn, "Error parsing object");
                }

                template<typename T, bool strict>
                fc::variants arrayFromStream_v1(T& in)
                {
                    fc::variants ar;
                    try
                    {
                        if (in.peek() != '[');

                        in.get();
                        skip_white_space_v1(in);

                        while (in.peek() != ']')
                        {
                            if (in.peek() == ',')
                            {
                                in.get();
                                continue;
                            }
                            if (skip_white_space_v1(in)) continue;
                            ar.push_back(variant_from_stream_v1<T, strict>(in));
                            skip_white_space_v1(in);
                            // in.get();
                        }

                        in.get();


                    } FC_RETHROW_EXCEPTIONS(warn, "Attempting to parse array ${array}",
                        ("array", ar));
                    return ar;
                }


                template<typename T, bool strict, bool allow_escape>
                fc::string quoteStringFromStream_v1(T& in)
                {
                    fc::stringstream token;
                    try
                    {
                        char q = in.get();
                        switch (q)
                        {
                        case '\'':
                            if (strict)

                                // falls through
                        case '"':
                            break;
                        default:
                            if (strict)
                                ;

                        }
                        if (in.peek() == q)
                        {
                            in.get();
                            if (in.peek() != q)
                                return fc::string();

                            // triple quote processing
                            if (strict)
                            {
                            }
                            else
                            {
                                in.get();

                                while (true)
                                {
                                    char c = in.peek();
                                    if (c == q)
                                    {
                                        in.get();
                                        char c2 = in.peek();
                                        if (c2 == q)
                                        {
                                            char c3 = in.peek();
                                            if (c3 == q)
                                            {
                                                in.get();
                                                return token.str();
                                            }
                                            token << q << q;
                                            continue;
                                        }
                                        token << q;
                                        continue;
                                    }
                                    else if (c == '\x04')
                                    {
                                    }
                                    else if (allow_escape && (c == '\\'))
                                        token << parseEscape_v1(in);
                                    else
                                    {
                                        in.get();
                                        token << c;
                                    }
                                }
                            }
                        }

                        while (true)
                        {
                            char c = in.peek();

                            if (c == q)
                            {
                                in.get();
                                return token.str();
                            }
                            else if (c == '\x04')
                                token.str();
                            else if (allow_escape && (c == '\\'))
                                token << parseEscape_v1(in);
                            else if ((c == '\r') | (c == '\n'))
                                token.str();
                            else
                            {
                                in.get();
                                token << c;
                            }
                        }

                    } FC_RETHROW_EXCEPTIONS(warn, "while parsing token '${token}'",
                        ("token", token.str()));
                }


                template<typename T>
                fc::string tokenFromStream_v1(T& in)
                {
                    fc::stringstream token;
                    try
                    {
                        char c = in.peek();

                        while (true)
                        {
                            switch (c = in.peek())
                            {
                            case '\\':
                                token << parseEscape_v1(in);
                                break;
                            case '\t':
                            case ' ':
                            case ',':
                            case ':':
                            case '\0':
                            case '\n':
                            case '\x04':
                                in.get();
                                return token.str();
                            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
                            case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                            case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                            case 'y': case 'z':
                            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
                            case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                            case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                            case 'Y': case 'Z':
                            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
                            case '8': case '9':
                            case '_': case '-': case '.': case '+': case '/':
                                token << c;
                                in.get();
                                break;
                            default:
                                return token.str();
                            }
                        }
                        return token.str();
                    }
                    catch (const fc::eof_exception&)
                    {
                        return token.str();
                    }
                    catch (const std::ios_base::failure&)
                    {
                        return token.str();
                    }

                    FC_RETHROW_EXCEPTIONS(warn, "while parsing token '${token}'",
                        ("token", token.str()));
                }
                template<typename T, bool strict>
                fc::string stringFromStream_v1(T& in)
                {
                    try
                    {
                        char c = in.peek(), c2;

                        switch (c)
                        {
                        case '\'':
                            if (strict)

                                // falls through
                        case '"':
                            return quoteStringFromStream_v1<T, strict, true>(in);
                        case 'r':
                            if (strict)

                        case 'R':
                            in.get();
                            c2 = in.peek();
                            switch (c2)
                            {
                            case '"':
                            case '\'':
                                if (strict)

                                    return quoteStringFromStream_v1<T, strict, false>(in);
                            default:
                                if (strict)

                                    return c + tokenFromStream_v1(in);
                            }
                            break;
                        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
                        case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                        case 'q':           case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                        case 'y': case 'z':
                        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
                        case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                        case 'Q':           case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                        case 'Y': case 'Z':
                        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
                        case '8': case '9':
                        case '_': case '-': case '.': case '+': case '/':
                            if (strict)

                                return tokenFromStream_v1(in);
                        default:
                            ;
                        }

                    } FC_RETHROW_EXCEPTIONS(warn, "while parsing string");
                }


                template<typename T, bool strict>
                variant wordFromStream(T& in)
                {
                    fc::string token = tokenFromStream(in);

                    FC_ASSERT(token.length() > 0);

                    switch (token[0])
                    {
                    case 'n':
                        if (token == "null")
                            return variant();
                        break;
                    case 't':
                        if (token == "true")
                            return variant(true);
                        break;
                    case 'f':
                        if (token == "false")
                            return variant(false);
                        break;
                    default:
                        break;
                    }

                    if (!strict)
                        return token;

                    ;
                }

                struct CharValueTable
                {
                public:
                    CharValueTable()
                    {
                        for (size_t i = 0; i < 0x100; i++)
                            c2v[i] = 0xFF;
                        c2v[(unsigned char)'0'] = 0;
                        c2v[(unsigned char)'1'] = 1;
                        c2v[(unsigned char)'2'] = 2;
                        c2v[(unsigned char)'3'] = 3;
                        c2v[(unsigned char)'4'] = 4;
                        c2v[(unsigned char)'5'] = 5;
                        c2v[(unsigned char)'6'] = 6;
                        c2v[(unsigned char)'7'] = 7;
                        c2v[(unsigned char)'8'] = 8;
                        c2v[(unsigned char)'9'] = 9;
                        c2v[(unsigned char)'a'] = c2v[(unsigned char)'A'] = 10;
                        c2v[(unsigned char)'b'] = c2v[(unsigned char)'B'] = 11;
                        c2v[(unsigned char)'c'] = c2v[(unsigned char)'C'] = 12;
                        c2v[(unsigned char)'d'] = c2v[(unsigned char)'D'] = 13;
                        c2v[(unsigned char)'e'] = c2v[(unsigned char)'E'] = 14;
                        c2v[(unsigned char)'f'] = c2v[(unsigned char)'F'] = 15;
                        c2v[(unsigned char)'g'] = c2v[(unsigned char)'G'] = 16;
                        c2v[(unsigned char)'h'] = c2v[(unsigned char)'H'] = 17;
                        c2v[(unsigned char)'i'] = c2v[(unsigned char)'I'] = 18;
                        c2v[(unsigned char)'j'] = c2v[(unsigned char)'J'] = 19;
                        c2v[(unsigned char)'k'] = c2v[(unsigned char)'K'] = 20;
                        c2v[(unsigned char)'l'] = c2v[(unsigned char)'L'] = 21;
                        c2v[(unsigned char)'m'] = c2v[(unsigned char)'M'] = 22;
                        c2v[(unsigned char)'n'] = c2v[(unsigned char)'N'] = 23;
                        c2v[(unsigned char)'o'] = c2v[(unsigned char)'O'] = 24;
                        c2v[(unsigned char)'p'] = c2v[(unsigned char)'P'] = 25;
                        c2v[(unsigned char)'q'] = c2v[(unsigned char)'Q'] = 26;
                        c2v[(unsigned char)'r'] = c2v[(unsigned char)'R'] = 27;
                        c2v[(unsigned char)'s'] = c2v[(unsigned char)'S'] = 28;
                        c2v[(unsigned char)'t'] = c2v[(unsigned char)'T'] = 29;
                        c2v[(unsigned char)'u'] = c2v[(unsigned char)'U'] = 30;
                        c2v[(unsigned char)'v'] = c2v[(unsigned char)'V'] = 31;
                        c2v[(unsigned char)'w'] = c2v[(unsigned char)'W'] = 32;
                        c2v[(unsigned char)'x'] = c2v[(unsigned char)'X'] = 33;
                        c2v[(unsigned char)'y'] = c2v[(unsigned char)'Y'] = 34;
                        c2v[(unsigned char)'z'] = c2v[(unsigned char)'Z'] = 35;
                        return;
                    }

                    uint8_t operator[](char index) const { return c2v[index & 0xFF]; }

                    uint8_t c2v[0x100];
                };
                template<uint8_t base>
                fc::variant parseInt_v1(const fc::string& token, size_t start)
                {
                    static const CharValueTable ctbl;
                    static const uint64_t INT64_MAX_PLUS_ONE = static_cast<uint64_t>(INT64_MAX)+1;

                    size_t i = start, n = token.length();



                    uint64_t val = 0;
                    //uint64_t maxb4mul = UINT64_MAX / base;

                    while (true)
                    {
                        char c = token[i];
                        uint8_t vc = ctbl[c];

                        val *= base;
                        uint64_t newval = val + vc;


                        val = newval;
                        i++;
                        if (i >= n)
                            break;
                    }
                    if (token[0] == '-')
                    {


                        // special cased to avoid trying to compute -INT64_MIN which is probably undefined or something
                        if (val == INT64_MAX_PLUS_ONE)
                            return fc::variant(INT64_MIN);
                        return fc::variant(-static_cast<int64_t>(val));
                    }
                    return fc::variant(val);
                }
                template<bool strict, uint8_t base>
                fc::variant maybeParseInt_v1(const fc::string& token, size_t start)
                {
                    try
                    {
                        return parseInt_v1<base>(token, start);
                    }
                    catch (const fc::exception &)
                    {
                        return fc::variant(token);
                    }
                }

                template<bool strict>
                fc::variant parseNumberOrStr_v1(const fc::string& token)
                {

                    size_t i = 0, n = token.length();
                    switch (token[0])
                    {
                    case '+':
                        i++;
                        break;
                    case '-':
                        i++;
                        break;
                    default:
                        break;
                    }
                    char c = token[i++];
                    switch (c)
                    {
                    case '0':
                        if (i >= n)
                            return fc::variant(uint64_t(0));
                        switch (token[i])
                        {
                        case 'b':
                        case 'B':
                            i++;
                            return maybeParseInt_v1<strict, 2>(token, i + 1);
                        case 'o':
                        case 'O':
                            return maybeParseInt_v1<strict, 8>(token, i + 1);
                        case 'x':
                        case 'X':
                            return maybeParseInt_v1<strict, 16>(token, i + 1);
                        case '.':
                        case 'e':
                        case 'E':
                            break;
                        default:;
                            // since this is a lookahead, other cases will be treated later

                        }
                        break;
                    case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                        break;
                    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
                    case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                    case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                    case 'y': case 'z':
                    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
                    case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                    case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                    case 'Y': case 'Z':
                    case '_': case '-': case '.': case '+': case '/':

                        return fc::variant(token);
                    default:
                        ;
                    }
                    size_t start = i - 1;
                    bool dot_ok = true;

                    // int frac? exp?
                    while (true)
                    {
                        if (i >= n)
                            if (dot_ok)
                                return parseInt_v1<10>(token, start);
                            else
                                return fc::variant(fc::to_double(token.c_str()));
                        char c = token[i++];
                        switch (c)
                        {
                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                            break;
                        case '.':
                            if (dot_ok)
                            {

                                dot_ok = false;
                                if (i == n)
                                {
                                    return fc::variant(fc::to_double(token.c_str()));
                                }

                                c = token[i + 1];
                                switch (c)
                                {
                                case '0': case '1': case '2': case '3': case '4':
                                case '5': case '6': case '7': case '8': case '9':
                                    break;
                                case 'e':
                                case 'E':
                                    break;
                                case 'a': case 'b': case 'c': case 'd':           case 'f': case 'g': case 'h':
                                case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                                case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                                case 'y': case 'z':
                                case 'A': case 'B': case 'C': case 'D':           case 'F': case 'G': case 'H':
                                case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                                case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                                case 'Y': case 'Z':
                                case '_': case '-': case '.': case '+': case '/':

                                    return fc::variant(token);
                                default:
                                    ;
                                }
                            }
                            else
                            {
                                return fc::variant(token);
                            }
                            break;
                        case 'e':
                        case 'E':
                            if (i == n)
                            {

                                return fc::variant(token);
                            }
                            c = token[i++];
                            switch (c)
                            {
                            case '+': case '-':
                                if (i == n)
                                {

                                    return fc::variant(token);
                                }
                                break;
                            case '0': case '1': case '2': case '3': case '4':
                            case '5': case '6': case '7': case '8': case '9':
                                break;
                            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
                            case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                            case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                            case 'y': case 'z':
                            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
                            case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                            case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                            case 'Y': case 'Z':
                            case '_':           case '.':           case '/':

                                return fc::variant(token);
                            default:
                                ;
                            }
                            while (true)
                            {
                                if (i == n)
                                    break;
                                c = token[i++];
                                switch (c)
                                {
                                case '0': case '1': case '2': case '3': case '4':
                                case '5': case '6': case '7': case '8': case '9':
                                    break;
                                case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
                                case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                                case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                                case 'y': case 'z':
                                case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
                                case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                                case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                                case 'Y': case 'Z':
                                case '_': case '-': case '.': case '+': case '/':

                                    return fc::variant(token);
                                }
                            }
                            return fc::variant(fc::to_double(token.c_str()));
                        case 'a': case 'b': case 'c': case 'd':           case 'f': case 'g': case 'h':
                        case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                        case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                        case 'y': case 'z':
                        case 'A': case 'B': case 'C': case 'D':           case 'F': case 'G': case 'H':
                        case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                        case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                        case 'Y': case 'Z':
                        case '_': case '-':           case '+': case '/':

                            return fc::variant(token);
                        default:
                            ;
                        }
                    }
                }
                template<typename T, bool strict>
                variant numberFromStream_v1(T& in)
                {
                    fc::string token = tokenFromStream_v1(in);
                    variant result = parseNumberOrStr_v1<strict>(token);

                    return result;
                }


                template<typename T, bool strict>
                variant wordFromStream_v1(T& in)
                {
                    fc::string token = tokenFromStream_v1(in);

                    FC_ASSERT(token.length() > 0);

                    switch (token[0])
                    {
                    case 'n':
                        if (token == "null")
                            return variant();
                        break;
                    case 't':
                        if (token == "true")
                            return variant(true);
                        break;
                    case 'f':
                        if (token == "false")
                            return variant(false);
                        break;
                    default:
                        break;
                    }

                    if (!strict)
                        return token;

                }
                template<typename T, bool strict>
                variant variant_from_stream_v1(T& in)
                {
                    skip_white_space_v1(in);
                    variant var;
                    while (char c = in.peek())
                    {
                        switch (c)
                        {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r':
                            in.get();
                            continue;
                        case '"':
                            return stringFromStream_v1<T, strict>(in);
                        case '{':
                            return objectFromStream_v1<T, strict>(in);
                        case '[':
                            return arrayFromStream_v1<T, strict>(in);
                        case '-':
                        case '+':
                        case '.':
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                            return numberFromStream_v1<T, strict>(in);
                            // null, true, false, or 'warning' / string
                        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
                        case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
                        case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
                        case 'y': case 'z':
                        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
                        case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
                        case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
                        case 'Y': case 'Z':
                        case '_':                               case '/':
                            return wordFromStream_v1<T, strict>(in);
                        case 0x04: // ^D end of transmission
                        case EOF:
                            ;
                        default:
                            stringFromToken_v1(in);
                        }
                    }
                    return variant();
                }
                variant from_string_v1(const std::string& utf8_str)
                {
                    try {
                        fc::stringstream in(utf8_str);
                        //in.exceptions( std::ifstream::eofbit );

                        return variant_from_stream_v1<fc::stringstream, false>(in);

                    } FC_RETHROW_EXCEPTIONS(warn, "", ("str", utf8_str))
                }

                fc::http::reply::status_code handle_http_rpc(const fc::http::request& r, const fc::http::server::response& s)
                {
                    fc::http::reply::status_code status = fc::http::reply::OK;
                    std::string str(r.body.data(), r.body.size());
                    //wlog( "RPC: ${r}", ("r",str) );
                    fc::string method_name;

                    fc::optional<std::string> invalid_rpc_request_message;

                    try {
                        auto rpc_call = from_string_v1(str).get_object();
                        method_name = rpc_call["method"].as_string();
                        auto params = rpc_call["params"].get_array();

                        auto request_key = method_name + "=" + fc::json::to_string(rpc_call["params"]);


                        if (fc::time_point(thinkyoung::blockchain::now()) - _last_cache_clear_time > fc::seconds(ALP_BLOCKCHAIN_BLOCK_INTERVAL_SEC))
                        {
                            _last_cache_clear_time = thinkyoung::blockchain::now();
                            _call_cache.clear();
                        }
                        else if (_cache_enabled)
                        {
                            auto cache_itr = _call_cache.find(request_key);
                            if (cache_itr != _call_cache.end())
                            {
                                status = fc::http::reply::OK;
                                s.set_status(status);

                                auto reply = cache_itr->second;
                                s.set_length(reply.size());
                                s.write(reply.c_str(), reply.size());
                                auto reply_log = reply.size() > 253 ? reply.substr(0, 253) + ".." : reply;
                                fc_ilog(fc::logger::get("rpc"), "Result ${path} ${method}: ${reply}", ("path", r.path)("method", method_name)("reply", reply_log));
                                return status;
                            }
                        }


                        auto params_log = fc::json::to_string(rpc_call["params"]);
                        if (method_name.find("wallet") != std::string::npos || method_name.find("priv") != std::string::npos)
                            params_log = "***";
                        fc_ilog(fc::logger::get("rpc"), "Processing ${path} ${method} (${params})", ("path", r.path)("method", method_name)("params", params_log));

                        auto call_itr = _alias_map.find(method_name);
                        if (call_itr != _alias_map.end())
                        {
                            fc::mutable_variant_object  result;
                            result["id"] = rpc_call["id"];
                            try
                            {
                                result["result"] = dispatch_authenticated_method(_method_map[call_itr->second], params);
                                //   auto reply = fc::json::to_string( result );
                                status = fc::http::reply::OK;
                                s.set_status(status);
                            }
                            catch (const fc::canceled_exception&)
                            {
                                throw;
                            }
                            catch (const fc::exception& e)
                            {
                                status = fc::http::reply::InternalServerError;
                                s.set_status(status);
                                result["error"] = fc::mutable_variant_object("message", e.to_string())("detail", e.to_detail_string())("code", e.code());
                            }
                            //ilog( "${e}", ("e",result) );
                            auto reply = fc::json::to_string(result);
                            s.set_length(reply.size());
                            s.write(reply.c_str(), reply.size());
                            auto reply_log = reply.size() > 253 ? reply.substr(0, 253) + ".." : reply;
                            fc_ilog(fc::logger::get("rpc"), "Result ${path} ${method}: ${reply}", ("path", r.path)("method", method_name)("reply", reply_log));

                            if (_method_map[method_name].cached)
                                _call_cache[request_key] = reply;

                            return status;
                        }
                        else
                        {
                            fc_ilog(fc::logger::get("rpc"), "Invalid Method ${path} ${method}", ("path", r.path)("method", method_name));
                            elog("Invalid Method ${path} ${method}", ("path", r.path)("method", method_name));
                            std::string message = "Invalid Method: " + method_name;
                            fc::mutable_variant_object  result;
                            result["id"] = rpc_call["id"];
                            status = fc::http::reply::NotFound;
                            s.set_status(status);
                            result["error"] = fc::mutable_variant_object("message", message);
                            auto reply = fc::json::to_string(result);
                            s.set_length(reply.size());
                            s.write(reply.c_str(), reply.size());
                            return status;
                        }
                    }
                    catch (const fc::canceled_exception&)
                    {
                        throw;
                    }
                    catch (const fc::exception& e)
                    {
                        invalid_rpc_request_message = e.to_detail_string();
                    }
                    catch (const std::exception& e)
                    {
                        invalid_rpc_request_message = e.what();
                    }
                    catch (...)
                    {
                        invalid_rpc_request_message = "...";
                    }

                    if (invalid_rpc_request_message)
                    {
                        fc_ilog(fc::logger::get("rpc"), "Invalid RPC Request ${path} ${method}: ${e}", ("path", r.path)("method", method_name)("e", *invalid_rpc_request_message));
                        elog("Invalid RPC Request ${path} ${method}: ${e}", ("path", r.path)("method", method_name)("e", *invalid_rpc_request_message));
                        std::string message = "Invalid RPC Request\n";
                        message += *invalid_rpc_request_message;
                        s.set_length(message.size());
                        status = fc::http::reply::BadRequest;
                        s.set_status(status);
                        s.write(message.c_str(), message.size());
                    }
                    return status;
                }

                void accept_loop()
                {
                    while (!_accept_loop_complete.canceled())
                    {
                        fc::tcp_socket_ptr sock = std::make_shared<fc::tcp_socket>();
                        try
                        {
                            _tcp_serv->accept(*sock);
                        }
                        catch (const fc::canceled_exception&)
                        {
                            throw;
                        }
                        catch (const fc::exception& e)
                        {
                            elog("fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string()));
                            continue;
                        }

                        auto buf_istream = std::make_shared<fc::buffered_istream>(sock);
                        auto buf_ostream = std::make_shared<fc::buffered_ostream>(sock);

                        auto json_con = std::make_shared<fc::rpc::json_connection>(std::move(buf_istream),
                            std::move(buf_ostream));
                        register_methods(json_con);
                        auto receipt = _open_json_connections.insert(json_con);

                        json_con->exec().on_complete([this, receipt, sock](fc::exception_ptr e){
                            ilog("json_con exited");
                            sock->close();
                            _open_json_connections.erase(receipt.first);
                            if (e)
                                elog("Connection exited with error: ${error}", ("error", e->what()));
                        });
                    }
                }

                void encrypted_accept_loop(const thinkyoung::blockchain::PrivateKeyType& server_key)
                {
                    while (!_encrypted_accept_loop_complete.canceled())
                    {
                        net::StcpSocketPtr sock = std::make_shared<net::StcpSocket>();
                        try
                        {
                            _stcp_serv->accept(sock->get_socket());
                            sock->accept();
                            auto signature = server_key.sign_compact(fc::digest(sock->get_shared_secret()));//Ç©Ãû
                            auto padded_signature = fc::raw::pack(signature);
                            while (padded_signature.size() % 16)
                                padded_signature.push_back(0);
                            sock->write(padded_signature.data(), padded_signature.size());
                        }
                        catch (const fc::canceled_exception&)
                        {
                            throw;
                        }
                        catch (const fc::exception& e)
                        {
                            elog("fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string()));
                            continue;
                        }

                        auto buf_istream = std::make_shared<fc::buffered_istream>(sock);
                        auto buf_ostream = std::make_shared<utilities::PaddingOstream<>>(sock);

                        auto json_con = std::make_shared<fc::rpc::json_connection>(std::move(buf_istream),
                            std::move(buf_ostream));
                        register_methods(json_con);
                        auto receipt = _open_json_connections.insert(json_con);

                        json_con->exec().on_complete([this, receipt, sock](fc::exception_ptr e){
                            ilog("json_con exited");
                            sock->close();
                            _open_json_connections.erase(receipt.first);
                            if (e)
                                elog("Connection exited with error: ${error}", ("error", e->what()));
                        });
                    }
                }

                void register_methods(fc::rpc::json_connection_ptr con)
                {
                    ilog("login!");
                    fc::rpc::json_connection* capture_con = con.get();

                    // the login method is a special case that is only used for raw json connections
                    // (not for the CLI or HTTP(s) json rpc)
                    con->add_method("login", boost::bind(&RpcServerImpl::login, this, capture_con, _1));
                    for (const MethodMapType::value_type& method : _method_map)
                    {
                        if (method.second.method)
                        {
                            // old method using old registration system
                            auto bind_method = boost::bind(&RpcServerImpl::dispatch_method_from_json_connection,
                                this, capture_con, method.second, _1);
                            con->add_method(method.first, bind_method);
                            for (auto alias : method.second.aliases)
                                con->add_method(alias, bind_method);
                        }
                    }

                    register_common_api_methods(con);
                } // register methods

                fc::variant dispatch_method_from_json_connection(fc::rpc::json_connection* con,
                    const thinkyoung::api::MethodData& method_data,
                    const fc::variants& arguments)
                {
                    // ilog( "arguments: ${params}", ("params",arguments) );
                    if ((method_data.prerequisites & thinkyoung::api::json_authenticated) &&
                        _authenticated_connection_set.find(con) == _authenticated_connection_set.end())
                        FC_THROW_EXCEPTION(login_required, "not logged in");
                    return dispatch_authenticated_method(method_data, arguments);
                }

                fc::variant dispatch_authenticated_method(const thinkyoung::api::MethodData& method_data,
                    const fc::variants& arguments_from_caller)
                {
                    fc::scoped_lock<fc::mutex> lock(_rpc_mutex);

                    if (!method_data.method)
                    {
                        // then this is a method using our new generated code
                        return direct_invoke_positional_method(method_data.name, arguments_from_caller);
                    }
                    //ilog("method: ${method_name}, arguments: ${params}", ("method_name", method_data.name)("params",arguments_from_caller));
                    if (method_data.prerequisites & thinkyoung::api::wallet_open)
                        verify_wallet_is_open();
                    if (method_data.prerequisites & thinkyoung::api::wallet_unlocked)
                        verify_wallet_is_unlocked();
                    if (method_data.prerequisites & thinkyoung::api::connected_to_network)
                        verify_connected_to_network();
                    if (method_data.prerequisites & thinkyoung::api::sandbox_open)
                        verify_sandbox_is_open();


                    fc::variants modified_positional_arguments;
                    fc::mutable_variant_object modified_named_arguments;

                    bool has_named_parameters = false;
                    unsigned next_argument_index = 0;
                    for (const thinkyoung::api::ParameterData& parameter : method_data.parameters)
                    {
                        if (parameter.classification == thinkyoung::api::required_positional
                            || parameter.classification == thinkyoung::api::required_positional_hidden)
                        {
                            if (arguments_from_caller.size() < next_argument_index + 1)
                                FC_THROW_EXCEPTION(missing_parameter, "missing required parameter ${parameter_name}", ("parameter_name", parameter.name));
                            modified_positional_arguments.push_back(arguments_from_caller[next_argument_index++]);
                        }
                        else if (parameter.classification == thinkyoung::api::optional_positional)
                        {
                            if (arguments_from_caller.size() > next_argument_index)
                                // the caller provided this optional argument
                                modified_positional_arguments.push_back(arguments_from_caller[next_argument_index++]);
                            else if (parameter.default_value.valid())
                                // we have a default value for this paramter, use it
                                modified_positional_arguments.push_back(*parameter.default_value);
                            else
                                // missing optional parameter with no default value, stop processing
                                break;
                        }
                        else if (parameter.classification == thinkyoung::api::optional_named)
                        {
                            has_named_parameters = true;
                            if (arguments_from_caller.size() > next_argument_index)
                            {
                                // user provided a map of named arguments.  If the user gave a value for this argument,
                                // take it, else use our default value
                                fc::variant_object named_parameters_from_caller = arguments_from_caller[next_argument_index].get_object();
                                if (named_parameters_from_caller.contains(parameter.name.c_str()))
                                    modified_named_arguments[parameter.name.c_str()] = named_parameters_from_caller[parameter.name.c_str()];
                                else if (parameter.default_value.valid())
                                    modified_named_arguments[parameter.name.c_str()] = *parameter.default_value;
                            }
                            else if (parameter.default_value.valid())
                            {
                                // caller didn't provide any map of named parameters, just use our default values
                                modified_named_arguments[parameter.name.c_str()] = *parameter.default_value;
                            }
                        }
                    }

                    if (has_named_parameters)
                        modified_positional_arguments.push_back(modified_named_arguments);

                    //ilog("After processing: method: ${method_name}, arguments: ${params}", ("method_name", method_data.name)("params",modified_positional_arguments));

                    return method_data.method(modified_positional_arguments);
                }

                // This method invokes the function directly, called by the CLI intepreter.
                fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
                {
                    // ilog( "method: ${method} arguments: ${params}", ("method",method_name)("params",arguments) );
                    auto iter = _alias_map.find(method_name);
                    if (iter == _alias_map.end())
                        FC_THROW_EXCEPTION(unknown_method, "Invalid command ${command}", ("command", method_name));
                    return dispatch_authenticated_method(_method_map[iter->second], arguments);
                }

                fc::variant login(fc::rpc::json_connection* json_connection, const fc::variants& params);
            };

            thinkyoung::api::CommonApi* RpcServerImpl::get_client() const
            {
                return _client;
            }
            void RpcServerImpl::verify_json_connection_is_authenticated(fc::rpc::json_connection* json_connection) const
            {
                if (json_connection &&
                    _authenticated_connection_set.find(json_connection) == _authenticated_connection_set.end())
                    FC_THROW("The RPC connection must be logged in before executing this command");
            }
            void RpcServerImpl::verify_wallet_is_open() const
            {
                if (!_client->get_wallet()->is_open())
                    throw rpc_wallet_open_needed_exception(FC_LOG_MESSAGE(error, "The wallet must be open before executing this command"));
            }
            void RpcServerImpl::verify_wallet_is_unlocked() const
            {
                if (_client->get_wallet()->is_locked())
                    throw rpc_wallet_unlock_needed_exception(FC_LOG_MESSAGE(error, "The wallet's spending key must be unlocked before executing this command"));
            }

            void RpcServerImpl::verify_sandbox_is_open() const
            {
                if (!_client->get_chain()->get_is_in_sandbox())
                    throw  rpc_sandbox_open_needed_exception(FC_LOG_MESSAGE(error, "The sandbox must be opened before executing this command"));
            }

            void RpcServerImpl::verify_connected_to_network() const
            {
                if (!_client->is_connected())
                    throw rpc_client_not_connected_exception(FC_LOG_MESSAGE(error, "The client must be connected to the network to execute this command"));
            }
            void RpcServerImpl::store_method_metadata(const thinkyoung::api::MethodData& method_metadata)
            {
                _self->register_method(method_metadata);
            }

            //register_method(method_data{"login", JSON_METHOD_IMPL(login),
            //          /* description */ "authenticate JSON-RPC connection",
            //          /* returns: */    "bool",
            //          /* params:          name            type       required */
            //                            {{"username", "string",  true},
            //                             {"password", "string",  true}},
            //        /* prerequisites */ 0});
            fc::variant RpcServerImpl::login(fc::rpc::json_connection* json_connection, const fc::variants& params)
            {
                FC_ASSERT(params.size() == 2);
                FC_ASSERT(params[0].as_string() == _config.rpc_user);
                FC_ASSERT(params[1].as_string() == _config.rpc_password);
                _authenticated_connection_set.insert(json_connection);
                return fc::variant(true);
            }

            std::string RpcServerImpl::help(const std::string& command_name) const
            {
                std::string help_string;
                if (command_name.empty()) //if no arguments, display list of commands with short description
                {
                    for (const MethodMapType::value_type& method_data_pair : _method_map)
                        if (method_data_pair.second.name[0] != '_' && method_data_pair.second.name.substr(0, 8) != "bitcoin_") // hide undocumented commands
                            help_string += make_short_description(method_data_pair.second, false);
                }
                else
                { //display detailed description of requested command
                    auto alias_iter = _alias_map.find(command_name);
                    if (alias_iter != _alias_map.end())
                    {
                        auto method_iter = _method_map.find(alias_iter->second);
                        FC_ASSERT(method_iter != _method_map.end(), "internal error, mismatch between _method_map and _alias_map");
                        const thinkyoung::api::MethodData& method_data = method_iter->second;
                        help_string += "Usage:\n";
                        help_string += make_short_description(method_data);
                        help_string += method_data.detailed_description;
                        if (method_data.aliases.size() > 0)
                            help_string += "\naliases: " + boost::join(method_data.aliases, ", ");
                    }
                    else
                    {
                        // no exact matches for the command they requested.
                        // If they give us a prefix, give them the list of commands that start
                        // with that prefix (i.e. "help wallet" will return wallet_open, wallet_close, &c)
                        std::set<std::string> matching_commands;
                        for (auto method_iter = _method_map.lower_bound(command_name);
                            method_iter != _method_map.end() && method_iter->first.compare(0, command_name.size(), command_name) == 0;
                            ++method_iter)
                            matching_commands.insert(method_iter->first);
                        // If they give us an alias (or its prefix), give them the list of real command names, eliminating duplication
                        for (auto alias_itr = _alias_map.lower_bound(command_name);
                            alias_itr != _alias_map.end() && alias_itr->first.compare(0, command_name.size(), command_name) == 0;
                            ++alias_itr)
                            matching_commands.insert(alias_itr->second);
                        for (auto command : matching_commands)
                        {
                            auto method_iter = _method_map.find(command);
                            FC_ASSERT(method_iter != _method_map.end(), "internal error, mismatch between _method_map and _alias_map");
                            help_string += make_short_description(method_iter->second);
                        }
                        if (help_string.empty())
                            throw rpc_misc_error_exception(FC_LOG_MESSAGE(error, "No help available for command \"${command}\"", ("command", command_name)));
                    }
                }

                return boost::algorithm::trim_copy(help_string);
            }

        } // detail


        RpcServer::RpcServer(thinkyoung::client::Client* client) :
            my(new detail::RpcServerImpl(client))
        {
            my->_thread = &fc::thread::current();
            my->_self = this;
            try {
                my->register_common_api_method_metadata();
            }FC_RETHROW_EXCEPTIONS(warn, "register common api")
        }

        RpcServer::~RpcServer()
        {
            try
            {
                shutdown_rpc_server();
                wait_till_rpc_server_shutdown();
                // just to be safe, destroy the  servers inside this try/catch block in case they throw
                my->_tcp_serv.reset();
                my->_httpd.reset();
            }
            catch (const fc::exception& e)
            {
                wlog("unhandled exception thrown in destructor.\n${e}", ("e", e.to_detail_string()));
            }
        }

        bool RpcServer::configure_rpc(const RpcServerConfig& cfg)
        {
            if (!cfg.is_valid())
                return false;
            my->_cache_enabled = cfg.enable_cache;

            try
            {
                my->_config = cfg;
                my->_tcp_serv = std::make_shared<fc::tcp_server>();
                int attempts = 0;
                bool success = false;

                while (!success) {
                    try
                    {
                        my->_tcp_serv->listen(cfg.rpc_endpoint);
                        success = true;
                    }
                    catch (fc::exception& e)
                    {
                        FC_ASSERT(++attempts < 30, "Unable to bind RPC port; refusing to continue.");
                        ulog("Failed to bind RPC port ${endpoint}; waiting 10 seconds and retrying (attempt ${attempt}/30)",
                            ("endpoint", cfg.rpc_endpoint)("attempt", attempts));
                        elog("Failed to bind RPC port ${endpoint} with error ${e}", ("endpoint", cfg.rpc_endpoint)("e", e.to_detail_string()));
                    }
                    if (!success)
                        fc::usleep(fc::seconds(10));
                }

                ilog("listening for json rpc connections on port ${port}", ("port", my->_tcp_serv->get_port()));

                my->_accept_loop_complete = fc::async([=]{ my->accept_loop(); }, "rpc_server accept_loop");

                return true;
            } FC_RETHROW_EXCEPTIONS(warn, "attempting to configure rpc server ${port}", ("port", cfg.rpc_endpoint)("config", cfg));
        }

        bool RpcServer::configure_http(const RpcServerConfig& cfg)
        {
            if (!cfg.is_valid())
                return false;

            my->_cache_enabled = cfg.enable_cache;

            try
            {
                my->_config = cfg;
                auto m = my.get();
                my->_httpd = std::make_shared<fc::http::server>();
                int attempts = 0;
                bool success = false;

                while (!success) {
                    try
                    {
                        my->_httpd->listen(cfg.httpd_endpoint);
                        success = true;
                    }
                    catch (fc::exception& e)
                    {
                        FC_ASSERT(++attempts < 30, "Unable to bind HTTPD port; refusing to continue.");
                        ulog("Failed to bind HTTPD port ${endpoint}; waiting 10 seconds and retrying (attempt ${attempt}/30)",
                            ("endpoint", cfg.httpd_endpoint)("attempt", attempts));
                        elog("Failed to bind HTTPD port ${endpoint} with error ${e}", ("endpoint", cfg.rpc_endpoint)("e", e.to_detail_string()));
                    }
                    if (!success)
                        fc::usleep(fc::seconds(10));
                }

                my->_httpd->on_request([m](const fc::http::request& r, const fc::http::server::response& s){ m->handle_request(r, s); });
                return true;
            } FC_RETHROW_EXCEPTIONS(warn, "attempting to configure rpc server ${port}", ("port", cfg.rpc_endpoint)("config", cfg));
        }

        bool RpcServer::configure_encrypted_rpc(const RpcServerConfig& cfg)
        {
            if (!cfg.is_valid())
                return false;
            my->_cache_enabled = cfg.enable_cache;
            if (cfg.encrypted_rpc_wif_key.empty())
            {
                std::cerr << ("No WIF");
                return false;
            }
            auto server_key = utilities::wif_to_key(cfg.encrypted_rpc_wif_key);
            if (!server_key.valid())
            {
                std::cerr << ("Invalid WIF");
                return false;
            }

            try
            {
                my->_config = cfg;
                my->_stcp_serv = std::make_shared<fc::tcp_server>();
                int attempts = 0;
                bool success = false;

                while (!success) {
                    try
                    {
                        my->_stcp_serv->listen(cfg.encrypted_rpc_endpoint);
                        success = true;
                    }
                    catch (fc::exception& e)
                    {
                        FC_ASSERT(++attempts < 30, "Unable to bind encrypted RPC port; refusing to continue.");
                        ulog("Failed to bind encrypted RPC port ${endpoint}; waiting 10 seconds and retrying (attempt ${attempt}/30)",
                            ("endpoint", cfg.encrypted_rpc_endpoint)("attempt", attempts));
                        elog("Failed to bind encrypted RPC port ${endpoint} with error ${e}", ("endpoint", cfg.encrypted_rpc_endpoint)("e", e.to_detail_string()));
                    }
                    if (!success)
                        fc::usleep(fc::seconds(10));
                }

                ilog("listening for encrypted json rpc connections on port ${port}", ("port", my->_stcp_serv->get_port()));

                my->_encrypted_accept_loop_complete = fc::async([=]{ my->encrypted_accept_loop(*server_key); }, "rpc_server accept_loop");

                return true;
            } FC_RETHROW_EXCEPTIONS(warn, "attempting to configure rpc server ${port}", ("port", cfg.encrypted_rpc_endpoint)("config", cfg));

        }

        fc::variant RpcServer::direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
        {
            return my->direct_invoke_method(method_name, arguments);
        }

        const thinkyoung::api::MethodData& RpcServer::get_method_data(const std::string& method_name)
        {
            auto iter = my->_alias_map.find(method_name);
            if (iter != my->_alias_map.end())
                return my->_method_map[iter->second];
            FC_THROW_EXCEPTION(unknown_method, "Method \"${name}\" not found", ("name", method_name));
        }
        void RpcServer::set_http_file_callback(const HttpCallbackType& callback)
        {
            my->_http_file_callback = callback;
        }

        std::vector<thinkyoung::api::MethodData> RpcServer::get_all_method_data() const
        {
            std::vector<thinkyoung::api::MethodData> result;
            for (const detail::RpcServerImpl::MethodMapType::value_type& value : my->_method_map)
                result.emplace_back(value.second);
            return result;
        }

        void RpcServer::register_method(thinkyoung::api::MethodData data)
        {
            FC_ASSERT(my->_alias_map.find(data.name) == my->_alias_map.end(), "attempting to register an exsiting method name ${m}", ("m", data.name));
            my->_alias_map[data.name] = data.name;
            for (auto alias : data.aliases)
            {
                FC_ASSERT(my->_alias_map.find(alias) == my->_alias_map.end(), "attempting to register an exsiting method name ${m}", ("m", alias));
                my->_alias_map[alias] = data.name;
            }
            my->_method_map.insert(detail::RpcServerImpl::MethodMapType::value_type(data.name, data));
        }


        void RpcServer::wait_till_rpc_server_shutdown()
        {
            // wait until a quit has been signalled
            try
            {
                my->_on_quit_promise->wait();
            }
            catch (const fc::canceled_exception&)
            {
            }
            // if we were running a TCP server, also wait for it to shut down
            if (my->_tcp_serv && my->_accept_loop_complete.valid())
            {
                try
                {
                    my->_accept_loop_complete.wait();
                }
                catch (const fc::canceled_exception&)
                {
                }
            }
        }

        void RpcServer::shutdown_rpc_server()
        {
            // shutdown the server.  add a little delay to give the response to the "stop" method call a chance
            // to make it to the caller
            // my->_thread->async([=]() { fc::usleep(fc::milliseconds(10)); close(); });
            // Because we never waited on the above call we would crash... when rpc_server is
            // deleted before it can execute.
            if (my->_on_quit_promise)
                my->_on_quit_promise->set_value();
            if (my->_tcp_serv)
                my->_tcp_serv->close();
            if (my->_accept_loop_complete.valid() && !my->_accept_loop_complete.ready())
                my->_accept_loop_complete.cancel(__FUNCTION__);
        }

        std::string RpcServer::help(const std::string& command_name) const
        {
            return my->help(command_name);
        }

        MethodMapType RpcServer::meta_help()const
        {
            return my->_method_map;
        }

        fc::optional<fc::ip::endpoint> RpcServer::get_rpc_endpoint() const
        {
            if (my->_tcp_serv)
                return my->_tcp_serv->get_local_endpoint();
            return fc::optional<fc::ip::endpoint>();
        }

        fc::optional<fc::ip::endpoint> RpcServer::get_httpd_endpoint() const
        {
            if (my->_httpd)
                return my->_httpd->get_local_endpoint();
            return fc::optional<fc::ip::endpoint>();
        }


        Exception::Exception(fc::log_message&& m) :
            fc::exception(fc::move(m)) {}

        Exception::Exception(){}
        Exception::Exception(const Exception& t) :
            fc::exception(t) {}
        Exception::Exception(fc::log_messages m) :
            fc::exception() {}

#define RPC_EXCEPTION_IMPL(TYPE, ERROR_CODE, DESCRIPTION) \
  TYPE::TYPE(fc::log_message&& m) : \
    Exception(fc::move(m)) {} \
  TYPE::TYPE(){} \
  TYPE::TYPE(const TYPE& t) : Exception(t) {} \
  TYPE::TYPE(fc::log_messages m) : \
    Exception() {} \
  const char* TYPE::what() const throw() { return DESCRIPTION; } \
  int32_t TYPE::get_rpc_error_code() const { return ERROR_CODE; }

        // the codes here match bitcoine error codes in https://github.com/bitcoin/bitcoin/blob/master/src/rpcprotocol.h#L34
        RPC_EXCEPTION_IMPL(rpc_misc_error_exception, -1, "std::exception is thrown during command handling")
            RPC_EXCEPTION_IMPL(rpc_client_not_connected_exception, -9, "The client is not connected to the network")
            RPC_EXCEPTION_IMPL(rpc_wallet_unlock_needed_exception, -13, "The wallet's spending key must be unlocked before executing this command")
            RPC_EXCEPTION_IMPL(rpc_wallet_passphrase_incorrect_exception, -14, "The wallet passphrase entered was incorrect")

            // these error codes don't match anything in bitcoin
            RPC_EXCEPTION_IMPL(rpc_wallet_open_needed_exception, -100, "The wallet must be opened before executing this command")
            //sandbox related
            RPC_EXCEPTION_IMPL(rpc_sandbox_open_needed_exception, -101, "The sandbox must be opened before executing this command")

    }
} // thinkyoung::rpc
