#ifndef glua_disassemble_h
#define glua_disassemble_h

#include <glua/lprefix.h>
#include <glua/lua.h>
#include <glua/lobject.h>
#include <string>
#include <glua/glua_proto_info.h>

namespace glua {
	namespace decompile {

#define CC(r) (ISK((r)) ? 'K' : 'R')
#define CV(r) (ISK((r)) ? INDEXK(r) : r)

#define RK(r) (RegOrConst(f, r))

#define MAXCONSTSIZE 1024

		std::string luadec_disassemble(GluaDecompileContextP ctx, Proto* fwork, int dflag, std::string name);

		void luadec_disassembleSubFunction(GluaDecompileContextP ctx, Proto* f, int dflag, const char* funcnumstr);

		std::string RegOrConst(const Proto* f, int r);

	}
}

#endif // #ifndef glua_disassemble_h
