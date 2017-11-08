/*
   author: pli
   date: 2017.11.07
   lvm manager, run or stop the lvm process
*/

#ifndef _LVM_MGR_H_
#define _LVM_MGR_H_

#include <fc/filesystem.hpp>
#include <fc/interprocess/process.hpp>

#include <memory>

namespace thinkyoung {
    namespace lvm {
        class LvmMgr {
         public:
             LvmMgr();
             ~LvmMgr();

             void run_lvm();
             void set_lvm_path_file(const fc::path& lvm_path_file);

          private:
             void terminate_lvm_process();
             void start_lvm_process();

         private:
             fc::path _lvm_path_file;
             fc::process_ptr _p_lvm_process;
        };
        typedef std::shared_ptr<LvmMgr> LvmMgrPtr;
    }
} // thinkyoung::lvm
#endif
