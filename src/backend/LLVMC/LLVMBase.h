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
	LLVMTargetDataRef data;
	value_map map;
	LLVMValueRef fn;
	rc_block *blocks;
	int CurrentBlock;
	b32 IsCurrentFnRetInPtr;
};

rc_block RCCreateBlock(generator *gen, u32 ID, b32 Set = true);
void RCSetBlock(generator *gen, int Index);
void RCEmitFile(LLVMTargetMachineRef Machine, LLVMModuleRef Mod, string ModuleName, b32 OutputBC);
void RCGenerateFunction(generator *gen, function fn);


