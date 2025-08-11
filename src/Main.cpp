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
#include "Polymorph.h"
#include "Type.h"
#include "IR.h"
#include "Threading.h"
#include "Interpreter.h"
#include "x64CodeWriter.h"
#include "CommandLine.h"
#include "Dict.h"
//#include "Linearize.h"
#include "StackAllocator.h"
#include "Globals.h"
#include "InterpDebugger.h"
#include "InterpBinaryOps.h"
#include "InterpCasts.h"
#include "Pipeline.h"

#if 0
#include "backend/LLVMFileOutput.h"
#include "backend/LLVMFileCast.h"
#else

#include "backend/LLVMC/LLVMBase.h"
#include "backend/LLVMC/LLVMType.h"
#include "backend/LLVMC/LLVMValue.h"
#include "backend/LLVMC/LLVMPasses.h"
#include "backend/LLVMC/LLVMTypeInfoGlobal.h"

//#include "backend/RegAlloc.h"
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
#include "Polymorph.cpp"
#include "Type.cpp"
#include "IR.cpp"
#include "Threading.cpp"
#include "Interpreter.cpp"
#include "x64CodeWriter.cpp"
#include "CommandLine.cpp"
#include "DumpInfo.cpp"
//#include "Linearize.cpp"
#include "StackAllocator.cpp"
#include "InterpDebugger.cpp"
#include "InterpBinaryOps.cpp"
#include "InterpCasts.cpp"
#include "Pipeline.cpp"
#if 0
#include "backend/LLVMFileOutput.cpp"
#include "backend/LLVMFileCast.cpp"
#else

#include "backend/LLVMC/LLVMBase.cpp"
#include "backend/LLVMC/LLVMType.cpp"
#include "backend/LLVMC/LLVMValue.cpp"
#include "backend/LLVMC/LLVMPasses.cpp"
#include "backend/LLVMC/LLVMTypeInfoGlobal.cpp"

//#include "backend/RegAlloc.cpp"
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

#ifdef __cplusplus
extern "C"
#endif
const char* __asan_default_options() { return "detect_leaks=0"; }

dynamic<string> ConfigIDs = {};

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
		NoSetDefaultLib = true;
		Builder += "/NODEFAULTLIB ";
		if(CompileFlags & CF_Standalone)
		{
			Builder += "/ENTRY:start ";
		}
		else
		{
			Builder += "/ENTRY:main ";
		}
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

void AddStdFiles(dynamic<string> &Files, u32 Flags, interp_string Internals)
{
	string StdFiles[] = {
		STR_LIT("base.rcp"),
		STR_LIT("os.rcp"),
		STR_LIT("io.rcp"),
		STR_LIT("mem.rcp"),
		STR_LIT("strings.rcp"),
		STR_LIT("array.rcp"),
		STR_LIT("compile.rcp"),
		STR_LIT("math.rcp"),
	};

	if((Flags & CF_Standalone) == 0)
	{
		uint Count = ARR_LEN(StdFiles);
		for(int i = 0; i < Count; ++i)
		{
			Files.Push(StdFiles[i]);
		}
	}

	// Doesn't care about CF_Standalone
	Files.Push(STR_LIT("intrin.rcp"));

	if(Flags & CF_NoStdLib)
	{
		Files.Push(STR_LIT("req.rcp"));
	}

	if(Internals.Data == NULL)
	{
		Files.Push(STR_LIT("internal.rcp"));
	}
	else
	{
		// @Note: make sure it's null terminated
		string Str = MakeString(Internals.Data, Internals.Count);
		Files.Push(Str);
	}
}


int
main(int ArgCount, char *Args[])
{
	InitVLib();

	InitializeLogger();
	InitializeLexer();

	SetLogLevel(LOG_INFO);
	SetBonusMessage(STR_LIT(""));
	AddVectorTypes();

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

	DumpingInfo = (CommandLine.Flags & CommandFlag_dumpinfo) != 0;

	string StdLibDir = GetFilePath(MakeString(GetStdDir()), "");
	StdLibDir.Size--;
	StdLibDir = MakeString(StdLibDir.Data, StdLibDir.Size);
	Assert(AddLookupPath(STR_LIT(".")));
	if(!AddLookupPath(StdLibDir))
	{
		LogCompilerError("Error: Invalid installation, compiler couldn't find standard library directory at %.*s\n", StdLibDir.Size, StdLibDir.Data);
		exit(1);
	}


#if _WIN32
	DLs.Push(OpenLibrary("kernel32"));
	DLs.Push(OpenLibrary("user32"));
	DLs.Push(OpenLibrary("ntdll"));
	DLs.Push(OpenLibrary("msvcrt"));
	DLs.Push(OpenLibrary("ucrt"));
	DLs.Push(OpenLibrary("ucrtbase"));
#elif CM_LINUX
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);
	DLs.Push(OpenLibrary("libc.so"));
	DLs.Push(OpenLibrary(GetFilePath(Dir, "system_call.so").Data));
#else

#endif
	ForArray(Idx, CommandLine.ImportDLLs)
	{
		 DLIB Lib = OpenLibrary(CommandLine.ImportDLLs[Idx].Data);
		 if(!Lib)
		 {
			 LFATAL("Passed shared library %s could not be found", CommandLine.ImportDLLs[Idx].Data);
		 }
		 DLs.Push(Lib);
	}

	CreatePipeline();

	dynamic<timers> Timers = {};
	slice<module*> ModuleArray = {};
	compile_info *Info = NewType(compile_info);
	slice<function> BuildFileFunctions = {};
	interpreter VM = {};
	timer_group VMBuildTimer = {};
	timer_group VMBuildTimer2 = {};

#if _WIN32
	ConfigIDs.Push(STR_LIT("Windows"));
#else
	ConfigIDs.Push(STR_LIT("Unix"));
#endif

	if(CommandLine.SingleFile.Data == NULL)
	{
		timers BuildTimers = {};
		file BuildFile = {};
		slice<module*> BuildModules = {};

		// @TODO: maybe actually check the host machine?
		ConfigIDs.Push(STR_LIT("x86"));
		ConfigIDs.Push(STR_LIT("x64"));

		{
			dynamic<string> FileNames = {};
			FileNames.Push(CommandLine.BuildFile);
			AddStdFiles(FileNames, false, {});
			auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("build"), STR_LIT("compile"));
			BuildModules = r.Modules;
			BuildFile = *r.Files[r.EntryFileIdx];
			BuildTimers = r.Timers;

			// Clear run-time defines
			ConfigIDs = {};
		}

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
		if(CompileT->Function.ArgCount < 1 ||
				GetTypeNameAsString(CompileT->Function.Args[0]) != STR_LIT("*compile.CompileInfo"))
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

		MakeInterpreter(VM, BuildModules, BuildFile.IR->MaxRegisters);
		if(HasErroredOut())
			exit(1);

		{


			if(InterpreterTrace)
				LINFO("Interpreting compile function");
			interpret_result Result = InterpretFunction(&VM, *CompileFunction, {&InfoValue, 1});
			PlatformClearSignalHandler();

			if(Result.Kind == INTERPRET_RUNTIME_ERROR)
			{
				LogCompilerError("Error: Failed to evaluate build.compile\n");
				return 1;
			}

			VLibStopTimer(&VMBuildTimer);

			for(int i = 0; i < Info->DirectoryCount; ++i)
			{
				interp_string InterpDir = Info->Directories[i];
				string Path = { .Data = InterpDir.Data, .Size = InterpDir.Count };
				if(!AddLookupPath(Path))
				{
					LogCompilerError("Error: Couldn't find source directory: %.*s\n",
							Path.Size, Path.Data);

				}
			}

			if(Info->Flags & CF_Standalone)
			{
				Info->Flags |= CF_NoStdLib;
			}

			if((Info->Flags & CF_NoStdLib) == 0)
			{
				ConfigIDs.Push(STR_LIT("LIBC"));
			}

			CompileFlags = Info->Flags;
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

			using pt = platform_target;
			switch(PTarget)
			{
				case pt::Windows:
				{
					ConfigIDs.Push(STR_LIT("Windows"));
				} break;
				case pt::UnixBased:
				{
					ConfigIDs.Push(STR_LIT("Unix"));
				} break;
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
			AddStdFiles(FileNames, Info->Flags, Info->InternalFile);

			For(ConfigIDs)
			{
				LDEBUG("CONFIG %s", it->Data);
			}

			auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("main"), STR_LIT("main"));
			slice<file*> Files = r.Files;
			ModuleArray = r.Modules;
			FileTimer = r.Timers;

			// Remake vm to evaluate enums with new info

			VMBuildTimer2 = VLibStartTimer("VM");
			interpreter ComptimeVM = {};
			MakeInterpreter(ComptimeVM, ModuleArray, 0);
			PlatformClearSignalHandler();
			if(HasErroredOut())
				exit(1);
			VLibStopTimer(&VMBuildTimer2);


			FileTimer.LLVM = VLibStartTimer("LLVM");
#if 1
			RCGenerateCode(CurrentPipeline.Queue, ModuleArray, Files, CommandLine.Flags, Info, ComptimeVM.StoredGlobals);
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
			ComptimeVM.StackAllocator.Free();

			Timers.Push(FileTimer);
		}
	}
	else
	{
		using pt = platform_target;
		switch(PTarget)
		{
			case pt::Windows:
			{
				ConfigIDs.Push(STR_LIT("Windows"));
			} break;
			case pt::UnixBased:
			{
				ConfigIDs.Push(STR_LIT("Unix"));
			} break;
		}
		CommandLine.Flags |= CF_DebugInfo;

		timers FileTimer = {};

		dynamic<string> FileNames = {};
		FileNames.Push(CommandLine.SingleFile);
		AddStdFiles(FileNames, false, {});

		auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("main"), STR_LIT("main"));
		slice<file*> Files = r.Files;
		ModuleArray = r.Modules;
		FileTimer = r.Timers;
		
		MakeInterpreter(VM, ModuleArray, 100);
		if(HasErroredOut())
			exit(1);

		FileTimer.LLVM = VLibStartTimer("LLVM");
		RCGenerateCode(CurrentPipeline.Queue, ModuleArray, Files, CommandLine.Flags, Info, VM.StoredGlobals);
		VLibStopTimer(&FileTimer.LLVM);
		VM.StackAllocator.Free();
	}


	auto LinkTimer = VLibStartTimer("Linking");
	if(Info->Flags & CF_NoLink) {}
	else
	{
		system(MakeLinkCommand(CommandLine, ModuleArray, Info->Flags).Data);
	}
	VLibStopTimer(&LinkTimer);

	function *AfterFunction = FindFunction(BuildFileFunctions, STR_LIT("after_link"));
	if(AfterFunction)
	{
		if(InterpreterTrace)
			LINFO("Interpreting after_link function");

		PlatformSetSignalHandler(InterpSegFault, &VM);
		VM.HasSetSigHandler = true;

		InterpretFunction(&VM, *AfterFunction, {});

		PlatformClearSignalHandler();
	}

	/* Clean up */
	if((Info->Flags & CF_NoLink) == 0)
	{
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
	VM.StackAllocator.Pop();

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
		LWARN("Compile Time Evaluation:   %lldms", TimeTaken(&VMBuildTimer2)/ 1000);
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
        //case T_CAST:        return "cast";
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

