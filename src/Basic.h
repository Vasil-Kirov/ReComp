#pragma once

#if _WIN32
#include <windows.h>
#include <dbghelp.h>
#else
#include <signal.h>
#include <execinfo.h>
#endif

#include <xmmintrin.h>
#include <stdint.h>

#define UNUSED(param) (void)(param)

union IVEC4 {
    __m128i v;
    int32_t e[4];
};

union VEC4 {
    __m128 v;
    float e[4];
};

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
typedef unsigned int uint;

#if _WIN32
typedef SSIZE_T ssize_t;
#define BREAK __debugbreak()
#else
#define BREAK raise(SIGTRAP)
#endif

#if defined(DEBUG)
#define Assert(expression) if(!(expression)) { LERROR("--- INTERNAL COMPILER ERROR ---\nFile: %s\nFunction %s\nLine: %d",\
		__FILE__, __FUNCTION__, __LINE__); PrintStacktrace(); BREAK; __builtin_trap(); }
#define unreachable { Assert(false); __builtin_unreachable(); }
#else 
#define Assert(expression) {}
#define unreachable { __builtin_unreachable(); }
#endif

#define HAS_FLAG(x, f) (((x) & f) != 0)


#define BIT(x) (1 << (x))

#define ARR_LEN(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define FOR_ARRAY(ARR, LEN) for(auto It = ARR; It < ARR + LEN; ++It)

struct token;
struct node;
struct ir;

#include <stdio.h>
#include <vlib.h>

#if _WIN32
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
#else
static void PrintStacktrace()
{
    void *stack[128];
    size_t size;
    char **strings;
    size_t i;

    // Get the stack trace
    size = backtrace(stack, 128);
    strings = backtrace_symbols(stack, size);

    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    // Print the stack trace
    printf("\nStack Trace:\n");
    for (i = 0; i < size; i++) {
        printf("\t%s\n", strings[i]);
    }


    free(strings);
}
#endif
