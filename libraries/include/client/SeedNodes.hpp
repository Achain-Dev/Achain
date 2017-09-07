#pragma once

namespace thinkyoung {
    namespace client {

#ifndef ACT_TEST_NETWORK
        static const std::vector<std::string> SeedNodes =
        {
        }; 
#else
        static const std::vector<std::string> SeedNodes
        {
        };
#endif

    }
} // thinkyoung::client
