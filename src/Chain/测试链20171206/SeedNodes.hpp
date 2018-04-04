#pragma once 
namespace thinkyoung { 
    namespace client { 
#ifndef ALP_TEST_NETWORK 
    static const std::vector<std::string> SeedNodes = {
"13.125.59.140:61696","52.78.47.183:61696"
 }; 
#else 
 static const std::vector<std::string> SeedNodes { }; 
#endif
} 
} // thinkyoung::client 
