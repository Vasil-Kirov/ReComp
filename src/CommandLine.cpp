#include "CommandLine.h"
#include "Basic.h"
#include "Log.h"
#include "VString.h"

extern bool InterpreterTrace;
extern bool NoThreads;
slice<string> GlobalIRModules;

const char *HELP = R"#(
USAGE: rcp.exe [options] build.rcp

OPTIONS:
	--help
		Displays this message
	--time
		Show how long each step of the compilation process took
	--vmdll my_file.dll
		Add Shared Library file to be used by the interpreter
	--ir module
		Show the internal representation for the specified module
	--llvm
		Output llvm ir files
	--log
		Output debug prints
	--interp-trace
		Print the location of the interpreter, useful for debugging crashes
	--file name
		Compile single code file instead of build file
	--dump-info
		Dumps info about the compilation in a binary format, useful for tools. Info file is called rcp.dump
	--no-thread
		Disables multi threading
)#";

command_line ParseCommandLine(int ArgCount, char *CArgs[])
{
	string Args[64] = {};
	Assert(ArgCount < 64);
	for(int i = 1; i < ArgCount; ++i)
	{
		Args[i - 1] = MakeString(CArgs[i]);
	}
	ArgCount--;
	command_line Result = {};

	const string CompileCommands[] = {
		STR_LIT("--vmdll"),
		STR_LIT("--time"),
		STR_LIT("--help"),
		STR_LIT("--ir"),
		STR_LIT("--link"),
		STR_LIT("--llvm"),
		STR_LIT("--log"),
		STR_LIT("--interp-trace"),
		STR_LIT("--file"),
		STR_LIT("--dump-info"),
		STR_LIT("--no-thread"),
	};

	dynamic<string> ImportDLLs = {};
	dynamic<string> LinkCMDs = {};
	dynamic<string> IRModules = {};
	for(int i = 0; i < ArgCount; ++i)
	{
		string Arg = Args[i];
		if(Arg.Size < 1)
		{
			LFATAL("Invalid argument %s", Arg.Data);
			RET_EMPTY(command_line);
		}

		if(Result.Flags & CommandFlag_link)
		{
			LinkCMDs.Push(Arg);
			continue;
		}

		if(StringsMatchNoCase(Arg, CompileCommands[0]))
		{
			if(i + 1 == ArgCount)
			{
				LFATAL("Expected shared library name after --vmdll", Arg.Data);
				RET_EMPTY(command_line);
			}
			i++;
			ImportDLLs.Push(Args[i]);
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[1]))
		{
			Result.Flags |= CommandFlag_time;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[2]))
		{
			LINFO(HELP);
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[3]))
		{
			if(i + 1 == ArgCount)
			{
				LFATAL("Expected module name after --ir", Arg.Data);
				RET_EMPTY(command_line);
			}
			i++;
			IRModules.Push(Args[i]);
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[4]))
		{
			Result.Flags |= CommandFlag_link;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[5]))
		{
			Result.Flags |= CommandFlag_llvm;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[6]))
		{
			SetLogLevel(LOG_DEBUG);
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[7]))
		{
			InterpreterTrace = true;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[8]))
		{
			if(i + 1 == ArgCount)
			{
				LFATAL("Expected file name after --file");
			}
			i++;
			Result.SingleFile = Args[i];
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[9]))
		{
			Result.Flags |= CommandFlag_dumpinfo;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[10]))
		{
			Result.Flags |= CommandFlag_nothread;
			NoThreads = true;
		}
		else
		{
			if(Result.BuildFile.Data != NULL)
			{
				LFATAL("Multiple build files passed %s and %s",
						Result.BuildFile.Data, Arg.Data);
				RET_EMPTY(command_line);
			}
			Result.BuildFile = Arg;
		}
	}
	if(Result.BuildFile.Data == NULL && Result.SingleFile.Data == NULL)
	{
		LFATAL("No input file");
		RET_EMPTY(command_line);
	}
	if(Result.BuildFile.Data != NULL && Result.SingleFile.Data != NULL)
	{
		LFATAL("Build file %s passed despite --file flag", Result.BuildFile.Data);
	}

	Result.LinkArgs = LinkCMDs;
	Result.ImportDLLs = SliceFromArray(ImportDLLs);
	Result.IRModules = SliceFromArray(IRModules);

	GlobalIRModules = Result.IRModules;
	return Result;
}

bool ShouldOutputIR(string MName)
{
	For(GlobalIRModules)
	{
		if(*it == MName)
			return true;
	}
	return false;
}


