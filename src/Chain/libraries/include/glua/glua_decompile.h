#ifndef glua_decompile_h
#define LUADEC_DECOMPILE_H

#include <glua/lprefix.h>
#include <glua/lua.h>
#include <glua/llimits.h>
#include <glua/glua_structs.h>
#include <glua/glua_statement.h>
#include <glua/glua_proto_info.h>
#include <glua/lcode.h>
#include <string>
#include <sstream>
#include <vector>

namespace glua {
	namespace decompile {

		struct BoolOp_ {
			ListItem super;
			std::string op1;
			std::string op2;
			OpCode op;
			int neg;
			int pc;
			int dest;
		};
		typedef struct BoolOp_ BoolOp;

		struct LoopItem_ {
			LoopItem_* parent;
			LoopItem_* child;
			LoopItem_* prev;
			LoopItem_* next;

			StatementType type;
			int prep;
			int start;
			int body;
			int end;
			int out;

			int indent;
			AstStatement* block;

			inline LoopItem_()
			{
				parent = nullptr;
				child = nullptr;
				prev = nullptr;
				next = nullptr;
				prep = 0;
				start = 0;
				body = 0;
				end = 0;
				out = 0;
			}

		};
		typedef struct LoopItem_ LoopItem;

		struct Function_ {
			/* This is the Lua proto for the function */
			const Proto* f;
			/* program counter during symbolic interpretation */
			int pc;
			/* These act as the VM registers */
			std::string R[MAXARG_A];
			/* These store the priority for the operation that stored the value in each
			register */
			int Rprio[MAXARG_A];
			/* Boolean values indicating if register holds a table */
			int Rtabl[MAXARG_A];
			/* Registers standing for local variables. */
			int Rvar[MAXARG_A];
			/* Pending code to be flushed */
			int Rpend[MAXARG_A];
			/* Registers for internal use */
			int Rinternal[MAXARG_A];
			/* Registers used in call returns */
			int Rcall[MAXARG_A];
			/* 'a' of last CALL instruction -- used with 0-param CALLs */
			int lastCall;
			/* This is a list for creation of tables */
			List tables;
			/* State variables for the TEST instruction. */
			int testpending;
			int testjump;
			/* Pending variable-registers */
			List vpend;
			/* Pending temp-registers */
			IntSet* tpend;
			/* Loop Tree Structure */
			LoopItem* loop_tree;
			/* Pointer to Current Loop */
			LoopItem* loop_ptr;
			/* List for 'break' */
			List breaks;
			/* List for 'continue' */
			List continues;
			/* List of destination pc of JMP */
			List jmpdests;
			/* Control of do/end blocks. */
			IntSet* do_opens;
			IntSet* do_closes;
			int released_local;
			/* Skip for-variables on do-end check */
			int ignore_for_variables;
			int freeLocal;
			/* boolean operations */
			List bools;

			AstStatement* funcBlock;
			AstStatement* currStmt;
			int firstLine;
			int lastLine;
			/* holds the printed function */
			std::stringstream decompiledCode;
			/* indent */
			int indent;

			std::string funcnumstr;

			inline Function_()
			{
				f = nullptr;
				pc = 0;
				memset(Rprio, 0x0, sizeof(int) & MAXARG_A);
				memset(Rtabl, 0x0, sizeof(int) * MAXARG_A);
				memset(Rvar, 0x0, sizeof(int) * MAXARG_A);
				memset(Rpend, 0x0, sizeof(int) * MAXARG_A);
				memset(Rinternal, 0x0, sizeof(int) * MAXARG_A);
				memset(Rcall, 0x0, sizeof(int) * MAXARG_A);
				lastCall = 0;
				InitList(&tables);
				testpending = 0;
				testjump = 0;
				InitList(&vpend);
				tpend = nullptr;
				loop_tree = nullptr;
				loop_ptr = nullptr;
				InitList(&breaks);
				InitList(&continues);
				InitList(&jmpdests);
				do_opens = nullptr;
				do_closes = nullptr;
				released_local = 0;
				ignore_for_variables = 0;
				freeLocal = 0;
				InitList(&bools);
				funcBlock = nullptr;
				currStmt = nullptr;
				firstLine = 0;
				lastLine = 0;
				indent = 0;
			}
		};
		typedef struct Function_ Function;

		struct DecTableItem_ {
			ListItem super;
			std::string value;
			int index;
			std::string key;
		};
		typedef struct DecTableItem_ DecTableItem;

		struct DecTable_ {
			ListItem super;
			int reg;
			List array;
			int arraySize;
			List keyed;
			int keyedSize;
			int pc;
			inline DecTable_()
			{
				reg = 0;
				arraySize = 0;
				InitList(&array);
				InitList(&keyed);
				keyedSize = 0;
				pc = 0;
			}
		};
		typedef struct DecTable_ DecTable;

		struct Variable_ {
			ListItem super;
			char* name;
			int reg;
			inline Variable_()
			{
				name = nullptr;
				reg = 0;
			}
		};
		typedef struct Variable_ Variable;

		struct IntListItem_ {
			ListItem super;
			int value;
			inline IntListItem_()
			{
				value = 0;
			}
		};
		typedef struct IntListItem_ IntListItem;

		std::string GetR(GluaDecompileContextP ctx, Function* F, int r);

		struct LogicExp_ {
			LogicExp_* parent;
			LogicExp_* next;
			LogicExp_* prev;
			LogicExp_* subexp;
			int is_chain;
			std::string op1;
			std::string op2;
			OpCode op;
			int dest;
			int neg;
			inline LogicExp_()
			{
				parent = nullptr;
				next = nullptr;
				prev = nullptr;
				subexp = nullptr;
				is_chain = 0;
				dest = 0;
				neg = 0;
			}
		};
		typedef struct LogicExp_ LogicExp;

		LogicExp* MakeExpNode(BoolOp* boolOp);
		LogicExp* MakeExpChain(int dest);
		LogicExp* FindLogicExpTreeRoot(LogicExp* exp);
		void DeleteLogicExpSubTree(LogicExp* exp);
		void DeleteLogicExpTree(LogicExp* exp);
		void PrintLogicItem(std::stringstream &str, LogicExp* exp, int inv, int rev);
		void PrintLogicExp(std::stringstream &str, int dest, LogicExp* exp, int inv_, int rev_);

		void AddStatement(GluaDecompileContextP ctx, Function* F, std::stringstream &str);
		void ShowState(GluaDecompileContextP ctx, Function* F);

		enum IndexType_ {
			DOT = 0,
			SELF = 1,
			TABLE = 2,
			SQUARE_BRACKET = 3
		};
		typedef enum IndexType_ IndexType;
		IndexType MakeIndex(GluaDecompileContextP ctx, Function* F, std::stringstream &str, std::string rstr, IndexType type);
		int isIdentifier(std::string src);

		// @param lfag: 是否显示调试信息
		std::string luaU_decompile(GluaDecompileContextP ctx, Proto* f, int lflag);
		void luaU_decompileSubFunction(GluaDecompileContextP ctx, Proto* f, int dflag, std::string funcnumstr);

		BoolOp* NewBoolOp();
		BoolOp* MakeBoolOp(std::string op1, std::string op2, OpCode op, int neg, int pc, int dest);
		void ClearBoolOp(BoolOp* ptr, void* dummy);
		void DeleteBoolOp(BoolOp* ptr);

		void listParams(GluaDecompileContextP ctx, const Proto* f, std::stringstream &str);
		void listUpvalues(GluaDecompileContextP ctx, const Proto* f, std::stringstream &str);

		std::string ProcessCode(GluaDecompileContextP ctx, Proto* f, int indent, int func_checking, std::string funcnumstr);
		std::string ProcessSubFunction(GluaDecompileContextP ctx, Proto* cf, int func_checking, std::string funcnumstr);

		int FunctionCheck(GluaDecompileContextP ctx, Proto* f, std::string funcnumstr, std::stringstream &str);
		int CompareProto(const Proto* f1, const Proto* f2, std::stringstream &str);

		int printFileNames(FILE* out);

	}
}

#endif // #ifndef glua_decompile_h
