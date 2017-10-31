#pragma once
#include <memory>

namespace thinkyoung {
    namespace db {

        class peer;
        typedef std::shared_ptr<peer> peer_ptr;

        class peer_ram;
        typedef std::shared_ptr<peer_ram> peer_ram_ptr;

    }
} // namespace thinkyoung::db 
