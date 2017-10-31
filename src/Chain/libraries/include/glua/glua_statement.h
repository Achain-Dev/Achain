#ifndef glua_statement_h
#define glua_statement_h

// Abstract syntax tree, Statements tree
#include <glua/glua_structs.h>
#include <glua/glua_proto_info.h>
#include <string>
#include <sstream>
#include <vector>

namespace glua {
	namespace decompile {

		enum StatementType_ {
			SIMPLE_STMT,
			BREAK_STMT,
			RETURN_STMT,
			FUNCTION_STMT,
			DO_STMT,
			WHILE_STMT,
			REPEAT_STMT,
			FORLOOP_STMT,
			TFORLOOP_STMT,
			IF_STMT,
			IF_THEN_STMT,
			IF_ELSE_STMT,
			JMP_DEST_STMT // virtual statement, mark for JMP destination, some of the statements will be printed as label in 5.2
		};
		typedef enum StatementType_ StatementType;

		extern const char* const stmttype[];

		class AstStatement {
		public:
			ListItem super;
			AstStatement* parent;
			StatementType type;
			std::string code;
			List* sub;
			int line;
			int sub_print_count;
			int comment_print_count;
		public:
			inline AstStatement() :parent(nullptr), sub(nullptr), line(0), sub_print_count(0), comment_print_count(0) {}
		};

		AstStatement* MakeStatement(StatementType type, std::string code);
		AstStatement* MakeSimpleStatement(std::string code);
		AstStatement* MakeBlockStatement(StatementType type, std::string code);
		AstStatement* MakeIfStatement(std::string test);

#define ThenStmt(ifstmt) lua_cast(AstStatement*, (ifstmt)->sub->head)
#define ElseStmt(ifstmt) lua_cast(AstStatement*, (ifstmt)->sub->tail)
#define ThenStart(ifstmt) (lua_cast(AstStatement*, (ifstmt)->sub->head)->line)
#define ElseStart(ifstmt) (lua_cast(AstStatement*, (ifstmt)->sub->tail)->line)

		void ClearAstStatement(AstStatement* stmt, void* dummy);
		void DeleteAstStatement(AstStatement* stmt);

		void PrintIndent(std::stringstream &buff, int indent);
		void PrintAstStatement(GluaDecompileContextP ctx, AstStatement* stmt, std::stringstream &buff, int indent);
		void PrintAstSub(GluaDecompileContextP ctx, AstStatement* blockstmt, std::stringstream &buff, int indent);

		void AddToStatement(AstStatement* stmt, AstStatement* sub);

	}
}

#endif // #ifndef glua_statement_h
