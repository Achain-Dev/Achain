/*
** $Id: luac.c,v 1.75 2015/03/12 01:58:27 lhf Exp $
** Lua compiler (saves bytecodes to files; also lists bytecodes)
** See Copyright Notice in lua.h
*/

#define luac_cpp
#define LUA_CORE

#include "glua/lprefix.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glua/lua.h"
#include "glua/lauxlib.h"

#include "glua/lobject.h"
#include "glua/lstate.h"
#include "glua/lundump.h"
#include "glua/lcompile.h"

static void PrintFunction(const Proto* f, int full);
// #define luaU_print	PrintFunction

#define PROGNAME	"luac"		/* default program name */
#define OUTPUT		PROGNAME ".out"	/* default output file */

static int listing = 0;			/* list bytecodes? */
static int dumping = 0;			/* dump bytecodes? */
static int stripping = 0;			/* strip debug information? */
static char Output[] = { OUTPUT };	/* default output file name */
static const char* output = Output;	/* actual output file name */
static const char* progname = PROGNAME;	/* actual program name */

static void fatal(const char* message)
{
    fprintf(stderr, "%s: %s\n", progname, message);
    exit(EXIT_FAILURE);
}

static void cannot(const char* what)
{
    fprintf(stderr, "%s: cannot %s %s: %s\n", progname, what, output, strerror(errno));
    exit(EXIT_FAILURE);
}

static void usage(const char* message)
{
    if (*message == '-')
        fprintf(stderr, "%s: unrecognized option '%s'\n", progname, message);
    else
        fprintf(stderr, "%s: %s\n", progname, message);
    fprintf(stderr,
        "usage: %s [options] [filenames]\n"
        "Available options are:\n"
        "  -l       list (use -l -l for full listing)\n"
        "  -o name  output to file 'name' (default is \"%s\")\n"
        "  -p       parse only\n"
        "  -s       strip debug information\n"
        "  -v       show version information\n"
        "  --       stop handling options\n"
        "  -        stop handling options and process stdin\n"
        , progname, Output);
    exit(EXIT_FAILURE);
}

#define IS(s)	(strcmp(argv[i],s)==0)

static int doargs(int argc, char* argv[])
{
    int i;
    int version = 0;
    if (argv[0] != nullptr && *argv[0] != 0) progname = argv[0];
    for (i = 1; i < argc; i++)
    {
        if (*argv[i] != '-')			/* end of options; keep it */
            break;
        else if (IS("--"))			/* end of options; skip it */
        {
            ++i;
            if (version) ++version;
            break;
        }
        else if (IS("-"))			/* end of options; use stdin */
            break;
        else if (IS("-l"))			/* list */
            ++listing;
        else if (IS("-o"))			/* output file */
        {
            output = argv[++i];
            if (output == nullptr || *output == 0 || (*output == '-' && output[1] != 0))
                usage("'-o' needs argument");
            if (IS("-")) output = nullptr;
        }
        else if (IS("-p"))			/* parse only */
            dumping = 0;
        else if (IS("-s"))			/* strip debug information */
            stripping = 1;
        else if (IS("-v"))			/* show version */
            ++version;
        else					/* unknown option */
            usage(argv[i]);
    }
    if (i == argc && (listing || !dumping))
    {
        dumping = 0;
        argv[--i] = Output;
    }
    if (version)
    {
        printf("%s\n", LUA_COPYRIGHT);
        if (version == argc - 1) exit(EXIT_SUCCESS);
    }
    return i;
}

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

static const Proto* combine(lua_State* L, int n)
{
    if (n == 1)
        return toproto(L, -1);
    else
    {
        Proto* f;
        int i = n;
        if (lua_load(L, reader, &i, "=(" PROGNAME ")", nullptr) != LUA_OK) fatal(lua_tostring(L, -1));
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

static int pmain(lua_State* L)
{
    int argc = (int)lua_tointeger(L, 1);
    char** argv = (char**)lua_touserdata(L, 2);
    const Proto* f;
    int i;
    if (!lua_checkstack(L, argc)) fatal("too many input files");
    for (i = 0; i < argc; i++)
    {
        const char* filename = IS("-") ? nullptr : argv[i];
        if (luaL_loadfile(L, filename) != LUA_OK) fatal(lua_tostring(L, -1));
    }
    f = combine(L, argc);
    if (listing) luaU_print(f, listing > 1);
    if (dumping)
    {
        FILE* D = (output == nullptr) ? stdout : fopen(output, "wb");
        if (!D) 
        {
          cannot("open"); 
          return 0;
        }
        lua_lock(L);
        luaU_dump(L, f, writer, D, stripping);
        lua_unlock(L);
        if (ferror(D)) cannot("write");
        if (fclose(D)) cannot("close");
    }
    return 0;
}

int main2(int argc, char* argv[])
{
    lua_State* L;
    int i = doargs(argc, argv);
    argc -= i; argv += i;

    if (argc <= 0) usage("no input files given");
    L = luaL_newstate();
    if (L == nullptr) fatal("cannot create state: not enough memory");
    lua_pushcfunction(L, &pmain);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) fatal(lua_tostring(L, -1));
    lua_close(L);
    return EXIT_SUCCESS;
}

/*
** $Id: luac.c,v 1.75 2015/03/12 01:58:27 lhf Exp $
** print bytecodes
** See Copyright Notice in lua.h
*/

#include <ctype.h>
#include <stdio.h>

#define luac_c
#define LUA_CORE

#include "glua/ldebug.h"
#include "glua/lobject.h"
#include "glua/lopcodes.h"

#define VOID(p)		((const void*)(p))

static void PrintString(const TString* ts)
{
    luaL_PrintString(stdout, ts);
}

static void PrintConstant(const Proto* f, int i)
{
    luaL_PrintConstant(stdout, f, i);
}
static void PrintCode(const Proto* f)
{
    luaL_PrintCode(stdout, f);
}

static void PrintHeader(const Proto* f)
{
    luaL_PrintHeader(stdout, f);
}

static void PrintDebug(const Proto* f)
{
    luaL_PrintDebug(stdout, f);
}

static void PrintFunction(const Proto* f, int full)
{
    luaL_PrintFunctionToFile(stdout, f, full);
}
