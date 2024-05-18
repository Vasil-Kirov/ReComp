#pragma once
#include "../String.h"
#include "../Type.h"

void
LLVMCast(string_builder *Builder, u32 Value, char Prefix, const type *From, const type *To);
const char *GetLLVMTypeChar(const type *Type);


