#define lcompile_cpp
#define LUA_CORE

#include "glua/lprefix.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "glua/lcompile.h"
#include "glua/lauxlib.h"
#include "glua/lobject.h"
#include "glua/lstate.h"
#include "glua/lundump.h"
#include "glua/thinkyoung_lua_lib.h"

using thinkyoung::lua::api::global_glua_chain_api;

static int listing = 1;			/* list bytecodes? */
static int dumping = 1;			/* dump bytecodes? */
static int stripping = 0;			/* strip debug information? */
static char Output[] = { OUTPUT };	/* default output file name */
static const char* output = Output;	/* actual output file name */
static const char* progname = PROGNAME;	/* actual program name */


//static void fatal(const char* message)
//{
//	fprintf(stderr, "%s: %s\n", progname, message);
//	exit(EXIT_FAILURE);
//}

#define IS(s)	(strcmp(argv[i],s)==0)

#define FUNCTION "(function()end)();"

static const char* reader(lua_State *L, void *ud, size_t *size)
{
    UNUSED(L);
    if ((*(int*)ud)--)
    {
        *size = sizeof(FUNCTION) - 1;
        return FUNCTION;
    }
    else
    {
        *size = 0;
        return nullptr;
    }
}

#define toproto(L,i) getproto(L->top+(i))

static void l_fatal(lua_State *L, const char *data)
{
    const char *msg = lua_pushfstring(L, "fatal error %s", data);
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_COMPILE_ERROR, msg);
    thinkyoung::lua::lib::notify_lua_state_stop(L);
}

static const Proto* combine(lua_State* L, int n)
{
    if (n == 1)
        return toproto(L, -1);
    else
    {
        Proto* f;
        int i = n;
        if (lua_load(L, reader, &i, "=(" PROGNAME ")", nullptr) != LUA_OK)
        {
            // fatal(lua_tostring(L, -1));
            l_fatal(L, lua_tostring(L, -1));
            return nullptr;
        }
        f = toproto(L, -1);
        for (i = 0; i < n; i++)
        {
            f->p[i] = toproto(L, i - n - 1);
            if (f->p[i]->sizeupvalues > 0) f->p[i]->upvalues[0].instack = 0;
        }
        f->sizelineinfo = 0;
        return f;
    }
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
    UNUSED(L);
    return (fwrite(p, size, 1, (FILE*)u) != 1) && (size != 0);
}


#include <ctype.h>
#include <stdio.h>

#define luac_c
#define LUA_CORE

#include "glua/ldebug.h"
#include "glua/lobject.h"
#include "glua/lopcodes.h"

#define VOID(p)		((const void*)(p))

void luaL_PrintString(FILE *out, const TString* ts)
{
    const char* s = getstr(ts);
    size_t i, n = tsslen(ts);
    fprintf(out, "%c", '"');
    for (i = 0; i < n; i++)
    {
        int c = (int)(unsigned char)s[i];
        switch (c)
        {
        case '"':  fprintf(out, "\\\""); break;
        case '\\': fprintf(out, "\\\\"); break;
        case '\a': fprintf(out, "\\a"); break;
        case '\b': fprintf(out, "\\b"); break;
        case '\f': fprintf(out, "\\f"); break;
        case '\n': fprintf(out, "\\n"); break;
        case '\r': fprintf(out, "\\r"); break;
        case '\t': fprintf(out, "\\t"); break;
        case '\v': fprintf(out, "\\v"); break;
        default:	if (isprint(c))
            fprintf(out, "%c", c);
                    else
                        fprintf(out, "\\%03d", c);
        }
    }
    fprintf(out, "%c", '"');
}

void luaL_PrintConstant(FILE *out, const Proto* f, int i)
{
    const TValue* o = &f->k[i];
    switch (ttype(o))
    {
    case LUA_TNIL:
        fprintf(out, "nil");
        break;
    case LUA_TBOOLEAN:
        fprintf(out, bvalue(o) ? "true" : "false");
        break;
    case LUA_TNUMFLT:
    {
        char buff[100];
        snprintf(buff, 100, LUA_NUMBER_FMT, fltvalue(o));
        fprintf(out, "%s", buff);
        if (buff[strspn(buff, "-0123456789")] == '\0') fprintf(out, ".0");
        break;
    }
    case LUA_TNUMINT:
        fprintf(out, LUA_INTEGER_FMT, ivalue(o));
        break;
    case LUA_TSHRSTR: case LUA_TLNGSTR:
        luaL_PrintString(out, tsvalue(o));
        break;
    default:				/* cannot happen */
        fprintf(out, "? type=%d", ttype(o));
        break;
    }
}

#define UPVALNAME(x) ((f->upvalues[x].name) ? getstr(f->upvalues[x].name) : "-")
#define MYK(x)		(-1-(x))

void luaL_PrintCode(FILE *out, const Proto* f)
{
    const Instruction* code = f->code;
    int pc, n = f->sizecode;
    for (pc = 0; pc < n; pc++)
    {
        Instruction i = code[pc];
        OpCode o = GET_OPCODE(i);
        int a = GETARG_A(i);
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        int ax = GETARG_Ax(i);
        int bx = GETARG_Bx(i);
        int sbx = GETARG_sBx(i);
        int line = getfuncline(f, pc);
        fprintf(out, "\t%d\t", pc + 1);
        if (line>0) fprintf(out, "[%d]\t", line); else fprintf(out, "[-]\t");
        fprintf(out, "%-9s\t", luaP_opnames[o]);
        switch (getOpMode(o))
        {
        case iABC:
            fprintf(out, "%d", a);
            if (getBMode(o) != OpArgN) fprintf(out, " %d", ISK(b) ? (MYK(INDEXK(b))) : b);
            if (getCMode(o) != OpArgN) fprintf(out, " %d", ISK(c) ? (MYK(INDEXK(c))) : c);
            break;
        case iABx:
            fprintf(out, "%d", a);
            if (getBMode(o) == OpArgK) fprintf(out, " %d", MYK(bx));
            if (getBMode(o) == OpArgU) fprintf(out, " %d", bx);
            break;
        case iAsBx:
            fprintf(out, "%d %d", a, sbx);
            break;
        case iAx:
            fprintf(out, "%d", MYK(ax));
            break;
        }
        switch (o)
        {
        case OP_LOADK:
            fprintf(out, "\t; "); luaL_PrintConstant(out, f, bx);
            break;
        case OP_GETUPVAL:
        case OP_SETUPVAL:
            fprintf(out, "\t; %s", UPVALNAME(b));
            break;
        case OP_GETTABUP:
            fprintf(out, "\t; %s", UPVALNAME(b));
            if (ISK(c)) { fprintf(out, " "); luaL_PrintConstant(out, f, INDEXK(c)); }
            break;
        case OP_SETTABUP:
            fprintf(out, "\t; %s", UPVALNAME(a));
            if (ISK(b)) { fprintf(out, " "); luaL_PrintConstant(out, f, INDEXK(b)); }
            if (ISK(c)) { fprintf(out, " "); luaL_PrintConstant(out, f, INDEXK(c)); }
            break;
        case OP_GETTABLE:
        case OP_SELF:
            if (ISK(c)) { fprintf(out, "\t; "); luaL_PrintConstant(out, f, INDEXK(c)); }
            break;
        case OP_SETTABLE:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_POW:
        case OP_DIV:
        case OP_IDIV:
        case OP_BAND:
        case OP_BOR:
        case OP_BXOR:
        case OP_SHL:
        case OP_SHR:
        case OP_EQ:
        case OP_LT:
        case OP_LE:
            if (ISK(b) || ISK(c))
            {
                fprintf(out, "\t; ");
                if (ISK(b)) luaL_PrintConstant(out, f, INDEXK(b)); else fprintf(out, "-");
                fprintf(out, " ");
                if (ISK(c)) luaL_PrintConstant(out, f, INDEXK(c)); else fprintf(out, "-");
            }
            break;
        case OP_JMP:
        case OP_FORLOOP:
        case OP_FORPREP:
        case OP_TFORLOOP:
            fprintf(out, "\t; to %d", sbx + pc + 2);
            break;
        case OP_CLOSURE:
            fprintf(out, "\t; %p", VOID(f->p[bx]));
            break;
        case OP_SETLIST:
            if (c == 0) fprintf(out, "\t; %d", (int)code[++pc]); else fprintf(out, "\t; %d", c);
            break;
        case OP_EXTRAARG:
            fprintf(out, "\t; "); luaL_PrintConstant(out, f, ax);
            break;
        default:
            break;
        }
        fprintf(out, "\n");
    }
}

#define SS(x)	((x==1)?"":"s")
#define S(x)	(int)(x),SS(x)

void luaL_PrintHeader(FILE *out, const Proto* f)
{
    const char* s = f->source ? getstr(f->source) : "=?";
    if (*s == '@' || *s == '=')
        s++;
    else if (*s == LUA_SIGNATURE[0])
        s = "(bstring)";
    else
        s = "(string)";
    fprintf(out, "\n%s <%s:%d,%d> (%d instruction%s at %p)\n",
        (f->linedefined == 0) ? "main" : "function", s,
        f->linedefined, f->lastlinedefined,
        S(f->sizecode), VOID(f));
    fprintf(out, "%d%s param%s, %d slot%s, %d upvalue%s, ",
        (int)(f->numparams), f->is_vararg ? "+" : "", SS(f->numparams),
        S(f->maxstacksize), S(f->sizeupvalues));
    fprintf(out, "%d local%s, %d constant%s, %d function%s\n",
        S(f->sizelocvars), S(f->sizek), S(f->sizep));
}

void luaL_PrintDebug(FILE *out, const Proto* f)
{
    int i, n;
    n = f->sizek;
    fprintf(out, "constants (%d) for %p:\n", n, VOID(f));
    for (i = 0; i < n; i++)
    {
        fprintf(out, "\t%d\t", i + 1);
        luaL_PrintConstant(out, f, i);
        fprintf(out, "\n");
    }
    n = f->sizelocvars;
    fprintf(out, "locals (%d) for %p:\n", n, VOID(f));
    for (i = 0; i < n; i++)
    {
        fprintf(out, "\t%d\t%s\t%d\t%d\n",
            i, getstr(f->locvars[i].varname), f->locvars[i].startpc + 1, f->locvars[i].endpc + 1);
    }
    n = f->sizeupvalues;
    fprintf(out, "upvalues (%d) for %p:\n", n, VOID(f));
    for (i = 0; i < n; i++)
    {
        fprintf(out, "\t%d\t%s\t%d\t%d\n",
            i, UPVALNAME(i), f->upvalues[i].instack, f->upvalues[i].idx);
    }
}

void luaL_PrintFunctionToFile(FILE *out, const Proto* f, int full)
{
    int i, n = f->sizep;
    luaL_PrintHeader(out, f);
    luaL_PrintCode(out, f);
    if (full) luaL_PrintDebug(out, f);
    for (i = 0; i < n; i++) luaL_PrintFunctionToFile(out, f->p[i], full);
}

void luaL_PrintFunction(const Proto* f, int full)
{
    luaL_PrintFunctionToFile(stdout, f, full);
}
