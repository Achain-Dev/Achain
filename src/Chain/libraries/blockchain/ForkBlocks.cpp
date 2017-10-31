//#define DEFAULT_LOGGER "blockchain"

#include <blockchain/ForkBlocks.hpp>
#include <boost/preprocessor/tuple.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <map>
#include <algorithm>
#include <assert.h>
#include <string>

namespace thinkyoung {
    namespace blockchain {

        namespace detail
        {
            struct ForkVersionData
            {
                uint32_t    block_number;
                std::string version_number;
            };
            std::map<uint32_t, ForkVersionData> fork_version_data_by_revision;
            bool fork_version_data_by_revision_map_initialized = false;

            void initialize_fork_version_data_if_necessary()
            {
                if (!detail::fork_version_data_by_revision_map_initialized)
                {
#define INSERT_ELEMENT(r, data, elem) detail::fork_version_data_by_revision[BOOST_PP_TUPLE_ELEM(3, 2, elem)] = detail::ForkVersionData{BOOST_PP_TUPLE_ELEM(3, 0, elem), BOOST_PP_TUPLE_ELEM(3, 1, elem)};
                    BOOST_PP_SEQ_FOR_EACH(INSERT_ELEMENT, _, ALP_FORK_TO_UNIX_TIME_LIST)
                        detail::fork_version_data_by_revision_map_initialized = true;
                }
            }
        } // end namespace detail

        uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t revision_time)
        {
            detail::initialize_fork_version_data_if_necessary();

            auto iter = detail::fork_version_data_by_revision.lower_bound(revision_time);
            if (iter == detail::fork_version_data_by_revision.end() ||
                iter->first != revision_time)
            {
                if (iter == detail::fork_version_data_by_revision.begin())
                    return 0;
                else
                    --iter;
            }
            return iter->second.block_number;
        }

        std::vector<uint32_t> get_list_of_fork_block_numbers()
        {
            detail::initialize_fork_version_data_if_necessary();
            std::vector<uint32_t> fork_block_numbers;
            fork_block_numbers.reserve(detail::fork_version_data_by_revision.size());
            for (auto value : detail::fork_version_data_by_revision)
                fork_block_numbers.push_back(value.second.block_number);
            assert(std::is_sorted(fork_block_numbers.begin(), fork_block_numbers.end()));
            return fork_block_numbers;
        }

    }
} // thinkyoung::blockchain
