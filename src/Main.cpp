#include "Basic.h"
#include "Memory.h"
#include "vlib.h"
#include "llvm-c/TargetMachine.h"
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
#include "CommandLine.h"
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
#include "CommandLine.cpp"
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
	b32 FoundMain = false;
	string MainName = STR_LIT("main");
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		if(File->Module.Name == MainName)
		{
			FoundMain = true;
			b32 FoundMainMain = false;
			ForArray(mi, File->Module.Globals)
			{
				symbol *sym = File->Module.Globals[mi];
				if(sym->Flags & SymbolFlag_Function &&
						*sym->Name == MainName)
				{
					FoundMainMain = true;
					break;
				}
			}
			if(!FoundMainMain)
			{
				LFATAL("Missing main function in main module");
			}
			break;
		}
	}

	if(!FoundMain)
	{
		LFATAL("Missing main module");
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
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		AnalyzeDefineStructs(File->Checker, SliceFromArray(File->Nodes));
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

void ParseAndAnalyzeFile(file *File, timers *Timers, uint Flag)
{
	Timers->TypeCheck = VLibStartTimer("Type Checking");
	Analyze(File->Checker, SliceFromArray(File->Nodes));
	VLibStopTimer(&Timers->TypeCheck);

	Timers->IR = VLibStartTimer("Intermediate Representation Generation");
	File->IR = NewType(ir);
	*File->IR = BuildIR(File);
	VLibStopTimer(&Timers->IR);
	
	if(Flag & CommandFlag_ir)
	{
		string Dissasembly = Dissasemble(SliceFromArray(File->IR->Functions));
		LDEBUG("[ MODULE %s ]\n\n%s", File->Module.Name.Data, Dissasembly.Data);
	}
}

string MakeLinkCommand(command_line CMD, slice<file> Files)
{
	string_builder Builder = MakeBuilder();
	Builder += "LINK.EXE /nologo /defaultlib:MSVCRT /ENTRY:mainCRTStartup /OUT:a.exe !internal.obj ";

	ForArray(Idx, Files)
	{
		Builder += Files[Idx].Module.Name;
		Builder += ".obj ";
	}

	ForArray(Idx, CMD.LinkArgs)
	{
		Builder += CMD.LinkArgs[Idx];
		Builder += ' ';
	}


	return MakeString(Builder);
}

struct compile_info
{
	const char *FileNames[1024];
	i32 FileCount;
};

file CompileBuildFile(string Name, timers *Timers, u32 *CompileInfoTypeIdx)
{
	type *FileArray = NewType(type);
	FileArray->Kind = TypeKind_Array;
	FileArray->Array.Type = Basic_cstring;
	FileArray->Array.MemberCount = 1024;

	u32 FileArrayType = AddType(FileArray);
	static struct_member CompileInfoMembers[] = {
		{STR_LIT("files"), FileArrayType},
		{STR_LIT("file_count"), Basic_i32},
	};

	file File = GetModule(Name, Timers);
	string_builder CompileInfoName = MakeBuilder();
	CompileInfoName += File.Module.Name;
	CompileInfoName += STR_LIT("!CompileInfo");

	type *CompileInfoType = NewType(type);
	CompileInfoType->Kind = TypeKind_Struct;
	CompileInfoType->Struct.Name = MakeString(CompileInfoName);
	CompileInfoType->Struct.Members = {CompileInfoMembers, ARR_LEN(CompileInfoMembers)};
	CompileInfoType->Struct.Flags = 0;
	
	u32 CompileInfo = AddType(CompileInfoType);
	*CompileInfoTypeIdx = CompileInfo;

	auto NodeSlice = SliceFromArray(File.Nodes);
	AnalyzeForModuleStructs(NodeSlice, File.Module);
	*File.Checker = AnalyzeFunctionDecls(NodeSlice, &File.Module);
	File.Checker->Imported = &File.Imported;
	ParseAndAnalyzeFile(&File, Timers, 0);

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

	command_line CommandLine = ParseCommandLine(ArgCount, Args);

	HMODULE DLLs[256] = {};
	int DLLCount = 0;

	DLLs[DLLCount++] = LoadLibrary("kernel32");
	DLLs[DLLCount++] = LoadLibrary("user32");
	DLLs[DLLCount++] = LoadLibrary("ntdll");
	DLLs[DLLCount++] = LoadLibrary("msvcrt");
	DLLs[DLLCount++] = LoadLibrary("ucrtbase");
	ForArray(Idx, CommandLine.ImportDLLs)
	{
		DLLs[DLLCount++] = LoadLibrary(CommandLine.ImportDLLs[Idx].Data);
	}

	auto CompileFunction = STR_LIT("compile");
	b32 FoundCompile = false;

	dynamic<timers> Timers = {};
	timers BuildTimers = {};
	u32 CompileInfo;
	file BuildFile = CompileBuildFile(CommandLine.BuildFile, &BuildTimers, &CompileInfo);
	Timers.Push(BuildTimers);

	timer_group VMBuildTimer = VLibStartTimer("VM");

	interpreter VM = MakeInterpreter(BuildFile.IR->GlobalSymbols, BuildFile.IR->MaxRegisters, DLLs, DLLCount);


	slice<file> FileArray = {};
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
			Out.ptr = VAlloc(GetTypeSize(CompileInfo));

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
				ParseAndAnalyzeFile(File, &FileTimer, CommandLine.Flags);
				Timers.Push(FileTimer);
			}
			timers LLVMTimers = {};
			LLVMTimers.LLVM = VLibStartTimer("LLVM");
			FileArray = SliceFromArray(Files);
			LLVMTargetMachineRef Machine = RCGenerateMain(FileArray);
			RCGenerateCode(FileArray, Machine, true);
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
	system(MakeLinkCommand(CommandLine, FileArray).Data);
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

	if(CommandLine.Flags & CommandFlag_time)
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

