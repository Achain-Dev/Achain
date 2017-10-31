// Most of this file has been ported from EncryptionUtils.cpp from BitcoinArmory:
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright(C) 2011-2013, Armory Technologies, Inc.                         //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE or http://www.gnu.org/licenses/agpl.html                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// For the KDF:
//
// This technique is described in Colin Percival's paper on memory-hard
// key-derivation functions, used to create "scrypt":
//
//       http://www.tarsnap.com/scrypt/scrypt.pdf
//
// The goal is to create a key-derivation function that can force a memory
// requirement on the thread applying the KDF.  By picking a sequence-length
// of 1,000,000, each thread will require 32 MB of memory to compute the keys,
// which completely disarms GPUs of their massive parallelization capabilities
// (for maximum parallelization, the kernel must use less than 1-2 MB/thread)
//
// Even with less than 1,000,000 hashes, as long as it requires more than 64
// kB of memory, a GPU will have to store the computed lookup tables in global
// memory, which is extremely slow for random lookup.  As a result, GPUs are
// no better (and possibly much worse) than a CPU for brute-forcing the passwd
//
// This KDF is actually the ROMIX algorithm described on page 6 of Colin's
// paper.  This was chosen because it is the simplest technique that provably
// achieves the goal of being secure, and memory-hard.
//
// The computeKdfParams method well test the speed of the system it is running
// on, and try to pick the largest memory-size the system can compute in less
// than 0.25s (or specified target).
//
//
// NOTE:  If you are getting an error about invalid argument types, from python,
//        it is usually because you passed in a BinaryData/Python-string instead
//        of a SecureBinaryData object
//
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <stdint.h>
#include <string>

namespace fc {
////////////////////////////////////////////////////////////////////////////////
// A memory-bound key-derivation function -- uses a variation of Colin
// Percival's ROMix algorithm: http://www.tarsnap.com/scrypt/scrypt.pdf
//
// The computeKdfParams method takes in a target time, T, for computation
// on the computer executing the test.  The final KDF should take somewhere
// between T/2 and T seconds.
    class romix
    {
    public:
        romix(uint32_t memReqts, uint32_t numIter, std::string salt);

        std::string deriveKey_OneIter(std::string const & password);
        std::string deriveKey(std::string const & password);

    private:
        uint32_t hashOutputBytes_;
        uint32_t kdfOutputBytes_;    // size of final key data

        uint32_t memoryReqtBytes_;
        uint32_t sequenceCount_;
        std::string salt_;            // prob not necessary amidst numIter, memReqts
                                      // but I guess it can't hurt

        uint32_t numIterations_;     // We set the ROMIX params for a given memory
                                      // req't. Then run it numIter times to meet
                                      // the computation-time req't
    };

}


