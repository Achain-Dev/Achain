/*
** $Id: lauxlib.c,v 1.284 2015/11/19 19:16:22 roberto Exp $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/

#define lauxlib_cpp

#include <glua/lprefix.h>

#include <errno.h>
#include <stdarg.h>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>
#include <vector>
#include <stack>
#include <algorithm>


/* This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
*/

#include <glua/lua.h>

#include <glua/lauxlib.h>
#include <glua/lobject.h>
#include <glua/lapi.h>
#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>
#include <glua/glua_lutil.h>
#include <glua/exceptions.h>

using thinkyoung::lua::api::global_glua_chain_api;


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** search for 'objidx' in table at index -1.
** return 1 + string at top if find a good name.
*/
static int findfield(lua_State *L, int objidx, int level) {
    if (level == 0 || !lua_istable(L, -1))
        return 0;  /* not found */
    lua_pushnil(L);  /* start 'next' loop */
    while (lua_next(L, -2)) {  /* for each pair in table */
        if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
            if (lua_rawequal(L, objidx, -1)) {  /* found object? */
                lua_pop(L, 1);  /* remove value (but keep name) */
                return 1;
            }
            else if (findfield(L, objidx, level - 1)) {  /* try recursively */
                lua_remove(L, -2);  /* remove table (but keep name) */
                lua_pushliteral(L, ".");
                lua_insert(L, -2);  /* place '.' between the two names */
                lua_concat(L, 3);
                return 1;
            }
        }
        lua_pop(L, 1);  /* remove value */
    }
    return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
** (registry._LOADED).
*/
static int pushglobalfuncname(lua_State *L, lua_Debug *ar) {
    int top = lua_gettop(L);
    lua_getinfo(L, "f", ar);  /* push function */
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    if (findfield(L, top + 1, 2)) {
        const char *name = lua_tostring(L, -1);
        if (strncmp(name, "_G.", 3) == 0) {  /* name start with '_G.'? */
            lua_pushstring(L, name + 3);  /* push name without prefix */
            lua_remove(L, -2);  /* remove original name */
        }
        lua_copy(L, -1, top + 1);  /* move name to proper place */
        lua_pop(L, 2);  /* remove pushed values */
        return 1;
    }
    else {
        lua_settop(L, top);  /* remove function and global table */
        return 0;
    }
}


static void pushfuncname(lua_State *L, lua_Debug *ar) {
    if (pushglobalfuncname(L, ar)) {  /* try first a global name */
        lua_pushfstring(L, "function '%s'", lua_tostring(L, -1));
        lua_remove(L, -2);  /* remove name */
    }
    else if (*ar->namewhat != '\0')  /* is there a name from code? */
        lua_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
    else if (*ar->what == 'm')  /* main? */
        lua_pushliteral(L, "main chunk");
    else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
        lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
    else  /* nothing left... */
        lua_pushliteral(L, "?");
}


static int lastlevel(lua_State *L) {
    lua_Debug ar;
    int li = 1, le = 1;
    /* find an upper bound */
    while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
    /* do a binary search */
    while (li < le) {
        int m = (li + le) / 2;
        if (lua_getstack(L, m, &ar)) li = m + 1;
        else le = m;
    }
    return le - 1;
}


LUALIB_API void luaL_traceback(lua_State *L, lua_State *L1,
    const char *msg, int level) {
    lua_Debug ar;
    int top = lua_gettop(L);
    int last = lastlevel(L1);
    int n1 = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
    if (msg)
        lua_pushfstring(L, "%s\n", msg);
    luaL_checkstack(L, 10, nullptr);
    lua_pushliteral(L, "stack traceback:");
    while (lua_getstack(L1, level++, &ar)) {
        if (n1-- == 0) {  /* too many levels? */
            lua_pushliteral(L, "\n\t...");  /* add a '...' */
            level = last - LEVELS2 + 1;  /* and skip to last ones */
        }
        else {
            lua_getinfo(L1, "Slnt", &ar);
            lua_pushfstring(L, "\n\t%s:", ar.short_src);
            if (ar.currentline > 0)
                lua_pushfstring(L, "%d:", ar.currentline);
            lua_pushliteral(L, " in ");
            pushfuncname(L, &ar);
            if (ar.istailcall)
                lua_pushliteral(L, "\n\t(...tail calls...)");
            lua_concat(L, lua_gettop(L) - top);
        }
    }
    lua_concat(L, lua_gettop(L) - top);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

LUALIB_API int luaL_argerror(lua_State *L, int arg, const char *extramsg) {
    lua_Debug ar;
    if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
        return luaL_error(L, "bad argument #%d (%s)", arg, extramsg);
    lua_getinfo(L, "n", &ar);
    if (strcmp(ar.namewhat, "method") == 0) {
        arg--;  /* do not count 'self' */
        if (arg == 0)  /* error is in the self argument itself? */
            return luaL_error(L, "calling '%s' on bad self (%s)",
            ar.name, extramsg);
    }
    if (ar.name == nullptr)
        ar.name = (pushglobalfuncname(L, &ar)) ? lua_tostring(L, -1) : "?";
    return luaL_error(L, "bad argument #%d to '%s' (%s)",
        arg, ar.name, extramsg);
}


static int typeerror(lua_State *L, int arg, const char *tname) {
    const char *msg;
    const char *typearg;  /* name for the type of the actual argument */
    if (luaL_getmetafield(L, arg, "__name") == LUA_TSTRING)
        typearg = lua_tostring(L, -1);  /* use the given type name */
    else if (lua_type(L, arg) == LUA_TLIGHTUSERDATA)
        typearg = "light userdata";  /* special name for messages */
    else
        typearg = luaL_typename(L, arg);  /* standard name */
    msg = lua_pushfstring(L, "%s expected, got %s", tname, typearg);
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, msg);
    return luaL_argerror(L, arg, msg);
}


static void tag_error(lua_State *L, int arg, int tag) {
    typeerror(L, arg, lua_typename(L, tag));
}


LUALIB_API void luaL_where(lua_State *L, int level) {
    lua_Debug ar;
    if (lua_getstack(L, level, &ar)) {  /* check function at level */
        lua_getinfo(L, "Sl", &ar);  /* get info about it */
        if (ar.currentline > 0) {  /* is there info? */
            lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
            return;
        }
    }
    lua_pushliteral(L, "");  /* else, no information available... */
}


LUALIB_API int luaL_error(lua_State *L, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    luaL_where(L, 1);
    lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    lua_concat(L, 2);
    return lua_error(L);
}


LUALIB_API int luaL_fileresult(lua_State *L, int stat, const char *fname) {
    int en = errno;  /* calls to Lua API may change this value */
    if (stat) {
        lua_pushboolean(L, 1);
        return 1;
    }
    else {
        lua_pushnil(L);
        if (fname)
            lua_pushfstring(L, "%s: %s", fname, strerror(en));
        else
            lua_pushstring(L, strerror(en));
        lua_pushinteger(L, en);
        return 3;
    }
}


#if !defined(l_inspectstat)	/* { */

#if defined(LUA_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
      else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


LUALIB_API int luaL_execresult(lua_State *L, int stat) {
    const char *what = "exit";  /* type of termination */
    if (stat == -1)  /* error? */
        return luaL_fileresult(L, 0, nullptr);
    else {
        l_inspectstat(stat, what);  /* interpret result */
        if (*what == 'e' && stat == 0)  /* successful termination? */
            lua_pushboolean(L, 1);
        else
            lua_pushnil(L);
        lua_pushstring(L, what);
        lua_pushinteger(L, stat);
        return 3;  /* return true/nil,what,code */
    }
}

/* }====================================================== */


/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

LUALIB_API int luaL_newmetatable(lua_State *L, const char *tname) {
    if (luaL_getmetatable(L, tname) != LUA_TNIL)  /* name already in use? */
        return 0;  /* leave previous value on top, but return 0 */
    lua_pop(L, 1);
    lua_createtable(L, 0, 2);  /* create metatable */
    lua_pushstring(L, tname);
    lua_setfield(L, -2, "__name");  /* metatable.__name = tname */
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, tname);  /* registry.name = metatable */
    return 1;
}


LUALIB_API void luaL_setmetatable(lua_State *L, const char *tname) {
    luaL_getmetatable(L, tname);
    lua_setmetatable(L, -2);
}


LUALIB_API void *luaL_testudata(lua_State *L, int ud, const char *tname) {
    void *p = lua_touserdata(L, ud);
    if (p != nullptr) {  /* value is a userdata? */
        if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
            luaL_getmetatable(L, tname);  /* get correct metatable */
            if (!lua_rawequal(L, -1, -2))  /* not the same? */
                p = nullptr;  /* value is a userdata with wrong metatable */
            lua_pop(L, 2);  /* remove both metatables */
            return p;
        }
    }
    return nullptr;  /* value is not a userdata with a metatable */
}


LUALIB_API void *luaL_checkudata(lua_State *L, int ud, const char *tname) {
    void *p = luaL_testudata(L, ud, tname);
    if (p == nullptr) typeerror(L, ud, tname);
    return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

LUALIB_API int luaL_checkoption(lua_State *L, int arg, const char *def,
    const char *const lst[]) {
    const char *name = (def) ? luaL_optstring(L, arg, def) :
        luaL_checkstring(L, arg);
    int i;
    for (i = 0; lst[i]; i++)
        if (strcmp(lst[i], name) == 0)
            return i;
    return luaL_argerror(L, arg,
        lua_pushfstring(L, "invalid option '%s'", name));
}


LUALIB_API void luaL_checkstack(lua_State *L, int space, const char *msg) {
    /* keep some extra space to run error routines, if needed */
    const int extra = LUA_MINSTACK;
    if (!lua_checkstack(L, space + extra)) {
        if (msg)
            luaL_error(L, "stack overflow (%s)", msg);
        else
            luaL_error(L, "stack overflow");
    }
}


LUALIB_API void luaL_checktype(lua_State *L, int arg, int t) {
    if (lua_type(L, arg) != t)
        tag_error(L, arg, t);
}


LUALIB_API void luaL_checkany(lua_State *L, int arg) {
    if (lua_type(L, arg) == LUA_TNONE)
        luaL_argerror(L, arg, "value expected");
}


LUALIB_API const char *luaL_checklstring(lua_State *L, int arg, size_t *len) {
    const char *s = lua_tolstring(L, arg, len);
    if (!s) tag_error(L, arg, LUA_TSTRING);
    return s;
}


LUALIB_API const char *luaL_optlstring(lua_State *L, int arg,
    const char *def, size_t *len) {
    if (lua_isnoneornil(L, arg)) {
        if (len)
            *len = (def ? strlen(def) : 0);
        return def;
    }
    else return luaL_checklstring(L, arg, len);
}


LUALIB_API lua_Number luaL_checknumber(lua_State *L, int arg) {
    int isnum;
    lua_Number d = lua_tonumberx(L, arg, &isnum);
    if (!isnum)
        tag_error(L, arg, LUA_TNUMBER);
    return d;
}


LUALIB_API lua_Number luaL_optnumber(lua_State *L, int arg, lua_Number def) {
    return luaL_opt(L, luaL_checknumber, arg, def);
}


static void interror(lua_State *L, int arg) {
    if (lua_isnumber(L, arg))
        luaL_argerror(L, arg, "number has no integer representation");
    else
        tag_error(L, arg, LUA_TNUMBER);
}


LUALIB_API lua_Integer luaL_checkinteger(lua_State *L, int arg) {
    int isnum;
    lua_Integer d = lua_tointegerx(L, arg, &isnum);
    if (!isnum) {
        interror(L, arg);
    }
    return d;
}


LUALIB_API lua_Integer luaL_optinteger(lua_State *L, int arg,
    lua_Integer def) {
    return luaL_opt(L, luaL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
    void *box;
    size_t bsize;
} UBox;


static void *resizebox(lua_State *L, int idx, size_t newsize) {
    void *ud;
    lua_Alloc allocf = lua_getallocf(L, &ud);
    UBox *box = (UBox *)lua_touserdata(L, idx);
    void *temp = allocf(ud, box->box, box->bsize, newsize);
    if (temp == nullptr && newsize > 0) {  /* allocation error? */
        resizebox(L, idx, 0);  /* free buffer */
        luaL_error(L, "not enough memory for buffer allocation");
    }
    box->box = temp;
    box->bsize = newsize;
    return temp;
}


static int boxgc(lua_State *L) {
    resizebox(L, 1, 0);
    return 0;
}


static void *newbox(lua_State *L, size_t newsize) {
    UBox *box = (UBox *)lua_newuserdata(L, sizeof(UBox));
    box->box = nullptr;
    box->bsize = 0;
    if (luaL_newmetatable(L, "LUABOX")) {  /* creating metatable? */
        lua_pushcfunction(L, boxgc);
        lua_setfield(L, -2, "__gc");  /* metatable.__gc = boxgc */
    }
    lua_setmetatable(L, -2);
    return resizebox(L, -1, newsize);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->initb)


/*
** returns a pointer to a free area with at least 'sz' bytes
*/
LUALIB_API char *luaL_prepbuffsize(luaL_Buffer *B, size_t sz) {
    lua_State *L = B->L;
    if (B->size - B->n < sz) {  /* not enough space? */
        char *newbuff;
        size_t newsize = B->size * 2;  /* double buffer size */
        if (newsize - B->n < sz)  /* not big enough? */
            newsize = B->n + sz;
        if (newsize < B->n || newsize - B->n < sz)
            luaL_error(L, "buffer too large");
        /* create larger buffer */
        if (buffonstack(B))
            newbuff = (char *)resizebox(L, -1, newsize);
        else {  /* no buffer yet */
            newbuff = (char *)newbox(L, newsize);
            memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
        }
        B->b = newbuff;
        B->size = newsize;
    }
    return &B->b[B->n];
}


LUALIB_API void luaL_addlstring(luaL_Buffer *B, const char *s, size_t l) {
    if (l > 0) {  /* avoid 'memcpy' when 's' can be nullptr */
        char *b = luaL_prepbuffsize(B, l);
        memcpy(b, s, l * sizeof(char));
        luaL_addsize(B, l);
    }
}


LUALIB_API void luaL_addstring(luaL_Buffer *B, const char *s) {
    luaL_addlstring(B, s, strlen(s));
}


LUALIB_API void luaL_pushresult(luaL_Buffer *B) {
    lua_State *L = B->L;
    lua_pushlstring(L, B->b, B->n);
    if (buffonstack(B)) {
        resizebox(L, -2, 0);  /* delete old buffer */
        lua_remove(L, -2);  /* remove its header from the stack */
    }
}


LUALIB_API void luaL_pushresultsize(luaL_Buffer *B, size_t sz) {
    luaL_addsize(B, sz);
    luaL_pushresult(B);
}


LUALIB_API void luaL_addvalue(luaL_Buffer *B) {
    lua_State *L = B->L;
    size_t l;
    const char *s = lua_tolstring(L, -1, &l);
    if (buffonstack(B))
        lua_insert(L, -2);  /* put value below buffer */
    luaL_addlstring(B, s, l);
    lua_remove(L, (buffonstack(B)) ? -2 : -1);  /* remove value */
}


LUALIB_API void luaL_buffinit(lua_State *L, luaL_Buffer *B) {
    B->L = L;
    B->b = B->initb;
    B->n = 0;
    B->size = LUAL_BUFFERSIZE;
}


LUALIB_API char *luaL_buffinitsize(lua_State *L, luaL_Buffer *B, size_t sz) {
    luaL_buffinit(L, B);
    return luaL_prepbuffsize(B, sz);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header */
#define freelist	0


LUALIB_API int luaL_ref(lua_State *L, int t) {
    int ref;
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);  /* remove from stack */
        return LUA_REFNIL;  /* 'nil' has a unique fixed reference */
    }
    t = lua_absindex(L, t);
    lua_rawgeti(L, t, freelist);  /* get first free element */
    ref = (int)lua_tointeger(L, -1);  /* ref = t[freelist] */
    lua_pop(L, 1);  /* remove it from stack */
    if (ref != 0) {  /* any free element? */
        lua_rawgeti(L, t, ref);  /* remove it from list */
        lua_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
    }
    else  /* no free elements */
        ref = (int)lua_rawlen(L, t) + 1;  /* get a new reference */
    lua_rawseti(L, t, ref);
    return ref;
}


LUALIB_API void luaL_unref(lua_State *L, int t, int ref) {
    if (ref >= 0) {
        t = lua_absindex(L, t);
        lua_rawgeti(L, t, freelist);
        lua_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
        lua_pushinteger(L, ref);
        lua_rawseti(L, t, freelist);  /* t[freelist] = ref */
    }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
    int n;  /* number of pre-read characters */
    FILE *f;  /* file being read */
    char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF(lua_State *L, void *ud, size_t *size) {
    LoadF *lf = (LoadF *)ud;
    (void)L;  /* not used */
    if (lf->n > 0) {  /* are there pre-read characters to be read? */
        *size = lf->n;  /* return them (chars already in buffer) */
        lf->n = 0;  /* no more pre-read characters */
    }
    else {  /* read a block from file */
        /* 'fread' can return > 0 *and* set the EOF flag. If next call to
           'getF' called 'fread', it might still wait for user input.
           The next check avoids this problem. */
        if (feof(lf->f)) return nullptr;
        *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
    }
    return lf->buff;
}


static int errfile(lua_State *L, const char *what, int fnameindex) {
    const char *serr = strerror(errno);
    const char *filename = lua_tostring(L, fnameindex) + 1;
    lua_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, luaL_checkstring(L, -1));
    lua_remove(L, fnameindex);
    return LUA_ERRFILE;
}


static int skipBOM(LoadF *lf) {
    const char *p = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
    int c;
    lf->n = 0;
    do {
        c = getc(lf->f);
        if (c == EOF || c != *(const unsigned char *)p++) return c;
        lf->buff[lf->n++] = c;  /* to be read by the parser */
    } while (*p != '\0');
    lf->n = 0;  /* prefix matched; discard it */
    return getc(lf->f);  /* return next character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment(LoadF *lf, int *cp) {
    int c = *cp = skipBOM(lf);
    if (c == '#') {  /* first line is a comment (Unix exec. file)? */
        do {  /* skip first line */
            c = getc(lf->f);
        } while (c != EOF && c != '\n');
        *cp = getc(lf->f);  /* skip end-of-line, if present */
        return 1;  /* there was a comment */
    }
    else return 0;  /* no comment */
}

static lua_State *globalL = nullptr;
static const char *progname = "thinkyoung_lua";

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message(const char *pname, const char *msg) {
    if (pname) lua_writestringerror("%s: ", pname);
    lua_writestringerror("%s\n", msg);
}

/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State *L, lua_Debug *ar) {
    (void)ar;  /* unused arg. */
    lua_sethook(L, nullptr, 0, 0);  /* reset hook */
    luaL_error(L, "interrupted!");
}

/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr) {  /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
            return 1;  /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)",
            luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    globalL = L;  /* to be available to 'laction' */
    signal(SIGINT, laction);  /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    if (lua_gettop(L) > 0)
        lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(lua_State *L, int status) {
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        l_message(progname, msg);
        lua_pop(L, 1);  /* remove message */
    }
    return status;
}

static int dochunk(lua_State *L, int status) {
    if (status == LUA_OK) status = docall(L, 0, 0);
    return report(L, status);
}

static int dofile(lua_State *L, const char *name) {
    return dochunk(L, luaL_loadfile(L, name));
}


static int dostring(lua_State *L, const char *s, const char *name) {
    return dochunk(L, luaL_loadbuffer(L, s, strlen(s), name));
}


/*
** Calls 'require(name)' and stores the result in a global variable
** with the given name.
*/
static int dolibrary(lua_State *L, const char *name) {
    int status;
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    status = docall(L, 1, 1);  /* call 'require(name)' */
    if (status == LUA_OK)
        lua_setglobal(L, name);  /* global[name] = require return */
    return report(L, status);
}

/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs(lua_State *L) {
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    n = (int)luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -i);  /* remove table from the stack */
    return n;
}

static int handle_script(lua_State *L, char **argv) {
    // READ THIS
    int status;
    const char *fname = argv[0];
    if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
        fname = nullptr;  /* stdin */
    status = luaL_loadfile(L, fname);
    if (status == LUA_OK) {
        int n = pushargs(L);  /* push arguments to script */
        status = docall(L, n, LUA_MULTRET);
    }
    return report(L, status);
}

/* bits of various argument indicators in 'args' */
#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */

#if !defined(LUA_PROMPT)
#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#endif

#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME		"lua"
#endif

#if !defined(LUA_MAXINPUT)
#define LUA_MAXINPUT		512
#endif

#if !defined(LUA_INIT_VAR)
#define LUA_INIT_VAR		"LUA_INIT"
#endif

#define LUA_INITVARVERSION  \
	LUA_INIT_VAR "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR

/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any Lua code (or an error code if it finds
** any invalid argument). 'first' returns the first not-handled argument
** (either the script name or a bad argument in case of error).
*/
static int collectargs(char **argv, int *first) {
    int args = 0;
    int i;
    for (i = 1; argv[i] != nullptr; i++) {
        *first = i;
        if (argv[i][0] != '-')  /* not an option? */
            return args;  /* stop handling options */
        switch (argv[i][1]) {  /* else check option */
        case '-':  /* '--' */
            if (argv[i][2] != '\0')  /* extra characters after '--'? */
                return has_error;  /* invalid option */
            *first = i + 1;
            return args;
        case '\0':  /* '-' */
            return args;  /* script "name" is '-' */
        case 'E':
            if (argv[i][2] != '\0')  /* extra characters after 1st? */
                return has_error;  /* invalid option */
            args |= has_E;
            break;
        case 'i':
            args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
        case 'v':
            if (argv[i][2] != '\0')  /* extra characters after 1st? */
                return has_error;  /* invalid option */
            args |= has_v;
            break;
        case 'e':
            args |= has_e;  /* FALLTHROUGH */
        case 'l':  /* both options need an argument */
            if (argv[i][2] == '\0') {  /* no concatenated argument? */
                i++;  /* try next 'argv' */
                if (argv[i] == nullptr || argv[i][0] == '-')
                    return has_error;  /* no next argument or it is another option */
            }
            break;
        default:  /* invalid option */
            return has_error;
        }
    }
    *first = i;  /* no script name */
    return args;
}


/*
** Processes options 'e' and 'l', which involve running Lua code.
** Returns 0 if some code raises an error.
*/
static int runargs(lua_State *L, char **argv, int n) {
    int i;
    for (i = 1; i < n; i++) {
        int option = argv[i][1];
        lua_assert(argv[i][0] == '-');  /* already checked */
        if (option == 'e' || option == 'l') {
            int status;
            const char *extra = argv[i] + 2;  /* both options need an argument */
            if (*extra == '\0') extra = argv[++i];
            lua_assert(extra != nullptr);
            status = (option == 'e')
                ? dostring(L, extra, "=(command line)")
                : dolibrary(L, extra);
            if (status != LUA_OK) return 0;
        }
    }
    return 1;
}

/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') go to positive indices;
** other arguments (before the script name) go to negative indices.
** If there is no script name, assume interpreter's name as base.
*/
static void createargtable(lua_State *L, char **argv, int argc, int script) {
    int i, narg;
    if (script == argc) script = 0;  /* no script name? */
    narg = argc - (script + 1);  /* number of positive indices */
    lua_createtable(L, narg, script + 1);
    for (i = 0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - script);
    }
    lua_setglobal(L, "arg");
}

static int handle_luainit(lua_State *L) {
    const char *name = "=" LUA_INITVARVERSION;
    const char *init = getenv(name + 1);
    if (init == nullptr) {
        name = "=" LUA_INIT_VAR;
        init = getenv(name + 1);  /* try alternative name */
    }
    if (init == nullptr) return LUA_OK;
    else if (init[0] == '@')
        return dofile(L, init + 1);
    else
        return dostring(L, init, name);
}

static int pmain_of_run_compiled_file(lua_State *L)
{
    int argc = (int)lua_tointeger(L, 1);
    char **argv = (char **)lua_touserdata(L, 2);
    int script;
    int args = collectargs(argv, &script);
    luaL_checkversion(L);  /* check that interpreter has correct version */
    if (argv[0] && argv[0][0]) progname = argv[0];
    createargtable(L, argv, argc, script);  /* create table 'arg' */
    if (!(args & has_E)) {  /* no option '-E'? */
        if (handle_luainit(L) != LUA_OK)  /* run LUA_INIT */
            return 0;  /* error running LUA_INIT */
    }
    if (!runargs(L, argv, script))  /* execute arguments -e and -l */
        return 0;  /* something failed */
    if (script < argc &&  /* execute main script (if there is one) */
        handle_script(L, argv + script) != LUA_OK)
        return 0;
    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}

LUA_API int lua_docompiledfile(lua_State *L, const char *filename)
{
	try
	{
		if (luaL_loadfile(L, filename))
		{
			printf("error\n");
			return LUA_ERRRUN;
		}
		lua_pcall(L, 0, 0, 0);
		return LUA_OK;
	}
	catch(const std::exception &e)
	{
      global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "error in load bytecode file, %s", e.what());
		return LUA_ERRRUN;
	}
}

LUA_API int lua_docompiled_bytestream(lua_State *L, void *stream_addr)
{
    GluaModuleByteStreamP stream = (GluaModuleByteStreamP) stream_addr;
    if (luaL_loadbufferx(L, stream->buff.data(), stream->buff.size(), "compiled_chunk", "binary"))
    {
        printf("error\n");
        return LUA_ERRRUN;
    }
    lua_pcall(L, 0, 0, 0);
    return LUA_OK;
}

static bool findloader_for_import_stream(lua_State *L, const char *name) {
    int i;
    luaL_Buffer msg;  /* to build error message */
    luaL_buffinit(L, &msg);
    lua_getglobal(L, "package");
    /* push 'package.searchers' to index 4 in the stack */
    if (lua_getfield(L, -1, "searchers") != LUA_TTABLE)
        luaL_error(L, "'package.searchers' must be a table");
    lua_remove(L, 3);
    /*  iterate over available searchers to find a loader */
    for (i = 1;; i++) {
        if (lua_rawgeti(L, 3, i) == LUA_TNIL) {  /* no more searchers? */
            lua_pop(L, 1);  /* remove nil */
            luaL_pushresult(&msg);  /* create error message */
            luaL_error(L, "module '%s' not found:%s", name, lua_tostring(L, -1));
        }
        lua_pushstring(L, name);
        lua_call(L, 1, 2);  /* call it *///searcher_thinkyoung
        if (lua_isfunction(L, -2))  /* did it find a loader? */
            return true;  /* module loader found */
        else if (lua_isstring(L, -2)) {  /* searcher returned error message? */
            lua_pop(L, 1);  /* remove extra return */
            luaL_addvalue(&msg);  /* concatenate error message */
            if (global_glua_chain_api->has_exception(L))
            {
                return false;
            }
        }
        else
            lua_pop(L, 2);  /* remove both returns */
    }
    return false;
}

static bool findloader(lua_State *L, const char *name) {
    return findloader_for_import_stream(L, name);
}

static bool findloader_for_import_contract(lua_State *L, const char *name) {
    return findloader_for_import_stream(L, name);
}

int luaL_require_module(lua_State *L)
{
    if (lua_gettop(L) < 1)
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "require need 1 argument of contract name");
        return 0;
    }
    const char *name = luaL_checkstring(L, 1);
    lua_settop(L, 1);  /* _LOADED table will be at index 2 */
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, 2, name);  /* _LOADED[name] */
    if (lua_toboolean(L, -1))  /* is it there? */
        return 1;  /* package is already loaded */
    /* else must load package */
    lua_pop(L, 1);  /* remove 'getfield' result */
    bool loaderfound = findloader(L, name);
    lua_pushstring(L, name);  /* pass name as argument to module loader */
    lua_insert(L, -2);  /* name is 1st argument (before search data) */
    lua_call(L, 2, 1);  /* run loader to load module */
    if (!lua_isnil(L, -1))  /* non-nil return? */
        lua_setfield(L, 2, name);  /* _LOADED[name] = returned value */
    if (lua_getfield(L, 2, name) == LUA_TNIL) {   /* module set no value? */
        lua_pushboolean(L, 1);  /* use true as result */
        lua_pushvalue(L, -1);  /* extra copy to be returned */
        lua_setfield(L, 2, name);  /* _LOADED[name] = true */
    }
    return 1;
}

struct GluaStorageValue;
struct GluaStorageValue lua_type_to_storage_value_type(lua_State *L, int index, size_t len);

// 包装后的合约的api函数
static int contract_api_wrapper_func(lua_State *L)
{
	int api_func_index = lua_upvalueindex(1); // api func
	const char *contract_id = lua_tostring(L, lua_upvalueindex(2));
	// contract_id压栈
	auto contract_id_stack = thinkyoung::lua::lib::get_using_contract_id_stack(L, true);
	if (!contract_id_stack)
		return 0;
	contract_id_stack->push(contract_id);
	lua_pushvalue(L, api_func_index);
	auto args_count = lua_gettop(L) - 1;
	for(int i=0;i<args_count;++i)
	{
		lua_pushvalue(L, 1 + i);
	}
    auto nresults = 1; // FIXME: 是否有返回值，1 or 0
	lua_call(L, args_count, nresults);
	// contract_id出栈
	if (contract_id_stack->size() > 0)
		contract_id_stack->pop();
	return nresults;
}

// 包装合约的api函数
// args: apiFunc, contract_id, ret: wrapedApiFunc
static int contract_api_wrapper(lua_State *L)
{
	// 获取合约id，加入upval/关联到这个closure
	const char *contract_id = luaL_checkstring(L, 2);
	lua_pushvalue(L, 1); // 压函数本身入栈
	lua_pushstring(L, contract_id);
	lua_pushcclosure(L, &contract_api_wrapper_func, 2);
	return 1;
}

static bool contract_table_traverser_to_wrap_api(lua_State *L, void *ud)
{
	if (!lua_isfunction(L, -1))
		return true;
	int contract_table_index = *((int*)ud);
	const char *key = lua_tostring(L, -2);
	lua_getfield(L, contract_table_index, "id");
	const char *contract_id = lua_tostring(L, -1);
	lua_pop(L, 1);
	lua_pushcfunction(L, contract_api_wrapper);
	lua_pushvalue(L, -2);
	lua_pushstring(L, contract_id);
	lua_call(L, 2, 1);
	lua_setfield(L, contract_table_index, key);
	
	return true;
}

static bool lua_get_contract_apis_direct(lua_State *L, GluaModuleByteStream *stream, char *error)
{
    int *stopped_pointer = thinkyoung::lua::lib::get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
    if (nullptr != stopped_pointer && (*stopped_pointer) > 0)
        return false;
    intptr_t stream_p = (intptr_t)stream;
    std::string name = std::string(STREAM_CONTRACT_PREFIX) + std::to_string(stream_p);
    const char *contract_name = name.c_str();

    lua_pushstring(L, name.c_str());
    const char *filename = name.c_str();
    lua_settop(L, 1);  /* _LOADED table will be at index 2 */
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, 2, filename);  /* _LOADED[name] */
    /* else must load package */
    lua_pop(L, 1);  /* remove 'getfield' result */
    if (!findloader_for_import_stream(L, filename))
        return false;
	
    lua_pushstring(L, filename);  /* pass name as argument to module loader */
    lua_insert(L, -2);  /* name is 1st argument (before search data) */

    lua_call(L, 2, 1);  /* run loader to load module */
    if (nullptr != stopped_pointer && (*stopped_pointer) > 0)
        return false;
    if (!lua_isnil(L, -1))  /* non-nil return? */
    {
        if (lua_istable(L, -1)) {
            int it = lua_gettop(L);
            lua_pushnil(L);
            int apis_count = 0;
            char *contract_apis[THINKYOUNG_CONTRACT_APIS_LIMIT];
			memset(contract_apis, 0x0, sizeof(contract_apis));
			std::set<std::string> contract_apis_set; // 用来记录有序的contract_apis集合
			std::set<std::string> offline_contract_apis_set; // 用来记录有序的offline_contract_apis集合
            while (lua_next(L, it))
            {
                if (!lua_isstring(L, -2))
                    continue;
                char *key = (char *)lua_tostring(L, -2);
                if (strcmp(key, "locals") == 0)
                {
                    // save locals to LuaModuleStream
                    if (lua_istable(L, -1))
                    {
                        if (stream->offline_apis.size() > 0)
                        {
							stream->offline_apis.clear();
                        }
                        size_t offline_apis_count;
                        try{
                            lua_len(L, -1);
                            offline_apis_count = (size_t)lua_tointegerx(L, -1, nullptr);
                            lua_pop(L, 1);
                            for (size_t i = 0; i < offline_apis_count; ++i)
                            {
                                lua_geti(L, -1, i + 1);
                                if (!lua_isstring(L, -1))
                                {
                                    lua_pop(L, 1);
                                    continue;
                                }
                                const char *api_name = luaL_checkstring(L, -1);
								offline_contract_apis_set.insert(api_name);
                                lua_pop(L, 1);
                            }
							stream->offline_apis.clear();
							for(const auto &item : offline_contract_apis_set)
							{
								if (stream->offline_apis.size() >= CONTRACT_MAX_OFFLINE_API_COUNT)
									break;
								stream->offline_apis.push_back(item);
							}
                        }
                        catch (...)
                        {

                        }
                    }
                    lua_pop(L, 1);
                    continue;
                }
                lua_pop(L, 1);
                // store module info into thinkyoung, limit not too many apis
                if (strlen(key) > THINKYOUNG_CONTRACT_API_NAME_MAX_LENGTH) {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract module api name must be less than 1024 characters\n");
                    return false;
                }

				contract_apis_set.insert(key);
            }
			for(const auto &item : contract_apis_set)
			{
				contract_apis[apis_count] = (char*)malloc((item.length() + 1) * sizeof(char));
				memset(contract_apis[apis_count], 0x0, (item.length() + 1) * sizeof(char));
				memcpy(contract_apis[apis_count], item.c_str(), sizeof(char) * (item.length() + 1));
				contract_apis[apis_count][item.length()] = '\0';
				apis_count += 1;
			}
            // if the contract info stored in thinkyoung before, fetch and check whether the apis are the same. if not the same, error
            /*char *stored_contract_apis[THINKYOUNG_CONTRACT_APIS_LIMIT];
            int stored_contract_apis_count;*/

			stream->contract_apis.clear();
            for (auto i = 0; i < apis_count; ++i)
            {
				stream->contract_apis.push_back(contract_apis[i]);
            }

            // for (int i = 0; i < apis_count; ++i)
            // {
            // 	free(contract_apis[i]); // need free in all return statement
            // }
            // lua_pop(L, 1); // this line code will pop the module return result from stack
        }
        else {
			const char *msg = "this thinkyoung contract not return a table";
			lua_set_compile_error(L, msg);
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, msg);
            return false;
        }

        lua_fill_contract_info_for_use(L);
        // set name of contract to contract module table
        bool use_self_name = glua::util::starts_with(name, std::string(ADDRESS_CONTRACT_PREFIX)) || glua::util::starts_with(name, std::string(STREAM_CONTRACT_PREFIX));
        lua_pushstring(L, use_self_name ? CURRENT_CONTRACT_NAME : name.c_str());
        lua_setfield(L, -2, "name");
		char contract_id[CONTRACT_ID_MAX_LENGTH] = "\0";
		size_t contract_id_size = 0;
        global_glua_chain_api->get_contract_address_by_name(L, thinkyoung::lua::lib::unwrap_any_contract_name(name.c_str()).c_str(), contract_id, &contract_id_size);
		contract_id[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
        // lua_pushstring(L, CURRENT_CONTRACT_NAME);
		lua_pushstring(L, contract_id);
        lua_setfield(L, -2, "id");

		
        // lua_setfield(L, 2, filename);  /* _LOADED[name] = returned value */
    }
    else
    {
		const char *msg = "this thinkyoung contract not return a table";
		lua_set_compile_error(L, msg);
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, msg);
        return false;
    }
    if (lua_getfield(L, 2, filename) == LUA_TNIL) {   /* module set no value? */
        lua_pushboolean(L, 1);  /* use true as result */
        lua_pushvalue(L, -1);  /* extra copy to be returned */
        lua_setfield(L, 2, filename);  /* _LOADED[name] = true */
    }
    return true;
}

static int lua_get_contract_apis_cfunction(lua_State *L)
{
    if (lua_gettop(L) < 1)
    {
        lua_pushboolean(L, false);
        return 1;
    }
    GluaModuleByteStream *stream = (GluaModuleByteStream*)lua_touserdata(L, 1);
    char *error = lua_gettop(L) > 1 ? (char *)lua_touserdata(L, 2) : nullptr;
    auto result = lua_get_contract_apis_direct(L, stream, error);
    lua_pushboolean(L, result);
    return 1;
}

/**
 * get contract apis in stream
 */
bool luaL_get_contract_apis(lua_State *L, GluaModuleByteStream *stream, char *error)
{
    lua_pushcfunction(L, lua_get_contract_apis_cfunction);
    lua_pushlightuserdata(L, stream);
    if (nullptr != error)
        lua_pushlightuserdata(L, error);
    int args_count = nullptr != error ? 2 : 1;
    lua_pcall(L, args_count, 1, 0);
    if (lua_gettop(L) > 1)
    {
        auto result = lua_toboolean(L, -1);
        lua_pop(L, 2);
        return result > 0 ? true : false;
    }
    else
    {
        lua_pop(L, 1);
        return false;
    }
}

void lua_fill_contract_info_for_use(lua_State *L)
{
    lua_newtable(L);
    lua_setfield(L, -2, "_data");
    lua_getglobal(L, "contract_mt");
    lua_setmetatable(L, -2); // 设置contract的metatable为contract_mt
    // contract.storage = {....}
    lua_newtable(L); // {}
    lua_setfield(L, -2, "storage");	// 
    lua_getfield(L, -1, "storage");	// {}
    lua_pushvalue(L, -2); // {}, contract
    lua_setfield(L, -2, "contract"); // {}
    lua_getglobal(L, "thinkyoung"); // {}, thinkyoung
    lua_pushvalue(L, -2); // {}, thinkyoung, {}
    lua_getfield(L, -2, "storage_mt"); // {}, thinkyoung, {}, storage_mt
    lua_setmetatable(L, -2); // {}, thinkyoung, {}   设置storage的metatable为storage_mt
    lua_pop(L, 3);
}

static std::string unwrap_get_contract_address(std::string namestr)
{
    std::string address;
    std::stringstream ss(namestr.substr(strlen(ADDRESS_CONTRACT_PREFIX)));
    ss >> address;
    return address;
}

static GluaModuleByteStream *unwrap_get_contract_stream(std::string namestr)
{
    intptr_t stream_p;
    std::stringstream ss(namestr.substr(strlen(STREAM_CONTRACT_PREFIX)));
    ss >> stream_p;
    GluaModuleByteStream *stream = (GluaModuleByteStream*)stream_p;
    return stream;
}

static std::string get_contract_name_using_in_lua(std::string namestr)
{
    bool use_self_name = glua::util::starts_with(namestr, ADDRESS_CONTRACT_PREFIX)
        || glua::util::starts_with(namestr, STREAM_CONTRACT_PREFIX);
    return use_self_name ? CURRENT_CONTRACT_NAME : thinkyoung::lua::lib::unwrap_any_contract_name(namestr.c_str());
}

static std::string get_contract_id_using_in_lua(lua_State *L, std::string namestr, bool is_pointer, bool is_stream)
{
    if (is_pointer)
    {
        std::string address;
        std::stringstream(namestr.substr(strlen(ADDRESS_CONTRACT_PREFIX))) >> address;
        return address;
    }
    else if (!is_pointer && !is_stream)
    {
        char address[CONTRACT_ID_MAX_LENGTH];
        memset(address, 0x0, sizeof(char) * CONTRACT_ID_MAX_LENGTH);
        size_t address_len = 0;
        global_glua_chain_api->get_contract_address_by_name(L, thinkyoung::lua::lib::unwrap_any_contract_name(namestr.c_str()).c_str(), address, &address_len);
        address[CONTRACT_ID_MAX_LENGTH-1] = '\0';
        return address;
    }
    else
    {
		auto name = thinkyoung::lua::lib::unwrap_any_contract_name(namestr.c_str());
		char address[CONTRACT_ID_MAX_LENGTH];
		memset(address, 0x0, sizeof(char) * CONTRACT_ID_MAX_LENGTH);
		size_t address_len = 0;
        global_glua_chain_api->get_contract_address_by_name(L, thinkyoung::lua::lib::unwrap_any_contract_name(name.c_str()).c_str(), address, &address_len);
		address[CONTRACT_ID_MAX_LENGTH - 1] = '\0';
		return address;
        // return CURRENT_CONTRACT_NAME;
    }
}

/************************************************************************/
/* FIXME import contract by contract id, need copy code from luaL_import_contract_module */
/************************************************************************/
int luaL_import_contract_module_from_address(lua_State *L)
{
    if (lua_gettop(L) < 1)
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "import_contract_from_address need 1 argument of contract name");
        return 0;
    }
    const char *contract_id = luaL_checkstring(L, 1);
    if (!contract_id)
        return 0;
    const char *name = contract_id;
    if (glua::util::ends_with(std::string(name), std::string(".lua"))
		|| glua::util::ends_with(std::string(name), std::string(GLUA_SOURCE_FILE_EXTENTION_NAME))) {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this contract byte stream not matched with the info stored in thinkyoung api");
        return 0;
    }
    std::string name_str;
    name_str = std::string(ADDRESS_CONTRACT_PREFIX) + thinkyoung::lua::lib::unwrap_any_contract_name(contract_id);
    name = name_str.c_str();
    auto unwrap_name = thinkyoung::lua::lib::unwrap_any_contract_name(contract_id);
    const char *filename = name;
    lua_settop(L, 1);  /* _LOADED table will be at index 2 */
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, 2, filename);  /* _LOADED[name] */
    if (lua_toboolean(L, -1))  /* is it there? */
        return 1;  /* package is already loaded */
    /* else must load package */
    lua_pop(L, 1);  /* remove 'getfield' result */

    size_t global_size_before = luaL_count_global_variables(L);
    std::list<std::string> global_vars_before;
    luaL_get_global_variables(L, &global_vars_before);

    // check whether the contract existed
    bool exists;
    std::string namestr(name);
    exists = global_glua_chain_api->check_contract_exist_by_address(L, contract_id);
    if (!exists)
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this contract not found");
        return 0;
    }
    findloader_for_import_contract(L, name);
    lua_pushstring(L, filename);  /* pass name as argument to module loader */
    lua_insert(L, -2);  /* name is 1st argument (before search data) */
    auto update_loaded_func = [&]() {
        if (lua_getfield(L, 2, filename) == LUA_TNIL) {   /* module set no value? */
            lua_pushboolean(L, 1);  /* use true as result */
            lua_pushvalue(L, -1);  /* extra copy to be returned */
            lua_setfield(L, 2, filename);  /* _LOADED[name] = true */
        }
    };
    struct exit_scope_of_update_loaded
    {
        std::function<void(void)>	_update_loaded_func;
        exit_scope_of_update_loaded(std::function<void(void)> func) : _update_loaded_func(func) {}
        ~exit_scope_of_update_loaded(){
            _update_loaded_func();
        }
    } exit_scope1(update_loaded_func);

    lua_call(L, 2, 1);  /* run loader to load module */
    if (!lua_isnil(L, -1))  /* non-nil return? */
    {
        if (lua_istable(L, -1)) {
            int it = lua_gettop(L);
            lua_pushnil(L);
            int apis_count = 0;
            char *contract_apis[THINKYOUNG_CONTRACT_APIS_LIMIT];
            memset(contract_apis, 0x0, THINKYOUNG_CONTRACT_APIS_LIMIT*sizeof(char*));
            while (lua_next(L, it))
            {
                if (apis_count >= THINKYOUNG_CONTRACT_APIS_LIMIT)
                {
                    lua_pop(L, 1);
                    break;
                }
                if (!lua_isstring(L, -2))
                {
                    lua_pop(L, 1);
                    continue;
                }
                char *key = (char *)lua_tostring(L, -2);

                lua_pop(L, 1);
                // store module info into thinkyoung, limit not too many apis
                if (strlen(key) > THINKYOUNG_CONTRACT_API_NAME_MAX_LENGTH) {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract module api name must be less than %d characters", THINKYOUNG_CONTRACT_API_NAME_MAX_LENGTH);
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
                if (strcmp(key, "locals") == 0)
                    continue;
                contract_apis[apis_count] = (char*)malloc((strlen(key) + 1) * sizeof(char));
				memset(contract_apis[apis_count], 0x0, (strlen(key) + 1) * sizeof(char));
                if (!contract_apis[apis_count])
                {
                    lmalloc_error(L);
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
                memcpy(contract_apis[apis_count], key, sizeof(char) * (strlen(key) + 1));
				contract_apis[apis_count][strlen(key)] = '\0';
                apis_count += 1;
            }
            // if the contract info stored in thinkyoung before, fetch and check whether the apis are the same. if not the same, error
            auto clear_stored_contract_info = [&]() {
                // global_glua_chain_api->free_contract_info(L, unwrap_name.c_str(), stored_contract_apis, &stored_contract_apis_count);
            };
            std::string address = contract_id;
			auto stored_contract_info = std::make_shared<GluaContractInfo>();
            if (global_glua_chain_api->get_stored_contract_info_by_address(L, address.c_str(), stored_contract_info))
            {
                struct exit_scope_of_stored_contract_info
                {
                    std::function<void(void)> _clear_stored_contract_info;
                    exit_scope_of_stored_contract_info(std::function<void(void)> clear_stored_contract_info)
                        : _clear_stored_contract_info(clear_stored_contract_info) {}
                    ~exit_scope_of_stored_contract_info(){
                        _clear_stored_contract_info();
                    }
                } exit_scope1(clear_stored_contract_info);
                // found this contract stored in the thinkyoung api before
                if (stored_contract_info->contract_apis.size() != apis_count)
                {
                    char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH];
                    snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH - 1, "this contract byte stream not matched with the info stored in thinkyoung api, need %d apis but only found %d", stored_contract_info->contract_apis.size(), apis_count);
                    if (strlen(L->compile_error) < 1)
                        memcpy(L->compile_error, error_msg, LUA_COMPILE_ERROR_MAX_LENGTH);
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_msg);
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
                for (auto i = 0; i < apis_count; ++i)
                {
                    auto a = stored_contract_info->contract_apis[i].c_str();
                    int matched = 0;
                    for (int j = 0; j < stored_contract_info->contract_apis.size(); ++j)
                    {
                        char *b = contract_apis[j];
                        if (!a || !b)
                        {
                            char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH];
                            snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH - 1, "empty contract api name");
                            if (strlen(L->compile_error) < 1)
                                memcpy(L->compile_error, error_msg, LUA_COMPILE_ERROR_MAX_LENGTH);
                            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_msg);
                            return 0;
                        }
                        if (strcmp(a, b) == 0)
                        {
                            matched = 1;
                            break;
                        }
                    }
                    if (!matched)
                    {
                        char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH];
                        snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH - 1, "the contract api not match info stored in thinkyoung");
                        if (strlen(L->compile_error) < 1)
                            memcpy(L->compile_error, error_msg, LUA_COMPILE_ERROR_MAX_LENGTH);
                        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_msg);
                        thinkyoung::lua::lib::notify_lua_state_stop(L);
                        return 0;
                    }
                }
                // check _G size whether change
                size_t global_size_after = luaL_count_global_variables(L);
                std::list<std::string> global_vars_after;
                luaL_get_global_variables(L, &global_vars_after);
                if (global_size_before != global_size_after || !glua::util::compare_string_list(global_vars_before, global_vars_after))
                {
                    // check all global variables not changed, don't call code eg. ```_G['abc'] = nil; abc = 1;```
                    char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH] = "\0";
                    /*
                    snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH - 1, "contract can't use global variables");
                    if (strlen(L->compile_error) < 1)
                    memcpy(L->compile_error, error_msg, LUA_COMPILE_ERROR_MAX_LENGTH);
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_msg);
                    */
                    lcompile_error_set(L, error_msg, "contract can't use global variables");
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
            }
            else
            {
                char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH] = "\0";
                /*
                snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH - 1, "contract info not stored before");
                if (strlen(L->compile_error) < 1)
                memcpy(L->compile_error, error_msg, LUA_COMPILE_ERROR_MAX_LENGTH);
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_msg);
                */
                lcompile_error_set(L, error_msg, "contract info not stored before");
                thinkyoung::lua::lib::notify_lua_state_stop(L);
                return 0;
            }
        }
        else {
            char error_msg[LUA_COMPILE_ERROR_MAX_LENGTH] = "\0";
            /*
            snprintf(error_msg, LUA_COMPILE_ERROR_MAX_LENGTH - 1, "this thinkyoung contract not return a table");
            if (strlen(L->compile_error) < 1)
            memcpy(L->compile_error, error_msg, LUA_COMPILE_ERROR_MAX_LENGTH);
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, error_msg);
            */
            lcompile_error_set(L, error_msg, "this thinkyoung contract not return a table");
            return 0;
        }

        lua_fill_contract_info_for_use(L);

        // set name of contract to contract module table
        lua_pushstring(L, get_contract_name_using_in_lua(namestr).c_str());
        lua_setfield(L, -2, "name");
        lua_pushstring(L, contract_id);
        lua_setfield(L, -2, "id");

		auto starting_contract_address = thinkyoung::lua::lib::get_starting_contract_address(L);
        bool is_starting_contract = false;
        if (starting_contract_address.length()>0)
        {
            if (strcmp(contract_id, starting_contract_address.c_str()) == 0)
                is_starting_contract = true;
        }


		// 给合约中各api加上proxy,做pre/post操作，其中压栈合约ID
		auto contract_table_index = lua_gettop(L);
		luaL_traverse_table(L, contract_table_index, contract_table_traverser_to_wrap_api, &contract_table_index);


        if (!is_starting_contract)
        {
			for(const auto &special_api_name : thinkyoung::lua::lib::contract_special_api_names)
			{
				lua_pushnil(L);
				lua_setfield(L, -2, special_api_name.c_str());
			}
        }

        lua_setfield(L, 2, filename);  /* _LOADED[name] = returned value */
    }
    else
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this thinkyoung contract not return a table");
        return 0;
    }
    /*
    if (lua_getfield(L, 2, filename) == LUA_TNIL) {   // module set no value?
    lua_pushboolean(L, 1);  // use true as result
    lua_pushvalue(L, -1);  // extra copy to be returned
    lua_setfield(L, 2, filename);  // _LOADED[name] = true
    }
    */
    return 1;
}

/**
 * import_contract function
 * restrain name must be pure file name, with no '.lua' extension. And can load bytestream from thinkyoung api.
 * try load bytestream first, then try write bytes to tmp file and then load the new file
 */
int luaL_import_contract_module(lua_State *L)
{
    if (lua_gettop(L) < 1 || !lua_isstring(L, 1))
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "import_contract need 1 string argument of contract name");
        return 0;
    }
    const char *origin_contract_name = luaL_checkstring(L, -1);
    const char *name = origin_contract_name;
    if (glua::util::ends_with(name, ".lua"), glua::util::ends_with(name, GLUA_SOURCE_FILE_EXTENTION_NAME)) {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this contract byte stream not matched with the info stored in thinkyoung api");
        return 0;
    }
    bool is_pointer = glua::util::starts_with(name, ADDRESS_CONTRACT_PREFIX);
    bool is_stream = glua::util::starts_with(name, STREAM_CONTRACT_PREFIX);
    std::string name_str;
    if (!is_pointer && !is_stream)
    {
        name_str = thinkyoung::lua::lib::wrap_contract_name(origin_contract_name);
        name = name_str.c_str();
    }
    auto unwrap_name = thinkyoung::lua::lib::unwrap_any_contract_name(origin_contract_name);
    const char *filename = name;
    lua_settop(L, 1);  /* _LOADED table will be at index 2 */
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, 2, filename);  /* _LOADED[name] */
    if (lua_toboolean(L, -1))  /* is it there? */
        return 1;  /* package is already loaded */
    /* else must load package */
    lua_pop(L, 1);  /* remove 'getfield' result */

    size_t global_size_before = luaL_count_global_variables(L);
    std::list<std::string> global_vars_before;
    luaL_get_global_variables(L, &global_vars_before);

    // check whether the contract existed
    bool exists;
    std::string namestr(name);
    if (is_pointer)
    {
        std::string address = unwrap_get_contract_address(namestr);
        exists = global_glua_chain_api->check_contract_exist_by_address(L, address.c_str());
    }
    else if (is_stream)
    {
        GluaModuleByteStream *stream = unwrap_get_contract_stream(namestr);
        exists = true;
    }
    else
    {
        exists = global_glua_chain_api->check_contract_exist(L, origin_contract_name);
    }
    if (!exists)
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this contract not found");
        return 0;
    }
    if (!is_stream)
        findloader_for_import_contract(L, name);
    else
        findloader_for_import_stream(L, filename);
    lua_pushstring(L, filename);  /* pass name as argument to module loader */
    lua_insert(L, -2);  /* name is 1st argument (before search data) */
    auto update_loaded_func = [&]() {
        if (lua_getfield(L, 2, filename) == LUA_TNIL) {   /* module set no value? */
            lua_pushboolean(L, 1);  /* use true as result */
            lua_pushvalue(L, -1);  /* extra copy to be returned */
            lua_setfield(L, 2, filename);  /* _LOADED[name] = true */
        }
    };
    struct exit_scope_of_update_loaded
    {
        std::function<void(void)>	_update_loaded_func;
        exit_scope_of_update_loaded(std::function<void(void)> func) : _update_loaded_func(func) {}
        ~exit_scope_of_update_loaded(){
            _update_loaded_func();
        }
    } exit_scope1(update_loaded_func);

    lua_call(L, 2, 1);  /* run loader to load module */
    if (!lua_isnil(L, -1))  /* non-nil return? */
    {
        if (lua_istable(L, -1)) {
            int it = lua_gettop(L);
            lua_pushnil(L);
            int apis_count = 0;
            char *contract_apis[THINKYOUNG_CONTRACT_APIS_LIMIT];
            while (lua_next(L, it))
            {
                if (apis_count >= THINKYOUNG_CONTRACT_APIS_LIMIT)
                {
                    lua_pop(L, 1);
                    break;
                }
                if (!lua_isstring(L, -2))
                {
                    lua_pop(L, 1);
                    continue;
                }
                char *key = (char *)lua_tostring(L, -2);
                if (strcmp(key, "locals") == 0)
                {
                    lua_pop(L, 1);
                    continue;
                }
                lua_pop(L, 1);
                // store module info into thinkyoung, limit not too many apis
                if (strlen(key) > THINKYOUNG_CONTRACT_API_NAME_MAX_LENGTH) {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract module api name must be less than 1024 characters\n");
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
                contract_apis[apis_count] = (char*)malloc((strlen(key) + 1) * sizeof(char));
				memset(contract_apis[apis_count], 0x0, (strlen(key) + 1) * sizeof(char));
                memcpy(contract_apis[apis_count], key, sizeof(char) * (strlen(key) + 1));
				contract_apis[apis_count][strlen(key)] = '\0';
                apis_count += 1;
            }
            // if the contract info stored in thinkyoung before, fetch and check whether the apis are the same. if not the same, error
			auto stored_contract_info = std::make_shared<GluaContractInfo>();
            auto clear_stored_contract_info = [&]() {
                //if (!is_stream)
                //    global_glua_chain_api->free_contract_info(L, unwrap_name.c_str(), stored_contract_apis, &stored_contract_apis_count);
            };
            std::string address = unwrap_name;
            if (!is_pointer && !is_stream)
            {
                char address_chars[50];
                size_t address_len = 0;
                global_glua_chain_api->get_contract_address_by_name(L, unwrap_name.c_str(), address_chars, &address_len);
                if (address_len > 0)
                    address = std::string(address_chars);
            }
            if (global_glua_chain_api->get_stored_contract_info_by_address(L, address.c_str(), stored_contract_info))
            {
                struct exit_scope_of_stored_contract_info
                {
                    std::function<void(void)> _clear_stored_contract_info;
                    exit_scope_of_stored_contract_info(std::function<void(void)> clear_stored_contract_info)
                        : _clear_stored_contract_info(clear_stored_contract_info) {}
                    ~exit_scope_of_stored_contract_info(){
                        _clear_stored_contract_info();
                    }
                } exit_scope1(clear_stored_contract_info);
                // found this contract stored in the thinkyoung api before
                if (stored_contract_info->contract_apis.size() != apis_count)
                {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this contract byte stream not matched with the info stored in thinkyoung api");
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
                for (auto i = 0; i < apis_count; ++i)
                {
                    auto a = stored_contract_info->contract_apis[i].c_str();
                    int matched = 0;
                    for (int j = 0; j < stored_contract_info->contract_apis.size(); ++j)
                    {
                        char *b = contract_apis[j];
                        if (nullptr == a || nullptr == b)
                        {
                            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "empty contract api name");
                            return 0;
                        }
                        if (strcmp(a, b) == 0)
                        {
                            matched = 1;
                            break;
                        }
                    }
                    if (!matched)
                    {
                        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "the contract api not match info stored in thinkyoung");
                        thinkyoung::lua::lib::notify_lua_state_stop(L);
                        return 0;
                    }
                }
                // check _G size whether change
                size_t global_size_after = luaL_count_global_variables(L);
                std::list<std::string> global_vars_after;
                luaL_get_global_variables(L, &global_vars_after);
                if (global_size_before != global_size_after || !glua::util::compare_string_list(global_vars_before, global_vars_after))
                {
                    // check all global variables not changed, don't call code eg. ```_G['abc'] = nil; abc = 1;```
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract can't use global variables");
                    thinkyoung::lua::lib::notify_lua_state_stop(L);
                    return 0;
                }
            }
            else
            {
                global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "contract info not stored before");
                thinkyoung::lua::lib::notify_lua_state_stop(L);
                return 0;
            }
        }
        else {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this thinkyoung contract not return a table");
            return 0;
        }

        lua_fill_contract_info_for_use(L);

        // set name of contract to contract module table
        lua_pushstring(L, get_contract_name_using_in_lua(namestr).c_str());
        lua_setfield(L, -2, "name");
		auto contract_id = get_contract_id_using_in_lua(L, namestr, is_pointer, is_stream);
        lua_pushstring(L, contract_id.c_str());
        lua_setfield(L, -2, "id");
        // 只有源头合约保留合约的特殊API方法，引用其他合约不能调用这几个函数，import时去除

        auto starting_contract_address = thinkyoung::lua::lib::get_starting_contract_address(L);
        bool is_starting_contract = false;
        if (starting_contract_address.length() > 0)
        {
            if (strcmp(get_contract_id_using_in_lua(L, namestr, is_pointer, is_stream).c_str(),
					starting_contract_address.c_str()) == 0)
                is_starting_contract = true;
        }

        if (!is_starting_contract)
        {
			for(const auto &api_name : thinkyoung::lua::lib::contract_special_api_names)
			{
				lua_pushnil(L);
				lua_setfield(L, -2, api_name.c_str());

				lua_pushstring(L, api_name.c_str());
				lua_pushnil(L);
				lua_rawset(L, -3);
			}
        }

		// 给合约中各api加上proxy,做pre/post操作，其中压栈合约ID
		auto contract_table_index = lua_gettop(L);
		luaL_traverse_table(L, contract_table_index, contract_table_traverser_to_wrap_api, &contract_table_index);

       
        lua_setfield(L, 2, filename);  /* _LOADED[name] = returned value */
    }
    else
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "this thinkyoung contract not return a table");
        return 0;
    }
    /*
    if (lua_getfield(L, 2, filename) == LUA_TNIL) {   // module set no value?
    lua_pushboolean(L, 1);  // use true as result
    lua_pushvalue(L, -1);  // extra copy to be returned
    lua_setfield(L, 2, filename);  // _LOADED[name] = true
    }
    */
    return 1;
}

static int lua_real_execute_contract_api(lua_State *L
  , const char *contract_name, const char *api_name, const char *arg1
)
{
    /*
    if (lua_gettop(L) < 2)
        return 0;
    const char *contract_name = luaL_checkstring(L, 1);
    const char *api_name = luaL_checkstring(L, 2);
     const char *arg1 = lua_gettop(L)>2 && lua_isstring(L, 3) ? luaL_checkstring(L, 3) : nullptr;
     */
	std::string arg1_str = arg1 ? std::string(arg1) : ""; // 这里单独保存一份是因为arg1，可能栈上数据哪里被pop了
    
    // FIXME
    if (!(glua::util::starts_with(contract_name, STREAM_CONTRACT_PREFIX)
        || glua::util::starts_with(contract_name, ADDRESS_CONTRACT_PREFIX))
        && !global_glua_chain_api->check_contract_exist(L, contract_name))
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "can't find this contract");
        lua_pushinteger(L, LUA_ERRRUN);
        return 1;
    }
    bool is_address = glua::util::starts_with(contract_name, ADDRESS_CONTRACT_PREFIX);
    char address[CONTRACT_ID_MAX_LENGTH + 1] = "\0";
    size_t address_size = 0;
    std::string wrapper_contract_name_str = thinkyoung::lua::lib::wrap_contract_name(contract_name);
    std::string unwrapper_name = thinkyoung::lua::lib::unwrap_any_contract_name(contract_name);
    if (!is_address)
        global_glua_chain_api->get_contract_address_by_name(L, unwrapper_name.c_str(), address, &address_size);
    else
    {
        strncpy(address, unwrapper_name.c_str(), CONTRACT_ID_MAX_LENGTH);
        address_size = unwrapper_name.length();
        address[address_size] = '\0';
    }
    // std::stringstream codess;
    // codess << "import_contract '" << contract_name << "'";
    // luaL_dostring(L, codess.str().c_str());
	auto saved_out = L->out;
	auto saved_err = L->err;
	L->out = nullptr;
	L->err = nullptr;
    lua_pushstring(L, contract_name);

	std::string api_name_str(api_name);

    luaL_import_contract_module(L); // FIXME: searcher_Lua添加了时， 这里好像内存越界，改动了api_name, pushnexttemplate这个函数好像有问题
    
    lua_settop(L, 1);  /* _LOADED table will be at index 2 */
    
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, 2, wrapper_contract_name_str.c_str());  /* _LOADED[name] */

	L->out = saved_out;
	L->err = saved_err;

    if (!lua_toboolean(L, -1))  /* is it there? */
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "need load contract before execute contract api");
        lua_pushinteger(L, LUA_ERRRUN);
        return 1;
    }
    if (!lua_istable(L, -1))
    {
        lua_pushinteger(L, LUA_ERRRUN);
        return 1;
    }

    bool is_self = glua::util::starts_with(contract_name, STREAM_CONTRACT_PREFIX)
        || glua::util::starts_with(contract_name, ADDRESS_CONTRACT_PREFIX);

    lua_fill_contract_info_for_use(L);

    lua_pushstring(L, is_self ? CURRENT_CONTRACT_NAME : contract_name);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, address);
    lua_setfield(L, -2, "id");

	for (const auto &special_api_name : thinkyoung::lua::lib::contract_special_api_names)
	{
		if (special_api_name != api_name_str) 
		{
			lua_pushnil(L);
			lua_setfield(L, -2, special_api_name.c_str());
		}
	}

    lua_getfield(L, -1, api_name_str.c_str());
    if (lua_isfunction(L, -1))
    {
        lua_pushvalue(L, -2); // push self		 
        //if (nullptr != arg1)
        //    lua_pushstring(L, arg1);
		if (glua::util::vector_contains(thinkyoung::lua::lib::contract_int_argument_special_api_names, api_name_str))
		{
			std::stringstream arg_ss;
			arg_ss << arg1_str;
			lua_Integer arg1_int = 0;
			arg_ss >> arg1_int;
			lua_pushinteger(L, arg1_int);
		}
		else
		{
			lua_pushstring(L, arg1_str.c_str());
		}

        // lua_pcall(L, nullptr != arg1 ? 2 : 1, 1, 0);
		lua_pcall(L, 2, 1, 0);
        //if (nullptr != arg1)
        //    lua_pop(L, 1);
		lua_pop(L, 1);
        lua_pop(L, 1); // pop self
    } else
    {
		global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "Can't find api %s in this contract", api_name_str.c_str());
		lua_pop(L, 1);
		return 0;
    }
    lua_pop(L, 1);
    lua_pushinteger(L, LUA_OK);
    return 1;
}

LUA_API int lua_execute_contract_api(lua_State *L, const char *contract_name,
	const char *api_name, const char *arg1, std::string *result_json_string)
{
	auto contract_address = thinkyoung::lua::lib::malloc_managed_string(L, CONTRACT_ID_MAX_LENGTH + 1);
	memset(contract_address, 0x0, CONTRACT_ID_MAX_LENGTH + 1);
	size_t address_size = 0;
	global_glua_chain_api->get_contract_address_by_name(L, contract_name, contract_address, &address_size);
	if (address_size > 0)
	{
		GluaStateValue value;
		value.string_value = contract_address;
		thinkyoung::lua::lib::set_lua_state_value(L, STARTING_CONTRACT_ADDRESS, value, LUA_STATE_VALUE_STRING);
	}

	lua_createtable(L, 0, 0);
	lua_setglobal(L, "last_return");
    
    /*
    lua_pushcfunction(L, lua_real_execute_contract_api);
    lua_pushstring(L, contract_name);
    lua_pushstring(L, api_name);
    if (nullptr != arg1)
        lua_pushstring(L, arg1);

    // FIXME: 这里看能不能不需要这样调用而是直接调用
    int status = lua_pcall(L, nullptr != arg1 ? 3 : 2, 1, 0);
    */
    int status = lua_real_execute_contract_api(L, contract_name, api_name, arg1);

    if (lua_gettop(L) < 1)
        return LUA_ERRRUN;
    int result = lua_toboolean(L, -1);
	if (result>0 && result_json_string)
	{
		// FIXME
		lua_getglobal(L, "last_return");
		auto last_return_value_json = luaL_tojsonstring(L, -1, nullptr);
		auto last_return_value_json_string = std::string(last_return_value_json);
		lua_pop(L, 1);
		*result_json_string = last_return_value_json_string;
	}
	if(result)
		result = luaL_commit_storage_changes(L);
    return result > 0 ? LUA_OK : LUA_ERRRUN;
}

LUA_API int lua_execute_contract_api_by_address(lua_State *L, const char *address,
	const char *api_name, const char *arg1, std::string *result_json_string)
{
    std::string name = std::string(ADDRESS_CONTRACT_PREFIX) + std::string(address);
    return lua_execute_contract_api(L, name.c_str(), api_name, arg1, result_json_string);
}

LUA_API int lua_execute_contract_api_by_stream(lua_State *L, GluaModuleByteStream *stream,
	const char *api_name, const char *arg1, std::string *result_json_string)
{
    intptr_t stream_p = (intptr_t)stream;
    std::string name = std::string(STREAM_CONTRACT_PREFIX) + std::to_string(stream_p);
    return lua_execute_contract_api(L, name.c_str(), api_name, arg1, result_json_string);
}

std::shared_ptr<GluaModuleByteStream> lua_common_open_contract(lua_State *L, const char *name, char *error)
{
    std::string namestr(name);
    if (glua::util::starts_with(namestr, ADDRESS_CONTRACT_PREFIX))
    {
        std::string pointer_str = namestr.substr(strlen(ADDRESS_CONTRACT_PREFIX), namestr.length() - strlen(ADDRESS_CONTRACT_PREFIX));
        std::string address;
        std::stringstream(pointer_str) >> address;
        auto stream = global_glua_chain_api->open_contract_by_address(L, address.c_str());
        if (stream && stream->contract_level != CONTRACT_LEVEL_FOREVER && (stream->contract_name.length() < 1 || stream->contract_state == CONTRACT_STATE_DELETED))
        {
            auto start_contract_address = thinkyoung::lua::lib::get_starting_contract_address(L);
            if (start_contract_address.length()>0 && stream->contract_name.length() < 1 && std::string(address) == start_contract_address)
            {
                return stream;
            }
            lerror_set(L, error, "only active and upgraded contract %s can be imported by others", namestr.c_str());
            return nullptr;
        }
        else
            return stream;
    }
    else if (glua::util::starts_with(namestr, STREAM_CONTRACT_PREFIX))
    {
        std::string p_str = namestr.substr(strlen(STREAM_CONTRACT_PREFIX), namestr.length() - strlen(STREAM_CONTRACT_PREFIX));
        intptr_t p;
        std::stringstream(p_str) >> p;
		auto stream = std::make_shared<GluaModuleByteStream>(*(GluaModuleByteStream*)p);
        return stream;
    }
    else
    {
        return global_glua_chain_api->open_contract(L, name);
    }
}

LUALIB_API bool luaL_is_bytecode_file(lua_State *L, const char *filename)
{
	LoadF lf;
	int c;
	bool result = false;
	if (filename == nullptr) {
		lua_pushliteral(L, "=stdin");
		lf.f = stdin;
	}
	else {
		lf.f = fopen(filename, "r");
		if (lf.f == nullptr) return false;
	}
	if (skipcomment(&lf, &c))  /* read initial portion */
		lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
	if (c == LUA_SIGNATURE[0] && filename) {  /* binary file? */
		result = true;
	}
	if (filename)
		fclose(lf.f);
	return result;
}

LUALIB_API int luaL_loadfilex(lua_State *L, const char *filename,
    const char *mode) {
    LoadF lf;
    int status, readstatus;
    int c;
    int fnameindex = lua_gettop(L) + 1;  /* index of filename on the stack */
    if (filename == nullptr) {
        lua_pushliteral(L, "=stdin");
        lf.f = stdin;
    }
    else {
        lua_pushfstring(L, "@%s", filename);
        lf.f = fopen(filename, "r");
        if (lf.f == nullptr) return errfile(L, "open", fnameindex);
    }
    if (skipcomment(&lf, &c))  /* read initial portion */
        lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
    if (c == LUA_SIGNATURE[0] && filename) {  /* binary file? */
        lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
        if (lf.f == nullptr) return errfile(L, "reopen", fnameindex);
        skipcomment(&lf, &c);  /* re-read initial portion */
    }
    if (c != EOF)
        lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
    status = lua_load(L, getF, &lf, lua_tostring(L, -1), mode);
    readstatus = ferror(lf.f);
    if (filename) fclose(lf.f);  /* close file (even in case of errors) */
    if (readstatus) {
        lua_settop(L, fnameindex);  /* ignore results from 'lua_load' */
        return errfile(L, "read", fnameindex);
    }
    lua_remove(L, fnameindex);
    return status;
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
    UNUSED(L);
    int status = (fwrite(p, size, 1, (FILE*)u) != 1) && (size != 0);
    return status;
}

static int writer_to_stream(lua_State *L, const void *p, size_t size, void *u)
{
    UNUSED(L);
	GluaModuleByteStreamP stream = (GluaModuleByteStreamP) u;
    if (!stream)
        return 1;
	auto old_buff_size = stream->buff.size();
	stream->buff.resize(old_buff_size + size);
    memcpy(stream->buff.data() + old_buff_size, p, size);
    return 0;
}

static int lua_compilefile_preload(lua_State *L, LoadF &lf, const char *filename, char *error,
	struct LuaCompileFilePreloadResult *result, bool use_type_check = false, GluaModuleByteStreamP stream = nullptr, bool is_contract = false)
{
    int status, readstatus;
    int c;
    int fnameindex = lua_gettop(L) + 1;
    auto using_filename = filename;
    global_glua_chain_api->clear_exceptions(L);
	auto code = thinkyoung::lua::lib::pre_modify_lua_source_code(L, filename, error, nullptr, use_type_check, stream, is_contract);
	if (global_glua_chain_api->has_exception(L))
    {
        lcompile_error_set(L, error, "compile error");
        return LUA_ERRRUN;
    }
	auto tf = std::tmpfile();
	if (!tf)
	{
		lerror_set(L, error, "can't create tmpfile");
		return LUA_ERRRUN;
	}
	fwrite(code.c_str(), sizeof(char), code.length(), tf);
    // using_filename = using_filename_str.c_str();
    lua_pushfstring(L, "@%s", filename);
    // lf.f = fopen(using_filename, "r");
	fseek(tf, 0, SEEK_SET);
	lf.f = tf;
    if (lf.f == nullptr)
    {
        lcompile_error_set(L, error, "Can't open file %s", using_filename);
        return errfile(L, "open", 1);
    }
    if (skipcomment(&lf, &c))  // read initial portion
        lf.buff[lf.n++] = '\n';  // add line to correct line numbers
    if (c == LUA_SIGNATURE[0] && using_filename) {  // binary file? 
        lf.f = freopen(using_filename, "rb", lf.f);  // reopen in binary mode
        if (lf.f == nullptr) return errfile(L, "reopen", 1);
        skipcomment(&lf, &c);  // re-read initial portion
    }
    if (c != EOF)
        lf.buff[lf.n++] = c; // 'c' is the first character of the stream
    const char *chunk_name = lua_tostring(L, -1);
    result->chunk_name = chunk_name;
	auto saved_out = L->out;
	auto saved_err = L->err;
	L->out = nullptr;
	L->err = nullptr;
    status = lua_load(L, getF, &lf, chunk_name, nullptr);
	L->out = saved_out;
	L->err = saved_err;
    if (status != 0)
    {
        // if (using_filename) fclose(lf.f);
		if (lf.f) fclose(lf.f);
        const char *error_str = "parse thinkyoung lua code error";
        if (strlen(L->compile_error) > 0)
            error_str = L->compile_error;
        if (error)
            strncpy(error, error_str, LUA_COMPILE_ERROR_MAX_LENGTH);
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_PARSER_ERROR, error_str);
        // lcompile_error_set(L, error, "parse thinkyoung lua code error");
        return LUA_ERRFILE;
    }
    readstatus = ferror(lf.f);
    // if (using_filename) fclose(lf.f);  /* close file (even in case of errors) */
	if (lf.f) fclose(lf.f);
    if (readstatus) {
        lua_settop(L, fnameindex);  /* ignore results from 'lua_load' */
        int errresult = errfile(L, "read", fnameindex);
        return errresult;
    }
    lua_remove(L, fnameindex);
    return LUA_OK;
}

LUA_API int luaL_compilefilex(const char *filename, FILE *out_file, char *error, bool is_contract)
{
    thinkyoung::lua::lib::GluaStateScope scope;
    lua_State* L = scope.L();
    LoadF lf;
    struct LuaCompileFilePreloadResult pre_result;
    int preload_status = lua_compilefile_preload(L, lf, filename, error, &pre_result, false, nullptr, is_contract);
    if (preload_status != LUA_OK)
        return preload_status;
    int result_status = lua_compile(L, getF, &lf, pre_result.chunk_name, "text", writer, out_file);
    return result_status;
}

/**
 * copied code from luaL_compilefilex
 */
LUA_API int luaL_compilefile(const char *filename, const char *out_filename, char *error, bool use_type_check, bool is_contract)
{
    thinkyoung::lua::lib::GluaStateScope scope;
    lua_State* L = scope.L();
    LoadF lf;
    struct LuaCompileFilePreloadResult pre_result;
    int preload_status = lua_compilefile_preload(L, lf, filename, error, &pre_result, use_type_check, nullptr, is_contract);
    if (preload_status != LUA_OK)
        return preload_status;
    int result_status = lua_compilex(L, getF, &lf, pre_result.chunk_name, "text", writer, out_filename);
    lcompile_error_get(L, error);
    return result_status;
}

LUA_API int luaL_compilefile_to_stream(lua_State *L, const char *filename,
    GluaModuleByteStreamP stream, char *error, bool use_type_check, bool is_contract)
{
    LoadF lf;
    struct LuaCompileFilePreloadResult pre_result;
    int preload_status = lua_compilefile_preload(L, lf, filename, error, &pre_result, use_type_check, stream, is_contract);
    if (preload_status != LUA_OK)
        return preload_status;
    if (global_glua_chain_api->has_exception(L))
    {
        return LUA_ERRRUN;
    }
    int status = lua_compile_to_stream(L, getF, &lf, pre_result.chunk_name, "text", writer_to_stream, stream);
    stream->is_bytes = true;
    lcompile_error_get(L, error);
    return status;
}

typedef struct LoadS {
    const char *s;
    size_t size;
} LoadS;


static const char *getS(lua_State *L, void *ud, size_t *size) {
    LoadS *ls = (LoadS *)ud;
    (void)L;  /* not used */
    if (ls->size == 0) return nullptr;
    *size = ls->size;
    ls->size = 0;
    return ls->s;
}


LUALIB_API int luaL_loadbufferx(lua_State *L, const char *buff, size_t size,
    const char *name, const char *mode) {
    LoadS ls;
    ls.s = buff;
    ls.size = size;
    return lua_load(L, getS, &ls, name, mode);
}

LUALIB_API int luaL_loadbufferx_with_check(lua_State *L, const char *buff, size_t size,
    const char *name, const char *mode, const int check_type) {
    LoadS ls;
    ls.s = buff;
    ls.size = size;
    return lua_load_with_check(L, getS, &ls, name, mode, check_type);
}


LUALIB_API int luaL_loadstring(lua_State *L, const char *s) {
    return luaL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



LUALIB_API int luaL_getmetafield(lua_State *L, int obj, const char *event) {
    if (!lua_getmetatable(L, obj))  /* no metatable? */
        return LUA_TNIL;
    else {
        int tt;
        lua_pushstring(L, event);
        tt = lua_rawget(L, -2);
        if (tt == LUA_TNIL)  /* is metafield nil? */
            lua_pop(L, 2);  /* remove metatable and metafield */
        else
            lua_remove(L, -2);  /* remove only metatable */
        return tt;  /* return metafield type */
    }
}


LUALIB_API int luaL_callmeta(lua_State *L, int obj, const char *event) {
    obj = lua_absindex(L, obj);
    if (luaL_getmetafield(L, obj, event) == LUA_TNIL)  /* no metafield? */
        return 0;
    lua_pushvalue(L, obj);
    lua_call(L, 1, 1);
    return 1;
}


LUALIB_API lua_Integer luaL_len(lua_State *L, int idx) {
    lua_Integer l;
    int isnum;
    lua_len(L, idx);
    l = lua_tointegerx(L, -1, &isnum);
    if (!isnum)
        luaL_error(L, "object length is not an integer");
    lua_pop(L, 1);  /* remove object */
    return l;
}


LUALIB_API const char *luaL_tolstring(lua_State *L, int idx, size_t *len) {
    if (!luaL_callmeta(L, idx, "__tostring")) {  /* no metafield? */
        switch (lua_type(L, idx)) {
        case LUA_TNUMBER: {
            if (lua_isinteger(L, idx))
            {
              auto value = lua_tointeger(L, idx);
              std::string s = std::to_string(value);
              lua_pushfstring(L, "%s", s.c_str());
            }
            else {
              auto value = lua_tonumber(L, idx);
              std::string s = std::to_string(value);
              lua_pushfstring(L, "%s", s.c_str());
            }
            break;
        }
        case LUA_TSTRING:
            lua_pushvalue(L, idx);
            break;
        case LUA_TBOOLEAN:
            lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
            break;
        case LUA_TNIL:
            lua_pushliteral(L, "nil");
            break;
        default:
            auto value_typename = luaL_typename(L, idx);
			lua_pushfstring(L, "%s: %d", value_typename,
				0);
            //lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
            //    lua_topointer(L, idx));
            break;
        }
    }
    return lua_tolstring(L, -1, len);
}

void free_lua_table_map(lua_State *L, GluaTableMapP map)
{
    // map->~GluaTableMap();
    // lua_free(L, map);
    // delete map;	// TODO: free recur
}

GluaTableMapP luaL_create_lua_table_map_in_memory_pool(lua_State *L)
{
    auto lua_table_map_list_p = thinkyoung::lua::lib::get_lua_state_value(L, LUA_TABLE_MAP_LIST_STATE_MAP_KEY).pointer_value;
    if (nullptr == lua_table_map_list_p)
    {
        lua_table_map_list_p = (void*)new std::list<GluaTableMapP>();
        if (nullptr == lua_table_map_list_p)
        {
            exit(1);
        }
        GluaStateValue value;
        value.pointer_value = lua_table_map_list_p;
        thinkyoung::lua::lib::set_lua_state_value(L, LUA_TABLE_MAP_LIST_STATE_MAP_KEY, value, LUA_STATE_VALUE_POINTER);
    }
    //auto p = (GluaTableMapP)lua_malloc(L, sizeof(GluaTableMap));
    auto p = new GluaTableMap();
    if (nullptr == p)
    {
        global_glua_chain_api->throw_exception(L, THINKYOUNG_API_SIMPLE_ERROR, "out of memory");
        thinkyoung::lua::lib::notify_lua_state_stop(L);
        return nullptr;
    }
    // new(p)GluaTableMap();
    auto list_p = (std::list<GluaTableMapP>*)lua_table_map_list_p;
    list_p->push_back(p);
    return p;
}

/**
* read lua table to hashmap
*/
GluaTableMapP lua_table_to_map_with_nested(lua_State *L, int index, std::list<const void*> &jsons, size_t recur_depth)
{
    if (index > lua_gettop(L))
        return nullptr;
    if (!lua_istable(L, index))
        return nullptr;
    GluaTableMapP map = luaL_create_lua_table_map_in_memory_pool(L);
    luaL_traverse_table_with_nested(L, index, lua_table_to_map_traverser_with_nested, map, jsons, recur_depth);
    return map;
}

struct GluaStorageValue lua_type_to_storage_value_type_with_nested(lua_State *L, int index, size_t len, std::list<const void *> &jsons, size_t recur_depth)
{
    struct GluaStorageValue storage_value;
    if (index > lua_gettop(L))
    {
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_not_support;
        storage_value.value.int_value = 0;
        return storage_value;
    }
	if(recur_depth>LUA_MAP_TRAVERSER_MAX_DEPTH)
	{
		storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
		storage_value.value.int_value = 0;
		return storage_value;
	}
    switch (lua_type(L, index))
    {
    case LUA_TNIL:
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
        storage_value.value.int_value = 0;
        return storage_value;
    case LUA_TBOOLEAN:
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_bool;
        storage_value.value.bool_value = BOOL_VAL(lua_toboolean(L, index));
        return storage_value;
    case LUA_TNUMBER:
        if (lua_isinteger(L, index))
        {
            storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_int;
            storage_value.value.int_value = (lua_Integer)lua_tointeger(L, index);
            return storage_value;
        }
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_number;
        storage_value.value.number_value = lua_tonumber(L, index);
        return storage_value;
    case LUA_TSTRING:
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
        storage_value.value.string_value = const_cast<char*>(lua_tostring(L, index));
        return storage_value;
    case LUA_TTABLE:
        try{
            lua_len(L, index);
        }
        catch (...)
        {
            storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
            storage_value.value.int_value = 0;
            return storage_value;
        }
        len = (size_t)lua_tointegerx(L, -1, nullptr);
        lua_pop(L, 1);
		if(len < 0 || len > INT32_MAX)
		{
			// too big table
			storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_null;
			storage_value.value.int_value = 0;
			return storage_value;
		}
		// FIXME: 根据子项类型修改
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_unknown_table;
        if (len > 0)
            storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_unknown_array;
        storage_value.value.table_value = lua_table_to_map_with_nested(L, index, jsons, recur_depth+1);
        return storage_value;
    case LUA_TUSERDATA:
		// 如果是Stream，转成stream类型
	{
		auto addr = lua_touserdata(L, index);
		if (global_glua_chain_api->is_object_in_pool(L, (intptr_t)addr, GluaOutsideObjectTypes::OUTSIDE_STREAM_STORAGE_TYPE))
		{
			storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_stream;
			storage_value.value.userdata_value = addr;
		}
		else
		{
			storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_userdata;
			storage_value.value.userdata_value = (void *)0; // lua_touserdata(L, index);
		}
		return storage_value;
	}
    case LUA_TFUNCTION:
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_not_support;
		storage_value.value.pointer_value = (void *)0; // (void*)lua_tocfunction(L, index); // 这里用0是为了让不同节点执行结果一致，用内存地址可能会每次执行结果不一样
        return storage_value;
    default:
        storage_value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_not_support;
        storage_value.value.int_value = 0;
        return storage_value;
    }
}

struct GluaStorageValue lua_type_to_storage_value_type(lua_State *L, int index, size_t len)
{
    std::list<const void*> jsons;
    return lua_type_to_storage_value_type_with_nested(L, index, len, jsons, 0);
}

// TODO: 递归层次限制
bool lua_table_to_map_traverser_with_nested(lua_State *L, void *ud, size_t len, std::list<const void*> &jsons, size_t recur_depth)
{
    GluaTableMapP map = (GluaTableMapP) ud;
    if (lua_gettop(L) < 2)
        return false;
    if (!lua_isstring(L, -2) && !lua_isinteger(L, -1)) // now only support integer and string as table key, when using by thinkyoung
        return false;
    std::string key;
	auto key_type = lua_type(L, -2);
	if (key_type == LUA_TBOOLEAN)
		key = std::to_string(lua_toboolean(L, -2));
	else if (lua_isinteger(L, -2))
		key = std::to_string(lua_tointeger(L, -2));
	else if (key_type == LUA_TNUMFLT || key_type == LUA_TNUMINT || key_type == LUA_TNUMBER)
		key = std::to_string(lua_tonumber(L, -2));
	else if (key_type == LUA_TSTRING)
        key = std::string(lua_tostring(L, -2));
    else 
        return false;
	if (key == "package") // FIXME: _G中package模块内容有嵌套递归，暂时全部排除掉避免报错
		return true;
    auto addr = lua_topointer(L, -1);
    GluaStorageValue value;
    bool json_found=false;
    auto jit=jsons.begin();
    while(jit!=jsons.end())
    {
        if(*jit==addr)
        {
            json_found=true;
            break;
        }
        ++jit;
    }
    if (nullptr != addr && lua_istable(L, -1) && json_found)
    {
        value.type = thinkyoung::blockchain::StorageValueTypes::storage_value_string;
        std::string addr_str = std::to_string((intptr_t) addr);
		addr_str = "address";
		char *addr_s = (char*)lua_malloc(L, (1 + addr_str.length()) * sizeof(char));
        memcpy(addr_s, addr_str.c_str(), (1 + addr_str.length()) * sizeof(char));
        value.value.string_value = addr_s;
    }
    else
        value = lua_type_to_storage_value_type_with_nested(L, -1, len, jsons, recur_depth);
    (*map)[key] = value;
    return true;
}

/**
 * 把map转成json写入字符串流
 * @param map 源map
 * @param ss 要写入的字符串流
 * @param is_array 是否作为一个array来进行json转换(因为lua的table中可能包含array和hashmap两部分)
 */
static void luatablemap_to_json_stream(GluaTableMapP map, std::stringstream &ss, bool is_array=false)
{
	if(!is_array)
	{
		// 检查是否是数组
		std::vector<int> all_int_keys;
		bool has_wrong_array_format = false;
		for(const auto &p : *map)
		{
			std::string key(p.first);
			if(key.length()<1)
			{
				has_wrong_array_format = true;
				break;
			}
			int int_key = 0;
			if(key=="0")
			{
				int_key = 0;
			}
			else
			{
				try
				{
					int_key = std::stoi(key);
					if (int_key == 0)
					{
						has_wrong_array_format = true;
						break;
					}
				}catch(std::exception e)
				{
					has_wrong_array_format = true;
					break;
				}
			}
			all_int_keys.push_back(int_key);
		}
		if(!has_wrong_array_format)
		{
			std::sort(all_int_keys.begin(), all_int_keys.end());
			for (int i = 1; i <= all_int_keys.size(); ++i)
			{
				if (i != all_int_keys[i-1])
				{
					has_wrong_array_format = true;
					break;
				}
			}
			if (!has_wrong_array_format)
			{
				is_array = true;
			}
		}
	}

	if (is_array)
		ss << "[";
	else
		ss << "{";
    for (auto it = map->begin(); it != map->end(); ++it)
    {
        if (it != map->begin())
            ss << ",";
        struct GluaStorageValue value = it->second;
		if (!is_array)
		{
			std::string key(it->first);
			ss << "\"" << glua::util::escape_string(key) << "\":";
		}
        switch (value.type)
        {
		case thinkyoung::blockchain::StorageValueTypes::storage_value_null:
            ss << "null";
            break;
		case thinkyoung::blockchain::StorageValueTypes::storage_value_bool:
            ss << (value.value.bool_value ? "true" : "false");
            break;
		case thinkyoung::blockchain::StorageValueTypes::storage_value_int:
            ss << value.value.int_value;
            break;
		case thinkyoung::blockchain::StorageValueTypes::storage_value_number:
            ss << value.value.number_value;
            break;
		case thinkyoung::blockchain::StorageValueTypes::storage_value_string:
        {
            auto str=std::string(value.value.string_value);
            ss << "\"" << glua::util::escape_string(str) << "\"";
            break;
        }
		case thinkyoung::blockchain::StorageValueTypes::storage_value_userdata:
            ss << "\"userdata\"";
		default: 
		{
			if (thinkyoung::blockchain::is_any_table_storage_value_type(value.type)
				|| thinkyoung::blockchain::is_any_array_storage_value_type(value.type))
			{
				luatablemap_to_json_stream(value.value.table_value, ss);
				break;
			}
			ss << "\"userdata\"";
		}
        }
    }
	if (is_array)
		ss << "]";
	else
		ss << "}";
}

static const char *tojsonstring_with_nested(lua_State *L, int idx, size_t *len, std::list<const void*> &jsons)
{
    const void *addr = lua_topointer(L, idx);
    if (nullptr != addr && glua::util::find(jsons.begin(), jsons.end(), addr))
    {
        lua_pushfstring(L, "%p", addr);
        return lua_tolstring(L, -1, len);
    }
    if (!luaL_callmeta(L, idx, "__tojsonstring")) {  /* no metafield? */
        switch (lua_type(L, idx)) {
        case LUA_TNUMBER: {
            if (lua_isinteger(L, idx))
                lua_pushfstring(L, "%I", lua_tointeger(L, idx));
            else
                lua_pushfstring(L, "%f", lua_tonumber(L, idx));
            break;
        }
        case LUA_TSTRING:
            lua_pushvalue(L, idx);
            break;
        case LUA_TBOOLEAN:
            lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
            break;
        case LUA_TNIL:
            lua_pushliteral(L, "nil");
            break;
        case LUA_TTABLE:
        {
            jsons.push_back(addr);
            GluaTableMapP map = luaL_create_lua_table_map_in_memory_pool(L);
            luaL_traverse_table_with_nested(L, idx, lua_table_to_map_traverser_with_nested, map, jsons, 0);
            std::stringstream ss;
            luatablemap_to_json_stream(map, ss);
            if (len)
                *len = ss.str().size();
            lua_pushlstring(L, ss.str().c_str(), ss.str().size());
        }
        break;
        default:
			lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
				(intptr_t) 0);
            //lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
            //    lua_topointer(L, idx));
            break;
        }
    }
    return lua_tolstring(L, -1, len);
}

LUALIB_API const char *(luaL_tojsonstring)(lua_State *L, int idx, size_t *len)
{
    std::list<const void*> jsons;
    return tojsonstring_with_nested(L, idx, len, jsons);
}


/*
** {======================================================
** Compatibility with 5.1 module functions
** =======================================================
*/
#if defined(LUA_COMPAT_MODULE)

static const char *luaL_findtable(lua_State *L, int idx,
    const char *fname, int szhint) {
    const char *e;
    if (idx) lua_pushvalue(L, idx);
    do {
        e = strchr(fname, '.');
        if (e == nullptr) e = fname + strlen(fname);
        lua_pushlstring(L, fname, e - fname);
        if (lua_rawget(L, -2) == LUA_TNIL) {  /* no such field? */
            lua_pop(L, 1);  /* remove this nil */
            lua_createtable(L, 0, (*e == '.' ? 1 : szhint)); /* new table for field */
            lua_pushlstring(L, fname, e - fname);
            lua_pushvalue(L, -2);
            lua_settable(L, -4);  /* set new table into field */
        }
        else if (!lua_istable(L, -1)) {  /* field has a non-table value? */
            lua_pop(L, 2);  /* remove table and value */
            return fname;  /* return problematic part of the name */
        }
        lua_remove(L, -2);  /* remove previous table */
        fname = e + 1;
    } while (*e == '.');
    return nullptr;
}


/*
** Count number of elements in a luaL_Reg list.
*/
static int libsize(const luaL_Reg *l) {
    int size = 0;
    for (; l && l->name; l++) size++;
    return size;
}


/*
** Find or create a module table with a given name. The function
** first looks at the _LOADED table and, if that fails, try a
** global variable with that name. In any case, leaves on the stack
** the module table.
*/
LUALIB_API void luaL_pushmodule(lua_State *L, const char *modname,
    int sizehint) {
    luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 1);  /* get _LOADED table */
    if (lua_getfield(L, -1, modname) != LUA_TTABLE) {  /* no _LOADED[modname]? */
        lua_pop(L, 1);  /* remove previous result */
        /* try global variable (and create one if it does not exist) */
        lua_pushglobaltable(L);
        if (luaL_findtable(L, 0, modname, sizehint) != nullptr)
            luaL_error(L, "name conflict for module '%s'", modname);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, modname);  /* _LOADED[modname] = new table */
    }
    lua_remove(L, -2);  /* remove _LOADED table */
}


LUALIB_API void luaL_openlib(lua_State *L, const char *libname,
    const luaL_Reg *l, int nup) {
    luaL_checkversion(L);
    if (libname) {
        luaL_pushmodule(L, libname, libsize(l));  /* get/create library table */
        lua_insert(L, -(nup + 1));  /* move library table to below upvalues */
    }
    if (l)
        luaL_setfuncs(L, l, nup);
    else
        lua_pop(L, nup);  /* remove upvalues */
}

#endif
/* }====================================================== */

/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LUALIB_API void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
    luaL_checkstack(L, nup, "too many upvalues");
    for (; l->name != nullptr; l++) {  /* fill the table with given functions */
        int i;
        for (i = 0; i < nup; i++)  /* copy upvalues to the top */
            lua_pushvalue(L, -nup);
        lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
        lua_setfield(L, -(nup + 2), l->name);
    }
    lua_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
LUALIB_API int luaL_getsubtable(lua_State *L, int idx, const char *fname) {
    if (lua_getfield(L, idx, fname) == LUA_TTABLE)
        return 1;  /* table already there */
    else {
        lua_pop(L, 1);  /* remove previous result */
        idx = lua_absindex(L, idx);
        lua_newtable(L);
        lua_pushvalue(L, -1);  /* copy to be left at top */
        lua_setfield(L, idx, fname);  /* assign new table to field */
        return 0;  /* false, because did not find table there */
    }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
LUALIB_API void luaL_requiref(lua_State *L, const char *modname,
    lua_CFunction openf, int glb) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, -1, modname);  /* _LOADED[modname] */
    if (!lua_toboolean(L, -1)) {  /* package not already loaded? */
        lua_pop(L, 1);  /* remove field */
        lua_pushcfunction(L, openf);
        lua_pushstring(L, modname);  /* argument to open function */
        lua_call(L, 1, 1);  /* call 'openf' to open module */
        lua_pushvalue(L, -1);  /* make copy of module (call result) */
        lua_setfield(L, -3, modname);  /* _LOADED[modname] = module */
    }
    lua_remove(L, -2);  /* remove _LOADED table */
    if (glb) {
        lua_pushvalue(L, -1);  /* copy of module */
        lua_setglobal(L, modname);  /* _G[modname] = module */
    }
}


LUALIB_API const char *luaL_gsub(lua_State *L, const char *s, const char *p,
    const char *r) {
    const char *wild;
    size_t l = strlen(p);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while ((wild = strstr(s, p)) != nullptr) {
        luaL_addlstring(&b, s, wild - s);  /* push prefix */
        luaL_addstring(&b, r);  /* push replacement in place of pattern */
        s = wild + l;  /* continue after 'p' */
    }
    luaL_addstring(&b, s);  /* push last suffix */
    luaL_pushresult(&b);
    return lua_tostring(L, -1);
}


static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  /* not used */
    if (nsize == 0) {
        free(ptr);
        return nullptr;
    }
    else
        return realloc(ptr, nsize);
}


static int panic(lua_State *L) {
    lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
        lua_tostring(L, -1));
    return 0;  /* return to Lua to abort */
}


LUALIB_API lua_State *luaL_newstate(void) {
    lua_State *L = lua_newstate(l_alloc, nullptr);
    if (L) lua_atpanic(L, &panic);
    return L;
}


LUALIB_API void luaL_checkversion_(lua_State *L, lua_Number ver, size_t sz) {
    const lua_Number *v = lua_version(L);
    if (sz != LUAL_NUMSIZES)  /* check numeric types */
        luaL_error(L, "core and library have incompatible numeric types");
    if (v != lua_version(nullptr))
        luaL_error(L, "multiple Lua VMs detected");
    else if (*v != ver)
        luaL_error(L, "version mismatch: app. needs %f, Lua core provides %f",
        ver, *v);
}

