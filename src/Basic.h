#pragma once
#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>
#include "vlib.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef wchar_t wchar;

typedef u32 b32;
typedef float f32;
typedef double f64;

#if defined(DEBUG)
#define Assert(expression) if(!(expression)) { LERROR("--- COMPILER BUG ---\nFile: %s\nFunction %s\nLine: %d",\
		__FILE__, __FUNCTION__, __LINE__); PrintStacktrace(); __debugbreak(); __builtin_trap(); }
#else 
#define Assert(expression) {}
#endif

#define BIT(x) (1 << (x))

#define ARR_LEN(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define FOR_ARRAY(ARR, LEN) for(auto It = ARR; It < ARR + LEN; ++It)


static void PrintStacktrace()
{
	printf("\nStack Trace:\n");

	void *stack[128];
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	int frames = CaptureStackBackTrace(1, 128, stack, NULL);

	SYMBOL_INFO *symbol = (SYMBOL_INFO *)VAlloc(sizeof(SYMBOL_INFO) + 256);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	for(int i = 0; i < frames; ++i)
	{
		SymFromAddr(process, (DWORD64)stack[i], 0, symbol);
		printf("\t%s\n", symbol->Name);
	}

	VFree(symbol);
}

