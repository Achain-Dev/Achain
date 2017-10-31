/* luadec, based on luac */
#include <glua/glua_common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <glua/lua.h>
#include <glua/lauxlib.h>
#include <glua/ldebug.h>
#include <glua/lobject.h>
#include <glua/lopcodes.h>
#include <glua/lundump.h>
#include <glua/lstring.h>

#include <glua/glua_compat.h>
#include <glua/glua_structs.h>
#include <glua/glua_proto_info.h>
#include <glua/glua_statement.h>
#include <glua/glua_decompile.h>

namespace glua {
	namespace decompile {

#define DEBUG_PRINT

#define stddebug stdout

#define SRCVERSION "d100ae7"

#ifndef LUA_DEBUG
#define luaB_opentests(L)
#endif

#ifndef PROGNAME
#define PROGNAME "GluaDec"	/* program name */
#endif

#define	OUTPUT "luadec.out"		/* default output file */

#define VERSION "1.0"

#define VERSION_STRING VERSION " rev: " SRCVERSION

// int debug = 0;					/* debug decompiler? */
// static char* funcnumstr = NULL;
// static int printfuncnum = 0;      /* print function nums? */
// static int dumping = 1;			/* dump bytecodes? */
// static int stripping = 0;			/* strip debug information? */
// static int disassemble = 0;		/* disassemble? */
// int locals = 0;					/* strip debug information? */
// int localdeclare[255][255];
// int functionnum;
// int process_sub = 1;            /* process sub functions? */
// int func_check = 0;				/* compile decompiled function and compare? */
int string_encoding = GBK;
// int guess_locals = 1;
// lua_State* glstate;
// Proto* glproto;
static int lds2 = 0;
char* LDS2;
static char Output[] = { OUTPUT };		/* default output file name */
static const char* output = Output;		/* output file name */
static const char* progname = PROGNAME;	/* actual program name */


static void clear_sstream(std::stringstream &ss)
{
	ss.str(std::string());
	ss.clear();
}


Proto* toproto(lua_State* L, int i) {
	auto c = (const Closure*) lua_topointer(L, i);
	return c->l.p;
}

#define	IS(s)	(strcmp(argv[i],s)==0)

int Inject(GluaDecompileContextP ctx, Proto * fp, int functionnum) {
	int f, i, c, n, at;
	char number[255] = "";
	for (f = 0; f<2; f++) {
		for (i = 0; i<255; i++) {
			ctx->localdeclare[f][i] = -1;
		}
	}
	f = 0;
	i = 0;
	c = 0;
	n = 0;
	at = 0;
	while (LDS2[c] != '\0') {
		switch (LDS2[c]) {
		case '-':
			if (n != 0) {
				if (f == functionnum) {
					ctx->localdeclare[at][i] = atoi(number);
				}
			}
			at = 1;
			n = 0;
			break;
		case ',':
			if (n != 0) {
				if (f == functionnum) {
					ctx->localdeclare[at][i] = atoi(number);
				}
			}
			i++;
			n = 0;
			at = 0;
			break;
		case ';':
			if (n != 0) {
				if (f == functionnum) {
					ctx->localdeclare[at][i] = atoi(number);
				}
			}
			i = 0;
			n = 0;
			at = 0;
			f++;
			break;
		default:
			number[n] = LDS2[c];
			n++;
			number[n] = '\0';
			break;
		}
		c++;
	}
	if (n != 0) {
		if (f == functionnum) {
			ctx->localdeclare[at][i] = atoi(number);
		}
	}

	fp->sizelocvars = 0;
	for (i = 0; i<255; i++) {
		if (ctx->localdeclare[0][i] != -1) {
			fp->sizelocvars++;
		}
	}

	if (fp->sizelocvars>0) {
		fp->locvars = luaM_newvector(ctx->glstate, fp->sizelocvars, LocVar);
		for (i = 0; i<fp->sizelocvars; i++) {
			std::string names;
			names = std::string("l_") + std::to_string(functionnum) + "_" + std::to_string(i + fp->numparams);
			fp->locvars[i].varname = luaS_new(ctx->glstate, names.c_str());
			fp->locvars[i].startpc = ctx->localdeclare[0][i];
			fp->locvars[i].endpc = ctx->localdeclare[1][i];
		}
	}

	for (f = 0; f<2; f++) {
		for (i = 0; i<255; i++) {
			ctx->localdeclare[f][i] = -1;
		}
	}

	return 1;
}

int LocalsLoad(GluaDecompileContextP ctx, const char* text) {
	int f, i, c, n;
	char number[255] = "";
	if (!text || *text == '\0') {
		return 0;
	}
	for (f = 0; f<255; f++) {
		for (i = 0; i<255; i++) {
			ctx->localdeclare[f][i] = -1;
		}
	}
	f = 0;
	i = 0;
	c = 0;
	n = 0;
	while (text[c] != '\0') {
		switch (text[c]) {
		case ',':
			if (n != 0) {
				ctx->localdeclare[f][i] = atoi(number);
			}
			i++;
			n = 0;
			break;
		case ';':
			if (n != 0) {
				ctx->localdeclare[f][i] = atoi(number);
			}
			i = 0;
			n = 0;
			f++;
			break;
		default:
			number[n] = text[c];
			n++;
			number[n] = '\0';
			break;
		}
		c++;
	}
	if (n != 0) {
		ctx->localdeclare[f][i] = atoi(number);
	}

	return 1;
}

// extern int locals;
// extern int localdeclare[255][255];
// extern int functionnum;
// extern int process_sub;           /* process sub functions? */
// extern int func_check;            /* compile decompiled function and compare */
// extern int guess_locals;
// extern lua_State* glstate;
// extern Proto* glproto;

// char unknown_local[] = { "ERROR_unknown_local_xxxx" };
// char unknown_upvalue[] = { "ERROR_unknown_upvalue_xxxx" };
// StringBuffer* errorStr;
// char errortmp[256];

/*
* -------------------------------------------------------------------------
*/

void FixLocalNames(GluaDecompileContextP ctx, Proto* f, std::string funcnumstr) {
	int i;
	std::string tmpname;
	int need_arg = NEED_ARG(f);
	int func_endpc = FUNC_BLOCK_END(f);

	if (f->sizelocvars < f->numparams + need_arg) {
		f->locvars = luaM_reallocvector(ctx->glstate, f->locvars, f->sizelocvars, f->numparams + need_arg, LocVar);
		for (i = 0; i < f->numparams; i++) {
			tmpname = std::string("p_") + funcnumstr + "_" + std::to_string(i);
			f->locvars[i].varname = luaS_new(ctx->glstate, tmpname.c_str());
			f->locvars[i].startpc = 0;
			f->locvars[i].endpc = func_endpc;
		}
		if (need_arg) {
			tmpname = "arg";
			f->locvars[i].varname = luaS_new(ctx->glstate, tmpname.c_str());
			f->locvars[i].startpc = 0;
			f->locvars[i].endpc = func_endpc;
		}
		f->sizelocvars = f->numparams + need_arg;
	}
	for (i = 0; i < f->sizelocvars; i++) {
		TString* name = f->locvars[i].varname;
		if (name == nullptr || LUA_STRLEN(name) == 0 ||
			strlen(getstr(name)) == 0 || !isIdentifier(getstr(name))) {
			tmpname = std::string("l_") + funcnumstr + "_" + std::to_string(i);
			name = luaS_new(ctx->glstate, tmpname.c_str());
			f->locvars[i].varname = name;
		}
	}
}

// i : index of Proto.locvars, not reg number
std::string getLocalName(GluaDecompileContextP ctx, const Proto* f, int i) {
	if (f->locvars && i < f->sizelocvars) {
		// no need to test after FixLocalNames
		return getstr(f->locvars[i].varname);
	}
	else {
		ctx->unknown_local = std::string("ERROR_unknown_local_") + std::to_string(i);
		return ctx->unknown_local;
	}
}

// i : index of Proto.upvalues
std::string getUpvalName(GluaDecompileContextP ctx, const Proto* f, int i) {
	if (f->upvalues && i < f->sizeupvalues) {
		// no need to test after FixUpvalNames
		return getstr(UPVAL_NAME(f, i));
	}
	else {
		ctx->unknown_upvalue = std::string("ERROR_unknown_upvalue_") + std::to_string(i);
		return ctx->unknown_upvalue;
	}
}

#define UPVALUE(ctx, i) (getUpvalName(ctx, F->f,i))
#define LOCAL(ctx, i) (getLocalName(ctx, F->f,i))
#define REGISTER(r) F->R[r]
#define PRIORITY(r) (r>=MAXSTACK ? 0 : F->Rprio[r])
#define PENDING(r) F->Rpend[r]
#define CALL(r) F->Rcall[r]
#define IS_TABLE(r) F->Rtabl[r]
#define IS_VARIABLE(r) F->Rvar[r]

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#define SET_ERROR(ctx, F,e) { ctx->error_ss << "-- DECOMPILER ERROR at PC" << (F)->pc << ": " << (e) << "\n"; RawAddStatement((F), ctx->error_ss); }
/*  error = e; errorCode = __LINE__; */ /*if (debug) { printf("DECOMPILER ERROR: %s\n", e);  }*/

// extern int debug;

// static char* error;
// static int errorCode;

int GetJmpAddr(Function* F, int addr) {
	int real_end = addr;
	if (real_end >= F->f->sizecode) {
		real_end = F->f->sizecode;
		return real_end;
	}
	if (real_end < 0) {
		real_end = -1;
		return real_end;
	}
	while (GET_OPCODE(F->f->code[real_end]) == OP_JMP) {
		real_end = GETARG_sBx(F->f->code[real_end]) + real_end + 1;
	}
	return real_end;
}

void RawAddStatement(Function* F, std::stringstream &ss);
void DeclareLocal(GluaDecompileContextP ctx, Function* F, int ixx, std::string value);

LoopItem* NewLoopItem(StatementType type, int prep, int start, int body, int end, int out) {
	auto self = new LoopItem();

	self->parent = nullptr;
	self->child = nullptr;
	self->prev = nullptr;
	self->next = nullptr;

	self->type = type;
	self->prep = prep;
	self->start = start;
	self->body = body;
	self->end = end;
	self->out = out;

	self->indent = 0;
	self->block = MakeBlockStatement(type, "");

	return self;
}

int MatchLoopItem(LoopItem* item, LoopItem* match) {
	return ((item->type == match->type) || (match->type == INT_MIN))
		&& ((item->prep == match->prep) || (match->prep == INT_MIN))
		&& ((item->start == match->start) || (match->start == INT_MIN))
		&& ((item->body == match->body) || (match->body == INT_MIN))
		&& ((item->end == match->end) || (match->end == INT_MIN))
		&& ((item->out == match->out) || (match->out == INT_MIN));
}

int AddToLoopTree(Function* F, LoopItem* item) {
	while (F->loop_ptr) {
		if (item->start >= F->loop_ptr->start && item->end < F->loop_ptr->end) {
			//find parent , then insert as the first child
			item->parent = F->loop_ptr;
			item->next = F->loop_ptr->child;
			item->prev = nullptr;
			item->child = nullptr;
			item->indent = F->loop_ptr->indent + 1;

			if (F->loop_ptr->child) {
				F->loop_ptr->child->prev = item;
			}
			F->loop_ptr->child = item;
			F->loop_ptr = item;
			return 1;
		}
		else {
			F->loop_ptr = F->loop_ptr->parent;
		}
	}
	return 0;
}

void DeleteLoopTree(LoopItem* item) {
	if (!item) return;
	DeleteLoopTree(item->child);
	DeleteLoopTree(item->next);
	delete item;
}

IntListItem* NewIntListItem(int v) {
	auto self = new IntListItem();
	self->super.prev = nullptr;
	self->super.next = nullptr;
	self->value = v;
	return self;
}

int MatchIntListItem(IntListItem* item, int* match_value) {
	return (item->value == *match_value);
}

void DeleteIntListItem(IntListItem* item, void* dummy) {
	delete item;
}

LogicExp* MakeExpNode(BoolOp* boolOp) {
	auto node = new LogicExp();
	node->parent = nullptr;
	node->subexp = nullptr;
	node->next = nullptr;
	node->prev = nullptr;
	node->op1 = boolOp->op1;
	node->op2 = boolOp->op2;
	node->op = boolOp->op;
	node->dest = boolOp->dest;
	node->neg = boolOp->neg;
	node->is_chain = 0;
	return node;
}

LogicExp* MakeExpChain(int dest) {
	auto node = new LogicExp();
	node->parent = nullptr;
	node->subexp = nullptr;
	node->next = nullptr;
	node->prev = nullptr;
	node->op1 = "";
	node->op2 = "";
	node->neg = 0;
	node->dest = dest;
	node->is_chain = 1;
	return node;
}

LogicExp* FindLogicExpTreeRoot(LogicExp* exp) {
	LogicExp* curr = exp;
	while (curr->parent) {
		curr = curr->parent;
	}
	return curr;
}

void DeleteLogicExpSubTree(LogicExp* exp) {
	if (exp) {
		DeleteLogicExpSubTree(exp->subexp);
		DeleteLogicExpSubTree(exp->next);
		exp->op1.clear();
		exp->op2.clear();
		delete exp;
	}
}

void DeleteLogicExpTree(LogicExp* exp) {
	if (exp) {
		LogicExp* root = FindLogicExpTreeRoot(exp);
		DeleteLogicExpSubTree(root);
	}
}

void PrintLogicItem(std::stringstream &str, LogicExp* exp, int inv, int rev) {
	if (exp->subexp) {
		str << "(";
		PrintLogicExp(str, exp->dest, exp->subexp, inv, rev);
		str << ")";
	}
	else {
		char* op;
		int cond = exp->neg;
		if (inv) cond = !cond;
		if (rev) cond = !cond;
		if (cond) {
			op = (char*)invopstr(exp->op);
		}
		else {
			op = (char*)opstr(exp->op);
		}
		if ((exp->op != OP_TEST) && (exp->op != OP_TESTSET)) {
			str << exp->op1 << " " << op << " " << exp->op2;
		}
		else if (op) {
			str << op << " " << exp->op2;
		}
		else {
			str << exp->op2;
		}
	}
}

void PrintLogicExp(std::stringstream &str, int dest, LogicExp* exp, int inv_, int rev) {
	int inv = inv_;
	while (exp->next) {
		char* op;
		int cond = exp->dest > dest;
		inv = cond ? inv_ : !inv_;
		PrintLogicItem(str, exp, inv, rev);
		exp = exp->next;
		if (inv_) cond = !cond;
		if (rev) cond = !cond;
		op = (char*)(cond ? "and" : "or");
		str << " " << op << " ";
	}
	PrintLogicItem(str, exp, inv_, rev);
}

void TieAsNext(LogicExp* curr, LogicExp* item) {
	curr->next = item;
	item->prev = curr;
	item->parent = curr->parent;
}

void Untie(LogicExp* curr, int* thenaddr) {
	LogicExp* previous = curr->prev;
	if (previous) {
		previous->next = nullptr;
	}
	curr->prev = nullptr;
	curr->parent = nullptr;
}


void TieAsSubExp(LogicExp* parent, LogicExp* item) {
	parent->subexp = item;
	while (item) {
		item->parent = parent;
		item = item->next;
	}
}

LogicExp* MakeBoolean(GluaDecompileContextP ctx, Function* F, int* thenaddr, int* endif) {
	int i;
	int firstaddr, elseaddr;
	BoolOp *first, *realLast, *last, *tmpLast, *curr;
	int lastCount;
	LogicExp *currExp = nullptr, *firstExp = nullptr;
	int dest;

	if (endif) {
		*endif = 0;
	}

	if (F->bools.size == 0) {
		SET_ERROR(ctx, F, "Attempted to build a boolean expression without a pending context");
		return nullptr;
	}

	first = (BoolOp*)FirstItem(&(F->bools));
	realLast = (BoolOp*)LastItem(&(F->bools));
	last = realLast;
	firstaddr = first->pc + 2;
	*thenaddr = last->pc + 2;
	elseaddr = last->dest;

	for (curr = realLast; curr; curr = lua_cast(BoolOp*, curr->super.prev)) {
		int dest = curr->dest;
		if ((elseaddr > *thenaddr) &&
			(((curr->op == OP_TEST) || (curr->op == OP_TESTSET)) ? (dest > elseaddr + 1) :
			(dest > elseaddr))) {
			last = curr;
			*thenaddr = curr->pc + 2;
			elseaddr = dest;
		}
	}

	tmpLast = last;
	for (curr = first; curr && curr != tmpLast; curr = lua_cast(BoolOp*, curr->super.next)) {
		int dest = curr->dest;
		if (elseaddr > firstaddr) {
			if (dest < firstaddr) {
				last = curr;
				*thenaddr = curr->pc + 2;
				elseaddr = dest;
			}
		}
		else {
			if (dest == firstaddr) {
				last = curr;
				*thenaddr = curr->pc + 2;
				elseaddr = dest;
			}
			else {
				break;
			}
		}
	}

	dest = first->dest;
	currExp = MakeExpNode(first);

	if (dest > firstaddr && dest <= *thenaddr) {
		firstExp = MakeExpChain(dest);
		TieAsSubExp(firstExp, currExp);
	}
	else {
		firstExp = currExp;
		if (endif) {
			*endif = dest;
		}
	}

	if (ctx->debug) {
		printf("\n");
		for (curr = first, i = 0; curr && curr != lua_cast(BoolOp*, last->super.next); i++, curr = lua_cast(BoolOp*, curr->super.next)) {
			BoolOp* op = curr;
			if (ctx->debug) {
				printf("Exps(%d): at %d\tdest %d\tneg %d\t(%s %s %s) cpd %d \n", i,
					op->pc, op->dest, op->neg, op->op1.c_str(), opstr(op->op),
					op->op2.c_str(), currExp->parent ? currExp->parent->dest : -1);
			}
		}
		printf("\n");
	}

	for (curr = lua_cast(BoolOp*, first->super.next), lastCount = 1; curr && curr != lua_cast(BoolOp*, last->super.next); curr = lua_cast(BoolOp*, curr->super.next), lastCount++) {
		BoolOp* op = curr;
		int at = op->pc;
		int dest = op->dest;

		LogicExp* exp = MakeExpNode(op);
		if (dest < firstaddr) {
			/* jump to loop in a while */
			TieAsNext(currExp, exp);
			currExp = exp;
			if (endif)
				*endif = dest;
		}
		else if (dest > *thenaddr) {
			/* jump to "else" */
			TieAsNext(currExp, exp);
			currExp = exp;
			if (endif) {
				if ((op->op != OP_TEST) && (op->op != OP_TESTSET)) {
					if (*endif != 0 && *endif != dest) {
						SET_ERROR(ctx, F, "Unhandled construct in 'MakeBoolean' P1");
						//return NULL;
					}
				}
				*endif = dest;
			}
		}
		else if (dest == currExp->dest) {
			/* within current chain */
			TieAsNext(currExp, exp);
			currExp = exp;
		}
		else if (dest > currExp->dest) {
			if (currExp->parent == nullptr || dest < currExp->parent->dest) {
				/* creating a new level */
				LogicExp* subexp = MakeExpChain(dest);
				LogicExp* savecurr;
				TieAsNext(currExp, exp);
				currExp = exp;
				savecurr = currExp;
				if (currExp->parent == nullptr) {
					TieAsSubExp(subexp, firstExp);
					firstExp = subexp;
				}
			}
			else if (dest > currExp->parent->dest) {
				/* start a new chain */
				LogicExp* prevParent;
				LogicExp* chain;
				TieAsNext(currExp, exp);
				currExp = currExp->parent;
				if (!currExp->is_chain) {
					DeleteLogicExpTree(firstExp);
					SET_ERROR(ctx, F, "Unhandled construct in 'MakeBoolean' P2");
					return nullptr;
				};
				prevParent = currExp->parent;
				chain = MakeExpChain(dest);
				Untie(currExp, thenaddr);
				if (prevParent && prevParent->is_chain) {
					prevParent = prevParent->subexp;
				}
				TieAsSubExp(chain, currExp);

				//currExp->parent = prevParent; // WHY use this line cause a memory leak, but output is better
				if (!prevParent) {
					firstExp = chain;
				}
				else {
					// todo
					TieAsNext(prevParent, chain);
				}
			}
			else {
				SET_ERROR(ctx, F, "Unhandled construct in 'MakeBoolean' P3");
				DeleteLogicExpSubTree(exp);
			}
		}
		else if (dest > firstaddr && dest < currExp->dest) {
			/* start a new chain */
			LogicExp* subexp = MakeExpChain(dest);
			TieAsSubExp(subexp, exp);
			TieAsNext(currExp, subexp);
			currExp = exp;
		}
		else {
			DeleteLogicExpSubTree(exp);
			DeleteLogicExpTree(firstExp);
			SET_ERROR(ctx, F, "Unhandled construct in 'MakeBoolean' P4");
			return nullptr;
		}

		if (currExp->parent && at + 3 > currExp->parent->dest) {
			currExp->parent->dest = currExp->dest;
			if (curr != last) {
				LogicExp* chain = MakeExpChain(currExp->dest);
				TieAsSubExp(chain, firstExp);
				firstExp = chain;
			}
			currExp = currExp->parent;
		}
	}
	if (firstExp->is_chain) {
		firstExp = firstExp->subexp;
	}
	if (last) {
		if (F->bools.tail == (ListItem*)last) {
			F->bools.head = nullptr;
			F->bools.tail = nullptr;
			F->bools.size = 0;
		}
		else {
			F->bools.head = last->super.next;
			F->bools.head->prev = nullptr;
			F->bools.size -= lastCount;
		}

	}
	curr = last;
	while (curr) {
		BoolOp* prev = lua_cast(BoolOp*, curr->super.prev);
		DeleteBoolOp(curr);
		curr = prev;
	}
	if (endif && *endif == 0) {
		*endif = *thenaddr;
	}
	return firstExp;
}

std::string WriteBoolean(LogicExp* exp, int* thenaddr, int* endif, int test) {
	std::string result;
	std::stringstream str;

	if (exp) {
		PrintLogicExp(str, *thenaddr, exp, 0, test);
		if (test && endif && *endif == 0) {
			//SET_ERROR(F, "Unhandled construct in 'WriteBoolean'");
			// result = (char*)calloc(30, sizeof(char));
			// sprintf(result, "__UNHANDLEDCONTRUCT_1__");
			result = "__UNHANDLEDCONTRUCT_1__";
			goto WriteBoolean_CLEAR_HANDLER1;
		}
	}
	else {
		// result = (char*)calloc(30, sizeof(char));
		// sprintf(result, "__UNHANDLEDCONTRUCT_2__");
		result = "__UNHANDLEDCONTRUCT_2__";
		goto WriteBoolean_CLEAR_HANDLER1;
	}
	result = str.str();

WriteBoolean_CLEAR_HANDLER1:

	return result;
}

std::string OutputBoolean(GluaDecompileContextP ctx, Function* F, int* thenaddr, int* endif, int test) {
	std::string result;
	LogicExp* exp = nullptr;
	ctx->error.clear();

	exp = MakeBoolean(ctx, F, thenaddr, endif);
	if (ctx->error.length()>0) goto OutputBoolean_CLEAR_HANDLER1;
	result = WriteBoolean(exp, thenaddr, endif, test);
	if (ctx->error.length()>0) goto OutputBoolean_CLEAR_HANDLER1;

OutputBoolean_CLEAR_HANDLER1:
	if (exp) DeleteLogicExpTree(exp);

	return result;
}

void RawAddAstStatement(Function* F, AstStatement* stmt) {
	if (F->released_local) {
		AstStatement* block = F->currStmt;
		AstStatement* curr = lua_cast(AstStatement*, block->sub->head);
		AstStatement* tail = lua_cast(AstStatement*, block->sub->tail);
		AstStatement* prev = nullptr;
		int count = 0;
		int lpc = F->released_local;
		F->released_local = 0;
		while (curr) {
			if (curr->line >= lpc) {
				break;
			}
			prev = curr;
			curr = lua_cast(AstStatement*, prev->super.next);
			count++;
		}
		if (curr) {
			//TODO check list.size
			int blockSize = block->sub->size;

			AstStatement* dostmt = MakeBlockStatement(DO_STMT, nullptr);
			dostmt->line = lpc;

			while (curr) {
				AstStatement* next = lua_cast(AstStatement*, curr->super.next);
				RemoveFromList(block->sub, lua_cast(ListItem*, curr));
				AddToStatement(dostmt, curr);
				curr = next;
			}
			AddToStatement(block, dostmt);
			F->currStmt = dostmt;
		}
	}
	stmt->line = F->pc;
	AddToStatement(F->currStmt, stmt);
	F->lastLine = F->pc;
}

void RawAddStatement(Function* F, std::stringstream &ss) {
	AstStatement* stmt = MakeSimpleStatement(ss.str());
	stmt->line = F->pc;
	RawAddAstStatement(F, stmt);
}

void FlushWhile1(Function* F) {
	LoopItem* walk = F->loop_ptr;
	std::stringstream str;

	if (walk->type == WHILE_STMT && walk->start <= F->pc && walk->body == -1) {
		AstStatement* whilestmt = walk->block;
		whilestmt->code = std::string("1");
		RawAddAstStatement(F, whilestmt);
		F->currStmt = whilestmt;
		walk->body = walk->start;
		walk = walk->parent;
	}
}

void FlushBoolean(GluaDecompileContextP ctx, Function* F) {
	if (F->bools.size == 0) {
		FlushWhile1(F);
	}
	while (F->bools.size > 0) {
		AstStatement* whilestmt = nullptr;
		int endif = 0, thenaddr = 0;
		std::string test;
		std::stringstream str;
		LoopItem* walk = nullptr;

		test = OutputBoolean(ctx, F, &thenaddr, &endif, 0);
		if (ctx->error.length()>0) goto FlushBoolean_CLEAR_HANDLER1;

		//TODO find another method to determine while loop body to output while do
		//search parent
		walk = F->loop_ptr;
		if (walk->type == WHILE_STMT && walk->out == endif - 1 && walk->body == -1) {
			int whileStart = walk->start;
			walk->body = thenaddr;
			whilestmt = walk->block;
		}

		if (whilestmt) {
			whilestmt->code = test;
			test = "";
			RawAddAstStatement(F, whilestmt);
			F->currStmt = whilestmt;
		}
		else {
			AstStatement* ifstmt = nullptr;
			FlushWhile1(F);
			ifstmt = MakeIfStatement(test);
			ThenStart(ifstmt) = thenaddr;
			ElseStart(ifstmt) = endif - 1;
			test = "";
			RawAddAstStatement(F, ifstmt);
			F->currStmt = ThenStmt(ifstmt);
		}

	FlushBoolean_CLEAR_HANDLER1:
		if (ctx->error.length()>0) return;
	}
	F->testpending = 0;
}

void DeclareLocalsAddStatement(GluaDecompileContextP ctx, Function* F, std::stringstream &statement) {
	if (F->pc > 0) {
		FlushBoolean(ctx, F);
		if (ctx->error.length()>0) return;
	}
	RawAddStatement(F, statement);
}

void AddStatement(GluaDecompileContextP ctx, Function* F, std::stringstream &statement) {
	FlushBoolean(ctx, F);
	if (ctx->error.length()>0) return;

	RawAddStatement(F, statement);
}

void AddAstStatement(GluaDecompileContextP ctx, Function* F, AstStatement* stmt) {
	FlushBoolean(ctx, F);
	if (ctx->error.length()>0) return;

	RawAddAstStatement(F, stmt);
}

/*
* -------------------------------------------------------------------------
*/

void DeclarePendingLocals(GluaDecompileContextP ctx, Function* F);

void AssignGlobalOrUpvalue(Function* F, std::string dest, std::string src) {
	F->testjump = 0;
	AddToVarList(&(F->vpend), dest, src, -1);
}

void AssignReg(GluaDecompileContextP ctx, Function* F, int reg, std::string src, int prio, int mayTest) {
	std::string dest = REGISTER(reg);
	std::string nsrc;

	if (PENDING(reg)) {
		if (ctx->guess_locals) {
			ctx->errortmp = std::string("Overwrote pending register: R") + std::to_string(reg) + " in 'AssignReg'";
			SET_ERROR(ctx, F, ctx->errortmp);
		}
		else {
			ctx->errortmp = std::string("Overwrote pending register: R") + std::to_string(reg) + " in 'AssignReg'. Creating missing local";
			SET_ERROR(ctx, F, ctx->errortmp);
			DeclareLocal(ctx, F, reg, dest);
		}
		return;
	}

	PENDING(reg) = 1;
	CALL(reg) = 0;
	F->Rprio[reg] = prio;

	if (ctx->debug) {
		printf("SET_SIZE(tpend) = %d \n", SET_SIZE(F->tpend));
	}

	nsrc = std::string(src);
	if (F->testpending == reg + 1 && mayTest && F->testjump == F->pc + 2) {
		int thenaddr, endif;
		std::string test = OutputBoolean(ctx, F, &thenaddr, &endif, 1);
		if (ctx->error.length()>0) {
			return;
		}
		if (endif >= F->pc) {
			std::stringstream str;
			str << test << " or " << src;
			nsrc = str.str();
			F->testpending = 0;
			F->Rprio[reg] = 8;
		}
	}
	F->testjump = 0;

	if (!IS_VARIABLE(reg)) {
		REGISTER(reg) = nsrc;
		AddToSet(F->tpend, reg);
	}
	else {
		AddToVarList(&(F->vpend), dest, nsrc, reg);
	}
}

/*
** Table Functions
*/

DecTableItem* NewTableItem(std::string value, int index, std::string key) {
	auto self = new DecTableItem();
	self->super.prev = nullptr;
	self->super.next = nullptr;
	self->value = value;
	self->index = index;
	self->key = std::string(key);
	return self;
}

static char string_last_char(std::string const &str)
{
	return str.length() > 0 ? str[str.length() - 1] : -1;
}

void ClearTableItem(DecTableItem* item, void* dummy) {
	if (item) {
		item->key.clear();
		item->value.clear();
	}
}

void DeleteTableItem(DecTableItem* item) {
	ClearTableItem(item, nullptr);
	delete item;
}

int MatchTable(DecTable* tbl, int* reg) {
	return tbl->reg == *reg;
}

void DeleteTable(DecTable* tbl) {
	ClearList(&(tbl->keyed), (ListItemFn)ClearTableItem);
	ClearList(&(tbl->array), (ListItemFn)ClearTableItem);
	delete tbl;
}

void CloseTable(Function* F, int r) {
	DecTable* tbl = (DecTable*)RemoveFromList(&(F->tables), FindFromListTail(&(F->tables), (ListItemCmpFn)MatchTable, &r));
	DeleteTable(tbl);
	IS_TABLE(r) = 0;
}

void PrintTableItemNumeric(std::stringstream &str, DecTableItem* item) {
	const auto &value = item->value;
	if (value[0] == '{' && string_last_char(str.str()) != '\n') {
		str << "\n";
	}
	str << value;
	if (value[value.length() - 1] == '}') {
		str << "\n";
	}
}

void PrintTableItemKeyed(GluaDecompileContextP ctx, Function* F, std::stringstream &str, DecTableItem* item) {
	const auto &value = item->value;
	if (value[0] == '{' && string_last_char(str.str()) != '\n') {
		str << "\n";
	}
	MakeIndex(ctx, F, str, item->key, TABLE);
	str << " = " << item->value;
	if (value[value.length() - 1] == '}') {
		str << "\n";
	}
}

std::string PrintTable(GluaDecompileContextP ctx, Function* F, int r, int returnCopy) {
	std::string result;
	int numerics = 0;
	DecTableItem* item;
	std::stringstream str;
	DecTable* tbl = (DecTable*)FindFromListTail(&(F->tables), (ListItemCmpFn)MatchTable, &r);
	if (tbl == nullptr) {
		IS_TABLE(r) = 0;
		return F->R[r];
	}
	item = lua_cast(DecTableItem*, tbl->array.head);
	if (item) {
		numerics = 1;
		PrintTableItemNumeric(str, item);
		item = lua_cast(DecTableItem*, item->super.next);
		while (item) {
			str << ", ";
			PrintTableItemNumeric(str, item);
			item = lua_cast(DecTableItem*, item->super.next);
		}
	}
	item = lua_cast(DecTableItem*, tbl->keyed.head);
	if (item) {
		if (numerics) {
			str << "; ";
		}
		PrintTableItemKeyed(ctx, F, str, item);
		item = lua_cast(DecTableItem*, item->super.next);
		while (item) {
			str << ", ";
			PrintTableItemKeyed(ctx, F, str, item);
			item = lua_cast(DecTableItem*, item->super.next);
		}
	}
	str << "}";
	PENDING(r) = 0;
	AssignReg(ctx, F, r, str.str().c_str(), 0, 0);
	if (ctx->error.length()>0) {
		result = "";
	}
	else if (returnCopy) {
		result = str.str();
	}
	CloseTable(F, r);
	return result;
}

DecTable* NewTable(int r, int b, int c, int pc) {
	auto self = new DecTable();
	self->super.prev = nullptr;
	self->super.next = nullptr;
	InitList(&(self->array));
	InitList(&(self->keyed));
	self->reg = r;
	self->arraySize = luaO_fb2int(b);
	self->keyedSize = luaO_fb2int(c);
	self->pc = pc;
	return self;
}

void StartTable(Function* F, int r, int b, int c, int pc) {
	auto tbl = NewTable(r, b, c, pc);
	AddToListHead(&(F->tables), (ListItem*)tbl);
	PENDING(r) = 1;
	IS_TABLE(r) = 1;
}

void AddToTable(Function* F, DecTable* tbl, std::string value, std::string key) {
	DecTableItem* item;
	List* list;
	if (key.length()<1) {
		list = &(tbl->array);
		item = NewTableItem(value, list->size, "");
	}
	else {
		list = &(tbl->keyed);
		item = NewTableItem(value, 0, key);
	}
	AddToList(list, (ListItem*)item);
}

void SetList(GluaDecompileContextP ctx, Function* F, int a, int b, int c) {
	int i;
	DecTable* tbl = (DecTable*)FindFromListTail(&(F->tables), (ListItemCmpFn)MatchTable, &a);
	if (!tbl) {
		ctx->errortmp = std::string("No list found for R") + std::to_string(a) + " , SetList fails";
		SET_ERROR(ctx, F, ctx->errortmp);
		return;
	}
	if (b == 0) {
		std::string rstr;
		i = 1;
		while (1) {
			rstr = GetR(ctx, F, a + i);
			if (ctx->error.length()>0)
				return;
			if (strcmp(rstr.c_str(), ".end") == 0)
				break;
			AddToTable(F, tbl, rstr, nullptr); // Lua5.1 specific TODO: it's not really this :(
			i++;
		};
	} //should be {...} or func(func()) ,when b == 0, that will use all avaliable reg from R(a)

	for (i = 1; i <= b; i++) {
		auto rstr = GetR(ctx, F, a + i);
		if (ctx->error.length()>0)
			return;
		AddToTable(F, tbl, rstr, nullptr); // Lua5.1 specific TODO: it's not really this :(
	}
}

void UnsetPending(GluaDecompileContextP ctx, Function* F, int r) {
	if (!IS_VARIABLE(r)) {
		if (!PENDING(r) && !CALL(r)) {
			if (ctx->guess_locals) {
				ctx->errortmp = std::string("Confused about usage of register: R") + std::to_string(r) + " in 'UnsetPending'";
				SET_ERROR(ctx, F, ctx->errortmp);
			}
			else {
				ctx->errortmp = std::string("Confused about usage of register: R") + std::to_string(r) + " in 'UnsetPending'. Creating missing local";
				SET_ERROR(ctx, F, ctx->errortmp);
				DeclareLocal(ctx, F, r, REGISTER(r));
			}
			return;
		}
		PENDING(r) = 0;
		RemoveFromSet(F->tpend, r);
	}
}

int SetTable(GluaDecompileContextP ctx, Function* F, int a, std::string bstr, std::string cstr) {
	DecTable* tbl = (DecTable*)FindFromListTail(&(F->tables), (ListItemCmpFn)MatchTable, &a);
	if (!tbl) {
		UnsetPending(ctx, F, a);
		return 0;
	}
	AddToTable(F, tbl, cstr, bstr);
	return 1;
}

/*
** BoolOp Functions
*/

BoolOp* NewBoolOp() {
	auto value = new BoolOp();
	value->super.prev = nullptr;
	value->super.next = nullptr;
	value->op1 = "";
	value->op2 = "";
	return value;
}

BoolOp* MakeBoolOp(std::string op1, std::string op2, OpCode op, int neg, int pc, int dest) {
	auto value = new BoolOp();
	value->super.prev = nullptr;;
	value->super.next = nullptr;
	value->op1 = op1;
	value->op2 = op2;
	value->op = op;
	value->neg = neg;
	value->pc = pc;
	value->dest = dest;
	return value;
}

void ClearBoolOp(BoolOp* ptr, void* dummy) {
	if (ptr) {
		ptr->op1.clear();
		ptr->op2.clear();
	}
}

void DeleteBoolOp(BoolOp* ptr) {
	ClearBoolOp(ptr, nullptr);
	delete ptr;
}

/*
** -------------------------------------------------------------------------
*/

Function* NewFunction(const Proto* f) {
	auto self = new Function();
	self->f = f;
	memset(&(self->tables), 0x0, sizeof(List));
	InitList(&(self->vpend));
	self->tpend = NewIntSet(0);

	self->loop_tree = NewLoopItem(FUNCTION_STMT, -1, 0, 0, f->sizecode - 1, f->sizecode);
	self->loop_ptr = self->loop_tree;

	self->funcBlock = self->loop_tree->block;
	self->currStmt = self->funcBlock;

	self->loop_tree->block = self->funcBlock;

	InitList(&(self->breaks));
	InitList(&(self->continues));
	InitList(&(self->jmpdests));

	self->do_opens = NewIntSet(0);
	self->do_closes = NewIntSet(0);
	clear_sstream(self->decompiledCode);

	InitList(&(self->bools));

	self->funcnumstr = "";

	return self;
}

void DeleteFunction(Function* self) {
	int i;
	DeleteAstStatement(self->funcBlock);
	ClearList(&(self->bools), (ListItemFn)ClearBoolOp);
	/*
	* clean up registers
	*/
	for (i = 0; i < MAXARG_A; i++) {
		if (self->R[i].length()>0) {
			self->R[i].clear();
		}
	}
	ClearList(&(self->vpend), (ListItemFn)ClearVarListItem);
	DeleteIntSet(self->tpend);
	ClearList(&(self->breaks), nullptr);
	ClearList(&(self->continues), nullptr);
	ClearList(&(self->jmpdests), (ListItemFn)ClearAstStatement);
	DeleteLoopTree(self->loop_tree);
	DeleteIntSet(self->do_opens);
	DeleteIntSet(self->do_closes);
	if (self->funcnumstr.length()>0) {
		self->funcnumstr.clear();
	}
	delete self;
}

void DeclareVariable(GluaDecompileContextP ctx, Function* F, std::string name, int reg);

std::string GetR(GluaDecompileContextP ctx, Function* F, int r) {
	if (IS_TABLE(r)) {
		PrintTable(ctx, F, r, 0);
		if (ctx->error.length()>0) return nullptr;
	}
	UnsetPending(ctx, F, r);
	if (ctx->error.length()>0) return nullptr;

	if (F->R[r].length()<1) {
		char sb[] = { "R%rrrrr%_PC%pcccccccc%" };
		sprintf(sb, "R%d_PC%d", r, F->pc);
		DeclareVariable(ctx, F, sb, r);
	}//dirty hack , some numeric FOR loops may cause error
	return F->R[r];
}

void DeclareVariable(GluaDecompileContextP ctx, Function* F, std::string name, int reg) {
	IS_VARIABLE(reg) = 1;
	if (F->R[reg].length()>0) {
		F->R[reg].clear();
	}
	F->R[reg] = name;
	F->Rprio[reg] = 0;
	UnsetPending(ctx, F, reg);
	if (ctx->error.length()>0) return;
}

void OutputAssignments(GluaDecompileContextP ctx, Function* F) {
	int i, srcs;
	ListItem *walk, *tail;
	std::stringstream vars;
	std::string exps;
	if (!SET_IS_EMPTY(F->tpend)) {
		goto OutputAssignments_ERROR_HANDLER;
	}
	srcs = 0;
	walk = F->vpend.head;
	tail = F->vpend.tail;
	i = 0;
	while (walk) {
		int r = lua_cast(VarListItem*, walk)->reg;
		auto src = lua_cast(VarListItem*, walk)->src;
		auto dest = lua_cast(VarListItem*, walk)->dest;
		if (!(r == -1 || PENDING(r))) {
			ctx->errortmp = std::string("Confused about usage of register: R") + std::to_string(r) + " in 'OutputAssignments'";
			SET_ERROR(ctx, F, ctx->errortmp);
			goto OutputAssignments_ERROR_HANDLER;
		}

		if (i > 0) {
			auto replaced = std::string(", ") + vars.str();
			clear_sstream(vars);
			vars << replaced;
		}
		auto replaced2 = std::string(dest) + vars.str();
		clear_sstream(vars);
		vars << replaced2;

		if (src.length()>0 && (srcs > 0 || (srcs == 0 && strcmp(src.c_str(), "nil") != 0) || walk == tail)) {
			if (srcs > 0) {
				exps = ", " + exps;
			}
			exps = src + exps;
			srcs++;
		}
		if (r != -1) {
			PENDING(r) = 0;
		}
		walk = walk->next;
		i++;
	}

	if (i > 0) {
		vars << " = ";
		vars << exps;
		AddStatement(ctx, F, vars);
		if (ctx->error.length()>0) {
			goto OutputAssignments_ERROR_HANDLER;
		}
	}
OutputAssignments_ERROR_HANDLER:
	ClearList(&(F->vpend), (ListItemFn)ClearVarListItem);
}

void ReleaseLocals(GluaDecompileContextP ctx, Function* F) {
	int i;
	for (i = F->f->sizelocvars - 1; i >= 0; i--) {
		if (F->f->locvars[i].endpc == F->pc) {
			int r;
			F->freeLocal--;
			if (F->freeLocal < 0) {
				F->freeLocal = 0;
				fprintf(stderr, "freeLocal<0 in void ReleaseLocals(Function* F)\n");
				fprintf(stderr, " at line %d in file %s\n", __LINE__, __FILE__);
				fprintf(stderr, " for lua files: ");
				printFileNames(stderr);
				fprintf(stderr, "\n");
				fprintf(stderr, " at lua function %s pc=%d\n\n", F->funcnumstr.c_str(), F->pc);
				fflush(stderr);
				SET_ERROR(ctx, F, "freeLocal<0 in 'ReleaseLocals'");
				return;
			}
			r = F->freeLocal;
			//fprintf(stderr,"%d %d %d\n",i,r, F->pc);
			if (!IS_VARIABLE(r)) {
				// fprintf(stderr,"--- %d %d\n",i,r);
				ctx->errortmp = std::string("Confused about usage of register R") + std::to_string(r) + " for local variables in 'ReleaseLocals'";
				SET_ERROR(ctx, F, ctx->errortmp);
				return;
			}
			IS_VARIABLE(r) = 0;
			F->Rprio[r] = 0;
			if (!F->ignore_for_variables && !F->released_local) {
				F->released_local = F->f->locvars[i].startpc;
			}
		}
	}
	F->ignore_for_variables = 0;
}

void DeclareLocals(GluaDecompileContextP ctx, Function* F) {
	int i;
	int locals;
	int internalLocals = 0;
	//int loopstart;
	//int loopvars;
	int loopconvert;
	std::stringstream str;
	std::stringstream rhs;
	std::string names[MAXARG_A];
	int startparams = 0;
	/*
	* Those are declaration of parameters.
	*/
	if (F->pc == 0) {
		startparams = F->f->numparams;
		if (NEED_ARG(F->f)) {
			startparams++;
		}
	}
	str << "local ";
	rhs << " = ";
	locals = 0;
	if (F->pc != 0) {
		for (i = startparams; i < F->f->maxstacksize; i++) {
			if (ctx->functionnum >= 0 && ctx->functionnum < 255 && ctx->localdeclare[ctx->functionnum][i] == F->pc) {
				int r = i;
				char name[12];
				memset(name, 0x0, sizeof(name));
				sprintf(name, "l_%d_%d", ctx->functionnum, i);
				name[11] = '\0';
				if (F->Rinternal[r]) {
					names[r] = std::string(name);
					F->Rinternal[r] = 0;
					internalLocals++;
					continue;
				}
				if (PENDING(r)) {
					if (locals > 0) {
						str << ", ";
						rhs << ", ";
					}
					str << name;
					rhs << GetR(ctx, F, r);
				}
				else {
					str << ", " << name;
				}
				CALL(r) = 0;
				IS_VARIABLE(r) = 1;
				names[r] = std::string(name);
				locals++;
			}
		}
	}
	loopconvert = 0;
	for (i = startparams; i < F->f->sizelocvars; i++) {
		if (F->f->locvars[i].startpc == F->pc) {
			int r = F->freeLocal + locals + internalLocals;
			Instruction instr = F->f->code[F->pc];
			// handle FOR loops
			if (GET_OPCODE(instr) == OP_FORPREP) {
				F->f->locvars[i].startpc = F->pc + 1;
				continue;
			}
			// handle TFOR loops
			if (GET_OPCODE(instr) == OP_JMP) {
				Instruction n2 = F->f->code[F->pc + 1 + GETARG_sBx(instr)];
				//fprintf(stderr,"3 %d\n",F->pc+1+GETARG_sBx(instr));
				//fprintf(stderr,"4 %s %d\n",luaP_opnames[GET_OPCODE(n2)], F->pc+GETARG_sBx(instr));
				if (GET_OPCODE(n2) == LUADEC_TFORLOOP) {
					F->f->locvars[i].startpc = F->pc + 1;
					continue;
				}
			}
			if ((F->Rinternal[r])) {
				names[r] = std::string(LOCAL(ctx, i));
				PENDING(r) = 0;
				IS_VARIABLE(r) = 1;
				F->Rinternal[r] = 0;
				internalLocals++;
				continue;
			}
			if (PENDING(r)) {
				if (locals > 0) {
					str << ", ";
					rhs << ", ";
				}
				str << LOCAL(ctx, i);
				rhs << GetR(ctx, F, r);
				if (ctx->error.length()>0) return;
			}
			else {
				if (locals > 0) {
					str << ", ";
				}
				str << LOCAL(ctx, i);
			}
			CALL(r) = 0;
			IS_VARIABLE(r) = 1;
			names[r] = std::string(LOCAL(ctx, i));
			locals++;
		}
	}
	if (locals > 0) {
		str << rhs.str();
		if (strcmp(rhs.str().c_str(), " = ") == 0) {
			str << "nil";
		}
		DeclareLocalsAddStatement(ctx, F, str);
		if (ctx->error.length()>0) return;
	}
	for (i = 0; i < locals + internalLocals; i++) {
		int r = F->freeLocal + i;
		DeclareVariable(ctx, F, names[r], r);
		if (ctx->error.length()>0) return;
	}
	F->freeLocal += locals + internalLocals;
}

void PrintFunctionCheck(Function* F) {
}

std::string PrintFunction(GluaDecompileContextP ctx, Function* F) {
	auto &buff = F->decompiledCode;
	int indent = F->indent;
	PrintFunctionCheck(F);

	clear_sstream(buff);

	if (IsMain(ctx, F->f)) {
		buff << "-- params : " << F->funcBlock->code << "\n";
		PrintAstSub(ctx, F->funcBlock, buff, 0);
	}
	else {
		buff << "function(" << F->funcBlock->code << ")\n";
		PrintAstSub(ctx, F->funcBlock, buff, indent + 1);
		PrintIndent(buff, indent);
		buff << "end\n";
	}

	return std::string(F->decompiledCode.str());
}

/*
** -------------------------------------------------------------------------
*/

std::string RegisterOrConstant(GluaDecompileContextP ctx, Function* F, int r) {
	if (ISK(r)) {
		return DecompileConstant(F->f, INDEXK(r));
	}
	else {
		return GetR(ctx, F, r);
	}
}

// isalpha in stdlib is undefined when ch>=256 , may throw a assertion error.
int luadec_isalpha(int ch) {
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}
int luadec_isdigit(int ch) {
	return (ch >= '0' && ch <= '9');
}
int luadec_isalnum(int ch) {
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

const int numofkeywords = 23;
const char* keywords[] = {
	"and", "break", "do", "else", "elseif",
	"end", "false", "for", "function", "if",
	"in", "local", "nil", "not", "or",
	"repeat", "return", "then", "true", "until",
	"while", "continue", "goto"
};

int isIdentifier(std::string src) {
	int dot = 0;
	if (luadec_isalpha(src[0]) || src[0] == '_') {
		int i;
		const char *at;
		int len = (int)src.length();
		dot = 1;
		for (at = src.c_str() + 1; at < src.c_str() + len; at++) {
			if (!(luadec_isalnum(*at) || *at == '_')) {
				dot = 0;
				break;
			}
		}
		for (i = 0; i < numofkeywords; i++) {
			if (strcmp(keywords[i], src.c_str()) == 0) {
				dot = 0;
				break;
			}
		}
	}
	return dot;
}

/*
** type: DOT, SELF, TABLE, SQUARE_BRACKET
** input and output
** rstr   "a"  " a"    "or"    a       a+2
** SELF   :a   ERROR   ERROR   ERROR   ERROR
** DOT    .a   [" a"]  ["or"]  [a]     [a+2]
** TABLE   a   [" a"]  ["or"]  [a]     [a+2]
** SB    ["a"] [" a"]  ["or"]  [a]     [a+2]
*/
IndexType MakeIndex(GluaDecompileContextP ctx, Function* F, std::stringstream &str, std::string rstr, IndexType type) {
	int ret;
	int len = (int) rstr.length();
	if (type == SQUARE_BRACKET) {
		str << "[" << rstr << "]";
		return SQUARE_BRACKET;
	}
	/*
	* see if index can be expressed without quotes
	*/
	if (rstr[0] == '\"' && rstr[len - 1] == '\"') {
		rstr[len - 1] = '\0';
		if (isIdentifier((rstr.c_str() + 1))) {
			switch (type) {
			case SELF:
				str << ":" << (rstr.c_str() + 1);
				ret = SELF;
				break;
			case DOT:
				str << "." << (rstr.c_str() + 1);
				ret = DOT;
				break;
			case TABLE:
				str << (rstr.c_str() + 1);
				ret = TABLE;
				break;
			}
			rstr[len - 1] = '\"';
			return (IndexType_) ret;
		}
		rstr[len - 1] = '\"';
	}

	if (type != SELF) {
		str << "[" << rstr << "]";
		return SQUARE_BRACKET;
	}
	else {
		std::string errorbuff;
		str << ":[" << rstr << "]";
		errorbuff = "[" + rstr + "] should be a SELF Operator";
		SET_ERROR(ctx, F, errorbuff);
		return SQUARE_BRACKET;
	}
}

void ShowState(GluaDecompileContextP ctx, Function* F) {
	int i;
	ListItem* walk;
	fprintf(stddebug, "\n");
	fprintf(stddebug, "next bool: %d\n", F->bools.size);
	fprintf(stddebug, "locals(%d): ", F->freeLocal);
	for (i = 0; i < F->freeLocal; i++) {
		fprintf(stddebug, "%d{%s} ", i, REGISTER(i).c_str());
	}
	fprintf(stddebug, "\n");
	fprintf(stddebug, "vpend(%d): ", F->vpend.size);

	walk = F->vpend.head;
	i = 0;
	while (walk) {
		int r = lua_cast(VarListItem*, walk)->reg;
		auto src = lua_cast(VarListItem*, walk)->src;
		auto dest = lua_cast(VarListItem*, walk)->dest;
		if (r != -1 && !PENDING(r)) {
			ctx->errortmp = std::string("Confused about usage of register R") + std::to_string(r) + " for variables";
			SET_ERROR(ctx, F, ctx->errortmp);
			return;
		}
		fprintf(stddebug, "%d{%s=%s} ", r, dest.c_str(), src.c_str());
		walk = walk->next;
	}
	fprintf(stddebug, "\n");
	fprintf(stddebug, "tpend(%d): ", SET_SIZE(F->tpend));
	walk = F->tpend->list.head;
	while (walk) {
		int r = lua_cast(IntSetItem*, walk)->value;
		fprintf(stddebug, "%d{%s} ", r, REGISTER(r).c_str());
		if (!PENDING(r)) {
			ctx->errortmp = std::string("Confused about usage of register R") + std::to_string(r) + " for temporaries";
			SET_ERROR(ctx, F, ctx->errortmp);
			return;
		}
		walk = walk->next;
	}
	fprintf(stddebug, "\n");
}

#define TRY(ctx, x)  x; if (ctx->error.length()>0) goto errorHandler

void DeclareLocal(GluaDecompileContextP ctx, Function* F, int ixx, std::string value) {
	if (!IS_VARIABLE(ixx)) {
		char x[10] = "";
		std::stringstream str;

		sprintf(x, "l_%d_%d", ctx->functionnum, ixx);
		DeclareVariable(ctx, F, x, ixx);
		IS_VARIABLE(ixx) = 1;
		clear_sstream(str);
		str << "local " << x << " = " << value;
		RawAddStatement(F, str);
		F->freeLocal++;
	}
}

void DeclarePendingLocals(GluaDecompileContextP ctx, Function* F) {
	std::stringstream str;
	if (SET_SIZE(F->tpend)>0) {
		if (ctx->guess_locals) {
			clear_sstream(str);
			str << "-- WARNING: pending registers.";
		}
		else {
			ListItem* walk = F->tpend->list.head;
			clear_sstream(str);
			str << "-- WARNING: pending registers. Declaring locals.";
			AddStatement(ctx, F, str);
			while (walk) {
				int reg = lua_cast(IntSetItem*, walk)->value;
				std::string s(REGISTER(reg));
				GetR(ctx, F, reg);
				DeclareLocal(ctx, F, reg, s);
				walk = walk->next;
			}
		}
	}
}

Proto* toproto(lua_State* L, int i);

int FunctionCheck(GluaDecompileContextP ctx, Proto* f, std::string funcnumstr, std::stringstream &str) {
	lua_State* newState;
	int check_result;
	auto decompiled = ProcessSubFunction(ctx, f, 1, funcnumstr);
	newState = lua_open();
	if (luaL_loadstring(newState, decompiled.c_str()) != 0) {
		check_result = -1;
		clear_sstream(str);
		str << "-- function check fail " << funcnumstr << " : cannot compile";
	}
	else {
		std::stringstream compare_result_str;
		Proto* newProto = toproto(newState, -1);;
		if (!IsMain(ctx, f)) {
			newProto = newProto->p[0];
		}
		check_result = CompareProto(f, newProto, compare_result_str);
		if (check_result == 0) {
			clear_sstream(str);
			str << "-- function check pass " << funcnumstr;
		}
		else {
			clear_sstream(str);
			str << "-- function check fail " << funcnumstr << " : " << compare_result_str.str();
		}
	}

	lua_close(newState);
	return check_result;
}

int CompareProto(const Proto* fleft, const Proto* fright, std::stringstream &str) {
	int sizesame, pc, minsizecode;
	int diff = 0;
	clear_sstream(str);
	if (fleft->numparams != fright->numparams) {
		diff++;
		str << " different params size;";
	}
	if (NUPS(fleft) != NUPS(fright)) {
		diff++;
		str << " different upvalues size;";
	}
	if (fleft->is_vararg != fright->is_vararg) {
		diff++;
		str << " different is_vararg;";
	}
	if (fleft->sizecode != fright->sizecode) {
		diff++;
		str << " different code size;";
	}
	sizesame = 0;
	minsizecode = MIN(fleft->sizecode, fright->sizecode);
	for (pc = 0; pc < minsizecode; pc++) {
		Instruction ileft = fleft->code[pc];
		Instruction iright = fright->code[pc];
		if (ileft == iright) {
			sizesame++;
		}
		else {
			Inst left = extractInstruction(ileft);
			Inst right = extractInstruction(iright);
			if (left.op == OP_EQ && right.op == OP_EQ) {
				if (left.a == right.a && left.b == right.c && left.c == right.b) {
					sizesame++;
				}
			}
			else if ((left.op == OP_LT && right.op == OP_LE) ||
				(left.op == OP_LE && right.op == OP_LT)) {
				if (left.a == !right.a && left.b == right.c && left.c == right.b) {
					sizesame++;
				}
			}
		}
	}
	if (sizesame != fleft->sizecode) {
		diff++;
		str << " sizecode org: " << fleft->sizecode << ", decompiled: " << fright->sizecode << ", same: " << sizesame << ";";
	}
	return diff;
}

std::string PrintFunctionOnlyParamsAndUpvalues(GluaDecompileContextP ctx, const Proto* f, int indent, std::string funcnumstr) {
	std::stringstream code;
	code << "function(";
	listParams(ctx, f, code);
	code << ") ";

	if (NUPS(f) > 0) {
		code << "local _upvalues_ = {";
		listUpvalues(ctx, f, code);
		code << "};";
	}

	code << " end";
	return code.str();
}

void listParams(GluaDecompileContextP ctx, const Proto* f, std::stringstream &str) {
	if (f->numparams > 0) {
		int i = 0;
		str << getLocalName(ctx, f, i);
		for (i = 1; i < f->numparams; i++) {
			str << ", " << getLocalName(ctx, f, i);
		}
		if (f->is_vararg) {
			str << ", ...";
		}
	}
	else if (f->is_vararg) {
		str << "...";
	}
}

void listUpvalues(GluaDecompileContextP ctx, const Proto* f, std::stringstream &str) {
	int i = 0;
	str << getUpvalName(ctx, f, i);
	for (i = 1; i < NUPS(f); i++) {
		str << ", " << getUpvalName(ctx, f, i);
	}
}

int isTestOpCode(OpCode op) {
	return (op == OP_EQ || op == OP_LE || op == OP_LT || op == OP_TEST || op == OP_TESTSET);
}

AstStatement* LeaveBlock(GluaDecompileContextP ctx, Function* F, AstStatement* currStmt, StatementType type) {
	char msg[128];
	while (currStmt && currStmt->type != type) {
		sprintf(msg, "LeaveBlock: unexpected jumping out %s", stmttype[currStmt->type]);
		SET_ERROR(ctx, F, msg);
		if (currStmt->type == FUNCTION_STMT || currStmt->type == FORLOOP_STMT || currStmt->type == TFORLOOP_STMT) {
			sprintf(msg, "LeaveBlock: cannot find end of %s , stop at %s", stmttype[type], stmttype[currStmt->type]);
			SET_ERROR(ctx, F, msg);
			return currStmt;
		}
		currStmt = currStmt->parent;
	}

	if (currStmt) {
		return currStmt->parent;
	}
	else {
		sprintf(msg, "LeaveBlock: cannot find end of %s ,fatal NULL returned", stmttype[type]);
		SET_ERROR(ctx, F, msg);
		return nullptr;
	}
}

void PrintLoopTree(LoopItem* li, int indent) {
	LoopItem* child = li->child;
	int i = 0;
	for (i = 0; i < indent; i++) {
		fprintf(stderr, "  ");
	}
	fprintf(stderr, "%s=0x%x prep=%d start=%d body=%d end=%d out=%d block=0x%x \n"
		, stmttype[li->type], (intptr_t) li, li->prep, li->start, li->body, li->end, li->out, (intptr_t) li->block);
	while (child) {
		PrintLoopTree(child, indent + 1);
		child = child->next;
	}
}

/*
** from lfunc.c : const char *luaF_getlocalname (const Proto *f, int local_number, int pc)
** local_number and pc are 1-based not 0-based
** Look for n-th local variable at line `line' in function `func'.
** Returns -1 if not found.
*/
int getLocVarIndex(const Proto* f, int local_number, int pc) {
	int i;
	for (i = 0; i < f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
		if (pc <= f->locvars[i].endpc) {
			local_number--;
			if (local_number == 0)
				return i;
		}
	}
	return -1;
}

int testLocVarIndex(const Proto* f, int reg, int pc, int Rvar[], int RvarTop, int func_checking, std::string funcnumstr) {
	int locVarIndex = getLocVarIndex(f, reg + 1, pc + 1);
	int currLocVar = (reg < RvarTop) ? Rvar[reg] : -1;

	if (currLocVar < 0) {
		if (locVarIndex < 0) {
			if (!func_checking) {
				fprintf(stderr, "currLocVar < 0 && locVarIndex < 0 : f=%s pc=%d r=%d RvarTop=%d currLocVar=%d locVarIndex==%d at ",
					funcnumstr.c_str(), pc, reg, RvarTop, currLocVar, locVarIndex);
				printFileNames(stderr);
				fprintf(stderr, "\n");
			}
			return -1;
		}
		else {
			if (!func_checking) {
				fprintf(stderr, "currLocVar < 0 && locVarIndex >= 0 : f=%s pc=%d r=%d RvarTop=%d currLocVar=%d locVarIndex=%d varname=%s startpc=%d endpc=%d at ",
					funcnumstr.c_str(), pc, reg, RvarTop, currLocVar, locVarIndex,
					getstr(f->locvars[locVarIndex].varname), f->locvars[locVarIndex].startpc, f->locvars[locVarIndex].endpc);
				printFileNames(stderr);
				fprintf(stderr, "\n");
			}
			return locVarIndex;
		}
	}
	else if (locVarIndex < 0) {
		if (!func_checking) {
			fprintf(stderr, "currLocVar >= 0 && locVarIndex < 0 : f=%s pc=%d r=%d locVarIndex=%d currLocVar=%d varname=%s startpc=%d endpc=%d at ",
				funcnumstr.c_str(), pc, reg, locVarIndex, currLocVar,
				getstr(f->locvars[currLocVar].varname), f->locvars[currLocVar].startpc, f->locvars[currLocVar].endpc);
			printFileNames(stderr);
			fprintf(stderr, "\n");
		}
		return currLocVar;
	}
	else if (currLocVar != locVarIndex) {
		if (!func_checking) {
			fprintf(stderr, "currLocVar != locVarIndex : f=%s pc=%d r=%d currLocVar=%d locVarIndex=%d varname=%s startpc=%d endpc=%d at ",
				funcnumstr.c_str(), pc, reg, currLocVar, locVarIndex,
				getstr(f->locvars[locVarIndex].varname), f->locvars[locVarIndex].startpc, f->locvars[locVarIndex].endpc);
			printFileNames(stderr);
			fprintf(stderr, "\n");
		}
		return locVarIndex;
	}
	else {
		// correct routine , currLocVar == locVarIndex >= 0
		return currLocVar;
	}
}

#define TestLocVarIndex(reg, pc) testLocVarIndex(f, reg, pc, Rvar, RvarTop, func_checking, funcnumstr)
//#define TestLocVarIndex(reg, pc) do { } while(0)

#define GetLocVarIndex(reg, pc) testLocVarIndex(f, reg, pc, Rvar, RvarTop, func_checking, funcnumstr)
//#define GetLocVarIndex(reg, pc) getLocVarIndex(f, reg + 1, pc + 1)
//#define GetLocVarIndex(reg, pc) ((reg < RvarTop) ? Rvar[reg] : -1)

int gargc = 0;
char** gargv = nullptr;
int filename_argv_from = 0;

int printFileNames(FILE* out) {
	int i;
	if (gargc > filename_argv_from) {
		fprintf(out, "%s", gargv[filename_argv_from]);
		for (i = filename_argv_from + 1; i < gargc; i++) {
			fprintf(out, " , %s", gargv[i]);
		}
	}
	return gargc - filename_argv_from;
}

void GluaDecompileContext::reset_error()
{
	this->error_ss.clear();
}

std::string ProcessCode(GluaDecompileContextP ctx, Proto* f, int indent, int func_checking, std::string funcnumstr)
{
	int i = 0;

	int ignoreNext = 0;

	InitOperators();

	Function* F;
	std::stringstream str;

	const Instruction* code = f->code;
	int pc, n = f->sizecode;
	int baseIndent = indent;

	std::string output;

	int currLocVar = 0;
	int RvarTop = 0;
	int Rvar[MAXARG_A];

	LoopItem* next_child;

	F = NewFunction(f);
	F->funcnumstr = funcnumstr;
	F->indent = indent;
	F->pc = 0;
	ctx->error = "";

	FixLocalNames(ctx, f, funcnumstr);

	/*
	* Function parameters are stored in registers from 0 on.
	*/
	for (i = 0; i < f->numparams; i++) {
		TRY(ctx, DeclareVariable(ctx, F, getLocalName(ctx, F->f, i), i));
		IS_VARIABLE(i) = 1;
	}
	F->freeLocal = f->numparams;

	listParams(ctx, f, str);
	F->funcBlock->code = str.str();
	if (!IsMain(ctx, f)) {
		F->indent++;
	}

	// make function comment
	clear_sstream(str);
	str << "-- function num : " << funcnumstr;
	if (NUPS(f) > 0) {
		str << " , upvalues : ";
		listUpvalues(ctx, f, str);
	}
	TRY(ctx, RawAddStatement(F, str));
	clear_sstream(str);

	if (ctx->func_check == 1 && func_checking == 0) {
		int func_check_result = FunctionCheck(ctx, f, funcnumstr, str);
		TRY(ctx, RawAddStatement(F, str));
	}

	if (NEED_ARG(f)) {
		TRY(ctx, DeclareVariable(ctx, F, "arg", F->freeLocal));
		F->freeLocal++;
	}

	if (ctx->locals) {
		for (i = F->freeLocal; i<f->maxstacksize; i++) {
			DeclareLocal(ctx, F, i, "nil");
		}
	}

	for (pc = n - 1; pc >= 0; pc--) {
		Instruction i = code[pc];
		OpCode o = GET_OPCODE(i);
		int a = GETARG_A(i);
		int sbc = GETARG_sBx(i);
		int dest;
		int real_end;

		Instruction i_1 = 0;
		OpCode o_1 = (OpCode) (OP_VARARG + 1);
		if (pc > 0) {
			i_1 = code[pc - 1];
			o_1 = GET_OPCODE(i_1);
		}

		while (pc < F->loop_ptr->start) {
			F->loop_ptr = F->loop_ptr->parent;
		}

		if (pc > 0 && o_1 == OP_SETLIST && GETARG_C(i_1) == 0) {
			continue;
		}
		if (o == OP_SETLIST && GETARG_C(i) == 0) {
			continue;
		}

		dest = sbc + pc + 1;
		real_end = GetJmpAddr(F, pc + 1);

			if (o == OP_JMP && a > 0) {
				// instead OP_CLOSE in 5.2 : if (A) close all upvalues >= R(A-1)
				int startreg = a - 1;

				AddToSet(F->do_opens, f->locvars[startreg].startpc);
				AddToSet(F->do_closes, f->locvars[startreg].endpc);
				// continue; // comment out because of 5.2 OP_JMP
			}

				if (o == OP_TFORLOOP) {
					// OP_TFORCALL /* A C	R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2)); */
					// OP_TFORLOOP /* A sBx	if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx } */
					LoopItem* item = NewLoopItem(TFORLOOP_STMT, dest - 1, dest, dest, pc, real_end);
					AddToLoopTree(F, item);
					continue;
				}

				if (o == OP_FORLOOP) {
					LoopItem* item = NewLoopItem(FORLOOP_STMT, dest - 1, dest, dest, pc, real_end);
					AddToLoopTree(F, item);
					continue;
				}

				if (o == OP_JMP && pc > 0) {
					AstStatement* jmp = nullptr;
					AstStatement* jmpdest = lua_cast(AstStatement*, F->jmpdests.tail);
					while (jmpdest && jmpdest->line > dest) {
						jmpdest = lua_cast(AstStatement*, jmpdest->super.prev);
					}
					if (jmpdest == nullptr || jmpdest->line < dest) {
						AstStatement* newjmpdest = MakeBlockStatement(JMP_DEST_STMT, "");
						newjmpdest->line = dest;
						AddAllAfterListItem(&(F->jmpdests), (ListItem*)jmpdest, (ListItem*)newjmpdest);
						jmpdest = newjmpdest;
					}
					jmp = MakeSimpleStatement("");
					jmp->line = pc;
					jmp->parent = jmpdest;
					AddToListHead(jmpdest->sub, (ListItem*)jmp);

					if (dest == F->loop_ptr->out) {
						if (!isTestOpCode(o_1)) {
							//breaks
							IntListItem* intItem = NewIntListItem(pc);
							AddToList(&(F->breaks), lua_cast(ListItem*, intItem));
						}
					}
					else if (F->loop_ptr->start <= dest && dest < pc) {
						if (isTestOpCode(o_1)) { //REPEAT jump back
												 /*
												 ** if the out loop(loop_ptr) is while and body=loop_ptr.start,
												 ** jump back may be 'until' or 'if', they are the same,
												 ** but 'if' is more clear, so we skip making a loop to choose 'if'.
												 ** see the lua code:
												 ** local a,b,c,f
												 **
												 ** while 1 do
												 **	repeat
												 **		f(b)
												 **	until c
												 **	f(a)
												 ** end
												 **
												 ** while 1 do
												 **	f(b)
												 ** 	if c then
												 ** 		f(a)
												 **	end
												 ** end
												 */
							if (!((F->loop_ptr->type == WHILE_STMT) && (dest == F->loop_ptr->start))) {
								LoopItem* item = NewLoopItem(REPEAT_STMT, dest, dest, dest, pc, real_end);
								AddToLoopTree(F, item);
							}
						}
						else {
							// WHILE jump back
							LoopItem* item = NewLoopItem(WHILE_STMT, dest, dest, dest, pc, real_end);
							AddToLoopTree(F, item);
						}
					}
				}
			}

			F->loop_ptr = F->loop_tree;
			next_child = F->loop_tree->child;

			if (ctx->debug) {
				fprintf(stderr, "LoopTree of function %s\n", funcnumstr.c_str());
				PrintLoopTree(F->loop_ptr, 0);
			}

			ignoreNext = 0;
			for (pc = 0; pc < n; pc++) {
				Instruction i = code[pc];
				OpCode o = GET_OPCODE(i);
				int a = GETARG_A(i);
				int b = GETARG_B(i);
				int c = GETARG_C(i);
				int bc = GETARG_Bx(i);
				int sbc = GETARG_sBx(i);

				F->pc = pc;

				// pop 所有 endpc < pc 的
				while (RvarTop > 0 && f->locvars[Rvar[RvarTop - 1]].endpc < pc + 1) {
					RvarTop--;
					Rvar[RvarTop] = -1;
				}
				// push 所有 startpc <= pc 的，移到下一个未使用的变量
				while (currLocVar < f->sizelocvars && f->locvars[currLocVar].startpc <= pc + 1) {
					Rvar[RvarTop] = currLocVar;
					RvarTop++;
					currLocVar++;
					TestLocVarIndex(RvarTop - 1, pc);
				}
				// 那么此时 vars[r] 即对应 reg[r] 的变量

				if (pc > F->loop_ptr->end) {
					next_child = F->loop_ptr->next;
					F->loop_ptr = F->loop_ptr->parent;
				}

				while (next_child && pc >= next_child->body) {
					F->loop_ptr = next_child;
					next_child = F->loop_ptr->child;
				}

				// nil optimization of Lua 5.1
				if (pc == 0) {
					int ixx, num_nil = -1;
					switch (o) {
					case OP_SETTABUP:
						if (!ISK(b)) {
							num_nil = b;
						}
					case OP_GETTABUP:
						if (!ISK(c)) {
							num_nil = MAX(num_nil, c);
						}
						break;
					case OP_SETUPVAL:
						num_nil = a;
						break;
					case OP_JMP:
						break;
					default:
						num_nil = a - 1;
						break;
					}
					for (ixx = F->freeLocal; ixx <= num_nil; ixx++) {
						TRY(ctx, AssignReg(ctx, F, ixx, "nil", 0, 1));
						PENDING(ixx) = 1;
					}
				}
				if (ignoreNext) {
					ignoreNext--;
					continue;
				}

				/*
				* Disassembler info
				*/
				if (ctx->debug) {
					fprintf(stddebug, "----------------------------------------------\n");
					fprintf(stddebug, "\t%d\t", pc + 1);
					fprintf(stddebug, "%-9s\t", luaP_opnames[o]);
					switch (getOpMode(o)) {
					case iABC:
						fprintf(stddebug, "%d %d %d", a, b, c);
						break;
					case iABx:
						fprintf(stddebug, "%d %d", a, bc);
						break;
					case iAsBx:
						fprintf(stddebug, "%d %d", a, sbc);
						break;
					}
					fprintf(stddebug, "\n");
				}

				TRY(ctx, DeclareLocals(ctx, F));
				TRY(ctx, ReleaseLocals(ctx, F));

				while (RemoveFromSet(F->do_opens, pc)) {
					AstStatement* blockstmt = MakeBlockStatement(DO_STMT, "");
					AddAstStatement(ctx, F, lua_cast(AstStatement*, blockstmt));
					F->currStmt = blockstmt;
				}

				while (RemoveFromSet(F->do_closes, pc)) {
					AstStatement* block = F->currStmt;
					F->currStmt = LeaveBlock(ctx, F, block, DO_STMT);
				}

				while ((F->currStmt->type == IF_THEN_STMT || F->currStmt->type == IF_ELSE_STMT)
					&& ElseStart(F->currStmt->parent) == GetJmpAddr(F, pc)) {
					AstStatement* ifstmt = F->currStmt->parent;
					F->currStmt = ifstmt->parent;
				}

				if (F->jmpdests.head && lua_cast(AstStatement*, F->jmpdests.head)->line == pc) {
					AstStatement* jmpdest = lua_cast(AstStatement*, RemoveFromList(&(F->jmpdests), F->jmpdests.head));
					AddToStatement(F->currStmt, jmpdest);
				}

				if ((F->loop_ptr->start == pc) && (F->loop_ptr->type == REPEAT_STMT || F->loop_ptr->type == WHILE_STMT)) {
					LoopItem* walk = F->loop_ptr;

					while (walk->parent && (walk->parent->start == pc) && (walk->parent->type == REPEAT_STMT || walk->parent->type == WHILE_STMT)) {
						walk = walk->parent;
					}

					while (!(walk == F->loop_ptr)) {
						AstStatement* loopstmt = nullptr;
						if (walk->type == WHILE_STMT) {
							walk->body = walk->start;
							loopstmt = walk->block;
							loopstmt->code = std::string("1");
						}
						else if (walk->type == REPEAT_STMT) {
							loopstmt = walk->block;
						}
						RawAddAstStatement(F, lua_cast(AstStatement*, loopstmt));
						F->currStmt = loopstmt;
						walk = walk->child;
					}

					if (walk->type == REPEAT_STMT) {
						AstStatement* loopstmt = walk->block;
						RawAddAstStatement(F, lua_cast(AstStatement*, loopstmt));
						F->currStmt = loopstmt;
					}
					else if (walk->type == WHILE_STMT) {
						/*
						** try to process all while as " while 1 do if "
						** see the lua code:
						** local f, a, b, c
						**
						** while test do
						** 	whilebody
						** end
						**
						** while 1 do
						** 	if test then
						** 		whilebody
						** 	else
						** 		break
						** 	end
						** end
						*/
						AstStatement* loopstmt = walk->block;
						loopstmt->code = std::string("1");
						RawAddAstStatement(F, lua_cast(AstStatement*, loopstmt));
						F->currStmt = loopstmt;
						walk->body = walk->start;
					}
				}

				clear_sstream(str);

				switch (o) {
				case OP_MOVE:
					/* Upvalue handling added to OP_CLOSURE */
				{
					std::string bstr;
					if (a == b)
						break;
					if (CALL(b) < 2)
						bstr = GetR(ctx, F, b);
					else
						UnsetPending(ctx, F, b);
					if (ctx->error.length()>0)
						goto errorHandler;
					/*
					* Copy from one register to another
					*/
					TRY(ctx, AssignReg(ctx, F, a, bstr, PRIORITY(b), 1));
					break;
				}
				case OP_LOADK:
				{
					/*
					* Constant. Store it in register.
					*/
					auto ctt = DecompileConstant(f, bc);
					TRY(ctx, AssignReg(ctx, F, a, ctt, 0, 1));
					break;
				}
				case OP_LOADKX:
				{
					int ax = GETARG_Ax(code[pc + 1]);
					auto ctt = DecompileConstant(f, ax);
					TRY(ctx, AssignReg(ctx, F, a, ctt, 0, 1));
					pc++;
					break;
				}
				case OP_EXTRAARG:
					break;
				case OP_LOADBOOL:
				{
					if ((F->bools.size == 0) || (c == 0)) {
						/*
						* assign boolean constant
						*/
						if (PENDING(a)) {
							// some boolean constructs overwrite pending regs :(
							TRY(ctx, UnsetPending(ctx, F, a));
						}
						TRY(ctx, AssignReg(ctx, F, a, b ? "true" : "false", 0, 1));
					}
					else {
						/*
						* assign boolean value
						*/
						int thenaddr = 0, endif = 0;
						std::string test;
						TRY(ctx, test = OutputBoolean(ctx, F, &thenaddr, nullptr, 1));
						TRY(ctx, AssignReg(ctx, F, a, test, 0, 0));
					}
					if (c)
						ignoreNext = 1;
					break;
				}
				case OP_LOADNIL:
				{
					int ra, rb;
					ra = a;
					// 5.2	A B	R(A) to R(A+B) := nil
					rb = a + b;
					do {
						TRY(ctx, AssignReg(ctx, F, rb--, "nil", 0, 1));
					} while (rb >= ra);
					break;
				}
				case OP_VARARG: // Lua5.1 specific.
				{
					int i;
					/*
					* Read ... into register.
					*/
					if (b == 0) {
						IS_VARIABLE(a) = 0; PENDING(a) = 0;
						TRY(ctx, AssignReg(ctx, F, a, "...", 0, 1));
						CALL(a) = 1;
						IS_VARIABLE(a + 1) = 0; PENDING(a + 1) = 0;
						TRY(ctx, AssignReg(ctx, F, a + 1, ".end", 0, 1));
						CALL(a + 1) = 2;
					}
					else {
						for (i = 0; i < b - 1; i++) {
							PENDING(a + i) = 0;
							TRY(ctx, AssignReg(ctx, F, a + i, "...", 0, 1));
							CALL(a + i) = i + 1;
						}
					}
					break;
				}
				case OP_GETUPVAL:
				{
					/*	A B	R(A) := UpValue[B]				*/
					TRY(ctx, AssignReg(ctx, F, a, UPVALUE(ctx, b), 0, 1));
					break;
				}
				case OP_GETTABUP:
				{
					/*	A B C	R(A) := UpValue[B][RK(C)]			*/
					auto upvstr = UPVALUE(ctx, b);
					auto keystr = RegisterOrConstant(ctx, F, c);
					clear_sstream(str);
					if (upvstr == "_ENV" && ISK(c)) {
						if (MakeIndex(ctx, F, str, keystr, TABLE) != TABLE) {
							auto added = std::string(upvstr) + str.str();
							clear_sstream(str);
							str << added;
						}
					}
					else {
						clear_sstream(str);
						str << upvstr;
						MakeIndex(ctx, F, str, keystr, DOT);
					}
					TRY(ctx, AssignReg(ctx, F, a, str.str(), 0, 1));
					break;
				}
				case OP_GETTABLE:
				{
					/*	A B C	R(A) := R(B)[RK(C)]				*/
					std::string bstr;
					std::string cstr;
					TRY(ctx, cstr = RegisterOrConstant(ctx, F, c));
					TRY(ctx, bstr = GetR(ctx, F, b));
					if (isIdentifier(bstr)) {
						clear_sstream(str);
						str << bstr;
					}
					else {
						clear_sstream(str);
						str << "(" << bstr << ")";
					}
					MakeIndex(ctx, F, str, cstr, DOT);
					TRY(ctx, AssignReg(ctx, F, a, str.str(), 0, 0));
					break;
				}
				case OP_SETTABUP:
				{
					/*	A B C	UpValue[A][RK(B)] := RK(C)			*/
					auto upvstr = UPVALUE(ctx, a);
					auto keystr = RegisterOrConstant(ctx, F, b);
					auto cstr = RegisterOrConstant(ctx, F, c);
					clear_sstream(str);
					if (upvstr == "_ENV" && ISK(b)) {
						if (MakeIndex(ctx, F, str, keystr, TABLE) != TABLE) {
							auto replaced = std::string(upvstr) + str.str();
							clear_sstream(str);
							str << replaced;
						}
					}
					else {
						clear_sstream(str);
						str << upvstr;
						MakeIndex(ctx, F, str, keystr, DOT);
					}
					TRY(ctx, AssignGlobalOrUpvalue(F, str.str(), cstr));
					break;
				}
				case OP_SETUPVAL:
				{
					/*	A B	UpValue[B] := R(A)				*/
					auto var = UPVALUE(ctx, b);
					std::string astr;
					TRY(ctx, astr = GetR(ctx, F, a));
					TRY(ctx, AssignGlobalOrUpvalue(F, var, astr));
					break;
				}
				case OP_SETTABLE:
				{
					/*	A B C	R(A)[RK(B)] := RK(C)				*/
					std::string astr;
					std::string bstr, cstr;
					int settable;
					TRY(ctx, bstr = RegisterOrConstant(ctx, F, b));
					TRY(ctx, cstr = RegisterOrConstant(ctx, F, c));
					/*
					* first try to add into a table
					*/
					TRY(ctx, settable = SetTable(ctx, F, a, bstr, cstr));
					if (!settable) {
						/*
						* if failed, just output an assignment
						*/
						TRY(ctx, astr = GetR(ctx, F, a));
						if (isIdentifier(astr)) {
							clear_sstream(str);
							str << astr;
						}
						else {
							clear_sstream(str);
							str << "(" << astr << ")";
						}
						MakeIndex(ctx, F, str, bstr, DOT);
						TRY(ctx, AssignGlobalOrUpvalue(F, str.str(), cstr));
					}
					break;
				}
				case OP_NEWTABLE:
				{
					TRY(ctx, StartTable(F, a, b, c, pc));
					break;
				}
				case OP_SELF:
				{
					/*
					* Read table entry into register.
					*/
					std::string bstr;
					std::string cstr;
					TRY(ctx, cstr = RegisterOrConstant(ctx, F, c));
					TRY(ctx, bstr = GetR(ctx, F, b));

					TRY(ctx, AssignReg(ctx, F, a + 1, bstr, PRIORITY(b), 0));
					if (isIdentifier(bstr)) {
						clear_sstream(str);
						str << bstr;
					}
					else {
						str << "(" << bstr << ")";
					}
					MakeIndex(ctx, F, str, cstr, SELF);
					TRY(ctx, AssignReg(ctx, F, a, str.str(), 0, 0));
					break;
				}
				case OP_ADD:
				case OP_SUB:
				case OP_MUL:
				case OP_DIV:
				case OP_POW:
				case OP_MOD:
				case OP_IDIV:
				case OP_BAND:
				case OP_BOR:
				case OP_BXOR:
				case OP_SHL:
				case OP_SHR:
				{
					std::string bstr, cstr;
					const char *oper = operators[o];
					int prio = priorities[o];
					int bprio = PRIORITY(b);
					int cprio = PRIORITY(c);
					TRY(ctx, bstr = RegisterOrConstant(ctx, F, b));
					TRY(ctx, cstr = RegisterOrConstant(ctx, F, c));
					// FIXME: might need to change from <= to < here
					if ((prio != 1 && bprio <= prio) || (prio == 1 && bstr[0] != '-')) {
						str << bstr;
					}
					else {
						str << "(" << bstr << ")";
					}
					str << " " << oper << " ";
					// FIXME: being conservative in the use of parentheses
					if (cprio < prio) {
						str << cstr;
					}
					else {
						str << "(" << cstr << ")";
					}
					TRY(ctx, AssignReg(ctx, F, a, str.str(), prio, 0));
					break;
				}
				case OP_UNM:
				case OP_NOT:
				case OP_LEN:
				case OP_BNOT:
				{
					std::string bstr;
					int prio = priorities[o];
					int bprio = PRIORITY(b);
					TRY(ctx, bstr = GetR(ctx, F, b));
					str << operators[o];
					if (bprio <= prio) {
						str << bstr;
					}
					else {
						str << "(" << bstr << ")";
					}
					TRY(ctx, AssignReg(ctx, F, a, str.str(), 0, 0));
					break;
				}
				case OP_CONCAT:
				{
					int i;
					for (i = b; i <= c; i++) {
						std::string istr;
						TRY(ctx, istr = GetR(ctx, F, i));
						if (PRIORITY(i) > priorities[o]) {
							str << "(" << istr << ")";
						}
						else {
							str << istr;
						}
						if (i < c)
							str << " .. ";
					}
					TRY(ctx, AssignReg(ctx, F, a, str.str(), 0, 0));
					break;
				}
				case OP_JMP:
				{
					// instead OP_CLOSE in 5.2 : if (A) close all upvalues >= R(A-1)
					int dest = sbc + pc + 1;
					Instruction idest = code[dest];
					IntListItem* foundInt = (IntListItem*)RemoveFromList(&(F->breaks), FindFromListTail(&(F->breaks), (ListItemCmpFn)MatchIntListItem, &pc));
					if (foundInt != nullptr) { // break
						AstStatement* breakstmt = MakeStatement(BREAK_STMT, "");
						delete foundInt;
						TRY(ctx, AddAstStatement(ctx, F, breakstmt));
					}
					else if (F->loop_ptr->end == pc) { // until jmp has been processed, tforloop has ignored the jmp, forloop does not have a jmp
						if (F->currStmt->type == IF_THEN_STMT && ElseStart(F->currStmt->parent) == GetJmpAddr(F, pc + 1)) {
							// Change 'while 1 do if' to 'while'
							AstStatement* currStmt = F->currStmt;
							AstStatement* ifStmt = currStmt->parent;
							AstStatement* parentStmt = ifStmt->parent;
							if (parentStmt->type == WHILE_STMT && parentStmt->sub->size == 1) {
								// if is the first statment of while body
								AstStatement* whileStmt = parentStmt;
								const auto &whileTest = whileStmt->code;
								if (whileTest == "1") {
									// ifthen to while
									RemoveFromList(ifStmt->sub, (ListItem*)currStmt);
									currStmt->type = WHILE_STMT;
									currStmt->code = ifStmt->code;
									currStmt->line = ifStmt->line;
									currStmt->parent = whileStmt->parent;
									RemoveFromList(whileStmt->parent->sub, (ListItem*)whileStmt);
									AddToStatement(currStmt->parent, currStmt);
									ifStmt->code = "";
									DeleteAstStatement(whileStmt);
								}
							}
						}
						F->currStmt = LeaveBlock(ctx, F, F->currStmt, WHILE_STMT);
					}
					else if (F->currStmt->type == IF_THEN_STMT && ElseStart(F->currStmt->parent) == GetJmpAddr(F, pc + 1)) {
						// jmp before 'else'
						AstStatement* ifstmt = F->currStmt->parent;
						F->currStmt = ElseStmt(ifstmt);
						ElseStart(ifstmt) = GetJmpAddr(F, dest); // reuse ElseStart as pc of endif
					}
					else if (next_child && next_child->type == TFORLOOP_STMT
						&& next_child->prep == pc) { // jmp of generic for
													 // TODO 5.2 OP_TFORCALL
													 /*
													 * generic 'for'
													 */
						int i;
						std::string generator, control, state;
						std::string vname[40];
						AstStatement* tforstmt = nullptr;

						if (GET_OPCODE(idest) != LUADEC_TFORLOOP) {
							fprintf(stderr, "error JMP at %d of TFORLOOP_STMT : GET_OPCODE(idest) != LUADEC_TFORLOOP\n", pc);
						}

						a = GETARG_A(idest);
						c = GETARG_C(idest);

						generator = GetR(ctx, F, a);
						control = GetR(ctx, F, a + 2);
						state = GetR(ctx, F, a + 1);
						for (i = 1; i <= c; i++) {
							if (!IS_VARIABLE(a + 2 + i)) {
								int i2;
								int loopvars = 0;
								vname[i - 1] = "";
								for (i2 = 0; i2 < f->sizelocvars; i2++) {
									if (f->locvars[i2].startpc == pc + 1) {
										loopvars++;
										//search for the loop variable. Set it's endpc one step further so it will be the same for all loop variables
										if (GET_OPCODE(F->f->code[f->locvars[i2].endpc - 2]) == LUADEC_TFORLOOP) {
											f->locvars[i2].endpc -= 2;
										}
										if (GET_OPCODE(F->f->code[f->locvars[i2].endpc - 1]) == LUADEC_TFORLOOP) {
											f->locvars[i2].endpc -= 1;
										}
										if (loopvars == 3 + i) {
											vname[i - 1] = LOCAL(ctx, i2);
											break;
										}
									}
								}
								if (vname[i - 1].length()<1) {
									std::string tmp = std::string("i_") + std::to_string(i);
									TRY(ctx, DeclareVariable(ctx, F, tmp, a + 2 + i));
									vname[i - 1] = F->R[a + 2 + i];
								}
							}
							else {
								vname[i - 1] = F->R[a + 2 + i];
							}
							F->Rinternal[a + 2 + i] = 1;
						}

						DeclarePendingLocals(ctx, F);

						clear_sstream(str);
						str << vname[0];
						for (i = 2; i <= c; i++) {
							str << "," << vname[i - 1];
						}
						str << " in " << generator;

						F->Rinternal[a] = 1;
						F->Rinternal[a + 1] = 1;
						F->Rinternal[a + 2] = 1;

						tforstmt = next_child->block;
						tforstmt->code = str.str();
						AddAstStatement(ctx, F, tforstmt);
						F->currStmt = tforstmt;
						break;
					}
					else if (sbc == 2 && GET_OPCODE(code[pc + 2]) == OP_LOADBOOL) {
						/*
						* y = (a > b or c) -- assigne statement may be bool or value
						* JMP 2
						* LOADBOOL Ra 0 1 must mark one as useful
						* LOADBOOL Ra 1 0
						* ::jmp_target
						*/
						fprintf(stderr, "processing OP_JMP to } else if (sbc == 2 && GET_OPCODE(code[pc+2]) == OP_LOADBOOL) { \n");
						fprintf(stderr, " at line %d in file %s\n", __LINE__, __FILE__);
						fprintf(stderr, " for lua files: ");
						printFileNames(stderr);
						fprintf(stderr, "\n");
						fprintf(stderr, " at lua function %s pc=%d\n\n", funcnumstr.c_str(), pc);
						fflush(stderr);
						{
							int boola = GETARG_A(code[pc + 1]);
							int thenaddr = 0, endif = 0;
							std::string test;
							/* skip */
							std::string ra = REGISTER(boola);
							AddToList(&(F->bools), (ListItem*)MakeBoolOp(ra, ra, OP_TESTSET, c, pc + 3, dest));
							F->testpending = a + 1;
							F->testjump = dest;
							TRY(ctx, test = OutputBoolean(ctx, F, &thenaddr, nullptr, 1));
							TRY(ctx, UnsetPending(ctx, F, boola));
							TRY(ctx, AssignReg(ctx, F, boola, test, 0, 0));
							ignoreNext = 2;
						}
					}
					else if (GET_OPCODE(idest) == OP_LOADBOOL) { // WHY
																 /*
																 * y = (a or b==c) -- assigne statement may be bool (calucate at last)
																 * constant boolean value
																 * JMP
																 * ....skipped , not decompiled
																 * ::jmp_target
																 * LOADBOOL
																 */
						fprintf(stderr, "processing OP_JMP to } else if (GET_OPCODE(idest) == OP_LOADBOOL) { \n");
						fprintf(stderr, " at line %d in file %s\n", __LINE__, __FILE__);
						fprintf(stderr, " for lua files: ");
						printFileNames(stderr);
						fprintf(stderr, "\n");
						fprintf(stderr, " at lua function %s pc=%d\n\n", funcnumstr.c_str(), pc);
						fflush(stderr);
						//pc = dest - 2;
					}
					else if (sbc == 0) {
						/* dummy jump -- ignore it */
						break;
					}
					else { // WHY
						fprintf(stderr, "processing OP_JMP to } else { \n");
						fprintf(stderr, " at line %d in file %s\n", __LINE__, __FILE__);
						fprintf(stderr, " for lua files: ");
						printFileNames(stderr);
						fprintf(stderr, "\n");
						fprintf(stderr, " at lua function %s pc=%d\n\n", funcnumstr.c_str(), pc);
						fflush(stderr);
						{
							int nextpc = pc + 1;
							int nextsbc = sbc - 1;
							for (;;) {
								Instruction nextins = code[nextpc];
								if (GET_OPCODE(nextins) == OP_JMP && GETARG_sBx(nextins) == nextsbc) {
									nextpc++;
									nextsbc--;
								}
								else
									break;
								if (nextsbc == -1) {
									break;
								}
							}
							if (nextsbc == -1) {
								pc = nextpc - 1;
								break;
							}

							/*
							if (F->indent > baseIndent) {
							StringBuffer_printf(str, "do return end");
							TRY(AddStatement(F, str));
							} else {
							pc = dest-2;
							}
							*/
						}
					}
					break;
				}
				case OP_EQ:
				case OP_LT:
				case OP_LE:
				{
					// WHY can't we remove it
					if (ISK(b)) {
						int swap = b;
						b = c;
						c = swap;
						if (o != OP_EQ) a = !a;
						if (o == OP_LT) o = OP_LE;
						else if (o == OP_LE) o = OP_LT;
					}
					AddToList(&(F->bools), (ListItem*)MakeBoolOp(RegisterOrConstant(ctx, F, b), RegisterOrConstant(ctx, F, c), o, a, pc + 1, -1));
					goto LOGIC_NEXT_JMP;
					break;
				}
				case OP_TESTSET: // Lua5.1 specific TODO: correct it
				{
					std::string ra, rb;

					if (!IS_VARIABLE(a)) {
						ra = REGISTER(a);
						PENDING(a) = 0;
					}
					else {
						TRY(ctx, ra = GetR(ctx, F, a));
					}
					TRY(ctx, rb = GetR(ctx, F, b));
					AddToList(&(F->bools), (ListItem*)MakeBoolOp(ra, rb, o, c, pc + 1, -1));
					F->testpending = a + 1;
					goto LOGIC_NEXT_JMP;
					break;
				}
				case OP_TEST:
				{
					std::string ra;

					TRY(ctx, ra = GetR(ctx, F, a));
					if (!IS_VARIABLE(a)) {
						PENDING(a) = 0;
						F->testpending = a + 1;
					}
					AddToList(&(F->bools), (ListItem*)MakeBoolOp(ra, ra, o, c, pc + 1, -1));
					goto LOGIC_NEXT_JMP;
					break;
				}
			LOGIC_NEXT_JMP:
				{
					int dest;
					BoolOp* lastBool;
					pc++;
					F->pc = pc;
					i = code[pc];
					o = GET_OPCODE(i);
					if (o != OP_JMP) {
						assert(0);
					}
					sbc = GETARG_sBx(i);
					dest = sbc + pc + 2;
					lastBool = lua_cast(BoolOp*, LastItem(&(F->bools)));
					lastBool->dest = dest;
					if (F->testpending) {
						F->testjump = dest;
					}
					if ((F->loop_ptr->type == REPEAT_STMT) && (F->loop_ptr->end == F->pc)) {
						int endif, thenaddr;
						std::string test;
						LogicExp* exp = nullptr;
						AstStatement* currStmt = F->currStmt;
						AstStatement* parentStmt = nullptr;
						TRY(ctx, exp = MakeBoolean(ctx, F, &thenaddr, &endif));
						TRY(ctx, test = WriteBoolean(exp, &thenaddr, &endif, 0));

						while (currStmt && currStmt->type != REPEAT_STMT) {
							currStmt = currStmt->parent;
						}
						if (currStmt) {
							currStmt->code = test;
							test = "";
							F->currStmt = currStmt->parent;
						}
						else {
							SET_ERROR(ctx, F, "LeaveBlock: cannot find 'until' of 'repeat'");
						}
						if (exp) DeleteLogicExpTree(exp);
					}
					break;
				}
				case OP_CALL:
				case OP_TAILCALL:
				{
					/*
					* Function call. The CALL opcode works like this:
					* R(A),...,R(A+F-2) := R(A)(R(A+1),...,R(A+B-1))
					*/
					int i, limit, self;
					std::string astr;
					self = 0;

					if (b == 0) {
						limit = a + 1;
						while (PENDING(limit) || IS_VARIABLE(limit)) limit++;
					}
					else {
						limit = a + b;
					}
					clear_sstream(str);
					TRY(ctx, astr = GetR(ctx, F, a));
					if (isIdentifier(astr)) {
						str << astr << "(";
					}
					else {
						auto at = astr.c_str() + astr.length() - 1;
						while (at > astr.c_str() && (luadec_isalnum(*at) || *at == '_')) {
							at--;
						}
						if (*at == ':') {
							self = 1;
							str << astr << "(";
						}
						else {
							str << "(" << astr << ")(";
						}
					}

					for (i = a + 1; i < limit; i++) {
						std::string ireg;
						TRY(ctx, ireg = GetR(ctx, F, i));
						if (ireg == ".end")
							break;
						if (self && i == a + 1)
							continue;
						if (i > a + 1 + self)
							str << ", ";
						if (ireg.length()>0)
							str << ireg;
					}
					str << ")";

					if (c == 0) {
						F->lastCall = a;
					}
					if (GET_OPCODE(code[pc + 1]) == OP_LOADNIL && GETARG_A(code[pc + 1]) == a + 1) {
						auto replaced = "(" + str.str() + ")";
						clear_sstream(str);
						str << replaced;
						c += GETARG_B(code[pc + 1]) - GETARG_A(code[pc + 1]) + 1;
						// ignoreNext = 1;
					}
					if (o == OP_CALL && c == 1) {
						TRY(ctx, AddStatement(ctx, F, str));
					}
					else if (o == OP_TAILCALL || c == 0) {
						IS_VARIABLE(a) = 0; PENDING(a) = 0;
						TRY(ctx, AssignReg(ctx, F, a, str.str().c_str(), 0, 0));
						CALL(a) = 1;
						IS_VARIABLE(a + 1) = 0; PENDING(a + 1) = 0;
						TRY(ctx, AssignReg(ctx, F, a + 1, ".end", 0, 1));
						CALL(a + 1) = 2;
					}
					else {
						PENDING(a) = 0;
						TRY(ctx, AssignReg(ctx, F, a, str.str().c_str(), 0, 0));
						for (i = 0; i < c - 1; i++) {
							CALL(a + i) = i + 1;
						}
					}
					break;
				}
				case OP_RETURN:
				{
					/*
					* Return call. The RETURN opcode works like this: return
					* R(A),...,R(A+B-2)
					*/
					AstStatement* returnstmt = MakeStatement(RETURN_STMT, "");
					int i, limit;

					/* skip the last RETURN */
					if (pc == n - 1)
						break;
					if (b == 0) {
						limit = a;
						while (PENDING(limit) || IS_VARIABLE(limit)) limit++;
					}
					else
						limit = a + b - 1;
					clear_sstream(str);
					for (i = a; i < limit; i++) {
						auto istr = GetR(ctx, F, i);
						if (istr == ".end")
							break;
						if (i > a)
							str << ", ";
						TRY(ctx, str << istr);
					}
					returnstmt->code = str.str();
					TRY(ctx, AddAstStatement(ctx, F, returnstmt));
					break;
				}
				case OP_FORLOOP: //Lua5.1 specific. TODO: CHECK
				{
					AstStatement* currStmt = F->currStmt;

					int i, r_begin, r_end;
					Instruction i_forprep = code[pc + sbc];
					int a_forprep = GETARG_A(i_forprep);
					assert(GET_OPCODE(i_forprep) == OP_FORPREP);
					r_begin = a_forprep;
					r_end = a_forprep + 3;
					for (i = r_begin; i <= r_end; i++)
					{
						IS_VARIABLE(i) = 0;
						F->Rinternal[i] = 0;
					}

					F->ignore_for_variables = 0;

					F->currStmt = LeaveBlock(ctx, F, currStmt, FORLOOP_STMT);
					break;
				}
				case LUADEC_TFORLOOP: // TODO: CHECK
				{
					// 5.1 OP_TFORLOOP	A C		R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));
					//	if R(A+3) ~= nil then R(A+2)=R(A+3) else pc++
					// 5.1 OP_JMP sBx	pc += sBx

					// 5.2 OP_TFORCALL	A C		R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2));
					// 5.2 OP_TFORLOOP	A sBx	if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx }

					AstStatement* currStmt = F->currStmt;

					int i, r_begin, r_end;
					r_begin = a;
					r_end = a + 2 + c;
					for (i = r_begin; i <= r_end; i++)
					{
						IS_VARIABLE(i) = 0;
						F->Rinternal[i] = 0;
					}

					F->ignore_for_variables = 0;

					F->currStmt = LeaveBlock(ctx, F, currStmt, TFORLOOP_STMT);

					ignoreNext = 1;
					break;
				}
				case OP_TFORLOOP:
					break;
				case OP_FORPREP: //Lua5.1 specific. TODO: CHECK
				{
					/*
					* numeric 'for'
					*/
					int i;
					int step;
					std::string idxname;
					std::string initial, a1str, endstr;
					int stepLen;
					AstStatement* forstmt = nullptr;

					TRY(ctx, initial = GetR(ctx, F, a));
					TRY(ctx, endstr = GetR(ctx, F, a + 2));
					TRY(ctx, a1str = GetR(ctx, F, a + 1));

					if (!IS_VARIABLE(a + 3)) {
						int loopvars = 0;
						for (i = 0; i < f->sizelocvars; i++) {
							if (f->locvars[i].startpc == pc + 1) {
								loopvars++;
								//search for the loop variable. Set it's endpc one step further so it will be the same for all loop variables
								if (GET_OPCODE(F->f->code[f->locvars[i].endpc - 1]) == OP_FORLOOP) {
									f->locvars[i].endpc -= 1;
								}
								if (loopvars == 4) {
									idxname = LOCAL(ctx, i);
									break;
								}
							}
						}
						if (idxname.length()<1) {
							idxname = "i";
							TRY(ctx, DeclareVariable(ctx, F, idxname, a + 3));
						}
					}
					else {
						idxname = F->R[a + 3];
					}
					DeclarePendingLocals(ctx, F);
					/*
					* if A argument for FORLOOP is not a known variable,
					* it was declared in the 'for' statement. Look for
					* its name in the locals table.
					*/



					//initial = luadec_strdup(initial);
					step = stoi(REGISTER(a + 2));
					stepLen = (int) (REGISTER(a + 2)).length();
					// findSign = strrchr(initial, '-');

					// if (findSign) {
					//    initial[strlen(initial) - stepLen - 3] = '\0';
					// }

					if (step == 1) {
						clear_sstream(str);
						str << idxname << " = " << initial << ", " << a1str;
					}
					else {
						clear_sstream(str);
						str << idxname << " = " << initial << ", " << a1str << ", " << REGISTER(a + 2);
					}

					/*
					* Every numeric 'for' declares 4 variables.
					*/
					F->Rinternal[a] = 1;
					F->Rinternal[a + 1] = 1;
					F->Rinternal[a + 2] = 1;
					F->Rinternal[a + 3] = 1;

					if (next_child->type != FORLOOP_STMT) {
						fprintf(stderr, "next_child->type != FORLOOP_STMT\n");
					}
					forstmt = next_child->block; // TODO check FORLOOP_STMT
					forstmt->code = str.str();
					AddAstStatement(ctx, F, forstmt);
					F->currStmt = forstmt;
					break;
				}
				case OP_SETLIST:
				{
					if (c == 0) {
						Instruction i_next_arg = code[pc + 1];
						ignoreNext = 1;
						if (GET_OPCODE(i_next_arg) == OP_EXTRAARG) {
							c = GETARG_Ax(i_next_arg);
						}
						else {
							SET_ERROR(ctx, F, "SETLIST with c==0, but not followed by EXTRAARG.");
						}
					}
					TRY(ctx, SetList(ctx, F, a, b, c));
					break;
				}
				case OP_CLOSURE:
				{
					/*
					* Function.
					*/
					int i;
					int uvn;
					int cfnum = ctx->functionnum;
					Proto* cf = f->p[c];
					std::string tmpname;

					uvn = NUPS(cf);

					// always FixUpvalNames
					// lua 5.1 : upvalue names = next n opcodes after CLOSURE
					// lua 5.2 : upvalue names determined by cf->upvalues->instack and cf->upvalues->idx
					for (i = 0; i < uvn; i++) {
						TString* upvalname = UPVAL_NAME(cf, i);
						if (upvalname == nullptr || LUA_STRLEN(upvalname) == 0 ||
							strlen(getstr(upvalname)) == 0 || !isIdentifier(getstr(upvalname))) {
							Upvaldesc upval = cf->upvalues[i];
							int b = upval.idx;

								if (upval.instack == 1) {
									// Rvar[b] is enough
									int locVarIndex = GetLocVarIndex(b, pc);

									if (f->locvars && locVarIndex >= 0 && locVarIndex < f->sizelocvars) {
										// no need to test after FixLocalNames
										upvalname = f->locvars[locVarIndex].varname;
									}
									else {
										tmpname = "l_" + funcnumstr + "_r" + std::to_string(b);
										upvalname = luaS_new(ctx->glstate, tmpname.c_str());
									}
								}
								else if (upval.instack == 0) {
									// Get name from upvalue name
									if (f->upvalues && b < f->sizeupvalues) {
										// no need to check after FixUpvalNames
										upvalname = UPVAL_NAME(f, b);
									}
									else {
										tmpname = "u_" + funcnumstr + "_" + std::to_string(b);
										upvalname = luaS_new(ctx->glstate, tmpname.c_str());
									}
								}
								else {
									tmpname = "up_" + funcnumstr + "_" + std::to_string(i);
									upvalname = luaS_new(ctx->glstate, tmpname.c_str());
								}
								UPVAL_NAME(cf, i) = upvalname;
							}
						}
						/* upvalue determinition end */

						if (func_checking) {
							std::string newfuncnumstr;
							ctx->functionnum = c;
							newfuncnumstr = funcnumstr + "_" + std::to_string(c);
							const auto &code = PrintFunctionOnlyParamsAndUpvalues(ctx, cf, F->indent, newfuncnumstr);
							clear_sstream(str);
							str << code;
						}
						else if (!ctx->process_sub) {
							clear_sstream(str);
							str << "DecompiledFunction_" << funcnumstr << "_" << c;
						}
						else {
							std::string newfuncnumstr;
							ctx->functionnum = c;
							newfuncnumstr = funcnumstr + "_" + std::to_string(c);
							const auto &code = ProcessCode(ctx, cf, F->indent, 0, newfuncnumstr);
							clear_sstream(str);
							str << code;
						}
						TRY(ctx, AssignReg(ctx, F, a, str.str().c_str(), 0, 0));

						break;
					}
				default:
					char ostr_tmp[100];
					memset(ostr_tmp, 0x0, sizeof(ostr_tmp));
					sprintf(ostr_tmp, "-- unhandled opcode? : 0x%02X", o);
					ostr_tmp[99] = '\0';
					clear_sstream(str);
					str << ostr_tmp;
					TRY(ctx, AddStatement(ctx, F, str));
					break;
				}

				if (ctx->debug) {
					TRY(ctx, ShowState(ctx, F));
				}

				TRY(ctx, OutputAssignments(ctx, F));
				}

				TRY(ctx, FlushBoolean(ctx, F));

				if (SET_SIZE(F->tpend)>0) {
					clear_sstream(str);
					str << "-- WARNING: undefined locals caused missing assignments!";
					TRY(ctx, AddStatement(ctx, F, str));
				}

				if (F->jmpdests.size > 0) {
					clear_sstream(str);
					str << "-- DECOMPILER ERROR: " << F->jmpdests.size << " unprocessed JMP targets";
					TRY(ctx, AddStatement(ctx, F, str));
					while (F->jmpdests.head) {
						AstStatement* jmpdest = lua_cast(AstStatement*, RemoveFromList(&(F->jmpdests), F->jmpdests.head));
						AddToStatement(F->currStmt, jmpdest);
					}
				}

				if (!IsMain(ctx, f)) {
					F->indent--;
				}

				output = PrintFunction(ctx, F);
				DeleteFunction(F);
				return output;

			errorHandler:
				printf("ERRORHANDLER\n");
				{
					AstStatement* stmt;
					clear_sstream(str);
					str << "--[[ DECOMPILER ERROR " << ctx->error_code << ": " << ctx->error << " ]]";
					stmt = MakeSimpleStatement(str.str());
					stmt->line = F->pc;
					AddToStatement(F->currStmt, stmt);
					F->lastLine = F->pc;
				}
				output = PrintFunction(ctx, F);
				DeleteFunction(F);
				ctx->error = "";
				return output;
			}

			std::string luaU_decompile(GluaDecompileContextP ctx, Proto* f, int dflag) {
				// TODO: 语法尽量做成类似glua的语法
				// char* code;
				ctx->debug = dflag;
				ctx->functionnum = 0;
				
				ctx->reset_error();
				auto code = ProcessCode(ctx, f, 0, 0, std::string("0"));
				ctx->reset_error();
				std::string code_str(code);
				fflush(stdout);
				fflush(stderr);
				return code_str;
			}

			Proto* findSubFunction(GluaDecompileContextP ctx, Proto* f, std::string funcnumstr, std::string &realfuncnumstr) {
				Proto* cf = f;
				auto startstr = funcnumstr;
				const char* endstr;

				int c = stoi(startstr);
				if (c != 0) {
					return nullptr;
				}
				endstr = strchr(startstr.c_str(), '_');
				startstr = endstr + 1;
				realfuncnumstr = "0";
				ctx->functionnum = 0;

				while (!(endstr == nullptr)) {
					c = stoi(startstr);
					if (c < 0 || c >= cf->sizep) {
						return nullptr;
					}
					cf = cf->p[c];
					endstr = strchr(startstr.c_str(), '_');
					startstr = endstr + 1;
					realfuncnumstr = realfuncnumstr + "_" + std::to_string(c);
					ctx->functionnum = c + 1;
				}
				return cf;
			}

			std::string ProcessSubFunction(GluaDecompileContextP ctx, Proto* cf, int func_checking, std::string funcnumstr) {
				int i;
				int uvn = NUPS(cf);
				std::stringstream buff;
				std::string tmpname;

				/* determining upvalues */
				if (!cf->upvalues) {
					// Lua 5.1 only
					cf->sizeupvalues = uvn;
					cf->upvalues = luaM_newvector(ctx->glstate, uvn, UPVAL_TYPE);
					for (i = 0; i < uvn; i++) {
						tmpname = std::string("upval_") + funcnumstr + "_" + std::to_string(i);
						UPVAL_NAME(cf, i) = luaS_new(ctx->glstate, tmpname.c_str());
					}
				}
				else {
					// FixUpvalNames
					for (i = 0; i < uvn; i++) {
						TString* name = UPVAL_NAME(cf, i);
						if (name == nullptr || LUA_STRLEN(name) == 0 ||
							strlen(getstr(name)) == 0 || !isIdentifier(getstr(name))) {
							// TODO 5.2 Maybe we should trace up to get _ENV ?
							// Also wen can get the location where upval defined
							tmpname = std::string("upval_") + funcnumstr + "_" + std::to_string(i);
							UPVAL_NAME(cf, i) = luaS_new(ctx->glstate, tmpname.c_str());
						}
					}
				}

				clear_sstream(buff);
				if (!IsMain(ctx, cf)) {
					if (NUPS(cf) > 0) {
						buff << "local ";
						listUpvalues(ctx, cf, buff);
						buff << "\n";
					}
					buff << "DecompiledFunction_" << funcnumstr << " = ";
				}
				std::string code = ProcessCode(ctx, cf, 0, func_checking, funcnumstr);

				buff << code << "\n";
				code = buff.str();
				return code;
			}

			void luaU_decompileSubFunction(GluaDecompileContextP ctx, Proto* f, int dflag, std::string funcnumstr) {
				std::string realfuncnumstr;;

				Proto* cf = findSubFunction(ctx, f, funcnumstr, realfuncnumstr);
				if (!cf) {
					fprintf(stderr, "No such sub function num : %s , use -pn option to get available num.\n", funcnumstr.c_str());
					return;
				}
				
				ctx->reset_error();
				const auto &code = ProcessSubFunction(ctx, cf, 0, realfuncnumstr);
				ctx->reset_error();
				printf("%s\n", code.c_str());
				fflush(stdout);
				fflush(stderr);
			}

			}
}