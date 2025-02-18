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
	timer_group LLVM;
};

struct pipeline_result {
	slice<file*> Files;
	slice<module*> Modules;
	timers Timers;
	int EntryFileIdx;
};

bool PipelineDoFile(string GivenPath);
pipeline_result RunPipeline(slice<string> InitialFiles, string EntryModule, string EntryPoint);

