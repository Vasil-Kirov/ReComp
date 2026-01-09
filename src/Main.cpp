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
#include "backend/x86.h"

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
#include "backend/x86.cpp"

#endif
#include "ConstVal.cpp"

#if defined(_WIN32)
#include "Win32.cpp"
#define MICROSOFT_CRAZINESS_IMPLEMENTATION
#include "microsoft_craziness.h"

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

enum link_command_type
{
	LCT_List,
	LCT_System,
};

struct link_command
{
	link_command_type Type;
	union
	{
		struct {
			string Command;
			slice<string> Args;
		} List;
		string System;
	};
};

link_command MakeLinkCommand(command_line CMD, slice<module*> Modules, compile_info *Info)
{
	link_command LinkCommand = {};

	string Command = STR_LIT("");
	dynamic<string> Args = {};

	string_builder Builder = MakeBuilder();
	u32 CompileFlags = Info->Flags;
#if _WIN32
	b32 NoSetDefaultLib = false;
	b32 NoSetEntryPoint = false;

	{
		Find_Result WinSdk = find_visual_studio_and_windows_sdk();

		if(WinSdk.windows_sdk_version != 0)
		{
			LinkCommand.Type = LCT_List;
			Builder.printf("\"%ls/LINK.EXE\" /LIBPATH:\"%ls\" /LIBPATH:\"%ls\" /LIBPATH:\"%ls\" ",
					WinSdk.vs_exe_path, WinSdk.windows_sdk_ucrt_library_path, WinSdk.windows_sdk_um_library_path, WinSdk.vs_library_path);

			Command = QuickBuild("%ls\\LINK.EXE", WinSdk.vs_exe_path);
			Args.Push(QuickBuild("/LIBPATH:\"%ls\"", WinSdk.windows_sdk_um_library_path));
			Args.Push(QuickBuild("/LIBPATH:\"%ls\"", WinSdk.windows_sdk_ucrt_library_path));
			Args.Push(QuickBuild("/LIBPATH:\"%ls\"", WinSdk.vs_library_path));

		}
		else
		{
			LinkCommand.Type = LCT_System;
			Builder += "LINK.EXE ";
		}

		free_resources(&WinSdk);
	}

	if(LinkCommand.Type == LCT_List)
	{
		Args.Push(STR_LIT("/nologo"));
		Args.Push(STR_LIT("/OUT:a.exe"));
		if(g_CompileFlags & CF_DebugInfo)
			Args.Push(STR_LIT("/DEBUG"));
	}
	else
	{
		Builder += "/nologo /OUT:a.exe ";
		if(g_CompileFlags & CF_DebugInfo)
			Builder += "/DEBUG";
	}

	if(Info->EntryPoint.Data)
	{
		NoSetEntryPoint = true;
		if(LinkCommand.Type == LCT_List)
		{
			Args.Push(QuickBuild("/ENTRY:%.*s", (int)Info->EntryPoint.Count, Info->EntryPoint.Data));
		}
		else
		{
			Builder.printf("/ENTRY:%.*s ", (int)Info->EntryPoint.Count, Info->EntryPoint.Data);
		}
	}

	if(CompileFlags & CF_SanAdress)
	{
		string Std = MakeString(GetStdDir());
		string AsanLib = GetFilePath(Std, "libs/clang_rt.asan-x86_64.lib ");
		if(LinkCommand.Type == LCT_List)
			Args.Push(AsanLib);
		else
			Builder += AsanLib;

		if((CompileFlags & CF_NoLibC) == 0)
		{
			NoSetDefaultLib = true;
			if(LinkCommand.Type == LCT_List)
				Args.Push(STR_LIT("/DEFAULTLIB:LIBCMT"));
			else
				Builder += "/DEFAULTLIB:LIBCMT ";
		}
	}

	if(CompileFlags & CF_NoLibC)
	{
		NoSetDefaultLib = true;
		if(LinkCommand.Type == LCT_List)
			Args.Push(STR_LIT("/NODEFAULTLIB"));
		else
			Builder += "/NODEFAULTLIB ";

		if(!NoSetEntryPoint)
		{
			if(LinkCommand.Type == LCT_List)
				Args.Push(STR_LIT("/ENTRY:main"));
			else
				Builder += "/ENTRY:main ";
		}
	}
	else if(!NoSetEntryPoint)
	{
		if(LinkCommand.Type == LCT_List)
			Args.Push(STR_LIT("/ENTRY:mainCRTStartup"));
		else
			Builder += "/ENTRY:mainCRTStartup ";
	}

	if(!NoSetDefaultLib)
	{
		if(LinkCommand.Type == LCT_List)
			Args.Push(STR_LIT("/DEFAULTLIB:MSVCRT"));
		else
			Builder += "/DEFAULTLIB:MSVCRT ";
	}

#elif CM_LINUX
	LinkCommand.Type = LCT_System;
	const char *StdDir = GetStdDir();
	string Dir = MakeString(StdDir);

	string SystemCallObj = GetFilePath(Dir, "system_call.o");
	string Entry = STR_LIT("_start");
	if(CompileFlags & CF_NoLibC)
		Entry = STR_LIT("main");

	if(Info->EntryPoint.Count != 0)
		Entry = string { .Data = Info->EntryPoint.Data, .Size = Info->EntryPoint.Count };
	Builder += "ld -e ";
	Builder += Entry;
	if(CompileFlags & CF_NoLibC)
	{
		Builder += " -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
	}
	else
	{
		Builder += " -lc -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
		Builder += FindObjectFiles();
	}
	Builder += SystemCallObj;
	Builder += ' ';
#else
#error Implement Link Command
#endif

	ForArray(Idx, Modules)
	{
		if(LinkCommand.Type == LCT_List)
		{
			Args.Push(QuickBuild("%.*s.obj", (int)Modules[Idx]->Name.Size, Modules[Idx]->Name.Data));
		}
		else
		{
			Builder += Modules[Idx]->Name;
			Builder += ".obj ";
		}
	}

	ForArray(Idx, CMD.LinkArgs)
	{
		if(LinkCommand.Type == LCT_List)
		{
			Args.Push(CMD.LinkArgs[Idx]);
		}
		else
		{
			Builder += CMD.LinkArgs[Idx];
			Builder += ' ';
		}
	}

	if(LinkCommand.Type == LCT_List)
	{
		LinkCommand.List.Command = Command;
		LinkCommand.List.Args = SliceFromArray(Args);
	}
	else
	{
		LinkCommand.System = MakeString(Builder);
	}
	return LinkCommand;
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
	else
	{
		Files.Push(STR_LIT("base.rcp"));
	}

	// Doesn't care about CF_Standalone
	Files.Push(STR_LIT("intrin.rcp"));

	if(Flags & CF_NoLibC)
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
	if(CommandLine.BuildFile.Data == NULL && CommandLine.SingleFile.Data == NULL)
		return 1;

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
	interpreter BuildVM = {};
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

		MakeInterpreter(BuildVM, BuildModules, BuildFile.IR->MaxRegisters);
		if(HasErroredOut())
			exit(1);

		{


			if(InterpreterTrace)
				LINFO("Interpreting compile function");
			interpret_result Result = InterpretFunction(&BuildVM, *CompileFunction, {&InfoValue, 1});
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

			if((Info->Flags & CF_NoLibC) == 0)
			{
				ConfigIDs.Push(STR_LIT("LIBC"));
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

			auto r = RunPipeline(SliceFromArray(FileNames), STR_LIT("main"), EntryPoint);
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
			case pt::Wasm:
			{
				ConfigIDs.Push(STR_LIT("WASM"));
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
		
		MakeInterpreter(BuildVM, ModuleArray, 100);
		if(HasErroredOut())
			exit(1);

		FileTimer.LLVM = VLibStartTimer("LLVM");
		RCGenerateCode(CurrentPipeline.Queue, ModuleArray, Files, CommandLine.Flags, Info, BuildVM.StoredGlobals);
		VLibStopTimer(&FileTimer.LLVM);
		BuildVM.StackAllocator.Free();
	}


	auto LinkTimer = VLibStartTimer("Linking");
	if(Info->Flags & CF_NoLink) {}
	else
	{
		link_command Link = MakeLinkCommand(CommandLine, ModuleArray, Info);
		switch(Link.Type)
		{
			case LCT_List:
			{
#if _WIN32
				auto b = MakeBuilder();
				/*
				b += Link.List.Command;
				b += ' ';
				*/
				b += "LINK.EXE ";

				For(Link.List.Args)
				{
					b += *it;
					b += ' ';
				}

				auto CommandLine = MakeString(b);
				LDEBUG("LINK: (%s) %s", Link.List.Command.Data, CommandLine.Data);

				PROCESS_INFORMATION ProcessInfo = {};
				STARTUPINFOA SInfo = {};
				SInfo.cb = sizeof(STARTUPINFOA);
				if(CreateProcessA(Link.List.Command.Data, (char *)CommandLine.Data, NULL, NULL, true, 0, NULL, NULL, &SInfo, &ProcessInfo))
				{
					WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
					CloseHandle(ProcessInfo.hProcess);
					CloseHandle(ProcessInfo.hThread);
				}
				else
				{
					LogCompilerError("Error: Couldn't spawn process for link command: %s", GetLastError());
				}
#else
#error Implement a way to invoke a proces with the link command
#endif

			} break;
			case LCT_System:
			{
				system(Link.System.Data);
			} break;
		}
	}
	VLibStopTimer(&LinkTimer);

	function *AfterFunction = FindFunction(BuildFileFunctions, STR_LIT("after_link"));
	if(AfterFunction)
	{
		if(InterpreterTrace)
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
	BuildVM.StackAllocator.Pop();

	i64 ParseTime = 0;
	i64 TypeCheckTime = 0;
	i64 IRBuildTime = 0;
	i64 FlowTypingTime = 0;
	i64 LLVMTime = 0;

	ForArray(Idx, Timers)
	{
		ParseTime     += TimeTaken(&Timers.Data[Idx].Parse);
		TypeCheckTime += TimeTaken(&Timers.Data[Idx].TypeCheck);
		IRBuildTime   += TimeTaken(&Timers.Data[Idx].IR);
		FlowTypingTime+= TimeTaken(&Timers.Data[Idx].FlowTyping);
		LLVMTime      += TimeTaken(&Timers.Data[Idx].LLVM);
	}

	if(CommandLine.Flags & CommandFlag_time)
	{
		LWARN("Compiling Finished...");
		LWARN("Parsing:                   %lldms", ParseTime                / 1000);
		LWARN("Type Checking:             %lldms", TypeCheckTime            / 1000);
		LWARN("Intermediate Generation:   %lldms", IRBuildTime              / 1000);
		LWARN("Flow Typing:               %lldms", FlowTypingTime           / 1000);
		LWARN("Interpreting Build File:   %lldms", TimeTaken(&VMBuildTimer) / 1000);
		LWARN("Compile Time Evaluation:   %lldms", TimeTaken(&VMBuildTimer2)/ 1000);
		LWARN("LLVM Code Generation:      %lldms", LLVMTime                 / 1000);
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

