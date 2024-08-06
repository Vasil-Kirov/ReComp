#pragma once
#include <Type.h>
#include <llvm-c/Core.h>

struct LLVMTypeEntry
{
	u32 TypeID;
	LLVMTypeRef LLVMRef;
};

LLVMTypeRef ConvertToLLVMType(LLVMContextRef Context, u32 TypeID);

void LLVMCreateOpaqueStringStructType(LLVMContextRef Context, u32 TypeID);
void LLVMCreateOpaqueStructType(LLVMContextRef Context, u32 TypeID);
LLVMTypeRef LLVMDefineStructType(LLVMContextRef Context, u32 TypeID);
LLVMTypeRef LLVMCreateFunctionType(LLVMContextRef Context, u32 TypeID);

LLVMOpcode RCCast(const type *From, const type *To);
void LLVMClearTypeMap();
void LLVMMapType(u32 TypeID, LLVMTypeRef LLVMType);



