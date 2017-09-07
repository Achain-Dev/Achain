#ifndef glua_proto_info_h
#define glua_proto_info_h

#include <glua/ldebug.h>
#include <glua/lobject.h>
#include <glua/lopcodes.h>
#include <glua/lundump.h>
#include <string>
#include <sstream>
#include <memory>

namespace glua {
	namespace decompile {

#define ASCII 437
#define GB2312 20936
#define GBK 936
#define GB18030 54936
#define BIG5 950
#define UTF8 65001

#define ENCODINGS "ASCII GB2312 GBK GB18030 BIG5 UTF8"

#define GLOBAL(r) ((char*)svalue(&f->k[r]))
#define opstr(o) ((o)==OP_EQ?"==":(o)==OP_LE?"<=":(o)==OP_LT?"<":(((o)==OP_TEST)||((o)==OP_TESTSET))?NULL:"?") // Lua5.1 specific
#define invopstr(o) ((o)==OP_EQ?"~=":(o)==OP_LE?">":(o)==OP_LT?">=":(((o)==OP_TEST)||((o)==OP_TESTSET))?"not":"?") // Lua5.1 specific
#define IsMain(ctx, f) ((f)==(ctx)->glproto)

		extern const char* operators[];

		extern int priorities[];

		extern int string_encoding;

		typedef struct Inst_ Inst;
		struct Inst_ {
			OpCode op;
			int a;
			int b;
			int c;
			int bx;
			int sbx;
			int ax;
		};

		Inst extractInstruction(Instruction i);
		Instruction assembleInstruction(Inst inst);

		void InitOperators();

		int getEncoding(const char* src);

		std::string DecompileString(const TValue* o);

		std::string DecompileConstant(const Proto* f, int i);

		class GluaDecompileContext
		{
		public:
			std::stringstream error_ss;
			int localdeclare[255][255];
			int functionnum;
			int locals; /* strip debug information? */
			int disassemble; /* disassemble? */
			int stripping; /* strip debug information? */
			int dumping; /* dump bytecodes? */
			int printfuncnum; /* print function nums? */
			int process_sub; /* process sub functions? */
			int func_check; /* compile decompiled function and compare? */
			int guess_locals;
			std::string funcnumstr;
			int debug; /* debug decompiler? */
			lua_State* glstate;
			Proto* glproto;
			std::string unknown_local;
			std::string unknown_upvalue;
			std::string errortmp;

			std::string error;
			int error_code;

			inline GluaDecompileContext()
			{
				memset(localdeclare, 0x0, sizeof(localdeclare));
				functionnum = 0;
				locals = 0;
				disassemble = 0;
				stripping = 0;
				dumping = 1;
				printfuncnum = 0;
				process_sub = 1;
				func_check = 0;
				guess_locals = 1;
				debug = 0;
				glstate = nullptr;
				glproto = nullptr;
				unknown_local = "ERROR_unknown_local_xxxx";
				unknown_upvalue = "ERROR_unknown_upvalue_xxxx";
				error_code = 0;
			}

		public:
			void reset_error();
		};

        typedef std::shared_ptr<GluaDecompileContext> GluaDecompileContextP;

	}
}

#endif // #ifndef glua_proto_info_h
