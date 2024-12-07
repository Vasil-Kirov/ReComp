#include "Basic.h"
#include "Log.h"
#include <stdarg.h>
#include "Platform.h"

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
	
	char *FinalFormat = (char *)VAlloc(MB(1));

	va_list Args;
	va_start(Args, Format);
	
	vsnprintf(FinalFormat, MB(1), FormatCopy.Data, Args);
	
	va_end(Args);

	string_builder PrintBuilder = MakeBuilder();
	PushBuilder(&PrintBuilder, FinalFormat);
	PushBuilder(&PrintBuilder, "\n");
	string Print = MakeString(PrintBuilder);
	
	PlatformOutputString(Print, Level);
	
	if(Level < LOG_WARN || Level == LOG_DEBUG)
	{
#if 0
		platform_write_file(ToPrint, (i32)vstd_strlen(ToPrint), LogFile, false);
#endif
	}
	
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

