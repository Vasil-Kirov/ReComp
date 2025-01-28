#include "ConstVal.h"
#include "Dynamic.h"
#include "DynamicLib.h"
#include "Errors.h"
#include "IR.h"
#include "Memory.h"
#include "VString.h"
#include "vlib.h"
#include "Semantics.h"
#include <Interpreter.h>
#include <Type.h>
#include <Log.h>
#include <cstddef>
#include <x64CodeWriter.h>
#include <math.h>

bool InterpreterTrace = false;
dynamic<DLIB> DLs = {};

void *InterpreterAllocateString(interpreter *VM, const string *String)
{
	void *Memory = ArenaAllocate(&VM->Arena, String->Size + 1, true);
	memcpy(Memory, String->Data, String->Size);
	((u8 *)Memory)[String->Size] = 0;
	return Memory;
}

void CopyRegisters(interpreter *VM, interpreter_scope NewScope)
{
	memcpy(NewScope.Registers, VM->Registers.Registers, VM->Registers.LastRegister * sizeof(value));
	NewScope.LastAdded = VM->Registers.LastAdded;
	NewScope.LastRegister = VM->Registers.LastRegister;
}

#include <dyncall.h>

char GetSigChar(const type *T, int *NumberOfElems, DCaggr **ExtraArg, dynamic<DCaggr*> AggrToFree)
{
	char Sig = 0;
	switch(T->Kind)
	{
		case TypeKind_Basic:
		{
			switch (T->Basic.Kind)
			{
				case Basic_bool:
				{
					Sig = DC_SIGCHAR_BOOL;
				} break;
				case Basic_u8:
				{
					Sig = DC_SIGCHAR_UCHAR;
				} break;
				case Basic_u16:
				{
					Sig = DC_SIGCHAR_USHORT;
				} break;
				case Basic_u32:
				{
					Sig = DC_SIGCHAR_UINT;
				} break;
				case Basic_u64:
				{
					Sig = DC_SIGCHAR_ULONGLONG;
				} break;
				case Basic_i8:
				{
					Sig = DC_SIGCHAR_CHAR;
				} break;
				case Basic_i16:
				{
					Sig = DC_SIGCHAR_SHORT;
				} break;
				case Basic_i32:
				{
					Sig = DC_SIGCHAR_INT;
				} break;
				case Basic_i64:
				{
					Sig = DC_SIGCHAR_LONGLONG;
				} break;
				case Basic_f32:
				{
					Sig = DC_SIGCHAR_FLOAT;
				} break;
				case Basic_f64:
				{
					Sig = DC_SIGCHAR_DOUBLE;
				} break;
				case Basic_type:
				case Basic_int:
				{
					int RegisterSize = GetRegisterTypeSize() / 8;
					switch(RegisterSize)
					{
						case 8: Sig = DC_SIGCHAR_LONGLONG; break;
						case 4: Sig = DC_SIGCHAR_INT; break;
						case 2: Sig = DC_SIGCHAR_SHORT; break;
						default: unreachable;
					}
				} break;
				case Basic_uint:
				{
					int RegisterSize = GetRegisterTypeSize() / 8;
					switch(RegisterSize)
					{
						case 8: Sig = DC_SIGCHAR_ULONGLONG; break;
						case 4: Sig = DC_SIGCHAR_UINT; break;
						case 2: Sig = DC_SIGCHAR_USHORT; break;
						default: unreachable;
					}
				} break;
				case Basic_string:
				{
					Sig = DC_SIGCHAR_AGGREGATE;
					*ExtraArg = MakeAggr(Basic_string, AggrToFree);
				} break;
				case Basic_UntypedFloat:
				case Basic_UntypedInteger:
				case Basic_auto:
				case Basic_module:
				Assert(false);
			}
		} break;
		case TypeKind_Array:
		{
			if(NumberOfElems)
			{
				*NumberOfElems = T->Array.MemberCount;
				Sig = GetSigChar(GetType(T->Array.Type), NULL, ExtraArg, AggrToFree);
			}
			else
			{
				Sig = DC_SIGCHAR_POINTER;
			}
		} break;
		case TypeKind_Pointer:
		{
			Sig = DC_SIGCHAR_POINTER;
		} break;
		case TypeKind_Enum:
		{
			Sig = GetSigChar(GetType(T->Enum.Type), NumberOfElems, ExtraArg, AggrToFree);
		} break;
		case TypeKind_Slice:
		{
			*ExtraArg = MakeAggr(T, AggrToFree);
			Sig = DC_SIGCHAR_AGGREGATE;
		} break;
		case TypeKind_Struct:
		{
			*ExtraArg = MakeAggr(T, AggrToFree);
			Sig = DC_SIGCHAR_AGGREGATE;
		} break;
		default: unreachable;
	}
	return Sig;
}

DCaggr *MakeAggr(const type *T, dynamic<DCaggr*> AggrToFree)
{
	DCaggr *Aggr = dcNewAggr(T->Struct.Members.Count, GetTypeSize(T));

	if(T->Kind == TypeKind_Struct)
	{
		ForArray(Idx, T->Struct.Members)
		{
			auto it = T->Struct.Members[Idx];
			auto MemT = GetType(it.Type);
			DCaggr *ExtraArg = NULL;
			int NumberOfElems = 1;
			char Sig = GetSigChar(MemT, &NumberOfElems, &ExtraArg, AggrToFree);

			dcAggrField(Aggr, Sig, GetStructMemberOffset(T, Idx), NumberOfElems, ExtraArg);
		}
	}
	else if(T->Kind == TypeKind_Slice)
	{
		char First = GetSigChar(GetType(Basic_int), NULL, NULL, AggrToFree);
		char Second = DC_SIGCHAR_POINTER;
		dcAggrField(Aggr, First, 0, 0);
		dcAggrField(Aggr, Second, GetRegisterTypeSize()/8, 0);
	}
	else if(IsString(T))
	{
		char First = GetSigChar(GetType(Basic_int), NULL, NULL, AggrToFree);
		char Second = DC_SIGCHAR_STRING;
		dcAggrField(Aggr, First, 0, 0);
		dcAggrField(Aggr, Second, GetRegisterTypeSize()/8, 0);
	}

	dcCloseAggr(Aggr);
	AggrToFree.Push(Aggr);
	return Aggr;
}

DCaggr *MakeAggr(u32 TIdx, dynamic<DCaggr*> AggrToFree)
{
	const type *T = GetType(TIdx);
	return MakeAggr(T, AggrToFree);
}

u64 PerformForeignFunctionCall(interpreter *VM, call_info *Info, value *Operand)
{
	Assert(Operand->ptr);
	const type *FnT = GetType(Operand->Type);
	if(FnT->Kind == TypeKind_Pointer)
		FnT = GetType(FnT->Pointer.Pointed);

	Assert(FnT->Kind == TypeKind_Function);
	
	dynamic<DCaggr*> Aggrs = {};

	DCCallVM *dc = dcNewCallVM(MB(8));
	if(Info->Args.Count >= FnT->Function.ArgCount)
	{
		dcMode(dc, DC_CALL_C_ELLIPSIS_VARARGS);
	}
	else
	{
		dcMode(dc, DC_CALL_C_DEFAULT);
	}
	dcReset(dc);

	if(FnT->Function.Returns.Count == 1)
	{
		const type *Ret = GetType(FnT->Function.Returns[0]);
		if(IsString(Ret))
		{
			DCaggr *Aggr = MakeAggr(Ret, Aggrs);
			dcBeginCallAggr(dc, Aggr);
		}
		else if(Ret->Kind == TypeKind_Slice)
		{
			DCaggr *Aggr = MakeAggr(Ret, Aggrs);
			dcBeginCallAggr(dc, Aggr);
		}
		else if(Ret->Kind == TypeKind_Struct)
		{
			DCaggr *Aggr = MakeAggr(Ret, Aggrs);
			dcBeginCallAggr(dc, Aggr);
		}
	}

	for(int i = 0; i < Info->Args.Count; ++i)
	{
		const value *Arg = VM->Registers.GetValue(Info->Args[i]);
		u32 TIdx = INVALID_TYPE;
		if(i >= FnT->Function.ArgCount)
		{
			TIdx = Arg->Type;
		}
		else
		{
			TIdx = FnT->Function.Args[i];
		}

		const type *T = GetType(TIdx);
		if(T->Kind == TypeKind_Enum)
			T = GetType(T->Enum.Type);

		switch(T->Kind)
		{
			case TypeKind_Basic:
			{
				switch (T->Basic.Kind)
				{
					case Basic_bool:
					{
						dcArgBool(dc, Arg->u64);
					} break;
					case Basic_u8:
					{
						dcArgChar(dc, Arg->u64);
					} break;
					case Basic_u16:
					{
						dcArgShort(dc, Arg->u64);
					} break;
					case Basic_u32:
					{
						// @TODO: might be wrong
						dcArgInt(dc, Arg->u64);
					} break;
					case Basic_u64:
					{
						dcArgChar(dc, Arg->u64);
					} break;
					case Basic_i8:
					{
						dcArgChar(dc, Arg->i64);
					} break;
					case Basic_i16:
					{
						dcArgShort(dc, Arg->i64);
					} break;
					case Basic_i32:
					{
						// @TODO: might be wrong
						dcArgInt(dc, Arg->i64);
					} break;
					case Basic_i64:
					{
						dcArgLongLong(dc, Arg->i64);
					} break;
					case Basic_f32:
					{
						dcArgFloat(dc, Arg->f32);
					} break;
					case Basic_f64:
					{
						dcArgFloat(dc, Arg->f64);
					} break;
					case Basic_type:
					case Basic_int:
					{
						int RegisterSize = GetRegisterTypeSize() / 8;
						switch(RegisterSize)
						{
							case 8: dcArgLongLong(dc, Arg->i64); break;
							case 4: dcArgInt(dc, Arg->i64); break;
							case 2: dcArgShort(dc, Arg->i64); break;
							default: unreachable;
						}
					} break;
					case Basic_uint:
					{
						int RegisterSize = GetRegisterTypeSize() / 8;
						switch(RegisterSize)
						{
							case 8: dcArgLongLong(dc, Arg->u64); break;
							case 4: dcArgInt(dc, Arg->u64); break;
							case 2: dcArgShort(dc, Arg->u64); break;
							default: unreachable;
						}
					} break;
					case Basic_string:
					{
						DCaggr *StrT = MakeAggr(Basic_string, Aggrs);
						dcArgAggr(dc, StrT, Arg->ptr);
					} break;
					case Basic_UntypedFloat:
					case Basic_UntypedInteger:
					case Basic_auto:
					case Basic_module:
					Assert(false);
				}
			} break;
			case TypeKind_Slice:
			case TypeKind_Struct:
			{
				DCaggr *Aggr = MakeAggr(T, Aggrs);
				dcArgAggr(dc, Aggr, Arg->ptr);
			} break;
			case TypeKind_Pointer:
			case TypeKind_Array:
			{
				dcArgPointer(dc, Arg->ptr);
			} break;
			default:
			{
				LDEBUG("%s", GetTypeName(T));
				unreachable;
			}
		}
	}

	u64 Result = 0;
	if(FnT->Function.Returns.Count == 0)
	{
		dcCallVoid(dc, Operand->ptr);
	}
	else if(FnT->Function.Returns.Count == 1)
	{
		const type *Ret = GetType(FnT->Function.Returns[0]);
		switch(Ret->Kind)
		{
			case TypeKind_Basic:
			{
				if(Ret->Basic.Kind == Basic_bool)
				{
					Result = dcCallBool(dc, Operand->ptr);
				}
				else if(Ret->Basic.Kind == Basic_f32)
				{
					Result = dcCallFloat(dc, Operand->ptr);
				}
				else if(Ret->Basic.Kind == Basic_f64)
				{
					Result = dcCallDouble(dc, Operand->ptr);
				}
				else if(Ret->Basic.Kind == Basic_string)
				{
					void *Ptr = VM->Stack.Peek().Allocate(GetTypeSize(Basic_string));
					DCaggr *Aggr = MakeAggr(Ret, Aggrs);
					dcCallAggr(dc, Operand->ptr, Aggr, Ptr);
					Result = (u64)Ptr;
				}
				else
				{
					uint Size = GetTypeSize(Ret);
					switch(Size)
					{
						case 1:
						Result = dcCallChar(dc, Operand->ptr); break;
						case 2:
						Result = dcCallShort(dc, Operand->ptr); break;
						case 4:
						// @TODO: might be wrong
						Result = dcCallInt(dc, Operand->ptr); break;
						case 8:
						Result = dcCallLongLong(dc, Operand->ptr); break;
					}
				}
			} break;
			case TypeKind_Struct:
			case TypeKind_Slice:
			{
				void *Ptr = VM->Stack.Peek().Allocate(GetTypeSize(Ret));
				DCaggr *Aggr = MakeAggr(Ret, Aggrs);
				dcCallAggr(dc, Operand->ptr, Aggr, Ptr);
				Result = (u64)Ptr;
			} break;
			case TypeKind_Pointer:
			{
				Result = (u64)dcCallPointer(dc, Operand->ptr);
			} break;
			default: unreachable;
		}
	}
	else
	{
		// @TODO: Error out
		Assert(false);
	}

	For(Aggrs)
	{
		dcFreeAggr(*it);
	}
	Aggrs.Free();
	dcFree(dc);
	return Result;
#if 0
	typedef u64 (*inter_fn)(void *, value *);

	value *Args = (value *)VAlloc(Info->Args.Count * sizeof(value));
	ForArray(Idx, Info->Args)
	{
		Args[Idx] = *VM->Registers.GetValue(Info->Args[Idx]);
	}
	// @LEAK?
	assembler Asm = MakeAssembler(KB(1));
	//  Windows:
	//  rcx = operand
	//  rdx = value *args
	//  Linux:
	//  rdi = operand
	//  rsi = value *args
	// 
	//  
	//  push rbp
	//  mov rbp, rsp
	//  sub rsp, 32
	//
	//  push rcx
	//  push rdx
	//  
	//  mov rax, [rsp]
	//  mov rax, [rax + offsetof(
	//
	//  pop rdx
	//  ..
	//
	//	xor rax, rax
	//	call rcx
	//  mov rsp, rbp
	//  pop rbp
	//  ret
	//
	//
	// 
	//

#if _WIN32
	operand ConventionRegisters[] = {RegisterOperand(reg_c), RegisterOperand(reg_d), RegisterOperand(reg_r8),
		RegisterOperand(reg_r9)};
#elif CM_LINUX
	operand ConventionRegisters[] = {RegisterOperand(reg_di), RegisterOperand(reg_si), RegisterOperand(reg_d),
		RegisterOperand(reg_c), RegisterOperand(reg_r8), RegisterOperand(reg_r9)};

	Asm.Push(RegisterOperand(reg_bp));
	Asm.Mov64(RegisterOperand(reg_bp), RegisterOperand(reg_sp));
#else
#error "Unknown calling convention"
#endif
	//operand ConventionFloatRegisters[] = {RegisterOperand(reg_xmm0), RegisterOperand(reg_xmm1),
		//RegisterOperand(reg_xmm2), RegisterOperand(reg_xmm3)};


	//Asm.Mov64(RegisterOperand(reg_bp), RegisterOperand(reg_sp));


#if _WIN32
	int StackAllocated = (Info->Args.Count * 16) + 40;
	Asm.Sub(RegisterOperand(reg_sp), ConstantOperand(StackAllocated));
	Asm.Mov64(RegisterOperand(reg_r11), RegisterOperand(reg_c));
	Asm.Mov64(RegisterOperand(reg_r10), RegisterOperand(reg_d));
#elif CM_LINUX
	Asm.Mov64(RegisterOperand(reg_r11), RegisterOperand(reg_di));
	Asm.Mov64(RegisterOperand(reg_r10), RegisterOperand(reg_si));
#else
#error "Unknown calling convention"
#endif

	//const type *FnType = GetType(Operand->Type & ~(1 << 31));

	uint CurrentInt = 0;
	//uint CurrentFloat = 0;

	int Idx = 0;
	for(; Idx < Info->Args.Count && CurrentInt < ARR_LEN(ConventionRegisters); ++Idx)
	{
		const type *Type = GetType(Args[Idx].Type);
		//Asm.Peek(RegisterOperand(reg_a));
		Asm.Lea64(RegisterOperand(reg_a), OffsetOperand(reg_r10, Idx * sizeof(value)));
		switch(Type->Kind)
		{
			case TypeKind_Basic:
			case TypeKind_Pointer:
			{
				if(CurrentInt < ARR_LEN(ConventionRegisters))
				{
					Asm.Mov64(ConventionRegisters[CurrentInt++], OffsetOperand(reg_a, offsetof(value, u64)));
				}
				else
				{
					Assert(false);
					//Asm.Mov64(RegisterOperand(reg_a), OffsetOperand(reg_a, offsetof(value, u64)));
					//Asm.Push(RegisterOperand(reg_a));
				}
			} break;

			default: unreachable;
		}
	}

	if(Idx < Info->Args.Count)
	{
#if _WIN32
		for(int i = Info->Args.Count - 1; i >= Idx; --i)
		{

			Asm.Lea64(RegisterOperand(reg_a), OffsetOperand(reg_r10, i * sizeof(value)));
			Asm.Mov64(RegisterOperand(reg_a), OffsetOperand(reg_a, offsetof(value, u64)));
			Asm.Mov64(OffsetOperand(reg_sp, i * 8), RegisterOperand(reg_a));
		}
#elif CM_LINUX
		for(int i = Info->Args.Count - 1; i >= Idx; --i)
		{
			Asm.Lea64(RegisterOperand(reg_a), OffsetOperand(reg_r10, i * sizeof(value)));
			Asm.Mov64(RegisterOperand(reg_a), OffsetOperand(reg_a, offsetof(value, u64)));
			Asm.Push(RegisterOperand(reg_a));
		}
#else
#error IMPLEMENT
#endif
	}

	Asm.Call(RegisterOperand(reg_r11));

#if _WIN32
	Asm.Add(RegisterOperand(reg_sp), ConstantOperand(StackAllocated));

#elif CM_LINUX
	Asm.Mov64(RegisterOperand(reg_sp), RegisterOperand(reg_bp));
	Asm.Pop(RegisterOperand(reg_bp));
#endif

	Asm.Ret();

#if 0
	printf("Call to %%%d:\n", Info->Operand);
	for(int Idx = 0; Idx < Asm.CurrentOffset; ++Idx)
	{
		printf("%02x ", ((u8 *)Asm.Code)[Idx]);
	}
	putchar('\n');
#endif

	inter_fn ToCall = (inter_fn)Asm.Code;

	u64 Result = ToCall(Operand->ptr, Args);
	FreeVirtualMemory(Asm.Code);
	return Result;
#endif
}

void Store(interpreter *VM, value *Ptr, value *Value, u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	int TypeSize = GetTypeSize(Type);
	if(IsLoadableType(Type))
	{
		memcpy(Ptr->ptr, &Value->u64, TypeSize);
	}
	else
	{
		memcpy(Ptr->ptr, Value->ptr, TypeSize);
	}
	//if(Type->Kind == TypeKind_Basic)
	//{
	//	int TypeSize = GetTypeSize(Type);
	//	if(Type->Basic.Kind == Basic_cstring || Type->Basic.Kind == Basic_string)
	//		*(void **)Ptr->ptr = Value->ptr;
	//	else
	//		memcpy(Ptr->ptr, &Value->u64, TypeSize);
	//}
	//else if(Type->Kind == TypeKind_Pointer)
	//{
	//	*(void **)Ptr->ptr = Value->ptr;
	//}
	//else
	//{
	//	memcpy(Ptr->ptr, Value->ptr, GetTypeSize(Type));
	//}

}

void *IndexVM(interpreter *VM, u32 Left, u32 Right, u32 TypeIdx, u32 *OutType, b32 UseConstant = false)
{
	void *Result = NULL;
	value *Operand = VM->Registers.GetValue(Left);
	const type *Type = GetType(TypeIdx);
	switch(Type->Kind)
	{
		case TypeKind_Array:
		{
			int TypeSize = GetTypeSize(Type->Array.Type);
			*OutType = Type->Array.Type;
			if(UseConstant)
			{
				Result = ((u8 *)Operand->ptr) + (TypeSize * Right);
			}
			else
			{
				value *Index = VM->Registers.GetValue(Right);
				Result = ((u8 *)Operand->ptr) + (TypeSize * Index->u64);
			}
		} break;
		case TypeKind_Struct:
		{
			int Offset = GetStructMemberOffset(Type, Right);
			*OutType = Type->Struct.Members[Right].Type;
			Result = ((u8 *)Operand->ptr) + Offset;
		} break;
		case TypeKind_Pointer:
		{
			int TypeSize = GetTypeSize(Type->Pointer.Pointed);
			*OutType = Type->Pointer.Pointed;

			if(UseConstant)
			{
				Result = ((u8 *)Operand->ptr) + (TypeSize * Right);
			}
			else
			{
				value *Index = VM->Registers.GetValue(Right);
				const type *IdxT = GetType(Index->Type);
				if(HasBasicFlag(IdxT, BasicFlag_Unsigned))
					Result = ((u8 *)Operand->ptr) + (TypeSize * Index->u64);
				else
					Result = ((u8 *)Operand->ptr) + (TypeSize * Index->i64);
			}
		} break;
		case TypeKind_Basic:
		{
			if(HasBasicFlag(Type, BasicFlag_String))
			{
				int Offset = Right * GetRegisterTypeSize() / 8;
				Result = ((u8 *)Operand->ptr) + Offset;

				if(Right == 0)
					*OutType = GetPointerTo(Basic_u8);
				else
					*OutType = Basic_int;
			}
			else
				unreachable;
		} break;
		case TypeKind_Slice:
		{
			int Offset = Right * GetRegisterTypeSize() / 8;
			Result = ((u8 *)Operand->ptr) + Offset;

			if(Right == 1)
				*OutType = GetPointerTo(INVALID_TYPE);
			else
				*OutType = Basic_int;
		} break;
		default: unreachable;
	}

	return Result;
}

basic_block FindBlockByID(slice<basic_block> Blocks, int ID)
{
	ForArray(Idx, Blocks)
	{
		if(Blocks[Idx].ID == ID)
			return Blocks[Idx];
	}
	unreachable;
}

#define ALLOC(SIZE) Globals ? ArenaAllocate(&VM->Arena, SIZE) : VM->Stack.Peek().Allocate(SIZE)

void DoAllocationForInstructions(interpreter *VM, slice<instruction> Instructions, b32 Globals)
{
	For(Instructions)
	{
		switch(it->Op)
		{
			case OP_ALLOC:
			{
				const type *Type = GetType(it->Type);
				uint Size = GetTypeSize(Type);
				value Value;
				Value.Type = GetPointerTo(it->Type);
				Value.ptr = ALLOC(Size);
				VM->Registers.AddValue(it->Result, Value);
			} break;
			case OP_LOAD:
			{
				const type *T = GetType(it->Type);
				switch(T->Kind)
				{
					case TypeKind_Basic:
					{
						if(T->Basic.Kind != Basic_string)
							break;

						uint Size = sizeof(size_t) * 2;
						value Value;
						Value.Type = Basic_string;
						Value.ptr = ALLOC(Size);
						VM->Registers.AddValue(it->Result, Value);
					} break;
					case TypeKind_Vector:
					case TypeKind_Struct:
					case TypeKind_Array:
					{
						uint Size = GetTypeSize(it->Type);
						value Value;
						Value.Type = it->Type;
						Value.ptr = ALLOC(Size);
						VM->Registers.AddValue(it->Result, Value);
					} break;
					case TypeKind_Slice:
					{
						uint Size = sizeof(size_t) * 2;
						value Value;
						Value.Type = it->Type;
						Value.ptr = ALLOC(Size);
						VM->Registers.AddValue(it->Result, Value);
					} break;
					default: break;
				}
			} break;
			case OP_ALLOCGLOBAL:
			{
				int Size = GetTypeSize(it->Type);
				value Result;
				Result.ptr = ArenaAllocate(&VM->Arena, Size * it->BigRegister);
				Result.Type = GetArrayType(it->Type, it->BigRegister);
				VM->Registers.AddValue(it->Result, Result);
			} break;
			case OP_CONST:
			{
				const type *Type = GetType(it->Type);
				if(IsString(Type))
				{
					const_value *Val = (const_value *)it->BigRegister;
					void *Memory = ALLOC(sizeof(size_t)*2);

					*(size_t *)Memory = GetUTF8Count(Val->String.Data);

					void *StringData = InterpreterAllocateString(VM, Val->String.Data);
					void **MemoryLocation = (void **)Memory + 1;
					*MemoryLocation = StringData;

					value Value;
					Value.Type = it->Type;
					Value.ptr  = Memory;
					VM->Registers.AddValue(it->Result, Value);
				}
				else if(IsCString(Type))
				{
					const_value *Val = (const_value *)it->BigRegister;

					value Value;
					Value.Type = it->Type;
					Value.ptr  = InterpreterAllocateString(VM, Val->String.Data);;

					VM->Registers.AddValue(it->Result, Value);
				}
			} break;
			default: break;
		}
	}
}

void DoAllocationForBlocks(interpreter *VM, slice<basic_block> Blocks, b32 Globals)
{
	ForArray(Idx, Blocks)
	{
		basic_block& Block = Blocks.Data[Idx];
		DoAllocationForInstructions(VM, SliceFromArray(Block.Code), Globals);
	}
}

interpret_result Run(interpreter *VM, slice<basic_block> OptionalBlocks, slice<value> OptionalArgs)
{
	ForArray(InstrIdx, VM->Executing->Code)
	{
		instruction I = VM->Executing->Code[InstrIdx];
		switch(I.Op)
		{
			case OP_NOP:
			{
			} break;
			case OP_GLOBAL:
			{
				// 	%0 = OP_INDEX  arr
				// 	%1 = OP_GLOBAL fn_foo
				// 	%0 = OP_STORE  %1
				const symbol *s = (const symbol *)I.Ptr;

				value *v = VM->Globals.GetValue(s->Register);
				Assert(v->Flags & value_flag::Global);
				value NewVal = *v;
				Assert(NewVal.ptr);
				VM->Registers.AddValue(I.Result, NewVal);
				VM->Registers.Links.Push(global_link{.GlobalRegister = s->Register, .LocalRegister = I.Result});
			} break;
			case OP_ZEROUT:
			{
				value *Value = VM->Registers.GetValue(I.Right);
				memset(Value->ptr, 0, GetTypeSize(I.Type));
			} break;
			case OP_PTRDIFF:
			{
				auto LHS = VM->Registers.GetValue(I.Left);
				auto RHS = VM->Registers.GetValue(I.Right);
				value Value = {};
				Value.Type = I.Type;
				Value.ptr = (void *)((u8 *)LHS->ptr - (u8 *)RHS->ptr);
				VM->Registers.AddValue(I.Result, Value);
			} break;
			case OP_CONSTINT:
			{
				u64 Val = I.BigRegister;
				value Value = {};
				Value.Type = I.Type;
				Value.u64 = Val;
				VM->Registers.AddValue(I.Result, Value);
			} break;
			case OP_ENUM_ACCESS:
			const_value V;
			{
				const type *T = GetType(I.Type);
				Assert(T->Kind == TypeKind_Enum);
				V = T->Enum.Members[I.Right].Value;
				if(T->Enum.Members[I.Right].Evaluate.IsValid())
				{
					return { INTERPRET_RUNTIME_ERROR };
				}
				I.Op = OP_CONST;
				I.BigRegister = (u64)&V;
			}
			// fallthrough
			case OP_CONST:
			{
				b32 NoAdd = false;
				value VMValue = {};
				VMValue.Type = I.Type;
				const_value *Val = (const_value *)I.BigRegister;
				const type *Type = GetType(I.Type);
				if(Type->Kind == TypeKind_Enum)
					Type = GetType(Type->Enum.Type);

				if(Type->Kind == TypeKind_Basic)
				{
					if(Type->Basic.Flags & BasicFlag_Float)
					{
						switch(Type->Basic.Kind)
						{
							case Basic_f64:
							{
								switch(Val->Type)
								{
									case const_type::Integer:
									{
										VMValue.f64 = Val->Int.Signed;
									} break;
									case const_type::Float:
									{
										VMValue.f64 = Val->Float;
									} break;
									default: unreachable;
								}
							} break;
							case Basic_f32:
							{
								switch(Val->Type)
								{
									case const_type::Integer:
									{
										VMValue.f32 = Val->Int.Signed;
									} break;
									case const_type::Float:
									{
										VMValue.f32 = Val->Float;
									} break;
									default: unreachable;
								}
							} break;
							default: unreachable;
						}
					}
					else if(Type->Basic.Flags & BasicFlag_Integer)
					{
						switch(Val->Type)
						{
							case const_type::Integer:
							{
								if(Val->Int.IsSigned)
									VMValue.i64 = Val->Int.Signed;
								else
									VMValue.u64 = Val->Int.Unsigned;
							} break;
							case const_type::Float:
							{
								if(Val->Int.IsSigned)
									VMValue.i64 = Val->Float;
								else
									VMValue.u64 = Val->Float;
							} break;
							default: unreachable;
						}
					}
					else if(Type->Basic.Flags & BasicFlag_String)
					{
						NoAdd = true;
						//@Note: Should be done before running the function
						//void *Memory = VM->Stack.Peek().Allocate(sizeof(size_t)*2);

						//*(size_t *)Memory = GetUTF8Count(Val->String.Data);

						//void *StringData = InterpreterAllocateString(VM, Val->String.Data);
						//void **MemoryLocation = (void **)Memory + 1;
						//*MemoryLocation = StringData;

						//VMValue.ptr = Memory;
					}
					else if(Type->Basic.Flags & BasicFlag_Boolean)
					{
						if(Val->Type == const_type::Integer)
						{
							VMValue.u64 = Val->Int.Unsigned ? 1 : 0;
						}
						else
						{
							Assert(Val->Type == const_type::Float);
							VMValue.u64 = Val->Float ? 1 : 0;
						}
					}
					else if(HasBasicFlag(Type, BasicFlag_TypeID))
					{

						if(Val->Type == const_type::Integer)
						{
							VMValue.u64 = Val->Int.Unsigned;
						}
						else
						{
							Assert(Val->Type == const_type::Float);
							VMValue.f64 = Val->Float;
						}
					}
					else
					{
						LDEBUG("%s", GetTypeName(Type));
						unreachable;
					}
				}
				else if(Type->Kind == TypeKind_Pointer)
				{
					if(IsCString(Type))
					{
						switch(Val->Type)
						{
							case const_type::Integer:
							{
								if(Val->Int.IsSigned)
									VMValue.ptr = (void *)Val->Int.Signed;
								else
									VMValue.ptr = (void *)Val->Int.Unsigned;
							} break;
							case const_type::Aggr:
							case const_type::Float:
							{
								unreachable;
							} break;
							case const_type::String:
							{
								NoAdd = true;
								// @Note: Done before execution
								// VMValue.ptr = InterpreterAllocateString(VM, Val->String.Data);
							} break;
						}
					}
					else
						VMValue.ptr = (void *)Val->Int.Unsigned;
				}
				else
				{
					Assert(Val->Type == const_type::Aggr);
					VMValue.ptr = Val->Struct.Ptr;
				}

				if(!NoAdd)
					VM->Registers.AddValue(I.Result, VMValue);
			} break;
			case OP_UNREACHABLE:
			{
				LERROR("REACHED UNREACHABLE STATEMENT IN INTERPRETER!!!");
				unreachable;
			} break;
			case OP_ALLOC:
			{
				// @Note: Done before execution
				//const type *Type = GetType(I.Type);
				//uint Size = GetTypeSize(Type);
				//value Value = {};
				//Value.Type = GetPointerTo(I.Type);
				//Value.ptr = VM->Stack.Peek().Allocate(Size);
				//VM->Registers.AddValue(I.Result, Value);
			} break;
			case OP_LOAD:
			{
#define LOAD_T(T, size) case Basic_##T: \
						{ \
							Result.u64 = *(u##size *)Value->ptr; \
						} break

				b32 NoResult = false;
				value *Value = VM->Registers.GetValue(I.Right);
				//Value->Type = I.Type;
				value Result = {};
				Result.Type = I.Type;

				const type *Type = GetType(I.Type);
				if(Type->Kind == TypeKind_Enum)
				{
					Type = GetType(Type->Enum.Type);
				}

				switch(Type->Kind)
				{
					case TypeKind_Basic:
					{
						switch(Type->Basic.Kind)
						{
							LOAD_T(bool, 8);

							LOAD_T(u8, 8);
							LOAD_T(u16, 16);
							LOAD_T(u32, 32);
							LOAD_T(u64, 64);

							LOAD_T(i8, 8);
							LOAD_T(i16, 16);
							LOAD_T(i32, 32);
							LOAD_T(i64, 64);

							LOAD_T(f32, 32);
							LOAD_T(f64, 64);

							LOAD_T(int, 64);
							LOAD_T(uint, 64);

							LOAD_T(type, 64);

							case Basic_string:
							{
								NoResult = true;
								// @Note: Done before execution
								//Result.ptr = VM->Stack.Peek().Allocate(Size);
								uint Size = sizeof(size_t) * 2;
								value *Result = VM->Registers.GetValue(I.Result);
								memcpy(Result->ptr, Value->ptr, Size);
							} break;
							default: unreachable;
						}
					} break;
					case TypeKind_Pointer:
					{
						Result.ptr = *(void **)Value->ptr;
					} break;
					case TypeKind_Struct:
					case TypeKind_Array:
					{
						NoResult = true;
						// @Note: Done before execution
						uint Size = GetTypeSize(Type);
						value *Result = VM->Registers.GetValue(I.Result);
						memcpy(Result->ptr, Value->ptr, Size);
					} break;
					case TypeKind_Slice:
					{
						NoResult = true;
						// @Note: Done before execution
						uint Size = sizeof(size_t) * 2;
						value *Result = VM->Registers.GetValue(I.Result);
						memcpy(Result->ptr, Value->ptr, Size);
					} break;
					case TypeKind_Vector:
					{
						NoResult = true;
						uint Size = GetTypeSize(Type);
						value *Result = VM->Registers.GetValue(I.Result);
						memcpy(Result->ptr, Value->ptr, Size);
					} break;
					default: {
						LDEBUG("%s", GetTypeName(Type));

						unreachable;
					} break;
				}
				if(!NoResult)
					VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_TYPEINFO:
			{
				value *TypeTable = VM->Globals.GetValue(I.Left);
				value *Idx = VM->Registers.GetValue(I.Right);
				void *Data = *(((void **)TypeTable->ptr) + 1);
				u8 *Result = ((u8 *)Data) + Idx->i64 * GetTypeSize(I.Type);
				value V = {};
				V.Type = GetPointerTo(I.Type);
				V.ptr = Result;
				VM->Registers.AddValue(I.Result, V);
			} break;
			case OP_SWITCHINT:
			{
				ir_switchint *Info = (ir_switchint *)I.BigRegister;
				value *Matcher = VM->Registers.GetValue(Info->Matcher);
				bool Found = false;
				ForArray(Idx, Info->Cases)
				{
					u32 Case = Info->Cases[Idx];
					value *Value = VM->Registers.GetValue(Info->OnValues[Idx]);
					if(Matcher->u64 == Value->u64)
					{
						if(!OptionalBlocks.IsValid())
							return { INTERPRET_RUNTIME_ERROR };

						Found = true;
						VM->Executing->Code = SliceFromArray(FindBlockByID(OptionalBlocks, Case).Code);
					}
				}
				if(!Found)
				{
					VM->Executing->Code = SliceFromArray(FindBlockByID(OptionalBlocks, Info->After).Code);
				}

				InstrIdx = -1;
			} break;
			case OP_STORE:
			{
				value *Left = VM->Registers.GetValue(I.Left);
				value *Right = VM->Registers.GetValue(I.Right);
				Store(VM, Left, Right, I.Type);
				if(VM->KeepTrackOfStoredGlobals && Right->Flags & value_flag::Global)
				{
					u32 GlobalRegister = -1;
					For(VM->Registers.Links)
					{
						if(it->LocalRegister == I.Right)
						{
							GlobalRegister = it->GlobalRegister;
						}
					}
					// @TODO: Investigate if this should be an if or an assert
					if(GlobalRegister != -1)
					{
						VM->StoredGlobals[Left->ptr] = GlobalRegister;
					}
				}
			} break;
			case OP_INDEX:
			{
				value Result = {};
				Result.ptr = IndexVM(VM, I.Left, I.Right, I.Type, &Result.Type);
				Result.Type = GetPointerTo(Result.Type);
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_RET:
			{
				if(I.Left == -1)
				{
					return { INTERPRET_OK, {} };
				}
				else
				{
					if(VM->IsCurrentFnRetInPtr)
					{
						value *RetPtr = &OptionalArgs.Data[0];
						value *RetVal = VM->Registers.GetValue(I.Left);
						Store(VM, RetPtr, RetVal, I.Type);
						return { INTERPRET_OK, *RetPtr };
					}
					else
					{
						return { INTERPRET_OK, *VM->Registers.GetValue(I.Left) };
					}
				}
			} break;
			case OP_CAST:
			{
				//u32 FromType = I.Right;
				u32 ToType = I.Type;
				value *Val = VM->Registers.GetValue(I.Left);
				value Result = {};
				Result.Type = ToType;
				// @TODO:?
				Result.ptr = Val->ptr;
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)I.BigRegister;
				value *Operand = VM->Registers.GetValue(CallInfo->Operand);
				if(Operand->ptr == NULL)
				{
					RaiseError(true, *VM->ErrorInfo.Peek(), "Compile time interpreter cannot find called function. If it's in a dynamic library, you can load dynamic libraries for compile time execution using the #load_dl and #load_system_dl directives.");
					return { INTERPRET_RUNTIME_ERROR };
				}

				if(((u64)Operand->ptr & (1ull << 63)) == 0)
				{
					u64 Result = PerformForeignFunctionCall(VM, CallInfo, Operand);
					if(I.Type != INVALID_TYPE)
					{
						value Value = {};
						Value.Type = I.Type;
						Value.u64 = Result;
						VM->Registers.AddValue(I.Result, Value);
					}
				}
				else
				{
					function *F = (function *)((u64)Operand->ptr & ~(1ull << 63));
					dynamic<value> Args = {};
					ForArray(Idx, CallInfo->Args)
					{
						Args.Push(*VM->Registers.GetValue(CallInfo->Args[Idx]));
					}
					
					code_chunk *Executing = VM->Executing;
					interpreter_scope CurrentScope = VM->Registers;
					interpreter_scope NewScope = {};

					//NewScope.Init(F->LastRegister, VM->StackAllocator.Push(F->LastRegister * sizeof(value)));

					VM->Registers = NewScope;

					interpret_result Result = InterpretFunction(VM, *F, SliceFromArray(Args));

					VM->Registers = CurrentScope;
					VM->Executing = Executing;

					VM->Registers.AddValue(I.Result, Result.Result);

					Args.Free();
					//VM->StackAllocator.Pop();
					//NewScope.Free();
					if(Result.Kind == INTERPRET_RUNTIME_ERROR)
						return Result;
				}
			} break;
			case OP_ALLOCGLOBAL:
			{
				// @Note: Done before execution
				//int Size = GetTypeSize(I.Type);
				//value Result = {};
				//Result.ptr = ArenaAllocate(&VM->Arena, Size * I.BigRegister);
				//Result.Type = GetArrayType(I.Type, I.BigRegister);
				//VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_ARG:
			{
				if(!OptionalArgs.IsValid() || VM->CurrentFn.Name == NULL)
				{
					Assert(false);
					return { INTERPRET_RUNTIME_ERROR };
				}
				int Index = I.BigRegister;
				arg_location Loc;
				if(Index < VM->CurrentFn.Args.Count)
				{
					Loc = VM->CurrentFn.Args[Index];
				}
				else
				{
					arg_location Last = VM->CurrentFn.Args[VM->CurrentFn.Args.Count - 1];
					Loc = arg_location{.Load = LoadAs_Normal, .Start = Last.Start + Last.Count, .Count = 1};
				}

				switch(Loc.Load)
				{
					case LoadAs_Normal:
					{
						Assert(Loc.Count == 1);
						VM->Registers.AddValue(I.Result, OptionalArgs[Loc.Start]);
					} break;
					case LoadAs_Int:
					{
						Assert(Loc.Count == 1);
						Assert(GetType(I.Type)->Kind == TypeKind_Struct);
						int TypeSize = GetTypeSize(I.Type);
						void *Memory = VM->Stack.Peek().Allocate(TypeSize);
						value Value = {};
						Value.Type = I.Type;
						Value.ptr = Memory;
						// @TODO: Undefined behavior
						Store(VM, &Value, &OptionalArgs.Data[Loc.Start], I.Type);
						VM->Registers.AddValue(I.Result, Value);
					} break;
					case LoadAs_MultiInt:
					{
						Assert(Loc.Count == 2);
						Assert(GetType(I.Type)->Kind == TypeKind_Struct);
						int TypeSize = GetTypeSize(I.Type);
						u8 *Memory = (u8 *)VM->Stack.Peek().Allocate(TypeSize);
						memcpy(Memory, &OptionalArgs.Data[Loc.Start].u64, 8);
						switch(TypeSize-8)
						{
							case 8:
							{
								memcpy(Memory+8, &OptionalArgs.Data[Loc.Start+1].u64, 8);
							} break;
							case 4:
							{
								memcpy(Memory+8, &OptionalArgs.Data[Loc.Start+1].u64, 4);
							} break;
							case 2:
							{
								memcpy(Memory+8, &OptionalArgs.Data[Loc.Start+1].u64, 2);
							} break;
							default :unreachable;
						}

						value Value = {};
						Value.Type = I.Type;
						Value.ptr = Memory;
						VM->Registers.AddValue(I.Result, Value);
					} break;
					case LoadAs_Floats:
					{
						const type *T = GetType(I.Type);
						Assert(T->Kind == TypeKind_Struct);
						int TypeSize = GetTypeSize(I.Type);
						void *Memory = VM->Stack.Peek().Allocate(TypeSize);
						u8 *Ptr = (u8 *)Memory;
						int Passed = 0;
						ForArray(Idx, T->Struct.Members)
						{
							const type *MemT = GetType(OptionalArgs.Data[Loc.Start + Passed++].Type);
							int MemSize = GetTypeSize(MemT);
							if(MemT->Kind == TypeKind_Vector)
							{
								memcpy(Ptr, OptionalArgs.Data[Loc.Start].ptr, GetTypeSize(MemT));
								Idx++;
							}
							else
							{
								Assert(HasBasicFlag(MemT, BasicFlag_Float));
								if(MemT->Basic.Kind == Basic_f32)
								{
									memcpy(Ptr, &OptionalArgs.Data[Loc.Start+Idx].f32, 4);
								}
								else
								{
									memcpy(Ptr, &OptionalArgs.Data[Loc.Start+Idx].f64, 8);
								}
							}
							Ptr += MemSize;
						}
						value Value = {};
						Value.Type = I.Type;
						Value.ptr = Memory;
						VM->Registers.AddValue(I.Result, Value);
					} break;
				}
			} break;
			case OP_IF:
			{
				if(!OptionalBlocks.IsValid())
				{
					Assert(false);
					return { INTERPRET_RUNTIME_ERROR };
				}

				value *Cond = VM->Registers.GetValue(I.Result);
				if(Cond->u64)
					VM->Executing->Code = SliceFromArray(FindBlockByID(OptionalBlocks, I.Left).Code);
				else
					VM->Executing->Code = SliceFromArray(FindBlockByID(OptionalBlocks, I.Right).Code);
				InstrIdx = -1;
			} break;
			case OP_JMP:
			{
				if(!OptionalBlocks.IsValid())
				{
					Assert(false);
					return { INTERPRET_RUNTIME_ERROR };
				}

				VM->Executing->Code = SliceFromArray(FindBlockByID(OptionalBlocks, I.BigRegister).Code);
				InstrIdx = -1;
			} break;
			case OP_ARRAYLIST:
			{
				array_list_info *Info = (array_list_info *)I.BigRegister;
				for(int Idx = 0; Idx < Info->Count; ++Idx)
				{
					value *Member = VM->Registers.GetValue(Info->Registers[Idx]);
					
					u32 OutType;
					void *MemberLocation = IndexVM(VM, Info->Alloc, Idx, I.Type, &OutType, true);
					value Ptr = {};
					Ptr.ptr = MemberLocation;
					Store(VM, &Ptr, Member, OutType);
				}
			} break;
			BIN_OP(ADD, +);
			BIN_OP(SUB, -);
			BIN_OP(MUL, *);
			BIN_OP(DIV, /);
			case OP_MOD:
			{
				const type *Type = GetType(I.Type); 
				value Result = {};
				Result.Type = I.Type;
				value *Left  = VM->Registers.GetValue(I.Left);
				value *Right = VM->Registers.GetValue(I.Right);
				if(Type->Kind == TypeKind_Basic)
				{
					switch(Type->Basic.Kind)
					{
						case Basic_bool:
						case Basic_u8:
						case Basic_u16:
						case Basic_u32:
						case Basic_u64:
						case Basic_uint:
						{
							Result.u64 = Left->u64 % Right->u64;
						} break;
						case Basic_i8:
						case Basic_i16:
						case Basic_i32:
						case Basic_i64:
						case Basic_int:
						{
							Result.i64 = Left->i64 % Right->i64;
						} break;
						case Basic_f32:
						{
							Result.f32 = fmod(Left->f32, Right->f32);
						} break;
						case Basic_f64:
						{
							Result.f64 = fmod(Left->f64, Right->f64);
						} break;
						default: unreachable;
					}
				}
				else
				{
					Assert(false);
				}
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_MEMCMP:
			{
				ir_memcmp *Info = (ir_memcmp *)I.BigRegister;
				value *p1 = VM->Registers.GetValue(Info->LeftPtr);
				value *p2 = VM->Registers.GetValue(Info->RightPtr);
				value *count = VM->Registers.GetValue(Info->Count);
				value Result = {};
				Result.Type = Basic_bool;
				Result.u64 = memcmp(p1->ptr, p2->ptr, count->i64) == 0;
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_MEMSET:
			{
				value *p = VM->Registers.GetValue(I.Right);
				memset(p->ptr, 0, GetTypeSize(I.Type));
			} break;
			BIN_BIN_OP(AND, &);
			BIN_BIN_OP(OR, |);
			BIN_BIN_OP(SR, >>);
			BIN_BIN_OP(SL, <<);
			BIN_COMP_OP(GREAT, >);
			BIN_COMP_OP(LESS, <);
			BIN_COMP_OP(GEQ, >=);
			BIN_COMP_OP(LEQ, <=);
			BIN_COMP_OP(EQEQ,==);
			BIN_COMP_OP(NEQ, !=);
			case OP_DEBUGINFO:
			{
				ir_debug_info *Info = (ir_debug_info *)I.BigRegister;
				if(Info->type == IR_DBG_INTERP_ERROR_INFO)
					VM->ErrorInfo.Peek() = Info->err_i.ErrorInfo;

				if(InterpreterTrace)
				{
					if(Info->type == IR_DBG_LOCATION)
						LINFO("%s at line: %d", VM->CurrentFnName.Data, Info->loc.LineNo);
				}
			} break;
			case OP_RESULT:
			{
				VM->Registers.LastAdded = I.Right;
			}
			break;
			default:
			{
				LERROR("-- COMPILER BUG --\nUnsupported Interpreter OP: (%d/%d)", I.Op, OP_COUNT-1);
				return { INTERPRET_RUNTIME_ERROR };
			} break;
		}
	}
	return { INTERPRET_NORETURN, *VM->Registers.GetValue(VM->Registers.LastAdded)};
}

void MakeInterpreter(interpreter &VM, slice<module*> Modules, u32 MaxRegisters)
{
	ForArray(MIdx, Modules)
	{
		module *m = Modules[MIdx];
		ForArray(FIdx, m->Files)
		{
			file *f = m->Files[FIdx];
			ForArray(Idx, f->IR->Functions)
			{
				MaxRegisters = mmax(MaxRegisters, f->IR->Functions[Idx].LastRegister);
			}
		}
	}

	InitArenaMem(&VM.Arena, GB(64), MB(1));
	VM.Globals.Init(MaxRegisters, VM.StackAllocator.Push(MaxRegisters * sizeof(value)));

	ForArray(MIdx, Modules)
	{
		module *m = Modules[MIdx];
		ForArray(Idx, m->Globals.Data)
		{
			symbol *s = m->Globals.Data[Idx];

			value Value = {};
			Value.Type = s->Type;
			Value.Flags |= value_flag::Global;
			if(s->LinkName)
			{
				static const string GlobalInits = STR_LIT("base.global_initializers");
				//static const string Main = STR_LIT("main.main");
				if(*s->LinkName == GlobalInits/* || *s->LinkName == Main*/)
					continue;
			}

			const type *T = GetType(s->Type);
			if(T->Kind == TypeKind_Function)
			{
				if(s->Flags & SymbolFlag_Extern)
				{
					For(DLs)
					{
						void *Proc = GetSymLibrary(*it, s->LinkName->Data);
						if(Proc)
						{
							Value.ptr = Proc;
							break;
						}
					}

					if(Value.ptr == NULL)
					{
						//LERROR("Couldn't find external function %s in compiler linked DLLs.\n\t Referenced in module %s", s->LinkName->Data, m->Name.Data);
					}
				}
				else
				{
					Value.ptr = s->Node->Fn.IR;
					Value.ptr = (void *)((u64)Value.ptr | (1ull << 63));
				}
			}
			else
			{
				Value.ptr = ArenaAllocate(&VM.Arena, GetTypeSize(T));
			}
			VM.Globals.AddValue(s->Register, Value);
		}
	}

	VM.KeepTrackOfStoredGlobals = true;
	EvaluateEnums(&VM);

	ForArray(MIdx, Modules)
	{
		module *m = Modules[MIdx];
		ForArray(FIdx, m->Files)
		{
			file *f = m->Files[FIdx];
			DoGlobals(&VM, f->IR);
		}
	}

	string TypeTableInitName = STR_LIT("base.__TypeTableInit");
	ForArray(MIdx, Modules)
	{
		module *m = Modules[MIdx];
		ForArray(FIdx, m->Files)
		{
			file *f = m->Files[FIdx];
			ForArray(fnIdx, f->IR->Functions)
			{
				function fn = f->IR->Functions[fnIdx];
				if(*fn.Name == TypeTableInitName)
				{
					auto Result = InterpretFunction(&VM, fn, {});
					Assert(Result.Kind == INTERPRET_OK);
				}
			}
		}
	}

	ForArray(MIdx, Modules)
	{
		module *m = Modules[MIdx];
		ForArray(FIdx, m->Files)
		{
			file *f = m->Files[FIdx];
			DoRuns(&VM, f->IR);
		}
	}

	VM.KeepTrackOfStoredGlobals = false;
}

interpret_result RunBlocks(interpreter *VM, function Fn, slice<basic_block> Blocks, slice<value>Args, slice<instruction> Start, b32 Globals=false)
{
	function WasFn = VM->CurrentFn;
	VM->CurrentFn = Fn;

	VM->ErrorInfo.Push(NULL);
	binary_stack Stack = {};
	Stack.Memory = VM->StackAllocator.Push(MB(1));
	VM->Stack.Push(Stack);
	code_chunk Chunk;
	Chunk.Code = Start;
	VM->Executing = &Chunk;

	VM->Registers.Init(Fn.LastRegister, VM->StackAllocator.Push(Fn.LastRegister * sizeof(value)));

	DoAllocationForBlocks(VM, Blocks, Globals);
	interpret_result Result = Run(VM, Blocks, Args);

	VM->Registers.DeInit();

	VM->StackAllocator.Pop();

	VM->StackAllocator.Pop();
	VM->Stack.Pop();
	VM->ErrorInfo.Pop();
	VM->CurrentFn = WasFn;
	return Result;
}

interpret_result InterpretFunction(interpreter *VM, function Function, slice<value> Args)
{
	string SaveCurrentFn = VM->CurrentFnName;

	if(InterpreterTrace && Function.Name)
	{
		VM->CurrentFnName = *Function.Name;
		LINFO("Interp calling function %s with args:", Function.LinkName->Data);
		ForArray(Idx, Args)
		{
			LINFO("\t[%d]%s", Idx, GetTypeName(Args[Idx].Type));
		}
	}

	b32 WasCurrentFnRetInPtr = VM->IsCurrentFnRetInPtr;

	VM->IsCurrentFnRetInPtr = Function.ReturnPassedInPtr;
	interpret_result Result = RunBlocks(VM, Function, SliceFromArray(Function.Blocks), Args, SliceFromArray(Function.Blocks[0].Code));

	VM->IsCurrentFnRetInPtr = WasCurrentFnRetInPtr;
	VM->CurrentFnName = SaveCurrentFn;

	return Result;
}

void EvaluateEnums(interpreter *VM)
{
	uint TC = GetTypeCount();

	binary_stack Stack = {};
	Stack.Memory = VM->StackAllocator.Push(MB(1));

	VM->Stack.Push(Stack);

	for(int i = 0; i < TC; ++i)
	{ 
		const type *T = GetType(i);
		if(T->Kind == TypeKind_Enum)
		{
			For(T->Enum.Members)
			{
				interpret_result Result = RunBlocks(VM, {}, {}, {}, it->Evaluate);
				if(Result.Kind == INTERPRET_RUNTIME_ERROR)
				{
					RaiseError(false, *it->Expr->ErrorInfo,
							"Couldn't evaluate enum expression at compile time.\n"
							"Make sure that you are not using other enums in the expression");
				}
				else
				{
					it->Value = FromInterp(Result.Result);
				}

				// @NOTE: this marks the enum as evaluated
				it->Evaluate = {};
			}
		}
	}

	VM->StackAllocator.Pop();
	VM->Stack.Pop();
}

void DoGlobals(interpreter *VM, ir *IR)
{
	For(IR->Globals)
	{
		if(it->Init.Name == NULL)
		{
			value Result = {};
			Result.Type = it->s->Type;
			Result.ptr = ArenaAllocate(&VM->Arena, GetTypeSize(it->s->Type));
			Result.Flags |= value_flag::Global;
			VM->Globals.AddValue(it->s->Register, Result);
			continue;
		}

		interpret_result Result = RunBlocks(VM, it->Init, SliceFromArray(it->Init.Blocks), {}, SliceFromArray(it->Init.Blocks[0].Code), true);
		if(Result.Kind == INTERPRET_RUNTIME_ERROR)
		{
			exit(1);
		}
		else
		{
			Result.Result.Type = it->s->Type;
			const_value ConstVal = FromInterp(Result.Result);
			it->Value = ConstVal;

			if(IsLoadableType(it->s->Type))
			{
				value Global = {};
				Global.Type = it->s->Type;
				Global.ptr = ArenaAllocate(&VM->Arena, GetTypeSize(Global.Type));
				Store(VM, &Global, &Result.Result, it->s->Type);
				Result.Result = Global;
			}

			Result.Result.Flags |= value_flag::Global;
			VM->Globals.AddValue(it->s->Register, Result.Result);
		}
	}
}

void DoRuns(interpreter *VM, ir *IR)
{
	b32 DoAbort = false;

	For(IR->GlobalRuns)
	{
		interpret_result Result = RunBlocks(VM, *it, SliceFromArray(it->Blocks), {}, SliceFromArray(it->Blocks[0].Code));
		if(Result.Kind == INTERPRET_RUNTIME_ERROR)
		{
			DoAbort = true;
			continue;
		}
	}

	if(DoAbort)
	{
		exit(1);
	}

	ForArray(Idx, IR->Functions)
	{
		auto fn = IR->Functions[Idx];
		For(fn.Runs)
		{
			uint BlockIndex = -1;
			ForArray(BIdx, fn.Blocks)
			{
				if(fn.Blocks[BIdx].ID == it->BlockID)
				{
					BlockIndex = BIdx;
					break;
				}
			}
			Assert(BlockIndex != -1);
			instruction RunI = fn.Blocks[BlockIndex].Code[it->Index];
			uint RunBlockID = RunI.Right;
			uint RunIndex = -1;
			ForArray(BIdx, fn.Blocks)
			{
				if(fn.Blocks[BIdx].ID == RunBlockID)
				{
					RunIndex = BIdx;
					break;
				}
			}
			Assert(RunIndex != -1);
			interpret_result Result = RunBlocks(VM, fn, SliceFromArray(fn.Blocks), {}, SliceFromArray(fn.Blocks[RunIndex].Code));
			instruction Unreachable = {};
			Unreachable.Op = OP_UNREACHABLE;
			fn.Blocks.Data[RunIndex].Code.Push(Unreachable);

			if(Result.Kind == INTERPRET_RUNTIME_ERROR)
			{
				DoAbort = true;
				continue;
			}
			if(RunI.Type != INVALID_TYPE)
			{
				Result.Result.Type = RunI.Type;
				const_value ConstVal = FromInterp(Result.Result);
				instruction NewI = {};
				NewI.Op = OP_CONST;
				NewI.Type = RunI.Type;
				NewI.BigRegister = (u64)DupeType(ConstVal, const_value);
				NewI.Result = RunI.Result;
				fn.Blocks[BlockIndex].Code.Data[it->Index] = NewI;
			}
		}
	}

	if(DoAbort)
	{
		exit(1);
	}
}

