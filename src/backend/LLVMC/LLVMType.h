#pragma once
#include <Type.h>
#include <llvm-c/Core.h>

struct LLVMTypeEntry
{
	u32 TypeID;
	LLVMTypeRef LLVMRef;
};

struct LLVMDebugMetadataEntry
{
	u32 TypeID;
	LLVMMetadataRef Ref;
};

struct generator;

LLVMTypeRef ConvertToLLVMType(LLVMContextRef Context, u32 TypeID);

void LLVMCreateOpaqueStringStructType(LLVMContextRef Context, u32 TypeID);
void LLVMCreateOpaqueStructType(LLVMContextRef Context, u32 TypeID);
LLVMTypeRef LLVMDefineStructType(LLVMContextRef Context, u32 TypeID);
LLVMTypeRef LLVMCreateFunctionType(LLVMContextRef Context, u32 TypeID);
void LLMVDebugOpaqueStruct(generator *gen, u32 TypeID);
LLVMMetadataRef LLMVDebugDefineStruct(generator *gen, u32 TypeID);
LLVMMetadataRef ToDebugTypeLLVM(generator *gen, u32 TypeID);

LLVMOpcode RCCast(const type *From, const type *To);
void LLVMClearTypeMap();
void LLVMMapType(u32 TypeID, LLVMTypeRef LLVMType);
void LLVMDebugMapType(u32 TypeID, LLVMMetadataRef LLVMType);

enum {
	DW_ATE_address       = 1,
	DW_ATE_boolean       = 2,
	DW_ATE_float         = 4,
	DW_ATE_signed        = 5,
	DW_ATE_signed_char   = 6,
	DW_ATE_unsigned      = 7,
	DW_ATE_unsigned_char = 8,
};


