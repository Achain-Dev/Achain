#include <glua/glua_common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <string>
#include <sstream>

#include <glua/lua.h>
#include <glua/lauxlib.h>
#include <glua/ldebug.h>
#include <glua/lobject.h>
#include <glua/lopcodes.h>
#include <glua/lundump.h>
#include <glua/lstring.h>

#include <glua/glua_compat.h>
#include <glua/glua_proto_info.h>
#include <glua/glua_disassemble.h>

namespace glua {
	namespace decompile {

		// extern int process_sub;           /* process sub functions? */

		static void clear_sstream(std::stringstream &ss)
		{
			ss.str(std::string());
			ss.clear();
		}

		Proto* findSubFunction(GluaDecompileContextP ctx, Proto* f, std::string funcnumstr, std::string &realfuncnumstr);

		void luadec_disassembleSubFunction(GluaDecompileContextP ctx, Proto* f, int dflag, const char* funcnumstr) {
			std::string realfuncnumstr;
			Proto* cf = findSubFunction(ctx, f, funcnumstr, realfuncnumstr);
			if (cf == nullptr) {
				fprintf(stderr, "No such sub function num : %s , use -pn option to get available num.\n", funcnumstr);
				return;
			}
			luadec_disassemble(ctx, cf, dflag, realfuncnumstr);
		}

		std::string RegOrConst(const Proto* f, int r) {
			if (ISK(r)) {
				return DecompileConstant(f, INDEXK(r));
			}
			else {
				char tmp[10];
				memset(tmp, 0x0, sizeof(tmp));
				sprintf(tmp, "R%d", r);
				tmp[9] = '\0';
				return tmp;
			}
		}

		std::string luadec_disassemble(GluaDecompileContextP ctx, Proto* fwork, int dflag, std::string name) {
			std::stringstream result;
			std::string line;
			std::stringstream lend;

			char tmp[MAXCONSTSIZE + 128];
			char tmp2[MAXCONSTSIZE + 128];
			std::string tmpconstant1;
			std::string tmpconstant2;

			Proto* f = fwork;
			int pc;
			auto name_len = name.length();
			int ignoreNext = 0;

			result << "; Function:        " << name << "\n";
			result << "; Defined at line: " << f->linedefined << "\n";
			result << "; #Upvalues:       " << NUPS(f) << "\n";
			result << "; #Parameters:     " << (int)f->numparams << "\n";
			result << "; Is_vararg:       " << (int)f->is_vararg << "\n";
			result << "; Max Stack Size:  " << (int)f->maxstacksize << "\n";
			result << "\n";

			ignoreNext = 0;
			for (pc = 0; pc < f->sizecode; pc++) {
				Instruction i = f->code[pc];
				OpCode o = GET_OPCODE(i);
				int a = GETARG_A(i);
				int b = GETARG_B(i);
				int c = GETARG_C(i);
				int bc = GETARG_Bx(i);
				int sbc = GETARG_sBx(i);
				int dest;
				line = "";
				clear_sstream(lend);

				if (ignoreNext) {
					ignoreNext--;
					printf("%5d [-]: %u\n", pc, i);
					continue;
				}
				switch (o) {
				case OP_MOVE:
					/*	A B	R(A) := R(B)					*/
					line = std::string("R") + std::to_string(a) + " R" + std::to_string(b);
					lend << "R" << a << " := R" << b;
					break;
				case OP_LOADK:
					/*	A Bx	R(A) := Kst(Bx)					*/
					line = std::string("R") + std::to_string(a) + " K" + std::to_string(bc);
					tmpconstant1 = DecompileConstant(f, bc);
					lend << "R" << a << " := " << tmpconstant1;
					break;
				case OP_LOADKX:
				{
					/*	A 	R(A) := Kst(extra arg)				*/
					int ax = GETARG_Ax(f->code[pc + 1]);
					line = std::string("R") + std::to_string(a);
					tmpconstant1 = DecompileConstant(f, ax);
					lend << "R" << a << " := K" << ax << " , " << tmpconstant1;
					break;
				}
				case OP_EXTRAARG:
				{
					/*	Ax	extra (larger) argument for previous opcode	*/
					int ax = GETARG_Ax(i);
					line = std::to_string(ax);
					break;
				}
				case OP_LOADBOOL:
					/*	A B C	R(A) := (Bool)B; if (C) pc++			*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c);
					if (c) {
						lend << "R" << a << " := " << (b ? "true" : "false") << "; goto " << pc + 2;
					}
					else {
						lend << "R" << a << " := " << (b ? "true" : "false");
					}
					break;
				case OP_LOADNIL:
				{
					/*	A B	R(A), ..., R(A+B) := nil		*/
					int rb = a + b;
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b);
					if (rb > a) {
						lend << "R" << a << " to R" << rb << " := nil";
					}
					else if (rb <= a) {
						lend << "R" << rb << " := nil";
					}
					break;
				}
				case OP_VARARG:
					/*	A B	R(A), R(A+1), ..., R(A+B-2) = vararg		*/
					// lua-5.1 and ANoFrillsIntroToLua51VMInstructions.pdf are wrong
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b);
					if (b > 2) {
						lend << "R" << a << " to R" << a + b - 2 << " := ...";
					}
					else if (b == 2) {
						lend << "R" << a << " := ...";
					}
					else if (b == 0) {
						lend << "R" << a << " to top := ...";
					}
					break;
				case OP_GETUPVAL:
					/*	A B	R(A) := UpValue[B]				*/
					line = std::string("R") + std::to_string(a) + " U" + std::to_string(b);
					lend << "R" << a << " := U" << b;
					break;
				case OP_GETTABUP:
					/*	A B C	R(A) := UpValue[B][RK(C)]			*/
					line = std::string("R") + std::to_string(a) + " U" + std::to_string(b) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(c);
					lend << "R" << a << " := U" << b << "[" << tmpconstant1 << "]";
					break;
				case OP_GETTABLE:
					/*	A B C	R(A) := R(B)[RK(C)]				*/
					line = std::string("R") + std::to_string(a) + " R" + std::to_string(b) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(c);
					lend << "R" << a << " := R" << b << "[" << tmpconstant1 << "]";
					break;
				case OP_SETTABUP:
					/*	A B C	UpValue[A][RK(B)] := RK(C)			*/
					line = std::string("U") + std::to_string(a) + " " + std::to_string(CC(b)) + std::to_string(CV(b)) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(b);
					tmpconstant2 = RK(c);
					lend << "U" << a << "[" << tmpconstant1 << "] := " << tmpconstant2;
					break;
				case OP_SETUPVAL:
					/*	A B	UpValue[B] := R(A)				*/
					line = std::string("R") + std::to_string(a) + " U" + std::to_string(b);
					lend << "U" << b << " := R" << a;
					break;
				case OP_SETTABLE:
					/*	A B C	R(A)[RK(B)] := RK(C)				*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(CC(b)) + std::to_string(CV(b)) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(b);
					tmpconstant2 = RK(c);
					lend << "R" << a << "[" << tmpconstant1 << "] := " << tmpconstant2;
					break;
				case OP_NEWTABLE:
					/*	A B C	R(A) := {} (size = B,C)				*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c);
					lend << "R" << a << " := {} (size = " << b << "," << c << ")";
					break;
				case OP_SELF:
					/*	A B C	R(A+1) := R(B); R(A) := R(B)[RK(C)]		*/
					line = std::string("R") + std::to_string(a) + " R" + std::to_string(b) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(c);
					lend << "R" << (a + 1) << " := R" << b << "; R" << a << " := R" << b << "[" << tmpconstant1 << "]";
					break;
				case OP_ADD:
					/*	A B C	R(A) := RK(B) + RK(C)				*/
				case OP_SUB:
					/*	A B C	R(A) := RK(B) - RK(C)				*/
				case OP_MUL:
					/*	A B C	R(A) := RK(B) * RK(C)				*/
				case OP_DIV:
					/*	A B C	R(A) := RK(B) / RK(C)				*/
				case OP_POW:
					/*	A B C	R(A) := RK(B) % RK(C)				*/
				case OP_MOD:
					/*	A B C	R(A) := RK(B) ^ RK(C)				*/
				case OP_IDIV:
					/*	A B C	R(A) := RK(B) // RK(C)				*/
				case OP_BAND:
					/*	A B C	R(A) := RK(B) & RK(C)				*/
				case OP_BOR:
					/*	A B C	R(A) := RK(B) | RK(C)				*/
				case OP_BXOR:
					/*	A B C	R(A) := RK(B) ~ RK(C)				*/
				case OP_SHL:
					/*	A B C	R(A) := RK(B) << RK(C)				*/
				case OP_SHR:
					/*	A B C	R(A) := RK(B) >> RK(C)				*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(CC(b)) + std::to_string(CV(b)) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(b);
					tmpconstant2 = RK(c);
					lend << "R" << a << " := " << tmpconstant1 << " " << operators[o] << " " << tmpconstant2;
					break;
				case OP_UNM:
					/*	A B	R(A) := -R(B)					*/
				case OP_NOT:
					/*	A B	R(A) := not R(B)				*/
				case OP_LEN:
					/*	A B	R(A) := length of R(B)				*/
				case OP_BNOT:
					/*	A B	R(A) := ~R(B)					*/
					line = std::string("R") + std::to_string(a) + " R" + std::to_string(b);
					lend << "R" << a << " := " << operators[o] << "R" << b;
					break;
				case OP_CONCAT:
					/*	A B C	R(A) := R(B).. ... ..R(C)			*/
					line = std::string("R") + std::to_string(a) + " R" + std::to_string(b) + " R" + std::to_string(c);
					lend << "R" << a << " := concat(R" << b << " to R" << c << ")";
					break;
				case OP_JMP:
					/*	sBx	pc+=sBx					*/
					dest = pc + sbc + 1;
					line = std::to_string(sbc);
					lend << "PC += " << sbc << " (goto " << dest << ")";
					// instead OP_CLOSE in 5.2 : if (A) close all upvalues >= R(A-1)
					// lua-5.2/src/lopcodes.h line 199 is wrong. See lua-5.2/src/lvm.c line 504:
					// if (a > 0) luaF_close(L, ci->u.l.base + a - 1);
					line = std::string("R") + std::to_string(a) + " " + std::to_string(sbc);
					if (a > 0) {
						lend << "; close all upvalues in R" << (a - 1) << " to top";
					}
					break;
				case OP_EQ:
					/*	A B C	if ((RK(B) == RK(C)) ~= A) then pc++		*/
				case OP_LT:
					/*	A B C	if ((RK(B) <  RK(C)) ~= A) then pc++  		*/
				case OP_LE:
					/*	A B C	if ((RK(B) <= RK(C)) ~= A) then pc++  		*/
					dest = GETARG_sBx(f->code[pc + 1]) + pc + 2;
					line = std::to_string(a) + " " + std::to_string(CC(b)) + std::to_string(CV(b)) + " " + std::to_string(CC(c)) + std::to_string(CV(c));
					tmpconstant1 = RK(b);
					tmpconstant2 = RK(c);
					lend << "if " << tmpconstant1 << " " << (a ? invopstr(o) : opstr(o)) << " " << tmpconstant2 << " then goto " << (pc + 2) << " else goto " << dest;
					break;
				case OP_TEST:
					/*	A C	if not (R(A) <=> C) then pc++			*/
					dest = GETARG_sBx(f->code[pc + 1]) + pc + 2;
					line = std::string("R") + std::to_string(a) + " " + std::to_string(c);
					lend << "if " << (c ? "not" : "") << "R" << a << " then goto " << (pc + 2) << " else goto " << dest;
					break;
				case OP_TESTSET:
					/*	A B C	if (R(B) <=> C) then R(A) := R(B) else pc++	*/
					dest = GETARG_sBx(f->code[pc + 1]) + pc + 2;
					line = std::string("R") + std::to_string(a) + " R" + std::to_string(b) + " " + std::to_string(c);
					lend << "if " << (c ? "" : "not ") << "R" << b << " then R" << a << " := R" << b << " ; goto " << dest << " else goto " << (pc + 2);
					break;
				case OP_CALL:
					/*	A B C	R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
				case OP_TAILCALL:
					/*	A B C	return R(A)(R(A+1), ... ,R(A+B-1))		*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c);
					if (b > 2) {
						sprintf(tmp, "R%d to R%d", a + 1, a + b - 1);
					}
					else if (b == 2) {
						sprintf(tmp, "R%d", a + 1);
					}
					else if (b == 1) {
						sprintf(tmp, "");
					}
					else if (b == 0) {
						sprintf(tmp, "R%d to top", a + 1);
					}

					if (c > 2) {
						sprintf(tmp2, "R%d to R%d", a, a + c - 2);
					}
					else if (c == 2) {
						sprintf(tmp2, "R%d", a);
					}
					else if (c == 1) {
						sprintf(tmp2, "");
					}
					else if (c == 0) {
						sprintf(tmp2, "R%d to top", a);
					}
					lend << tmp2 << " := R" << a << "(" << tmp << ")";
					break;
				case OP_RETURN:
					/*	A B	return R(A), ... ,R(A+B-2)	(see note)	*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b);
					if (b > 2) {
						sprintf(tmp, "R%d to R%d", a, a + b - 2);
					}
					else if (b == 2) {
						sprintf(tmp, "R%d", a);
					}
					else if (b == 1) {
						sprintf(tmp, "");
					}
					else if (b == 0) {
						sprintf(tmp, "R%d to top", a);
					}
					lend << "return " << tmp;
					break;
				case OP_FORLOOP:
					/*	A sBx	R(A)+=R(A+2);
					if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) }*/
					dest = pc + sbc + 1;
					line = std::string("R") + std::to_string(a) + " " + std::to_string(sbc);
					lend << "R" << a << " += R" << (a + 2) << "; if R" << a << " <= R" << (a + 1) << " then R"
						<< (a + 3) << " := R" << a << "; PC += " << sbc << " , goto " << dest << " end";
					break;
				case OP_FORPREP:
					/*	A sBx	R(A)-=R(A+2); pc+=sBx				*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(sbc);
					lend << "R" << a << " -= R" << (a + 2) << "; PC += " << sbc << " (goto " << (pc + sbc + 1) << ")";
					break;
				case LUADEC_TFORLOOP:
					/*	A C	R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));	*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(c);
					if (c == 1) {
						sprintf(tmp2, "R%d", a + 3);
					}
					else if (c > 1) {
						sprintf(tmp2, "R%d to R%d", a + 3, a + c + 2);
					}
					else {
						sprintf(tmp2, "ERROR c=0");
					}
					lend << tmp2 << " := R" << a << "(R" << (a + 1) << ",R" << (a + 2) << ")";
					break;
				case OP_TFORLOOP:
					/*	A sBx	if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx }*/
					dest = pc + sbc + 1;
					line = std::string("R") + std::to_string(a) + " " + std::to_string(sbc);
					lend << "if R" << (a + 1) << " ~= nil then { R" << a
						<< " := R" << (a + 1) << " ; PC += " << sbc << " (goto " << dest << ") }";
					break;
				case OP_SETLIST:
				{
					/*	A B C	R(A)[(C-1)*FPF+i] := R(A+i), 1 <= i <= B	*/
					int next_is_extraarg = 1;
					unsigned int realc = c, startindex;
					if (c == 0) {
						Instruction i_next_arg = f->code[pc + 1];
						if (GET_OPCODE(i_next_arg) == OP_EXTRAARG) {
							realc = GETARG_Ax(i_next_arg);
						}
						else {
							next_is_extraarg = 0;
						}
					}
					startindex = (realc - 1)*LFIELDS_PER_FLUSH;
					line = std::string("R") + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c);
					if (b == 0) {
						lend << "R" << a << "[" << startindex << "] to R" << a << "[topp] := R" << (a + 1) << " to top";
					}
					else if (b == 1) {
						lend << "R" << a << "[" << startindex << "] := R" << (a + 1);
					}
					else if (b > 1) {
						lend << "R" << a << "[" << startindex << "] to R" << a
							<< "[" << (startindex + b - 1) << "] := R" << (a + 1) << " to R" << (a + b);
					}
					if (c != 0) {
						lend << " ; R(a)[(c-1)*FPF+i] := R(a+i), 1 <= i <= b, a=" << a << ", b=" << b
							<< ", c=" << c << ", FPF=" << LFIELDS_PER_FLUSH;
					}
					else {
						lend << " ; R(a)[(realc-1)*FPF+i] := R(a+i), 1 <= i <= b, a=" << a << ", b=" << b
							<< ", c=" << c << ", realc=" << realc << ", FPF=" << LFIELDS_PER_FLUSH;
						if (!next_is_extraarg) {
							lend << " ; Error: SETLIST with c==0, but not followed by EXTRAARG.";
						}
						if (realc == 0) {
							lend << " ; Error: SETLIST with c==0, but realc==0.";
						}
					}
					break;
				}
				case OP_CLOSURE:
					/*	A Bx	R(A) := closure(KPROTO[Bx])		*/
					line = std::string("R") + std::to_string(a) + " " + std::to_string(bc);
					if (name_len > 0) {
						lend << "R" << a << " := closure(Function #" << name << "_" << bc << ")";
					}
					else {
						lend << "R" << a << " := closure(Function #" << bc << ")";
					}
					break;
				default:
					break;
				}
				char tmpstr[4096] = "\0";
				sprintf(tmpstr, "%5d [-]: %-9s %-13s; %s\n", pc, luaP_opnames[o], line.c_str(), lend.str().c_str());
				tmpstr[4095] = '\0';
				result << tmpstr;
			}
			clear_sstream(lend);
			result << "\n\n";
			if (ctx->process_sub && f->sizep != 0) {
				std::string subname;
				for (pc = 0; pc < f->sizep; pc++) {
					if (name_len > 0) {
						subname = name + "_" + std::to_string(pc);
					}
					else {
						subname = std::to_string(pc);
					}
					auto sub_result = luadec_disassemble(ctx, f->p[pc], dflag, subname);
					result << sub_result << "\n";
				}
			}
			return result.str();
		}

	}
}