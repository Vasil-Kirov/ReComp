/* date = January 26th 2022 0:24 pm */

#ifndef _LOG_H
#define _LOG_H

typedef enum _log_level
{
	LOG_FATAL,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
} log_level;

void Log(log_level Level, const char *Format, ...);
void SetLogLevel(log_level);

void InitializeLogger();

#define LFATAL(Format, ...) Log(LOG_FATAL, Format, ##__VA_ARGS__)
#define LERROR(Format, ...) Log(LOG_ERROR, Format, ##__VA_ARGS__)
#define LWARN(Format, ...)  Log(LOG_WARN, Format, ##__VA_ARGS__)
#define LINFO(Format, ...)  Log(LOG_INFO, Format, ##__VA_ARGS__)

#if defined(DEBUG)
#define LDEBUG(Format, ...) Log(LOG_DEBUG, Format, ##__VA_ARGS__)
#else
#define LDEBUG(Format, ...)
#endif

#endif //_LOG_H
