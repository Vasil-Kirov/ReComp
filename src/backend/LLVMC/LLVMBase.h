#pragma once
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include "LLVMValue.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include <IR.h>

struct rc_block
{
	LLVMBasicBlockRef Block;
};

// Using short names here because these will be used everywhere
struct generator
{
	LLVMContextRef ctx; // Context
	LLVMModuleRef  mod; // Module
	LLVMBuilderRef bld; // Builder
	LLVMDIBuilderRef dbg; // Debug Info Builder
	LLVMMetadataRef f_dbg; // Metadata for current file
	LLVMTargetDataRef data;
	value_map map;
	LLVMValueRef fn;
	rc_block *blocks;
	LLVMMetadataRef CurrentLocation;
	LLVMMetadataRef CurrentScope;
	int CurrentBlock;
	int BlockCount;
	b32 IsCurrentFnRetInPtr;
	u32 StructMembersPassed;
};

struct llvm_init_info
{
	LLVMTargetMachineRef Target;
	LLVMContextRef Context;
};

rc_block RCCreateBlock(generator *gen, u32 ID, b32 Set = true);
void RCSetBlock(generator *gen, int Index);
void RCEmitFile(LLVMTargetMachineRef Machine, LLVMModuleRef Mod, string ModuleName, b32 OutputBC);
void RCGenerateFunction(generator *gen, function fn);
LLVMValueRef RCGenerateMainFn(generator *gen, slice<file*> Files, LLVMValueRef InitFn);

