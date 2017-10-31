/*
** $Id: lvm.c,v 2.265 2015/11/23 11:30:45 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#define lvm_cpp
#define LUA_CORE

#include <glua/lprefix.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

#include <glua/lua.h>

#include <glua/ldebug.h>
#include <glua/ldo.h>
#include <glua/lfunc.h>
#include <glua/lgc.h>
#include <glua/lobject.h>
#include <glua/lopcodes.h>
#include <glua/lauxlib.h>
#include <glua/lstate.h>
#include <glua/lstring.h>
#include <glua/ltable.h>
#include <glua/ltm.h>
#include <glua/lvm.h>
#include <glua/thinkyoung_lua_api.h>
#include <glua/thinkyoung_lua_lib.h>
#include <glua/glua_debug_file.h>
#include <glua/lremote_debugger.h>

using thinkyoung::lua::api::global_glua_chain_api;


/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP	2000



/*
** 'l_intfitsf' checks whether a given integer can be converted to a
** float without rounding. Used in comparisons. Left undefined if
** all integers fit in a float precisely.
*/
#if !defined(l_intfitsf)

/* number of bits in the mantissa of a float */
#define NBM		(l_mathlim(MANT_DIG))

/*
** Check whether some integers may not fit in a float, that is, whether
** (maxinteger >> NBM) > 0 (that implies (1 << NBM) <= maxinteger).
** (The shifts are done in parts to avoid shifting by more than the size
** of an integer. In a worst case, NBM == 113 for long double and
** sizeof(integer) == 32.)
*/
#if ((((LUA_MAXINTEGER >> (NBM / 4)) >> (NBM / 4)) >> (NBM / 4)) \
	>> (NBM - (3 * (NBM / 4))))  >  0

#define l_intfitsf(i)  \
  (-((lua_Integer)1 << NBM) <= (i) && (i) <= ((lua_Integer)1 << NBM))

#endif

#endif



/*
** Try to convert a value to a float. The float case is already handled
** by the macro 'tonumber'.
*/
int luaV_tonumber_(const TValue *obj, lua_Number *n) {
    TValue v;
    if (ttisinteger(obj)) {
        *n = cast_num(ivalue(obj));
        return 1;
    }
    else if (cvt2num(obj) &&  /* string convertible to number? */
        luaO_str2num(svalue(obj), &v) == vslen(obj) + 1) {
        *n = nvalue(&v);  /* convert result of 'luaO_str2num' to a float */
        return 1;
    }
    else {
        return 0;  // conversion failed
    }
}


/*
** try to convert a value to an integer, rounding according to 'mode':
** mode == 0: accepts only integral values
** mode == 1: takes the floor of the number
** mode == 2: takes the ceil of the number
*/
int luaV_tointeger(const TValue *obj, lua_Integer *p, int mode) {
    TValue v;
again:
    if (ttisfloat(obj)) {
        lua_Number n = fltvalue(obj);
        lua_Number f = l_floor(n);
        if (n != f) {  /* not an integral value? */
            if (mode == 0) return 0;  /* fails if mode demands integral value */
            else if (mode > 1)  /* needs ceil? */
                f += 1;  /* convert floor to ceil (remember: n != f) */
        }
        return lua_numbertointeger(f, p);
    }
    else if (ttisinteger(obj)) {
        *p = ivalue(obj);
        return 1;
    }
    else if (cvt2num(obj) &&
        luaO_str2num(svalue(obj), &v) == vslen(obj) + 1) {
        obj = &v;
        goto again;  /* convert result from 'luaO_str2num' to an integer */
    }
    return 0;  /* conversion failed */
}


/*
** Try to convert a 'for' limit to an integer, preserving the
** semantics of the loop.
** (The following explanation assumes a non-negative step; it is valid
** for negative steps mutatis mutandis.)
** If the limit can be converted to an integer, rounding down, that is
** it.
** Otherwise, check whether the limit can be converted to a number.  If
** the number is too large, it is OK to set the limit as LUA_MAXINTEGER,
** which means no limit.  If the number is too negative, the loop
** should not run, because any initial integer value is larger than the
** limit. So, it sets the limit to LUA_MININTEGER. 'stopnow' corrects
** the extreme case when the initial value is LUA_MININTEGER, in which
** case the LUA_MININTEGER limit would still run the loop once.
*/
static int forlimit(const TValue *obj, lua_Integer *p, lua_Integer step,
    int *stopnow) {
    *stopnow = 0;  /* usually, let loops run */
    //  printf("forlimit\n");
    if (!luaV_tointeger(obj, p, (step < 0 ? 2 : 1))) {  /* not fit in integer? */
        lua_Number n;  /* try to convert to float */
        if (!tonumber(obj, &n)) /* cannot convert to float? */
            return 0;  /* not a number */
        if (luai_numlt(0, n)) {  /* if true, float is larger than max integer */
            *p = LUA_MAXINTEGER;
            if (step < 0) *stopnow = 1;
        }
        else {  /* float is smaller than min integer */
            *p = LUA_MININTEGER;
            if (step >= 0) *stopnow = 1;
        }
    }
    lua_Number  n2;
    if (!tonumber(obj, &n2)) {
        return 0;
    }
    return 1;
}


/*
** Complete a table access: if 't' is a table, 'tm' has its metamethod;
** otherwise, 'tm' is nullptr.
*/
void luaV_finishget(lua_State *L, const TValue *t, TValue *key, StkId val,
    const TValue *tm) {
    int loop;  /* counter to avoid infinite loops */
    lua_assert(tm != nullptr || !ttistable(t));
    for (loop = 0; loop < MAXTAGLOOP; loop++) {
        if (tm == nullptr) {  /* no metamethod (from a table)? */
            if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_INDEX)))
                luaG_typeerror(L, t, "index");  /* no metamethod */
        }
        if (ttisfunction(tm)) {  /* metamethod is a function */
            luaT_callTM(L, tm, t, key, val, 1);  /* call it */
            return;
        }
        t = tm;  /* else repeat access over 'tm' */
        if (luaV_fastget(L, t, key, tm, luaH_get)) {  /* try fast track */
            setobj2s(L, val, tm);  /* done */
            return;
        }
        /* else repeat */
    }
    luaG_runerror(L, "gettable chain too long; possible loop");
}


/*
** Main function for table assignment (invoking metamethods if needed).
** Compute 't[key] = val'
*/
void luaV_finishset(lua_State *L, const TValue *t, TValue *key,
    StkId val, const TValue *oldval) {
    int loop;  /* counter to avoid infinite loops */
    for (loop = 0; loop < MAXTAGLOOP; loop++) {
        const TValue *tm;
        if (oldval != nullptr) {
            // lua_assert(ttistable(t) && ttisnil(oldval));
            Table *h = hvalue(t); // save t table
            lua_assert(ttisnil(oldval));
            /* must check the metamethod */
            // if ((tm = fasttm(L, hvalue(t)->metatable, TM_NEWINDEX)) == nullptr &&
            if ((tm = fasttm(L, h->metatable, TM_NEWINDEX)) == nullptr &&
                /* no metamethod; is there a previous entry in the table? */
                (oldval != luaO_nilobject ||
                /* no previous entry; must create one. (The next test is
                   always true; we only need the assignment.) */
                   // (oldval = luaH_newkey(L, hvalue(t), key), 1))) {
                   (oldval = luaH_newkey(L, h, key), 1))) {
                /* no metamethod and (now) there is an entry with given key */
                setobj2t(L, lua_cast(TValue *, oldval), val);
                // invalidateTMcache(hvalue(t));
                // luaC_barrierback(L, hvalue(t), val);
                invalidateTMcache(h);
                luaC_barrierback(L, h, val);
                return;
            }
            /* else will try the metamethod */
        }
        else {  /* not a table; check metamethod */
            if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_NEWINDEX)))
                luaG_typeerror(L, t, "index");
            if (L->force_stopping)
                return;
        }
        /* try the metamethod */
        if (ttisfunction(tm)) {
            luaT_callTM(L, tm, t, key, val, 0);
            return;
        }
        t = tm;  /* else repeat assignment over 'tm' */
        if (luaV_fastset(L, t, key, oldval, luaH_get, val))
            return;  /* done */
        /* else loop */
    }
    luaG_runerror(L, "settable chain too long; possible loop");
}


/*
** Compare two strings 'ls' x 'rs', returning an integer smaller-equal-
** -larger than zero if 'ls' is smaller-equal-larger than 'rs'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segments
** of the strings.
*/
static int l_strcmp(const TString *ls, const TString *rs) {
    const char *l = getstr(ls);
    size_t ll = tsslen(ls);
    const char *r = getstr(rs);
    size_t lr = tsslen(rs);
	if (ll != lr)
		return (int)ll - (int) lr;
    for (;;) {  /* for each segment */
        int temp = strcoll(l, r);
        if (temp != 0)  /* not equal? */
            return temp;  /* done */
        else {  /* strings are equal up to a '\0' */
            size_t len = strlen(l);  /* index of first '\0' in both strings */
            if (len == lr)  /* 'rs' is finished? */
                return (len == ll) ? 0 : 1;  /* check 'ls' */
            else if (len == ll)  /* 'ls' is finished? */
                return -1;  /* 'ls' is smaller than 'rs' ('rs' is not finished) */
            /* both strings longer than 'len'; go on comparing after the '\0' */
            len++;
            l += len; ll -= len; r += len; lr -= len;
        }
    }
}

static int l_c_str_cmp(const char *l, const char *r)
{
	if (strcmp(l, r) == 0)
		return 0;
	lua_table_less cmp_op;
	return cmp_op(l, r) ? -1 : 1;
}


/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, if 'f' is outside the range for integers, result
** is trivial. Otherwise, compare them as integers. (When 'i' has no
** float representation, either 'f' is "far away" from 'i' or 'f' has
** no precision left for a fractional part; either way, how 'f' is
** truncated is irrelevant.) When 'f' is NaN, comparisons must result
** in false.
*/
static int LTintfloat(lua_Integer i, lua_Number f) {
#if defined(l_intfitsf)
    if (!l_intfitsf(i)) {
        if (f >= -cast_num(LUA_MININTEGER))  /* -minint == maxint + 1 */
            return 1;  /* f >= maxint + 1 > i */
        else if (f > cast_num(LUA_MININTEGER))  /* minint < f <= maxint ? */
            return (i < lua_cast(lua_Integer, f));  /* compare them as integers */
        else  /* f <= minint <= i (or 'f' is NaN)  -->  not(i < f) */
            return 0;
    }
#endif
    return luai_numlt(cast_num(i), f);  /* compare them as floats */
}


/*
** Check whether integer 'i' is less than or equal to float 'f'.
** See comments on previous function.
*/
static int LEintfloat(lua_Integer i, lua_Number f) {
#if defined(l_intfitsf)
    if (!l_intfitsf(i)) {
        if (f >= -cast_num(LUA_MININTEGER))  /* -minint == maxint + 1 */
            return 1;  /* f >= maxint + 1 > i */
        else if (f >= cast_num(LUA_MININTEGER))  /* minint <= f <= maxint ? */
            return (i <= lua_cast(lua_Integer, f));  /* compare them as integers */
        else  /* f < minint <= i (or 'f' is NaN)  -->  not(i <= f) */
            return 0;
    }
#endif
    return luai_numle(cast_num(i), f);  /* compare them as floats */
}


/*
** Return 'l < r', for numbers.
*/
static int LTnum(const TValue *l, const TValue *r) {
    if (ttisinteger(l)) {
        lua_Integer li = ivalue(l);
        if (ttisinteger(r))
            return li < ivalue(r);  /* both are integers */
        else  /* 'l' is int and 'r' is float */
            return LTintfloat(li, fltvalue(r));  /* l < r ? */
    }
    else {
        lua_Number lf = fltvalue(l);  /* 'l' must be float */
        if (ttisfloat(r))
            return luai_numlt(lf, fltvalue(r));  /* both are float */
        else if (luai_numisnan(lf))  /* 'r' is int and 'l' is float */
            return 0;  /* NaN < i is always false */
        else  /* without NaN, (l < r)  <-->  not(r <= l) */
            return !LEintfloat(ivalue(r), lf);  /* not (r <= l) ? */
    }
}


/*
** Return 'l <= r', for numbers.
*/
static int LEnum(const TValue *l, const TValue *r) {
    if (ttisinteger(l)) {
        lua_Integer li = ivalue(l);
        if (ttisinteger(r))
            return li <= ivalue(r);  /* both are integers */
        else  /* 'l' is int and 'r' is float */
            return LEintfloat(li, fltvalue(r));  /* l <= r ? */
    }
    else {
        lua_Number lf = fltvalue(l);  /* 'l' must be float */
        if (ttisfloat(r))
            return luai_numle(lf, fltvalue(r));  /* both are float */
        else if (luai_numisnan(lf))  /* 'r' is int and 'l' is float */
            return 0;  /*  NaN <= i is always false */
        else  /* without NaN, (l <= r)  <-->  not(r < l) */
            return !LTintfloat(ivalue(r), lf);  /* not (r < l) ? */
    }
}


/*
** Main operation less than; return 'l < r'.
*/
int luaV_lessthan(lua_State *L, const TValue *l, const TValue *r) {
    int res;
    if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
        return LTnum(l, r);
    else if (ttisstring(l) && ttisstring(r))  /* both are strings? */
        return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
	else if (ttisstring(l) && ttisnumber(r))
	{
		auto lstr = svalue(l);
		auto rstr = std::to_string(ttisinteger(r) ? ivalue(r) : fltvalue(r));
		return l_c_str_cmp(lstr, rstr.c_str());
	}
	else if(ttisnumber(l) && ttisstring(r))
	{
		auto lstr = std::to_string(ttisinteger(l) ? ivalue(l) : fltvalue(l));
		auto rstr = svalue(r);
		return l_c_str_cmp(lstr.c_str(), rstr);
	}
    else if ((res = luaT_callorderTM(L, l, r, TM_LT)) < 0)  /* no metamethod? */
        luaG_ordererror(L, l, r);  /* error */
    return res;
}


/*
** Main operation less than or equal to; return 'l <= r'. If it needs
** a metamethod and there is no '__le', try '__lt', based on
** l <= r iff !(r < l) (assuming a total order). If the metamethod
** yields during this substitution, the continuation has to know
** about it (to negate the result of r<l); bit CIST_LEQ in the call
** status keeps that information.
*/
int luaV_lessequal(lua_State *L, const TValue *l, const TValue *r) {
    int res;
    if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
        return LEnum(l, r);
    else if (ttisstring(l) && ttisstring(r))  /* both are strings? */
        return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
	else if(ttisstring(l) && ttisnumber(r))
	{
		auto lstr = svalue(l);
		auto rstr = std::to_string(ttisinteger(r) ? ivalue(r) : fltvalue(r));
		return l_c_str_cmp(lstr, rstr.c_str()) <= 0;
	}
	else if(ttisnumber(l) && ttisstring(r))
	{
		auto lstr = std::to_string(ttisinteger(l) ? ivalue(l) : fltvalue(l));
		auto rstr = svalue(r);
		return l_c_str_cmp(lstr.c_str(), rstr) <= 0;
	}
    else if ((res = luaT_callorderTM(L, l, r, TM_LE)) >= 0)  /* try 'le' */
        return res;
    else {  /* try 'lt': */
        L->ci->callstatus |= CIST_LEQ;  /* mark it is doing 'lt' for 'le' */
        res = luaT_callorderTM(L, r, l, TM_LT);
        L->ci->callstatus ^= CIST_LEQ;  /* clear mark */
        if (res < 0)
            luaG_ordererror(L, l, r);
        return !res;  /* result is negated */
    }
}


/*
** Main operation for equality of Lua values; return 't1 == t2'.
** L == nullptr means raw equality (no metamethods)
*/
int luaV_equalobj(lua_State *L, const TValue *t1, const TValue *t2) {
    const TValue *tm;
    if (ttype(t1) != ttype(t2)) {  /* not the same variant? */
        if (ttnov(t1) != ttnov(t2) || ttnov(t1) != LUA_TNUMBER)
            return 0;  /* only numbers can be equal with different variants */
        else {  /* two numbers with different variants */
            lua_Integer i1, i2;  /* compare them as integers */
            return (tointeger(t1, &i1) && tointeger(t2, &i2) && i1 == i2);
        }
    }
    /* values have same type and same variant */
    switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMINT: return (ivalue(t1) == ivalue(t2));
    case LUA_TNUMFLT: return luai_numeq(fltvalue(t1), fltvalue(t2));
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TLCF: return fvalue(t1) == fvalue(t2);
    case LUA_TSHRSTR: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case LUA_TLNGSTR: return luaS_eqlngstr(tsvalue(t1), tsvalue(t2));
    case LUA_TUSERDATA: {
        if (uvalue(t1) == uvalue(t2)) return 1;
        else if (L == nullptr) return 0;
        tm = fasttm(L, uvalue(t1)->metatable, TM_EQ);
        if (tm == nullptr)
            tm = fasttm(L, uvalue(t2)->metatable, TM_EQ);
        break;  /* will try TM */
    }
    case LUA_TTABLE: {
        if (hvalue(t1) == hvalue(t2)) return 1;
        else if (L == nullptr) return 0;
        tm = fasttm(L, hvalue(t1)->metatable, TM_EQ);
        if (tm == nullptr)
            tm = fasttm(L, hvalue(t2)->metatable, TM_EQ);
        break;  /* will try TM */
    }
    default:
        return gcvalue(t1) == gcvalue(t2);
    }
    if (tm == nullptr)  /* no TM? */
        return 0;  /* objects are different */
    luaT_callTM(L, tm, t1, t2, L->top, 1);  /* call TM */
    return !l_isfalse(L->top);
}


/* macro used by 'luaV_concat' to ensure that element at 'o' is a string */
#define tostring(L,o)  \
	(ttisstring(o) || (cvt2str(o) && (luaO_tostring(L, o), 1)))

#define isemptystr(o)	(ttisshrstring(o) && tsvalue(o)->shrlen == 0)

/* copy strings in stack from top - n up to top - 1 to buffer */
static void copy2buff(StkId top, int n, char *buff) {
    size_t tl = 0;  /* size already copied */
    do {
        size_t l = vslen(top - n);  /* length of string being copied */
        memcpy(buff + tl, svalue(top - n), l * sizeof(char));
        tl += l;
    } while (--n > 0);
}


/*
** Main operation for concatenation: concat 'total' values in the stack,
** from 'L->top - total' up to 'L->top - 1'.
*/
void luaV_concat(lua_State *L, int total) {
    lua_assert(total >= 2);
    do {
        StkId top = L->top;
        int n = 2;  /* number of elements handled in this pass (at least 2) */
        if (!(ttisstring(top - 2) || cvt2str(top - 2)) || !tostring(L, top - 1))
            luaT_trybinTM(L, top - 2, top - 1, top - 2, TM_CONCAT);
        else if (isemptystr(top - 1))  /* second operand is empty? */
            cast_void(tostring(L, top - 2));  /* result is first operand */
        else if (isemptystr(top - 2)) {  /* first operand is an empty string? */
            setobjs2s(L, top - 2, top - 1);  /* result is second op. */
        }
        else {
            /* at least two non-empty string values; get as many as possible */
            size_t tl = vslen(top - 1);
            TString *ts;
            /* collect total length and number of strings */
            for (n = 1; n < total && tostring(L, top - n - 1); n++) {
                size_t l = vslen(top - n - 1);
                if (l >= (MAX_SIZE / sizeof(char)) - tl)
                    luaG_runerror(L, "string length overflow");
                tl += l;
            }
            if (tl <= LUAI_MAXSHORTLEN) {  /* is result a short string? */
                char buff[LUAI_MAXSHORTLEN];
                copy2buff(top, n, buff);  /* copy strings to buffer */
                ts = luaS_newlstr(L, buff, tl);
            }
            else {  /* long string; copy strings directly to final result */
                ts = luaS_createlngstrobj(L, tl);
                copy2buff(top, n, getstr(ts));
            }
            setsvalue2s(L, top - n, ts);  /* create result */
        }
        total -= n - 1;  /* got 'n' strings to create 1 new */
        L->top -= n - 1;  /* popped 'n' strings and pushed one */
    } while (total > 1);  /* repeat until only 1 result left */
}


/*
** Main operation 'ra' = #rb'.
*/
void luaV_objlen(lua_State *L, StkId ra, const TValue *rb) {
    const TValue *tm;
    switch (ttype(rb)) {
    case LUA_TTABLE: {
        Table *h = hvalue(rb);
        tm = fasttm(L, h->metatable, TM_LEN);
        if (tm) break;  /* metamethod? break switch to call it */
        setivalue(ra, luaH_getn(h));  /* else primitive len */
        return;
    }
    case LUA_TSHRSTR: {
        setivalue(ra, tsvalue(rb)->shrlen);
        return;
    }
    case LUA_TLNGSTR: {
        setivalue(ra, tsvalue(rb)->u.lnglen);
        return;
    }
    default: {  /* try metamethod */
        tm = luaT_gettmbyobj(L, rb, TM_LEN);
        if (ttisnil(tm))  /* no metamethod? */
            luaG_typeerror(L, rb, "get length of");
        break;
    }
    }
    luaT_callTM(L, tm, rb, rb, ra, 1);
}


/*
** Integer division; return 'm // n', that is, floor(m/n).
** C division truncates its result (rounds towards zero).
** 'floor(q) == trunc(q)' when 'q >= 0' or when 'q' is integer,
** otherwise 'floor(q) == trunc(q) - 1'.
*/
lua_Integer luaV_div(lua_State *L, lua_Integer m, lua_Integer n) {
    if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
        if (n == 0)
            luaG_runerror(L, "attempt to divide by zero");
        return intop(-, 0, m);   /* n==-1; avoid overflow with 0x80000...//-1 */
    }
    else {
        lua_Integer q = m / n;  /* perform C division */
        if ((m ^ n) < 0 && m % n != 0)  /* 'm/n' would be negative non-integer? */
            q -= 1;  /* correct result for different rounding */
        return q;
    }
}


/*
** Integer modulus; return 'm % n'. (Assume that C '%' with
** negative operands follows C99 behavior. See previous comment
** about luaV_div.)
*/
lua_Integer luaV_mod(lua_State *L, lua_Integer m, lua_Integer n) {
    if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
        if (n == 0)
            luaG_runerror(L, "attempt to perform 'n%%0'");
        return 0;   /* m % -1 == 0; avoid overflow with 0x80000...%-1 */
    }
    else {
        lua_Integer r = m % n;
        if (r != 0 && (m ^ n) < 0)  /* 'm/n' would be non-integer negative? */
            r += n;  /* correct result for different rounding */
        return r;
    }
}


/* number of bits in an integer */
#define NBITS	cast_int(sizeof(lua_Integer) * CHAR_BIT)

/*
** Shift left operation. (Shift right just negates 'y'.)
*/
lua_Integer luaV_shiftl(lua_Integer x, lua_Integer y) {
    if (y < 0) {  /* shift right? */
        if (y <= -NBITS) return 0;
        else return intop(>> , x, -y);
    }
    else {  /* shift left */
        if (y >= NBITS) return 0;
        else return intop(<< , x, y);
    }
}


/*
** check whether cached closure in prototype 'p' may be reused, that is,
** whether there is a cached closure with the same upvalues needed by
** new closure to be created.
*/
static LClosure *getcached(Proto *p, UpVal **encup, StkId base) {
    LClosure *c = p->cache;
    if (c != nullptr) {  /* is there a cached closure? */
        int nup = p->sizeupvalues;
        Upvaldesc *uv = p->upvalues;
        int i;
        for (i = 0; i < nup; i++) {  /* check whether it has right upvalues */
            TValue *v = uv[i].instack ? base + uv[i].idx : encup[uv[i].idx]->v;
            if (c->upvals[i]->v != v)
                return nullptr;  /* wrong upvalue; cannot reuse closure */
        }
    }
    return c;  /* return cached closure (or nullptr if no cached closure) */
}


/*
** create a new Lua closure, push it in the stack, and initialize
** its upvalues. Note that the closure is not cached if prototype is
** already black (which means that 'cache' was already cleared by the
** GC).
*/
static void pushclosure(lua_State *L, Proto *p, UpVal **encup, StkId base,
    StkId ra) {
    int nup = p->sizeupvalues;
    Upvaldesc *uv = p->upvalues;
    int i;
    LClosure *ncl = luaF_newLclosure(L, nup);
    ncl->p = p;
    setclLvalue(L, ra, ncl);  /* anchor new closure in stack */
    for (i = 0; i < nup; i++) {  /* fill in its upvalues */
        if (uv[i].instack)  /* upvalue refers to local variable? */
            ncl->upvals[i] = luaF_findupval(L, base + uv[i].idx);
        else  /* get upvalue from enclosing function */
            ncl->upvals[i] = encup[uv[i].idx];
        if (nullptr != ncl->upvals[i])
            ncl->upvals[i]->refcount++;
        /* new closure is white, so we do not need a barrier here */
    }
    if (!isblack(p))  /* cache will not break GC invariant? */
        p->cache = ncl;  /* save it on cache for reuse */
}

/*
** finish execution of an opcode interrupted by an yield
*/
void luaV_finishOp(lua_State *L) {
    CallInfo *ci = L->ci;
    StkId base = ci->u.l.base;
    Instruction inst = *(ci->u.l.savedpc - 1);  /* interrupted instruction */
    OpCode op = GET_OPCODE(inst);
    switch (op) {  /* finish its execution */
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_IDIV:
    case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR:
    case OP_MOD: case OP_POW:
    case OP_UNM: case OP_BNOT: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_SELF: {
        setobjs2s(L, base + GETARG_A(inst), --L->top);
        break;
    }
    case OP_LE: case OP_LT: case OP_EQ: {
        int res = !l_isfalse(L->top - 1);
        L->top--;
        if (ci->callstatus & CIST_LEQ) {  /* "<=" using "<" instead? */
            lua_assert(op == OP_LE);
            ci->callstatus ^= CIST_LEQ;  /* clear mark */
            res = !res;  /* negate result */
        }
        lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
        if (res != GETARG_A(inst))  /* condition failed? */
            ci->u.l.savedpc++;  /* skip jump instruction */
        break;
    }
    case OP_CONCAT: {
        StkId top = L->top - 1;  /* top when 'luaT_trybinTM' was called */
        int b = GETARG_B(inst);      /* first element to concatenate */
        int total = cast_int(top - 1 - (base + b));  /* yet to concatenate */
        setobj2s(L, top - 2, top);  /* put TM result in proper position */
        if (total > 1) {  /* are there elements to concat? */
            L->top = top - 1;  /* top is one after last element (at top-2) */
            luaV_concat(L, total);  /* concat them (may yield again) */
        }
        /* move final result to final position */
        setobj2s(L, ci->u.l.base + GETARG_A(inst), L->top - 1);
        L->top = ci->top;  /* restore top */
        break;
    }
    case OP_TFORCALL: {
        lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_TFORLOOP);
        L->top = ci->top;  /* correct top */
        break;
    }
    case OP_CALL: {
        if (GETARG_C(inst) - 1 >= 0)  /* nresults >= 0? */
            L->top = ci->top;  /* adjust results */
        break;
    }
    case OP_TAILCALL: case OP_SETTABUP: case OP_SETTABLE:
        break;
    default: lua_assert(0);
    }
}




/*
** {==================================================================
** Function 'luaV_execute': main interpreter loop
** ===================================================================
*/


/*
** some macros for common tasks in 'luaV_execute'
*/


#define RA(i)	(base+GETARG_A(i))
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))


/* execute a jump instruction */
#define dojump(ci,i,e) \
  { int a = GETARG_A(i); \
    if (a != 0) luaF_close(L, ci->u.l.base + a - 1); \
    ci->u.l.savedpc += GETARG_sBx(i) + e; }

/* for test instructions, execute the jump instruction that follows it */
#define donextjump(ci)	{ i = *ci->u.l.savedpc; dojump(ci, i, 1); }


#define Protect(x)	{ {x;}; base = ci->u.l.base; }

#define checkGC(L,c)  \
	{ luaC_condGC(L, L->top = (c),  /* limit of live values */ \
                         Protect(L->top = ci->top));  /* restore top */ \
           luai_threadyield(L); }


#define vmdispatch(o)	switch(o)
#define vmcase(l)	case l:
#define vmbreak		break


/*
** copy of 'luaV_gettable', but protecting call to potential metamethod
** (which can reallocate the stack)
*/
#define gettableProtected(L,t,k,v)  { const TValue *aux; \
  if (luaV_fastget(L,t,k,aux,luaH_get)) { setobj2s(L, v, aux); } \
    else Protect(luaV_finishget(L,t,k,v,aux)); }


/* same for 'luaV_settable' */
#define settableProtected(L,t,k,v) { const TValue *slot; \
  if (!luaV_fastset(L,t,k,slot,luaH_get,v)) \
    Protect(luaV_finishset(L,t,k,v,slot)); }


// FIXME: duplicate code in thinkyoung_lua_lib.cpp
typedef std::unordered_map<std::string, std::pair<std::string, bool>> LuaDebuggerInfoList;

static LuaDebuggerInfoList g_debug_infos;
static bool g_debug_running = false;

static glua::debugger::LRemoteDebugger *remote_debugger; // TODO: 什么时候关闭

static LuaDebuggerInfoList *get_lua_debugger_info_list_in_state(lua_State *L)
{
	return &g_debug_infos;
}
static int enter_lua_debugger(lua_State *L)
{
	LuaDebuggerInfoList *debugger_info = get_lua_debugger_info_list_in_state(L);
	if (!debugger_info)
		return 0;
	if (lua_gettop(L) > 0 && lua_isstring(L, 1))
	{
		// if has string parameter, use this function to get the value of the name in running debugger;
		auto found = debugger_info->find(luaL_checkstring(L, 1));
		if (found == debugger_info->end())
			return 0;
		return 0;
	}

	debugger_info->clear();
	g_debug_running = true;

	// get all info snapshot in current lua_State stack
	// and then pause the thread(but other thread can use this lua_State
	// maybe you want to start a REPL before real enter debugger

	// first need back to caller func frame

	CallInfo *ci = L->ci;
	auto old_l_top = L->top;
	CallInfo *oci = ci->previous;
	Proto *np = getproto(ci->func);
	Proto *p = getproto(oci->func);
	LClosure *ocl = clLvalue(oci->func);
	int real_localvars_count = (int)(ci->func - oci->func - 1);
	int linedefined = p->linedefined;
	// fprintf(L->out, "debugging into line %d\n", linedefined); // FIXME: logging the debugging code line
	L->ci = oci; // FIXME: 考虑不直接复制ci，改成类似函数入栈出栈的方式
	int top = lua_gettop(L);

	// capture locals vars and values(need get localvar name from whereelse)
	for (int i = 0; i < top; ++i)
	{
		luaL_tojsonstring(L, i + 1, nullptr);
		const char *value_str = luaL_checkstring(L, -1);
		lua_pop(L, 1);
		// if the debugger position is before the localvar position, ignore the next localvars
		if (i >= real_localvars_count) // the localvars is after the debugger() call
			break;
		// get local var name
		if (i < p->sizelocvars)
		{
			LocVar localvar = p->locvars[i];
			const char *varname = getstr(localvar.varname);

			if (std::string(value_str) != "nil") // 考虑到还没用用到的var也会表现为nil，这里不输出nil值的变量
			{
				fprintf(L->out, "[debugger]%s=%s\n", varname, value_str);
			}
			debugger_info->insert(std::make_pair(varname, std::make_pair(value_str, false)));
			if(remote_debugger)
			{
				remote_debugger->set_last_lvm_debugger_status(varname, value_str, false);
			}
		}
	}
	// capture upvalues vars and values(need get upvalue name from whereelse)
	L->ci = ci;
	for (int i = 0; i < p->sizeupvalues; ++i)
	{
		Upvaldesc upval = p->upvalues[i];
		const char *upval_name = getstr(upval.name);
		if (std::string(upval_name) == "_ENV")
			continue;
		UpVal *upv = ocl->upvals[upval.idx];
		lua_pushinteger(L, 1);
		setobj2s(L, ci->func + 1, upv->v);
		auto value_string1 = luaL_tojsonstring(L, -1, nullptr);
		lua_pop(L, 2);
		if (std::string(value_string1) != "nil")
		{
			fprintf(L->out, "[debugger-upvalue]%s=%s\n", upval_name, value_string1);
		}
		debugger_info->insert(std::make_pair(upval_name, std::make_pair(value_string1, true)));
		if (remote_debugger)
		{
			remote_debugger->set_last_lvm_debugger_status(upval_name, value_string1, true);
		}
	}
	L->ci = ci;
	L->top = old_l_top;
	if (L->debugger_pausing)
	{
		do
		{
			std::this_thread::yield();
		} while (g_debug_running); // while not exit lua debugger
	}
	return 0;
}

static int exit_lua_debugger(lua_State *L)
{
	g_debug_infos.clear();
	g_debug_running = false;
	return 0;
}

static bool debugger_watcher_started = false;

// FIXME: end duplicate code in thinkyoung_lua_lib.cpp

static int get_line_in_current_proto(CallInfo* ci, Proto *proto)
{
	int idx = (int)(ci->u.l.savedpc - proto->code); // 在proto中执行到的指令的偏移量
	if (idx < proto->sizecode)
	{
		int line_in_proto = proto->lineinfo[idx];
		return line_in_proto;
	}
	else
		return 0;
}

#define lua_check_in_vm_error(cond, error_msg) {    \
if (!(cond)) {      \
  L->force_stopping = true; \
  global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, (error_msg));       \
  vmbreak;                                   \
     }                             \
}
void luaV_execute(lua_State *L)
{
    if (L->force_stopping)
        return;
    CallInfo *ci = L->ci;
    LClosure *cl;
    TValue *k;
    StkId base;
    ci->callstatus |= CIST_FRESH;  /* fresh invocation of 'luaV_execute" */
newframe:  /* reentry point when frame changes (call/return) */
    lua_assert(ci == L->ci);
    cl = clLvalue(ci->func);  /* local reference to function's closure */
    k = cl->p->k;  /* local reference to function's constant table */
    base = ci->u.l.base;  /* local copy of function's base */

    int insts_limit = thinkyoung::lua::lib::get_lua_state_value(L, INSTRUCTIONS_LIMIT_LUA_STATE_MAP_KEY).int_value;
    int *stopped_pointer = thinkyoung::lua::lib::get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
    if (nullptr == stopped_pointer)
    {
        thinkyoung::lua::lib::notify_lua_state_stop(L);
        thinkyoung::lua::lib::resume_lua_state_running(L);
        stopped_pointer = thinkyoung::lua::lib::get_lua_state_value(L, LUA_STATE_STOP_TO_RUN_IN_LVM_STATE_MAP_KEY).int_pointer_value;
    }
    int has_insts_limit = insts_limit > 0 ? 1 : 0;
    int *insts_executed_count = thinkyoung::lua::lib::get_lua_state_value(L, INSTRUCTIONS_EXECUTED_COUNT_LUA_STATE_MAP_KEY).int_pointer_value;
    if (nullptr == insts_executed_count)
    {
        insts_executed_count = static_cast<int*>(lua_malloc(L, sizeof(int)));
        *insts_executed_count = 0;
        GluaStateValue lua_state_value_of_exected_count;
        lua_state_value_of_exected_count.int_pointer_value = insts_executed_count;
        thinkyoung::lua::lib::set_lua_state_value(L, INSTRUCTIONS_EXECUTED_COUNT_LUA_STATE_MAP_KEY, lua_state_value_of_exected_count, LUA_STATE_VALUE_INT_POINTER);
    }
    if (*insts_executed_count < 0)
        *insts_executed_count = 0;

    lua_getglobal(L, "last_return");
	bool use_last_return = true; // lua_istable(L, -1);
    lua_pop(L, 1);

	int last_debug_line_in_file = -1;

    /* main loop of interpreter */
    for (;;) {
		while (L->bytecode_debugger_opened && remote_debugger && remote_debugger->is_pausing_lvm())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		// TODO: 添加断点调试, 外部socket设置文件名，断点位置（字节码指令offset)列表，继续/暂停等
		if(L->bytecode_debugger_opened)
		{
			// TODO
			if(!remote_debugger)
			{
				remote_debugger = new glua::debugger::LRemoteDebugger();
			}
			L->debugger_pausing = false;
			
			int line_pre_defined = 7;
			//std::unordered_map<std::string, std::vector<int>> debugger_source_lines = {
				// {"@tests_typed/full_correct_typed.lua", { 15 + line_pre_defined, 15, 16, 17 }}
			//	{"@tests_typed/full_correct_typed.lua",{ 15 }}
			// };
			if(!remote_debugger->is_running())
			{
				remote_debugger->start_async();
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}

			auto debugger_source_lines = remote_debugger->debugger_source_lines();
			Proto *proto = cl->p;
			std::string proto_source((proto->source == nullptr) ? "(*no name)" : getstr(proto->source));
			std::string proto_source_ldf_filename = proto_source + ".ldf";
			thinkyoung::lua::core::LuaDebugFileInfo *ldf = nullptr;
			if (proto_source_ldf_filename[0] == '@')
			{
				proto_source_ldf_filename = proto_source_ldf_filename.substr(1);
				FILE *ldf_file = fopen(proto_source_ldf_filename.c_str(), "r");
				if (ldf_file)
				{
					auto ldf_deserialized = thinkyoung::lua::core::LuaDebugFileInfo::deserialize_from_file(ldf_file);
					ldf = new thinkyoung::lua::core::LuaDebugFileInfo();
					*ldf = ldf_deserialized;
					fclose(ldf_file);
				}
			}
			// TODO: 考虑碰到debugger时把上下文环境socket传回去
			auto source_found_in_debugger = debugger_source_lines.find(proto_source);
			if (source_found_in_debugger != debugger_source_lines.end())
			{
				auto debugger_lines = source_found_in_debugger->second;
				// Instruction i2 = *(ci->u.l.savedpc);
				int idx = (int)(ci->u.l.savedpc - proto->code); // 在proto中执行到的指令的偏移量
				if(idx<proto->sizecode)
				{
					int line_in_proto = proto->lineinfo[idx];
					for(const auto &need_debug_line : debugger_lines)
					{
						auto lua_need_debug_line = need_debug_line;
						if (ldf)
							lua_need_debug_line = (int) ldf->find_lua_line_by_glua_line(need_debug_line);
						if(lua_need_debug_line == proto->linedefined + line_in_proto)
						{
							int line_in_lua_file = proto->linedefined + line_in_proto -line_pre_defined; // FIXME
							
							if (line_in_lua_file != last_debug_line_in_file)
							{
								last_debug_line_in_file = line_in_lua_file;
								// TODO: paused to debug
								// TODO: 反汇编这条指令让其可读，获取上下文的局部变量的值，让remote调式方知道断点的代码位置

								// enter_lua_debugger(L);
								lua_pushcfunction(L, enter_lua_debugger);
								lua_pcall(L, 0, 0, 0);
								// exit_lua_debugger(L);

								// TODO: wait remote debugger tool to resume running
								// TODO: remote debugger tool can send back variable or functioncall to get result back
								lua_pushcfunction(L, exit_lua_debugger);
								lua_pcall(L, 0, 0, 0);

								remote_debugger->set_pausing_lvm(true);
								// remote_debugger->shutdown(); // FIXME: 暂时一次性就关闭，以后可能要改成根据远程调试器来关闭
							}
						}
					}
				}
			}
			if (ldf)
				delete ldf;
		}
        if (!ci || ci->u.l.savedpc == nullptr) {
          global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_LIMIT_OVER_ERROR, "wrong bytecode instruction, can't find savedpc");
          vmbreak;
        }
        Instruction i = *(ci->u.l.savedpc++);
        // printf("%d\n", i);
        StkId ra;

		*insts_executed_count += 1; // executed instructions count

        // limit instructions count, and executed instructions
        if (has_insts_limit && *insts_executed_count > insts_limit)
        {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_LIMIT_OVER_ERROR, "over instructions limit");
            vmbreak;
        }
        if (stopped_pointer && *stopped_pointer > 0)
            vmbreak;
        if (L->force_stopping)
            vmbreak;
        

        // when over contract api limit, also vmbreak
        if ((GET_OPCODE(i) == OP_CALL || GET_OPCODE(i) == OP_TAILCALL)
            && global_glua_chain_api->check_contract_api_instructions_over_limit(L))
        {
            global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_LIMIT_OVER_ERROR, "over instructions limit");
            vmbreak;
        }

        if (L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT))
            Protect(luaG_traceexec(L));
        /* WARNING: several calls may realloc the stack and invalidate 'ra' */
        ra = RA(i);
        lua_assert(base == ci->u.l.base);
        lua_assert(base <= L->top && L->top < L->stack + L->stacksize);
		// TODO: 运行时出错时把行号，函数名等记录下来
		
        vmdispatch(GET_OPCODE(i)) {
            vmcase(OP_MOVE) {
                setobjs2s(L, ra, RB(i));
                vmbreak;
            }
            vmcase(OP_LOADK) {
                TValue *rb = k + GETARG_Bx(i);
                setobj2s(L, ra, rb);
                vmbreak;
            }
            vmcase(OP_LOADKX) {
                TValue *rb;
                lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_EXTRAARG);
                rb = k + GETARG_Ax(*ci->u.l.savedpc++);
                setobj2s(L, ra, rb);
                vmbreak;
            }
            vmcase(OP_LOADBOOL) {
                setbvalue(ra, GETARG_B(i));
                if (GETARG_C(i)) ci->u.l.savedpc++;  /* skip next instruction (if C) */
                vmbreak;
            }
            vmcase(OP_LOADNIL) {
                int b = GETARG_B(i);
                lua_check_in_vm_error(b >= 0, "loadnil instruction arg must be positive integer");
                do {
                    setnilvalue(ra++);
                } while (b--);
                vmbreak;
            }
            vmcase(OP_GETUPVAL) {
                int b = GETARG_B(i);
                lua_check_in_vm_error(b < cl->nupvalues && b>=0, "upvalue error");
                setobj2s(L, ra, cl->upvals[b]->v);
                vmbreak;
            }
            vmcase(OP_GETTABUP) {
                auto upval_index = GETARG_B(i);
                lua_check_in_vm_error(upval_index < cl->nupvalues && upval_index >=0, "upvalue error");
                if (nullptr == cl->upvals[upval_index])
                {
                    *stopped_pointer = 1;
                    vmbreak;
                }
                TValue *upval = cl->upvals[upval_index]->v;
                TValue *rc = RKC(i);
                gettableProtected(L, upval, rc, ra);
                vmbreak;
            }
            vmcase(OP_GETTABLE) {
                StkId rb = RB(i);
                TValue *rc = RKC(i);
                bool istable = ttistable(rb);
                if (!istable)
                {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "getfield of nil, need table here");
                    L->force_stopping = true;
                    vmbreak;
                }
                gettableProtected(L, rb, rc, ra);
                vmbreak;
            }
            vmcase(OP_SETTABUP) {
                auto upval_index = GETARG_A(i);
                lua_check_in_vm_error(upval_index < cl->nupvalues && upval_index >=0, "upvalue error");
				if(!cl->upvals[upval_index])
				{
					global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "set upvalue of nil, need table here");
					L->force_stopping = true;
					vmbreak;
				}
                TValue *upval = cl->upvals[upval_index]->v;
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                settableProtected(L, upval, rb, rc);
                vmbreak;
            }
            vmcase(OP_SETUPVAL) {
                auto upval_index = GETARG_B(i);
                lua_check_in_vm_error(upval_index < cl->nupvalues && upval_index>=0, "upvalue error");
                UpVal *uv = cl->upvals[upval_index];
                setobj(L, uv->v, ra);
                luaC_upvalbarrier(L, uv);
                vmbreak;
            }
            vmcase(OP_SETTABLE) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                settableProtected(L, ra, rb, rc);
                vmbreak;
            }
            vmcase(OP_NEWTABLE) {
                int b = GETARG_B(i);
                int c = GETARG_C(i);
                Table *t = luaH_new(L);
                sethvalue(L, ra, t);
                if (b != 0 || c != 0)
                    luaH_resize(L, t, luaO_fb2int(b), luaO_fb2int(c));
                checkGC(L, ra + 1);
                vmbreak;
            }
            vmcase(OP_SELF) {
                const TValue *aux;
                StkId rb = RB(i);
                TValue *rc = RKC(i);
                TString *key = tsvalue(rc);  /* key must be a string */
                setobjs2s(L, ra + 1, rb);
                if (luaV_fastget(L, rb, key, aux, luaH_getstr)) {
                    setobj2s(L, ra, aux);
                }
                else
                {
                    Protect(luaV_finishget(L, rb, rc, ra, aux));
                }
                vmbreak;
            }
            vmcase(OP_ADD) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (ttisinteger(rb) && ttisinteger(rc)) {
                    lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                    setivalue(ra, intop(+, ib, ic));
                }
                else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    setfltvalue(ra, luai_numadd(L, nb, nc));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "+ can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_ADD));
                }
                vmbreak;
            }
            vmcase(OP_SUB) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (ttisinteger(rb) && ttisinteger(rc)) {
                    lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                    setivalue(ra, intop(-, ib, ic));
                }
                else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    setfltvalue(ra, luai_numsub(L, nb, nc));
                }
                else {
                  global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "- can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_SUB));
                }
                vmbreak;
            }
            vmcase(OP_MUL) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (ttisinteger(rb) && ttisinteger(rc)) {
                    lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                    setivalue(ra, intop(*, ib, ic));
                }
                else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    setfltvalue(ra, luai_nummul(L, nb, nc));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "* can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_MUL));
                }
                vmbreak;
            }
            vmcase(OP_DIV) {  /* float division (always with floats) */
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    setfltvalue(ra, luai_numdiv(L, nb, nc));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "/ can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_DIV));
                }
                vmbreak;
            }
            vmcase(OP_BAND) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Integer ib; lua_Integer ic;
                if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                    setivalue(ra, intop(&, ib, ic));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "& can only accept integer");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_BAND));
                }
                vmbreak;
            }
            vmcase(OP_BOR) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Integer ib; lua_Integer ic;
                if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                    setivalue(ra, intop(| , ib, ic));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "| can only accept integer");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_BOR));
                }
                vmbreak;
            }
            vmcase(OP_BXOR) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Integer ib; lua_Integer ic;
                if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                    setivalue(ra, intop(^, ib, ic));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "~ can only accept integer");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_BXOR));
                }
                vmbreak;
            }
            vmcase(OP_SHL) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Integer ib; lua_Integer ic;
                if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                    setivalue(ra, luaV_shiftl(ib, ic));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "<< can only accept integer");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_SHL));
                }
                vmbreak;
            }
            vmcase(OP_SHR) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Integer ib; lua_Integer ic;
                if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                    setivalue(ra, luaV_shiftl(ib, -ic));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, ">> can only accept integer");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_SHR));
                }
                vmbreak;
            }
            vmcase(OP_MOD) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (ttisinteger(rb) && ttisinteger(rc)) {
                    lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                    setivalue(ra, luaV_mod(L, ib, ic));
                }
                else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    lua_Number m;
                    luai_nummod(L, nb, nc, m);
                    setfltvalue(ra, m);
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "% can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_MOD));
                }
                vmbreak;
            }
            vmcase(OP_IDIV) {  /* floor division */
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (ttisinteger(rb) && ttisinteger(rc)) {
                    lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                    setivalue(ra, luaV_div(L, ib, ic));
                }
                else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    setfltvalue(ra, luai_numidiv(L, nb, nc));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "// can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_IDIV));
                }
                vmbreak;
            }
            vmcase(OP_POW) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                lua_Number nb; lua_Number nc;
                if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                    setfltvalue(ra, luai_numpow(L, nb, nc));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "^ can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rc, ra, TM_POW));
                }
                vmbreak;
            }
            vmcase(OP_UNM) {
                TValue *rb = RB(i);
                lua_Number nb;
                if (ttisinteger(rb)) {
                    lua_Integer ib = ivalue(rb);
                    setivalue(ra, intop(-, 0, ib));
                }
                else if (tonumber(rb, &nb)) {
                    setfltvalue(ra, luai_numunm(L, nb));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "-<exp> can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rb, ra, TM_UNM));
                }
                vmbreak;
            }
            vmcase(OP_BNOT) {
                TValue *rb = RB(i);
                lua_Integer ib;
                if (tointeger(rb, &ib)) {
                    setivalue(ra, intop(^, ~l_castS2U(0), ib));
                }
                else {
                    global_glua_chain_api->throw_exception(L, THINKYOUNG_API_LVM_ERROR, "~<exp> can only accept numbers");
                    Protect(luaT_trybinTM(L, rb, rb, ra, TM_BNOT));
                }
                vmbreak;
            }
            vmcase(OP_NOT) {
                TValue *rb = RB(i);
                int res = l_isfalse(rb);  /* next assignment may change this value */
                setbvalue(ra, res);
                vmbreak;
            }
            vmcase(OP_LEN) {
                Protect(luaV_objlen(L, ra, RB(i)));
                vmbreak;
            }
            vmcase(OP_CONCAT) {
                int b = GETARG_B(i);
                int c = GETARG_C(i);
                StkId rb;
                L->top = base + c + 1;  /* mark the end of concat operands */
                // TODO: check args are string
                Protect(luaV_concat(L, c - b + 1));
                ra = RA(i);  /* 'luaV_concat' may invoke TMs and move the stack */
                rb = base + b;
                setobjs2s(L, ra, rb);
                checkGC(L, (ra >= rb ? ra + 1 : rb));
                L->top = ci->top;  /* restore top */
                vmbreak;
            }
            vmcase(OP_JMP) {
                dojump(ci, i, 0); // maybe only `goto` source code line is compiled to OP_JMP opcode line
                // thinkyoung_api_lua_throw_exception(L, THINKYOUNG_API_LVM_ERROR, "thinkyoung lua not support goto symbol");
                vmbreak;
            }
            vmcase(OP_EQ) {
                TValue *rb = RKB(i);
                TValue *rc = RKC(i);
                Protect(
                    if (luaV_equalobj(L, rb, rc) != GETARG_A(i))
                        ci->u.l.savedpc++;
                    else
                        donextjump(ci);
                )
                    vmbreak;
            }
            vmcase(OP_LT) {
                Protect(
                    if (luaV_lessthan(L, RKB(i), RKC(i)) != GETARG_A(i))
                        ci->u.l.savedpc++;
                    else
                        donextjump(ci);
                )
                    vmbreak;
            }
            vmcase(OP_LE) {
                Protect(
                    if (luaV_lessequal(L, RKB(i), RKC(i)) != GETARG_A(i))
                        ci->u.l.savedpc++;
                    else
                        donextjump(ci);
                )
                    vmbreak;
            }
            vmcase(OP_TEST) {
                if (GETARG_C(i) ? l_isfalse(ra) : !l_isfalse(ra))
                    ci->u.l.savedpc++;
                else
                    donextjump(ci);
                vmbreak;
            }
            vmcase(OP_TESTSET) {
                TValue *rb = RB(i);
                if (GETARG_C(i) ? l_isfalse(rb) : !l_isfalse(rb))
                    ci->u.l.savedpc++;
                else {
                    setobjs2s(L, ra, rb);
                    donextjump(ci);
                }
                vmbreak;
            }
            vmcase(OP_CALL) {
                int b = GETARG_B(i);
                int nresults = GETARG_C(i) - 1;
                if (b != 0) L->top = ra + b;  /* else previous instruction set top */
				// int line_in_proto = get_line_in_current_proto(ci, cl->p);
                if (luaD_precall(L, ra, nresults)) {  /* C function? */
                    if (nresults >= 0)
                        L->top = ci->top;  /* adjust results */
                    Protect((void)0);  /* update 'base' */
                }
                else {  /* Lua function */
                    if (L->force_stopping)
                        vmbreak;
                    ci = L->ci;
                    goto newframe;  /* restart luaV_execute over new Lua function */
                }
                vmbreak;
            }
            vmcase(OP_TAILCALL) {
                int b = GETARG_B(i);
                if (b != 0) L->top = ra + b;  /* else previous instruction set top */
                lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
                if (luaD_precall(L, ra, LUA_MULTRET)) {  /* C function? */
                    Protect((void)0);  /* update 'base' */
                }
                else {
                    /* tail call: put called frame (n) in place of caller one (o) */
                    CallInfo *nci = L->ci;  /* called frame */
                    CallInfo *oci = nci->previous;  /* caller frame */
                    StkId nfunc = nci->func;  /* called function */
                    StkId ofunc = oci->func;  /* caller function */
                    /* last stack slot filled by 'precall' */
                    StkId lim = nci->u.l.base + getproto(nfunc)->numparams;
                    int aux;
                    /* close all upvalues from previous call */
                    if (cl->p->sizep > 0) luaF_close(L, oci->u.l.base);
                    /* move new frame into old one */
                    for (aux = 0; nfunc + aux < lim; aux++)
                        setobjs2s(L, ofunc + aux, nfunc + aux);
                    oci->u.l.base = ofunc + (nci->u.l.base - nfunc);  /* correct base */
                    oci->top = L->top = ofunc + (L->top - nfunc);  /* correct top */
                    oci->u.l.savedpc = nci->u.l.savedpc;
                    oci->callstatus |= CIST_TAIL;  /* function was tail called */
                    ci = L->ci = oci;  /* remove new frame */
                    lua_assert(L->top == oci->u.l.base + getproto(ofunc)->maxstacksize);
                    if (L->force_stopping)
                        vmbreak;
                    goto newframe;  /* restart luaV_execute over new Lua function */
                }
                vmbreak;
            }
            vmcase(OP_RETURN) {
                int a = GETARG_A(i);
                int b = GETARG_B(i);
                int top = lua_gettop(L);
                // return R(a), R(a+1), ... , R(a+b-2), b is return result count + 1, index from 0
                if (use_last_return && b > 1 && top >= a + 1)
                {
					/*for(int i=1;i<=top;++i)
					{
						auto param_json_str = luaL_tojsonstring(L, i, nullptr);
						printf("");
					}*/
					// auto ret_json_str = luaL_tojsonstring(L, a + 1, nullptr);
                    lua_getglobal(L, "_G");
                    lua_pushvalue(L, a + 1);
                    lua_setfield(L, -2, "last_return");
                    lua_pop(L, 1);
                }
                if (cl->p->sizep > 0) luaF_close(L, base);
                b = luaD_poscall(L, ci, ra, (b != 0 ? b - 1 : cast_int(L->top - ra)));

                if (ci->callstatus & CIST_FRESH)  /* local 'ci' still from callee */
                    return;  /* external invocation: return */
                else {  /* invocation via reentry: continue execution */
                    ci = L->ci;
                    if (b) L->top = ci->top;
                    lua_assert(isLua(ci));
                    lua_assert(GET_OPCODE(*((ci)->u.l.savedpc - 1)) == OP_CALL);
                    if (L->force_stopping)
                        vmbreak;
                    goto newframe;  /* restart luaV_execute over new Lua function */
                }
            }
            vmcase(OP_FORLOOP) {
                if (ttisinteger(ra)) {  /* integer loop? */
                    lua_Integer step = ivalue(ra + 2);
                    lua_Integer idx = intop(+, ivalue(ra), step); /* increment index */
                    lua_Integer limit = ivalue(ra + 1);
                    if ((0 < step) ? (idx <= limit) : (limit <= idx)) {
                        ci->u.l.savedpc += GETARG_sBx(i);  /* jump back */
                        chgivalue(ra, idx);  /* update internal index... */
                        setivalue(ra + 3, idx);  /* ...and external index */
                    }
                }
                else {  /* floating loop */
                    lua_Number step = fltvalue(ra + 2);
                    lua_Number idx = luai_numadd(L, fltvalue(ra), step); /* inc. index */
                    lua_Number limit = fltvalue(ra + 1);
                    if (luai_numlt(0, step) ? luai_numle(idx, limit)
                        : luai_numle(limit, idx)) {
                        ci->u.l.savedpc += GETARG_sBx(i);  /* jump back */
                        chgfltvalue(ra, idx);  /* update internal index... */
                        setfltvalue(ra + 3, idx);  /* ...and external index */
                    }
                }
                vmbreak;
            }
            vmcase(OP_FORPREP) {
                TValue *init = ra;
                TValue *plimit = ra + 1;
                TValue *pstep = ra + 2;
                lua_Integer ilimit;
                int stopnow;
                if (ttisinteger(init) && ttisinteger(pstep) &&
                    forlimit(plimit, &ilimit, ivalue(pstep), &stopnow)) {
                    /* all values are integer */
                    lua_Integer initv = (stopnow ? 0 : ivalue(init));
                    setivalue(plimit, ilimit);
                    setivalue(init, intop(-, initv, ivalue(pstep)));
                }
                else {  /* try making all values floats */
                    lua_Number ninit; lua_Number nlimit; lua_Number nstep;
                    if (!tonumber(plimit, &nlimit))
                        luaG_runerror(L, "'for' limit must be a number");
                    setfltvalue(plimit, nlimit);
                    if (!tonumber(pstep, &nstep))
                        luaG_runerror(L, "'for' step must be a number");
                    setfltvalue(pstep, nstep);
                    if (!tonumber(init, &ninit))
                        luaG_runerror(L, "'for' initial value must be a number");
                    setfltvalue(init, luai_numsub(L, ninit, nstep));
                }
                ci->u.l.savedpc += GETARG_sBx(i);
                vmbreak;
            }
            vmcase(OP_TFORCALL) {
                StkId cb = ra + 3;  /* call base */
                setobjs2s(L, cb + 2, ra + 2);
                setobjs2s(L, cb + 1, ra + 1);
                setobjs2s(L, cb, ra);
                L->top = cb + 3;  /* func. + 2 args (state and index) */
                Protect(luaD_call(L, cb, GETARG_C(i)));
                L->top = ci->top;
                i = *(ci->u.l.savedpc++);  /* go to next instruction */
                ra = RA(i);
                lua_assert(GET_OPCODE(i) == OP_TFORLOOP);
                goto l_tforloop;
            }
            vmcase(OP_TFORLOOP) {
            l_tforloop:
                if (!ttisnil(ra + 1)) {  /* continue loop? */
                    setobjs2s(L, ra, ra + 1);  /* save control variable */
                    ci->u.l.savedpc += GETARG_sBx(i);  /* jump back */
                }
                vmbreak;
            }
            vmcase(OP_SETLIST) {
                int n = GETARG_B(i);
                int c = GETARG_C(i);
                unsigned int last;
                Table *h;
                if (n == 0) n = cast_int(L->top - ra) - 1;
                if (c == 0) {
                    lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_EXTRAARG);
                    c = GETARG_Ax(*ci->u.l.savedpc++);
                }
                h = hvalue(ra);
                last = ((c - 1)*LFIELDS_PER_FLUSH) + n;
                if (last > h->sizearray)  /* needs more space? */
                    luaH_resizearray(L, h, last);  /* preallocate it at once */
                for (; n > 0; n--) {
                    TValue *val = ra + n;
                    luaH_setint(L, h, last--, val);
                    luaC_barrierback(L, h, val);
                }
                L->top = ci->top;  /* correct top (in case of previous open call) */
                vmbreak;
            }
            vmcase(OP_CLOSURE) {
                auto p_index = GETARG_Bx(i);
                lua_check_in_vm_error(p_index < cl->p->sizep, "too large sub proto index");
                Proto *p = cl->p->p[p_index];
                LClosure *ncl = getcached(p, cl->upvals, base);  /* cached closure */
                if (ncl == nullptr)  /* no match? */
                    pushclosure(L, p, cl->upvals, base, ra);  /* create a new one */
                else
                    setclLvalue(L, ra, ncl);  /* push cashed closure */
                checkGC(L, ra + 1);
                vmbreak;
            }
            vmcase(OP_VARARG) {
                int b = GETARG_B(i) - 1;  /* required results */
                int j;
                int n = cast_int(base - ci->func) - cl->p->numparams - 1;
                if (n < 0)  /* less arguments than parameters? */
                    n = 0;  /* no vararg arguments */
                if (b < 0) {  /* B == 0? */
                    b = n;  /* get all var. arguments */
                    Protect(luaD_checkstack(L, n));
                    ra = RA(i);  /* previous call may change the stack */
                    L->top = ra + n;
                }
                for (j = 0; j < b && j < n; j++)
                    setobjs2s(L, ra + j, base - n + j);
                for (; j < b; j++)  /* complete required results with nil */
                    setnilvalue(ra + j);
                vmbreak;
            }
            vmcase(OP_EXTRAARG) {
                lua_assert(0);
                vmbreak;
            }
        }
    }
}

/* }================================================================== */

