// #include <vld.h>
#include <glua/lprefix.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <cstdint>
#include <fstream>
#include <cassert>
#include <iostream>
#include <ctime>
#include <chrono>


#include <glua/lua.h>
#include <glua/lvm.h>
#include <glua/lapi.h>
#include <glua/lauxlib.h>

#include <glua/thinkyoung_lua_lib.h>
#include <glua/lcompile.h>
#include <glua/glua_tokenparser.h>
#include <glua/lparsercombinator.h>
#include <glua/ltypechecker.h>
#include <thinkyoung_lua_api.demo.h>

using thinkyoung::lua::api::global_glua_chain_api;

#define BOOST_TEST_MAIN
#define BOOST_TEST_STATIC_LINK
#define BOOST_TEST_MODULE GLUA_TEST

#include <UnitTest++/UnitTest++.h>
//#include <boost/test/unit_test.hpp>
//#include <boost/test/unit_test_log.hpp>

//using namespace boost::unit_test;

//#define GTEST  BOOST_AUTO_TEST_CASE
//#define GCHECK BOOST_CHECK
//#define GCHECK_EQUAL BOOST_CHECK_EQUAL

#define GTEST TEST
#define GCHECK CHECK
#define GCHECK_EQUAL CHECK_EQUAL

// lua_State* L;
int add(lua_State* L);

static size_t add_count = 0;

int add(lua_State* L)
{
	add_count += 1;
	//// fetch number from lua stack of index 1(lua index starts from 1)  
	double x = luaL_checknumber(L, 1);
	double y = luaL_checknumber(L, 2);
	double result = x + y;
	printf("result:%f2\n", result);
	lua_pushnumber(L, (lua_Number)(result));
	return 1;
}

static int say_hello(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	printf("hello, %s\n", name);
	return 1;
}

static int test_error(lua_State *L)
{
	// thinkyoung_api_lua_throw_exception(L, 100, "test error happen");
	return 0;
}

int unit_test_check_equal_number(lua_State *L)
{
	double x = luaL_checknumber(L, 1);
	double y = luaL_checknumber(L, 2);
	if (std::abs(x - y) < 0.00001)
	{
		return 1;
	}
	assert(x == y);
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "check equal error happen");
	return 0;
}

static const luaL_Reg hello_lib[] = {
	{"say_hello", say_hello},
	{"test_error", test_error},
	{"check_equal", unit_test_check_equal_number},
	{nullptr, nullptr}
};

LUALIB_API int luaopen_hello(lua_State *L)
{
	luaL_newlib(L, hello_lib);
	return 1;
}

/**
 * @return TRUE(1 or not 0) if compile and run successfully
 */
static bool compile_and_run(lua_State *L, std::string src_filename)
{
	thinkyoung::lua::lib::add_system_extra_libs(L);
	int compile_status = thinkyoung::lua::lib::compilefile_to_file(src_filename.c_str(), "tmp_to_run", nullptr);
	if (!compile_status)
	{
		printf("compile %s error\n", src_filename.c_str());
		return false;
	}
    global_glua_chain_api->clear_exceptions(L);
	const char *src = "tmp_to_run";
	bool run_status = thinkyoung::lua::lib::run_compiledfile(L, src);
	return run_status && !global_glua_chain_api->has_exception(L);
}

using namespace glua::parser;

/**
* @return TRUE(1 or not 0) if compile and run successfully, with no bytecode file generated in filesystem
*/
static int compile_and_run_with_no_file(lua_State *L, std::string src_filename,
	bool use_type_check=false, std::vector<std::string> *errors=nullptr)
{	 
	thinkyoung::lua::lib::add_system_extra_libs(L);
	if (use_type_check)
	{
		ParserContext ctx;
		ctx.generate_glua_parser();
		auto token_parser = std::make_shared<GluaTokenParser>(L);
		std::ifstream in2(src_filename);
		std::string code2((std::istreambuf_iterator<char>(in2)),
			(std::istreambuf_iterator<char>()));
		in2.close();
		ctx.set_inner_lib_code_lines(7);
		token_parser->parse(thinkyoung::lua::lib::get_typed_lua_lib_code() + code2);
		auto input_to_parse = glua::parser::Input(token_parser.get());
		auto pr = ctx.parse_syntax(input_to_parse);
		if (pr.state() != ParserState::SUCCESS)
		{
			std::cout << "parse syntax error " << ctx.get_error() << std::endl;
			if (errors && ctx.get_error().length()>0)
				errors->push_back(ctx.get_error());
			if (errors && ctx.simple_token_error().length() > 0)
				errors->push_back(ctx.simple_token_error());
			return 0;
		}
		GluaTypeChecker type_checker(&ctx);
		if (!type_checker.check_syntax_tree_type(pr.result()) || type_checker.has_error())
		{
			const char *error_str = "syntax tree type check error: ";
			std::cout << error_str << "\n";
			for (const auto & item : type_checker.errors())
			{
				std::cout << item.second << "\n";
				if (nullptr != errors)
					errors->push_back(item.second);
			}
			return 0;
		}
		for(const auto &event_name : type_checker.get_emit_event_types())
		{
			std::cout << "emit event " << event_name << std::endl;
		}
	}

	auto stream = std::make_shared<GluaModuleByteStream>();
	char error[LUA_COMPILE_ERROR_MAX_LENGTH+1];
	memset(error, 0x0, LUA_COMPILE_ERROR_MAX_LENGTH * sizeof(char));
	if (!thinkyoung::lua::lib::compilefile_to_stream(L, src_filename.c_str(), stream.get(), error, use_type_check))
	{
		printf("compile %s error\n", src_filename.c_str());
		if (strlen(error) > 0)
		{
			perror(error);
			lua_set_compile_error(L, error);
		}
        // thinkyoung::lua::lib::free_bytecode_stream(stream);
		return 0;
	}
    global_glua_chain_api->clear_exceptions(L);
	bool run_status = thinkyoung::lua::lib::run_compiled_bytestream(L, stream.get());
	// thinkyoung::lua::lib::free_bytecode_stream(stream);
	if(errors && strlen(L->runerror)>0)
	{
		errors->push_back(std::string(L->runerror));
	}
	return run_status && !global_glua_chain_api->has_exception(L);
}

static bool compile_and_run_contract_with_no_file(lua_State *L, std::string src_filename,
	bool use_type_check = false, std::vector<std::string> *errors = nullptr)
{
	GluaTypeInfoP contract_storage_type = nullptr;
	GluaTypeInfoP contract_type = nullptr;

	auto stream = std::make_shared<GluaModuleByteStream>();

	if (use_type_check)
	{
		ParserContext ctx;
		ctx.generate_glua_parser();
		auto token_parser = std::make_shared<GluaTokenParser>(L);
		std::ifstream in2(src_filename);
		std::string code2((std::istreambuf_iterator<char>(in2)),
			(std::istreambuf_iterator<char>()));
		in2.close();
		ctx.set_inner_lib_code_lines(7);
		token_parser->parse(thinkyoung::lua::lib::get_typed_lua_lib_code() + code2);
		auto input_to_parse = glua::parser::Input(token_parser.get());
		auto pr = ctx.parse_syntax(input_to_parse);
		if (pr.state() != ParserState::SUCCESS)
		{
			std::cout << "parse syntax error " << ctx.get_error() << std::endl;
			if (nullptr != errors && ctx.get_error().length()>0)
				errors->push_back(ctx.get_error());
			if (errors && ctx.simple_token_error().length() > 0)
				errors->push_back(ctx.simple_token_error());
			return 0;
		}
		GluaTypeChecker type_checker(&ctx);
		bool has_errors = !type_checker.check_contract_syntax_tree_type(pr.result(), &contract_type, &contract_storage_type) || type_checker.has_error();
		for (const auto &p : type_checker.get_imported_contracts())
		{
			if (!global_glua_chain_api->check_contract_exist(L, p.first.c_str()))
			{
				auto error_str = std::string("import_contract error in line ") + std::to_string(p.second) + ", contract " + p.first + " not found";
				std::cout << error_str << std::endl;
				if (errors)
					errors->push_back(error_str);
			}
		}
		if (has_errors)
		{
			const char *error_str = "syntax tree type check error: ";
			std::cout << error_str << "\n";
			for (const auto & item : type_checker.errors())
			{
				std::cout << item.second << "\n";
				if (errors)
					errors->push_back(item.second);
			}
			return 0;
		}

		for (const auto &event_name : type_checker.get_emit_event_types())
		{
			std::cout << "emit event " << event_name << std::endl;
		}
		if (contract_storage_type)
		{
			try
			{
				contract_storage_type->put_contract_storage_type_to_module_stream(stream.get());
				contract_storage_type->put_contract_apis_info_to_module_stream(stream.get());
			}
			catch (glua::core::GluaException const &e)
			{
				if (errors)
					errors->push_back(e.what());
				return false;
			}
		}

	}

	char error[LUA_COMPILE_ERROR_MAX_LENGTH + 1];
	memset(error, 0x0, sizeof(char) * (LUA_COMPILE_ERROR_MAX_LENGTH + 1));

	if (!thinkyoung::lua::lib::compile_contract_to_stream(src_filename.c_str(), stream.get(), error, nullptr, use_type_check))
	{
		printf("compile %s error %s\n", src_filename.c_str(), error);
		if (strlen(error) > 0 && errors)
			errors->push_back(error);
		return false;
	}
    global_glua_chain_api->clear_exceptions(L);
	bool run_status = thinkyoung::lua::lib::run_compiled_bytestream(L, stream.get());
	return run_status && !global_glua_chain_api->has_exception(L);
}

static bool compile_and_run_contract_with_no_file_and_init(lua_State *L, std::string src_filename
	, bool run_start = false, bool use_type_check = false, std::vector<std::string> *errors = nullptr)
{
	GluaTypeInfoP contract_storage_type = nullptr;
	GluaTypeInfoP contract_type = nullptr;
	if (use_type_check)
	{
		ParserContext ctx;
		ctx.generate_glua_parser();
		auto token_parser = std::make_shared<GluaTokenParser>(L);
		std::ifstream in2(src_filename);
		std::string code2((std::istreambuf_iterator<char>(in2)),
			(std::istreambuf_iterator<char>()));
		in2.close();
		ctx.set_inner_lib_code_lines(7);
		token_parser->parse(thinkyoung::lua::lib::get_typed_lua_lib_code() + code2);
		auto input_to_parse = glua::parser::Input(token_parser.get());
		auto pr = ctx.parse_syntax(input_to_parse);
		if (pr.state() != ParserState::SUCCESS)
		{
			std::cout << "parse syntax error " << ctx.get_error() << std::endl;
			if (nullptr != errors && ctx.get_error().length()>0)
				errors->push_back(ctx.get_error());
			if (errors && ctx.simple_token_error().length() > 0)
				errors->push_back(ctx.simple_token_error());
			return 0;
		}
		GluaTypeChecker type_checker(&ctx);
		if (!type_checker.check_contract_syntax_tree_type(pr.result(), &contract_type, &contract_storage_type) || type_checker.has_error())
		{
			const char *error_str = "syntax tree type check error: ";
			std::cout << error_str << "\n";
			for (const auto & item : type_checker.errors())
			{
				std::cout << item.second << "\n";
				if (errors)
					errors->push_back(item.second);
			}
			return 0;
		}
		for (const auto &event_name : type_checker.get_emit_event_types())
		{
			std::cout << "emit event " << event_name << std::endl;
		}
	}

	auto stream = std::make_shared<GluaModuleByteStream>();
	if (nullptr == stream)
		return false;
	if (!thinkyoung::lua::lib::compile_contract_to_stream(src_filename.c_str(), stream.get(), nullptr, nullptr, use_type_check))
	{
		printf("compile %s error\n", src_filename.c_str());
		// thinkyoung::lua::lib::free_bytecode_stream(stream);
		return false;
	}
    global_glua_chain_api->clear_exceptions(L);
	bool run_status = thinkyoung::lua::lib::run_compiled_bytestream(L, stream.get());
	thinkyoung::lua::lib::execute_contract_init(L, "tmp", stream.get(), "abcd", nullptr);
	if (run_start)
	{
		intptr_t pt = (intptr_t)stream.get();
		std::string name = STREAM_CONTRACT_PREFIX + std::to_string(pt);
		thinkyoung::lua::lib::GluaStateScope scope2;
		scope2.L()->debugger_pausing = L->debugger_pausing;
		std::string result_json_string;
		thinkyoung::lua::lib::execute_contract_api(L, name.c_str(), "start", "abcd", &result_json_string);
		std::cout << "api return: " << result_json_string << std::endl;
	}
	// thinkyoung::lua::lib::free_bytecode_stream(stream);
	return run_status && !global_glua_chain_api->has_exception(L);
}


//BOOST_AUTO_TEST_SUITE(TestOfGlua)

/*
GTEST(TEST_MATH_FLOOR)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	std::string filename("tests_lua/test_upval.lua");
	GCHECK(!compile_and_run_contract_with_no_file_and_init(scope.L(), filename));
	thinkyoung::lua::lib::compilefile_to_file(filename.c_str(), "test_upval.out", nullptr, false);
	LClosure *closure = thinkyoung::lua::lib::luaU_undump_from_file(L, "test_upval.out", "test_upval");
	GCHECK(closure != nullptr);
	FILE *tmp = fopen("test_upval.bytes.out", "w");
	luaU_fprint(tmp, closure->p, 1);
	fclose(tmp);
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(scope.L()));
}
*/

#if defined(_DEBUG)
#define CONTRACT_API_PERFORMANCE_TEST_REPEAT_COUNT 10
#else
#define CONTRACT_API_PERFORMANCE_TEST_REPEAT_COUNT 10000
#endif

// 测试调用合约API操作的glua部分的性能
GTEST(TEST_GLUA_CALL_CONTRACT_API_PERFORMANCE)
{
	printf("TEST_GLUA_CALL_CONTRACT_API_PERFORMANCE\n");
	int repeat_count = CONTRACT_API_PERFORMANCE_TEST_REPEAT_COUNT;
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	auto stream = std::make_shared<GluaModuleByteStream>();
	char compile_error[LUA_COMPILE_ERROR_MAX_LENGTH];
	memset(compile_error, 0x0, sizeof(char) * LUA_COMPILE_ERROR_MAX_LENGTH);
	CHECK(thinkyoung::lua::lib::compile_contract_to_stream("tests_typed/empty_correct_contract.glua",
		stream.get(), compile_error, nullptr, true));
	auto bytes_file = fopen("empty_correct_contract.out", "w");
	if(bytes_file)
	{
		fwrite(stream->buff.data(), sizeof(char), stream->buff.size(), bytes_file);
		fclose(bytes_file);
	}
	auto start_time = std::chrono::system_clock::now();
	for (int i = 0; i<repeat_count; ++i)
	{
		thinkyoung::lua::lib::GluaStateScope scope_for_run;
		auto address = std::string(STREAM_CONTRACT_PREFIX) + std::to_string((intptr_t)stream.get());
		if (LUA_OK != thinkyoung::lua::lib::execute_contract_api(scope_for_run.L(), address.c_str(), "start", "testarg", nullptr))
		{
			printf("error when call contract api\n");
		}
	}
	auto end_time = std::chrono::system_clock::now();
	std::chrono::duration<double> diff = end_time - start_time;
	auto using_time = diff.count();
	std::cout << "running " << repeat_count << " times call contract api directly using " << using_time << "s" << std::endl;
	std::cout << "each call using time " << (using_time / repeat_count) << "s" << std::endl;
	printf("end TEST_GLUA_CALL_CONTRACT_API_PERFORMANCE\n executed lua instructions count %d\n", scope.get_instructions_executed_count());
}


GTEST(TEST_C_FUNCTION)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	thinkyoung::lua::lib::add_global_c_function(L, "ADD", add);
	GCHECK(compile_and_run(L, "tests_lua/test_c_function.lua") == true);
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_C_MODULE_INJECT)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	thinkyoung::lua::lib::register_module(L, "hello", luaopen_hello);
	GCHECK(compile_and_run(L, "tests_lua/test_c_module_inject.lua"));
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_STORAGE_NOT_INIT_CONTRACT)
{
  printf("TEST_STORAGE_NOT_INIT_CONTRACT\n");
  thinkyoung::lua::lib::GluaStateScope scope;
  std::vector<std::string> errors;
  GCHECK(!compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_storage_not_init_contract.glua", false, true, &errors));
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_QUICK_STORAGE_CHANGE)
{
  thinkyoung::lua::lib::GluaStateScope scope;
  GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_quick_storage_change.glua", false, true));
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}


GTEST(TEST_CONTRACT_TRANSFER)
{
  thinkyoung::lua::lib::GluaStateScope scope;
  thinkyoung::lua::lib::compilefile_to_file("thinkyoung_lua_modules/thinkyoung_contract_contract_transfer_demo.lua", "thinkyoung_lua_modules/thinkyoung_contract_contract_transfer_demo", nullptr);
  GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_lua/test_contract_transfer.lua", false));
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}


GTEST(TEST_TOSTRING)
{
  printf("TEST_TOSTRING\n");
  thinkyoung::lua::lib::GluaStateScope scope;
  GCHECK(compile_and_run_with_no_file(scope.L(), "tests_lua/test_tostring.lua"));
  GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "b") == "table: 0");
  GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "c") == "{\"b\":\"userdata\",\"c\":{\"a\":1,\"b\":\"hi\"},\"name\":1}");
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_STORAGE_PERFORMANCE)
{
  printf("TEST_STORAGE_PERFORMANCE\n");
  thinkyoung::lua::lib::GluaStateScope scope;
  std::vector<std::string> errors;
  int i;
  //std::cin >> i;
  auto filename = "tests_typed/test_storage_performance.glua";
  GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), filename, true, true, &errors));
  auto stream = std::make_shared<GluaModuleByteStream>();
  thinkyoung::lua::lib::compile_contract_to_stream(filename, stream.get(), nullptr, nullptr, true);
  glua::util::TimeDiff time_diff(true);
  thinkyoung::lua::lib::GluaStateScope scope1;
  thinkyoung::lua::lib::run_compiled_bytestream(scope1.L(), stream.get());
  thinkyoung::lua::lib::execute_contract_init(scope1.L(), "tmp", stream.get(), "abcd", nullptr);

  intptr_t pt = (intptr_t)stream.get();
  std::string name = STREAM_CONTRACT_PREFIX + std::to_string(pt);
  // thinkyoung::lua::lib::GluaStateScope scope2;
  // thinkyoung::lua::lib::run_compiled_bytestream(scope2.L(), stream.get());
  thinkyoung::lua::lib::execute_contract_api_by_stream(scope1.L(), stream.get(), "start", "abcd", nullptr);
  
  time_diff.end();
  std::cout << "compile and run test_storage_performance use " << time_diff.diff_timestamp() << "s" << std::endl;
  GCHECK_EQUAL(errors.size(), 0);
  // std::cin >> i;
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}


GTEST(TEST_TYPED_FOR_VARIABLE_1)
{
  printf("TEST_TYPED_FOR_VARIABLE_1\n");
  thinkyoung::lua::lib::GluaStateScope scope;
  std::vector<std::string> errors;
  GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_for_variable1.glua", true, &errors));
  GCHECK(errors.size() == 4);
  GCHECK(errors[0].find("for step statement's expressions after `=` keyword must be int/number type") != std::string::npos);
  GCHECK(errors[1].find("variable expect string but got int") != std::string::npos);
  GCHECK(errors[2].find("token k, call .. with wrong type args at line , expected string") != std::string::npos);
  GCHECK(errors[3].find("token user_info, for range statement's first expression after `in` keyword must be function or object type") != std::string::npos);
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TEST_MARKET)
{
	printf("TEST_TYPED_TEST_MARKET\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
    std::string src_filename("tests_typed/test_market.lua");
	GCHECK(compile_and_run_contract_with_no_file(scope.L(), src_filename, true, &errors));
	GCHECK_EQUAL(errors.size(), 0);

    // test compile time
    std::ifstream in2(src_filename);
    std::string code2((std::istreambuf_iterator<char>(in2)),
      (std::istreambuf_iterator<char>()));
    in2.close();

    glua::util::TimeDiff time_diff;
    time_diff.start();
    size_t cycle_count = 1; // 重复次数
    for (size_t i = 0; i < cycle_count; ++i)
    {
      auto L = scope.L();
      GluaTypeInfoP contract_storage_type = nullptr;
      GluaTypeInfoP contract_type = nullptr;

      auto stream = std::make_shared<GluaModuleByteStream>();
      ParserContext ctx;
      ctx.generate_glua_parser();
      auto token_parser = std::make_shared<GluaTokenParser>(L);
      ctx.set_inner_lib_code_lines(7);
      token_parser->parse(thinkyoung::lua::lib::get_typed_lua_lib_code() + code2);
      auto input_to_parse = glua::parser::Input(token_parser.get());
      auto pr = ctx.parse_syntax(input_to_parse);
      GCHECK(pr.state() == ParserState::SUCCESS);
      GluaTypeChecker type_checker(&ctx);
      bool has_errors = !type_checker.check_contract_syntax_tree_type(pr.result(), &contract_type, &contract_storage_type) || type_checker.has_error();

      for (const auto &event_name : type_checker.get_emit_event_types())
      {
        std::cout << "emit event " << event_name << std::endl;
      }
      if (contract_storage_type)
      {
        try
        {
          contract_storage_type->put_contract_storage_type_to_module_stream(stream.get());
          contract_storage_type->put_contract_apis_info_to_module_stream(stream.get());
        }
        catch (glua::core::GluaException const &)
        {
        }
      }

      char error[LUA_COMPILE_ERROR_MAX_LENGTH + 1];
      memset(error, 0x0, sizeof(char) * (LUA_COMPILE_ERROR_MAX_LENGTH + 1));

      if (!thinkyoung::lua::lib::compile_contract_to_stream(src_filename.c_str(), stream.get(), error, nullptr, true))
      {
      }
    }

    time_diff.end();
    auto using_time = time_diff.diff_timestamp();
    std::cout << "running " << cycle_count << " times compile contract using " << using_time << "s" << std::endl;
    std::cout << "each call using time " << (using_time / cycle_count) << "s" << std::endl;
    // int i;
    // std::cin >> i;
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TOJSONSTRING)
{
	printf("TEST_TYPED_TOJSONSTRING\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_tojsonstring.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "t1") == "[1,2,\"abc\",{\"age\":24,\"name\":\"hello\"}]");
	GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "t2") == "{\"m\":234,\"n\":123,\"ab\":1}");
	GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "t3") == "[1,3,6,5]");
	GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "t5") == "{\"2\":123}");
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_ERROR_CHECK)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	lua_pushcfunction(L, test_error);
	lua_setglobal(L, "test_error");
	thinkyoung::lua::lib::add_global_c_function(L, "test_error", test_error);
	if (compile_and_run(L, "tests_lua/test_error.lua"))
	{
		perror("error in test error check case\n");
		GCHECK(0);
	}
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_GET_GLOBAL_TABLE)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	GCHECK(compile_and_run(L, "tests_lua/test_get_global_table.lua"));

	lua_getglobal(L, "env2");
	int it = lua_gettop(L);
	lua_pushnil(L);
	printf("Enum %s:", "ENV");
	int keys_count = 0;
	while (lua_next(L, it))
	{
		printf(" %s:  %d", (char *)lua_tostring(L, -2), (int)lua_tonumber(L, -1));
		lua_pop(L, 1);
		keys_count += 1;
	}
	printf("\n");
	lua_pop(L, 1);
	GCHECK(keys_count == 2);
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	printf("_G size: %d\n", luaL_count_global_variables(L));
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_FOR_LOOP_GOTO)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	if (!compile_and_run(L, "tests_lua/test_for_loop_goto.lua"))
	{
		perror("error in test for loop case\n");
		GCHECK(0);
	}
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	thinkyoung::lua::lib::close_lua_state(L);
}
	  
GTEST(TEST_ERROR_SYNTAX)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	int compile_status = thinkyoung::lua::lib::compilefile_to_file("tests_lua/test_error_syntax.lua", "error_syntax.out", nullptr);
	if (compile_status)
	{
		perror("the file is syntax error, while compile successfully. It's wrong!");
		GCHECK(0);
	}
}	

GTEST(TEST_LOAD_CONTRACT)
{
	printf("test load contract case\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	if (!compile_and_run_with_no_file(scope.L(), "tests_lua/test_load_contract.lua"))
	{
		perror("error in test load contract case\n");
		GCHECK(0);
	}
	thinkyoung::lua::lib::execute_contract_api(scope.L(), "demo2", "start", "thinkyoung", nullptr);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_NO_RETURN_CONTRACT)
{
	printf("TEST_NO_RETURN_CONTRACT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_lua/test_no_return_contract.lua", false, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("this thinkyoung contract not return a table") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_EXIT_CONTRACT)
{
	printf("TEST_EXIT_CONTRACT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_typed_exit_contract.glua", true, true, &errors));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_UNDUMP_BYTECODE)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	FILE *tmp = fopen("test4.bytes.out", "w");
	CHECK(thinkyoung::lua::lib::undump_from_bytecode_file_to_file(L, "test4.out", tmp));
	fclose(tmp);
	char decompile_error[LUA_VM_EXCEPTION_STRNG_MAX_LENGTH];
	memset(decompile_error, 0x0, sizeof(char) * LUA_VM_EXCEPTION_STRNG_MAX_LENGTH);
	auto decompiled_result = thinkyoung::lua::lib::decompile_bytecode_file_to_source_code(L, "test4.out", "test4", decompile_error);
	printf("decompiled result is:\n%s\n", decompiled_result.c_str());
	auto disassemble_result = thinkyoung::lua::lib::disassemble_bytecode_file(L, "test4.out", "test4", nullptr);
	printf("disassemble result is:\n%s\n", disassemble_result.c_str());
	GCHECK(decompiled_result.find("M.start = function(self)") != std::string::npos);
	GCHECK(disassemble_result.find("CLOSURE   R4 1         ; R4 := closure(Function #tmp_1)") != std::string::npos);
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_COMPILE_AND_RUN_BYTESTREAM)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	GCHECK(compile_and_run_contract_with_no_file(L, "tests_lua/test4.lua"));
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_COMPILE_CONTRACT)
{
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	GCHECK(compile_and_run_contract_with_no_file(L, "tests_lua/test4.lua"));
	GCHECK(!compile_and_run_contract_with_no_file(L, "tests_lua/test_wrong_contract.lua"));
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_EXECUTE_CONTRACT_API_BY_ADDRESS)
{
	printf("execute contract api by address test\n");
    thinkyoung::lua::lib::GluaStateScope scope;
    // thinkyoung::lua::lib::execute_contract_api_by_address(scope.L(), "1234", "start", "abc");
}

GTEST(TEST_CHECK_CONTRACT_BYTECODE)
{
	printf("test check contract bytecode case\n");
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	LClosure *closure = thinkyoung::lua::lib::luaU_undump_from_file(L, "test4.out", "test4");
	GCHECK(closure != nullptr);
	FILE *tmp = fopen("test4.bytes.out", "w");
	luaU_fprint(tmp, closure->p, 1);
	fclose(tmp);
	if (!thinkyoung::lua::lib::check_contract_proto(L, closure->p))
	{
		printf("this contract bytecode is wrong\n");
	}
	else
	{
		printf("this contract bytecode is right\n");
	}
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_STOP_VM)
{
	printf("test stop vm case\n");
	lua_State *L = thinkyoung::lua::lib::create_lua_state();
	compile_and_run(L, "tests_lua/test_stop_vm.lua");
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	GCHECK(thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L) < 50);
	thinkyoung::lua::lib::close_lua_state(L);
}

GTEST(TEST_STORAGE)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	auto stream = std::make_shared<GluaModuleByteStream>();
	thinkyoung::lua::lib::compile_contract_to_stream("tests_typed/test_storage.glua", stream.get(), nullptr, nullptr, true);
	// GCHECK(stream->offline_apis.size() == 1 && stream->offline_apis[0] == "start");
	// thinkyoung::lua::lib::free_bytecode_stream(stream);
	GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_storage.glua", true, true));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
	thinkyoung::lua::lib::commit_storage_changes(scope.L());
}  

GTEST(TEST_DEBUGGER)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	thinkyoung::lua::lib::GluaStateScope repl_scope;
	bool run_repl = false;
	if (run_repl)
	{
		repl_scope.start_repl_async();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	}
	scope.L()->debugger_pausing = run_repl;
	GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_lua/test_debugger.lua", true));
	
	//aut stream = std::make_shared<GluaModuleByteStream>();
	//thinkyoung::lua::lib::compilefile_to_file("tests_lua/test_debugger.lua", "test_debugger.bc", nullptr);
	//LClosure *cl = thinkyoung::lua::lib::luaU_undump_from_file(scope.L(), "test_debugger.bc", "test_debugger");
	//FILE *f = fopen("test_debugger.bct", "w");
	//luaL_PrintFunctionToFile(f, cl->p, 1);
	//fclose(f);
	//printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
	thinkyoung::lua::lib::commit_storage_changes(scope.L());
}
 
GTEST(TEST_SANDBOX)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	scope.enter_sandbox();
	// CHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_lua/test_storage.lua"));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
	scope.commit_storage_changes();
}

GTEST(TEST_GLOBALS)
{
	{
		thinkyoung::lua::lib::GluaStateScope scope;
		lua_pushinteger(scope.L(), 123);
		lua_setglobal(scope.L(), "test");
		lua_getglobal(scope.L(), "test");
		auto test = lua_tointeger(scope.L(), -1);
	}
	{
		thinkyoung::lua::lib::GluaStateScope scope;
		lua_getglobal(scope.L(), "test");
		auto test = lua_tointeger(scope.L(), -1);
	}
}

GTEST(TEST_CONTRACT_WITHOUT_INIT)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_lua/test_without_init.lua"));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_CHINESE_PATH)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_lua/测试中文.lua", true));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_COMPILE_ERROR)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_lua/test_compile_error.lua")==0);
	if (strlen(scope.L()->compile_error) > 0)
	{
		perror(scope.L()->compile_error);
	}
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_ERROR_USE_API)
{
	thinkyoung::lua::lib::GluaStateScope scope;
	auto result = compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_lua/test_error_use_api.lua", true);
	GCHECK(!result);
	if (strlen(scope.L()->compile_error) > 0)
	{
		perror(scope.L()->compile_error);
	}
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TOO_MANY_LOCALVARS)
{
	printf("TEST_TOO_MANY_LOCALVARS\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	GCHECK(!compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_lua/test_too_many_localvars.lua"));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_API_RETURN)
{
	printf("TEST_API_RETURN\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_api_return.glua", true, true, &errors));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_GOTO)
{
  printf("TEST_TYPED_GOTO\n");
  thinkyoung::lua::lib::GluaStateScope scope;
  std::vector<std::string> errors;
  GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_goto.glua", true, &errors));
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_BREAK)
{
  printf("TEST_TYPED_BREAK\n");
  thinkyoung::lua::lib::GluaStateScope scope;
  std::vector<std::string> errors;
  GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_break.glua", true, &errors));
  printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_MULTILINE_COMMENT_AND_STRING)
{
	printf("TEST_MULTILINE_COMMENT_AND_STRING\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_multiline_comment_and_string.glua", true, &errors));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_DUPLICATE_RETURN)
{
	printf("TEST_DUPLICATE_RETURN\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/duplicate_return.glua", true, &errors));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_IF_STAT)
{
	printf("TEST_TYPED_IF_STAT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_if_stat.glua", true, &errors));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_WRONG_SYNTAX_STRUCTURE1)
{
	printf("TEST_TYPED_WRONG_SYNTAX_STRUCTURE1\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_wrong_syntax_structure1.glua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("error not match after token a") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_NEW_TOKENIZER)
{
	printf("TEST_NEW_TOKENIZER\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	auto token_parser = std::make_shared<glua::parser::GluaTokenParser>(scope.L());
	std::ifstream in("tests_lua/test3.lua");
	std::string code((std::istreambuf_iterator<char>(in)),
		(std::istreambuf_iterator<char>()));
	in.close();
	try
	{
		token_parser->parse(code);
		GCHECK(token_parser->size() == 74);
		printf("test3.lua has %d tokens\n", token_parser->size());

		std::string dumped_code = token_parser->dump();
		// std::cout << dumped_code << std::endl;
		using namespace glua::parser;
		ParserContext ctx;
		ctx.generate_glua_parser();
		// ctx.finish(); // 左递归优化，间接左递归优化等. FIXME: BUG here
		auto token_parser2 = std::make_shared<GluaTokenParser>(scope.L());
		std::ifstream in2("tests_typed/full_demo.lua");
		std::string code2((std::istreambuf_iterator<char>(in2)),
			(std::istreambuf_iterator<char>()));
		in2.close();
		token_parser2->parse(code2);
		// token_parser2.parse(std::string(R"END(
		// (int) => string
		// )END"));
		auto using_token_parser = token_parser2;

		auto input_to_parse = glua::parser::Input(using_token_parser.get());
		auto pr = ctx.parse_syntax(input_to_parse);
		GCHECK(pr.state() == ParserState::SUCCESS);
		GluaTypeChecker type_checker(&ctx);
		bool type_check_result = type_checker.check_syntax_tree_type(pr.result());
		auto proto_type_info_str = type_checker.sprint_root_proto_var_type_infos();
		std::cout << proto_type_info_str << std::endl;
		auto dumped_code_from_type_checker = type_checker.dump();
		FILE *ldf_file = fopen("tests_typed/full_demo.ldf", "w");
		if(ldf_file)
		{
			type_checker.dump_ldf_to_file(ldf_file);
			fclose(ldf_file);
		}

		if (!type_check_result)
		{
			std::cout << "syntax tree type check error: ";
			auto errors = type_checker.errors();
			for (auto &e : errors)
			{
				std::cout << e.second << std::endl;
			}
			std::cout << std::endl;
		}
		printf("");
	}
	catch (std::exception e)
	{
		GCHECK(false);
		perror(e.what());
	}
}

GTEST(TEST_ECHO_HTTP_SERVER)
{
	printf("TEST_ECHO_HTTP_SERVER\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	// GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_echo_http_server.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_LET_VAR)
{
	printf("TEST_TYPED_LET_VAR\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/let_var.lua", true, &errors));
	GCHECK(errors.size() == 3);
	GCHECK(errors.at(0).find("changing variable b that can't be changed") != std::string::npos);
	GCHECK(errors.at(1).find("variable declared as type bool but got int") != std::string::npos);
	GCHECK(errors.at(2).find("token a1, can't declare duplicate variable a1") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_WRONG_BYTECODE)
{
	printf("TEST_WRONG_BYTECODE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	bool run_status = thinkyoung::lua::lib::run_compiledfile(scope.L(), "wrong_byte_code.bytes");
	GCHECK(!run_status);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_INT_NUMBER_PARSE)
{
	printf("TEST_TYPED_INT_NUMBER_PARSE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_int_number_parse.glua", true, &errors));
	GCHECK(errors.size() == 2);
	GCHECK(errors.at(0).find("token a5, declare variable a5 type int but got number") != std::string::npos);
	GCHECK(errors.at(1).find("token f1, function args types are: (int), using args are: (number)") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CORRECT_CONTRACT)
{
	printf("TEST_TYPED_CORRECT_CONTRACT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_correct_contract.glua", true, &errors));
	GCHECK(errors.size() == 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CONTROL_SCOPES)
{
	printf("TEST_TYPED_CONTROL_SCOPES\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_control_scopes.glua", true, &errors));
	GCHECK(errors.size() == 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_MISSING_END)
{
	printf("TEST_TYPED_MISSING_END\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_missing_end.glua", true, &errors));
	// GCHECK(errors.size() == 2);
	GCHECK(errors.at(0).find("maybe missing 'end' at end of file") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_MISSING_RECORD_COLON)
{
	printf("TEST_TYPED_MISSING_RECORD_COLON\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_missing_record_colon.glua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("missing } in line 6 but got address") != std::string::npos);
	GCHECK(errors.at(0).find("not end code or wrong structure of typedef") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_VAR_AND_TYPE_NAMESPACE)
{
	printf("TEST_TYPED_VAR_AND_TYPE_NAMESPACE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_var_and_type_namespace.glua", true, &errors));
	GCHECK(errors.size() == 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_SIMPLE_RECORD)
{
	printf("TEST_TYPED_SIMPLE_RECORD\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/simple_record.lua", true, &errors));

	GCHECK_EQUAL(errors.size(), 4);
	GCHECK(errors.at(0).find("token G1, record need 3 generic types but got 2") != std::string::npos);
	GCHECK(errors.at(1).find("token r3, must fill type generic type G1's type parameters") != std::string::npos);
	GCHECK(errors.at(2).find("type record Person{") != std::string::npos && errors.at(2).find("} can't access property not_found") != std::string::npos);
	GCHECK(errors.at(3).find("type record Person{") && errors.at(3).find("} can't access property not_found2") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_VISIT_PROP)
{
	printf("TEST_TYPED_VISIT_PROP\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/visit_prop.lua", true, &errors));
	GCHECK(errors.size() == 6);
	GCHECK(errors.at(0).find("type record Person{")!=std::string::npos && errors.at(0).find("} can't access property not_found") != std::string::npos);
	GCHECK(errors.at(1).find("type record Person{")!=std::string::npos && errors.at(1).find("} can't access property not_found2") != std::string::npos);
	GCHECK(errors.at(2).find("type Map<object> can't access property p1") != std::string::npos);
	GCHECK(errors.at(3).find("type record Person{")!=std::string::npos && errors.at(3).find("} can't access property 1") != std::string::npos);
	GCHECK(errors.at(4).find("token tbl1, Can't access type Map<object>'s property by nil property") != std::string::npos);
	GCHECK(errors.at(5).find("token tbl1_key2, type Map<object> can't access property tbl1_key2") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_NESTED_RECORD)
{
	printf("TEST_TYPED_NESTED_RECORD\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/nested_record.lua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("type record Person{id,name,age,fn} can't access property not_found") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CORRECT_NESTED_TABLE)
{
	printf("TEST_TYPED_CORRECT_NESTED_TABLE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/correct_nested_table.glua", true, &errors));
	GCHECK(errors.size() == 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_USE_KEYWORD_AS_VARNAME)
{
	printf("TEST_TYPED_USE_KEYWORD_AS_VARNAME\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_use_keyword_as_varname.glua", true, &errors));
	GCHECK(errors.size() == 6);
	GCHECK(errors.at(0).find("token return, can't use keyword 'return' as variable name") != std::string::npos);
	GCHECK(errors.at(1).find("token end, can't use keyword 'end' as variable name") != std::string::npos);
	GCHECK(errors.at(2).find("token do, can't use keyword 'do' as function/property name") != std::string::npos);
	GCHECK(errors.at(3).find("token do, can't use keyword 'do' as variable name") != std::string::npos);
	GCHECK(errors.at(4).find("token M, can't use keyword 'end' as function/property name") != std::string::npos);
	GCHECK(errors.at(5).find("token M, can't use keyword 'repeat' as function/property name") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TABLE_MODULE)
{
	printf("TEST_TYPED_TABLE_MODULE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_table_module.glua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("invalid value (table) at index 4 in table for 'concat'") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CONTRACT_STORAGE_PROPERTY_TYPE)
{
	printf("TEST_TYPED_CONTRACT_STORAGE_PROPERTY_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_contract_storage_property_type.glua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("property error's type error, all storage record type's properties' types must be one of int/number/bool/string/table/record") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CHANGE_CONTRACT_ID)
{
	printf("TEST_TYPED_CHANGE_CONTRACT_ID\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_change_contract_id.glua", true, &errors));
	GCHECK(errors.size() == 2);
	GCHECK(errors.at(0).find("token self, Can't change contract's id property") != std::string::npos);
	GCHECK(errors.at(1).find("token self, Can't change contract's name property") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TEST_STREAM_TYPE)
{
	printf("TEST_TYPED_TEST_STREAM_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_stream_type.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	GCHECK_EQUAL(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "t1"), "4");
	GCHECK_EQUAL(thinkyoung::lua::lib::get_global_string_variable(scope.L(), "t2"), "123979899");
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_SELF_TYPE)
{
	printf("TEST_TYPED_SELF_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/self_type.lua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("token self, Can't access func2 of record Contract<record PersonWithoutFn{")!=std::string::npos && errors.at(0).find("}") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_BIN_AND_UN_OP_EXP)
{
	printf("TEST_TYPED_BIN_AND_UN_OP_EXP\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/bin_and_un_op_type.lua", true, &errors));
	GCHECK(errors.size() == 5);
	GCHECK(errors.at(0).find("call + with wrong type args at line") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_UN_OP_RUN_ERROR1)
{
	printf("TEST_TYPED_UN_OP_RUN_ERROR1\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/un_op_run_error1.lua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("attempt to get length of a nil value (local 'a')") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_UN_OP_RUN_ERROR2)
{
	printf("TEST_TYPED_UN_OP_RUN_ERROR2\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/un_op_run_error2.lua", true, &errors));
	GCHECK(errors.size() == 1);
	GCHECK(errors.at(0).find("attempt to get length of a nil value") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CALCULATE)
{
	printf("TEST_TYPED_CALCULATE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_calculate.glua", true, &errors));
	GCHECK(errors.size() == 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_DENY_CALL_SPECIAL_APIS)
{
	printf("TEST_TYPED_DENY_CALL_SPECIAL_APIS\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_deny_call_special_apis.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 4);
	GCHECK(errors.at(0).find("Can't call init api of contract") != std::string::npos);
	GCHECK(errors.at(1).find("Can't call on_deposit api of contract") != std::string::npos);
	GCHECK(errors.at(2).find("Can't call on_destroy api of contract") != std::string::npos);
	GCHECK(errors.at(3).find("Can't call on_upgrade api of contract") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_NEW_ARRAY_AND_MAP_TYPE)
{
	printf("TEST_TYPED_NEW_ARRAY_AND_MAP_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_new_array_and_map_type.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_WRONG_NEW_ARRAY_AND_MAP_TYPE)
{
	printf("TEST_TYPED_WRONG_NEW_ARRAY_AND_MAP_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_wrong_new_array_and_map_type.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 16);
	GCHECK(errors.at(0).find("token {, Can't put array part in Map") != std::string::npos);
	GCHECK(errors.at(1).find("Can't put array and hashmap items in one table together")!=std::string::npos);
	GCHECK(errors.at(2).find("token b, declare variable b type Map<string> but got Map<object>") != std::string::npos);
	GCHECK(errors.at(3).find("token {, Can't put array part in Map") != std::string::npos);
	GCHECK(errors.at(4).find("token a1, declare variable a1 type record SPerson{")!=std::string::npos
		&& errors.at(4).find("} but got Array<int>") != std::string::npos);
	GCHECK(errors.at(5).find("token a2, declare variable a2 type record Address{country} but got record SPerson{") != std::string::npos);
	GCHECK(errors.at(6).find("token a3, declare variable a3 type Map<string> but got record SPerson{") != std::string::npos);
	GCHECK(errors.at(7).find("token a5, declare variable a5 type Map<string> but got Map<object>") != std::string::npos);
	GCHECK(errors.at(8).find("token a6, declare variable a6 type Map<string> but got Array<string>") != std::string::npos);
	GCHECK(errors.at(9).find("token a7, declare variable a7 type Array<string> but got record SPerson{") != std::string::npos);
	GCHECK(errors.at(10).find("token a9, declare variable a9 type Array<string> but got Array<object>") != std::string::npos);
	GCHECK(errors.at(11).find("token f2, declare variable f2 type string but got int") != std::string::npos);
	GCHECK(errors.at(12).find("token f4, declare variable f4 type int but got string") != std::string::npos);
	GCHECK(errors.at(13).find("token 1, type Map<string> can't access index 1") != std::string::npos);
	GCHECK(errors.at(14).find("token Map, must fill type generic type Map's type parameters") != std::string::npos);
	GCHECK(errors.at(15).find("token f6, must fill type generic type Map's type parameters") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_IMPORT_CONTRACT)
{
	printf("TEST_TYPED_IMPORT_CONTRACT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_import_contract.lua", true, &errors));
	GCHECK(errors.size() == 4);
	GCHECK(errors.at(0).find("contract not_found not found") != std::string::npos);
	GCHECK(errors.at(1).find("contract not_found2 not found") != std::string::npos);
	GCHECK(errors.at(2).find("token demo, Can't call init api of contract") != std::string::npos);
	GCHECK(errors.at(3).find("token demo, Can't access contract's storage property") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_FULL_CORRECT_TYPED)
{
	printf("TEST_TYPED_FULL_CORRECT_TYPED\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	scope.L()->bytecode_debugger_opened = false;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/full_correct_typed.glua", true, &errors));
	GCHECK(errors.size() == 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CALL_WITHOUT_BRACKETS)
{
	printf("TEST_TYPED_CALL_WITHOUT_BRACKETS\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/call_without_brackets.lua", true, &errors));
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CALL_WITHOUT_BRACKETS_ERROR)
{
	printf("TEST_TYPED_CALL_WITHOUT_BRACKETS_ERROR\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/call_without_brackets_error.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("token pprint, function args types are: (object)") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_DECLARE_WITH_TYPE)
{
	printf("TEST_TYPED_DECLARE_WITH_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/declare_with_type.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 3);
	GCHECK(errors.at(0).find("token name, when define record type, property's type declaration is required") != std::string::npos);
	GCHECK(errors.at(1).find("token m4, declare variable m4 type \"Male\" | \"Female\" | true | 123 | 1.23 but got string") != std::string::npos);
	GCHECK(errors.at(2).find("token fm2, function args types are: ('Chinese'), using args are: (bool)") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_FUNCTION_TYPE_DECLARE)
{
	printf("TEST_TYPED_FUNCTION_TYPE_DECLARE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/function_type_declare.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 4);
	GCHECK(errors.at(0).find("call + with wrong type args") != std::string::npos);
	GCHECK(errors.at(1).find("token add5, declare variable add5 type function number (number, number) but got function number (number, object)") != std::string::npos);
	GCHECK(errors.at(2).find("return too more values(can only accept 1 or 0 value)") != std::string::npos);
	GCHECK(errors.at(3).find("token p1, assign statement type error, declare type is function number (number, number) and value type is function number (number, object)") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_FUNCTION_TYPE_AND_CALL)
{
	printf("TEST_TYPED_FUNCTION_TYPE_AND_CALL\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/function_type_and_call.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 5);
	GCHECK(errors.at(0).find("token add1, function args types are: (number, number)") != std::string::npos);
	GCHECK(errors.at(1).find("token add1, function args types are: (number, number)") != std::string::npos);
	GCHECK(errors.at(2).find("token add1, function args types are: (number, number), using args are: (string, string)") != std::string::npos);
	GCHECK(errors.at(3).find("token add1, function args types are: (number, number), using args are: (int, int, int, int)") != std::string::npos);
	GCHECK(errors.at(4).find("token add1, function args types are: (number, number), using args are: (string, string, string, string)") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_NESTED_FUNCTION_DEFINE)
{
	printf("TEST_TYPED_NESTED_FUNCTION_DEFINE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/nested_function_define.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 2);
	GCHECK(errors.at(0).find("token b1, declare variable b1 type number but got string") != std::string::npos);
	GCHECK(errors.at(1).find("token b1, declare variable b1 type string but got number") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_RECORD_CONSTRUCTOR_AND_CALL_AS_FUNC)
{
	printf("TEST_TYPED_RECORD_CONSTRUCTOR_AND_CALL_AS_FUNC\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	// 暂时关闭__call语法
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_record_constructor_and_call_as_func.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("token h2, now syntax of treat record as function is not supported") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TABLE_PROPERTY_FUNCTION_CALL)
{
	printf("TEST_TYPED_TABLE_PROPERTY_FUNCTION_CALL\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_table_property_function_call.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_RECORD_TABLE_PARSE)
{
	printf("TEST_TYPED_RECORD_TABLE_PARSE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/record_table_parse.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_OVER_NUMBER_LIMIT)
{
	printf("TEST_TYPED_OVER_NUMBER_LIMIT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_over_number_limit.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_MULTI_LINES_LAMBDA)
{
	printf("TEST_TYPED_MULTI_LINES_LAMBDA\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	// TODO
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_multi_lines_lambda.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_CALL_CONTRACT_IT_SELF)
{
	printf("TEST_TYPED_CALL_CONTRACT_IT_SELF\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_call_contract_it_self.glua", false, true, &errors));
	GCHECK_EQUAL(errors.size(), 2);
	GCHECK(errors.at(0).find("token self, Can't access contract's init property") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_USE_NOT_INIT_VARIABLE)
{
	printf("TEST_TYPED_USE_NOT_INIT_VARIABLE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_use_not_init_variable.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 2);
	GCHECK(errors.at(0).find("token b, use variable b not inited") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_DEFINE_GLOBAL_IN_CONTRACT)
{
	printf("TEST_TYPED_DEFINE_GLOBAL_IN_CONTRACT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_define_global_in_contract.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 2);
	GCHECK(errors.at(0).find("token a , contract not allow define new variable") != std::string::npos);
	GCHECK(errors.at(1).find("token hello , contract not allow define new variable") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_FUNCTION_TYPE_UP)
{
	printf("TEST_TYPED_FUNCTION_TYPE_UP\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_function_type_up.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("attempt to perform arithmetic on a string value (local 'a')") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_EMIT_EVENTS)
{
	printf("TEST_TYPED_EMIT_EVENTS\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_emit_events.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 2);
	GCHECK(errors.at(0).find("token emit, emit statement argument must be string, but got int") != std::string::npos);
	GCHECK(errors.at(1).find("token emit, emit statement argument must be string, but got int") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_IMPORT_CORRECT_CONTRACT)
{
	printf("TEST_TYPED_IMPORT_CORRECT_CONTRACT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_import_correct_contract.glua", true, true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TOO_MANY_LOCALS)
{
	printf("TEST_TYPED_TOO_MANY_LOCALS\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file_and_init(scope.L(), "tests_typed/test_typed_too_many_locals.glua", true, true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("token function, too many local variables(129 variables), but limit count is 128") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_ARRAY_FETCH)
{
	printf("TEST_TYPED_ARRAY_FETCH\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_array_fetch.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_PAIRS)
{
	printf("TEST_TYPED_PAIRS\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/test_pairs.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	GCHECK(thinkyoung::lua::lib::get_global_string_variable(scope.L(),
		"t1") == "[[100,200],[\"a\",1],[\"m\",234],[\"n\",123],[\"ab\",1]]");
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_RETURN_UNION_TYPE)
{
	printf("TEST_TYPED_RETURN_UNION_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/return_union_type.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("token b, declare variable b type number but got number|string") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_INNER_MODULE_TYPE_INFO)
{
	printf("TEST_TYPED_INNER_MODULE_TYPE_INFO\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/test_inner_module_type_info.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 5);
	GCHECK(errors.at(0).find("token concat, function args types are: (table, string, object), using args are: (int)") != std::string::npos);
	GCHECK(errors.at(1).find("token table, function args types are: (table, object), using args are: ()") != std::string::npos);
	GCHECK(errors.at(2).find("token table, function args types are: (table, object), using args are: ()") != std::string::npos);
	GCHECK(errors.at(3).find("token string, function args types are: (string, string), using args are: (string)") != std::string::npos);
	GCHECK(errors.at(4).find("token b6, declare variable b6 type string but got number") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_GENERIC_TYPE)
{
	printf("TEST_TYPED_GENERIC_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/generic_type.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 4);
	GCHECK(errors.at(0).find("record need 3 generic types but got 2") != std::string::npos);
	GCHECK(errors.at(1).find("token r3, must fill type generic type G1's type parameters") != std::string::npos);
	GCHECK(errors.at(2).find("type record Person{")!=std::string::npos && errors.at(2).find("} can't access property not_found") != std::string::npos);
	GCHECK(errors.at(3).find("type record Person{")!=std::string::npos && errors.at(3).find("} can't access property not_found2") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TYPE_PARSE)
{
	printf("TEST_TYPED_TYPE_PARSE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/type_parse.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	// GCHECK(errors.at(0).find("token a5, function args types are: (number, number), using args are: (int)") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_CONTRACT_RETURN_WRONG_TYPE)
{
	printf("TEST_CONTRACT_RETURN_WRONG_TYPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_contract_return_wrong_type.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("contract must return contract type, but get") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_DEFINE_CONTRACT_API_USING_DOT)
{
	printf("TEST_DEFINE_CONTRACT_API_USING_DOT\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_contract_with_no_file(scope.L(), "tests_typed/test_define_contract_api_using_dot.glua", true, &errors));
	GCHECK_EQUAL(errors.size(), 1);
	GCHECK(errors.at(0).find("token M, should use '<varname>:<funcname>' to define record's member function, don't use '<varname>.<funcname>' when varname is record type") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_TYPEDEF_AND_PARTIAL)
{
	printf("TEST_TYPED_TYPEDEF_AND_PARTIAL\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(!compile_and_run_with_no_file(scope.L(), "tests_typed/typedef_and_partial.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 6);
	GCHECK(errors.at(0).find("token T, Can't find type T") != std::string::npos);
	GCHECK(errors.at(1).find("token G1, Can't use nil or undefined type in typedef") != std::string::npos);
	GCHECK(errors.at(2).find("token G1, record need 3 generic types but got 4") != std::string::npos);
	GCHECK(errors.at(3).find("token G1, record need 3 generic types but got 2") != std::string::npos);
	GCHECK(errors.at(4).find("token T, Can't find type T") != std::string::npos);
	GCHECK(errors.at(5).find("Can't use nil or undefined type in record definition") != std::string::npos);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}

GTEST(TEST_TYPED_VARIABLE_SCOPE)
{
	printf("TEST_TYPED_VARIABLE_SCOPE\n");
	thinkyoung::lua::lib::GluaStateScope scope;
	std::vector<std::string> errors;
	GCHECK(compile_and_run_with_no_file(scope.L(), "tests_typed/variable_scope.lua", true, &errors));
	GCHECK_EQUAL(errors.size(), 0);
	printf("executed lua instructions count %d\n", scope.get_instructions_executed_count());
}


// BOOST_AUTO_TEST_SUITE_END()
 
int simple_tests()
{
	auto L = thinkyoung::lua::lib::create_lua_state();
	
	thinkyoung::lua::lib::register_module(L, "hello", luaopen_hello);

	printf("lua state instructions limit count: %d\n", thinkyoung::lua::lib::get_lua_state_instructions_limit(L));
	thinkyoung::lua::lib::set_lua_state_instructions_limit(L, 100);

	thinkyoung::lua::lib::add_global_c_function(L, "ADD", add);
	thinkyoung::lua::lib::add_global_c_function(L, "test_error", test_error);
	thinkyoung::lua::lib::add_global_c_function(L, "check_equal", unit_test_check_equal_number);


	// luaL_dofile(L, "tests_lua/test2.lua");  

	
	if (luaL_loadfile(L, "tests_lua/test1.lua"))
	{
		printf("error\n");
	}

	lua_pcall(L, 0, 0, 0);
	lua_getglobal(L, "mylua");
	lua_pcall(L, 0, 0, 0); // call stack top function

	int compile_status;
	if (global_glua_chain_api->has_exception(L))
	{
		thinkyoung::lua::lib::close_lua_state(L);
		L = thinkyoung::lua::lib::create_lua_state();
	}
	const char *to_compile_filename = "tests_lua/test3.lua";
	compile_status = thinkyoung::lua::lib::compilefile_to_file("tests_lua/test3.lua", "thinkyoung_lua_modules/thinkyoung_contract_demo2", nullptr);

	auto stream = std::make_shared<GluaModuleByteStream>();					 
	if (thinkyoung::lua::lib::compilefile_to_stream(L, to_compile_filename, stream.get(), nullptr))
	{
		printf("compiled file done\n");
	}
	// thinkyoung::lua::lib::free_bytecode_stream(stream);

	// lua_docompiled_bytestream(L, stream);

	thinkyoung::lua::lib::run_compiledfile(L, "thinkyoung_lua_modules/thinkyoung_contract_demo2");	// may not affect?
	printf("executed compiled file done\n");

	thinkyoung::lua::lib::compilefile_to_file("tests_lua/test4.lua", "test4.out", nullptr);

	printf("hello my lua\n");
	printf("executed lua instructions count %d\n", thinkyoung::lua::lib::get_lua_state_instructions_executed_count(L));
	
	thinkyoung::lua::lib::close_lua_state(L);

	int status = UnitTest::RunAllTests();
	return status;
}

void stop_std(int *savedstd)
{
    savedstd[0] = dup(1); // save original stdout
	dup2(0, 1);
	savedstd[1] = dup(2);
	dup2(0, 2);
}

void resume_std(int *savedstd)
{
    dup2(savedstd[0], 1);   // restore original stdout
	dup2(savedstd[1], 2);
}

int main(int argc, char* argv[])
{
  global_glua_chain_api = new thinkyoung::lua::api::DemoGluaChainApi();

	int status;
	bool test_memory = false;

    int savedstd[2];
	if (test_memory)
	{
		stop_std(savedstd);
	}
	for (auto i = 0; i < (test_memory ? 100 : 1); ++i)
	{
		status = simple_tests();
	}
	if (test_memory)
	{
		resume_std(savedstd);
	}

	if (!test_memory && false)
	{
		thinkyoung::lua::lib::GluaStateScope L_scope;
		L_scope.add_system_extra_libs();
		// L_scope.change_out_file(fopen("REPL.out", "w"));
		// L_scope.change_err_file(fopen("REPL.err", "w"));
		thinkyoung::lua::lib::start_repl_async(L_scope.L());
		while (thinkyoung::lua::lib::check_repl_running(L_scope.L()))
		{
			// you can also use the same lua_State when the REPL running
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
		fclose(L_scope.L()->out);
		fclose(L_scope.L()->err);
	}
	
	getchar();
	return status;
}