#pragma once

namespace thinkyoung {
    namespace client {

#ifndef ALP_TEST_NETWORK
        static const std::vector<std::string> SeedNodes =
        {
            "47.52.62.164:61696",
            "47.90.206.220:61696",
            "47.90.202.190:61696",
            "47.91.21.67:61696",
            "47.91.22.116:61696",
            "47.52.102.41:61696",
            "101.200.32.205:61696",
            "59.110.138.70:61696",
            "47.88.153.65:61696",
            "47.88.228.70:61696",
            "node.achain.com:61696"
        }; 
#else
        static const std::vector<std::string> SeedNodes
        {
        };
#endif

    }
} // thinkyoung::client
