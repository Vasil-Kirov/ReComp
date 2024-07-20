#pragma once
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include "LLVMValue.h"
#include "llvm-c/Target.h"
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
};

rc_block RCCreateBlock(generator *gen, u32 ID, b32 Set = true);
void RCSetBlock(generator *gen, rc_block Block);


void RCGenerateCode(ir *IR);


