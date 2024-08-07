#include "CommandLine.h"
#include "Basic.h"
#include "Log.h"

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

	};

	dynamic<string> ImportDLLs = {};
	for(int i = 0; i < ArgCount; ++i)
	{
		string Arg = Args[i];
		if(Arg.Size < 2)
		{
			LFATAL("Invalid argument %s", Arg.Data);
			RET_EMPTY(command_line);
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

	Result.ImportDLLs = SliceFromArray(ImportDLLs);
	return Result;
}


