#pragma once
#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include "Dict.h"
#include "LLVMValue.h"
#include "LLVMType.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include <IR.h>
#include <unordered_map>

struct rc_block
{
	LLVMBasicBlockRef Block;
};

struct llvm_intrin
{
	LLVMValueRef Fn;
	LLVMTypeRef Type;
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
	value_map global;
	value_map map;
	LLVMValueRef fn;
	function irfn;
	rc_block *blocks;
	LLVMMetadataRef CurrentLocation;
	LLVMMetadataRef CurrentScope;
	int CurrentBlock;
	int BlockCount;
	b32 IsCurrentFnRetInPtr;
	dynamic<LLVMTypeEntry> LLVMTypeMap;
	dynamic<LLVMDebugMetadataEntry> LLVMDebugTypeMap;
	const std::unordered_map<void *, uint> &StoredGlobals;
	dict<llvm_intrin> Intrinsics;
};

struct llvm_init_info
{
	LLVMTargetMachineRef Target;
};

rc_block RCCreateBlock(generator *gen, u32 ID, b32 Set = true);
void RCSetBlock(generator *gen, int Index);
void RCEmitFile(LLVMTargetMachineRef Machine, LLVMModuleRef Mod, string ModuleName, b32 OutputBC);
void RCGenerateFunction(generator *gen, function fn);
LLVMValueRef RCGenerateMainFn(generator *gen, slice<file*> Files, LLVMValueRef InitFn);
llvm_init_info RCInitLLVM(struct compile_info *Info);

#define DEBUG_RUN(x) if(CompileFlags & CF_DebugInfo) { x }

