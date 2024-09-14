#include "CommandLine.h"
#include "Basic.h"
#include "Log.h"
#include "VString.h"

const char *HELP = R"#(
USAGE: rcp.exe [options] build.rcp

OPTIONS:
	--help
		Displays this message
	--time
		Show how long each step of the compilation process took
	--vmdll my_file.dll
		Add Shared Library file to be used by the interpreter
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
	};

	dynamic<string> ImportDLLs = {};
	dynamic<string> LinkCMDs = {};
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
			Result.Flags |= CommandFlag_ir;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[4]))
		{
			Result.Flags |= CommandFlag_link;
		}
		else if(StringsMatchNoCase(Arg, CompileCommands[5]))
		{
			Result.Flags |= CommandFlag_llvm;
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
	if(Result.BuildFile.Data == NULL)
	{
		LFATAL("No input file");
		RET_EMPTY(command_line);
	}

	Result.ImportDLLs = SliceFromArray(ImportDLLs);
	Result.LinkArgs = SliceFromArray(LinkCMDs);
	return Result;
}


