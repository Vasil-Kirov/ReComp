#pragma once


#if _WIN32
#define STRSAFE_NO_DEPRECATE

#include <windows.h>
#include <dbghelp.h>
#include "strsafe.h"
#else
#include <signal.h>
#include <execinfo.h>
#endif

#include <xmmintrin.h>
#include <stdint.h>

void PlatformClearSignalHandler();

#define UNUSED(param) (void)(param)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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
#define Assert(expression) do { if(!(expression)) { LERROR("--- INTERNAL COMPILER ERROR ---\nAssertion Failed: %s\nFile: %s\nFunction %s\nLine: %d", #expression,\
		__FILE__, __FUNCTION__, __LINE__); PrintStacktrace(); PlatformClearSignalHandler(); BREAK; __builtin_trap(); } }while(0)
#define unreachable { Assert(false); __builtin_unreachable(); }
#else 
#define Assert(expression) ((void)0)
#define unreachable { __builtin_unreachable(); }
#endif

#define HAS_FLAG(x, f) (((x) & f) != 0)


#define BIT(x) (1 << (x))

#define ARR_LEN(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define FOR_ARRAY(ARR, LEN) for(auto It = ARR; It < ARR + LEN; ++It)

#define SWAP(A, B) { auto tmp_var_name_do_not_use__ = A; A = B; B = tmp_var_name_do_not_use__; }

struct token;
struct node;
struct ir;

#include <stdio.h>
#include <vlib.h>

#if _WIN32

extern HANDLE g_SymProcess;
extern CRITICAL_SECTION g_SymLock;
extern LONG g_SymInitState; // 0 = not initialized, 1 = done

static void EnsureSymbolsInitialized()
{
    if (InterlockedCompareExchange(&g_SymInitState, 1, 0) == 0)
    {
        g_SymProcess = GetCurrentProcess();
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(g_SymProcess, NULL, TRUE);
        InitializeCriticalSection(&g_SymLock);
    }
}

static void PrintStacktrace()
{
	EnsureSymbolsInitialized();

	HANDLE out = GetStdHandle(STD_ERROR_HANDLE);
	const char header[] = {"\nStack Trace:\n"};

	DWORD written;
	WriteFile(out, header, sizeof(header)-1, &written, NULL);

	void *stack[128];

	int frames = CaptureStackBackTrace(1, 128, stack, NULL);

	unsigned char symbol_buf[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO *symbol = (SYMBOL_INFO *)&symbol_buf[0];
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	char line[512];
	char *bufend = &line[0];
	if (TryEnterCriticalSection(&g_SymLock))
	{
		for(int i = 0; i < frames; ++i)
		{
			if (SymFromAddr(g_SymProcess, (DWORD64)stack[i], 0, symbol))
			{
				StringCbPrintfExA(line, 512, &bufend, NULL, 0, "\t%.*s\n", symbol->NameLen, &symbol->Name[0]);
				WriteFile(out, line, bufend-line, &written, NULL);
			}
			else
			{
				StringCbPrintfExA(line, 512, &bufend, NULL, 0, "\t0x%p\n", stack[i]);
				WriteFile(out, line, bufend-line, &written, NULL);
			}
		}
		LeaveCriticalSection(&g_SymLock);
	}
	else
	{
		for(int i = 0; i < frames; ++i)
		{
			StringCbPrintfExA(line, 512, &bufend, NULL, 0, "\t0x%p\n", stack[i]);
			WriteFile(out, line, bufend-line, &written, NULL);
		}
	}
}
#else
static void PrintStacktrace()
{
    void *stack[128];
    size_t size;
    size_t i;

    // Get the stack trace
    size = backtrace(stack, 128);

    // Print the stack trace
    static const char header[] = "\nStack Trace:\n";
    write(STDERR_FILENO, header, sizeof(header) - 1);
	backtrace_symbols_fd(stack, size, STDERR_FILENO);
}
#endif
