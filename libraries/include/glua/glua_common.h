#ifndef glua_common_h
#define glua_common_h

#include <glua/lprefix.h>

#ifdef _DEBUG

#if defined MEMCHECK_VLD
#include <wchar.h>
#include <vld.h>
#elif defined MEMCHECK_MEMWATCH
#define MEMWATCH
#define MW_STDIO
#include <string.h>
#include "memwatch.h"
#endif

#endif // #ifdef _DEBUG

#define MACRO_STR_RAW(tok) #tok
#define MACRO_STR(tok) MACRO_STR_RAW(tok)

#endif // #ifndef glua_common_h
