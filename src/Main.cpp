#include "Dynamic.h"
#include "Memory.h"
#include "vlib.h"
#include <cstddef>
#include <sys/types.h>
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
#include "Dict.h"

#if 0
#include "backend/LLVMFileOutput.h"
#include "backend/LLVMFileCast.h"
#else

#include "backend/LLVMC/LLVMBase.h"
#include "backend/LLVMC/LLVMType.h"
#include "backend/LLVMC/LLVMValue.h"
#include "backend/LLVMC/LLVMPasses.h"
#include "backend/LLVMC/LLVMTypeInfoGlobal.h"

#include "backend/RegAlloc.h"
//#include "backend/x86.h"

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
#include "DumpInfo.cpp"
#if 0
#include "backend/LLVMFileOutput.cpp"
#include "backend/LLVMFileCast.cpp"
#else

#include "backend/LLVMC/LLVMBase.cpp"
#include "backend/LLVMC/LLVMType.cpp"
#include "backend/LLVMC/LLVMValue.cpp"
#include "backend/LLVMC/LLVMPasses.cpp"
#include "backend/LLVMC/LLVMTypeInfoGlobal.cpp"

#include "backend/RegAlloc.cpp"
//#include "backend/x86.cpp"

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

void ResolveSymbols(slice<file*> Files, b32 ExpectingMain)
{
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		AnalyzeForModuleStructs(NodeSlice, File->Module);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeEnums(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeDefineStructs(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeFillStructCaches(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeFunctionDecls(File->Checker, &File->Nodes, File->Module);
		//File->Module->Checker = File->Checker;
	}

	if(ExpectingMain)
	{
		b32 FoundMain = false;
		b32 FoundMainMain = false;
		string MainName = STR_LIT("main");
		ForArray(Idx, Files)
		{
			file *File = Files[Idx];
			if(File->Module->Name == MainName)
			{
				FoundMain = true;
				ForArray(mi, File->Module->Globals.Data)
				{
					symbol *sym = File->Module->Globals.Data[mi];
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
}

file *LexFile(string File, string *OutModuleName)
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

	file *Result = StringToTokens(FileData, ErrorInfo, OutModuleName);
	Result->Name = File;
	return Result;
}

dynamic<string> ConfigIDs = {};

void ParseFile(file *File, dynamic<module*> Modules)
{
	parse_result Parse = ParseTokens(File, SliceFromArray(ConfigIDs));
	File->Nodes = Parse.Nodes;
	File->Imported = ResolveImports(Parse.Imports, Modules);
	File->Checker = NewType(checker);
	File->Checker->Module = File->Module;
	File->Checker->Imported = File->Imported;
	File->Checker->File = File->Name;
}

void AnalyzeFile(file *File)
{
	Analyze(File->Checker, File->Nodes);
	if(HasErroredOut())
		exit(1);
}

void BuildIRFile(file *File, command_line CommandLine, u32 IRStartRegister)
{
	File->IR = NewType(ir);
	*File->IR = BuildIR(File, IRStartRegister);
	
	if(ShouldOutputIR(File->Module->Name, CommandLine))
	{
		string Dissasembly = Dissasemble(SliceFromArray(File->IR->Functions));
		LWARN("[ MODULE %s ]\n\n%s", File->Module->Name.Data, Dissasembly.Data);
	}
}

slice<file*> RunBuildPipeline(slice<string> FileNames, timers *Timers, command_line CommandLine, b32 WantMain, slice<module*> *OutModules)
{
	dynamic<module*> Modules = {};
	dynamic<file*> FileDyn = {};
	dynamic<string> ModuleNames = {};

	Timers->Parse = VLibStartTimer("Parse");
	ForArray(Idx, FileNames)
	{
		string ModuleName = {};
		file *File = LexFile(FileNames[Idx], &ModuleName);
		ModuleNames.Push(ModuleName);
		Assert(Idx == FileDyn.Count);
		FileDyn.Push(File);
	}
	slice<file*> Files = SliceFromArray(FileDyn);
	ForArray(Idx, Files)
	{
		file *F = Files[Idx];
		AddModule(Modules, F, ModuleNames[Idx]);
	}
	ForArray(Idx, FileNames)
	{
		file *F = Files[Idx];
		ParseFile(F, Modules);
	}
	VLibStopTimer(&Timers->Parse);

	if(HasErroredOut())
		exit(1);

	CurrentModules = SliceFromArray(Modules);
	u32 MaxCount;
	{
		Timers->TypeCheck = VLibStartTimer("Type Checking");
		ResolveSymbols(Files, WantMain);
		ForArray(Idx, FileNames)
		{
			file *F = Files[Idx];
			AnalyzeFile(F);
		}
		MaxCount = AssignIRRegistersForModuleSymbols(Modules);
		VLibStopTimer(&Timers->TypeCheck);
	}

	if(DumpingInfo)
	{
		binary_blob Blob = StartOutput();
		DumpU32(&Blob, Modules.Count);
		For(Modules)
		{
			DumpModule(&Blob, *it);
		}
		DumpTypeTable(&Blob);
		WriteBlobToFile(&Blob);
	}

	{
		Timers->IR = VLibStartTimer("Intermediate Representation Generation");
		ForArray(Idx, Files)
		{
			file *File = Files[Idx];
			BuildIRFile(File, CommandLine, MaxCount);
		}
		VLibStopTimer(&Timers->IR);
	}

	*OutModules = SliceFromArray(Modules);
	return Files;
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

string MakeLinkCommand(command_line CMD, slice<module*> Modules, u32 CompileFlags)
{
	string_builder Builder = MakeBuilder();
#if _WIN32
	b32 NoSetDefaultLib = false;
	Builder += "LINK.EXE /nologo /OUT:a.exe /DEBUG ";
	if(CompileFlags & CF_SanAdress)
	{
		string Std = MakeString(GetStdDir());
		Builder += GetFilePath(Std, "libs/clang_rt.asan-x86_64.lib ");
		if((CompileFlags & CF_NoStdLib) == 0)
		{
			NoSetDefaultLib = true;
			Builder += " /DEFAULTLIB:LIBCMT ";
		}
	}
	if(CompileFlags & CF_NoStdLib)
	{
		Builder += "/NODEFAULTLIB /ENTRY:main ";
	}
	else
	{
		Builder += "/ENTRY:mainCRTStartup ";
	}

	if(!NoSetDefaultLib)
	{
		Builder += "/DEFAULTLIB:MSVCRT ";
	}

#elif CM_LINUX
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);

	string SystemCallObj = GetFilePath(Dir, "system_call.o");
	if(CompileFlags & CF_NoStdLib)
	{
		Builder += "ld -e main -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
	}
	else
	{
		Builder += "ld -e _start -lc -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
		Builder += FindObjectFiles();
	}
	Builder += SystemCallObj;
	Builder += ' ';
#else
#error Implement Link Command
#endif

	ForArray(Idx, Modules)
	{
		Builder += Modules[Idx]->Name;
		Builder += ".obj ";
	}

	ForArray(Idx, CMD.LinkArgs)
	{
		Builder += CMD.LinkArgs[Idx];
		Builder += ' ';
	}

	string Command = MakeString(Builder);
	LDEBUG(Command.Data);
	return Command;
}


function *FindFunction(slice<function> Functions, string Name)
{
	For(Functions)
	{
		if(*it->Name == Name)
			return it;
	}

	return NULL;
}

void AddStdFiles(dynamic<string> &Files, b32 NoStdLib)
{
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);

	string StdFiles[] = {
		GetFilePath(Dir, "init.rcp"),
		GetFilePath(Dir, "os.rcp"),
		GetFilePath(Dir, "io.rcp"),
		GetFilePath(Dir, "mem.rcp"),
		GetFilePath(Dir, "strings.rcp"),
		GetFilePath(Dir, "array.rcp"),
		GetFilePath(Dir, "compile.rcp"),
		GetFilePath(Dir, "math.rcp"),
	};

	uint Count = ARR_LEN(StdFiles);
	for(int i = 0; i < Count; ++i)
	{
		Files.Push(StdFiles[i]);
	}

	if(NoStdLib)
	{
		Files.Push(GetFilePath(Dir, "req.rcp"));
	}
}

void CompileBuildFile(file *F, string Name, timers *Timers, u32 *CompileInfoTypeIdx, command_line CommandLine, slice<module*> *OutModules, b32 NoStdLib)
{
	dynamic<string> FileNames = {};
	AddStdFiles(FileNames, NoStdLib);
	FileNames.Push(Name);
	slice<file*> Files = RunBuildPipeline(SliceFromArray(FileNames), Timers, CommandLine, false, OutModules);
	for(int i = 0; i < Files.Count; ++i)
	{
		if(Files[i]->Module->Name == STR_LIT("build"))
		{
			*F = *Files[i];
			return;
		}
	}
	LFATAL("Failed to find build module in compile script");
	unreachable;
}

int
main(int ArgCount, char *Args[])
{
	InitVLib();
	SetGenericReplacement(INVALID_TYPE);

	InitializeLogger();
	InitializeLexer();

	SetLogLevel(LOG_INFO);
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

	DumpingInfo = (CommandLine.Flags & CF_DumpInfo) != 0;

	DLIB DLLs[256] = {};
	int DLLCount = 0;

#if _WIN32
	DLLs[DLLCount++] = OpenLibrary("kernel32");
	DLLs[DLLCount++] = OpenLibrary("user32");
	DLLs[DLLCount++] = OpenLibrary("ntdll");
	DLLs[DLLCount++] = OpenLibrary("msvcrt");
	DLLs[DLLCount++] = OpenLibrary("ucrt");
	DLLs[DLLCount++] = OpenLibrary("ucrtbase");
#elif CM_LINUX
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);
	DLLs[DLLCount++] = OpenLibrary("libc.so");
	DLLs[DLLCount++] = OpenLibrary(GetFilePath(Dir, "system_call.so").Data);
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

	dynamic<timers> Timers = {};
	slice<module*> ModuleArray = {};
	compile_info *Info = NewType(compile_info);
	slice<function> BuildFileFunctions = {};
	interpreter VM = {};
	timer_group VMBuildTimer = {};
	if(CommandLine.SingleFile.Data == NULL)
	{
		timers BuildTimers = {};
		u32 CompileInfo;
		file BuildFile = {};
		slice<module*> BuildModules = {};

		// @TODO: maybe actually check the host machine?
		ConfigIDs.Push(STR_LIT("x86"));
		ConfigIDs.Push(STR_LIT("x64"));
		//u32 BeforeTypeCount = GetTypeCount();
		CompileBuildFile(&BuildFile, CommandLine.BuildFile, &BuildTimers, &CompileInfo, CommandLine, &BuildModules, false);

		BuildFileFunctions = SliceFromArray(BuildFile.IR->Functions);

		Timers.Push(BuildTimers);

		value InfoValue = {};
		InfoValue.Type = GetPointerTo(INVALID_TYPE);
		InfoValue.ptr = Info;

		function *CompileFunction = FindFunction(BuildFileFunctions, STR_LIT("compile"));
		if(!CompileFunction)
		{
			LFATAL("File %s doesn't have the `compile` function defined, this function is used to define how to build the program", CommandLine.BuildFile.Data);
		}
		if(!CompileFunction || CompileFunction->Blocks.Count == 0)
		{
			LFATAL("compile function is empty");
		}
		const type *CompileT = GetType(CompileFunction->Type);
		Assert(CompileT->Kind == TypeKind_Function);
		if(CompileT->Function.Returns.Count != 1 ||
				GetTypeNameAsString(CompileT->Function.Returns[0]) != STR_LIT("compile.CompileInfo"))
		{
			LFATAL("compile function needs to return compile.CompileInfo");
		}
		ConfigIDs.Count = 0;

#if 0
		for(int i = 0; i < GetTypeCount(); ++i)
		{
			LDEBUG("%d: %s", i, GetTypeName(i));
		}
#endif

		VMBuildTimer = VLibStartTimer("VM");
		VM = MakeInterpreter(BuildModules, BuildFile.IR->MaxRegisters, DLLs, DLLCount);
		{


			if(InterpreterTrace)
				LINFO("Interpreting compile function");
			interpret_result Result = InterpretFunction(&VM, *CompileFunction, {&InfoValue, 1});

			VLibStopTimer(&VMBuildTimer);

			if(Info->Flags & CF_CrossAndroid)
			{
				PTarget = platform_target::UnixBased;
				Info->Flags |= CF_SharedLib;
				if(Info->TargetTriple.Data == NULL)
				{
					Info->TargetTriple.Data = "armv7-none-linux-androideabi";
					Info->TargetTriple.Count = VStrLen(Info->TargetTriple.Data);
				}
			}
			else
			{
			}

			if(Info->Arch == Arch_x86_64)
			{
				ConfigIDs.Push(STR_LIT("x86"));
				ConfigIDs.Push(STR_LIT("x64"));
			}
			else if(Info->Arch == Arch_x86)
			{
				ConfigIDs.Push(STR_LIT("x86"));
			}
			else if(Info->Arch == Arch_arm32)
			{
				ConfigIDs.Push(STR_LIT("arm32"));
			}
			else if(Info->Arch == Arch_arm64)
			{
				ConfigIDs.Push(STR_LIT("arm64"));
			}

			if(Info->Link.Count > 0)
			{
				string Args = MakeString(Info->Link.Data, Info->Link.Count);
				CommandLine.LinkArgs.Push(Args);
			}

			timers FileTimer = {};
			dynamic<string> FileNames = {};
			for(int i = 0; i < Info->FileCount; ++i)
			{
				FileNames.Push(MakeString(Info->FileNames[i].Data, Info->FileNames[i].Count));
			}
			AddStdFiles(FileNames, Info->Flags & CF_NoStdLib);

			For(ConfigIDs)
			{
				LDEBUG("CONFIG %s", it->Data);
			}

			slice<file*> FileArray = RunBuildPipeline(SliceFromArray(FileNames), &FileTimer, CommandLine, true, &ModuleArray);

			FileTimer.LLVM = VLibStartTimer("LLVM");
#if 1
			RCGenerateCode(ModuleArray, FileArray, CommandLine.Flags & CommandFlag_llvm, Info);
#else
			slice<reg_reserve_instruction> Reserved = SliceFromConst({
					reg_reserve_instruction{OP_DIV, SliceFromConst<uint>({0, 3, 0})},
					});

			reg_allocator r = MakeRegisterAllocator(Reserved, 4);
			ForArray(fi, FileArray)
			{
				ir *IR = FileArray[fi].IR;
				AllocateRegisters(&r, IR);
				string Dissasembly = Dissasemble(SliceFromArray(IR->Functions));
				LWARN("\t----[ALLOCATED]----\t\n[ MODULE %s ]\n\n%s", FileArray[fi].Module->Name.Data, Dissasembly.Data);\
			}
#endif
			VLibStopTimer(&FileTimer.LLVM);

			Timers.Push(FileTimer);

			if(Result.ToFreeStackMemory)
				VFree(Result.ToFreeStackMemory);
		}
	}
	else
	{
		timers FileTimer = {};
		dynamic<string> FileNames = {};
		FileNames.Push(CommandLine.SingleFile);
		AddStdFiles(FileNames, false);
		slice<file*> FileArray = RunBuildPipeline(SliceFromArray(FileNames), &FileTimer, CommandLine, true, &ModuleArray);

		FileTimer.LLVM = VLibStartTimer("LLVM");
		RCGenerateCode(ModuleArray, FileArray, CommandLine.Flags & CommandFlag_llvm, Info);
		VLibStopTimer(&FileTimer.LLVM);
	}


	auto LinkTimer = VLibStartTimer("Linking");
	if(Info->Flags & CF_NoLink) {}
	else
	{
		system(MakeLinkCommand(CommandLine, ModuleArray, Info->Flags).Data);

		/* Clean up */
		ForArray(Idx, ModuleArray)
		{
			string_builder Builder = MakeBuilder();
			Builder += ModuleArray[Idx]->Name;
			Builder += ".obj";
			string Path = MakeString(Builder);
			if(!PlatformDeleteFile(Path.Data)) {
				LDEBUG("Failed to detel file: %s", Path.Data);
			}
		}
	}
	VLibStopTimer(&LinkTimer);

	function *AfterFunction = FindFunction(BuildFileFunctions, STR_LIT("after_link"));
	if(AfterFunction)
	{
		if(InterpreterTrace)
			LINFO("Interpreting after_link function");
		InterpretFunction(&VM, *AfterFunction, {});
	}

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

	FreeAllArenas();
	return 0;
}

const char* GetTokenName(token_type Token) {
    switch (Token) {
		case T_PLUS:		return "+";
		case T_MIN:			return "-";
		case T_PTR:         return "*";
        case T_ADDROF:      return "&";
		case T_DECL:        return ":";
        case T_STARTSCOPE:  return "{";
        case T_ENDSCOPE:    return "}";
        case T_OPENPAREN:   return "(";
        case T_CLOSEPAREN:  return ")";
        case T_OPENBRACKET: return "[";
        case T_CLOSEBRACKET:return "]";
        case T_CAST:        return "cast";
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
		case T_PROFILE:		return "@profile";
        default: {
            char *C = AllocateString(2);
            C[0] = (char)Token;
            C[1] = 0;
            return C;
        }
    }
}

