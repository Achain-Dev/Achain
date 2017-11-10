#include <lvm/LvmMgr.hpp>

const uint64_t g_u64_timeout = 2 * 1000;

namespace thinkyoung {
    namespace lvm {

        LvmMgr::LvmMgr()
            : _lvm_path_file(fc::path()),
              _p_lvm_process(nullptr) {
        }

        LvmMgr::~LvmMgr() {
            terminate_lvm_process();
        }

        void LvmMgr::run_lvm() {
            terminate_lvm_process();
            start_lvm_process();
        }

        void LvmMgr::start_lvm_process() {
            if (fc::exists(_lvm_path_file)) {
                if (!_p_lvm_process) {
                    _p_lvm_process = std::make_shared<fc::process>();
                }
                std::vector<std::string> args;
                fc::path work_dir = _lvm_path_file.parent_path();
                _p_lvm_process->exec(_lvm_path_file, args,
                    work_dir, fc::iprocess::open_none);
            }
        }

        void LvmMgr::terminate_lvm_process() {
            if (_p_lvm_process) {
                _p_lvm_process->kill();
                _p_lvm_process->result(fc::microseconds(g_u64_timeout));
                _p_lvm_process.reset();
            }
        }

        void LvmMgr::set_lvm_path_file(const fc::path& lvm_path_file) {
#ifdef _WIN32
            _lvm_path_file = lvm_path_file / "lvm.exe";
#else
            _lvm_path_file = lvm_path_file / "lvm";
#endif
            std::string  str = _lvm_path_file.string();
        }
    }
} // thinkyoung::lvm
