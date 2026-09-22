#include "Pipeline.h"
#include "FlowTyping.h"
#include "Lexer.h"
#include "Memory.h"
#include "Module.h"
#include "Parser.h"
#include "Platform.h"
#include "Semantics.h"
#include "Threading.h"
#include "VString.h"
#include "Interpreter.h"
#include "CommandLine.h"
#include "Globals.h"
#include "DumpInfo.h"
#include "PassAst.h"
#include <mutex>

#if _WIN32
#include <shlwapi.h>
#define MICROSOFT_CRAZINESS_IMPLEMENTATION
#include "microsoft_craziness.h"
#endif

pipeline CurrentPipeline = {};
std::mutex PipelineMutex;
slice<file_substitute> Substitutes;

struct lookup_paths {
	std::mutex Mutex;
	dynamic<string> Paths;
};

lookup_paths Lookups = {};

void LexString(string FilePath, string FileData);

string GetLookupPathsPrintable(string FileName, string RelativePath)
{
	Lookups.Mutex.lock();

	scratch_arena Arena = {};
	char *Buf = (char *)Arena.Allocate(MAX_PATH_LEN);
	char *Absolute = (char *)Arena.Allocate(MAX_PATH_LEN);

	auto b = MakeBuilder();
	ForArray(Idx, Lookups.Paths)
	{
		auto it = Lookups.Paths[Lookups.Paths.Count-Idx-1];
		sprintf(Buf, "%.*s/%.*s", (int)it.Size, it.Data, (int)FileName.Size, FileName.Data);
		char *GotAbsolute = GetAbsolutePath(Buf, Absolute);
		if(GotAbsolute == NULL)
			continue;

		b.printf("\t%s\n", Absolute);
		memset(Absolute, 0, MAX_PATH_LEN);
	}
	//b.printf("\t%.*s/../%.*s\n", (int)RelativePath.Size, RelativePath.Data, (int)FileName.Size, FileName.Data);

	{
		auto b2 = MakeBuilder();
		b2.printf("%.*s/../%.*s\n", (int)RelativePath.Size, RelativePath.Data, (int)FileName.Size, FileName.Data);
		auto str = MakeString(b2);
		char Absolute[512] = {};
		if(GetAbsolutePath(str.Data, Absolute))
			b.printf("\t%s", Absolute);
	}

	Lookups.Mutex.unlock();
	return MakeString(b);
}

#if _WIN32

string Win32FixPathBackslashes(string Path)
{
	auto b = MakeBuilder();
	for(size_t i = 0; i < Path.Size; ++i)
	{
		if(Path.Data[i] == '/')
			b += '\\';
		else
			b += Path.Data[i];
	}
	return MakeString(b);
}

#endif

string FindFile(string FileName, string RelativePath)
{
	scratch_arena Arena = {};
	char *Buf = (char *)Arena.Allocate(MAX_PATH_LEN);
	char *Absolute = (char *)Arena.Allocate(MAX_PATH_LEN);
	Lookups.Mutex.lock();
	ForArray(Idx, Lookups.Paths)
	{
		auto it = Lookups.Paths[Lookups.Paths.Count-Idx-1];
		sprintf(Buf, "%.*s/%.*s", (int)it.Size, it.Data, (int)FileName.Size, FileName.Data);
		char *GotAbsolute = GetAbsolutePath(Buf, Absolute);
		if(GotAbsolute == NULL)
			continue;
		if(PlatformIsPathValid(GotAbsolute))
		{
			Lookups.Mutex.unlock();
			string Result = MakeString(GotAbsolute);
#if _WIN32
			Result = Win32FixPathBackslashes(Result);
#endif
			return Result;
		}
		memset(GotAbsolute, 0, MAX_PATH_LEN);
	}

	if(RelativePath.Size != 0)
	{
		sprintf(Buf, "%.*s/../%.*s", (int)RelativePath.Size, RelativePath.Data, (int)FileName.Size, FileName.Data);
		char *GotAbsolute = GetAbsolutePath(Buf, Absolute);
		if(GotAbsolute != NULL)
		{
			if(PlatformIsPathValid(GotAbsolute))
			{
				Lookups.Mutex.unlock();
				string Result = MakeString(GotAbsolute);
#if _WIN32
			Result = Win32FixPathBackslashes(Result);
#endif
				return Result;
			}
		}
	}

	Lookups.Mutex.unlock();

#if _WIN32
	if(!PathIsRelativeA(FileName.Data))
#elif CM_LINUX
	if(FileName.Size > 0 && FileName.Data[0] == '/')
#endif
	{
		if(PlatformIsPathValid(FileName.Data))
		{
#if _WIN32
			FileName = Win32FixPathBackslashes(FileName);
#endif
			return FileName;
		}
	}

	return STR_LIT("");
}

bool AddLookupPath(string PassPath)
{
	char *CPath = GetAbsolutePath(PassPath.Data);
	if(CPath == NULL)
		return false;

	if(!PlatformIsPathValid(CPath))
	{
		VFree(CPath);
		return false;
	}

	string Path = MakeString(CPath);
	VFree(CPath);

	Lookups.Mutex.lock();

	For(Lookups.Paths)
	{
		if(*it == Path)
		{
			Lookups.Mutex.unlock();
			return true;
		}
	}

	Lookups.Paths.Push(Path);

	Lookups.Mutex.unlock();
	return true;
}

void CreatePipeline()
{
	work_queue *Queue = CreateWorkQueue();
	InitThreadsForQueue(Queue);
	CurrentPipeline.Queue = Queue;
}

extern dynamic<string> ConfigIDs;

int AnalyzeFilesForSymbols(slice<file*> Files, string EntryModule, string EntryPoint, slice<module*> Modules);

void ResetPipelineState()
{
	CurrentPipeline.ParseResults.Results = {};
	CurrentPipeline.StagedFiles.FilePaths = {};
}


void BuildIRJob(void *Arg)
{
	((file *)Arg)->IR = NewType(ir);
	*((file *)Arg)->IR = BuildIR((file *)Arg);
}

pipeline_result RunPipeline(slice<string> InitialFiles, string EntryModule, string EntryPoint, slice<interp_file> CustomFiles)
{
	ResetPipelineState();
	binary_blob Blob = StartOutput();
	GlobalBlob = &Blob;

	timers Timers = {};

	// START OF PARSING         --------------------------------------------------
	Timers.Parse = VLibStartTimer("Parsing");

	For(InitialFiles)
	{
		if(!PipelineDoFile(*it))
		{
			LogCompilerError("Error: could not find file %.*s\n", it->Size, it->Data);
			CountError();
		}
	}

	dict<const string *> CustomModuleFileContents = {};
	For(CustomFiles)
	{
		string Path = StringFromInterp(it->Path);
		string FileData = ReadEntireFile(Path);
		if(FileData.Data == NULL)
		{
			LogCompilerError("Error: Couldn't find file: %.*s\n", Path.Size, Path.Data);
			CountError();
			continue;
		}
		
		CustomModuleFileContents.Add(Path, DupeType(FileData, string));
		LexString(Path, StringFromInterp(it->Content));
	}

	MainThreadWorkUntilDone(CurrentPipeline.Queue);

	dynamic<module*> Modules = {};
	For(CurrentPipeline.ParseResults.Results)
	{
		ForN(CustomFiles, cf)
		{
			if(StringFromInterp(cf->Path) == it->File->Name)
			{
				for(int I = 0; I < cf->NodeCount; ++I)
				{
					node *N = InterpToNode(cf->Nodes[I], CustomModuleFileContents);
					if(N)
						it->Nodes.Push(N);
				}
			}
		}
	}

	For(CurrentPipeline.ParseResults.Results)
	{
		if(it->ModuleName != "")
			AddModule(Modules, it->File, it->ModuleName);
	}
	array<file*> FileArray{CurrentPipeline.ParseResults.Results.Count};

	ForArray(Idx, CurrentPipeline.ParseResults.Results)
	{
		parse_result pr = CurrentPipeline.ParseResults.Results[Idx];
		if(pr.ModuleName == "")
			continue;

		file *File = pr.File;
		File->Nodes = pr.Nodes;
		File->Checker = NewType(checker);
		File->Checker->Module	= File->Module;
		File->Checker->File		= File->Name;
		For(pr.DynamicLibraries)
		{
			g_DLs.Push(*it);
		}
		FileArray[Idx] = CurrentPipeline.ParseResults.Results[Idx].File;
	}

	VLibStopTimer(&Timers.Parse);
	// END OF PARSING           --------------------------------------------------

	// START OF TYPE CHECKING   --------------------------------------------------
	Timers.TypeCheck = VLibStartTimer("Type Checking");

	ForArray(Idx, CurrentPipeline.ParseResults.Results)
	{
		parse_result pr = CurrentPipeline.ParseResults.Results[Idx];
		if(pr.ModuleName == "")
			continue;

		file *File = FileArray[Idx];
		File->Imported = ResolveImports(pr.Imports, Modules, SliceFromArray(FileArray));
		File->Checker->Imported	= File->Imported;
	}

	CurrentModules = SliceFromArray(Modules);
	slice<file *> Files = SliceFromArray(FileArray);

	int EntryIdx = AnalyzeFilesForSymbols(Files, EntryModule, EntryPoint, SliceFromArray(Modules));

	bool FoundInternal = false;
	For(Modules)
	{
		if((*it)->Name == "internal")
		{
			FoundInternal = true;
			CheckInternalModule(*it);
		}
	}
	if(!FoundInternal)
	{
		LogCompilerError("Error: Didn't find internal module, make sure to name it properly if including a custom one!\n");
		exit(1);
	}

	For(Files)
	{
		Analyze((*it)->Checker, (*it)->Nodes);
	}


	if (ToolPipe != -1 && DumpingInfo)
	{
		PipeInfoBlob(&Blob, Files, SliceFromArray(Modules));
		exit(0);
	}
	VLibStopTimer(&Timers.TypeCheck);
	// END OF TYPE CHECKING     --------------------------------------------------

	ExitIfErroredOut();

	// START OF IR GENERATION   --------------------------------------------------
	Timers.IR = VLibStartTimer("IR");

	g_LastAddedGlobal.store(AssignIRRegistersForModuleSymbols(Modules));

	For(Files)
	{
		job Job = {BuildIRJob, *it};
		PostJob(CurrentPipeline.Queue, Job);
	}

	MainThreadWorkUntilDone(CurrentPipeline.Queue);

	For(Files)
	{
		if(ShouldOutputIR((*it)->Module->Name))
		{
			string Dissasembly = Dissasemble((*it)->IR);
			LWARN("[ MODULE %s ]\n\n%s", (*it)->Module->Name.Data, Dissasembly.Data);
		}
	}

	BuildEnumIR();

	VLibStopTimer(&Timers.IR);
	// END OF IR GENERATION     --------------------------------------------------

	// START OF FLOW TYPING     --------------------------------------------------
	Timers.FlowTyping = VLibStartTimer("Flow Typing");

	ForArray(FIdx, Files)
	{
		auto File = Files[FIdx];
		For(File->IR->Functions)
		{
			FlowTypeFunction(it);
		}
	}

	VLibStopTimer(&Timers.FlowTyping);
	ExitIfErroredOut();
	// END OF FLOW TYPING     --------------------------------------------------

	GlobalBlob = NULL;
	return pipeline_result {
		.Files = Files,
		.Modules = SliceFromArray(Modules),
		.Timers = Timers,
		.EntryFileIdx = EntryIdx,
	};

}

void ParseFile(void *File_)
{
	file *File = (file *)File_;
	parse_result Result = ParseTokens(File, SliceFromArray(ConfigIDs));

	CurrentPipeline.ParseResults.Mutex.lock();
	CurrentPipeline.ParseResults.Results.Push(Result);
	CurrentPipeline.ParseResults.Mutex.unlock();
}

void LexString(string FilePath, string FileData)
{;
	error_info ErrorInfo = {};
	ErrorInfo.Data = DupeType(FileData, string);
	ErrorInfo.FileName = FilePath.Data;
	ErrorInfo.Range.StartLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndChar = 1;
	file *f = StringToTokens(FileData, ErrorInfo);
	f->Name = FilePath;

	job Job = {};
	Job.Data = f;
	Job.Task = ParseFile;
	PostJob(CurrentPipeline.Queue, Job);

}

void LexFile(void *FilePath_)
{
	string FilePath = *(string *)FilePath_;
	string FileData = {};
	For(Substitutes)
	{
		if(it->Original == FilePath)
		{
			FileData = it->SubContents;
			break;
		}
	}
	if(FileData.Data == NULL)
	{
		FileData = ReadEntireFile(FilePath);
		if(FileData.Data == NULL)
		{
			LogCompilerError("Error: Couldn't find file: %.*s\n", FilePath.Size, FilePath.Data);
			CountError();
			return;
		}
	}
	LexString(FilePath, FileData);
}

bool PipelineDoFile(string GivenPath, string RelativePath)
{
	string FilePath = FindFile(GivenPath, RelativePath);
	if(FilePath.Size == 0)
	{
		return false;
	}

	CurrentPipeline.StagedFiles.Mutex.lock();
	For(CurrentPipeline.StagedFiles.FilePaths)
	{
		if(*it == FilePath)
		{
			CurrentPipeline.StagedFiles.Mutex.unlock();
			return true;
		}
	}

	CurrentPipeline.StagedFiles.FilePaths.Push(FilePath);
	CurrentPipeline.StagedFiles.Mutex.unlock();

	job Job = {};
	Job.Data = DupeType(FilePath, string);
	Job.Task = LexFile;
	PostJob(CurrentPipeline.Queue, Job);
	return true;
}

error_info *CreateErrorInfoFromInterpLocation(interp_file_location Location, string FullName, const string *FileData)
{
	error_info *ErrI = NewType(error_info);
	ErrI->FileName = FullName.Data;
	ErrI->Data = FileData;
	ErrI->Range.StartLine = Location.line;
	ErrI->Range.EndLine = Location.line;
	ErrI->Range.StartChar = Location.chr;
	ErrI->Range.EndChar = Location.chr+1;

	return ErrI;
}

file *FindFileForCustomModule(string FileName, slice<module*> Modules)
{
	for(module *M : Modules)
	{
		for(file *F : M->Files)
		{
			if(F->Name == FileName)
				return F;
		}
	}
	return nullptr;
}

int AnalyzeFilesForSymbols(slice<file*> Files, string EntryModule, string EntryPoint, slice<module*> Modules)
{
	uint ErrorCount = GetNumErrors();

	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		FindAndReplaceGlobalLambdasWithFunctions(NodeSlice);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		AnalyzeSimpleGlobalVariables(File->Checker, NodeSlice);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		AnalyzeForModuleStructs(NodeSlice, File->Module);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeForUserDefinedTypes(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		AnalyzeEnumDefinitions(File->Checker, NodeSlice, File->Module);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeDefineStructs(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		CheckForRecursiveStructs(File->Checker, SliceFromArray(File->Nodes));
	}

	if (GetNumErrors() > ErrorCount)
	{
		if (ToolPipe != -1 && DumpingInfo)
		{
			PipeInfoBlob(nullptr, Files, Modules);
			exit(1);
		}
	}


	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeFunctionDecls(File->Checker, &File->Nodes, File->Module);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeEnums(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeFillStructCaches(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeGlobalVariables(File->Checker, SliceFromArray(File->Nodes), File->Module);
	}

	int Result = -1;
	b32 FoundModule = false;
	b32 FoundEntrypoint = false;
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		if(File->Module->Name == EntryModule)
		{
			FoundModule = true;
			for(auto [_, sym] : File->Module->Globals)
			{
				if(sym->Flags & SymbolFlag_Function &&
						*sym->Name == EntryPoint)
				{
					FoundEntrypoint = true;
					Result = Idx;
					break;
				}
			}
		}
	}

	if(g_CompileFlags & CF_Standalone)
	{}
	else if(!FoundModule && EntryModule.Size)
	{
		LogCompilerError("Error: Missing entry module %.*s\n", EntryModule.Size, EntryModule.Data);
		CountError();
	}
	else if(!FoundEntrypoint && EntryPoint.Size)
	{
		LogCompilerError("Error: Missing entry point %.*s\n", EntryPoint.Size, EntryPoint.Data);
		CountError();
	}
	return Result;
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

bool is_winsdk_result_valid(Find_Result *r)
{
	return r && r->windows_sdk_version != 0 && r->vs_exe_path && r->windows_sdk_um_library_path && r->windows_sdk_ucrt_library_path && r->vs_library_path;
}

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
		bool valid_sdk = is_winsdk_result_valid(&WinSdk);

		if(valid_sdk)
		{
			LinkCommand.Type = LCT_List;
			Builder.printf("\"%ls/LINK.EXE\" /LIBPATH:\"%ls\" /LIBPATH:\"%ls\" /LIBPATH:\"%ls\" ",
					WinSdk.vs_exe_path, WinSdk.windows_sdk_ucrt_library_path, WinSdk.windows_sdk_um_library_path, WinSdk.vs_library_path);

			Command = QuickBuild("%ls\\LINK.EXE", WinSdk.vs_exe_path);
			Args.Push(QuickBuild("/LIBPATH:\"%ls\"", WinSdk.windows_sdk_um_library_path));
			Args.Push(QuickBuild("/LIBPATH:\"%ls\"", WinSdk.windows_sdk_ucrt_library_path));
			Args.Push(QuickBuild("/LIBPATH:\"%ls\"", WinSdk.vs_library_path));
			if(CompileFlags & CF_SanAdress)
			{
				auto b = MakeBuilder();
				b.printf("%ls\\clang_rt.asan_dynamic-x86_64.dll", WinSdk.vs_exe_path);
				auto s = MakeString(b);
				PlatformCopyFile(s.Data, "clang_rt.asan_dynamic-x86_64.dll");
			}
			free_resources(&WinSdk);
		}
		else
		{
			LogCompilerError("Warning: Could not find windows sdk or visual studio paths, using fallback link command.\n");

			if(CompileFlags & CF_SanAdress)
			{
				LogCompilerError("Warning: Cannot resolve path for address sanitizer dll, please disable it.\n");
				CompileFlags &= ~CF_SanAdress;
			}
			LinkCommand.Type = LCT_System;
			Builder += "LINK.EXE ";
		}


	}

	if(LinkCommand.Type == LCT_List)
	{
		Args.Push(STR_LIT("/nologo"));
		Args.Push(QuickBuild("/OUT:%.*s ", (int)Info->Output.Count, Info->Output.Data));
		if(g_CompileFlags & CF_DebugInfo)
			Args.Push(STR_LIT("/DEBUG"));
	}
	else
	{
		Builder += "/nologo ";
		Builder.printf("/OUT:%.*s ", (int)Info->Output.Count, Info->Output.Data);
		if(g_CompileFlags & CF_DebugInfo)
			Builder += "/DEBUG ";
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

	bool SwitchLibCMT = false;
	if(CompileFlags & CF_SanUndefined)
	{
		string UBsanLib = STR_LIT("clang_rt.ubsan_standalone-x86_64.lib");
		if(LinkCommand.Type == LCT_List)
		{
			Args.Push(UBsanLib);
		}
		else
		{
			Builder += UBsanLib;
			Builder += " ";
		}

		SwitchLibCMT = true;
	}
	if(CompileFlags & CF_SanAdress)
	{
		string AsanLib = STR_LIT("clang_rt.asan_dynamic-x86_64.lib");
		if(LinkCommand.Type == LCT_List)
		{
			Args.Push(AsanLib);
			Args.Push(STR_LIT("/WHOLEARCHIVE:clang_rt.asan_static_runtime_thunk-x86_64.lib"));
		}
		else
		{
			Builder += AsanLib;
			Builder += " ";
			Builder += "/WHOLEARCHIVE:clang_rt.asan_static_runtime_thunk-x86_64.lib ";
		}

		SwitchLibCMT = true;
	}
	if(SwitchLibCMT && (CompileFlags & CF_NoLibC) == 0)
	{
		NoSetDefaultLib = true;
		if(LinkCommand.Type == LCT_List)
			Args.Push(STR_LIT("/DEFAULTLIB:LIBCMT"));
		else
			Builder += "/DEFAULTLIB:LIBCMT ";
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
	if(LinkCommand.Type == LCT_List)
	{
		Command = STR_LIT("ld");
		Args.Push(STR_LIT("ld"));
		Args.Push(STR_LIT("-e"));
		Args.Push(Entry);
	}
	else
	{
		Builder += "ld -e ";
		Builder += Entry;
		Builder += ' ';
	}
	slice<string> ObjFiles = FindObjectFiles();
	if(CompileFlags & CF_NoLibC)
	{
		if(LinkCommand.Type == LCT_List)
		{
			static_assert(false); // @TODO: update with Info->Output
			Args.Push(STR_LIT("-o"));
			Args.Push(STR_LIT("a"));
			Args.Push(STR_LIT("--dynamic-linker=/lib64/ld-linux-x86-64.so.2"));
		}
		else
		{
			static_assert(false); // @TODO: update with Info->Output
			Builder += " -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
		}
	}
	else
	{
		if(LinkCommand.Type == LCT_List)
		{
			Args.Push(STR_LIT("-lc"));
			static_assert(false); // @TODO: update with Info->Output
			Args.Push(STR_LIT("-o"));
			Args.Push(STR_LIT("a"));
			Args.Push(STR_LIT("--dynamic-linker=/lib64/ld-linux-x86-64.so.2"));
			For(ObjFiles)
				Args.Push(*it);
			Args.Push(SystemCallObj);
		}
		else
		{
			static_assert(false); // @TODO: update with Info->Output
			Builder += "-lc -o a --dynamic-linker=/lib64/ld-linux-x86-64.so.2 ";
			For(ObjFiles)
			{
				Builder += *it;
				Builder += " ";
			}

			Builder += SystemCallObj;
			Builder += ' ';
		}
	}
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

void RunLinker(compile_info *Info, command_line CommandLine, slice<module*> ModuleArray)
{
	if((Info->Flags & CF_NoLink) != 0 || g_StopCompileOutput) {}
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
#elif CM_LINUX
				array<char *> Args(Link.List.Args.Count);
				ForArray(Idx, Args)
				{
					Args[Idx] = strndup(Link.List.Args[Idx].Data, Link.List.Args[Idx].Size);
				}
				pid_t pid = fork();
				if(pid == 0)
				{
					execv(Link.List.Command.Data, Args.Data);
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
}

