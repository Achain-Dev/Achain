/*
** Windows stuff
*/
#if defined(_WIN32) 	/* { */

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  /* avoid warnings about ISO C functions */
#endif

#endif			/* } */

#include "UnitTest++/Config.h"
#ifndef UNITTEST_NO_DEFERRED_REPORTER

#include "UnitTest++/DeferredTestResult.h"
#include <cstring>

namespace UnitTest
{

   DeferredTestFailure::DeferredTestFailure()
      : lineNumber(-1)
   {
      failureStr[0] = '\0';
   }

   DeferredTestFailure::DeferredTestFailure(int lineNumber_, const char* failureStr_)
      : lineNumber(lineNumber_)
   {
      UNIITEST_NS_QUAL_STD(strcpy)(failureStr, failureStr_);
   }

   DeferredTestResult::DeferredTestResult()
      : suiteName("")
      , testName("")
      , failureFile("")
      , timeElapsed(0.0f)
      , failed(false)
   {}

   DeferredTestResult::DeferredTestResult(char const* suite, char const* test)
      : suiteName(suite)
      , testName(test)
      , failureFile("")
      , timeElapsed(0.0f)
      , failed(false)
   {}

   DeferredTestResult::~DeferredTestResult()
   {}

}

#endif
