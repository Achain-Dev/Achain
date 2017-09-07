#ifndef UNITTESTPP_H
#define UNITTESTPP_H

/*
** Windows stuff
*/
#if defined(_WIN32) 	/* { */

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  /* avoid warnings about ISO C functions */
#endif

#endif			/* } */

#include "Config.h"
#include "TestMacros.h"
#include "CheckMacros.h"
#include "RequireMacros.h"
#include "TestRunner.h"
#include "TimeConstraint.h"
#include "ReportAssert.h"

#endif
