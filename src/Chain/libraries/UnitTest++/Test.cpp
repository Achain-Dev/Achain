#include "UnitTest++/Config.h"
#include "UnitTest++/Test.h"
#include "UnitTest++/TestList.h"
#include "UnitTest++/TestResults.h"
#include "UnitTest++/AssertException.h"
#include "UnitTest++/MemoryOutStream.h"
#include "UnitTest++/ExecuteTest.h"

#ifdef UNITTEST_POSIX
#include "UnitTest++/Posix/SignalTranslator.h"
#endif

namespace UnitTest {

   TestList& Test::GetTestList()
   {
      static TestList s_list;
      return s_list;
   }

   Test::Test(char const* testName, char const* suiteName, char const* filename, int lineNumber)
      : m_details(testName, suiteName, filename, lineNumber)
      , m_nextTest(0)
      , m_isMockTest(false)
   {}

   Test::~Test()
   {}

   void Test::Run()
   {
      ExecuteTest(*this, m_details, m_isMockTest);
   }

   void Test::RunImpl() const
   {}

}
