#pragma once
#include <stdint.h>
#include <memory>
#include <fc/network/ip.hpp>

namespace thinkyoung {
    namespace net {

        namespace detail { class UpnpServiceImpl; }

        class UpnpService
        {
        public:
            UpnpService();
            ~UpnpService();

            fc::ip::address external_ip()const;
            uint16_t mapped_port()const;
            void map_port(uint16_t p);
        private:
            std::unique_ptr<detail::UpnpServiceImpl> my;
        };

    }
} // thinkyoung::net
