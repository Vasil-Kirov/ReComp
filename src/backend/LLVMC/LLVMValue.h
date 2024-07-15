#pragma once
#include <Dynamic.h>
#include <llvm-c/Types.h>

struct value_entry
{
	u32 Register;
	LLVMValueRef Value;
};

struct value_map
{
	dynamic<value_entry> Data;
	size_t Bottom = 0;

	void Add(u32 Register, LLVMValueRef Value);
	LLVMValueRef Get(u32);
	void Clear();
	void LockBottom();
};

