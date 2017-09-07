/*
** $Id: lprefix.h,v 1.2 2014/12/29 16:54:13 roberto Exp $
** Definitions for Lua code that must come before any other header file
** See Copyright Notice in lua.h
*/

#ifndef lprefix_h
#define lprefix_h


/*
** Allows POSIX/XSI stuff
*/
#if !defined(LUA_USE_C89)	/* { */

#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE           600
#elif _XOPEN_SOURCE == 0
#undef _XOPEN_SOURCE  /* use -D_XOPEN_SOURCE=0 to undefine it */
#endif

/*
** Allows manipulation of large files in gcc and some other compilers
*/
#if !defined(LUA_32BITS) && !defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE       1
#define _FILE_OFFSET_BITS       64
#endif

#endif				/* } */

/**
 * malloc and allocator
 */
// void* l_malloc(size_t size, const char *file, int line, const char *func);
// #define malloc(X) l_malloc( X, __FILE__, __LINE__, __FUNCTION__)


/*
** Windows stuff
*/
#if defined(_WIN32) 	/* { */

#define _CRT_NONSTDC_NO_DEPRECATE

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  /* avoid warnings about ISO C functions */
#endif

#include <io.h>
#include <functional>
#define dup _dup
#define dup2 _dup2
#define strdup _strdup
#define snprintf _snprintf

#elif defined(__APPLE__)

#include <fcntl.h>
#include <unistd.h>
#include <functional>

#elif defined(__linux__) || defined(__linux) || defined(__unix__) || defined(__unix) || defined(__MINGW32__)

#include <fcntl.h>
#include <unistd.h>
#include <tr1/functional>

#endif			/* } */


#endif

