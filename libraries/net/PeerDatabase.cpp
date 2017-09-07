#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/tag.hpp>

#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>

#include <net/PeerDatabase.hpp>
#include <db/LevelPodMap.hpp>
#include "db/LevelMap.hpp"



namespace thinkyoung {
    namespace net {
        namespace detail
        {
            using namespace boost::multi_index;

            struct PotentialPeerDatabaseEntry
            {
                uint32_t              database_key;
                PotentialPeerEntry peer_entry;

                PotentialPeerDatabaseEntry(uint32_t database_key, const PotentialPeerEntry& peer_entry) :
                    database_key(database_key),
                    peer_entry(peer_entry)
                {}
                PotentialPeerDatabaseEntry(const PotentialPeerDatabaseEntry& other) :
                    database_key(other.database_key),
                    peer_entry(other.peer_entry)
                {}

                const fc::time_point_sec& get_last_seen_time() const { return peer_entry.last_seen_time; }
                const fc::ip::endpoint&   get_endpoint() const { return peer_entry.endpoint; }
            };

            class PeerDatabaseImpl
            {
            public:
                struct last_seen_time_index {};
                struct endpoint_index {};
                typedef boost::multi_index_container < PotentialPeerDatabaseEntry,
                    indexed_by< ordered_non_unique< tag<last_seen_time_index>,
                    const_mem_fun< PotentialPeerDatabaseEntry,
                    const fc::time_point_sec&,
                    &PotentialPeerDatabaseEntry::get_last_seen_time>
                    >,
                    hashed_unique< tag<endpoint_index>,
                    const_mem_fun< PotentialPeerDatabaseEntry,
                    const fc::ip::endpoint&,
                    &PotentialPeerDatabaseEntry::get_endpoint
                    >,
                    std::hash < fc::ip::endpoint >
                    >
                    >
                > PotentialPeerSet;
                //private:
                typedef thinkyoung::db::level_pod_map<uint32_t, PotentialPeerEntry> PotentialPeerLeveldb;
                PotentialPeerLeveldb    _leveldb;

                PotentialPeerSet     _potential_peer_set;


            public:
                void open(const fc::path& databaseFilename);
                void close();
                void clear();
                void erase(const fc::ip::endpoint& endpointToErase);
                void update_entry(const PotentialPeerEntry& updatedentry);
                PotentialPeerEntry lookup_or_create_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup);
                fc::optional<PotentialPeerEntry> lookup_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup);

                PeerDatabase::iterator begin() const;
                PeerDatabase::iterator end() const;
                size_t size() const;
            };

            class PeerDatabaseIteratorImpl
            {
            public:
                typedef PeerDatabaseImpl::PotentialPeerSet::index<PeerDatabaseImpl::last_seen_time_index>::type::iterator LastSeenTimeIndexIterator;
                LastSeenTimeIndexIterator _iterator;
                PeerDatabaseIteratorImpl(const LastSeenTimeIndexIterator& iterator) :
                    _iterator(iterator)
                {}
            };
            PeerDatabaseIterator::PeerDatabaseIterator(const PeerDatabaseIterator& c)
                :boost::iterator_facade<PeerDatabaseIterator, const PotentialPeerEntry, boost::forward_traversal_tag>(c){}

            void PeerDatabaseImpl::open(const fc::path& databaseFilename)
            {
                try
                {
                    _leveldb.open(databaseFilename);
                }
                catch (const thinkyoung::db::level_pod_map_open_failure&)
                {
                    fc::remove_all(databaseFilename);
                    _leveldb.open(databaseFilename);
                }
                _potential_peer_set.clear();

                for (auto iter = _leveldb.begin(); iter.valid(); ++iter)
                    _potential_peer_set.insert(PotentialPeerDatabaseEntry(iter.key(), iter.value()));
#define MAXIMUM_PEERDB_SIZE 1000
                if (_potential_peer_set.size() > MAXIMUM_PEERDB_SIZE)
                {
                    // prune database to a reasonable size
                    auto iter = _potential_peer_set.begin();
                    std::advance(iter, MAXIMUM_PEERDB_SIZE);
                    while (iter != _potential_peer_set.end())
                    {
                        _leveldb.remove(iter->database_key);
                        iter = _potential_peer_set.erase(iter);
                    }
                }
            }

            void PeerDatabaseImpl::close()
            {
                _leveldb.close();
                _potential_peer_set.clear();
            }

            void PeerDatabaseImpl::clear()
            {
                auto iter = _leveldb.begin();
                while (iter.valid())
                {
                    uint32_t key_to_remove = iter.key();
                    ++iter;
                    try
                    {
                        _leveldb.remove(key_to_remove);
                    }
                    catch (fc::exception&)
                    {
                        // shouldn't happen, and if it does there's not much we can do
                    }
                }
                _potential_peer_set.clear();
            }

            void PeerDatabaseImpl::erase(const fc::ip::endpoint& endpointToErase)
            {
                auto iter = _potential_peer_set.get<endpoint_index>().find(endpointToErase);
                if (iter != _potential_peer_set.get<endpoint_index>().end())
                {
                    _leveldb.remove(iter->database_key);
                    _potential_peer_set.get<endpoint_index>().erase(iter);
                }
            }

            void PeerDatabaseImpl::update_entry(const PotentialPeerEntry& updatedentry)
            {
                auto iter = _potential_peer_set.get<endpoint_index>().find(updatedentry.endpoint);
                if (iter != _potential_peer_set.get<endpoint_index>().end())
                {
                    _potential_peer_set.get<endpoint_index>().modify(iter, [&updatedentry](PotentialPeerDatabaseEntry& entry) { entry.peer_entry = updatedentry; });
                    _leveldb.store(iter->database_key, updatedentry);
                }
                else
                {
                    uint32_t last_database_key;
                    _leveldb.last(last_database_key);
                    uint32_t new_database_key = last_database_key + 1;
                    PotentialPeerDatabaseEntry new_database_entry(new_database_key, updatedentry);
                    _potential_peer_set.get<endpoint_index>().insert(new_database_entry);
                    _leveldb.store(new_database_key, updatedentry);
                }
            }

            PotentialPeerEntry PeerDatabaseImpl::lookup_or_create_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup)
            {
                auto iter = _potential_peer_set.get<endpoint_index>().find(endpointToLookup);
                if (iter != _potential_peer_set.get<endpoint_index>().end())
                    return iter->peer_entry;
                return PotentialPeerEntry(endpointToLookup);
            }

            fc::optional<PotentialPeerEntry> PeerDatabaseImpl::lookup_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup)
            {
                auto iter = _potential_peer_set.get<endpoint_index>().find(endpointToLookup);
                if (iter != _potential_peer_set.get<endpoint_index>().end())
                    return iter->peer_entry;
                return fc::optional<PotentialPeerEntry>();
            }

            PeerDatabase::iterator PeerDatabaseImpl::begin() const
            {
                return PeerDatabase::iterator(new PeerDatabaseIteratorImpl(_potential_peer_set.get<last_seen_time_index>().begin()));
            }

            PeerDatabase::iterator PeerDatabaseImpl::end() const
            {
                return PeerDatabase::iterator(new PeerDatabaseIteratorImpl(_potential_peer_set.get<last_seen_time_index>().end()));
            }

            size_t PeerDatabaseImpl::size() const
            {
                return _potential_peer_set.size();
            }

            PeerDatabaseIterator::PeerDatabaseIterator()
            {
            }

            PeerDatabaseIterator::~PeerDatabaseIterator()
            {
            }

            PeerDatabaseIterator::PeerDatabaseIterator(PeerDatabaseIteratorImpl* impl) :
                my(impl)
            {
            }

            void PeerDatabaseIterator::increment()
            {
                ++my->_iterator;
            }

            bool PeerDatabaseIterator::equal(const PeerDatabaseIterator& other) const
            {
                return my->_iterator == other.my->_iterator;
            }

            const PotentialPeerEntry& PeerDatabaseIterator::dereference() const
            {
                return my->_iterator->peer_entry;
            }

        } // end namespace detail



        PeerDatabase::PeerDatabase() :
            my(new detail::PeerDatabaseImpl)
        {
        }

        PeerDatabase::~PeerDatabase()
        {}

        void PeerDatabase::open(const fc::path& databaseFilename)
        {
            my->open(databaseFilename);
        }

        void PeerDatabase::close()
        {
            my->close();
        }

        void PeerDatabase::clear()
        {
            my->clear();
        }

        void PeerDatabase::erase(const fc::ip::endpoint& endpointToErase)
        {
            my->erase(endpointToErase);
        }

        void PeerDatabase::update_entry(const PotentialPeerEntry& updatedentry)
        {
            my->update_entry(updatedentry);
        }

        PotentialPeerEntry PeerDatabase::lookup_or_create_entry_for_endpoint(const fc::ip::endpoint& endpointToLookup)
        {
            return my->lookup_or_create_entry_for_endpoint(endpointToLookup);
        }

        fc::optional<PotentialPeerEntry> PeerDatabase::lookup_entry_for_endpoint(const fc::ip::endpoint& endpoint_to_lookup)
        {
            return my->lookup_entry_for_endpoint(endpoint_to_lookup);
        }

        PeerDatabase::iterator PeerDatabase::begin() const
        {
            return my->begin();
        }

        PeerDatabase::iterator PeerDatabase::end() const
        {
            return my->end();
        }

        size_t PeerDatabase::size() const
        {
            return my->size();
        }
        std::vector<PotentialPeerEntry> PeerDatabase::get_all()const
        {
            std::vector<PotentialPeerEntry> results;
            auto itr = my->_leveldb.begin();
            while (itr.valid())
            {
                results.push_back(itr.value());
                ++itr;
            }
            return results;
        }


    }
} // end namespace thinkyoung::net
