#include <db/Exception.hpp>
#include <db/UpgradeLeveldb.hpp>
#include <fc/log/logger.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/filesystem/fstream.hpp>

namespace thinkyoung {
    namespace db {

        upgrade_db_mapper& upgrade_db_mapper::instance()
        {
            static upgrade_db_mapper  mapper;
            return mapper;
        }

        int32_t upgrade_db_mapper::add_type(const std::string& type_name, const upgrade_db_function& function)
        {
            _upgrade_db_function_registry[type_name] = function;
            return 0;
        }


        // this code has no alp dependencies, and it
        // could be moved to fc, if fc ever adds a leveldb dependency
        void try_upgrade_db(const fc::path& dir, leveldb::DB* dbase, const char* entry_type, size_t entry_type_size)
        {
            size_t old_entry_type_size = 0;
            std::string old_entry_type;
            fc::path entry_type_filename = dir / "entry_TYPE";
            //if no entry_TYPE file exists
            if (!boost::filesystem::exists(entry_type_filename))
            {
                //must be original type for the database
                old_entry_type = entry_type;
                int last_char = old_entry_type.length() - 1;
                //strip version number from current_entry_name and append 0 to set old_entry_type (e.g. mytype0)
                while (last_char >= 0 && isdigit(old_entry_type[last_char]))
                {
                    --last_char;
                }

                //upgradeable entry types should always end with version number
                if ('v' != old_entry_type[last_char])
                {
                    //ilog("Database ${db} is not upgradeable",("db",dir.to_native_ansi_path()));
                    return;
                }

                ++last_char;
                old_entry_type[last_char] = '0';
                old_entry_type.resize(last_char + 1);
            }
            else //read entry type from file
            {
                boost::filesystem::ifstream is(entry_type_filename);
                char buffer[120];
                is.getline(buffer, 120);
                old_entry_type = buffer;
                is >> old_entry_type_size;
            }
            if (old_entry_type != entry_type)
            {
                //check if upgrade function in registry
                auto upgrade_function_itr = upgrade_db_mapper::instance()._upgrade_db_function_registry.find(old_entry_type);
                if (upgrade_function_itr != upgrade_db_mapper::instance()._upgrade_db_function_registry.end())
                {
                    ilog("Upgrading database ${db} from ${old} to ${new}", ("db", dir.preferred_string())
                        ("old", old_entry_type)
                        ("new", entry_type));
                    //update database's entry_TYPE to new entry type name
                    boost::filesystem::ofstream os(entry_type_filename);
                    os << entry_type << std::endl;
                    os << entry_type_size;
                    //upgrade the database using upgrade function
                    upgrade_function_itr->second(dbase);
                }
                else
                {
                    elog("In ${db}, entry types ${old} and ${new} do not match, but no upgrade function found!",
                        ("db", dir.preferred_string())("old", old_entry_type)("new", entry_type));
                }
            }
            else if (old_entry_type_size == 0) //if entry type file never created, create it now
            {
                boost::filesystem::ofstream os(entry_type_filename);
                os << entry_type << std::endl;
                os << entry_type_size;
            }
            else if (old_entry_type_size != entry_type_size)
            {
                elog("In ${db}, entry type matches ${new}, but entry sizes do not match!",
                    ("db", dir.preferred_string())("new", entry_type));

            }
        }
    }
} // namespace thinkyoung;:db
