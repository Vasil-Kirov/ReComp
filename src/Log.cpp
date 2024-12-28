#include "Basic.h"
#include "Log.h"
#include <stdarg.h>
#include "Platform.h"
#include <mutex>

std::mutex LogMutex = {};

static const char * const LevelLabels[] = {
	"[FATAL] ", "[ERROR] ", "[WARNING] ", "[INFO] ", "[DEBUG] "
};

static char LogFile[260];
static log_level MinLevel = LOG_INFO;

void
InitializeLogger()
{
#if 0
	platform_get_absolute_path(LogFile);
	vstd_strcat(LogFile, "Errors.log");
	platform_write_file((void *)"", 0, LogFile, true);
#endif
}

void SetLogLevel(log_level Level)
{
	MinLevel = Level;
}

void
Log(log_level Level, const char *Format, ...)
{
	if(Level > MinLevel)
		return;

	string_builder Builder = MakeBuilder();
	PushBuilder(&Builder, LevelLabels[Level]);
	PushBuilder(&Builder, Format);

	string FormatCopy = MakeString(Builder);
	
	char *FinalFormat = (char *)VAlloc(LOG_BUFFER_SIZE);

	va_list Args;
	va_start(Args, Format);
	
	vsnprintf(FinalFormat, LOG_BUFFER_SIZE, FormatCopy.Data, Args);
	
	va_end(Args);

	string_builder PrintBuilder = MakeBuilder();
	PushBuilder(&PrintBuilder, FinalFormat);
	PushBuilder(&PrintBuilder, "\n");
	string Print = MakeString(PrintBuilder);
	
	LogMutex.lock();
	PlatformOutputString(Print, Level);
	LogMutex.unlock();
	
	
	if(Level == LOG_FATAL)
	{
#if DEBUG
		BREAK;
#endif
//		platform_message_box("Error", ToPrint);
		exit(1);
	}
	VFree(FinalFormat);
}

void LogCompilerError(const char *Format, ...)
{

	char *FinalFormat = (char *)VAlloc(LOG_BUFFER_SIZE);

	va_list Args;
	va_start(Args, Format);
	
	vsnprintf(FinalFormat, MB(1), Format, Args);
	
	va_end(Args);

	string_builder PrintBuilder = MakeBuilder();
	PushBuilder(&PrintBuilder, FinalFormat);
	PushBuilder(&PrintBuilder, "\n");
	string Print = MakeString(PrintBuilder);

	LogMutex.lock();
	PlatformOutputString(Print, (log_level)-1);
	LogMutex.unlock();

	VFree(FinalFormat);
}

