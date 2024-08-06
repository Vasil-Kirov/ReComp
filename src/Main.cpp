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
#include "Interpreter.h"
#include "x64CodeWriter.h"
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
#include "Interpreter.cpp"
#include "x64CodeWriter.cpp"
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

struct timers
{
	timer_group Parse;
	timer_group TypeCheck;
	timer_group IR;
	timer_group LLVM;
};

void ResolveSymbols(dynamic<file> Files)
{
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		AnalyzeForModuleStructs(NodeSlice, File->Module);
		*File->Checker = AnalyzeFunctionDecls(NodeSlice, &File->Module);
	}
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		ForArray(j, File->Imported)
		{
			ForArray(k, Files)
			{
				file MaybeMod = Files[k];
				if(MaybeMod.Module.Name == File->Imported[j].Name)
				{
					string As = File->Imported[j].As;
					File->Imported.Data[j] = MaybeMod.Module;
					File->Imported.Data[j].As = As;
				}
			}
		}
		File->Checker->Imported = &File->Imported;
	}
}

file GetModule(string File, timers *Timers)
{
	string FileData = ReadEntireFile(File);

	if(FileData.Data == NULL)
	{
		LFATAL("Couldn't find file: %s", File.Data);
	}

	error_info ErrorInfo = {};
	ErrorInfo.Data = DupeType(FileData, string);
	ErrorInfo.FileName = File.Data;
	ErrorInfo.Line = 1;
	ErrorInfo.Character = 1;

	Timers->Parse = VLibStartTimer("Parsing");
	file Result = StringToTokens(FileData, ErrorInfo);
	parse_result Parse = ParseTokens(Result.Tokens, Result.Module.Name);
	Result.Nodes = Parse.Nodes;
	Result.Imported = Parse.Imports;
	Result.Checker = NewType(checker);
	Result.Checker->Module = &Result.Module;
	VLibStopTimer(&Timers->Parse);
	return Result;
}

void ParseAndAnalyzeFile(file *File, timers *Timers)
{
	Timers->TypeCheck = VLibStartTimer("Type Checking");
	Analyze(File->Checker, SliceFromArray(File->Nodes));
	VLibStopTimer(&Timers->TypeCheck);

	Timers->IR = VLibStartTimer("Intermediate Representation Generation");
	File->IR = NewType(ir);
	*File->IR = BuildIR(File);
	VLibStopTimer(&Timers->IR);
	
#if 0
	string Dissasembly = Dissasemble(SliceFromArray(IR.Functions));
	LDEBUG("%s", Dissasembly.Data);
#endif
}

struct compile_info
{
	const char *FileNames[1024];
	i32 FileCount;
};

file CompileBuildFile(string Name, timers *Timers)
{
	file File = GetModule(Name, Timers);
	auto NodeSlice = SliceFromArray(File.Nodes);
	AnalyzeForModuleStructs(NodeSlice, File.Module);
	*File.Checker = AnalyzeFunctionDecls(NodeSlice, &File.Module);
	File.Checker->Imported = &File.Imported;
	ParseAndAnalyzeFile(&File, Timers);

	return File;
}

int
main(int ArgCount, char *Args[])
{
	InitVLib();

	InitializeLogger();
	InitializeLexer();

	if(ArgCount < 2)
	{
		LFATAL("Expected arguments");
	}

	auto kernel32 = LoadLibrary("kernel32");
	auto user32   = LoadLibrary("user32");
	auto ntdll    = LoadLibrary("ntdll");
	auto msvcrt   = LoadLibrary("msvcrt");
	auto ucrtbase = LoadLibrary("ucrtbase");
	auto testdll  = LoadLibrary("testdll");
	HMODULE DLLs[] = {
		kernel32,
		user32,
		ntdll,
		msvcrt,
		ucrtbase,
		testdll,
	};

	auto CompileFunction = STR_LIT("compile");
	b32 FoundCompile = false;

	type *FileArray = NewType(type);
	FileArray->Kind = TypeKind_Array;
	FileArray->Array.Type = Basic_cstring;
	FileArray->Array.MemberCount = 1024;
	u32 FileArrayType = AddType(FileArray);

	struct_member CompileInfoMembers[] = {
		{STR_LIT("files"), FileArrayType},
		{STR_LIT("file_count"), Basic_i32},
	};

	type *CompileInfoType = NewType(type);
	CompileInfoType->Kind = TypeKind_Struct;
	CompileInfoType->Struct.Name = STR_LIT("CompileInfo");
	CompileInfoType->Struct.Members = {CompileInfoMembers, ARR_LEN(CompileInfoMembers)};
	CompileInfoType->Struct.Flags = 0;
	
	u32 CompileInfo = AddType(CompileInfoType);

	dynamic<timers> Timers = {};
	timers BuildTimers = {};
	file BuildFile = CompileBuildFile(MakeString(Args[1]), &BuildTimers);
	Timers.Push(BuildTimers);

	timer_group VMBuildTimer = VLibStartTimer("VM");

	interpreter VM = MakeInterpreter(BuildFile.IR->GlobalSymbols, BuildFile.IR->MaxRegisters, DLLs, ARR_LEN(DLLs));

	ForArray(Idx, BuildFile.IR->Functions)
	{
		if(*BuildFile.IR->Functions[Idx].Name == CompileFunction)
		{
			if(BuildFile.IR->Functions[Idx].Blocks.Count == 0)
			{
				LFATAL("compile function doesn't have a body");
			}
			FoundCompile = true;

			value Out = {};
			Out.Type = GetPointerTo(CompileInfo);
			Out.ptr = VAlloc(GetTypeSize(CompileInfoType));

			interpret_result Result = InterpretFunction(&VM, BuildFile.IR->Functions[Idx], {&Out, 1});

			dynamic<file> Files = {};
			compile_info *Info = (compile_info *)Out.ptr;
			for(int i = 0; i < Info->FileCount; ++i)
			{
				timers FileTimer = {};
				file File = GetModule(MakeString(Info->FileNames[i]),
						&FileTimer);
				Files.Push(File);
			}
			ResolveSymbols(Files);
			ForArray(Idx, Files)
			{
				timers FileTimer = {};
				file *File = &Files.Data[Idx];
				ParseAndAnalyzeFile(File, &FileTimer);
				Timers.Push(FileTimer);
			}
			timers LLVMTimers = {};
			LLVMTimers.LLVM = VLibStartTimer("LLVM");
			RCGenerateCode(SliceFromArray(Files), true);
			VLibStopTimer(&LLVMTimers.LLVM);
			Timers.Push(LLVMTimers);


			if(Result.ToFreeStackMemory)
				VFree(Result.ToFreeStackMemory);
		}
	}

	if(!FoundCompile)
	{
		LFATAL("File %s doesn't have the `compile` function defined, this function is used to define how to build the program", Args[1]);
	}

	VLibStopTimer(&VMBuildTimer);



	auto LinkTimer = VLibStartTimer("Linking");
	//system("LINK.EXE /nologo /ENTRY:mainCRTStartup /defaultlib:libcmt /OUT:a.exe out.obj");
	VLibStopTimer(&LinkTimer);


	i64 ParseTime = 0;
	i64 TypeCheckTime = 0;
	i64 IRBuildTime = 0;
	i64 LLVMTimer = 0;

	ForArray(Idx, Timers)
	{
		ParseTime     += TimeTaken(&Timers.Data[Idx].Parse);
		TypeCheckTime += TimeTaken(&Timers.Data[Idx].TypeCheck);
		IRBuildTime   += TimeTaken(&Timers.Data[Idx].IR);
		LLVMTimer     += TimeTaken(&Timers.Data[Idx].LLVM);
	}

	if(ArgCount > 2 && MakeString(Args[2]) == STR_LIT("--time"))
	{
		LDEBUG("Compiling Finished...");
		LDEBUG("Parsing:                   %lldms", ParseTime                / 1000);
		LDEBUG("Type Checking:             %lldms", TypeCheckTime            / 1000);
		LDEBUG("Intermediate Generation:   %lldms", IRBuildTime              / 1000);
		LDEBUG("Interpreting Build File:   %lldms", TimeTaken(&VMBuildTimer) / 1000);
		LDEBUG("LLVM Code Generation:      %lldms", LLVMTimer                / 1000);
		LDEBUG("Linking:                   %lldms", TimeTaken(&LinkTimer)    / 1000);
	}

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

