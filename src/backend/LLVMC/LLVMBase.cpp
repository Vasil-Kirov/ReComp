#include "LLVMBase.h"
#include "CommandLine.h"
#include "LLVMTypeInfoGlobal.h"
#include "Interpreter.h"
#include "Module.h"
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
#include "Threading.h"
#include "LLVMPasses.h"
#include "llvm-c/Core.h"
#include "llvm-c/DebugInfo.h"
#include "llvm-c/Types.h"
#include "llvm-c/Target.h"
#include "llvm-c/Analysis.h"

std::mutex LLVMNoThreadSafetyMutex;

LLVMValueRef RCGetStringConstPtr(generator *gen, const string *String)
{
	return LLVMBuildGlobalStringPtr(gen->bld, String->Data, "");
}

void LLVMGetProperArrayIndex(generator *gen, LLVMValueRef Index, LLVMValueRef OutArray[2])
{
	OutArray[0] = LLVMConstInt(ConvertToLLVMType(gen, Basic_uint), 0, false);
	OutArray[1] = Index;
}

void RCGenerateIntrins(generator *gen)
{
	string RDTSC = STR_LIT("llvm.readcyclecounter");
	LLVMTypeRef RDTSCType = LLVMFunctionType(LLVMIntTypeInContext(gen->ctx,
				GetRegisterTypeSize()),
				NULL, 0, false);
	LLVMValueRef RDTSCLLVM = LLVMAddFunction(gen->mod, RDTSC.Data, RDTSCType);
	gen->Intrinsics.Add(RDTSC, llvm_intrin { RDTSCLLVM, RDTSCType } );

	string MemCmp = STR_LIT("memcmp");
	LLVMTypeRef MemCmpArgs[] = {
		LLVMPointerType(LLVMVoidTypeInContext(gen->ctx), 0),
		LLVMPointerType(LLVMVoidTypeInContext(gen->ctx), 0),
		LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize())
	};

	LLVMTypeRef MemCmpType = LLVMFunctionType(
			LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()/2),
				MemCmpArgs, 3, false);
	LLVMValueRef MemCmpLLVM = LLVMAddFunction(gen->mod, MemCmp.Data, MemCmpType);
	gen->Intrinsics.Add(MemCmp, llvm_intrin { MemCmpLLVM, MemCmpType } );
}

void RCGenerateDebugInfo(generator *gen, ir_debug_info *Info)
{
	switch(Info->type)
	{
		case IR_DBG_VAR:
		{
			if(gen->CurrentScope == NULL)
				return;
			LLVMValueRef LLVM = gen->map.Get(Info->var.Register);
			auto m_info = LLVMDIBuilderCreateAutoVariable(gen->dbg, gen->CurrentScope, Info->var.Name.Data, Info->var.Name.Size, gen->f_dbg, Info->var.LineNo, ToDebugTypeLLVM(gen, Info->var.TypeID), false, LLVMDIFlagZero, 0);
			LLVMMetadataRef Expr = LLVMDIBuilderCreateExpression(gen->dbg, NULL, 0);
			LLVMDIBuilderInsertDeclareAtEnd(gen->dbg, LLVM, m_info, Expr, gen->CurrentLocation, gen->blocks[gen->CurrentBlock].Block);
		} break;
		case IR_DBG_LOCATION:
		{
			if(gen->CurrentScope == NULL)
				return;
			gen->CurrentLocation = LLVMDIBuilderCreateDebugLocation(gen->ctx, Info->loc.LineNo, 0, gen->CurrentScope, NULL);
			LLVMSetCurrentDebugLocation2(gen->bld, gen->CurrentLocation);
		} break;
		case IR_DBG_SCOPE:
		{
			gen->CurrentScope = LLVMDIBuilderCreateLexicalBlock(gen->dbg, gen->CurrentScope, gen->f_dbg, Info->loc.LineNo, 0);
		} break;
		case IR_DBG_ARG:
		{
			LLVMMetadataRef Meta = LLVMDIBuilderCreateParameterVariable(
					gen->dbg, gen->CurrentScope,
					Info->arg.Name.Data, Info->arg.Name.Size,
					Info->arg.ArgNo,
					gen->f_dbg, Info->arg.LineNo,
					ToDebugTypeLLVM(gen, Info->arg.TypeID),
					false, LLVMDIFlagZero);
			LLVMMetadataRef Expr = LLVMDIBuilderCreateExpression(gen->dbg, NULL, 0);
			LLVMValueRef Value = gen->map.Get(Info->arg.Register);
			LLVMDIBuilderInsertDbgValueAtEnd(gen->dbg, Value, Meta, Expr, gen->CurrentLocation, gen->blocks[gen->CurrentBlock].Block);
		} break;
	}
}

template<typename t>
dynamic<t> RCCopyTypeMap(dynamic<t> Map)
{
	dynamic<t> Result = {};
	For(Map) {
		t Add = t { it->TypeID, it->Ref };
		Result.Push(Add);
	}
	return Result;
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
		case OP_ALLOC: // Handled before
		{
		} break;
		case OP_RDTSC:
		{
			llvm_intrin Intrin = gen->Intrinsics[STR_LIT("llvm.readcyclecounter")];
			Assert(Intrin.Fn);
			// @NOTE: I don't think the fences are needed
			//LLVMBuildFence(gen->bld, LLVMAtomicOrderingSequentiallyConsistent,
			//		0, "");
			LLVMValueRef Time = LLVMBuildCall2(gen->bld, Intrin.Type, Intrin.Fn, NULL, 0, "rdtsc");
			//LLVMBuildFence(gen->bld, LLVMAtomicOrderingSequentiallyConsistent,
			//		0, "");
			gen->map.Add(I.Result, Time);
		} break;
		case OP_TYPEINFO:
		{
			u32 TInfo = I.Type;
			u32 TInfoPtr = GetPointerTo(I.Type);
			u32 TInfoSlice = GetSliceType(TInfo);
			LLVMValueRef TypeTable = gen->map.Get(I.Left);
			LLVMValueRef Idx = gen->map.Get(I.Right);
			LLVMTypeRef TypeInfo = ConvertToLLVMType(gen, TInfo);
			LLVMTypeRef TypeSlice = ConvertToLLVMType(gen, TInfoSlice);
			LLVMTypeRef TypePtr = ConvertToLLVMType(gen, TInfoPtr);
			LLVMValueRef DataPtr = LLVMBuildStructGEP2(gen->bld, TypeSlice, TypeTable, 1, "");
			LLVMValueRef Data = LLVMBuildLoad2(gen->bld, TypePtr, DataPtr, "");
			LLVMValueRef Result = LLVMBuildGEP2(gen->bld, TypeInfo, Data, &Idx, 1, "");
			gen->map.Add(I.Result, Result);
		} break;
		case OP_ZEROUT:
		{
			LLVMValueRef Pointer = gen->map.Get(I.Right);
			LLVMTypeRef LLVMT = ConvertToLLVMType(gen, I.Type);
			if(IsLoadableType(I.Type))
			{
				LLVMValueRef Zero = LLVMConstNull(LLVMT);
				LLVMBuildStore(gen->bld, Zero, Pointer);
			}
			else
			{
				LLVMValueRef Zero = LLVMConstNull(LLVMInt8TypeInContext(gen->ctx));
				u64 Size = LLVMABISizeOfType(gen->data, LLVMT);
				u64 Alignment = LLVMABIAlignmentOfType(gen->data, LLVMT);
				LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
				LLVMBuildMemSet(gen->bld, Pointer, Zero, ValueSize, Alignment);
			}
		} break;
		case OP_ALLOCGLOBAL:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef Value = LLVMAddGlobal(gen->mod, LLVMArrayType2(LLVMType, I.BigRegister), "");
			LLVMSetInitializer(Value, LLVMConstNull(LLVMArrayType2(LLVMType, I.BigRegister)));
			gen->map.Add(I.Result, Value);
		} break;
		case OP_CONSTINT:
		{
			const type *T = GetType(I.Type);
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			if(HasBasicFlag(T, BasicFlag_Float))
			{
				f64 Val = I.BigRegister;
				Assert(I.BigRegister == 0);
				LLVMValueRef Value = LLVMConstReal(LLVMType, Val);
				gen->map.Add(I.Result, Value);
			}
			else
			{
				u64 Val = I.BigRegister;
				LLVMValueRef Value = LLVMConstInt(LLVMType, Val, false);
				gen->map.Add(I.Result, Value);
			}
		} break;
		case OP_ENUM_ACCESS:
		const_value V;
		{
			const type *T = GetType(I.Type);
			Assert(T->Kind == TypeKind_Enum);
			V = T->Enum.Members[I.Right].Value;
			I.Op = OP_CONST;
			I.BigRegister = (u64)&V;
		}
		// fallthrough
		case OP_CONST:
		{
			const_value *Val = (const_value *)I.BigRegister;
			const type *Type = GetType(I.Type);
			if(Type->Kind == TypeKind_Enum)
				Type = GetType(Type->Enum.Type);

			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);

			LLVMValueRef Value;
			if(Type->Kind == TypeKind_Pointer)
			{
				if(Val->Type == const_type::String)
				{
					Value = RCGetStringConstPtr(gen, Val->String.Data);
				}
				else if(Val->Int.Unsigned == 0)
				{
					Assert(Val->Type == const_type::Integer);
					Value = LLVMConstPointerNull(LLVMType);
				}
				else
				{
					unreachable;
				}
			}
			else if(Type->Basic.Flags & BasicFlag_Float)
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
				LLVMTypeRef IntType = ConvertToLLVMType(gen, Basic_int);
				LLVMValueRef DataPtr = RCGetStringConstPtr(gen, Val->String.Data);
				LLVMValueRef Size    = LLVMConstInt(IntType, GetUTF8Count(Val->String.Data), false);
				Value = LLVMBuildAlloca(gen->bld, LLVMType, "");

				LLVMValueRef SizePtr   = LLVMBuildStructGEP2(gen->bld, LLVMType, Value, 0, "Size");
				LLVMValueRef StringPtr = LLVMBuildStructGEP2(gen->bld, LLVMType, Value, 1, "String");
				LLVMBuildStore(gen->bld, Size,    SizePtr);
				LLVMBuildStore(gen->bld, DataPtr, StringPtr);
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
			else if(HasBasicFlag(Type, BasicFlag_TypeID))
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
			else
			{
				LDEBUG("%d", Type->Kind);
				unreachable;
			}
			gen->map.Add(I.Result, Value);

		} break;
		case OP_UNREACHABLE:
		{
			LLVMBuildUnreachable(gen->bld);
		} break;
		case OP_PTRDIFF:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			const type *T = GetType(I.Type);
			Assert(T->Kind == TypeKind_Pointer);
			LLVMTypeRef Type = ConvertToLLVMType(gen, T->Pointer.Pointed);
			LLVMValueRef Val = LLVMBuildPtrDiff2(gen->bld, Type, LHS, RHS, "");
			gen->map.Add(I.Result, Val);
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
		case OP_SL:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			LLVMValueRef Val = LLVMBuildShl(gen->bld, LHS, RHS, "");
			gen->map.Add(I.Result, Val);
		} break;
		case OP_SR:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);
			LLVMValueRef Val = NULL;
			const type *T = GetType(I.Type);
			if(HasBasicFlag(T, BasicFlag_Unsigned))
			{
				Val = LLVMBuildLShr(gen->bld, LHS, RHS, "");
			}
			else
			{
				Val = LLVMBuildAShr(gen->bld, LHS, RHS, "");
			}
			gen->map.Add(I.Result, Val);
		} break;
		case OP_FN:
		{
			function *Fn = (function *)I.BigRegister;
			LLVMTypeRef FnType = LLVMCreateFunctionType(gen, Fn->Type);
			LLVMValueRef LLVMFn = LLVMAddFunction(gen->mod, Fn->Name->Data, FnType);
			LLVMSetLinkage(LLVMFn, LLVMPrivateLinkage);


			generator NewGen = {};
			NewGen.ctx = gen->ctx;
			NewGen.mod = gen->mod;
			NewGen.bld = LLVMCreateBuilderInContext(NewGen.ctx);
			NewGen.dbg = gen->dbg;
			NewGen.f_dbg = gen->f_dbg;
			NewGen.data = gen->data;
			NewGen.fn = LLVMFn;
			NewGen.LLVMTypeMap = RCCopyTypeMap(gen->LLVMTypeMap);
			NewGen.LLVMDebugTypeMap = RCCopyTypeMap(gen->LLVMDebugTypeMap);
			NewGen.Intrinsics = gen->Intrinsics;

			gen->map.Add(I.Result, LLVMFn);
			for(int i = 0; i < gen->map.Bottom; ++i)
				NewGen.map.Add(gen->map.Data[i].Register, gen->map.Data[i].Value);
			RCGenerateFunction(&NewGen, *Fn);
			LLVMSetCurrentDebugLocation2(gen->bld, gen->CurrentLocation);
			NewGen.LLVMTypeMap.Free();
			NewGen.LLVMDebugTypeMap.Free();
		} break;
		case OP_CAST:
		{
			u32 FromType = I.Right;
			u32 ToType = I.Type;
			LLVMTypeRef LLVMToType = ConvertToLLVMType(gen, ToType);
			LLVMOpcode Op = RCCast(GetType(FromType), GetType(ToType));
			LLVMValueRef Val = gen->map.Get(I.Left);
			if(Op != LLVMCatchSwitch)
			{
				Val = LLVMBuildCast(gen->bld, Op, Val, LLVMToType, "");
			}
			gen->map.Add(I.Result, Val);
		} break;
		case OP_ARRAYLIST:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			array_list_info *Info = (array_list_info *)I.BigRegister;
			LLVMValueRef Val = gen->map.Get(Info->Alloc);
			const type *T = GetType(I.Type);
			Assert(T->Kind == TypeKind_Array);
			b32 Loadable = IsLoadableType(T->Array.Type);
			LLVMTypeRef MemberLLVM = ConvertToLLVMType(gen, T->Array.Type);

			LLVMTypeRef uintT = ConvertToLLVMType(gen, Basic_uint);
			for(int Idx = 0; Idx < Info->Count; ++Idx)
			{
				LLVMValueRef Member = gen->map.Get(Info->Registers[Idx]);
				LLVMValueRef Index = LLVMConstInt(uintT, Idx, false);

				LLVMValueRef Indexes[2];
				LLVMGetProperArrayIndex(gen, Index, Indexes);

				LLVMValueRef Location = LLVMBuildGEP2(gen->bld, LLVMType, Val, Indexes, 2, "");
				if(Loadable)
				{
					LLVMBuildStore(gen->bld, Member, Location);
				}
				else
				{
					u64 Size = GetTypeSize(T->Array.Type);
					LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
					uint Align = LLVMABIAlignmentOfType(gen->data, MemberLLVM);

					LLVMBuildMemCpy(gen->bld, Location, Align, Member, Align, ValueSize);
				}
			}

			gen->map.Add(I.Result, Val);
		} break;
		case OP_INDEX:
		{
			LLVMValueRef Operand = gen->map.Get(I.Left);
			LLVMTypeRef  LLVMType = ConvertToLLVMType(gen, I.Type);
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
				LLVMType = ConvertToLLVMType(gen, Type->Pointer.Pointed);
				Val = LLVMBuildGEP2(gen->bld, LLVMType, Operand, &Index, 1, "");
			}
			else if(IsString(Type))
			{
				Val = LLVMBuildStructGEP2(gen->bld, LLVMType, Operand, I.Right, "");
			}
			else if(Type->Kind == TypeKind_Struct)
			{
				if(Type->Struct.Flags & StructFlag_Union)
				{
					LLVMTypeRef Pointee = ConvertToLLVMType(gen, Type->Struct.Members[I.Right].Type);
					LLVMTypeRef ResType = LLVMPointerType(Pointee, 0);
					Val = LLVMBuildBitCast(gen->bld, Operand, ResType, "");
				}
				else
				{
					Val = LLVMBuildStructGEP2(gen->bld, LLVMType, Operand, I.Right, "");
				}
			}
			else if(Type->Kind == TypeKind_Slice)
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
				LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
				u64 Size = GetTypeSize(I.Type);//LLVMABISizeOfType(gen->data, LLVMType);
				LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
				uint Align = LLVMABIAlignmentOfType(gen->data, LLVMType);
				
				LLVMBuildMemCpy(gen->bld, Pointer, Align, Value, Align, ValueSize);
			}
		} break;
		case OP_MEMSET:
		{
			LLVMValueRef Pointer = gen->map.Get(I.Result);
			LLVMTypeRef Type = ConvertToLLVMType(gen, I.Type);
			u64 Size = LLVMABISizeOfType(gen->data, Type);
			u64 Alignment = LLVMABIAlignmentOfType(gen->data, Type);
			LLVMValueRef ValueZero = LLVMConstInt(LLVMInt8TypeInContext(gen->ctx), 0, false);
			LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
			LLVMBuildMemSet(gen->bld, Pointer, ValueZero, ValueSize, Alignment);
		} break;
		case OP_LOAD:
		{
			const type *T = GetType(I.Type);
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef Pointer = gen->map.Get(I.Right);
			Assert(Pointer);
			Assert(LLVMType);
			if(T->Kind == TypeKind_Vector)
			{
				LDEBUG("HERE");
			}

			if(IsLoadableType(T))
			{
				LLVMValueRef Value = LLVMBuildLoad2(gen->bld, LLVMType, Pointer, "");
				gen->map.Add(I.Result, Value);
			}
			else if(T->Kind == TypeKind_Function)
			{
				LLVMType = ConvertToLLVMType(gen, I.Type);
				LLVMType = LLVMPointerType(LLVMType, 0);
				LLVMValueRef Value = LLVMBuildLoad2(gen->bld, LLVMType, Pointer, "");
				gen->map.Add(I.Result, Value);
			}
			else
			{
				LLVMValueRef Value = gen->map.Get(I.Result);
				uint Align = LLVMPreferredAlignmentOfType(gen->data, LLVMType);
				uint Size = GetTypeSize(T);
				LLVMValueRef LLVMSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
				LLVMBuildMemCpy(gen->bld, Value, Align, Pointer, Align, LLVMSize);
			}
		} break;
		case OP_ARG:
		{
			u64 Idx = I.BigRegister;
			if(gen->IsCurrentFnRetInPtr)
				Idx++;
			LLVMValueRef Arg = LLVMGetParam(gen->fn, Idx);
			const type *Type = GetType(I.Type);
			if(Type->Kind == TypeKind_Struct && IsStructAllFloats(Type))
			{
				LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
				LLVMValueRef AsArg = LLVMBuildAlloca(gen->bld, LLVMType, "vec");
				for(int i = 0; i < Type->Struct.Members.Count / 2; ++i)
				{
					if(i != 0) {
						Idx++;
						Arg = LLVMGetParam(gen->fn, Idx);
					}
					
					auto Ptr = LLVMBuildStructGEP2(gen->bld, LLVMType, AsArg, i * 2, "");
					//LLVMBuildMemCpy(gen->bld, Ptr, AlignVec2, Arg, AlignVec2, SizeVec2);
					LLVMBuildStore(gen->bld, Arg, Ptr);
				}

				gen->map.Add(I.Result, AsArg);
			}
			else if(IsPassInAsIntType(Type))
			{
				LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
				LLVMValueRef AsArg = LLVMBuildAlloca(gen->bld, LLVMType, "arg");
				LLVMBuildStore(gen->bld, Arg, AsArg);
				gen->map.Add(I.Result, AsArg);
			}
#if 0
			else if(Idx >= FnType->Function.ArgCount)
			{
				Assert(FnType->Function.Flags & SymbolFlag_VarFunc);
				LLVMTypeRef LLVMType = ConvertToLLVMType(gen->ctx, I.Type);
				Arg = LLVMBuildLoad2(gen->bld, LLVMType, Arg, "args");
				gen->map.Add(I.Result, Arg);
			}
#endif
			else
			{
				gen->map.Add(I.Result, Arg);
			}
		} break;
		case OP_RET:
		{
			if(I.Left != -1)
			{
				LLVMValueRef Value = gen->map.Get(I.Left);
				if(gen->IsCurrentFnRetInPtr)
				{
					LLVMValueRef RetPtr = LLVMGetParam(gen->fn, 0);
					LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
					u64 Size = LLVMABISizeOfType(gen->data, LLVMType);
					LLVMValueRef ValueSize = LLVMConstInt(LLVMInt64TypeInContext(gen->ctx), Size, false);
					uint Align = LLVMABIAlignmentOfType(gen->data, LLVMType);

					LLVMBuildMemCpy(gen->bld, RetPtr, Align, Value, Align, ValueSize);
					LLVMBuildRetVoid(gen->bld);
				}
				else
				{
					LLVMBuildRet(gen->bld, Value);
				}
			}
			else
			{
				LLVMBuildRetVoid(gen->bld);
			}
		} break;
		case OP_SWITCHINT:
		{
			ir_switchint *Info = (ir_switchint *)I.BigRegister;
			LLVMValueRef Matcher = gen->map.Get(Info->Matcher);
			LLVMValueRef Switch = LLVMBuildSwitch(gen->bld, Matcher, gen->blocks[Info->After].Block, Info->Cases.Count);

			ForArray(Idx, Info->Cases)
			{
				u32 Case = Info->Cases[Idx];
				LLVMValueRef V = gen->map.Get(Info->OnValues[Idx]);
				LLVMAddCase(Switch, V, gen->blocks[Case].Block);
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
			if(Type->Kind == TypeKind_Enum)
			{
				Type = GetType(Type->Enum.Type);
			}

			LLVMValueRef Value;
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				 Value = LLVMBuildFCmp(gen->bld, LLVMRealOEQ, LHS, RHS, "");
			}
			else
			{
				Value = LLVMBuildICmp(gen->bld, LLVMIntEQ, LHS, RHS, "");
			}
			gen->map.Add(I.Result, Value);
		} break;
		case OP_AND:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);

			LLVMValueRef Value = LLVMBuildAnd(gen->bld, LHS, RHS, "");

			gen->map.Add(I.Result, Value);
		} break;
		case OP_OR:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);

			LLVMValueRef Value = LLVMBuildOr(gen->bld, LHS, RHS, "");

			gen->map.Add(I.Result, Value);
		} break;
		case OP_XOR:
		{
			LLVMValueRef LHS = gen->map.Get(I.Left);
			LLVMValueRef RHS = gen->map.Get(I.Right);

			LLVMValueRef Value = LLVMBuildXor(gen->bld, LHS, RHS, "");

			gen->map.Add(I.Result, Value);
		} break;
		case OP_CALL:
		{
			call_info *CallInfo = (call_info *)I.BigRegister;
			u32 CallType = I.Type;
			const type *Type = GetType(I.Type);
			if(Type->Kind == TypeKind_Pointer)
				CallType = Type->Pointer.Pointed;
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, CallType);
			Assert(CallInfo);
			Assert(LLVMType);

			LLVMValueRef Args[256] = {};
			ForArray(Idx, CallInfo->Args)
			{
				Args[Idx] = gen->map.Get(CallInfo->Args[Idx]);
			}

			LLVMValueRef Operand = gen->map.Get(CallInfo->Operand);
			Assert(Operand);
			LLVMValueRef Result = LLVMBuildCall2(gen->bld, LLVMType, Operand, Args, CallInfo->Args.Count, "");
			gen->map.Add(I.Result, Result);
		} break;
		case OP_MEMCMP:
		{

			ir_memcmp *Info = (ir_memcmp *)I.BigRegister;
			LLVMValueRef Args[3] = {};
			Args[0] = gen->map.Get(Info->LeftPtr);
			Args[1] = gen->map.Get(Info->RightPtr);
			Args[2] = gen->map.Get(Info->Count);

			llvm_intrin MemCmp = gen->Intrinsics[STR_LIT("memcmp")];
			Assert(MemCmp.Fn);

			LLVMValueRef Operand = MemCmp.Fn;
			LLVMTypeRef Type = MemCmp.Type;

			LLVMValueRef Result = LLVMBuildCall2(gen->bld, Type, Operand, Args, 3, "memcmp");
			LLVMValueRef Zero = LLVMConstNull(LLVMInt32TypeInContext(gen->ctx));

			Result = LLVMBuildICmp(gen->bld, LLVMIntEQ, Result, Zero, "to_bool");
			gen->map.Add(I.Result, Result);
		} break;
		case OP_DEBUGINFO:
		{
			ir_debug_info *Info = (ir_debug_info *)I.BigRegister;
			RCGenerateDebugInfo(gen, Info);
		} break;
		case OP_SPILL:
		case OP_TOPHYSICAL:
		case OP_COUNT:
		unreachable;
	}
}

LLVMMetadataRef RCGenerateDebugInfoForFunction(generator *gen, function fn)
{
	string LinkName = *fn.LinkName;

	LLVMMetadataRef Meta = LLVMDIBuilderCreateFunction(
			gen->dbg, gen->f_dbg,
			fn.Name->Data, fn.Name->Size,
			LinkName.Data, LinkName.Size,
			gen->f_dbg, fn.LineNo,
			ToDebugTypeLLVM(gen, fn.Type), false, true, 0, LLVMDIFlagZero, false);
	LLVMSetSubprogram(gen->fn, Meta);
	return Meta;
}

void RCGenerateFunction(generator *gen, function fn)
{
	gen->blocks = (rc_block *)VAlloc(fn.Blocks.Count * sizeof(rc_block));
	gen->BlockCount = fn.Blocks.Count;
	//gen->FnType = fn.Type;
	ForArray(Idx, fn.Blocks)
	{
		basic_block Block = fn.Blocks[Idx];
		gen->blocks[Block.ID] = RCCreateBlock(gen, Block.ID, false);
	}

	gen->CurrentBlock = -1;
	RCSetBlock(gen, 0);
	//LDEBUG("Fn: %s", fn.Name->Data);
	LLVMSetCurrentDebugLocation2(gen->bld, NULL);
	ForArray(Idx, fn.Blocks)
	{
		basic_block Block = fn.Blocks[Idx];
		ForArray(InstrIdx, Block.Code)
		{
			instruction I = Block.Code[InstrIdx];
			switch(I.Op)
			{
				case OP_ALLOC:
				{
					LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
					LLVMValueRef Val = LLVMBuildAlloca(gen->bld, LLVMType, "");

					gen->map.Add(I.Result, Val);
				} break;
				case OP_LOAD:
				{
					if(!IsLoadableType(I.Type))
					{
						LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
						LLVMValueRef Val = LLVMBuildAlloca(gen->bld, LLVMType, "");
						gen->map.Add(I.Result, Val);
					}
				} break;
				default: break;
			}
		}

		
	}

	if(fn.Type == INVALID_TYPE || fn.NoDebugInfo)
	{
		gen->IsCurrentFnRetInPtr = false;
		gen->CurrentScope = NULL;
		gen->CurrentLocation = NULL;
	}
	else
	{
		u32 Returns = ReturnsToType(GetType(fn.Type)->Function.Returns);
		gen->IsCurrentFnRetInPtr = IsRetTypePassInPointer(Returns);
		gen->CurrentScope = RCGenerateDebugInfoForFunction(gen, fn);
		LLVMSetCurrentDebugLocation2(gen->bld, gen->CurrentScope);
	}

	ForArray(Idx, fn.Blocks)
	{
		basic_block Block = fn.Blocks[Idx];
		RCSetBlock(gen, Block.ID);
		ForArray(InstrIdx, Block.Code)
		{
			RCGenerateInstruction(gen, Block.Code[InstrIdx]);
		}
	}
	VFree(gen->blocks);
}

LLVMValueRef RCGenerateFunctionSignature(generator *gen, function Function)
{
	LLVMCreateFunctionType(gen, Function.Type);
	return LLVMAddFunction(gen->mod, Function.Name->Data, ConvertToLLVMType(gen, Function.Type));
}

void RCGenerateComplexTypes(generator *gen)
{
	uint TypeCount = GetTypeCount();
	for(uint Index = 0; Index < TypeCount; ++Index)
	{
		const type *T = GetType(Index);
		if(T->Kind == TypeKind_Struct)
		{
			if(T->Struct.Flags & StructFlag_Generic)
			{
				continue;
			}
			LLVMCreateOpaqueStructType(gen, Index);
			LLMVDebugOpaqueStruct(gen, Index);
		}
		else if(T->Kind == TypeKind_Enum)
		{
			LLVMDebugDefineEnum(gen, T, Index);
		}
	}
	for(uint Index = 0; Index < TypeCount; ++Index)
	{
		const type *T = GetType(Index);
		if(T->Kind == TypeKind_Struct && (T->Struct.Flags & StructFlag_Generic) == 0)
		{
			LLVMDefineStructType(gen, Index);
			LLMVDebugDefineStruct(gen, Index);
		}
	}
}

void RCGenerateCompilerTypes(generator *gen)
{
	struct_member DataMember = {STR_LIT("data"), GetPointerTo(Basic_u8)};
	struct_member SizeMember = {STR_LIT("count"), Basic_int};
	type *StringType = NewType(type);

	StringType->Kind = TypeKind_Struct;
	StringType->Struct = {
		.Members = SliceFromConst({SizeMember, DataMember}),
		.Name = STR_LIT("string"),
		.Flags = 0,
	};
	u32 String = AddType(StringType);
	LLVMCreateOpaqueStructType(gen, String);
	LLMVDebugOpaqueStruct(gen, String);
	auto LLVMType = LLVMDefineStructType(gen, String);
	auto DebugType = LLMVDebugDefineStruct(gen, String);
	LLVMMapType(gen, Basic_string, LLVMType);
	LLVMDebugMapType(gen, Basic_string, DebugType);
}

void GetNameAndDirectory(char **OutName, char **OutDirectory, string Relative)
{
	char *Absolute = GetAbsolutePath(Relative.Data);
	size_t AbsoluteSize = strlen(Absolute);
	for(int i = AbsoluteSize; i >= 0; i--)
	{
		if(Absolute[i] == '\\' || Absolute[i] == '/')
		{
			*OutName = Absolute + i + 1;
			*OutDirectory = (char *)VAlloc(i+1);
			memcpy(*OutDirectory, Absolute, i);
			(*OutDirectory)[i] = 0;
			return;
		}
	}
	unreachable;
}

LLVMMetadataRef IntToMeta(generator *gen, int i)
{
	LLVMValueRef Value = LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), i, true);

	return LLVMValueAsMetadata(Value);
}

void RCGenerateFile(module *M, b32 OutputBC, slice<module*> Modules, slice<file*> Files, compile_info *Info)
{
	LDEBUG("Generating module: %s", M->Name.Data);

	llvm_init_info Machine = RCInitLLVM(Info);

	generator Gen = {};
	//file *File = M->Files[0];
	Gen.ctx = LLVMContextCreate();
	Gen.mod = LLVMModuleCreateWithNameInContext(M->Name.Data, Gen.ctx);


	char *FileName = NULL;
	char *FileDirectory = NULL;
	GetNameAndDirectory(&FileName, &FileDirectory, M->Files[0]->Name);
	LLVMSetSourceFileName(Gen.mod, FileName, strlen(FileName));
	LLVMSetTarget(Gen.mod, Info->TargetTriple.Data);
	Gen.map = {};
	Gen.bld = LLVMCreateBuilderInContext(Gen.ctx);
	Gen.dbg = LLVMCreateDIBuilder(Gen.mod);
	Gen.f_dbg = LLVMDIBuilderCreateFile(Gen.dbg,
			FileName, VStrLen(FileName),
			FileDirectory, VStrLen(FileDirectory));
	Gen.data = LLVMCreateTargetDataLayout(Machine.Target);
	LLVMSetModuleDataLayout(Gen.mod, Gen.data);

	string CompilerName = STR_LIT("RCP Compiler");

	RCGenerateIntrins(&Gen);

	LLVMDIBuilderCreateCompileUnit(
			Gen.dbg,
			LLVMDWARFSourceLanguageC99,
			Gen.f_dbg,
			CompilerName.Data, CompilerName.Size,
			false,
			NULL, 0, 0, "", 0, LLVMDWARFEmissionFull, 0, false, false, "", 0, "", 0);

#if defined (_WIN32)
	string CodeView = STR_LIT("CodeView");
	LLVMAddModuleFlag(Gen.mod, LLVMModuleFlagBehaviorWarning,
			CodeView.Data, CodeView.Size, IntToMeta(&Gen, 1));
#elif defined (CM_LINUX)
	string DwarV = STR_LIT("Dwarf Version");
	LLVMAddModuleFlag(Gen.mod, LLVMModuleFlagBehaviorWarning,
			DwarV.Data, DwarV.Size, IntToMeta(&Gen, 5));
#else
#error Unkown debug fromat for this OS
#endif

	string DIV = STR_LIT("Debug Info Version");
	LLVMAddModuleFlag(Gen.mod, LLVMModuleFlagBehaviorWarning,
			DIV.Data, DIV.Size, IntToMeta(&Gen, 3));

	RCGenerateCompilerTypes(&Gen);
	RCGenerateComplexTypes(&Gen);

	struct gen_fn_info {
		LLVMValueRef LLVM;
		string Name;
	};

	dynamic<gen_fn_info> Functions = {};

	LLVMValueRef MaybeInitFn = NULL;
	// Generate internal functions
	dict<LLVMValueRef> AddedFns = {};
	ForArray(FIdx, M->Files)
	{
		LLVMTypeRef FnType = LLVMFunctionType(LLVMVoidTypeInContext(Gen.ctx), NULL, 0, false);

		string_builder StrBuilder = MakeBuilder();
		PushBuilderFormated(&StrBuilder, "__GlobalInitializerFunction.%d", FIdx);
		string BaseName = MakeString(StrBuilder);
		string LinkName = StructToModuleName(BaseName, M->Name);
		LLVMValueRef Fn = LLVMAddFunction(Gen.mod, LinkName.Data, FnType);
		Functions.Push({.LLVM = Fn, .Name = LinkName});
		MaybeInitFn = Fn;
		AddedFns.Add(LinkName, Fn);
	}

	b32 IsInitModule = M->Name == STR_LIT("init");
	ForArray(MIdx, Modules)
	{
		// shadow
		module *m = Modules[MIdx];
		ForArray(GIdx, m->Globals.Data)
		{
			symbol *s = m->Globals.Data[GIdx];
			if(s->Flags & SymbolFlag_Generic) {
				continue;
			}

			if(IsInitModule)
			{
				if(*s->Name == STR_LIT("global_initializers"))
				{
					LLVMValueRef Fn = RCGenerateMainFn(&Gen, Files, MaybeInitFn);
					LLVMSetLinkage(Fn, LLVMExternalLinkage);
					Gen.map.Add(s->Register, Fn);
					continue;
				}
				if(*s->Name == STR_LIT("type_table"))
				{
					LLVMValueRef TypeTable = GenTypeInfo(&Gen);
					LLVMSetLinkage(TypeTable, LLVMExternalLinkage);
					Gen.map.Add(s->Register, TypeTable);
					continue;
				}
			}

			LLVMLinkage Linkage;
			if(s->Flags & SymbolFlag_Public)
				Linkage = LLVMExternalLinkage;
			else
				Linkage = /*LLVMPrivateLinkage*/ LLVMExternalLinkage;
			// @NOTE: Currently there are some probems with gneerating generic functions so all symbols will be public

			LLVMValueRef AlreadyIn = AddedFns[*s->LinkName];
			if(AlreadyIn)
			{
				Gen.map.Add(s->Register, AlreadyIn);
			}

			if(s->Flags & SymbolFlag_Function && GetType(s->Type)->Kind != TypeKind_Pointer)
			{
				string LinkName = *s->LinkName;

				//LLVMCreateFunctionType(Gen.ctx, s->Type);
				LLVMValueRef Fn = LLVMAddFunction(Gen.mod, LinkName.Data, 
						ConvertToLLVMType(&Gen, s->Type));
				LLVMSetLinkage(Fn, Linkage);
				LLVMSetVisibility(Fn, LLVMDefaultVisibility);
				Functions.Push({.LLVM = Fn, .Name = LinkName});
				Gen.map.Add(s->Register, Fn);
				AddedFns.Add(LinkName, Fn);
			}
			else
			{
				string LinkName = *s->LinkName;
				LLVMTypeRef LLVMType = ConvertToLLVMType(&Gen, s->Type);
				LLVMValueRef Global = LLVMAddGlobal(Gen.mod, LLVMType, LinkName.Data);
				LLVMSetLinkage(Global, Linkage);
				if(m->Name == M->Name)
					LLVMSetInitializer(Global, LLVMConstNull(LLVMType));
				Gen.map.Add(s->Register, Global);
				AddedFns.Add(LinkName, Global);
			}
		}
	}
	Gen.map.LockBottom();

	string TypeTableInitName = STR_LIT("init.__TypeTableInit");
	ForArray(FIdx, M->Files)
	{
		ir *IR = M->Files[FIdx]->IR;
		ForArray(Idx, IR->Functions)
		{
			if(IR->Functions[Idx].Blocks.Count != 0)
			{
				string Name = *IR->Functions[Idx].LinkName;
				if(Name == TypeTableInitName)
					continue;
				ForArray(LLVMFnIdx, Functions)
				{
					if(Functions[LLVMFnIdx].Name == Name)
					{
						Gen.fn = Functions[LLVMFnIdx].LLVM;
						break;
					}
				}
				Assert(Gen.fn);

				RCGenerateFunction(&Gen, IR->Functions[Idx]);
				Gen.map.Clear();
				Gen.fn = NULL;
			}
		}

		if(FIdx + 1 != M->Files.Count)
		{
			char *FileName = NULL;
			char *FileDirectory = NULL;
			GetNameAndDirectory(&FileName, &FileDirectory, M->Files[FIdx+1]->Name);
			Gen.f_dbg = LLVMDIBuilderCreateFile(Gen.dbg,
					FileName, VStrLen(FileName),
					FileDirectory, VStrLen(FileDirectory));
		}
	}


	LLVMDIBuilderFinalize(Gen.dbg);

	RunOptimizationPasses(&Gen, Machine.Target, Info->Optimization, Info->Flags);
	RCEmitFile(Machine.Target, Gen.mod, M->Name, OutputBC);

	LLVMDisposeDIBuilder(Gen.dbg);
	LLVMDisposeBuilder(Gen.bld);
	Gen.LLVMDebugTypeMap.Free();
	Gen.LLVMTypeMap.Free();
}

void RCEmitFile(LLVMTargetMachineRef Machine, LLVMModuleRef Mod, string FileName, b32 OutputBC)
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
		BCFileBuilder += FileName;
		BCFileBuilder += ".bc";
		string BCFile = MakeString(BCFileBuilder);

		if(LLVMPrintModuleToFile(Mod, BCFile.Data, &Error))
		{
			LERROR("Couldn't Print LLVM: %s", Error);
			LLVMDisposeMessage(Error);
		}
	}

	string_builder Obj = MakeBuilder();
	Obj += FileName;
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
	snprintf(Buff, 128, "block_%d", ID);
		Result.Block = LLVMAppendBasicBlockInContext(gen->ctx, gen->fn, Buff);

	if(Set)
		LLVMPositionBuilderAtEnd(gen->bld, Result.Block);

	return Result;
}

void RCSetBlock(generator *gen, int Index)
{
	if(gen->CurrentBlock == Index)
		return;

	gen->CurrentBlock = Index;
	LLVMPositionBuilderAtEnd(gen->bld, gen->blocks[Index].Block);
}

llvm_init_info RCInitLLVM(compile_info *Info)
{
	LLVMNoThreadSafetyMutex.lock();
	const char *features = LLVMGetHostCPUFeatures();
	if(Info->TargetTriple.Data == NULL)
	{
		Info->TargetTriple.Data = LLVMGetDefaultTargetTriple();
		Info->TargetTriple.Count = VStrLen(Info->TargetTriple.Data);
	}
	else
	{
		features = "";
	}
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
	LLVMInitializeAllDisassemblers();

	LLVMTargetRef Target;
	LLVMTargetMachineRef Machine;
	char *ErrorMessage = NULL;
	if(LLVMGetTargetFromTriple(Info->TargetTriple.Data, &Target, &ErrorMessage)) {
		LFATAL("Failed to find info for target triple %s\nLLVMError: %s", Info->TargetTriple.Data, ErrorMessage);
	}

	LLVMRelocMode reloc = LLVMRelocDefault;
	if(Info->Flags & CF_CrossAndroid)
		reloc = LLVMRelocPIC;

	Machine = LLVMCreateTargetMachine(Target, Info->TargetTriple.Data, "generic", features,
			LLVMCodeGenLevelNone, reloc, LLVMCodeModelDefault);

	llvm_init_info Result = {};
	Result.Target = Machine;
	LLVMNoThreadSafetyMutex.unlock();
	return Result;
}

LLVMValueRef RCGenerateMainFn(generator *gen, slice<file*> Files, LLVMValueRef InitFn)
{
	LLVMValueRef *FileFns = (LLVMValueRef *)VAlloc((Files.Count+1) * sizeof(LLVMValueRef));

	LLVMTypeRef FnType = LLVMFunctionType(LLVMVoidTypeInContext(gen->ctx), NULL, 0, false);
	ForArray(Idx, Files)
	{
		file *File = Files[Idx];
		if(File->Module->Name == STR_LIT("init"))
		{
			FileFns[Idx] = InitFn;
			continue;
		}

		int FileIndex = GetFileIndex(File->Module, File);

		string_builder StrBuilder = MakeBuilder();
		PushBuilderFormated(&StrBuilder, "__GlobalInitializerFunction.%d", FileIndex);
		string GlobalInit = MakeString(StrBuilder);
		string InitFnName = StructToModuleName(GlobalInit, File->Module->Name);
		FileFns[Idx] = LLVMAddFunction(gen->mod, InitFnName.Data, FnType);
	}

	LLVMValueRef MainFn = LLVMAddFunction(gen->mod, "init.global_initializers", FnType);
	LLVMBasicBlockRef Block = LLVMAppendBasicBlockInContext(gen->ctx, MainFn, "only_block");
	LLVMPositionBuilderAtEnd(gen->bld, Block);


	ForArray(Idx, Files)
	{
		LLVMTypeRef FnType = LLVMFunctionType(LLVMVoidTypeInContext(gen->ctx), NULL, 0, false);
		LLVMBuildCall2(gen->bld, FnType, FileFns[Idx], NULL, 0, "");
	}

	LLVMBuildRetVoid(gen->bld);
	return MainFn;
}

struct file_generate_info
{
	module *M;
	b32 OutputBC;
	slice<module*> Modules;
	slice<file*> Files;
	compile_info *Info;
};

void GenWorkerFn(void *Data)
{
	file_generate_info *Info = (file_generate_info *)Data;
	RCGenerateFile(Info->M, Info->OutputBC, Info->Modules, Info->Files, Info->Info);
	LDEBUG("Done with module: %s", Info->M->Name.Data);
}

void RCGenerateCode(slice<module*> Modules, slice<file*> Files, u32 CommandFlags, compile_info *Info)
{
	work_queue *Queue = CreateWorkQueue();
	if((CommandFlags & CommandFlag_nothread) == 0)
	{
		InitWorkQueue(Queue);
		Assert(LLVMIsMultithreaded());
	}

	ForArray(Idx, Modules)
	{
		file_generate_info *FInfo = NewType(file_generate_info);
		FInfo->M = Modules[Idx];
		FInfo->OutputBC = CommandFlags & CommandFlag_llvm;
		FInfo->Modules = Modules;
		FInfo->Files = Files;
		FInfo->Info = Info;
		job Job = {
			.Task = GenWorkerFn,
			.Data = FInfo,
		};
		PostJob(Queue, Job);
	}

	while(!IsQueueDone(Queue))
	{
		TryDoWork(Queue);
	}
	LLVMShutdown();
}

