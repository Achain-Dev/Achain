cmake_minimum_required(VERSION 2.8)
project(glua)

SET (CMAKE_CXX_COMPILER     "/usr/bin/g++")

# Boost settings
set(BOOST_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(Boost_USE_STATIC_LIBS     ON)
set(Boost_USE_MULTITHREADED   ON)
set(Boost_USE_STATIC_RUNTIME  ON)

# Specify Boost libraries that will use.
set(boost_components regex system thread chrono)
set(boost_components_option --with-system --with-date_time --with-regex)

FIND_PACKAGE( Boost 1.54.0 COMPONENTS ${boost_components} )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

INCLUDE_DIRECTORIES (
	../include
        ../../../fc-20160602formal/include
)

# -lboost_unit_test_framework
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -isystem -Wall -I../include -I /usr/include/boost -L\"/usr/lib/x86_64-linux-gnu/\" -lboost_system -lboost_filesystem -lboost_regex -pthread")

set(SOURCE_FILES
	single_glua_src/main.cpp
        # single_glua_src/cdemo1.cpp
	# thinkyoung_lua_api.cpp
	single_glua_src/thinkyoung_lua_api.demo.cpp
	thinkyoung_lua_lib.cpp
	glua_lutil.cpp
	language_server.cpp
	lapi.cpp
	lauxlib.cpp
	lbaselib.cpp
	lbitlib.cpp
	lcode.cpp
	lcompile.cpp
	lcorolib.cpp
	lctype.cpp
	ldblib.cpp
	ldebug.cpp
	ldo.cpp
	ldump.cpp
	lfunc.cpp
	lgc.cpp
	lthinkyounglib.cpp
	lhashmap.cpp
	lhttplib.cpp
	linit.cpp
	liolib.cpp
	ljsonlib.cpp
	ljsonrpclib.cpp
	llex.cpp
	lmathlib.cpp
	lmem.cpp
	lnetlib.cpp
	loadlib.cpp
	lobject.cpp
	lopcodes.cpp
	loslib.cpp
	lparsercombinator.cpp
	lparser.cpp
	lprefix.cpp
	lremote_debugger.cpp
	lrepl.cpp
	lstate.cpp
	lstring.cpp
	lstrlib.cpp
	ltable.cpp
	ltablib.cpp
	ltimelib.cpp
	ltm.cpp
	ltypechecker.cpp
	ltype_checker_type_info.cpp
	glua_astparser.cpp
	luac.cpp
	lua.cpp
	glua_debug_file.cpp
	glua_loader.cpp
	glua_state_scope.cpp
	glua_tokenparser.cpp
	glua_vm.cpp
	lundump.cpp
	lutf8lib.cpp
	lvm.cpp
	lzio.cpp
	glua_decompile.cpp
	glua_disassemble.cpp
	glua_proto_info.cpp
	glua_statement.cpp
	glua_StringBuffer.cpp
	glua_structs.cpp

	)

add_executable(glua ${SOURCE_FILES})

TARGET_LINK_LIBRARIES( glua ${Boost_PROGRAM_OPTIONS_LIBRARIES}
                      ${Boost_FILESYSTEM_LIBRARIES}
                      ${Boost_SYSTEM_LIBRARIES} )