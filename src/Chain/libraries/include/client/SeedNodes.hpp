#pragma once

namespace thinkyoung {
    namespace client {

#ifndef ALP_TEST_NETWORK
        static const std::vector<std::string> SeedNodes =
        {
            "10.23.0.220:61696"
        }; 
#else
        static const std::vector<std::string> SeedNodes
        {
        };
#endif

    }
} // thinkyoung::client
