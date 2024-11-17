#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>


__declspec(dllexport)
int mprintf(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int result = vprintf(fmt, args);

	va_end(args);
	return result;
}

