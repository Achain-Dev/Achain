// should be included after lua*.h
#ifndef glua_compat_h
#define glua_compat_h

#include "lua.h"

#define lua_open()	luaL_newstate()
#define luadec_freearray(L, b, n, t) luaM_freearray(L, b, n)

#define UPVAL_TYPE Upvaldesc
#define NUPS(f) (f->sizeupvalues)
#define UPVAL_NAME(f, r) (f->upvalues[r].name)

#define LUADEC_TFORLOOP OP_TFORCALL
#define FUNC_BLOCK_END(f) (f->sizecode)

// Lua >= 5.2 : is_vararg = 0 1 , never use parament arg, but main has a global arg
#define NEED_ARG(f) 0


#define rawtsvalue(o) tsvalue(o)

#ifdef tsslen
#define LUA_STRLEN(ts) tsslen(ts)
#else
#define LUA_STRLEN(ts) ((ts)->len)
#endif

#define MAXREGS 250
#define MAXSTACK MAXREGS

#endif // #ifndef LUADEC_LUA_COMPAT_H
