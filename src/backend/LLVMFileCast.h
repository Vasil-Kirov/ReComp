#pragma once
#include <VString.h>
#include <Type.h>

void
LLVMCast(string_builder *Builder, const char *Value, char Prefix, const type *From, const type *To);
const char *GetLLVMTypeChar(const type *Type);


