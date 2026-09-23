#include "Memory.h"
static b32 _MemoryInitializer = InitializeMemory();

#define VLIB_IMPLEMENTATION

#if defined(_WIN32)
#include "Win32.cpp"

#elif defined(CM_LINUX)
#include "Linux.cpp"
#else
#error unsupported platform
#endif


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
#include "FlowTyping.h"

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
#include "FlowTyping.cpp"
#include "PassAst.cpp"

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

#ifdef __cplusplus
extern "C"
#endif
const char* __asan_default_options() { return "detect_leaks=0"; }

dynamic<string> ConfigIDs = {};

// @Note: Destroys the original string
void FilePathToDirPath(char *FilePath)
{
	int i;
	for(i = 0; FilePath[i] != 0; ++i);
	int size = i;
	for(; FilePath[i] != '\\' && FilePath[i] != '/';--i);
	memset(FilePath + i + 1, 0, size - i - 1);
}

const char *GetRVCBinDir()
{
	char *Path = (char *)AllocatePermanent(VMAX_PATH);
	GetExePath(Path);
	FilePathToDirPath(Path);

	return Path;
}

string GetStdPathFromRVCBinDir(string Dir, const char *FileName)
{
	string_builder Builder = MakeBuilder();
	Builder += Dir;
	Builder += "../std/";
	Builder += FileName;
	return MakeString(Builder);
}


function *FindFunction(module *Module, string Name)
{
	if (!Module)
		return NULL;
	for(file *File : Module->Files)
	{
		for(auto &f : File->IR->Functions)
		{
			if(*f.Name == Name)
				return &f;
		}
	}

	return NULL;
}

void AddStdFiles(dynamic<string> &Files, u32 Flags, interp_string Internals)
{
	string StdFiles[] = {
		STR_LIT("base.rv"),
		STR_LIT("misc.rv"),
		STR_LIT("reflect.rv"),
		STR_LIT("ast.rv"),
		STR_LIT("os.rv"),
		STR_LIT("io.rv"),
		STR_LIT("mem.rv"),
		STR_LIT("strings.rv"),
		STR_LIT("array.rv"),
		STR_LIT("compile.rv"),
		STR_LIT("math.rv"),
	};

	if((Flags & CF_Standalone) == 0)
	{
		uint Count = ARR_LEN(StdFiles);
		for(int i = 0; i < Count; ++i)
		{
			Files.Push(StdFiles[i]);
		}
	}
	else
	{
		Files.Push(STR_LIT("base.rv"));
	}

	// Doesn't care about CF_Standalone
	Files.Push(STR_LIT("intrin.rv"));

	if(Flags & CF_NoLibC)
	{
		Files.Push(STR_LIT("req.rv"));
	}

	if(Internals.Data == NULL)
	{
		Files.Push(STR_LIT("internal.rv"));
	}
	else
	{
		// @Note: make sure it's null terminated
		string Str = MakeString(Internals.Data, Internals.Count);
		Files.Push(Str);
	}
}

void DefaultSignalHandler(void *)
{
	PlatformOutputString(STR_LIT("--- INTERNAL COMPILER ERROR ---\nException triggered!\n"), LOG_ERROR);
	PrintStacktrace();
	exit(1);
}

int
main(int ArgCount, char *Args[])
{
	PlatformSetSignalHandler(DefaultSignalHandler, NULL);

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
	if(CommandLine.BuildFile.Data == NULL && CommandLine.SingleFile.Data == NULL)
		return 1;

	ToolPipe = CommandLine.ToolPipe;
	DumpingInfo = (CommandLine.Flags & CommandFlag_dumpinfo) != 0;
	string StdLibDir = GetStdPathFromRVCBinDir(MakeString(GetRVCBinDir()), "");
	StdLibDir.Size--;
	StdLibDir = MakeString(StdLibDir.Data, StdLibDir.Size);
	AddLookupPath(STR_LIT("."));
	if(!AddLookupPath(StdLibDir))
	{
		LogCompilerError("Error: Invalid installation, compiler couldn't find standard library directory at %.*s\n", StdLibDir.Size, StdLibDir.Data);
		exit(1);
	}


#if _WIN32
	g_DLs.Push(OpenLibrary("kernel32"));
	g_DLs.Push(OpenLibrary("user32"));
	g_DLs.Push(OpenLibrary("ntdll"));
	g_DLs.Push(OpenLibrary("msvcrt"));
	g_DLs.Push(OpenLibrary("ucrt"));
	g_DLs.Push(OpenLibrary("ucrtbase"));
	//g_DLs.Push(OpenLibrary("ws2_32"));
#elif CM_LINUX
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);
	g_DLs.Push(OpenLibrary("libc.so"));
	g_DLs.Push(OpenLibrary(GetFilePath(Dir, "system_call.so").Data));
#else

#endif
	ForArray(Idx, CommandLine.ImportDLLs)
	{
		 DLIB Lib = OpenLibrary(CommandLine.ImportDLLs[Idx].Data);
		 if(!Lib)
		 {
			 LFATAL("Passed shared library %s could not be found", CommandLine.ImportDLLs[Idx].Data);
		 }
		 g_DLs.Push(Lib);
	}

	CreatePipeline();

	bool NeedToRestoreForAfterFunction = false;
	saved_type_table BuildTimeTypeTable = {};

	dynamic<timers> Timers = {};
	dynamic<timer_group> LinkTimers = {};
	slice<module*> ModuleArray = {};
	module *BuildModule = nullptr;
	interpreter BuildVM = {};
	dynamic<timer_group> VMBuildTimers  = {};
	dynamic<timer_group> VMBuildTimers2 = {};

#if _WIN32
	ConfigIDs.Push(STR_LIT("Windows"));
#else
	ConfigIDs.Push(STR_LIT("Unix"));
#endif

	if(CommandLine.SingleFile.Data == NULL)
	{
		timers BuildTimers = {};
		slice<module*> BuildModules = {};

		// @TODO: maybe actually check the host machine?
		ConfigIDs.Push(STR_LIT("x86"));
		ConfigIDs.Push(STR_LIT("x64"));

		bool WasDumpingInfo = DumpingInfo;
		DumpingInfo = false;

		char *BuildFilePath = GetAbsolutePath(CommandLine.BuildFile.Data);
		// @Note: If Path is null then we can't find the build file, we don't error here
		// because the RunPipeline will error on its own - Vasko 22/09/2026
		if (BuildFilePath) {
			FilePathToDirPath(BuildFilePath);
			AddLookupPath((string){BuildFilePath, strlen(BuildFilePath)});
		}

		{
			dynamic<string> FileNames = {};
			FileNames.Push(CommandLine.BuildFile);
			AddStdFiles(FileNames, false, {});
			auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("build"), STR_LIT(""));
			BuildModules = r.Modules;

			for(auto Module : BuildModules)
			{
				if(Module->Name == STR_LIT("build"))
				{
					BuildModule = Module;
					break;
				}
			}
			Assert(BuildModule);

			BuildTimers = r.Timers;

			// Clear run-time defines
			ConfigIDs.Count = 0;
		}
		DumpingInfo = WasDumpingInfo;

		Timers.Push(BuildTimers);

		function *CompileFunction = FindFunction(BuildModule, STR_LIT("compile"));
		if(CompileFunction)
		{
			const type *CompileT = GetType(CompileFunction->Type);
			Assert(CompileT->Kind == TypeKind_Function);
			if(CompileT->Function.ArgCount < 1 ||
					GetTypeNameAsString(CompileT->Function.Args[0]) != STR_LIT("*compile.CompileInfo"))
			{
				LFATAL("compile function needs to return compile.CompileInfo");
			}
		}

#if 0
		for(int i = 0; i < GetTypeCount(); ++i)
		{
			LDEBUG("%d: %s", i, GetTypeName(i));
		}
#endif

		timer_group VMBuildTimer = VLibStartTimer("VM");

		MakeInterpreter(BuildVM, BuildModules, 0);
		if(HasErroredOut())
			exit(1);

		{
			char WasDir[VMAX_PATH] = {};
			PlatformGetCWD(WasDir, VMAX_PATH);
			PlatformChangeCWD(BuildFilePath);

			if(CompileFunction)
			{
				compile_info *Info = NewType(compile_info);
				value InfoValue = {};
				InfoValue.Type = GetPointerTo(INVALID_TYPE);
				InfoValue.ptr = Info;

				if(g_InterpreterTrace)
					LINFO("Interpreting compile function");
				interpret_result Result = InterpretFunction(&BuildVM, *CompileFunction, {&InfoValue, 1});
				if(Result.Kind == INTERPRET_RUNTIME_ERROR)
				{
					LogCompilerError("Error: Failed to evaluate build.compile\n");
					return 1;
				}

				VLibStopTimer(&VMBuildTimer);
				VMBuildTimers.Push(VMBuildTimer);

				for(int i = 0; i < Info->DirectoryCount; ++i)
				{
					interp_string InterpDir = Info->Directories[i];
					string Dir = { .Data = InterpDir.Data, .Size = InterpDir.Count };

					if(!AddLookupPath(Dir))
					{
						LogCompilerError("Error: Couldn't find source directory: %.*s\n",
								Dir.Size, Dir.Data);

					}
				}
				g_CompileTargets.Push(*Info);
			}
			PlatformSetSignalHandler(DefaultSignalHandler, NULL);
			PlatformChangeCWD(WasDir);

			BuildTimeTypeTable = SaveTypeTableAndReset();
			for(compile_info &Info_ : g_CompileTargets)
			{
				compile_info *Info = &Info_;
				if(Info->Output.Count == 0)
				{
#if _WIN32
					Info->Output = {5, "a.exe"};
#else
					Info->Output = {1, "a"};
#endif
				}
				g_TargetArch = (arch)Info->Arch;

				for(size_t i = 0; i < Info->DefineCount; ++i)
				{
					ConfigIDs.Push(StringFromInterp(Info->Defines[i]));
				}
				if((Info->Flags & CF_NoLibC) == 0)
				{
					ConfigIDs.Push(STR_LIT("LIBC"));
				}
				if(Info->Flags & CF_Standalone)
				{
					ConfigIDs.Push(STR_LIT("Standalone"));
				}

				g_CompileFlags = Info->Flags;
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
				if(Info->Arch == Arch_Wasm32 || Info->Arch == Arch_Wasm64)
				{
					PTarget = platform_target::Wasm;
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
					case pt::Wasm:
					{
						ConfigIDs.Push(STR_LIT("WASM"));
					} break;
				}

				if(Info->Arch == Arch_x86_64)
				{
					ConfigIDs.Push(STR_LIT("x86"));
					ConfigIDs.Push(STR_LIT("x64"));
				}
				else if(Info->Arch == Arch_x86)
				{
					RegisterBitSize = 32;
					ConfigIDs.Push(STR_LIT("x86"));
				}
				else if(Info->Arch == Arch_arm32)
				{
					RegisterBitSize = 32;
					ConfigIDs.Push(STR_LIT("arm32"));
				}
				else if(Info->Arch == Arch_arm64)
				{
					ConfigIDs.Push(STR_LIT("arm64"));
				}
				else if(Info->Arch == Arch_Wasm32)
				{
					RegisterBitSize = 32;
					ConfigIDs.Push(STR_LIT("wasm32"));
					if(Info->TargetTriple.Data == NULL)
					{
						Info->TargetTriple.Data = "wasm32-unknown-unknown";
						Info->TargetTriple.Count = VStrLen(Info->TargetTriple.Data);
					}
				}
				else if(Info->Arch == Arch_Wasm64)
				{
					ConfigIDs.Push(STR_LIT("wasm64"));
					if(Info->TargetTriple.Data == NULL)
					{
						Info->TargetTriple.Data = "wasm64-unknown-unknown";
						Info->TargetTriple.Count = VStrLen(Info->TargetTriple.Data);
					}
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

				string EntryPoint = STR_LIT("main");
				if(Info->EntryPoint.Count != 0)
				{
					EntryPoint = MakeString(Info->EntryPoint.Data, Info->EntryPoint.Count);
				}

				slice<interp_file> CustomModules;
				CustomModules.Data = Info->CustomFiles;
				CustomModules.Count = Info->CustomFilesCount;

				auto _ = SaveTypeTableAndReset();
				AddVectorTypes();
				NeedToRestoreForAfterFunction = true;
				auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("main"), EntryPoint, CustomModules);
				slice<file*> Files = r.Files;
				ModuleArray = r.Modules;
				FileTimer = r.Timers;

				int SaveRegisterBitSize = RegisterBitSize;
				RegisterBitSize = sizeof(void*) * 8;

				function *ASTFunction = FindFunction(BuildModule, STR_LIT("inspect_ast"));
				if(ASTFunction)
				{
					saved_type_table CompileTypeTable = SaveTypeTableAndReset();
					RestoreTypeTable(BuildTimeTypeTable);
					u32 ASTNodeT = FindStructCanFail(STR_LIT("ast.Node"));
					if(ASTNodeT != Basic_error)
					{
						if(g_InterpreterTrace)
							LINFO("Interpreting after_link function");

						PlatformSetSignalHandler(InterpSegFault, &BuildVM);
						BuildVM.HasSetSigHandler = true;

						for(file *File : r.Files)
						{
							interp_slice Arg = NodeToInterpSlice(SliceFromArray(File->Nodes));
							value ArgValue = {};
							ArgValue.Type = GetSliceType(ASTNodeT);
							ArgValue.ptr = &Arg;
							InterpretFunction(&BuildVM, *ASTFunction, {&ArgValue, 1});
						}


						PlatformSetSignalHandler(DefaultSignalHandler, NULL);
					}
					RestoreTypeTable(CompileTypeTable);
				}

				// Remake vm to evaluate enums with new info

				timer_group VMBuildTimer2 = VLibStartTimer("VM");

				TypeTableInvalidateSizeCaches();
				interpreter ComptimeVM = {};
				MakeInterpreter(ComptimeVM, ModuleArray, 0);
				PlatformSetSignalHandler(DefaultSignalHandler, NULL);
				if(HasErroredOut())
					exit(1);

				RegisterBitSize = SaveRegisterBitSize;
				VLibStopTimer(&VMBuildTimer2);
				VMBuildTimers2.Push(VMBuildTimer2);

				if(!g_StopCompileOutput)
				{
					TypeTableInvalidateSizeCaches();
					FileTimer.LLVM = VLibStartTimer("LLVM");
					RCGenerateCode(CurrentPipeline.Queue, ModuleArray, Files, CommandLine.Flags, Info, ComptimeVM.StoredGlobals);
#if 0
					{
						InitX86OpUsage();
						slice<op_reg_usage> u = {OpUsagex86, ARR_LEN(OpUsagex86)};

						slice<uint> FnCallRegisters = SliceFromConst<uint>({
								2, 3, 6, 7
								});

						reg_allocator r = MakeRegisterAllocator(u, 11, FnCallRegisters);
						For(Files)
						{
							AllocateRegisters(&r, (*it)->IR);
						}
					}
#endif
					VLibStopTimer(&FileTimer.LLVM);
				}
				ComptimeVM.StackAllocator.Free();

				Timers.Push(FileTimer);
				if(DumpingInfo)
				{
					WriteCTags(ModuleArray);
				}

				auto LinkTimer = VLibStartTimer("Linking");
				RunLinker(Info, CommandLine, ModuleArray);
				VLibStopTimer(&LinkTimer);
				LinkTimers.Push(LinkTimer);

				/* Clean up */
				if((Info->Flags & CF_NoLink) == 0 && !g_StopCompileOutput)
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
			}
		}
	}
	else
	{
		// @TODO: maybe actually check the host machine?
		ConfigIDs.Push(STR_LIT("x64"));
		ConfigIDs.Push(STR_LIT("x86"));
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
			case pt::Wasm:
			{
				ConfigIDs.Push(STR_LIT("WASM"));
			} break;
		}
		CommandLine.Flags |= CF_DebugInfo;
		g_CompileFlags |= CF_DebugInfo;

		timers FileTimer = {};

		dynamic<string> FileNames = {};
		FileNames.Push(CommandLine.SingleFile);
		AddStdFiles(FileNames, false, {});

		auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("main"), STR_LIT("main"));
		slice<file*> Files = r.Files;
		ModuleArray = r.Modules;
		FileTimer = r.Timers;

		MakeInterpreter(BuildVM, ModuleArray, 100);
		if(HasErroredOut())
			exit(1);

		compile_info *Info = NewType(compile_info);
		if(!g_StopCompileOutput)
		{
			FileTimer.LLVM = VLibStartTimer("LLVM");
			RCGenerateCode(CurrentPipeline.Queue, ModuleArray, Files, CommandLine.Flags, Info, BuildVM.StoredGlobals);
			VLibStopTimer(&FileTimer.LLVM);
		}
		BuildVM.StackAllocator.Free();
		Timers.Push(FileTimer);
		if(DumpingInfo)
		{
			WriteCTags(ModuleArray);
		}
		
		auto LinkTimer = VLibStartTimer("Linking");
		RunLinker(Info, CommandLine, ModuleArray);
		VLibStopTimer(&LinkTimer);
		LinkTimers.Push(LinkTimer);
		
		/* Clean up */
		if((Info->Flags & CF_NoLink) == 0 && !g_StopCompileOutput)
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
	}


	function *AfterFunction = FindFunction(BuildModule, STR_LIT("after_link"));
	if(AfterFunction)
	{
		if(NeedToRestoreForAfterFunction)
		{
			RegisterBitSize = sizeof(void*) * 8;
			RestoreTypeTable(BuildTimeTypeTable);
		}
		if(g_InterpreterTrace)
			LINFO("Interpreting after_link function");

		PlatformSetSignalHandler(InterpSegFault, &BuildVM);
		BuildVM.HasSetSigHandler = true;

		interp_slice Objs = {};
		Objs.Count = ModuleArray.Count;
		Objs.Data = AllocatePermanent(sizeof(interp_string) * Objs.Count);
		ForArray(Idx, ModuleArray)
		{
			string_builder Builder = MakeBuilder();
			Builder += ModuleArray[Idx]->Name;
			Builder += ".obj";
			string Path = MakeString(Builder);
			((interp_string *)Objs.Data)[Idx] = {Path.Size, Path.Data};
		}


		value ObjsValue = {};
		ObjsValue.Type = GetSliceType(Basic_string);
		ObjsValue.ptr = &Objs;

		InterpretFunction(&BuildVM, *AfterFunction, {&ObjsValue, 1});

		PlatformSetSignalHandler(DefaultSignalHandler, NULL);
	}

	BuildVM.StackAllocator.Pop();

	i64 ParseTime = 0;
	i64 TypeCheckTime = 0;
	i64 IRBuildTime = 0;
	i64 FlowTypingTime = 0;
	i64 LLVMTime = 0;
	i64 LinkTime = 0;
	i64 VMTimer1 = 0;
	i64 VMTimer2 = 0;

	ForArray(Idx, Timers)
	{
		ParseTime     += TimeTaken(&Timers.Data[Idx].Parse);
		TypeCheckTime += TimeTaken(&Timers.Data[Idx].TypeCheck);
		IRBuildTime   += TimeTaken(&Timers.Data[Idx].IR);
		FlowTypingTime+= TimeTaken(&Timers.Data[Idx].FlowTyping);
		LLVMTime      += TimeTaken(&Timers.Data[Idx].LLVM);
	}
	ForArray(Idx, LinkTimers)
	{
		LinkTime += TimeTaken(&LinkTimers.Data[Idx]);
	}
	ForArray(Idx, VMBuildTimers)
	{
		VMTimer1 += TimeTaken(&VMBuildTimers.Data[Idx]);
	}
	ForArray(Idx, VMBuildTimers2)
	{
		VMTimer2 += TimeTaken(&VMBuildTimers2.Data[Idx]);
	}

	if(CommandLine.Flags & CommandFlag_time)
	{
		// @Note: Should probably have a better name for a function that logs without formatting...
		// Vasko - 22/09/2026
		LogCompilerError("Compiling Finished...\n");
		LogCompilerError("Parsing:                   %lldms\n", ParseTime                / 1000);
		LogCompilerError("Type Checking:             %lldms\n", TypeCheckTime            / 1000);
		LogCompilerError("Intermediate Generation:   %lldms\n", IRBuildTime              / 1000);
		LogCompilerError("Flow Typing:               %lldms\n", FlowTypingTime           / 1000);
		LogCompilerError("Interpreting Build File:   %lldms\n", VMTimer1                 / 1000);
		LogCompilerError("Compile Time Evaluation:   %lldms\n", VMTimer2                 / 1000);
		LogCompilerError("LLVM Code Generation:      %lldms\n", LLVMTime                 / 1000);
		LogCompilerError("Linking:                   %lldms\n", LinkTime                 / 1000);
	}

	FreeAllArenas();
	return 0;
}

const char* GetTokenName(token_type Token) {
    switch (Token) {
        case T_PLUS:         return "+";
        case T_MINUS:        return "-";
        case T_PTR:          return "*";
        case T_ADDROF:       return "&"; // & T_AND
        case T_DECL:         return ":";
        case T_STARTSCOPE:   return "{";
        case T_ENDSCOPE:     return "}";
        case T_OPENPAREN:    return "(";
        case T_CLOSEPAREN:   return ")";
        case T_OPENBRACKET:  return "[";
        case T_CLOSEBRACKET: return "]";
        case T_EQ:           return "=";
        case T_LESS:         return "<";
        case T_GREAT:        return ">";
        case T_COMMA:        return ",";
        case T_DOT:          return ".";
        case T_QMARK:        return "?";
        case T_BANG:         return "!";
        case T_SEMICOL:      return ";";
        case T_DOLLAR:       return "$";
        case T_DIV:          return "/";
        case T_BITNOT:       return "~";
        case T_EOF:          return "end of file";
        case T_ID:           return "identifier";
        case T_IF:           return "if";
        case T_ELSE:         return "else";
        case T_FOR:          return "for";
        case T_VAL:          return "number";
        case T_STR:          return "string";
        case T_NEQ:          return "!=";
        case T_GEQ:          return ">=";
        case T_LEQ:          return "<=";
        case T_EQEQ:         return "==";
        case T_ARR:          return "->";
        case T_PPLUS:        return "++";
        case T_MMIN:         return "--";
        case T_LOR:          return "||";
        case T_LAND:         return "&&";
        case T_SLEFT:        return "<<";
        case T_SRIGHT:       return ">>";
        case T_PEQ:          return "+=";
        case T_MEQ:          return "-=";
        case T_TEQ:          return "*=";
        case T_DEQ:          return "/=";
        case T_MODEQ:        return "%=";
        case T_SLEQ:         return "<<=";
        case T_SREQ:         return ">>=";
        case T_ANDEQ:        return "&=";
        case T_XOREQ:        return "^=";
        case T_OREQ:         return "|=";
        case T_FN:           return "fn";
        case T_CONST:        return "::";
        case T_SHADOW:       return "#shadow";
        case T_RETURN:       return "return";
        case T_FOREIGN:      return "#foreign";
        case T_CSTR:         return "c string";
        case T_STRUCT:       return "struct";
        case T_IMPORT:       return "#import";
        case T_AS:           return "as";
        case T_PUBLIC:       return "#public";
        case T_PRIVATE:      return "#private";
        case T_SIZEOF:       return "size_of";
        case T_IN:           return "in";
        case T_BREAK:        return "break";
        case T_TYPEOF:       return "type_of";
        case T_VARARG:       return "...";
        case T_PWDIF:        return "#if";
        case T_CHAR:         return "character";
        case T_ENUM:         return "enum";
        case T_SWITCH:       return "switch";
        case T_INTR:         return "#intrinsic";
        case T_DEFER:        return "defer";
        case T_LINK:         return "#link";
        case T_UNION:        return "union";
        case T_INFO:         return "type_info";
        case T_EMBED_BIN:    return "#embed_bin";
        case T_EMBED_STR:    return "#embed_str";
        case T_VOID:         return "void";
        case T_CONTINUE:     return "continue";
        case T_PWDELIF:      return "#elif";
        case T_PROFILE:      return "@profile";
        case T_ASSERT:       return "#assert";
        case T_USING:        return "using";
        case T_YIELD:        return "yield";
        case T_RUN:          return "#run";
        case T_LOAD_DL:       return "#load_dl";
        case T_LOAD_SYSTEM_DL:return "#load_system_dl";
        case T_PWDELSE:       return "#else";
        case T_THEN:          return "then";
        case T_INLINE:        return "#inline";
        case T_NEWCAST:       return "cast";
        case T_BITCAST:       return "bit_cast";
        case T_RAWSTRING:     return "```";
        case T_MODULE:        return "module";
        case T_FILE_LOCATION: return "#file_location";
        case T_STATIC:        return "#static";
        case T_NORETURN:      return "#no_return";
        case T_CASE:          return "case";
        case T_WASM_IMPORT:   return "#wasm_import";
        case T_TAG:           return "#tag";
        case T_CALLC:         return "#cc";
        case T_PACK:          return "#pack";
        case T_NOCHECK:       return "#nocheck";
		case T_SELF:          return "#self";
        default: {
            char *C = AllocateString(2);
            C[0] = (char)Token;
            C[1] = 0;
            return C;
        }
    }
}

