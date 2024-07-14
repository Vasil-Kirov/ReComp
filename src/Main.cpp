#include "Basic.h"
#include "Memory.h"
#include "vlib.h"
static b32 _MemoryInitializer = InitializeMemory();

#include "Log.h"
#include "VString.h"
#include "Platform.h"
#include "Lexer.h"
#include "Errors.h"
#include "Parser.h"
#include "Semantics.h"
#include "Type.h"
#include "IR.h"
#include "Threading.h"
#if 0
#include "backend/LLVMFileOutput.h"
#include "backend/LLVMFileCast.h"
#else

#include "backend/LLVMC/LLVMBase.h"
#include "backend/LLVMC/LLVMType.h"
#include "backend/LLVMC/LLVMValue.h"

#endif
#include "ConstVal.h"

#include "Memory.cpp"
#include "VString.cpp"
#include "Log.cpp"
#include "Lexer.cpp"
#include "Errors.cpp"
#include "Parser.cpp"
#include "Semantics.cpp"
#include "Type.cpp"
#include "IR.cpp"
#include "Threading.cpp"
#if 0
#include "backend/LLVMFileOutput.cpp"
#include "backend/LLVMFileCast.cpp"
#else

#include "backend/LLVMC/LLVMBase.cpp"
#include "backend/LLVMC/LLVMType.cpp"
#include "backend/LLVMC/LLVMValue.cpp"

#endif
#include "ConstVal.cpp"

#if defined(_WIN32)
#include "Win32.cpp"
#else
#error unsupported platform
#endif

int
main(int ArgCount, char *Args[])
{
	InitVLib();

	auto InitTimer = VLibStartTimer("Init");
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
	VLibStopTimer(&InitTimer);

	auto ParseTimer = VLibStartTimer("Parsing");
	token *Tokens = StringToTokens(FileData, ErrorInfo);
	node **Nodes = ParseTokens(Tokens);
	VLibStopTimer(&ParseTimer);

	auto TypeCheckTimer = VLibStartTimer("Type Checking");
	Analyze(Nodes);
	VLibStopTimer(&TypeCheckTimer);

	auto IRBuildTimer = VLibStartTimer("Intermediate Representation Generation");
	ir IR = BuildIR(Nodes);
	VLibStopTimer(&IRBuildTimer);
	
#if 0
	string Dissasembly = Dissasemble(IR.Functions);
	LDEBUG("%s", Dissasembly.Data);
#endif

	auto LLVMTimer = VLibStartTimer("LLVM Code Generation");
	RCGenerateCode(&IR);
	VLibStopTimer(&LLVMTimer);

	auto LinkTimer = VLibStartTimer("Linking");
	system("LINK.EXE /nologo /ENTRY:mainCRTStartup /defaultlib:libcmt /OUT:a.exe out.obj");
	VLibStopTimer(&LinkTimer);


	LDEBUG("Compiling Finished...");
	LDEBUG("Initialization:            %lldms", TimeTaken(&InitTimer)      / 1000);
	LDEBUG("Parsing:                   %lldms", TimeTaken(&ParseTimer)     / 1000);
	LDEBUG("Type Checking:             %lldms", TimeTaken(&TypeCheckTimer) / 1000);
	LDEBUG("Intermediate Generation:   %lldms", TimeTaken(&IRBuildTimer)   / 1000);
	LDEBUG("LLVM Code Generation:      %lldms", TimeTaken(&LLVMTimer)      / 1000);
	LDEBUG("Linking:                   %lldms", TimeTaken(&LinkTimer)      / 1000);

	return 0;
}

const char* GetTokenName(token_type Token) {
    switch (Token) {
        case T_EOF:   return "End of File";
        case T_ID:    return "Identifier";
        case T_IF:    return "if";
        case T_FOR:   return "for";
        case T_VAL:   return "Number";
        case T_STR:   return "String";
        case T_NEQ:   return "!=";
        case T_GEQ:   return ">=";
        case T_LEQ:   return "<=";
        case T_EQEQ:  return "==";
        case T_ARR:   return "->";
        case T_PPLUS: return "++";
        case T_MMIN:  return "--";
        default:
		{
			char *C = AllocateString(2);
			C[0] = (char)Token;
			C[1] = 0;
			return C;
		}
    }
}

