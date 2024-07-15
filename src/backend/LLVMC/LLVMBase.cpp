#include "LLVMBase.h"
#include "ConstVal.h"
#include "IR.h"
#include "Log.h"
#include "LLVMType.h"
#include "Dynamic.h"
#include "Type.h"
#include "Dynamic.h"
#include "llvm-c/Core.h"
#include "llvm-c/Types.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Analysis.h"
#include <stdio.h>

void RCGenerateInstruction(generator *gen, instruction I)
{
#define LLVM_BIN_OP(CAPITAL_OP, Op) \
	case OP_##CAPITAL_OP: \
	{ \
		LLVMValueRef LHS = gen->map.Get(I.Left); \
		LLVMValueRef RHS = gen->map.Get(I.Right); \
		const type *Type = GetType(I.Type); \
		LLVMValueRef Val; \
		if(Type->Basic.Flags & BasicFlag_Float) \
		{ \
			Val = LLVMBuildF##Op(gen->bld, LHS, RHS, ""); \
		} \
		else \
		{ \
			Val = LLVMBuild##Op(gen->bld, LHS, RHS, ""); \
		} \
		gen->map.Add(I.Result, Val);\
	}

#define LLVM_CMP_OP(CAPITAL_OP, O) case OP_##CAPITAL_OP: \
		{ \
			LLVMValueRef LHS = gen->map.Get(I.Left); \
			LLVMValueRef RHS = gen->map.Get(I.Right); \
			const type *Type = GetType(I.Type); \
			LLVMValueRef Val; \
			if(Type->Basic.Flags & BasicFlag_Float) \
			{ \
				Val = LLVMBuildFCmp(gen->bld, LLVMRealO##O, LHS, RHS, ""); \
			} \
			else \
			{ \
				LLVMIntPredicate Op; \
				if(Type->Basic.Flags & BasicFlag_Unsigned) \
					Op = LLVMIntU##O; \
				else \
					Op = LLVMIntS##O; \
				Val = LLVMBuildICmp(gen->bld, Op, LHS, RHS, ""); \
			} \
			gen->map.Add(I.Result, Val);\
		}

	switch(I.Op)
	{
		case OP_NOP:
		{
		} break;
		case OP_CONST:
		{
			// @TODO:
			const_value *Val = (const_value *)I.BigRegister;
			const type *Type = GetType(I.Type);
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);

			LLVMValueRef Value;
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				Value = LLVMConstReal(LLVMType, Val->Float);
			}
			else if(Type->Basic.Flags & BasicFlag_Integer)
			{
				Value = LLVMConstInt(LLVMType, Val->Int.Unsigned, false);
			}
			else if(Type->Basic.Flags & BasicFlag_String)
			{
				Value = LLVMBuildGlobalStringPtr(gen->bld, Val->String->Data, "");
			}
			else
			{
				Assert(false);
			}
			gen->map.Add(I.Result, Value);

		} break;
		LLVM_BIN_OP(ADD, Add) break;
		LLVM_BIN_OP(SUB, Sub) break;
		LLVM_BIN_OP(MUL, Mul) break;
		case OP_DIV:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			const type *Type = GetType(I.Type);
			LLVMValueRef Val;
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				Val = LLVMBuildFDiv(gen->bld, LHS, RHS, "");
			}
			else
			{
				if(Type->Basic.Flags & BasicFlag_Unsigned)
					Val = LLVMBuildUDiv(gen->bld, LHS, RHS, "");
				else
					Val = LLVMBuildSDiv(gen->bld, LHS, RHS, "");
			}
			gen->map.Add(I.Result, Val);
		} break;
		case OP_MOD:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			const type *Type = GetType(I.Type);
			LLVMValueRef Val;
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				Val = LLVMBuildFRem(gen->bld, LHS, RHS, "");
			}
			else
			{
				if(Type->Basic.Flags & BasicFlag_Unsigned)
					Val = LLVMBuildURem(gen->bld, LHS, RHS, "");
				else
					Val = LLVMBuildSRem(gen->bld, LHS, RHS, "");
			}
			gen->map.Add(I.Result, Val);
		} break;
		case OP_CAST:
		{
			u32 FromType = I.Right;
			u32 ToType = I.Type;
			LLVMTypeRef LLVMToType = ConvertToLLVMType(gen->ctx, ToType);
			LLVMOpcode Op = RCCast(GetType(FromType), GetType(ToType));
			LLVMValueRef Value = gen->map.Get(I.Left);
			LLVMValueRef Val = LLVMBuildCast(gen->bld, Op, Value, LLVMToType, "");
			gen->map.Add(I.Result, Val);
		} break;
		case OP_STORE:
		{
			LLVMValueRef Pointer = gen->map.Get(I.Result);
			LLVMValueRef Value = gen->map.Get(I.Right);
			LLVMValueRef NewValue = LLVMBuildStore(gen->bld, Value, Pointer);
			gen->map.Add(I.Result, NewValue);
		} break;
		case OP_LOAD:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
			LLVMValueRef Pointer = gen->map.Get(I.Right);
			Assert(Pointer);
			Assert(LLVMType);
			LLVMValueRef Value = LLVMBuildLoad2(gen->bld, LLVMType, Pointer, "");
			gen->map.Add(I.Result, Value);
		} break;
		case OP_ALLOC:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
			LLVMValueRef Val = LLVMBuildAlloca(gen->bld, LLVMType, "");
			gen->map.Add(I.Result, Val);
		} break;
		case OP_RET:
		{
			LLVMValueRef Value = gen->map.Get(I.Left);
			LLVMBuildRet(gen->bld, Value);
		} break;
		case OP_IF:
		{
			LLVMValueRef Cond = gen->map.Get(I.Result);
			LLVMBuildCondBr(gen->bld, Cond, gen->blocks[I.Left].Block, gen->blocks[I.Right].Block);
		} break;
		case OP_JMP:
		{
			LLVMBuildBr(gen->bld, gen->blocks[I.BigRegister].Block);
		} break;
		LLVM_CMP_OP(GREAT, GT) break;
		LLVM_CMP_OP(LESS, LT) break;
		LLVM_CMP_OP(GEQ, GE) break;
		LLVM_CMP_OP(LEQ, LE) break;
		case OP_NEQ:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			const type *Type = GetType(I.Type);
			LLVMValueRef Value;
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				Value = LLVMBuildFCmp(gen->bld, LLVMRealONE, LHS, RHS, "");
			}
			else
			{
				LLVMIntPredicate Op = LLVMIntNE;
				Value = LLVMBuildICmp(gen->bld, Op, LHS, RHS, "");
			}
			gen->map.Add(I.Result, Value);
		} break;
		case OP_EQEQ:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			const type *Type = GetType(I.Type);
			LLVMValueRef Value;
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				 Value = LLVMBuildFCmp(gen->bld, LLVMRealOEQ, LHS, RHS, "");
			}
			else
			{
				LLVMIntPredicate Op = LLVMIntEQ;
				Value = LLVMBuildICmp(gen->bld, Op, LHS, RHS, "");
			}
			gen->map.Add(I.Result, Value);
		} break;
		case OP_CALL:
		{
			call_info *CallInfo = (call_info *)I.BigRegister;
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
			Assert(CallInfo);
			Assert(LLVMType);

			LLVMValueRef Args[256] = {};
			ForArray(Idx, CallInfo->Args)
			{
				Args[Idx] = gen->map.Get(CallInfo->Args[Idx]);
			}

			LLVMValueRef Operand = gen->map.Get(CallInfo->Operand);
			LLVMValueRef Result = LLVMBuildCall2(gen->bld, LLVMType, Operand, Args, CallInfo->Args.Count, "");
			gen->map.Add(I.Result, Result);
		} break;
	}
}

void RCGenerateFunction(generator *gen, function fn)
{
	const type *FnType = GetType(fn.Type);
	gen->blocks = (rc_block *)VAlloc(fn.BlockCount * sizeof(rc_block));
	for(u32 Idx = 0; Idx < fn.BlockCount; ++Idx)
	{
		basic_block Block = fn.Blocks[Idx];
		gen->blocks[Idx] = RCCreateBlock(gen, Block.ID, false);
	}

	for(int Idx = 0; Idx < FnType->Function.ArgCount; ++Idx)
	{
		gen->map.Add(gen->map.Data.Count, LLVMGetParam(gen->fn, Idx));
	}

	for(u32 Idx = 0; Idx < fn.BlockCount; ++Idx)
	{
		basic_block Block = fn.Blocks[Idx];
		RCSetBlock(gen, gen->blocks[Idx]);
		for(u32 InstrIdx = 0; InstrIdx < Block.InstructionCount; ++InstrIdx)
		{
			RCGenerateInstruction(gen, Block.Code[InstrIdx]);
		}
	}
}

LLVMValueRef RCGenerateFunctionSignature(generator *gen, function Function)
{
	LLVMCreateFunctionType(gen->ctx, Function.Type);
	return LLVMAddFunction(gen->mod, Function.Name->Data, ConvertToLLVMType(gen->ctx, Function.Type));
}

void RCGenerateFile(ir *IR, const char *Name, LLVMTargetMachineRef Machine)
{
	generator Gen = {};
	Gen.ctx = LLVMContextCreate();
	Gen.mod = LLVMModuleCreateWithNameInContext(Name, Gen.ctx);
	Gen.bld = LLVMCreateBuilderInContext(Gen.ctx);

	dynamic<LLVMValueRef> Functions = {};
	ForArray(Idx, IR->Functions)
	{
		LLVMValueRef Fn = RCGenerateFunctionSignature(&Gen, IR->Functions[Idx]);
		Functions.Push(Fn);
		Gen.map.Add(Idx, Fn);
	}
	Gen.map.LockBottom();
	ForArray(Idx, IR->Functions)
	{
		if(IR->Functions[Idx].BlockCount != 0)
		{
			Gen.fn = Functions[Idx];
			RCGenerateFunction(&Gen, IR->Functions[Idx]);
			Gen.map.Clear();
		}
	}
	char *Error = NULL;

	if(LLVMVerifyModule(Gen.mod, LLVMReturnStatusAction, &Error))
	{
		LERROR("Couldn't Verify LLVM Module: %s", Error);
		LLVMDisposeMessage(Error);
	}

	if(LLVMPrintModuleToFile(Gen.mod, "LLVMMod.bc", &Error))
	{
		LERROR("Couldn't Print LLVM: %s", Error);
		LLVMDisposeMessage(Error);
	}

	if(LLVMTargetMachineEmitToFile(Machine, Gen.mod, "out.obj", LLVMObjectFile, (char **)&Error))
	{
		LERROR("Couldn't Generate File: %s", Error);
		LLVMDisposeMessage(Error);
	}

	LLVMDisposeBuilder(Gen.bld);
	LLVMDisposeModule(Gen.mod);
	LLVMContextDispose(Gen.ctx);
}

rc_block RCCreateBlock(generator *gen, u32 ID, b32 Set)
{
	rc_block Result;
	char Buff[128] = {};
	sprintf_s(Buff, 128, "block_%d", ID);
	Result.Block = LLVMAppendBasicBlockInContext(gen->ctx, gen->fn, Buff);
	if(Set)
		LLVMPositionBuilderAtEnd(gen->bld, Result.Block);

	return Result;
}

void RCSetBlock(generator *gen, rc_block Block)
{
	LLVMPositionBuilderAtEnd(gen->bld, Block.Block);
}

LLVMTargetMachineRef RCInitLLVM()
{
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
	LLVMInitializeAllDisassemblers();

	LLVMTargetRef Target;
	LLVMTargetMachineRef Machine;
	LLVMGetTargetFromTriple(LLVMGetDefaultTargetTriple(), &Target, NULL);
	Machine = LLVMCreateTargetMachine(Target, LLVMGetDefaultTargetTriple(), "generic", LLVMGetHostCPUFeatures(),
			LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelDefault);

	return Machine;
}

void RCGenerateCode(ir *IR)
{
	LLVMTargetMachineRef Machine = RCInitLLVM();
	RCGenerateFile(IR, "main", Machine);
}

