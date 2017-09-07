/*
** $Id: lstate.c,v 2.133 2015/11/13 12:16:51 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#define lstate_cpp
#define LUA_CORE

#include "glua/lprefix.h"


#include <stddef.h>
#include <string.h>
#include <cstdint>

#include "glua/lua.h"

#include "glua/lapi.h"
#include "glua/ldebug.h"
#include "glua/ldo.h"
#include "glua/lfunc.h"
#include "glua/lgc.h"
#include "glua/llex.h"
#include "glua/lmem.h"
#include "glua/lstate.h"
#include "glua/lstring.h"
#include "glua/ltable.h"
#include "glua/ltm.h"
#include "glua/thinkyoung_lua_api.h"
#include "glua/thinkyoung_lua_lib.h"


#if !defined(LUAI_GCPAUSE)
#define LUAI_GCPAUSE	200  /* 200% */
#endif

#if !defined(LUAI_GCMUL)
#define LUAI_GCMUL	200 /* GC runs 'twice the speed' of memory allocation */
#endif


/*
** a macro to help the creation of a unique random seed when a state is
** created; the seed is used to randomize hashes.
*/
#if !defined(luai_makeseed)
#include <time.h>
#define luai_makeseed()		lua_cast(unsigned int, time(nullptr))
#endif



/*
** thread state + extra space
*/
typedef struct LX {
    lu_byte extra_[LUA_EXTRASPACE];
    lua_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
    LX l;
    global_State g;
} LG;



#define fromstate(L)	(lua_cast(LX *, lua_cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** Compute an initial seed as random as possible. Rely on Address Space
** Layout Randomization (if present) to increase randomness..
*/
#define addbuff(b,p,e) \
  { size_t t = lua_cast(size_t, e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int makeseed(lua_State *L) {
    char buff[4 * sizeof(size_t)];
    unsigned int h = luai_makeseed();
    int p = 0;
    addbuff(buff, p, L);  /* heap variable */
    addbuff(buff, p, &h);  /* local variable */
    addbuff(buff, p, luaO_nilobject);  /* global variable */
    addbuff(buff, p, &lua_newstate);  /* public function */
    lua_assert(p == sizeof(buff));
    return luaS_hash(buff, p, h);
}


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void luaE_setdebt(global_State *g, l_mem debt) {
    l_mem tb = gettotalbytes(g);
    lua_assert(tb > 0);
    if (debt < tb - MAX_LMEM)
        debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
    g->totalbytes = tb - debt;
    g->GCdebt = debt;
}


CallInfo *luaE_extendCI(lua_State *L) {
    CallInfo *ci = luaM_new(L, CallInfo);
    lua_assert(L->ci->next == nullptr);
    L->ci->next = ci;
    ci->previous = L->ci;
    ci->next = nullptr;
    L->nci++;
    return ci;
}


/*
** free all CallInfo structures not in use by a thread
*/
void luaE_freeCI(lua_State *L) {
    CallInfo *ci = L->ci;
    CallInfo *next = ci->next;
    ci->next = nullptr;
    while ((ci = next) != nullptr) {
        next = ci->next;
        luaM_free(L, ci);
        L->nci--;
    }
}


/*
** free half of the CallInfo structures not in use by a thread
*/
void luaE_shrinkCI(lua_State *L) {
    CallInfo *ci = L->ci;
    CallInfo *next2;  /* next's next */
    /* while there are two nexts */
    while (ci->next != nullptr && (next2 = ci->next->next) != nullptr) {
        luaM_free(L, ci->next);  /* free next */
        L->nci--;
        ci->next = next2;  /* remove 'next' from the list */
        next2->previous = ci;
        ci = next2;  /* keep next's next */
    }
}


static void stack_init(lua_State *L1, lua_State *L) {
    int i; CallInfo *ci;
    /* initialize stack array */
    L1->stack = luaM_newvector(L, BASIC_STACK_SIZE, TValue);
    L1->stacksize = BASIC_STACK_SIZE;
    for (i = 0; i < BASIC_STACK_SIZE; i++)
        setnilvalue(L1->stack + i);  /* erase new stack */
    L1->top = L1->stack;
    L1->stack_last = L1->stack + L1->stacksize - EXTRA_STACK;
    /* initialize first ci */
    ci = &L1->base_ci;
    ci->next = ci->previous = nullptr;
    ci->callstatus = 0;
    ci->func = L1->top;
    setnilvalue(L1->top++);  /* 'function' entry for this 'ci' */
    ci->top = L1->top + LUA_MINSTACK;
    L1->ci = ci;
}


static void freestack(lua_State *L) {
    if (L->stack == nullptr)
        return;  /* stack not completely built yet */
    L->ci = &L->base_ci;  /* free the entire 'ci' list */
    luaE_freeCI(L);
    lua_assert(L->nci == 0);
    luaM_freearray(L, L->stack, L->stacksize);  /* free stack array */
}


/*
** Create registry table and its predefined values
*/
static void init_registry(lua_State *L, global_State *g) {
    TValue temp;
    /* create registry */
    Table *registry = luaH_new(L);
    sethvalue(L, &g->l_registry, registry);
    luaH_resize(L, registry, LUA_RIDX_LAST, 0);
    /* registry[LUA_RIDX_MAINTHREAD] = L */
    setthvalue(L, &temp, L);  /* temp = L */
    luaH_setint(L, registry, LUA_RIDX_MAINTHREAD, &temp);
    /* registry[LUA_RIDX_GLOBALS] = table of globals */
    sethvalue(L, &temp, luaH_new(L));  /* temp = new table (global table) */
    luaH_setint(L, registry, LUA_RIDX_GLOBALS, &temp);
}


/*
** open parts of the state that may cause memory-allocation errors.
** ('g->version' != nullptr flags that the state was completely build)
*/
static void f_luaopen(lua_State *L, void *ud) {
    global_State *g = G(L);
    UNUSED(ud);
    stack_init(L, L);  /* init stack */
    init_registry(L, g);
    luaS_init(L);
    luaT_init(L);
    luaX_init(L);
    g->gcrunning = 1;  /* allow gc */
    g->version = lua_version(nullptr);
    luai_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread(lua_State *L, global_State *g) {
    G(L) = g;
    L->stack = nullptr;
    L->ci = nullptr;
    L->nci = 0;
    L->stacksize = 0;
    L->twups = L;  /* thread has no upvalues */
    L->errorJmp = nullptr;
    L->nCcalls = 0;
    L->hook = nullptr;
    L->hookmask = 0;
    L->basehookcount = 0;
    L->allowhook = 1;
    resethookcount(L);
    L->openupval = nullptr;
    L->nny = 1;
    L->status = LUA_OK;
    L->errfunc = 0;
}


static void close_state(lua_State *L) {
    global_State *g = G(L);
    luaF_close(L, L->stack);  /* close all upvalues for this thread */
    luaC_freeallobjects(L);  /* collect all objects */
    if (g->version)  /* closing a fully built state? */
        luai_userstateclose(L);
    luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
    freestack(L);
    lua_assert(gettotalbytes(g) == sizeof(LG));
    (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


// TODO: lua_State中的新字段没有设置，不过暂时没用到协程
LUA_API lua_State *lua_newthread(lua_State *L) {
    global_State *g = G(L);
    lua_State *L1;
    lua_lock(L);
    luaC_checkGC(L);
    /* create new thread */
    L1 = &lua_cast(LX *, luaM_newobject(L, LUA_TTHREAD, sizeof(LX)))->l;
    L1->marked = luaC_white(g);
    L1->tt = LUA_TTHREAD;
    /* link it on list 'allgc' */
    L1->next = g->allgc;
    g->allgc = obj2gco(L1);
    /* anchor it on L stack */
    setthvalue(L, L->top, L1);
    api_incr_top(L);
    preinit_thread(L1, g);
    L1->hookmask = L->hookmask;
    L1->basehookcount = L->basehookcount;
    L1->hook = L->hook;
    resethookcount(L1);
    /* initialize L1 extra space */
    memcpy(lua_getextraspace(L1), lua_getextraspace(g->mainthread),
        LUA_EXTRASPACE);
    luai_userstatethread(L, L1);
    stack_init(L1, L);  /* init stack */
    lua_unlock(L);
    return L1;
}


void luaE_freethread(lua_State *L, lua_State *L1) {
    LX *l = fromstate(L1);
    luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
    lua_assert(L1->openupval == nullptr);
    luai_userstatefree(L, L1);
    freestack(L1);
    luaM_free(L, l);
}


LUA_API lua_State *lua_newstate(lua_Alloc f, void *ud) {
    int i;
    lua_State *L;
    global_State *g;
    LG *l = lua_cast(LG *, (*f)(ud, nullptr, LUA_TTHREAD, sizeof(LG)));
    if (l == nullptr) return nullptr;
    L = &l->l.l;
    g = &l->g;
    L->next = nullptr;
    L->tt = LUA_TTHREAD;
    g->currentwhite = bitmask(WHITE0BIT);
    L->marked = luaC_white(g);
    L->malloc_buffer = malloc(LUA_MALLOC_TOTAL_SIZE);
    L->malloc_pos = 0;
    L->malloced_buffers = new std::list<std::pair<ptrdiff_t, ptrdiff_t>>();
    memset(L->compile_error, 0x0, LUA_COMPILE_ERROR_MAX_LENGTH);
	memset(L->runerror, 0x0, LUA_VM_EXCEPTION_STRNG_MAX_LENGTH);
	L->bytecode_debugger_opened = false;
    L->in = stdin;
    L->out = stdout;
    L->err = stderr;
    L->force_stopping = false;
	L->exit_code = 0;
    L->debugger_pausing = false;
    L->preprocessor = nullptr;
    preinit_thread(L, g);
    g->frealloc = f;
    g->ud = ud;
    g->mainthread = L;
    g->seed = makeseed(L);
    g->gcrunning = 0;  /* no GC while building state */
    g->GCestimate = 0;
    g->strt.size = g->strt.nuse = 0;
    g->strt.hash = nullptr;
    setnilvalue(&g->l_registry);
    g->panic = nullptr;
    g->version = nullptr;
    g->gcstate = GCSpause;
    g->gckind = KGC_NORMAL;
    g->allgc = g->finobj = g->tobefnz = g->fixedgc = nullptr;
    g->sweepgc = nullptr;
    g->gray = g->grayagain = nullptr;
    g->weak = g->ephemeron = g->allweak = nullptr;
    g->twups = nullptr;
    g->totalbytes = sizeof(LG);
    g->GCdebt = 0;
    g->gcfinnum = 0;
    g->gcpause = LUAI_GCPAUSE;
    g->gcstepmul = LUAI_GCMUL;
    for (i = 0; i < LUA_NUMTAGS; i++) g->mt[i] = nullptr;
    if (luaD_rawrunprotected(L, f_luaopen, nullptr) != LUA_OK) {
        /* memory allocation error: free partial state */
        close_state(L);
        L = nullptr;
    }
    return L;
}


LUA_API void lua_close(lua_State *L) {
    L = G(L)->mainthread;  /* only the main thread can be closed */
    thinkyoung::lua::lib::close_lua_state_values(L);
    delete L->malloced_buffers;
    free(L->malloc_buffer);
    lua_lock(L);
    close_state(L);
}

static size_t align8(size_t s) {
    if ((s & 0x7) == 0)
        return s;
    return ((s >> 3) + 1) << 3;
};

// FIXME: use memory page, and use best fit malloc strategy
// TODO: 改成根据需要分阶梯申请内存池，不一开始就申请最大内存池（分成多个阶梯式大小的内存池）
void *lua_malloc(lua_State *L, size_t size)
{
    size = align8(size);
    if (size > LUA_MALLOC_TOTAL_SIZE)
    {
        thinkyoung::lua::lib::notify_lua_state_stop(L);
        return nullptr;
    }
    if (L->malloced_buffers->size() < 1)
    {
        auto offset = L->malloc_pos;
        void *p = (void*)((intptr_t)(L->malloc_buffer) + offset);
        L->malloc_pos += size;
        L->malloced_buffers->push_back(std::make_pair(offset, size));
        return p;
    }
    std::pair<ptrdiff_t, ptrdiff_t> last_pair;
    auto begin = L->malloced_buffers->begin();
    for (auto it = begin; it != L->malloced_buffers->end(); ++it)
    {
        if (it == begin)
        {
            if (it->first > (ptrdiff_t)size)
            {
                // can alloc memory before first block
                ptrdiff_t offset = 0;
                void *p = L->malloc_buffer;
                L->malloced_buffers->insert(L->malloced_buffers->begin(), std::make_pair(offset, size));
                return p;
            }
            last_pair = *it;
            continue;
        }
        if (it->first >= last_pair.first + last_pair.second + (ptrdiff_t)size)
        {
            ptrdiff_t offset = last_pair.first + last_pair.second;
            void *p = (void*)((intptr_t)(L->malloc_buffer) + offset);
            L->malloced_buffers->insert(it, std::make_pair(offset, size));
            return p;
        }
        last_pair = *it;
    }
    if (L->malloc_pos + size > LUA_MALLOC_TOTAL_SIZE)
    {
        thinkyoung::lua::lib::notify_lua_state_stop(L);
        return nullptr;
    }
    ptrdiff_t offset = L->malloc_pos;
    void *p = (void*)((intptr_t)(L->malloc_buffer) + offset);
    L->malloced_buffers->push_back(std::make_pair(offset, size));
    L->malloc_pos += size;
    return p;
}

void *lua_calloc(lua_State *L, size_t element_count, size_t element_size)
{
    void *p = lua_malloc(L, element_count * element_size);
    if (nullptr == p)
        return nullptr;
    memset(p, 0, element_count * element_size);
    return p;
}

void lua_free(lua_State *L, void *address)
{
    if (nullptr == address || nullptr == L)
        return;
    auto offset = (intptr_t)address - (intptr_t)L->malloc_buffer;
    if (offset < 0 || offset > LUA_MALLOC_TOTAL_SIZE)
        return;
    size_t offset_size = (size_t)offset;
    for (auto it = L->malloced_buffers->begin(); it != L->malloced_buffers->end(); ++it)
    {
        if (it->first == offset_size)
        {
            L->malloced_buffers->erase(it);
            return;
        }
    }
}
