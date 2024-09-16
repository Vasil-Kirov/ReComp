#include "Basic.h"
#include "Memory.h"
#include "vlib.h"
static b32 _MemoryInitializer = InitializeMemory();

#include "Module.h"
#include "Log.h"
#include "VString.h"
#include "DynamicLib.h"
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

#include "Module.cpp"
#include "Memory.cpp"
#include "VString.cpp"
#include "DynamicLib.cpp"
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
#elif defined(CM_LINUX)
#include "Linux.cpp"
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
	}
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		AnalyzeDefineStructs(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		AnalyzeFunctionDecls(File->Checker, &File->Nodes, File->Module);
		//File->Module->Checker = File->Checker;
	}

	b32 FoundMain = false;
	b32 FoundMainMain = false;
	string MainName = STR_LIT("main");
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		if(File->Module->Name == MainName)
		{
			FoundMain = true;
			ForArray(mi, File->Module->Globals)
			{
				symbol *sym = File->Module->Globals[mi];
				if(sym->Flags & SymbolFlag_Function &&
						*sym->Name == MainName)
				{
					FoundMainMain = true;
					break;
				}
			}
		}
	}

	if(!FoundMain)
	{
		LFATAL("Missing main module");
	}

	if(!FoundMainMain)
	{
		LFATAL("Missing main function in main module");
	}

}

file LexFile(string File, string *OutModuleName)
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

	file Result = StringToTokens(FileData, ErrorInfo, OutModuleName);
	Result.Name = File;
	return Result;
}

void ParseFile(file *File, dynamic<module> Modules)
{
	parse_result Parse = ParseTokens(File);
	File->Nodes = Parse.Nodes;
	File->Imported = ResolveImports(Parse.Imports, Modules);
	File->Checker = NewType(checker);
	File->Checker->Module = File->Module;
	File->Checker->Imported = File->Imported;
}

void AnalyzeFile(file *File)
{
	Analyze(File->Checker, File->Nodes);
}

void BuildIRFile(file *File, uint Flag, u32 IRStartRegister)
{
	File->IR = NewType(ir);
	*File->IR = BuildIR(File, IRStartRegister);
	
	if(Flag & CommandFlag_ir)
	{
		string Dissasembly = Dissasemble(SliceFromArray(File->IR->Functions));
		LWARN("[ MODULE %s ]\n\n%s", File->Module->Name.Data, Dissasembly.Data);
	}
}

string MakeLinkCommand(command_line CMD, slice<module> Modules)
{
	string_builder Builder = MakeBuilder();
#if _WIN32
	Builder += "LINK.EXE /nologo /ENTRY:mainCRTStartup /OUT:a.exe !internal.obj /DEBUG ";
#elif CM_LINUX
	Builder += "ld !internal.obj -e _start -lc -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
	Builder += FindObjectFiles();
#else
#error Implement Link Command
#endif

	ForArray(Idx, Modules)
	{
		Builder += Modules[Idx].Name;
		Builder += ".obj ";
	}

	ForArray(Idx, CMD.LinkArgs)
	{
		Builder += CMD.LinkArgs[Idx];
		Builder += ' ';
	}

	if(CMD.LinkArgs.Count == 0)
	{
#if _WIN32
		Builder += "/DEFAULTLIB:MSVCRT ";
#endif
	}


	LDEBUG(Builder.Data);
	return MakeString(Builder);
}

struct compile_info
{
	const char *FileNames[1024];
	i32 FileCount;
};

void CompileBuildFile(file *F, string Name, timers *Timers, u32 *CompileInfoTypeIdx)
{
	type *FileArray = AllocType(TypeKind_Array);
	FileArray->Array.Type = Basic_cstring;
	FileArray->Array.MemberCount = 1024;

	u32 FileArrayType = AddType(FileArray);
	static struct_member CompileInfoMembers[] = {
		{STR_LIT("files"), FileArrayType},
		{STR_LIT("file_count"), Basic_i32},
	};

	string ModuleName = {};
	*F = LexFile(Name, &ModuleName);
	module *M = NewType(module);
	M->Name = ModuleName;
	M->Files.Push(F);
	F->Module = M;
	ParseFile(F, {});
	string_builder CompileInfoName = MakeBuilder();
	CompileInfoName += "__";
	CompileInfoName += F->Module->Name;
	CompileInfoName += STR_LIT("!CompileInfo");

	type *CompileInfoType = AllocType(TypeKind_Struct);
	CompileInfoType->Struct.Name = MakeString(CompileInfoName);
	CompileInfoType->Struct.Members = {CompileInfoMembers, ARR_LEN(CompileInfoMembers)};
	CompileInfoType->Struct.Flags = 0;
	
	u32 CompileInfo = AddType(CompileInfoType);
	*CompileInfoTypeIdx = CompileInfo;

	auto NodeSlice = SliceFromArray(F->Nodes);
	AnalyzeForModuleStructs(NodeSlice, F->Module);
	AnalyzeFunctionDecls(F->Checker, &F->Nodes, F->Module);
	//F->Module->Checker = F->Checker;

	{
		Timers->TypeCheck = VLibStartTimer("Type Check");
		AnalyzeFile(F);
		VLibStopTimer(&Timers->TypeCheck);
	}

	{
		Timers->IR = VLibStartTimer("Building IR");
		BuildIRFile(F, 0, 0);
		VLibStopTimer(&Timers->IR);
	}
}

const char *GetStdDir()
{
	char *Path = (char *)AllocatePermanent(VMAX_PATH);
	GetExePath(Path);
	int i;
	for(i = 0; Path[i] != 0; ++i);
	int size = i;
	for(; Path[i] != '\\' && Path[i] != '/';--i);
	memset(Path + i + 1, 0, size - i - 1);

	return Path;
}

string GetFilePath(string Dir, const char *FileName)
{
	string_builder Builder = MakeBuilder();
	Builder += Dir;
	Builder += "../std/";
	Builder += FileName;
	return MakeString(Builder);
}

void AddStdFiles(dynamic<string> &Files)
{
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);

	string StdFiles[] = {
		GetFilePath(Dir, "init.rcp"),
		GetFilePath(Dir, "os.rcp"),
		GetFilePath(Dir, "string.rcp"),
		GetFilePath(Dir, "mem.rcp"),
		GetFilePath(Dir, "array.rcp"),
	};

	uint Count = ARR_LEN(StdFiles);
	for(int i = 0; i < Count; ++i)
	{
		Files.Push(StdFiles[i]);
	}
}

int
main(int ArgCount, char *Args[])
{
	InitVLib();
	SetGenericReplacement(INVALID_TYPE);

	InitializeLogger();
	InitializeLexer();

	SetLogLevel(LOG_WARN);
	SetBonusMessage(STR_LIT(""));

	if(ArgCount < 2)
	{
		LFATAL("Expected arguments");
	}

#if _WIN32
	PTarget = platform_target::Windows;
#elif CM_LINUX
	PTarget = platform_target::UnixBased;
#else
#error SET DEFAULT Target
#endif

	command_line CommandLine = ParseCommandLine(ArgCount, Args);

	DLIB DLLs[256] = {};
	int DLLCount = 0;

#if _WIN32
	DLLs[DLLCount++] = OpenLibrary("kernel32");
	DLLs[DLLCount++] = OpenLibrary("user32");
	DLLs[DLLCount++] = OpenLibrary("ntdll");
	DLLs[DLLCount++] = OpenLibrary("msvcrt");
	DLLs[DLLCount++] = OpenLibrary("ucrtbase");
#else

#endif
	ForArray(Idx, CommandLine.ImportDLLs)
	{
		 DLIB Lib = OpenLibrary(CommandLine.ImportDLLs[Idx].Data);
		 if(!Lib)
		 {
			 LFATAL("Passed shared library %s could not be found", CommandLine.ImportDLLs[Idx].Data);
		 }
		 DLLs[DLLCount++] = Lib;
	}

	auto CompileFunction = STR_LIT("compile");
	b32 FoundCompile = false;

	dynamic<timers> Timers = {};
	timers BuildTimers = {};
	u32 CompileInfo;
	file BuildFile = {};
	CompileBuildFile(&BuildFile, CommandLine.BuildFile, &BuildTimers, &CompileInfo);
	Timers.Push(BuildTimers);

	timer_group VMBuildTimer = VLibStartTimer("VM");

	interpreter VM = MakeInterpreter(BuildFile.IR->GlobalSymbols, BuildFile.IR->MaxRegisters, DLLs, DLLCount);


	slice<module> ModuleArray = {};
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

			timers FileTimer = {};
			dynamic<string> FileNames = {};
			dynamic<file> Files = {};
			dynamic<module> Modules = {};
			compile_info *Info = (compile_info *)Out.ptr;
			for(int i = 0; i < Info->FileCount; ++i)
			{
				FileNames.Push(MakeString(Info->FileNames[i]));
			}
			AddStdFiles(FileNames);

			{
				FileTimer.Parse = VLibStartTimer("Parse");
				ForArray(Idx, FileNames)
				{
					string ModuleName = {};
					file File = LexFile(FileNames[Idx], &ModuleName);
					Assert(Idx == Files.Count);
					Files.Push(File);
					file *F = &Files.Data[Idx];
					AddModule(Modules, F, ModuleName);
				}
				ForArray(Idx, FileNames)
				{
					file *F = &Files.Data[Idx];
					ParseFile(F, Modules);
				}
				VLibStopTimer(&FileTimer.Parse);
			}

			u32 MaxCount;
			{
				FileTimer.TypeCheck = VLibStartTimer("Type Checking");
				ResolveSymbols(Files);
				ForArray(Idx, FileNames)
				{
					file *F = &Files.Data[Idx];
					AnalyzeFile(F);
				}
				MaxCount = AssignIRRegistersForModuleSymbols(Modules);
				VLibStopTimer(&FileTimer.TypeCheck);
			}

			{
				FileTimer.IR = VLibStartTimer("Intermediate Representation Generation");
				ForArray(Idx, Files)
				{
					file *File = &Files.Data[Idx];
					BuildIRFile(File, CommandLine.Flags, MaxCount);
				}
				VLibStopTimer(&FileTimer.IR);
			}

			{
				FileTimer.TypeCheck = VLibStartTimer("Analyzing");
				ForArray(Idx, Modules)
				{
					AnalyzeModuleForRedifinitions(&Modules.Data[Idx]);
				}
				VLibStopTimer(&FileTimer.TypeCheck);
			}

			{
				FileTimer.LLVM = VLibStartTimer("LLVM");
				ModuleArray = SliceFromArray(Modules);
				FileArray = SliceFromArray(Files);
				llvm_init_info Machine = RCGenerateMain(FileArray);
				RCGenerateCode(Modules, Machine, CommandLine.Flags & CommandFlag_llvm);
				VLibStopTimer(&FileTimer.LLVM);
			}

			Timers.Push(FileTimer);


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
	system(MakeLinkCommand(CommandLine, ModuleArray).Data);

	/* Clean up */
	ForArray(Idx, ModuleArray)
	{
		string_builder Builder = MakeBuilder();
		Builder += ModuleArray[Idx].Name;
		Builder += ".obj ";
		string Path = MakeString(Builder);
		PlatformDeleteFile(Path.Data);
	}
	PlatformDeleteFile("!internal.obj");
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
		LWARN("Compiling Finished...");
		LWARN("Parsing:                   %lldms", ParseTime                / 1000);
		LWARN("Type Checking:             %lldms", TypeCheckTime            / 1000);
		LWARN("Intermediate Generation:   %lldms", IRBuildTime              / 1000);
		LWARN("Interpreting Build File:   %lldms", TimeTaken(&VMBuildTimer) / 1000);
		LWARN("LLVM Code Generation:      %lldms", LLVMTimer                / 1000);
		LWARN("Linking:                   %lldms", TimeTaken(&LinkTimer)    / 1000);
	}

	return 0;
}

const char* GetTokenName(token_type Token) {
    switch (Token) {
		case T_PTR:         return "*";
        case T_ADDROF:      return "&";
		case T_DECL:        return ":";
        case T_STARTSCOPE:  return "{";
        case T_ENDSCOPE:    return "}";
        case T_OPENPAREN:   return "(";
        case T_CLOSEPAREN:  return ")";
        case T_OPENBRACKET: return "[";
        case T_CLOSEBRACKET:return "]";
        case T_CAST:        return "Cast";
        case T_EQ:          return "=";
        case T_LESS:        return "<";
        case T_GREAT:       return ">";
        case T_COMMA:       return ",";
        case T_DOT:         return ".";
        case T_QMARK:       return "?";
        case T_BANG:        return "!";
        case T_SEMICOL:     return ";";
        case T_EOF:         return "End of File";
        case T_ID:          return "Identifier";
        case T_IF:          return "if";
        case T_ELSE:        return "else";
        case T_FOR:         return "for";
        case T_VAL:         return "Number";
        case T_STR:         return "String";
        case T_NEQ:         return "!=";
		case T_GEQ:         return ">=";
        case T_LEQ:         return "<=";
        case T_EQEQ:        return "==";
        case T_ARR:         return "->";
        case T_PPLUS:       return "++";
        case T_MMIN:        return "--";
        case T_LOR:         return "||";
        case T_LAND:        return "&&";
        case T_SLEFT:       return "<<";
        case T_SRIGHT:      return ">>";
        case T_PEQ:         return "+=";
        case T_MEQ:         return "-=";
        case T_TEQ:         return "*=";
        case T_DEQ:         return "/=";
        case T_MODEQ:       return "%=";
        case T_SLEQ:        return "<<=";
        case T_SREQ:        return ">>=";
        case T_ANDEQ:       return "&=";
        case T_XOREQ:       return "^=";
        case T_OREQ:        return "|=";
        case T_FN:          return "fn";
		case T_CONST:       return "::";
        case T_SHADOW:      return "#shadow";
        case T_RETURN:      return "return";
        case T_AUTOCAST:    return "xx";
        case T_FOREIGN:     return "#foreign";
        case T_CSTR:        return "C String";
        case T_STRUCT:      return "struct";
        case T_IMPORT:      return "#import";
        case T_AS:          return "as";
        case T_PUBLIC:      return "#public";
        case T_PRIVATE:     return "#private";
        case T_SIZEOF:      return "size_of";
        case T_IN:          return "in";
        case T_BREAK:       return "break";
        case T_TYPEOF:      return "type_of";
        case T_VARARG:      return "...";
        case T_PWDIF:       return "#if";
        case T_CHAR:        return "Character";
		case T_ENUM:        return "Enum";
		case T_DEFER:       return "defer";
        default: {
            char *C = AllocateString(2);
            C[0] = (char)Token;
            C[1] = 0;
            return C;
        }
    }
}

