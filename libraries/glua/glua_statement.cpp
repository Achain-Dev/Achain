#include <glua/glua_common.h>

#include <stdlib.h>

#include <glua/glua_statement.h>
#include <glua/llimits.h>
#include <glua/glua_proto_info.h>

namespace glua {
	namespace decompile {


		const char* const stmttype[] = {
			"SIMPLE_STMT",
			"BREAK_STMT",
			"RETURN_STMT",
			"FUNCTION_STMT",
			"DO_STMT",
			"WHILE_STMT",
			"REPEAT_STMT",
			"FORLOOP_STMT",
			"TFORLOOP_STMT",
			"IF_STMT",
			"IF_THEN_STMT",
			"IF_ELSE_STMT",
			"JMP_DEST_STMT"
		};

		void PrintIndent(std::stringstream &buff, int indent) {
			int i = 0;
			for (i = 0; i < indent; i++) {
				buff << "  ";
			}
		}

		void PrintAstSub(GluaDecompileContextP ctx, AstStatement* blockstmt, std::stringstream &buff, int indent) {
			List* sub = blockstmt->sub;
			ListItem* walk;
			if (sub == NULL) {
				return;
			}
			walk = sub->head;
			while (walk) {
				PrintAstStatement(ctx, lua_cast(AstStatement*, walk), buff, indent);
				blockstmt->sub_print_count++;
				walk = walk->next;
			}
		}

		AstStatement* MakeStatement(StatementType type, std::string code) {
			auto stmt = new AstStatement();
			stmt->type = type;
			stmt->code = code;
			stmt->sub = nullptr;
			stmt->sub_print_count = 0;
			stmt->comment_print_count = 0;
			return stmt;
		}

		AstStatement* MakeSimpleStatement(std::string code) {
			AstStatement* stmt = MakeStatement(SIMPLE_STMT, code);
			return stmt;
		}

		AstStatement* MakeBlockStatement(StatementType type, std::string code) {
			AstStatement* stmt = MakeStatement(type, code);
			stmt->sub = NewList();
			return stmt;
		}

		AstStatement* MakeIfStatement(std::string test) {
			AstStatement* ifstmt = MakeBlockStatement(IF_STMT, test);
			AddToStatement(ifstmt, MakeBlockStatement(IF_THEN_STMT, ""));
			AddToStatement(ifstmt, MakeBlockStatement(IF_ELSE_STMT, ""));
			return ifstmt;
		}

		void ClearAstStatement(AstStatement* stmt, void* dummy) {
			if (stmt == NULL) {
				return;
			}
			stmt->type = SIMPLE_STMT;
			stmt->line = 0;
			stmt->code.clear();

			if (stmt->sub) {
				ClearList(stmt->sub, (ListItemFn)ClearAstStatement);
				delete stmt->sub;
				stmt->sub = nullptr;
			}
			stmt->sub_print_count = 0;
			stmt->comment_print_count = 0;
		}

		void DeleteAstStatement(AstStatement* stmt) {
			ClearAstStatement(stmt, NULL);
			delete stmt;
		}

		void PrintSimpleStatement(AstStatement* stmt, std::stringstream &buff, int indent) {
			AstStatement* parent = stmt->parent;
			// start with '(' and not the first statement of the block
			if (parent->sub_print_count > 0 && stmt->code[0] == '(') {
				PrintIndent(buff, indent);
				buff << ";\n";
			}
			PrintIndent(buff, indent);
			buff << stmt->code << "\n";
			if (strncmp("--", stmt->code.c_str(), 2) == 0) {
				parent->sub_print_count--;
				parent->comment_print_count++;
			}
		}

		void PrintBreakStatement(AstStatement* stmt, std::stringstream &buff, int indent) {
			AstStatement* parent = stmt->parent;
			// "break;" and not the last statement of the block
			if ((parent->sub_print_count + parent->comment_print_count + 1) < parent->sub->size) {
				PrintIndent(buff, indent);
				buff << "do break end\n";
				// StringBuffer_addPrintf(buff, "do break end\n", stmt->code.c_str());
			}
			else {
				PrintIndent(buff, indent);
				buff << "break\n";
				// StringBuffer_addPrintf(buff, "break\n", stmt->code.c_str());
			}
		}

		void PrintReturnStatement(AstStatement* stmt, std::stringstream &buff, int indent) {
			AstStatement* parent = stmt->parent;
			// "return" and not the last statement of the block
			if ((parent->sub_print_count + parent->comment_print_count + 1) < parent->sub->size) {
				PrintIndent(buff, indent);
				buff << "do return " << stmt->code << " end\n";
			}
			else {
				PrintIndent(buff, indent);
				buff << "return " << stmt->code << "\n";
			}
		}

		void PrintBlockStatement(GluaDecompileContextP ctx, AstStatement* stmt, std::stringstream &buff, int indent) {
			std::stringstream startCode;
			std::stringstream endCode;
			switch (stmt->type) {
			case DO_STMT:
				startCode << "do";
				endCode << "end";
				break;
			case FUNCTION_STMT:
				startCode << "function(" << stmt->code << ")";
				endCode << "end";
				break;
			case WHILE_STMT:
				startCode << "while " << stmt->code << " do";
				endCode << "end";
				break;
			case REPEAT_STMT:
				startCode << "repeat";
				endCode << "until " << stmt->code;
				break;
			case FORLOOP_STMT:
				startCode << "for " << stmt->code << " do";
				endCode << "end";
				break;
			case TFORLOOP_STMT:
				startCode << "for " << stmt->code << " do";
				endCode << "end";
				break;
			default:
				PrintIndent(buff, indent);
				buff << "--  DECOMPILER ERROR: unexpected statement " << stmttype[stmt->type] << " , should be one of LOOP_STMT\n";
				goto PrintLoopStatement_ERROR_HANDLER;
			}
			PrintIndent(buff, indent);
			buff << startCode.str() << "\n";
			PrintAstSub(ctx, stmt, buff, indent + 1);
			PrintIndent(buff, indent);
			buff << endCode.str() << "\n";
		PrintLoopStatement_ERROR_HANDLER:
			(0);
		}

		void PrintIfStatement(GluaDecompileContextP ctx, AstStatement* stmt, std::stringstream &buff, int indent, int elseif) {
			AstStatement* thenstmt = lua_cast(AstStatement*, stmt->sub->head);
			List* thensub = thenstmt->sub;
			AstStatement* elsestmt = lua_cast(AstStatement*, stmt->sub->tail);
			List* elsesub = elsestmt->sub;
			int elsesize = elsesub->size;
			AstStatement* elsefirst = lua_cast(AstStatement*, elsesub->head);
			if (elseif) {
				PrintIndent(buff, indent);
				buff << "elseif " << stmt->code << " then\n";
			}
			else {
				PrintIndent(buff, indent);
				buff << "if " << stmt->code << " then\n";
			}
			if (ctx->debug) {
				PrintIndent(buff, indent + 1);
				buff << "-- AstStatement type=IF_THEN_STMT line=" << thenstmt->line << " size=" << thensub->size << "\n";
			}
			PrintAstSub(ctx, thenstmt, buff, indent + 1);
			if (elsesize == 0) {
				PrintIndent(buff, indent);
				buff << "end\n";
			}
			else if (elsesize == 1 && elsefirst->type == IF_STMT) {
				PrintIfStatement(ctx, elsefirst, buff, indent, 1);
			}
			else {
				PrintIndent(buff, indent);
				buff << "else\n";
				if (ctx->debug) {
					PrintIndent(buff, indent + 1);
					buff << "-- AstStatement type=IF_ELSE_STMT line=" << elsestmt->line << " size=" << elsesize << "\n";
				}
				PrintAstSub(ctx, elsestmt, buff, indent + 1);
				PrintIndent(buff, indent);
				buff << "end\n";
			}
		}

		void PrintJmpDestStatement(GluaDecompileContextP ctx, AstStatement* stmt, std::stringstream &buff, int indent) {
			if (ctx->debug) {
				ListItem* jmpitem = NULL;
				PrintIndent(buff, indent);
				buff << "-- JMP destination in line " << stmt->line << ", jump from line";
				jmpitem = stmt->sub->head;
				while (jmpitem) {
					AstStatement* jmpstmt = lua_cast(AstStatement*, jmpitem);
					buff << " " << jmpstmt->line;
					jmpitem = jmpitem->next;
				}
				buff << "\n";
			}
			if (stmt->code.length() > 0) {
				PrintIndent(buff, indent);
				buff << "::pc_" << stmt->line << "::\n";
			}
		}

		void PrintAstStatement(GluaDecompileContextP ctx, AstStatement* stmt, std::stringstream &buff, int indent) {
			switch (stmt->type) {
			case SIMPLE_STMT:
				PrintSimpleStatement(stmt, buff, indent);
				break;
			case BREAK_STMT:
				PrintBreakStatement(stmt, buff, indent);
				break;
			case RETURN_STMT:
				PrintReturnStatement(stmt, buff, indent);
				break;
			case DO_STMT:
			case FUNCTION_STMT:
			case WHILE_STMT:
			case REPEAT_STMT:
			case FORLOOP_STMT:
			case TFORLOOP_STMT:
				PrintBlockStatement(ctx, stmt, buff, indent);
				break;
			case IF_STMT:
				PrintIfStatement(ctx, stmt, buff, indent, 0);
				break;
			case IF_THEN_STMT:
			case IF_ELSE_STMT:
				PrintIndent(buff, indent);
				buff << "--  DECOMPILER ERROR: unexpected statement " << stmttype[stmt->type] << "\n";
				break;
			case JMP_DEST_STMT:
				PrintJmpDestStatement(ctx, stmt, buff, indent);
				break;
			}
		}

		void AddToStatement(AstStatement* stmt, AstStatement* sub) {
			if (stmt && sub) {
				sub->parent = stmt;
				AddToList(stmt->sub, (ListItem*)sub);
			}
		}

	}
}