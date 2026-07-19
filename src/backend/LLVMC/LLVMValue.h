#pragma once
#include "Dict.h"
#include <Dynamic.h>
#include <llvm-c/Types.h>

struct value_map
{
	map_int<LLVMValueRef> Data;
	size_t Bottom = 0;

	void Add(u32 Register, LLVMValueRef Value);
	void AddOrReplace(u32 Register, LLVMValueRef Value);
	LLVMValueRef Get(u32);
	void Clear();
};

