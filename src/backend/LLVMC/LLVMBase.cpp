#include "LLVMBase.h"
#include "Parser.h"
#include "Semantics.h"
#include "Basic.h"
#include "ConstVal.h"
#include "IR.h"
#include "Memory.h"
#include "Log.h"
#include "LLVMType.h"
#include "Dynamic.h"
#include "Type.h"
#include "Dynamic.h"
#include "llvm-c/Core.h"
#include "llvm-c/Types.h"
#include "llvm-c/Target.h"
#include "llvm-c/Analysis.h"

LLVMValueRef RCGetStringConstPtr(generator *gen, const string *String)
{
	return LLVMBuildGlobalStringPtr(gen->bld, String->Data, "");
}

void LLVMGetProperArrayIndex(generator *gen, LLVMValueRef Index, LLVMValueRef OutArray[2])
{
	OutArray[0] = LLVMConstInt(ConvertToLLVMType(gen->ctx, Basic_uint), 0, false);
	OutArray[1] = Index;
}

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
				if(Val->Type == const_type::Integer)
				{
					Value = LLVMConstReal(LLVMType, Val->Int.Signed);
				}
				else
				{
					Assert(Val->Type == const_type::Float);
					Value = LLVMConstReal(LLVMType, Val->Float);
				}
			}
			else if(Type->Basic.Flags & BasicFlag_Integer)
			{
				if(Val->Type == const_type::Integer)
				{
					Value = LLVMConstInt(LLVMType, Val->Int.Unsigned, Val->Int.IsSigned);
				}
				else
				{
					Assert(Val->Type == const_type::Float);
					Value = LLVMConstInt(LLVMType, Val->Float, true);
				}
			}
			else if(Type->Basic.Flags & BasicFlag_String)
			{
				LLVMTypeRef IntType = ConvertToLLVMType(gen->ctx, Basic_i32);
				LLVMValueRef DataPtr = RCGetStringConstPtr(gen, Val->String.Data);
				LLVMValueRef Size    = LLVMConstInt(IntType, Val->String.Data->Size, false);
				Value = LLVMBuildAlloca(gen->bld, LLVMType, "");

				LLVMValueRef StringPtr = LLVMBuildStructGEP2(gen->bld, LLVMType, Value, 0, "String");
				LLVMValueRef SizePtr   = LLVMBuildStructGEP2(gen->bld, LLVMType, Value, 1, "Size");
				LLVMBuildStore(gen->bld, DataPtr, StringPtr);
				LLVMBuildStore(gen->bld, Size,    SizePtr);
			}
			else if(Type->Basic.Flags & BasicFlag_CString)
			{
				Value = RCGetStringConstPtr(gen, Val->String.Data);
			}
			else if(Type->Basic.Flags & BasicFlag_Boolean)
			{
				if(Val->Type == const_type::Integer)
				{
					Value = LLVMConstInt(LLVMType, Val->Int.Unsigned, false);
				}
				else
				{
					Assert(Val->Type == const_type::Float);
					Value = LLVMConstInt(LLVMType, Val->Float, true);
				}
			}
			else
			{
				LDEBUG("%d", Type->Kind);
				unreachable;
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
			if(FromType == Basic_string && ToType == Basic_cstring)
			{
				LLVMValueRef Value = gen->map.Get(I.Left);
				LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, FromType);
				LLVMValueRef Ptr = LLVMBuildStructGEP2(gen->bld, LLVMType, Value, 0, "_cstringptr");
				LLVMValueRef Val = LLVMBuildLoad2(gen->bld, ConvertToLLVMType(gen->ctx, Basic_cstring), Ptr, "_cstring");
				gen->map.Add(I.Result, Val);
			}
			else
			{
				LLVMTypeRef LLVMToType = ConvertToLLVMType(gen->ctx, ToType);
				LLVMOpcode Op = RCCast(GetType(FromType), GetType(ToType));
				LLVMValueRef Value = gen->map.Get(I.Left);
				LLVMValueRef Val = LLVMBuildCast(gen->bld, Op, Value, LLVMToType, "");
				gen->map.Add(I.Result, Val);
			}
		} break;
		case OP_ARRAYLIST:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
			u32 *Registers = (u32 *)I.BigRegister;
			LLVMValueRef Val = LLVMBuildAlloca(gen->bld, LLVMType, "_array_list");

			int Idx = 0;
			while(Registers[Idx] != -1)
			{
				LLVMValueRef Member = gen->map.Get(Registers[Idx]);
				LLVMValueRef Index = LLVMConstInt(ConvertToLLVMType(gen->ctx, Basic_uint), Idx, false);

				LLVMValueRef Indexes[2];
				LLVMGetProperArrayIndex(gen, Index, Indexes);

				LLVMValueRef Location = LLVMBuildGEP2(gen->bld, LLVMType, Val, Indexes, 2, "");
				LLVMBuildStore(gen->bld, Member, Location);

				Idx++;
			}

			gen->map.Add(I.Result, Val);
		} break;
		case OP_INDEX:
		{
			LLVMValueRef Operand = gen->map.Get(I.Left);
			LLVMTypeRef  LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
			const type *Type = GetType(I.Type);

			LLVMValueRef Val;
			if(Type->Kind == TypeKind_Array)
			{
				LLVMValueRef Index = gen->map.Get(I.Right);
				LLVMValueRef Indexes[2];
				LLVMGetProperArrayIndex(gen, Index, Indexes);
				Val = LLVMBuildGEP2(gen->bld, LLVMType, Operand, Indexes, 2, "");
			}
			else if(Type->Kind == TypeKind_Pointer)
			{
				LLVMValueRef Index = gen->map.Get(I.Right);
				LLVMType = ConvertToLLVMType(gen->ctx, Type->Pointer.Pointed);
				Val = LLVMBuildGEP2(gen->bld, LLVMType, Operand, &Index, 1, "");
			}
			else if(Type->Kind == TypeKind_Struct)
			{
				Val = LLVMBuildStructGEP2(gen->bld, LLVMType, Operand, I.Right, "");
			}
			else
			{
				LDEBUG("%d", Type->Kind);
				unreachable;
			}
			gen->map.Add(I.Result, Val);
		} break;
		case OP_STORE:
		{
			LLVMValueRef Pointer = gen->map.Get(I.Result);
			LLVMValueRef Value = gen->map.Get(I.Right);
			if(IsLoadableType(I.Type))
			{
				LLVMValueRef NewValue = LLVMBuildStore(gen->bld, Value, Pointer);
				gen->map.Add(I.Result, NewValue);
			}
			else
			{
				LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
				u64 Size = LLVMABISizeOfType(gen->data, LLVMType);
				LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
				uint Align = LLVMABIAlignmentOfType(gen->data, LLVMType);
				
				LLVMBuildMemCpy(gen->bld, Pointer, Align, Value, Align, ValueSize);
			}
		} break;
		case OP_MEMSET:
		{
			LLVMValueRef Pointer = gen->map.Get(I.Result);
			LLVMTypeRef Type = ConvertToLLVMType(gen->ctx, I.Type);
			u64 Size = LLVMABISizeOfType(gen->data, Type);
			u64 Alignment = LLVMABIAlignmentOfType(gen->data, Type);
			LLVMValueRef ValueZero = LLVMConstInt(LLVMInt8TypeInContext(gen->ctx), 0, false);
			LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
			LLVMBuildMemSet(gen->bld, Pointer, ValueZero, ValueSize, Alignment);
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
		case OP_ARG:
		{
			gen->map.Add(I.Result, LLVMGetParam(gen->fn, I.BigRegister));
		} break;
		case OP_RET:
		{
			if(I.Left != -1)
			{
				LLVMValueRef Value = gen->map.Get(I.Left);
				LLVMBuildRet(gen->bld, Value);
			}
			else
			{
				LLVMBuildRetVoid(gen->bld);
			}
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
			u32 CallType = I.Type;
			const type *Type = GetType(I.Type);
			if(Type->Kind == TypeKind_Pointer)
				CallType = Type->Pointer.Pointed;
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, CallType);
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
		case OP_COUNT: unreachable;
	}
}

void RCGenerateFunction(generator *gen, function fn)
{
	gen->blocks = (rc_block *)VAlloc(fn.Blocks.Count * sizeof(rc_block));
	ForArray(Idx, fn.Blocks)
	{
		basic_block Block = fn.Blocks[Idx];
		gen->blocks[Idx] = RCCreateBlock(gen, Block.ID, false);
	}

	ForArray(Idx, fn.Blocks)
	{
		basic_block Block = fn.Blocks[Idx];
		RCSetBlock(gen, gen->blocks[Idx]);
		ForArray(InstrIdx, Block.Code)
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

void RCGenerateComplexTypes(generator *gen)
{
	uint TypeCount = GetTypeCount();
	for(uint Index = 0; Index < TypeCount; ++Index)
	{
		if(GetType(Index)->Kind == TypeKind_Struct)
		{
			LLVMCreateOpaqueStringStructType(gen->ctx, Index);
		}
	}
	for(uint Index = 0; Index < TypeCount; ++Index)
	{
		if(GetType(Index)->Kind == TypeKind_Struct)
		{
			LLVMDefineStructType(gen->ctx, Index);
		}
	}
}

void RCGenerateCompilerTypes(generator *gen)
{
	type *U8Ptr = NewType(type);
	U8Ptr->Kind = TypeKind_Pointer;
	U8Ptr->Pointer = {.Pointed = Basic_u8};
	u32 U8PtrID = AddType(U8Ptr);
	struct_member DataMember = {STR_LIT("data"), U8PtrID};
	struct_member SizeMember = {STR_LIT("size"), Basic_i32};
	type *StringType = NewType(type);

	StringType->Kind = TypeKind_Struct;
	StringType->Struct = {
		.Members = SliceFromConst({DataMember, SizeMember}),
		.Name = STR_LIT("string"),
		.Flags = 0,
	};
	u32 String = AddType(StringType);
	LLVMCreateOpaqueStringStructType(gen->ctx, String);
	auto LLVMType = LLVMDefineStructType(gen->ctx, String);
	LLVMMapType(Basic_string, LLVMType);
}

void RCGenerateFile(file *File, LLVMTargetMachineRef Machine, b32 OutputBC)
{
	LDEBUG("Generating file: %s", File->Module.Name.Data);
	ir *IR = File->IR;

	generator Gen = {};
	Gen.ctx = LLVMContextCreate();
	Gen.mod = LLVMModuleCreateWithNameInContext(File->Module.Name.Data, Gen.ctx);
	Gen.bld = LLVMCreateBuilderInContext(Gen.ctx);
	Gen.data = LLVMCreateTargetDataLayout(Machine);
	LLVMSetModuleDataLayout(Gen.mod, Gen.data);

	RCGenerateCompilerTypes(&Gen);
	RCGenerateComplexTypes(&Gen);

	dynamic<LLVMValueRef> Functions = {};

	// Generate internal function
	{
		LLVMTypeRef FnType = LLVMFunctionType(LLVMVoidTypeInContext(Gen.ctx), NULL, 0, false);
		string LinkName = STR_LIT("__!GlobalInitializerFunction");
		LinkName = StructToModuleName(LinkName, File->Module.Name);
		LLVMValueRef Fn = LLVMAddFunction(Gen.mod, LinkName.Data, FnType);
		Functions.Push(Fn);
	}

	uint Count = 0;
	ForArray(GIdx, File->Module.Globals)
	{
		symbol *s = File->Module.Globals[GIdx];
		LLVMLinkage Linkage;
		if(s->Flags & SymbolFlag_Public)
			Linkage = LLVMExternalLinkage;
		else
			Linkage = LLVMPrivateLinkage;
		if(s->Flags & SymbolFlag_Function)
		{
			string fnName = *s->Name;
			string LinkName = fnName;
			if((s->Flags & SymbolFlag_Foreign) == 0)
				LinkName = StructToModuleName(fnName, File->Module.Name);
			LLVMCreateFunctionType(Gen.ctx, s->Type);
			LLVMValueRef Fn = LLVMAddFunction(Gen.mod, LinkName.Data, 
					ConvertToLLVMType(Gen.ctx, s->Type));
			LLVMSetLinkage(Fn, Linkage);
			Functions.Push(Fn);
			Gen.map.Add(Count++, Fn);
		}
		else
		{
			string Name = *s->Name;
			string LinkName = Name;
			if((s->Flags & SymbolFlag_Foreign) == 0)
				LinkName = StructToModuleName(Name, File->Module.Name);
			LLVMTypeRef LLVMType = ConvertToLLVMType(Gen.ctx, s->Type);
			LLVMValueRef Global = LLVMAddGlobal(Gen.mod, LLVMType, LinkName.Data);
			LLVMSetLinkage(Global, Linkage);
			LLVMSetInitializer(Global, LLVMConstNull(LLVMType));
			Gen.map.Add(Count++, Global);
		}
	}

	ForArray(Idx, File->Imported)
	{
		auto m = File->Imported[Idx];
		ForArray(GIdx, m.Globals)
		{
			symbol *s = m.Globals[GIdx];
			if(s->Flags & SymbolFlag_Function)
			{
				LLVMCreateFunctionType(Gen.ctx, s->Type);
				string fnName = *s->Name;
				string LinkName = fnName;
				if((s->Flags & SymbolFlag_Foreign) == 0)
					LinkName = StructToModuleName(fnName, m.Name);
				LLVMValueRef Fn = LLVMAddFunction(Gen.mod, LinkName.Data, 
						ConvertToLLVMType(Gen.ctx, s->Type));
				Functions.Push(Fn);
				Gen.map.Add(Count++, Fn);
			}
			else
			{
				string Name = *s->Name;
				string LinkName = Name;
				if((s->Flags & SymbolFlag_Foreign) == 0)
					LinkName = StructToModuleName(Name, m.Name);
				LLVMTypeRef LLVMType = ConvertToLLVMType(Gen.ctx, s->Type);
				LLVMValueRef Global = LLVMAddGlobal(Gen.mod, LLVMType, LinkName.Data);
				Gen.map.Add(Count++, Global);

			} 
		}
	}
	Gen.map.LockBottom();
	ForArray(Idx, IR->Functions)
	{
		if(IR->Functions[Idx].Blocks.Count != 0)
		{
			Gen.fn = Functions[Idx];
			RCGenerateFunction(&Gen, IR->Functions[Idx]);
			Gen.map.Clear();
		}
	}

	RCEmitFile(Machine, Gen.mod, File->Module.Name, OutputBC);

	LLVMDisposeBuilder(Gen.bld);
	LLVMDisposeModule(Gen.mod);
	LLVMContextDispose(Gen.ctx);
}

void RCEmitFile(LLVMTargetMachineRef Machine, LLVMModuleRef Mod, string ModuleName, b32 OutputBC)
{
	char *Error = NULL;

#if 1
	if(LLVMVerifyModule(Mod, LLVMReturnStatusAction, &Error))
	{
		LERROR("Couldn't Verify LLVM Module: %s", Error);
		LLVMDisposeMessage(Error);
	}
#endif

	if(OutputBC)
	{
		string_builder BCFileBuilder = MakeBuilder();
		BCFileBuilder += ModuleName;
		BCFileBuilder += ".bc";
		string BCFile = MakeString(BCFileBuilder);

		if(LLVMPrintModuleToFile(Mod, BCFile.Data, &Error))
		{
			LERROR("Couldn't Print LLVM: %s", Error);
			LLVMDisposeMessage(Error);
		}
	}

	string_builder Obj = MakeBuilder();
	Obj += ModuleName;
	Obj += ".obj";

	if(LLVMTargetMachineEmitToFile(Machine, Mod, MakeString(Obj).Data, LLVMObjectFile, (char **)&Error))
	{
		LERROR("Couldn't Generate File: %s", Error);
		LLVMDisposeMessage(Error);
	}
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

LLVMTargetMachineRef RCGenerateMain(slice<file> Files)
{
	LLVMTargetMachineRef Machine = RCInitLLVM();

	generator Gen = {};
	Gen.ctx = LLVMContextCreate();
	Gen.mod = LLVMModuleCreateWithNameInContext("!internal", Gen.ctx);
	Gen.bld = LLVMCreateBuilderInContext(Gen.ctx);
	Gen.data = LLVMCreateTargetDataLayout(Machine);
	LLVMSetModuleDataLayout(Gen.mod, Gen.data);

	LLVMValueRef *FileFns = (LLVMValueRef *)VAlloc((Files.Count+1) * sizeof(LLVMValueRef));

	LLVMTypeRef FnType = LLVMFunctionType(LLVMVoidTypeInContext(Gen.ctx), NULL, 0, false);
	LLVMTypeRef Int32Ty = LLVMInt32TypeInContext(Gen.ctx);
	LLVMTypeRef MainFnType = LLVMFunctionType(Int32Ty, NULL, 0, false);
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		string_builder Builder = MakeBuilder();
		Builder += File->Module.Name;
		Builder += "!__!GlobalInitializerFunction";
		string InitFnName = MakeString(Builder);
		FileFns[Idx] = LLVMAddFunction(Gen.mod, InitFnName.Data, FnType);
	}

	{
		FileFns[Files.Count] = LLVMAddFunction(Gen.mod, "main!main", MainFnType);
	}

	LLVMValueRef MainFn = LLVMAddFunction(Gen.mod, "main", MainFnType);
	LLVMBasicBlockRef Block = LLVMAppendBasicBlockInContext(Gen.ctx, MainFn, "only_block");
	LLVMPositionBuilderAtEnd(Gen.bld, Block);


	ForArray(Idx, Files)
	{
		LLVMTypeRef FnType = LLVMFunctionType(LLVMVoidTypeInContext(Gen.ctx), NULL, 0, false);
		LLVMBuildCall2(Gen.bld, FnType, FileFns[Idx], NULL, 0, "");
	}

	LLVMValueRef Result = LLVMBuildCall2(Gen.bld, MainFnType, FileFns[Files.Count], NULL, 0, "");

	LLVMBuildRet(Gen.bld, Result);
	RCEmitFile(Machine, Gen.mod, STR_LIT("!internal"), true) ;
	VFree(FileFns);
	return Machine;
}

void RCGenerateCode(slice<file> Files, LLVMTargetMachineRef Machine, b32 OutputBC)
{
	ForArray(Idx, Files)
	{
		file *File = &Files.Data[Idx];
		RCGenerateFile(File, Machine, OutputBC);

		// NOT THREAD SAFE
		LLVMClearTypeMap();
	}
}

