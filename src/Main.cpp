#include "Memory.h"
static b32 _MemoryInitializer = InitializeMemory();

#include "Log.h"
#include "String.h"
#include "Platform.h"
#include "Lexer.h"
#include "Errors.h"
#include "Parser.h"
#include "Semantics.h"
#include "Type.h"
#include "IR.h"
#include "Threading.h"
#include "backend/LLVMFileOutput.h"
#include "backend/LLVMFileCast.h"

#include "Memory.cpp"
#include "String.cpp"
#include "Log.cpp"
#include "Lexer.cpp"
#include "Errors.cpp"
#include "Parser.cpp"
#include "Semantics.cpp"
#include "Type.cpp"
#include "IR.cpp"
#include "Threading.cpp"
#include "backend/LLVMFileOutput.cpp"
#include "backend/LLVMFileCast.cpp"

#if defined(_WIN32)
#include "Win32.cpp"
#else
#error unsupported platform
#endif

int
main(int ArgCount, char *Args[])
{
	InitializeLogger();
	InitializeLexer();

	if(ArgCount < 2)
	{
		LFATAL("Expected arguments");
	}

	string FileData = ReadEntireFile(MakeString(Args[1]));

	error_info ErrorInfo = {};
	ErrorInfo.Data = &FileData;
	ErrorInfo.FileName = Args[1];
	ErrorInfo.Line = 1;
	ErrorInfo.Character = 1;

	token *Tokens = StringToTokens(FileData, ErrorInfo);
	node **Nodes = ParseTokens(Tokens);
	Analyze(Nodes);
	ir IR = BuildIR(Nodes);
	string Dissasembly = Dissasemble(IR.Functions, ArrLen(IR.Functions));
	LDEBUG("%s", Dissasembly.Data);

	LLVMFileOutput(&IR);

	return 0;
}

const char* GetTokenName(token_type Token) {
    switch (Token) {
        case T_EOF:   return "Token End of File";
        case T_ID:    return "Token Identifier";
        case T_IF:    return "Token If";
        case T_FOR:   return "Token For";
        case T_NUM:   return "Token Number";
        case T_STR:   return "Token String";
        case T_NEQ:   return "Token Not Equal";
        case T_GEQ:   return "Token Greater or Equal";
        case T_LEQ:   return "Token Less or Equal";
        case T_EQEQ:  return "Token Equal Equal";
        case T_ARR:   return "Token Arrow";
        case T_PPLUS: return "Token Plus Plus";
        case T_MMIN:  return "Token Minus Minus";
        default:
		{
			char *C = AllocateString(2);
			C[0] = (char)Token;
			C[1] = 0;
			return C;
		}
    }
}

