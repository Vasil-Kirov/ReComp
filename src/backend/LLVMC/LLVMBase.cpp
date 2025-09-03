#include "LLVMBase.h"
#include "CommandLine.h"
#include "Globals.h"
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
#include <cstdlib>
#include <unordered_map>

std::mutex LLVMNoThreadSafetyMutex;

LLVMValueRef RCGetStringConstPtr(generator *gen, const string *String)
{
	auto LLVMu8 = LLVMIntTypeInContext(gen->ctx, 8);
	size_t Size = String->Size;
	if(Size == 0)
		Size = strlen(String->Data);

	auto Chars = array<LLVMValueRef>(Size+1);
	ForArray(Idx, Chars)
	{
		Chars[Idx] = LLVMConstInt(LLVMu8, String->Data[Idx], false);
	}
	Chars[Size] = LLVMConstNull(LLVMu8);

	auto ArrayType = LLVMArrayType(LLVMu8, Size+1);
	LLVMValueRef Global = LLVMAddGlobal(gen->mod, ArrayType, "");
	LLVMValueRef Init = LLVMConstArray(LLVMu8, Chars.Data, Chars.Count);

	LLVMSetInitializer(Global, Init);
	LLVMSetGlobalConstant(Global, true);
	LLVMSetLinkage(Global, LLVMLinkerPrivateLinkage);
	//return LLVMBuildGlobalStringPtr(gen->bld, String->Data, "");
	return Global;
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

	string Trap = STR_LIT("llvm.debugtrap");
	LLVMTypeRef TrapType = LLVMFunctionType(
			LLVMVoidTypeInContext(gen->ctx), NULL, 0, false);
	LLVMValueRef TrapLLVM = LLVMAddFunction(gen->mod, Trap.Data, TrapType);
	gen->Intrinsics.Add(Trap, llvm_intrin { TrapLLVM, TrapType } );
}

void RCGenerateDebugInfo(generator *gen, ir_debug_info *Info)
{
	if((g_CompileFlags & CF_DebugInfo) == 0)
		return;
	switch(Info->type)
	{
		case IR_DBG_VAR:
		{
			if(gen->CurrentScope == NULL)
				return;
			LLVMValueRef LLVM = gen->map.Get(Info->var.Register);
			auto m_info = LLVMDIBuilderCreateAutoVariable(gen->dbg, gen->CurrentScope, Info->var.Name.Data, Info->var.Name.Size, gen->f_dbg, Info->var.LineNo, ToDebugTypeLLVM(gen, Info->var.TypeID), false, LLVMDIFlagZero, 0);
			LLVMMetadataRef Expr = LLVMDIBuilderCreateExpression(gen->dbg, NULL, 0);
			LLVMDIBuilderInsertDeclareRecordAtEnd(gen->dbg, LLVM, m_info, Expr, gen->CurrentLocation, gen->blocks[gen->CurrentBlock].Block);
			//LLVMDIBuilderInsertDeclareAtEnd(gen->dbg, LLVM, m_info, Expr, gen->CurrentLocation, gen->blocks[gen->CurrentBlock].Block);
		} break;
		case IR_DBG_ERROR_INFO:
		{
			if(gen->CurrentScope == NULL)
				return;
			auto err_i = Info->err_i.ErrorInfo;
			gen->CurrentLocation = LLVMDIBuilderCreateDebugLocation(gen->ctx, err_i->Range.StartLine, 0, gen->CurrentScope, NULL);
			LLVMSetCurrentDebugLocation2(gen->bld, gen->CurrentLocation);
		} break;
		case IR_DBG_SCOPE:
		{
			gen->CurrentScope = LLVMDIBuilderCreateLexicalBlock(gen->dbg, gen->CurrentScope, gen->f_dbg, Info->loc.LineNo, 0);
		} break;
	}
}

template<typename t>
dynamic<t> RCCopyTypeMap(dynamic<t> Map)
{
	dynamic<t> Result = {};
	For(Map) {
		t Add = *it;//t { it->TypeID, it->Ref };
		Result.Push(Add);
	}
	return Result;
}

LLVMValueRef FromPtr(generator *gen, u32 TIdx, void *Ptr)
{
	const type *T = GetType(TIdx);
	LLVMTypeRef lt = ConvertToLLVMType(gen, TIdx);
	switch(T->Kind)
	{
		case TypeKind_Generic:
		case TypeKind_Function:
		case TypeKind_Invalid:
		unreachable;

		case TypeKind_Enum: { return FromPtr(gen, T->Enum.Type, Ptr); } break;
		case TypeKind_Basic:
		{
			switch(T->Basic.Kind)
			{
				case Basic_bool:
				{
					return LLVMConstInt(lt, *(u8 *)Ptr, false);
				} break;
				case Basic_u8:
				{
					return LLVMConstInt(lt, *(u8 *)Ptr, false);
				} break;
				case Basic_u16:
				{
					return LLVMConstInt(lt, *(u16 *)Ptr, false);
				} break;
				case Basic_u32:
				{
					return LLVMConstInt(lt, *(u32 *)Ptr, false);
				} break;
				case Basic_u64:
				{
					return LLVMConstInt(lt, *(u64 *)Ptr, false);
				} break;
				case Basic_i8:
				{
					return LLVMConstInt(lt, *(i8 *)Ptr, true);
				} break;
				case Basic_i16:
				{
					return LLVMConstInt(lt, *(i16 *)Ptr,true);
				} break;
				case Basic_i32:
				{
					return LLVMConstInt(lt, *(i32 *)Ptr,true);
				} break;
				case Basic_i64:
				{
					return LLVMConstInt(lt, *(i64 *)Ptr,true);
				} break;
				case Basic_f32:
				{
					return LLVMConstReal(lt, *(f32 *)Ptr);
				} break;
				case Basic_f64:
				{
					return LLVMConstReal(lt, *(f64 *)Ptr);
				} break;
				case Basic_type:
				case Basic_int:
				{
					int RegisterSize = GetRegisterTypeSize() / 8;
					switch(RegisterSize)
					{
						case 8: return LLVMConstInt(lt, *(i64 *)Ptr, true);
						case 4: return LLVMConstInt(lt, *(i32 *)Ptr, true);
						case 2: return LLVMConstInt(lt, *(i16 *)Ptr, true);
						default: unreachable;
					}
				} break;
				case Basic_uint:
				{
					int RegisterSize = GetRegisterTypeSize() / 8;
					switch(RegisterSize)
					{
						case 8: return LLVMConstInt(lt, *(u64 *)Ptr, false);
						case 4: return LLVMConstInt(lt, *(u32 *)Ptr, false);
						case 2: return LLVMConstInt(lt, *(u16 *)Ptr, false);
						default: unreachable;
					}
				} break;
				case Basic_string:
				{
					LLVMTypeRef IntTy = LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize());
					LLVMTypeRef Ptr = LLVMPointerTypeInContext(gen->ctx, 0);
					string Str;
					Str.Size = *(size_t *)Ptr;
					Str.Data = *((const char **)Ptr+1);

					LLVMValueRef Size = LLVMConstInt(IntTy, Str.Size, true);
					LLVMValueRef Data = RCGetStringConstPtr(gen, &Str);
					LLVMValueRef Vals[2] = { Size, Data };
					return LLVMConstStruct(Vals, 2, false);
				} break;
				case Basic_UntypedFloat:
				case Basic_UntypedInteger:
				case Basic_auto:
				case Basic_module:
				Assert(false);
			}
		} break;
		case TypeKind_Vector:
		{
			Assert(T->Vector.ElementCount <= 4);
			switch(T->Vector.Kind)
			{
				case Vector_Float:
				{
					if(T->Vector.ElementCount == 2)
					{
						LLVMValueRef Elems[2] = {};

						float *es = (float *)Ptr;
						for(int i = 0; i < T->Vector.ElementCount; ++i)
						{
							Elems[i] = LLVMConstReal(LLVMFloatTypeInContext(gen->ctx), es[i]);
						}
						return LLVMConstVector(Elems, T->Vector.ElementCount);
					}
					else
					{
						LLVMValueRef Elems[4] = {};

						VEC4 v = {.v = _mm_load_ps((float *)Ptr)};
						for(int i = 0; i < T->Vector.ElementCount; ++i)
						{
							Elems[i] = LLVMConstReal(LLVMFloatTypeInContext(gen->ctx), v.e[i]);
						}
						return LLVMConstVector(Elems, T->Vector.ElementCount);
					}
				} break;
				case Vector_Int:
				{
					if(T->Vector.ElementCount == 2)
					{
						LLVMValueRef Elems[2] = {};

						i32 *es = (i32 *)Ptr;
						for(int i = 0; i < T->Vector.ElementCount; ++i)
						{
							Elems[i] = LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), es[i], true);
						}
						return LLVMConstVector(Elems, T->Vector.ElementCount);
					}
					else
					{
						LLVMValueRef Elems[4] = {};

						IVEC4 v = {.v = _mm_load_si128((__m128i *)Ptr)};
						for(int i = 0; i < T->Vector.ElementCount; ++i)
						{
							Elems[i] = LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), v.e[i], true);
						}
						return LLVMConstVector(Elems, T->Vector.ElementCount);
					}
				} break;
			}
		} break;
		case TypeKind_Pointer:
		{
			void *Val = *(void **)Ptr;
			auto found = gen->StoredGlobals.find(Ptr);
			if(found != gen->StoredGlobals.end())
			{
				return gen->global.Get(found->second);
			}
			return LLVMConstInt(lt, (unsigned long long)Val, false);
		} break;
		case TypeKind_Struct:
		{
			u8 *Start = (u8 *)Ptr;
			auto Values = array<LLVMValueRef>(T->Struct.Members.Count);
			int Count = 0;
			ForArray(Idx, T->Struct.Members)
			{
				u8 *At = Start + GetStructMemberOffset(T, Idx);
				Values[Count++] = FromPtr(gen, T->Struct.Members[Idx].Type, At);
			}
			return LLVMConstNamedStruct(lt, Values.Data, Count);
		} break;
		case TypeKind_Slice:
		{
			LLVMTypeRef DataType = ConvertToLLVMType(gen, T->Slice.Type);
			LLVMTypeRef SizeType = ConvertToLLVMType(gen, Basic_int);
			size_t TypeSize = GetTypeSize(T->Slice.Type);
			u8 *At = (u8 *)Ptr;
			size_t Size = *(size_t *)At;
			At += sizeof(size_t);
			u8 *Elems = *(u8 **)At;
			auto Values = array<LLVMValueRef>(Size);
			int Count = 0;
			for(int i = 0; i < Size; ++i)
			{
				Values[Count++] = FromPtr(gen, T->Slice.Type, Elems);
				Elems += TypeSize;
			}
			LLVMTypeRef GlobalArrayType = LLVMArrayType2(DataType, Count);
			LLVMValueRef DataArray = LLVMConstArray2(DataType, Values.Data, Count);
			LLVMValueRef DataPtr = LLVMAddGlobal(gen->mod, GlobalArrayType, "");
			LLVMValueRef SizeVal = LLVMConstInt(SizeType, Size, true);

			LLVMSetInitializer(DataPtr, DataArray);
			LLVMSetLinkage(DataPtr, LLVMLinkerPrivateLinkage);

			auto Slice = SliceFromConst({SizeVal, DataPtr});
			return LLVMConstNamedStruct(lt, Slice.Data, 2);
		} break;
		case TypeKind_Array:
		{
			u8 *At = (u8 *)Ptr;
			int ElemSize = GetTypeSize(T->Array.Type);
			auto Values = array<LLVMValueRef>(T->Array.MemberCount);
			int Count = 0;
			for(int i = 0; i < T->Array.MemberCount; ++i)
			{
				Values[Count++] = FromPtr(gen, T->Array.Type, At);
				At += ElemSize;
			}
			return LLVMConstArray2(ConvertToLLVMType(gen, T->Array.Type), Values.Data, Count);
		} break;
	}
}

LLVMValueRef FromConstVal(generator *gen, const_value *Val, u32 TypeIdx, b32 StructAsPointer)
{
	const type *Type = GetType(TypeIdx);
	if(Type->Kind == TypeKind_Enum)
	{
		Type = GetType(Type->Enum.Type);
	}

	LLVMTypeRef LLVMType = ConvertToLLVMType(gen, TypeIdx);
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
			auto f = gen->StoredGlobals.find((void *)Val->Int.Unsigned);
			if(f != gen->StoredGlobals.end())
			{
				return gen->global.Get(f->second);
			}
			auto IntTy = ConvertToLLVMType(gen, Basic_uint);
			Value = LLVMConstInt(IntTy, Val->Int.Unsigned, false);
			Value = LLVMConstIntToPtr(Value, LLVMType);
		}
	}
	else if(Type->Kind == TypeKind_Vector)
	{
		switch(Type->Vector.Kind)
		{
			case Vector_Float:
			{
				VEC4 v = {.v = Val->Vector.F};
				LLVMValueRef Values[16] = {};
				Assert(Type->Vector.ElementCount <= 4);
				for(int i = 0; i < Type->Vector.ElementCount; ++i)
				{
					Values[i] = LLVMConstReal(LLVMFloatTypeInContext(gen->ctx), v.e[i]);
				}
				Value = LLVMConstVector(Values, Type->Vector.ElementCount);
			} break;
			case Vector_Int:
			{
				IVEC4 v = {.v = Val->Vector.I};
				LLVMValueRef Values[16] = {};
				Assert(Type->Vector.ElementCount <= 4);
				for(int i = 0; i < Type->Vector.ElementCount; ++i)
				{
					Values[i] = LLVMConstInt(LLVMInt32TypeInContext(gen->ctx), v.e[i], true);
				}
				Value = LLVMConstVector(Values, Type->Vector.ElementCount);
			} break;
		}
	}
	else if(HasBasicFlag(Type, BasicFlag_Float))
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
	else if(HasBasicFlag(Type, BasicFlag_Integer))
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
	else if(IsString(Type))
	{
		LLVMTypeRef IntType = ConvertToLLVMType(gen, Basic_int);
		LLVMValueRef DataPtr = RCGetStringConstPtr(gen, Val->String.Data);
		LLVMValueRef Size    = LLVMConstInt(IntType, Val->String.Data->Size, true);
		LLVMValueRef ConstantVals[2] = { Size, DataPtr };
		Value = LLVMConstNamedStruct(LLVMType, ConstantVals, 2);
		if(StructAsPointer)
		{
			LLVMValueRef Init = Value;
			Value = LLVMAddGlobal(gen->mod, LLVMType, "");
			LLVMSetInitializer(Value, Init);
			LLVMSetGlobalConstant(Value, true);
			LLVMSetLinkage(Value, LLVMLinkerPrivateLinkage);
		}
	}
	else if(HasBasicFlag(Type, BasicFlag_Boolean))
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
	else if(Type->Kind == TypeKind_Struct || Type->Kind == TypeKind_Slice || Type->Kind == TypeKind_Array)
	{
		Assert(Val->Type == const_type::Aggr);
		Value = FromPtr(gen, TypeIdx, Val->Struct.Ptr);
		if(StructAsPointer)
		{
			LLVMValueRef Initializer = Value;
			Value = LLVMAddGlobal(gen->mod, LLVMType, "");
			LLVMSetInitializer(Value, Initializer);
			LLVMSetGlobalConstant(Value, true);
			LLVMSetLinkage(Value, LLVMLinkerPrivateLinkage);
		}
	}
	else
	{
		LDEBUG("%d", Type->Kind);
		unreachable;
	}
	return Value;
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
		if(IsFloatOrVec(Type)) \
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
		case OP_RESULT:
		case OP_NOP:
		{
		} break;
		case OP_ALLOC: // Handled before
		{
		} break;
		case OP_GLOBAL:
		{
			const symbol *s = (const symbol *)I.Ptr;
			gen->map.Add(I.Result, gen->global.Get(s->Register));
		} break;
		case OP_RUN:
		{
			int CBlock = gen->CurrentBlock;
			LLVMPositionBuilderAtEnd(gen->bld, gen->blocks[I.Right].Block);
			//LLVMBuildUnreachable(gen->bld);

			LLVMPositionBuilderAtEnd(gen->bld, gen->blocks[CBlock].Block);
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
		case OP_DEBUG_BREAK:
		{
			llvm_intrin Intrin = gen->Intrinsics[STR_LIT("llvm.debugtrap")];
			Assert(Intrin.Fn);
			LLVMBuildCall2(gen->bld, Intrin.Type, Intrin.Fn, NULL, 0, "");
		} break;
		case OP_ATOMIC_ADD:
		{
			call_info *ci = (call_info *)I.BigRegister;
			LLVMValueRef Ptr = gen->map.Get(ci->Args[0]);
			LLVMValueRef Val = gen->map.Get(ci->Args[1]);
			LLVMValueRef OldVal = LLVMBuildAtomicRMW(gen->bld, LLVMAtomicRMWBinOpAdd, Ptr, Val, LLVMAtomicOrderingSequentiallyConsistent, false);

			gen->map.Add(I.Result, OldVal);
		} break;
		case OP_ATOMIC_LOAD:
		{
			call_info *ci = (call_info *)I.BigRegister;
			LLVMTypeRef IntType = ConvertToLLVMType(gen, Basic_int);
			LLVMValueRef Ptr = gen->map.Get(ci->Args[0]);
			LLVMValueRef Val = LLVMBuildLoad2(gen->bld, IntType, Ptr, "");
			LLVMSetOrdering(Val, LLVMAtomicOrderingSequentiallyConsistent);
			gen->map.Add(I.Result, Val);
		} break;
		case OP_FENCE:
		{
			LLVMBuildFence(gen->bld, LLVMAtomicOrderingSequentiallyConsistent, false, "");
		} break;
		case OP_CMPXCHG:
		{
			call_info *ci = (call_info *)I.BigRegister;
			Assert(ci->Args.Count == 3);

			LLVMValueRef Ptr = gen->map.Get(ci->Args[0]);
			LLVMValueRef Cmp = gen->map.Get(ci->Args[1]);
			LLVMValueRef New = gen->map.Get(ci->Args[2]);
			LLVMValueRef Result = LLVMBuildAtomicCmpXchg(gen->bld, Ptr, Cmp, New, LLVMAtomicOrderingSequentiallyConsistent, LLVMAtomicOrderingSequentiallyConsistent, false);

			LLVMValueRef Alloc = gen->map.Get(I.Result);
			LLVMValueRef Val = LLVMBuildExtractValue(gen->bld, Result, 0, "");
			LLVMValueRef Success = LLVMBuildExtractValue(gen->bld, Result, 1, "");


			LLVMTypeRef Types[] = {
				LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()),
				LLVMIntTypeInContext(gen->ctx, 1)
			};

			LLVMTypeRef ResTy = LLVMStructType(Types, 2, false);

			LLVMValueRef ValPtr = LLVMBuildStructGEP2(gen->bld, ResTy, Alloc, 0, "");
			LLVMValueRef SuccessPtr = LLVMBuildStructGEP2(gen->bld, ResTy, Alloc, 1, "");
			LLVMBuildStore(gen->bld, Val, ValPtr);
			LLVMBuildStore(gen->bld, Success, SuccessPtr);

		} break;
		case OP_TYPEINFO:
		{
			u32 TInfo = I.Type;
			u32 TInfoPtr = GetPointerTo(I.Type);
			u32 TInfoSlice = GetSliceType(TInfo);
			LLVMValueRef TypeTable = gen->global.Get(I.Left);
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
				LLVMValueRef ValueSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
				LLVMBuildMemSet(gen->bld, Pointer, Zero, ValueSize, Alignment);
			}
		} break;
		case OP_ALLOCGLOBAL:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef Value = LLVMAddGlobal(gen->mod, LLVMArrayType2(LLVMType, I.BigRegister), "");
			LLVMSetInitializer(Value, LLVMConstNull(LLVMArrayType2(LLVMType, I.BigRegister)));
			LLVMSetLinkage(Value, LLVMLinkerPrivateLinkage);
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

			LLVMValueRef Value = FromConstVal(gen, Val, I.Type, true);

			gen->map.Add(I.Result, Value);
		} break;
		case OP_NULL:
		{
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef Null = LLVMConstNull(LLVMType);
			gen->map.Add(I.Result, Null);
		} break;
		case OP_UNREACHABLE:
		{
			LLVMBuildUnreachable(gen->bld);
		} break;
		case OP_EXTRACT:
		{
			LLVMTypeRef IntTy = LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize());
			LLVMValueRef Vec = gen->map.Get(I.Left);
			LLVMValueRef Idx = LLVMConstInt(IntTy, I.Right, false);
			LLVMValueRef Res = LLVMBuildExtractElement(gen->bld, Vec, Idx, "");
			gen->map.Add(I.Result, Res);
		} break;
		case OP_INSERT:
		{
			LLVMTypeRef IntTy = LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize());
			if(I.Ptr == NULL)
			{
				u32 VecElem = GetVecElemType(I.Type);
				LLVMTypeRef VecTy = ConvertToLLVMType(gen, I.Type);
				LLVMTypeRef ElemTy = ConvertToLLVMType(gen, VecElem);
				LLVMValueRef Zero = LLVMConstInt(IntTy, 0, false);
				LLVMValueRef ElemNull = LLVMConstNull(ElemTy);
				LLVMValueRef Vec = LLVMGetUndef(VecTy);
				LLVMValueRef Out = LLVMBuildInsertElement(gen->bld, Vec, ElemNull, Zero, "");
				gen->map.Add(I.Result, Out);
			}
			else
			{
				ir_insert *Ins = (ir_insert *)I.Ptr;
				LLVMValueRef Vec = gen->map.Get(Ins->Register);
				LLVMValueRef Val = gen->map.Get(Ins->ValueRegister);
				LLVMValueRef Idx = LLVMConstInt(IntTy, Ins->Idx, false);
				LLVMValueRef Out = LLVMBuildInsertElement(gen->bld, Vec, Val, Idx, "");
				gen->map.Add(I.Result, Out);
			}
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
			if(IsFloatOrVec(Type))
			{
				Val = LLVMBuildFDiv(gen->bld, LHS, RHS, "");
			}
			else
			{
				if(!IsSigned(Type))
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
			if(IsFloatOrVec(Type))
			{
				Val = LLVMBuildFRem(gen->bld, LHS, RHS, "");
			}
			else
			{
				if(!IsSigned(Type))
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
			function *Fn = (function *)I.Ptr;
			LLVMTypeRef FnType = LLVMCreateFunctionType(gen, Fn->Type);
			LLVMValueRef LLVMFn = LLVMAddFunction(gen->mod, Fn->Name->Data, FnType);
			LLVMSetLinkage(LLVMFn, LLVMPrivateLinkage);


			generator NewGen = {.StoredGlobals = gen->StoredGlobals};
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
			NewGen.global = gen->global;

			gen->map.Add(I.Result, LLVMFn);
			RCGenerateFunction(&NewGen, *Fn);
			LLVMSetCurrentDebugLocation2(gen->bld, gen->CurrentLocation);
			NewGen.LLVMTypeMap.Free();
			NewGen.LLVMDebugTypeMap.Free();
		} break;
		case OP_PTRCAST:
		{
			LLVMValueRef Target = gen->map.Get(I.Right);
			LLVMTypeRef DestTy = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef Val = LLVMBuildPointerCast(gen->bld, Target, DestTy, "");
			gen->map.Add(I.Result, Val);
		} break;
		case OP_BITCAST:
		{
			LLVMValueRef Target = gen->map.Get(I.Right);
			LLVMTypeRef DestTy = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef Val = LLVMBuildBitCast(gen->bld, Target, DestTy, "");
			gen->map.Add(I.Result, Val);
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
					LLVMValueRef ValueSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
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
				LLVMValueRef ValueSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
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
			LLVMValueRef ValueSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
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
		case OP_MEMCPY:
		{
			LLVMValueRef Dst = gen->map.Get(I.Left);
			LLVMValueRef Src = gen->map.Get(I.Right);
			uint Size = GetTypeSize(I.Type);
			LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
			LLVMValueRef LLVMSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
			uint Align = LLVMPreferredAlignmentOfType(gen->data, LLVMType);
			LLVMBuildMemCpy(gen->bld, Dst, Align, Src, Align, LLVMSize);
		} break;
		case OP_ARG:
		{
			u64 Index = I.BigRegister;
			arg_location Loc;
			if(Index < gen->irfn.Args.Count)
			{
				Loc = gen->irfn.Args[Index];
			}
			else if(gen->irfn.Args.Count == 0)
			{
				Loc = {.Load = LoadAs_Normal, .Start = (int)Index, .Count = 1};
			}
			else
			{
				arg_location Last = gen->irfn.Args[gen->irfn.Args.Count - 1];
				u64 AfterLast = Index - gen->irfn.Args.Count;
				Loc = arg_location{.Load = LoadAs_Normal, .Start = Last.Start + Last.Count + (int)AfterLast, .Count = 1};
			}

			switch(Loc.Load)
			{
				case LoadAs_Normal:
				{
					Assert(Loc.Count == 1);
					LLVMValueRef Arg = LLVMGetParam(gen->fn, Loc.Start);
					gen->map.Add(I.Result, Arg);
				} break;
				case LoadAs_Int:
				{
					Assert(Loc.Count == 1);
					Assert(GetType(I.Type)->Kind == TypeKind_Struct);
					LLVMValueRef Arg = LLVMGetParam(gen->fn, Loc.Start);
					LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
					LLVMValueRef AsArg = LLVMBuildAlloca(gen->bld, LLVMType, "arg");
					LLVMBuildStore(gen->bld, Arg, AsArg);
					gen->map.Add(I.Result, AsArg);
				} break;
				case LoadAs_MultiInt:
				{
					Assert(Loc.Count == 2);
					Assert(GetType(I.Type)->Kind == TypeKind_Struct);
					LLVMValueRef Int1 = LLVMGetParam(gen->fn, Loc.Start);
					LLVMValueRef Int2 = LLVMGetParam(gen->fn, Loc.Start+1);
					LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
					LLVMValueRef AsArg = LLVMBuildAlloca(gen->bld, LLVMType, "arg");
					LLVMBuildStore(gen->bld, Int1, AsArg);
					int Idx = GetPointerPassIdx(I.Type, 8);
					LLVMValueRef Ptr = LLVMBuildStructGEP2(gen->bld, LLVMType, AsArg, Idx, "");
					LLVMBuildStore(gen->bld, Int2, Ptr);

					gen->map.Add(I.Result, AsArg);
				} break;
				case LoadAs_Floats:
				{
					const type *T = GetType(I.Type);
					Assert(T->Kind == TypeKind_Struct);
					LLVMTypeRef LLVMType = ConvertToLLVMType(gen, I.Type);
					LLVMValueRef AsArg = LLVMBuildAlloca(gen->bld, LLVMType, "arg");
					int At = Loc.Start;

					int i = 0;
					for(; i+1 < T->Struct.Members.Count; ++i)
					{
						LLVMValueRef Ptr = LLVMBuildStructGEP2(gen->bld, LLVMType, AsArg, i, "");
						u32 First = T->Struct.Members[i].Type;
						u32 Second = T->Struct.Members[i + 1].Type;
						if(First == Basic_f32 && Second == Basic_f32)
						{
							//Ptr = LLVMBuildPointerCast(gen->bld, Ptr, LLVMVectorType(LLVMFloatTypeInContext(gen->ctx), 2), "");
							LLVMBuildStore(gen->bld, LLVMGetParam(gen->fn, At), Ptr);
							++i;
						}
						else
						{
							LLVMBuildStore(gen->bld, LLVMGetParam(gen->fn, At), Ptr);
						}
						At++;
					}

					if(i < T->Struct.Members.Count)
					{
						LLVMValueRef Ptr = LLVMBuildStructGEP2(gen->bld, LLVMType, AsArg, i, "");
						LLVMBuildStore(gen->bld, LLVMGetParam(gen->fn, At), Ptr);
					}

					gen->map.Add(I.Result, AsArg);
				} break;
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
					LLVMValueRef ValueSize = LLVMConstInt(LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()), Size, false);
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
			LLVMBasicBlockRef Else = NULL;
			u32 BlockCount = Info->Cases.Count;
			if(Info->Default != -1)
			{
				Else = gen->blocks[Info->Cases[Info->Default]].Block;
				BlockCount--;
			}
			else
			{
				Else = gen->blocks[Info->After].Block;
			}
			LLVMValueRef Switch = LLVMBuildSwitch(gen->bld, Matcher, Else, BlockCount);

			ForArray(Idx, Info->Cases)
			{
				if(Idx != Info->Default)
				{
					u32 Case = Info->Cases[Idx];
					LLVMValueRef V = gen->map.Get(Info->OnValues[Idx]);
					LLVMAddCase(Switch, V, gen->blocks[Case].Block);
				}
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
		case OP_BITNOT:
		{
			LLVMValueRef RHS = gen->map.Get(I.Right);
			LLVMValueRef Value = LLVMBuildNot(gen->bld, RHS, "");
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
			ir_debug_info *Info = (ir_debug_info *)I.Ptr;
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
	auto Compare = [](const void *Aptr, const void *Bptr) -> int {
		basic_block *A = (basic_block *)Aptr;
		basic_block *B = (basic_block *)Bptr;
		return (i32)A->ID - (i32)B->ID;
	};
	qsort(fn.Blocks.Data, fn.Blocks.Count, sizeof(basic_block), Compare);
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
				case OP_CMPXCHG:
				{
					LLVMTypeRef Types[] = {
						LLVMIntTypeInContext(gen->ctx, GetRegisterTypeSize()),
						LLVMIntTypeInContext(gen->ctx, 1)
					};

					LLVMTypeRef ResTy = LLVMStructType(Types, 2, false);
					LLVMValueRef Alloc = LLVMBuildAlloca(gen->bld, ResTy, "");
					gen->map.Add(I.Result, Alloc);
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
		gen->IsCurrentFnRetInPtr = fn.ReturnPassedInPtr;
		gen->irfn = fn;
		if(g_CompileFlags & CF_DebugInfo)
		{
			gen->CurrentScope = RCGenerateDebugInfoForFunction(gen, fn);
			LLVMSetCurrentDebugLocation2(gen->bld, gen->CurrentScope);
		}
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
			DEBUG_RUN(LLMVDebugOpaqueStruct(gen, Index);)
		}
		else if(T->Kind == TypeKind_Enum)
		{
			DEBUG_RUN(LLVMDebugDefineEnum(gen, T, Index);)
		}
	}

	dynamic<u32> ResolveFailed = {};
	for(uint Index = 0; Index < TypeCount; ++Index)
	{
		const type *T = GetType(Index);
		if(T->Kind == TypeKind_Struct && (T->Struct.Flags & StructFlag_Generic) == 0)
		{
			LLVMDefineStructType(gen, Index);
			DEBUG_RUN(
			LLVMMetadataRef Got = LLMVDebugDefineStruct(gen, Index);
			if(Got == NULL)
				ResolveFailed.Push(Index);
			)
		}
	}

	DEBUG_RUN (
	u32 NeedToResolve = ResolveFailed.Count;
	while(ResolveFailed.Count != 0)
	{
		dynamic<u32> ToResolve = ResolveFailed;
		ResolveFailed.Count = 0;
		ResolveFailed.Data = NULL;

		For(ToResolve)
		{
			LLVMMetadataRef Got = LLMVDebugDefineStruct(gen, *it);
			if(Got == NULL)
				ResolveFailed.Push(*it);
		}

		ToResolve.Free();
		if(ResolveFailed.Count == NeedToResolve)
		{
			LERROR("--- INTERNAL COMPILER ERROR ---\nSpin lock on generating struct types, recursive struct definition");
			abort();
		}
		NeedToResolve = ResolveFailed.Count;
	}
	)
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
	auto LLVMType = LLVMDefineStructType(gen, String);
	LLVMMapType(gen, Basic_string, LLVMType);

	DEBUG_RUN(
			LLMVDebugOpaqueStruct(gen, String);
			auto DebugType = LLMVDebugDefineStruct(gen, String);
			LLVMDebugMapType(gen, Basic_string, DebugType);
	)
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

void RCGenerateFile(module *M, b32 OutputBC, compile_info *Info, const std::unordered_map<void *, uint> &StoredGlobals)
{
	LDEBUG("Generating module: %s", M->Name.Data);

	llvm_init_info Machine = RCInitLLVM(Info);

	generator Gen = {.StoredGlobals = StoredGlobals};
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
	DEBUG_RUN (
		Gen.dbg = LLVMCreateDIBuilder(Gen.mod);
		Gen.f_dbg = LLVMDIBuilderCreateFile(Gen.dbg,
				FileName, VStrLen(FileName),
				FileDirectory, VStrLen(FileDirectory));
	)
	
	if(GetRegisterTypeSize() == 64)
	{
		Gen.data = LLVMCreateTargetDataLayout(Machine.Target);
		LLVMSetModuleDataLayout(Gen.mod, Gen.data);
	}
	else if(GetRegisterTypeSize() == 32)
	{
		if(strstr(Info->TargetTriple.Data, "wasm32"))
		{
			LLVMSetDataLayout(Gen.mod, "e-m:e-p:32:32-p10:8:8-p20:8:8-i64:64-f128:64-n32:64-S128-ni:1:10:20");
		}
		else
		{
			LLVMSetDataLayout(Gen.mod, "e-m:x-p:32:32-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:32-n8:16:32-a:0:32-S32");
		}
		Gen.data = LLVMGetModuleDataLayout(Gen.mod);
	}
	else
	{
		LogCompilerError("--- COMPILER BUG ---""\nData layout for %d bit machine is not implemented!", GetRegisterTypeSize());
		exit(1);
	}

	string CompilerName = STR_LIT("RCP Compiler");

	RCGenerateIntrins(&Gen);

	DEBUG_RUN (
		LLVMDIBuilderCreateCompileUnit(
				Gen.dbg,
				LLVMDWARFSourceLanguageC99,
				Gen.f_dbg,
				CompilerName.Data, CompilerName.Size,
				false,
				NULL, 0, 0, "", 0, LLVMDWARFEmissionFull, 0, false, false, "", 0, "", 0);
		)

	DEBUG_RUN (
	if(strstr(Info->TargetTriple.Data, "windows"))
	{
		string CodeView = STR_LIT("CodeView");
		LLVMAddModuleFlag(Gen.mod, LLVMModuleFlagBehaviorWarning,
				CodeView.Data, CodeView.Size, IntToMeta(&Gen, 1));
	}
	else
	{
		string DwarV = STR_LIT("Dwarf Version");
		LLVMAddModuleFlag(Gen.mod, LLVMModuleFlagBehaviorWarning,
				DwarV.Data, DwarV.Size, IntToMeta(&Gen, 5));
	}

	string DIV = STR_LIT("Debug Info Version");
	LLVMAddModuleFlag(Gen.mod, LLVMModuleFlagBehaviorWarning,
			DIV.Data, DIV.Size, IntToMeta(&Gen, 3));
	)

	RCGenerateCompilerTypes(&Gen);
	RCGenerateComplexTypes(&Gen);

	struct gen_fn_info {
		LLVMValueRef LLVM;
		string Name;
	};

	dynamic<gen_fn_info> Functions = {};

	// Generate internal functions
	dict<LLVMValueRef> AddedFns = {};
#if 0
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
#endif

	dict<module*> ModuleDict = {};
	ModuleDict.Add(M->Name, M);
	ForArray(FIdx, M->Files)
	{
		auto Imported = M->Files[FIdx]->Imported;
		For(Imported)
		{
			ModuleDict.Add(it->M->Name, it->M);
		}
	}

	dynamic<module*> UnitModules = ModuleDict.Data;


	b32 IsInitModule = M->Name == STR_LIT("base");
	ForArray(MIdx, UnitModules)
	{
		// shadow
		module *m = UnitModules[MIdx];
		ForArray(GIdx, m->Globals.Data)
		{
			symbol *s = m->Globals.Data[GIdx];
			if(s->Flags & SymbolFlag_Generic || s->Flags & SymbolFlag_Intrinsic) {
				continue;
			}

			if(IsInitModule)
			{
#if 0
				if(*s->Name == STR_LIT("global_initializers"))
				{
					LLVMValueRef Fn = RCGenerateMainFn(&Gen, Files, MaybeInitFn);
					LLVMSetLinkage(Fn, LLVMExternalLinkage);
					Gen.map.Add(s->Register, Fn);
					continue;
				}
#endif
				if(*s->Name == STR_LIT("type_table"))
				{
					LLVMValueRef TypeTable = GenTypeInfo(&Gen);
					LLVMSetLinkage(TypeTable, LLVMExternalLinkage);
					Gen.global.Add(s->Register, TypeTable);
					continue;
				}
			}

			LLVMLinkage Linkage;
			if(s->Flags & SymbolFlag_Public || s->Flags & SymbolFlag_Extern)
				Linkage = LLVMExternalLinkage;
			else
				Linkage = LLVMPrivateLinkage;

			if(Linkage == LLVMPrivateLinkage && s->Checker->Module->Name != M->Name)
				continue;

			LLVMValueRef AlreadyIn = AddedFns[*s->LinkName];
			if(AlreadyIn)
			{
				Gen.global.Add(s->Register, AlreadyIn);
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
				Gen.global.Add(s->Register, Fn);
				AddedFns.Add(LinkName, Fn);
			}
#if 0
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
#endif
		}
	}

	ForArray(MIdx, UnitModules)
	{
		module *m = UnitModules[MIdx];
		ForArray(FIdx, m->Files)
		{
			ir *IR = m->Files[FIdx]->IR;
			For(IR->Globals)
			{
				LLVMLinkage Linkage;
				if(it->s->Flags & SymbolFlag_Public || it->s->Flags & SymbolFlag_Extern)
				{
					Linkage = LLVMExternalLinkage;
				}
				else
				{
					Linkage = LLVMPrivateLinkage;
					if(m->Name != M->Name)
						continue;
				}

				string LinkName = *it->s->LinkName;
				u32 TIdx = it->s->Type;
				const type *T = GetType(TIdx);
				if(T->Kind == TypeKind_Function)
					TIdx = GetPointerTo(TIdx);
				LLVMTypeRef LLVMType = ConvertToLLVMType(&Gen, TIdx);
				LLVMValueRef Global = LLVMAddGlobal(Gen.mod, LLVMType, LinkName.Data);
				//LLVMSetGlobalConstant(Global, it->s->Flags & SymbolFlag_Const);
				LLVMSetLinkage(Global, Linkage);

				if(m->Name == M->Name)
				{
					LLVMValueRef Init;
					if(it->Init.LinkName != NULL)
					{
						Init = FromConstVal(&Gen, &it->Value, TIdx, false);
					}
					else
					{
						Init = LLVMConstNull(LLVMType);
					}

					if((it->s->Flags & SymbolFlag_Extern) == 0)
						LLVMSetInitializer(Global, Init);


					DEBUG_RUN(
							LLVMMetadataRef DebugTy = ToDebugTypeLLVM(&Gen, it->s->Type);
							LLVMMetadataRef Expr = LLVMDIBuilderCreateExpression(Gen.dbg, NULL, 0);
							LLVMMetadataRef Decl = NULL;
							LLVMMetadataRef DebugGlobal = LLVMDIBuilderCreateGlobalVariableExpression(Gen.dbg, Gen.f_dbg,
								it->s->Name->Data, it->s->Name->Size,
								it->s->LinkName->Data, it->s->LinkName->Size,
								Gen.f_dbg, it->s->Node->ErrorInfo->Range.StartLine,
								DebugTy, (it->s->Flags & SymbolFlag_Public) != 0,
								Expr, Decl,
								GetTypeAlignment(it->s->Type)*8);
							LLVMGlobalSetMetadata(Global, 0, DebugGlobal);
							)
				}
				Gen.global.Add(it->s->Register, Global);
			}
		}
	}

	string TypeTableInitName = STR_LIT("base.__TypeTableInit");
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
				Assert(GetType(IR->Functions[Idx].Type)->Kind == TypeKind_Function);
				const type *T = GetType(IR->Functions[Idx].Type);
				if(T->Function.Flags & SymbolFlag_Inline)
				{
					string InlineStr = STR_LIT("alwaysinline");
					LLVMAttributeRef Inline = LLVMCreateEnumAttribute(Gen.ctx,
							LLVMGetEnumAttributeKindForName(InlineStr.Data, InlineStr.Size),
							0);
					LLVMAddAttributeAtIndex(Gen.fn, LLVMAttributeFunctionIndex, Inline);
				}
				if(T->Function.Flags & SymbolFlag_NoReturn)
				{
					string NoretStr = STR_LIT("noreturn");
					LLVMAttributeRef NoReturn = LLVMCreateEnumAttribute(Gen.ctx,
							LLVMGetEnumAttributeKindForName(NoretStr.Data, NoretStr.Size),
							0);
					LLVMAddAttributeAtIndex(Gen.fn, LLVMAttributeFunctionIndex, NoReturn);
				}

				Gen.map.Clear();
				Gen.fn = NULL;
			}
		}

		if((Info->Flags & CF_DebugInfo) && FIdx + 1 != M->Files.Count)
		{
			char *FileName = NULL;
			char *FileDirectory = NULL;
			GetNameAndDirectory(&FileName, &FileDirectory, M->Files[FIdx+1]->Name);
			Gen.f_dbg = LLVMDIBuilderCreateFile(Gen.dbg,
					FileName, VStrLen(FileName),
					FileDirectory, VStrLen(FileDirectory));
		}
	}


	DEBUG_RUN(LLVMDIBuilderFinalize(Gen.dbg);)

	RunOptimizationPasses(&Gen, Machine.Target, Info->Optimization, Info->Flags);
	RCEmitFile(Machine.Target, Gen.mod, M->Name, OutputBC);


	DEBUG_RUN(LLVMDisposeDIBuilder(Gen.dbg);)

	LLVMDisposeBuilder(Gen.bld);
	DEBUG_RUN(Gen.LLVMDebugTypeMap.Free();)
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
#if 0
	LLVMInitializeX86TargetInfo();
	LLVMInitializeX86Target();
	LLVMInitializeX86TargetMC();
	LLVMInitializeX86AsmParser();
	LLVMInitializeX86AsmPrinter();
	LLVMInitializeX86Disassembler();

	LLVMInitializeAArch64TargetInfo();
	LLVMInitializeAArch64Target();
	LLVMInitializeAArch64TargetMC();
	LLVMInitializeAArch64AsmParser();
	LLVMInitializeAArch64AsmPrinter();
	LLVMInitializeAArch64Disassembler();

	LLVMInitializeWebAssemblyTargetInfo();
	LLVMInitializeWebAssemblyTarget();
	LLVMInitializeWebAssemblyTargetMC();
	LLVMInitializeWebAssemblyAsmParser();
	LLVMInitializeWebAssemblyAsmPrinter();
	LLVMInitializeWebAssemblyDisassembler();

#else
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
	LLVMInitializeAllDisassemblers();
#endif

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
		if(File->Module->Name == STR_LIT("base"))
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

	LLVMValueRef MainFn = LLVMAddFunction(gen->mod, "base.global_initializers", FnType);
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
	const std::unordered_map<void *, uint> *StoredGlobals;
};

void GenWorkerFn(void *Data)
{
	file_generate_info *Info = (file_generate_info *)Data;
	RCGenerateFile(Info->M, Info->OutputBC, Info->Info, *Info->StoredGlobals);
	LDEBUG("Done with module: %s", Info->M->Name.Data);
}

void RCGenerateCode(work_queue *Queue, slice<module*> Modules, slice<file*> Files, u32 CommandFlags, compile_info *Info, const std::unordered_map<void *, uint> &StoredGlobals)
{
	if((CommandFlags & CommandFlag_nothread) == 0)
	{
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
		FInfo->StoredGlobals = &StoredGlobals;
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

