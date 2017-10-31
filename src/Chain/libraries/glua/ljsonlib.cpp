#define ljsonlib_cpp

#include <glua/lprefix.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <cassert>

#include <glua/lua.h>

#include <glua/lauxlib.h>
#include <glua/lualib.h>
#include <glua/glua_tokenparser.h>
#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>

using thinkyoung::lua::api::global_glua_chain_api;

using namespace glua::parser;

static GluaStorageValue nil_storage_value()
{
    GluaStorageValue value;
    value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
    value.value.int_value = 0;
    return value;
}

static GluaStorageValue tokens_to_lua_value(lua_State *L, GluaTokenParser *token_parser, bool *result)
{
    if (token_parser->eof())
    {
        if (nullptr != result)
        {
            *result = false;
        }
        return nil_storage_value();
    }
    auto cur_token = token_parser->current();
    if (token_parser->current_position() == token_parser->size() - 1 || (cur_token.type != '{' && cur_token.type != '['))
    {
        auto token = token_parser->current();
        token_parser->next();
        switch (token.type)
        {
        case TOKEN_RESERVED::LTK_INT:
        {
            auto token_str = token.token;
            std::stringstream ss;
            ss << token_str;
            lua_Integer token_int = 0;
            ss >> token_int;
            GluaStorageValue value;
            value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
            value.value.int_value = token_int;
            if (nullptr != result)
                *result = true;
            return value;
        }  break;
        case TOKEN_RESERVED::LTK_FLT:
        {
            auto token_str = token.token;
            std::stringstream ss;
            ss << token_str;
            lua_Number token_num = 0;
            ss >> token_num;
            GluaStorageValue value;
            value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
            value.value.number_value = token_num;
            if (nullptr != result)
                *result = true;
            return value;
        } break;
        case TOKEN_RESERVED::LTK_TRUE:
        case TOKEN_RESERVED::LTK_FALSE:
        case TOKEN_RESERVED::LTK_NAME:
        {
            auto token_str = token.token;
            if (token_str == "true")
            {
                GluaStorageValue value;
                value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_bool;
                value.value.bool_value = true;
                if (nullptr != result)
                    *result = true;
                return value;
            }
            else if (token_str == "false")
            {
                GluaStorageValue value;
                value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_bool;
                value.value.bool_value = false;
                if (nullptr != result)
                    *result = true;
                return value;
            }
            else if (token_str == "null")
            {
                GluaStorageValue value;
                value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
                value.value.int_value = 0;
                if (nullptr != result)
                    *result = true;
                return value;
            }
            else
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown symbol name %s)", token_str.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
        } break;
        case TOKEN_RESERVED::LTK_STRING:
        {
            GluaStorageValue value;
            value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
            auto token_str = token.token;
            value.value.string_value = thinkyoung::lua::lib::malloc_managed_string(L, token_str.length() + 1);
            memset(value.value.string_value, 0x0, token_str.length() + 1);
            strncpy(value.value.string_value, token_str.c_str(), token_str.length());
            assert(nullptr != value.value.string_value);
            if (nullptr != result)
                *result = true;
            return value;
        } break;
        default:
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
            if (nullptr != result)
                *result = false;
            return nil_storage_value();
        }
    }
    auto token = token_parser->current();
    token_parser->next();
    switch (token.type)
    {
    case '{':
    {
        if (token_parser->eof())
        {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
            if (nullptr != result)
                *result = false;
            return nil_storage_value();
        }
        if (token_parser->current().type == '}')
        {
            token_parser->next();
            GluaStorageValue value;
            value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_unknown_table;
            value.value.table_value = thinkyoung::lua::lib::create_managed_lua_table_map(L);
            assert(nullptr != value.value.table_value);
            if (nullptr != result)
                *result = true;
            return value;
        }
        GluaStorageValue table_value;
        table_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_unknown_table; // FIXME: 根据子项类型修改
        table_value.value.table_value = thinkyoung::lua::lib::create_managed_lua_table_map(L);
        assert(nullptr != table_value.value.table_value);
        do
        {
            if (token_parser->eof())
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            token = token_parser->current();
            token_parser->next();
            if (token.type != TOKEN_RESERVED::LTK_STRING)
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            auto prop_key = token.token;
            if (token_parser->eof())
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            token = token_parser->current();
            token_parser->next();
            if (token.type != ':')
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            if (token_parser->eof())
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            bool sub_result = false;
            auto sub_value = tokens_to_lua_value(L, token_parser, &sub_result);
            if (!sub_result)
            {
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            table_value.value.table_value->erase(prop_key);
            (*table_value.value.table_value)[prop_key] = sub_value;
            if (token_parser->eof())
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            token = token_parser->current();
            token_parser->next();
            if (token.type == ',')
            {
                // go on read the table content
            }
            else if (token.type == '}')
            {
                // end of table content
                break;
            }
            else
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
        } while (true);
        if (nullptr != result)
            *result = true;
        return table_value;
    }break;
    case '[':
    {
        if (token_parser->eof())
        {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
            if (nullptr != result)
                *result = false;
            return nil_storage_value();
        }
        if (token_parser->current().type == ']')
        {
            token_parser->next();
            GluaStorageValue value;
            value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_unknown_array;
            value.value.table_value = thinkyoung::lua::lib::create_managed_lua_table_map(L);
            assert(nullptr != value.value.table_value);
            if (nullptr != result)
                *result = true;
            return value;
        }
        GluaStorageValue table_value;
        table_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_unknown_array; // FIXME: 根据子项类型修改
        table_value.value.table_value = thinkyoung::lua::lib::create_managed_lua_table_map(L);
        assert(nullptr != table_value.value.table_value);
        size_t count = 0;
        do
        {
            if (token_parser->eof())
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            bool sub_result = false;
            auto sub_value = tokens_to_lua_value(L, token_parser, &sub_result);
            if (!sub_result)
            {
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            auto prop_key = std::to_string(++count);
            table_value.value.table_value->erase(prop_key);
            (*table_value.value.table_value)[prop_key] = sub_value;
            if (token_parser->eof())
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
            token = token_parser->current();
            token_parser->next();
            if (token.type == ',')
            {
                // go on read the array content
            }
            else if (token.type == ']')
            {
                // end of table content
                break;
            }
            else
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "parse json error(unknown token %s)", token.token.c_str());
                if (nullptr != result)
                    *result = false;
                return nil_storage_value();
            }
        } while (true);
        if (nullptr != result)
            *result = true;
        return table_value;
    } break;

    default:
    {
        bool result = false;
        auto value = tokens_to_lua_value(L, token_parser, &result);
        if (!result)
            return nil_storage_value();
        return value; // FIXME: this not check the matched { and [ and whether there are more chars after the token
    }
    }
}

static int json_to_lua(lua_State *L)
{
    if (lua_gettop(L) < 1)
        return 0;
    if (!lua_isstring(L, 1))
        return 0;
    auto json_str = luaL_checkstring(L, 1);
    thinkyoung::lua::lib::GluaStateScope scope;
    auto token_parser = std::make_shared<GluaTokenParser>(scope.L());
    token_parser->parse(std::string(json_str));
    token_parser->reset_position();
    bool result = false;
    auto value = tokens_to_lua_value(L, token_parser.get(), &result);
    if (!result)
    {
        return 0;
    }
    lua_push_storage_value(L, value);
    return 1;
}

static int lua_to_json(lua_State *L)
{
    if (lua_gettop(L) < 1)
        return 0;
    auto value = luaL_tojsonstring(L, 1, nullptr);
    lua_pushstring(L, value);
    return 1;
}

static const luaL_Reg dblib[] = {
    { "loads", json_to_lua },
    { "dumps", lua_to_json },
    { nullptr, nullptr }
};


LUAMOD_API int luaopen_json(lua_State *L) {
    luaL_newlib(L, dblib);
    return 1;
}

