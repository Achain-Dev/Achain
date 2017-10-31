/*
** $Id: lundump.c,v 2.44 2015/11/02 16:09:30 roberto Exp $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define lundump_cpp
#define LUA_CORE

#include <glua/lprefix.h>


#include <string.h>

#include <glua/lua.h>

#include <glua/ldebug.h>
#include <glua/ldo.h>
#include <glua/lfunc.h>
#include <glua/lmem.h>
#include <glua/lobject.h>
#include <glua/lstring.h>
#include <glua/lundump.h>
#include <glua/lzio.h>
#include <glua/thinkyoung_lua_lib.h>

using thinkyoung::lua::api::global_glua_chain_api;


#if !defined(luai_verifycode)
#define luai_verifycode(L,b,f)  /* empty */
#endif


typedef struct {
    lua_State *L;
    ZIO *Z;
    const char *name;
} LoadState;


static l_noret error(LoadState *S, const char *why) {
    luaO_pushfstring(S->L, "%s: %s precompiled chunk", S->name, why);
	global_glua_chain_api->throw_exception(S->L, THINKYOUNG_API_SIMPLE_ERROR, "%s: %s precompiled chunk", S->name, why);
	// TODO: set error in L struct
    luaD_throw(S->L, LUA_ERRSYNTAX);
}


/*
** All high-level loads go through LoadVector; you can change it to
** adapt to the endianness of the input
*/
#define LoadVector(S,b,n)	LoadBlock(S,b,(n)*sizeof((b)[0]))

static void LoadBlock(LoadState *S, void *b, size_t size) {
    if (luaZ_read(S->Z, b, size) != 0)
        error(S, "truncated");
}


#define LoadVar(S,x)		LoadVector(S,&x,1)


static lu_byte LoadByte(LoadState *S) {
    lu_byte x;
    LoadVar(S, x);
    return x;
}


static int LoadInt(LoadState *S) {
    int x;
    LoadVar(S, x);
    return x;
}


static lua_Number LoadNumber(LoadState *S) {
    lua_Number x;
    LoadVar(S, x);
    return x;
}


static lua_Integer LoadInteger(LoadState *S) {
    lua_Integer x;
    LoadVar(S, x);
    return x;
}


static TString *LoadString(LoadState *S) {
    size_t size = LoadByte(S);
    if (size == 0xFF)
        LoadVar(S, size);
    if (size == 0)
        return nullptr;
    else if (--size <= LUAI_MAXSHORTLEN) {  /* short string? */
        char buff[LUAI_MAXSHORTLEN];
        LoadVector(S, buff, size);
        return luaS_newlstr(S->L, buff, size);
    }
    else {  /* long string */
        TString *ts = luaS_createlngstrobj(S->L, size);
        LoadVector(S, getstr(ts), size);  /* load directly in final place */
        return ts;
    }
}

// 500M
const int g_sizelimit = 1024 * 1024 * 500;
static bool LoadCode(LoadState *S, Proto *f) {
    int n = LoadInt(S);
	if (n > g_sizelimit)
	{
		return false;
	}

    f->code = luaM_newvector(S->L, n, Instruction);
    f->sizecode = n;
    LoadVector(S, f->code, n);
	return true;
}


static void LoadFunction(LoadState *S, Proto *f, TString *psource);

// 500M
const int g_constantslimit = 1024 * 1024 * 500;
static bool LoadConstants(LoadState *S, Proto *f) {
    int i;
    int n = LoadInt(S);
	if (n > g_constantslimit)
	{
		return false;
	}

    f->k = luaM_newvector(S->L, n, TValue);
    f->sizek = n;
    for (i = 0; i < n; i++)
        setnilvalue(&f->k[i]);
    for (i = 0; i < n; i++) {
        TValue *o = &f->k[i];
        int t = LoadByte(S);
        switch (t) {
        case LUA_TNIL:
            setnilvalue(o);
            break;
        case LUA_TBOOLEAN:
            setbvalue(o, LoadByte(S));
            break;
        case LUA_TNUMFLT:
            setfltvalue(o, LoadNumber(S));
            break;
        case LUA_TNUMINT:
            setivalue(o, LoadInteger(S));
            break;
        case LUA_TSHRSTR:
        case LUA_TLNGSTR:
            setsvalue2n(S->L, o, LoadString(S));
            break;
        default:
            lua_assert(0);
        }
    }
	return true;
}

// 500M
static bool LoadProtos(LoadState *S, Proto *f) {
    int i;
    int n = LoadInt(S);
	if (n > g_sizelimit)
	{
		return false;
	}

    f->p = luaM_newvector(S->L, n, Proto *);
    f->sizep = n;
    for (i = 0; i < n; i++)
        f->p[i] = nullptr;
    for (i = 0; i < n; i++) {
        f->p[i] = luaF_newproto(S->L);
        LoadFunction(S, f->p[i], f->source);
    }

	return true;
}

// 500M
static bool LoadUpvalues(LoadState *S, Proto *f) {
    int i, n;
    n = LoadInt(S);
	if (n > g_sizelimit)
	{
		return false;
	}

    f->upvalues = luaM_newvector(S->L, n, Upvaldesc);
    f->sizeupvalues = n;
    for (i = 0; i < n; i++)
        f->upvalues[i].name = nullptr;
    for (i = 0; i < n; i++) {
        f->upvalues[i].instack = LoadByte(S);
        f->upvalues[i].idx = LoadByte(S);
    }

	return true;
}

// 500M
const int g_lineslimit = 1024 * 1024 * 500;
static bool LoadDebug(LoadState *S, Proto *f) {
    int i, n;
    n = LoadInt(S);
	if (n > g_lineslimit)
	{
		return false;
	}

    f->lineinfo = luaM_newvector(S->L, n, int);
    f->sizelineinfo = n;
    LoadVector(S, f->lineinfo, n);
    n = LoadInt(S);
    f->locvars = luaM_newvector(S->L, n, LocVar);
    f->sizelocvars = n;
    for (i = 0; i < n; i++)
        f->locvars[i].varname = nullptr;
    for (i = 0; i < n; i++) {
        f->locvars[i].varname = LoadString(S);
        f->locvars[i].startpc = LoadInt(S);
        f->locvars[i].endpc = LoadInt(S);
    }
    n = LoadInt(S);
    for (i = 0; i < n; i++)
        f->upvalues[i].name = LoadString(S);

	return true;
}


static void LoadFunction(LoadState *S, Proto *f, TString *psource) {
    f->source = LoadString(S);
    if (f->source == nullptr)  /* no source in dump? */
        f->source = psource;  /* reuse parent's source */
    f->linedefined = LoadInt(S);
    f->lastlinedefined = LoadInt(S);
    f->numparams = LoadByte(S);
    f->is_vararg = LoadByte(S);
    f->maxstacksize = LoadByte(S);

	if (!LoadCode(S, f))
	{
		return;
	}

	if (!LoadConstants(S, f))
	{
		return;
	}

	if (!LoadUpvalues(S, f))
	{
		return;
	}

	if (!LoadProtos(S, f))
	{
		return;
	}

	LoadDebug(S, f);
}


static void checkliteral(LoadState *S, const char *s, const char *msg) {
    char buff[sizeof(LUA_SIGNATURE) + sizeof(LUAC_DATA)]; /* larger than both */
    size_t len = strlen(s);
    LoadVector(S, buff, len);
    if (memcmp(s, buff, len) != 0)
        error(S, msg);
}


static void fchecksize(LoadState *S, size_t size, const char *tname) {
    if (LoadByte(S) != size)
        error(S, luaO_pushfstring(S->L, "%s size mismatch in", tname));
}


#define checksize(S,t)	fchecksize(S,sizeof(t),#t)

static void checkHeader(LoadState *S) {
    checkliteral(S, LUA_SIGNATURE + 1, "not a");  /* 1st char already checked */
    if (LoadByte(S) != LUAC_VERSION)
        error(S, "version mismatch in");
    if (LoadByte(S) != LUAC_FORMAT)
        error(S, "format mismatch in");
    checkliteral(S, LUAC_DATA, "corrupted");
    checksize(S, int32_t);
    checksize(S, LUA_SIZE_T_TYPE);
    checksize(S, Instruction);
    checksize(S, lua_Integer);
    checksize(S, lua_Number);
    if (LoadInteger(S) != LUAC_INT)
        error(S, "endianness mismatch in");
    if (LoadNumber(S) != LUAC_NUM)
        error(S, "float format mismatch in");
}


/*
** load precompiled chunk
*/
LClosure *luaU_undump(lua_State *L, ZIO *Z, const char *name) {
    LoadState S;
    LClosure *cl;
    if (*name == '@' || *name == '=')
        S.name = name + 1;
    else if (*name == LUA_SIGNATURE[0])
        S.name = "binary string";
    else
        S.name = name;
    S.L = L;
    S.Z = Z;
    checkHeader(&S); // FIXME: release模式下这里好像有问题会报错
    cl = luaF_newLclosure(L, LoadByte(&S));
    setclLvalue(L, L->top, cl);
    luaD_inctop(L);
    cl->p = luaF_newproto(L);
    LoadFunction(&S, cl->p, nullptr);
    lua_assert(cl->nupvalues == cl->p->sizeupvalues);
    luai_verifycode(L, buff, cl->p);
    return cl;
}
