#pragma once
#include "Dynamic.h"
#include "Parser.h"
#include "VString.h"
#include "Threading.h"

struct parse_stage_result {
	dynamic<parse_result> Results;
	std::mutex Mutex;
};

struct staged_files {
	dynamic<string> FilePaths;
	std::mutex Mutex;
};

struct pipeline {
	work_queue *Queue;
	parse_stage_result ParseResults;
	staged_files StagedFiles;
};

struct timers
{
	timer_group Parse;
	timer_group TypeCheck;
	timer_group IR;
	timer_group FlowTyping;
	timer_group LLVM;
};

struct pipeline_result {
	slice<file*> Files;
	slice<module*> Modules;
	timers Timers;
	int EntryFileIdx;
};

string FindFile(string FileName, string RelativePath = STR_LIT(""));
bool PipelineDoFile(string GivenPath, string RelativePath = STR_LIT(""));
pipeline_result RunPipeline(slice<string> InitialFiles, string EntryModule, string EntryPoint);
string GetLookupPathsPrintable(string FileName, string RelativePath);

