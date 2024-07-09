#pragma once
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
		__FILE__, __FUNCTION__, __LINE__); __debugbreak(); __builtin_trap(); }
#else 
#define Assert(expression) {}
#endif

#define BIT(x) (1 << (x))

#define ARR_LEN(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define FOR_ARRAY(ARR, LEN) for(auto It = ARR; It < ARR + LEN; ++It)


