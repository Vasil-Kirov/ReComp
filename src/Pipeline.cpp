#include "Pipeline.h"
#include "Lexer.h"
#include "Memory.h"
#include "Module.h"
#include "Parser.h"
#include "Platform.h"
#include "Semantics.h"
#include "Threading.h"
#include "VString.h"
#include "Interpreter.h"
#include <mutex>

pipeline CurrentPipeline = {};
std::mutex PipelineMutex;

struct lookup_paths {
	std::mutex Mutex;
	dynamic<string> Paths;
};

lookup_paths Lookups = {};

string GetLookupPathsPrintable(string FileName)
{
	Lookups.Mutex.lock();

	auto b = MakeBuilder();
	For(Lookups.Paths)
	{
		b.printf("\t%.*s/%.*s\n", (int)it->Size, it->Data, (int)FileName.Size, FileName.Data);
	}

	Lookups.Mutex.unlock();
	return MakeString(b);
}

string FindFile(string FileName)
{
	scratch_arena Arena = {};
	char *Buf = (char *)Arena.Allocate(MAX_PATH_LEN);
	char *Absolute = (char *)Arena.Allocate(MAX_PATH_LEN);
	Lookups.Mutex.lock();
	For(Lookups.Paths)
	{
		sprintf(Buf, "%.*s/%.*s", (int)it->Size, it->Data, (int)FileName.Size, FileName.Data);
		char *GotAbsolute = GetAbsolutePath(Buf, Absolute);
		if(GotAbsolute == NULL)
			continue;
		if(PlatformIsPathValid(GotAbsolute))
		{
			Lookups.Mutex.unlock();
			string Result = MakeString(GotAbsolute);
			return Result;
		}
		memset(GotAbsolute, 0, MAX_PATH_LEN);
	}
	Lookups.Mutex.unlock();
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

pipeline_result RunPipeline(slice<string> InitialFiles, string EntryModule, string EntryPoint)
{
	ResetPipelineState();
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

	if(HasErroredOut())
		exit(1);

	dynamic<module*> Modules = {};

	For(CurrentPipeline.ParseResults.Results)
	{
		AddModule(Modules, it->File, it->ModuleName);
	}

	array<file*> FileArray{CurrentPipeline.ParseResults.Results.Count};
	ForArray(Idx, CurrentPipeline.ParseResults.Results)
	{
		parse_result pr = CurrentPipeline.ParseResults.Results[Idx];
		file *File = pr.File;
		File->Nodes = pr.Nodes;
		File->Checker = NewType(checker);
		File->Checker->Module	= File->Module;
		File->Checker->File		= File->Name;
		For(pr.DynamicLibraries)
		{
			DLs.Push(*it);
		}
		FileArray[Idx] = CurrentPipeline.ParseResults.Results[Idx].File;
	}

	ForArray(Idx, CurrentPipeline.ParseResults.Results)
	{
		parse_result pr = CurrentPipeline.ParseResults.Results[Idx];
		file *File = FileArray[Idx];
		File->Imported = ResolveImports(pr.Imports, Modules, SliceFromArray(FileArray));
		File->Checker->Imported	= File->Imported;
	}

	if(HasErroredOut())
		exit(1);

	CurrentModules = SliceFromArray(Modules);
	slice<file *> Files = SliceFromArray(FileArray);

	int EntryIdx = AnalyzeFilesForSymbols(Files, EntryModule, EntryPoint);

	if(HasErroredOut())
		exit(1);

	For(Files)
	{
		Analyze((*it)->Checker, (*it)->Nodes);
	}

	if(HasErroredOut())
		exit(1);

	AssignIRRegistersForModuleSymbols(Modules);

	For(Files)
	{
		(*it)->IR = NewType(ir);
		*(*it)->IR = BuildIR(*it);
	}
	BuildEnumIR(SliceFromArray(Modules));

	return pipeline_result {
		.Files = Files,
		.Modules = SliceFromArray(Modules),
		.Timers = {},
		.EntryFileIdx = EntryIdx,
	};

		//if(ShouldOutputIR((*it)->Module->Name, CommandLine))
		//{
		//	string Dissasembly = Dissasemble(File->IR);
		//	LWARN("[ MODULE %s ]\n\n%s", File->Module->Name.Data, Dissasembly.Data);
		//}
}

void ParseFile(void *File_)
{
	file *File = (file *)File_;
	parse_result Result = ParseTokens(File, SliceFromArray(ConfigIDs));
	LDEBUG("%s: node_count %d", File->Name.Data, Result.Nodes.Count);

	CurrentPipeline.ParseResults.Mutex.lock();
	CurrentPipeline.ParseResults.Results.Push(Result);
	CurrentPipeline.ParseResults.Mutex.unlock();
}

void LexFile(void *FilePath_)
{
	string *FilePath = (string *)FilePath_;
	string FileData = ReadEntireFile(*FilePath);
	if(FileData.Data == NULL)
	{
		LogCompilerError("Couldn't find file: %.*s\n", FilePath->Size, FilePath->Data);
		CountError();
		return;
	}

	error_info ErrorInfo = {};
	ErrorInfo.Data = DupeType(FileData, string);
	ErrorInfo.FileName = FilePath->Data;
	ErrorInfo.Range.StartLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndChar = 1;
	file *f = StringToTokens(FileData, ErrorInfo);
	f->Name = *FilePath;

	if(!HasErroredOut())
	{
		job Job = {};
		Job.Data = f;
		Job.Task = ParseFile;
		PostJob(CurrentPipeline.Queue, Job);
	}
}

bool PipelineDoFile(string GivenPath)
{
	string FilePath = FindFile(GivenPath);
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

	if(HasErroredOut())
		exit(1);

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
		AnalyzeFunctionDecls(File->Checker, &File->Nodes, File->Module);
		//File->Module->Checker = File->Checker;
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
				ForArray(mi, File->Module->Globals.Data)
				{
					symbol *sym = File->Module->Globals.Data[mi];
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

