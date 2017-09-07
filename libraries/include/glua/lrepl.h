#ifndef lrepl_h
#define lrepl_h

#include <string.h>
#include <string>
#include <list>

#include "glua/lua.h"

/*
** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
** print any results.
*/
void luaL_doREPL(lua_State *L);

#endif