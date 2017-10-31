#include "UnitTest++/CurrentTest.h"
#include <cstddef>

namespace UnitTest {

   UNITTEST_LINKAGE TestResults*& CurrentTest::Results()
   {
      static TestResults* testResults = nullptr;
      return testResults;
   }

   UNITTEST_LINKAGE const TestDetails*& CurrentTest::Details()
   {
      static const TestDetails* testDetails = nullptr;
      return testDetails;
   }

}
