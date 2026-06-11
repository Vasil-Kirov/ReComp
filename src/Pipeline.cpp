#include "Pipeline.h"
#include "FlowTyping.h"
#include "IPC.h"
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
#endif

pipeline CurrentPipeline = {};
std::mutex PipelineMutex;
slice<file_substitute> Substitutes;

struct lookup_paths {
	std::mutex Mutex;
	dynamic<string> Paths;
};

lookup_paths Lookups = {};

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

int AnalyzeFilesForSymbols(slice<file*> Files, string EntryModule, string EntryPoint);

void ResetPipelineState()
{
	CurrentPipeline.ParseResults.Results = {};
	CurrentPipeline.StagedFiles.FilePaths = {};
}

pipeline_result RunPipeline(slice<string> InitialFiles, string EntryModule, string EntryPoint, slice<interp_module> CustomModules)
{
	ResetPipelineState();
	binary_blob Blob = StartOutput();
	GlobalBlob = &Blob;

	timers Timers = {};

	// START OF PARSING         --------------------------------------------------
	Timers.Parse = VLibStartTimer("Parsing");
	if(DumpingInfo)
		IPCAtStage |= 1 << 0;

	For(InitialFiles)
	{
		if(!PipelineDoFile(*it))
		{
			LogCompilerError("Error: could not find file %.*s\n", it->Size, it->Data);
			CountError();
		}
	}

	while(!IsQueueDone(CurrentPipeline.Queue))
	{
		TryDoWork(CurrentPipeline.Queue);
	}

	ExitIfErroredOut();
	if(DumpingInfo)
		IPCAtStage |= 1 << 1;

	dynamic<module*> Modules = {};

	For(CurrentPipeline.ParseResults.Results)
	{
		if(it->ModuleName != "")
			AddModule(Modules, it->File, it->ModuleName);
	}
	dict<const string *> CustomModuleFileContents = {};
	size_t CustomFileCount = 0;
	For(CustomModules)
	{
		if(it->FileCount <= 0)
			continue;
		string ModuleName = StringFromInterp(it->Name);
		for(int j = 0; j < it->FileCount; ++j)
		{
			string Name = StringFromInterp(it->Files[j]);
			string Found = FindFile(Name);
			if(Found == "")
			{
				LogCompilerError("Error: could not find file %.*s, given in custom module %.*s\n", Name.Size, Name.Data, ModuleName.Size, ModuleName.Data);
				CountError();
				continue;
			}
			string Data = ReadEntireFile(Found);
			if(Data == "")
			{
				LogCompilerError("Error: could not read file %.*s, given in custom module %.*s\n", Found.Size, Found.Data, ModuleName.Size, ModuleName.Data);
				CountError();
				continue;
			}
			CustomModuleFileContents.Add(Found, DupeType(Data, string));
			it->Files[j].Data = Found.Data;
			it->Files[j].Count = Found.Size;
			CustomFileCount++;
		}
	}
	array<file*> FileArray{CurrentPipeline.ParseResults.Results.Count + CustomFileCount};
	size_t AtFileArray = 0;
	For(CustomModules)
	{
		if(it->FileCount <= 0)
			continue;
		string ModuleName = StringFromInterp(it->Name);
		for(int j = 0; j < it->FileCount; ++j)
		{
			file *File = NewType(file);
			*File = {};

			string Name = StringFromInterp(it->Files[j]);
			File->Name = Name;
			AddModule(Modules, File, ModuleName);
			File->Checker = NewType(checker);
			File->Checker->Module = File->Module;
			File->Checker->File   = File->Name;

			File->IR = NewType(ir);
			*File->IR = {};
			for(int NodeI = 0; NodeI < it->NodeCount; ++NodeI)
			{
				node *N = InterpToNode(it->Nodes[NodeI], CustomModuleFileContents);
				if(N)
					File->Nodes.Push(N);
			}
			FileArray[AtFileArray++] = File;
		}
	}

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
		FileArray[Idx+CustomFileCount] = CurrentPipeline.ParseResults.Results[Idx].File;
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

		file *File = FileArray[Idx+CustomFileCount];
		File->Imported = ResolveImports(pr.Imports, Modules, SliceFromArray(FileArray));
		File->Checker->Imported	= File->Imported;
	}

	ExitIfErroredOut();
	if(DumpingInfo)
		IPCAtStage |= 1 << 2;

	CurrentModules = SliceFromArray(Modules);
	slice<file *> Files = SliceFromArray(FileArray);

	int EntryIdx = AnalyzeFilesForSymbols(Files, EntryModule, EntryPoint);

	ExitIfErroredOut();
	if(DumpingInfo)
		IPCAtStage |= 1 << 3;

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


	VLibStopTimer(&Timers.TypeCheck);
	// END OF TYPE CHECKING     --------------------------------------------------

	if(DumpingInfo)
	{
		IPCAtStage |= 1 << 4;
		IPCSetModules(SliceFromArray(Modules));
		IPCListenAndServe();
	}
	ExitIfErroredOut();

	// START OF IR GENERATION   --------------------------------------------------
	Timers.IR = VLibStartTimer("IR");

	g_LastAddedGlobal.store(AssignIRRegistersForModuleSymbols(Modules));

	For(Files)
	{
		(*it)->IR = NewType(ir);
		*(*it)->IR = BuildIR(*it);

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
	// START OF FLOW TYPING     --------------------------------------------------

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

	error_info ErrorInfo = {};
	ErrorInfo.Data = DupeType(FileData, string);
	ErrorInfo.FileName = FilePath.Data;
	ErrorInfo.Range.StartLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndChar = 1;
	file *f = StringToTokens(FileData, ErrorInfo);
	f->Name = FilePath;

	if(!HasErroredOut())
	{
		job Job = {};
		Job.Data = f;
		Job.Task = ParseFile;
		PostJob(CurrentPipeline.Queue, Job);
	}
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

int AnalyzeFilesForSymbols(slice<file*> Files, string EntryModule, string EntryPoint)
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
		slice<node *> NodeSlice = SliceFromArray(File->Nodes);
		AnalyzeEnumDefinitions(NodeSlice, File->Module);
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeEnums(File->Checker, SliceFromArray(File->Nodes));
	}
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeForUserDefinedTypes(File->Checker, SliceFromArray(File->Nodes));
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

	ExitIfErroredOut();

	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		AnalyzeFunctionDecls(File->Checker, &File->Nodes, File->Module);
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
	if(EntryModule.Size && EntryPoint.Size)
	{
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

		if(!FoundModule)
		{
			LogCompilerError("Error: Missing entry module %.*s\n", EntryModule.Size, EntryModule.Data);
			CountError();
		}
		else if(!FoundEntrypoint)
		{
			LogCompilerError("Error: Missing entry point %.*s\n", EntryPoint.Size, EntryPoint.Data);
			CountError();
		}
	}
	return Result;
}

