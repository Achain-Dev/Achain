#pragma once
#include <boost/iterator/iterator_facade.hpp>

#include <fc/network/ip.hpp>
#include <fc/time.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw.hpp>

namespace thinkyoung {
    namespace net {

        enum PotentialPeerLastConnectionDisposition
        {
            never_attempted_to_connect,
            last_connection_failed,
            last_connection_rejected,
            last_connection_handshaking_failed,
            last_connection_succeeded
        };

        struct PotentialPeerEntry
        {
            fc::ip::endpoint                  endpoint;
            fc::time_point_sec                last_seen_time;
            fc::enum_type<uint8_t, PotentialPeerLastConnectionDisposition> last_connection_disposition;
            fc::time_point_sec                last_connection_attempt_time;
            uint32_t                          number_of_successful_connection_attempts;
            uint32_t                          number_of_failed_connection_attempts;
            fc::optional<fc::exception>       last_error;

            PotentialPeerEntry() :
                number_of_successful_connection_attempts(0),
                number_of_failed_connection_attempts(0){}

            PotentialPeerEntry(fc::ip::endpoint endpoint,
                fc::time_point_sec last_seen_time = fc::time_point_sec(),
                PotentialPeerLastConnectionDisposition last_connection_disposition = never_attempted_to_connect) :
                endpoint(endpoint),
                last_seen_time(last_seen_time),
                last_connection_disposition(last_connection_disposition),
                number_of_successful_connection_attempts(0),
                number_of_failed_connection_attempts(0)
            {}
        };

        namespace detail
        {
            class PeerDatabaseImpl;

            class PeerDatabaseIteratorImpl;
            class PeerDatabaseIterator : public boost::iterator_facade < PeerDatabaseIterator, const PotentialPeerEntry, boost::forward_traversal_tag >
            {
            public:
                PeerDatabaseIterator();
                ~PeerDatabaseIterator();
                explicit PeerDatabaseIterator(PeerDatabaseIteratorImpl* impl);
                PeerDatabaseIterator(const PeerDatabaseIterator& c);

            private:
                friend class boost::iterator_core_access;
                void increment();
                bool equal(const PeerDatabaseIterator& other) const;
                const PotentialPeerEntry& dereference() const;
            private:
                std::unique_ptr<PeerDatabaseIteratorImpl> my;
            };
        }


        class PeerDatabase
        {
        public:
            PeerDatabase();
            ~PeerDatabase();

            void open(const fc::path& databaseFilename);
            void close();
            void clear();

            void erase(const fc::ip::endpoint& endpointToErase);

            void update_entry(const PotentialPeerEntry& updatedentry);
            PotentialPeerEntry lookup_or_create_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup);
            fc::optional<PotentialPeerEntry> lookup_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup);

            std::vector<PotentialPeerEntry> get_all()const;

            typedef detail::PeerDatabaseIterator iterator;
            iterator begin() const;
            iterator end() const;
            size_t size() const;
        private:
            std::unique_ptr<detail::PeerDatabaseImpl> my;
        };

    }
} // end namespace thinkyoung::net

FC_REFLECT_ENUM(thinkyoung::net::PotentialPeerLastConnectionDisposition, (never_attempted_to_connect)(last_connection_failed)(last_connection_rejected)(last_connection_handshaking_failed)(last_connection_succeeded))
FC_REFLECT(thinkyoung::net::PotentialPeerEntry, (endpoint)(last_seen_time)(last_connection_disposition)(last_connection_attempt_time)(number_of_successful_connection_attempts)(number_of_failed_connection_attempts)(last_error))
