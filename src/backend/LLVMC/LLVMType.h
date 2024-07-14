#pragma once
#include <Type.h>
#include <llvm-c/Core.h>

struct LLVMTypeEntry
{
	u32 TypeID;
	LLVMTypeRef LLVMRef;
};

LLVMTypeRef ConvertToLLVMType(LLVMContextRef Context, u32 TypeID);
void LLVMCreateOpaqueStructType(LLVMContextRef Context, u32 TypeID);
void LLVMDefineStructType(LLVMContextRef Context, u32 TypeID);
void LLVMCreateFunctionType(LLVMContextRef Context, u32 TypeID);
LLVMOpcode RCCast(const type *From, const type *To);



