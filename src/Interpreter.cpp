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

u64 PerformForeignFunctionCall(interpreter *VM, call_info *Info, value *Operand)
{
	if(Operand->ptr == NULL) {
		LDEBUG("Not found function, returning 0");
		return 0;
	}

	typedef u64 (*inter_fn)(void *, value *);

	value *Args = (value *)VAlloc(Info->Args.Count * sizeof(value));
	ForArray(Idx, Info->Args)
	{
		Args[Idx] = *VM->Registers.GetValue(Info->Args[Idx]);
	}

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

void DoAllocationForInstructions(interpreter *VM, slice<instruction> Instructions)
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
				Value.ptr = VM->Stack.Peek().Allocate(Size);
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
						Value.ptr = VM->Stack.Peek().Allocate(Size);
						VM->Registers.AddValue(it->Result, Value);
					} break;
					case TypeKind_Struct:
					case TypeKind_Array:
					{
						uint Size = GetTypeSize(it->Type);
						value Value;
						Value.Type = it->Type;
						Value.ptr = VM->Stack.Peek().Allocate(Size);
						VM->Registers.AddValue(it->Result, Value);
					} break;
					case TypeKind_Slice:
					{
						uint Size = sizeof(size_t) * 2;
						value Value;
						Value.Type = it->Type;
						Value.ptr = VM->Stack.Peek().Allocate(Size);
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
					void *Memory = VM->Stack.Peek().Allocate(sizeof(size_t)*2);

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

void DoAllocationForBlocks(interpreter *VM, slice<basic_block> Blocks)
{
	ForArray(Idx, Blocks)
	{
		basic_block& Block = Blocks.Data[Idx];
		DoAllocationForInstructions(VM, SliceFromArray(Block.Code));
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
			case OP_ZEROUT:
			{
				value *Value = VM->Registers.GetValue(I.Right);
				memset(Value->ptr, 0, GetTypeSize(I.Type));
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
					Assert(false);

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
					default: unreachable;
				}
				if(!NoResult)
					VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_TYPEINFO:
			{
				value *TypeTable = VM->Registers.GetValue(I.Left);
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
					return { INTERPRET_RUNTIME_ERROR };

				if(Operand->Type & (1 << 31))
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
					function *F = (function *)Operand->ptr;
					dynamic<value> Args = {};
					ForArray(Idx, CallInfo->Args)
					{
						Args.Push(*VM->Registers.GetValue(CallInfo->Args[Idx]));
					}
					
					code_chunk *Executing = VM->Executing;
					interpreter_scope CurrentScope = VM->Registers;
					interpreter_scope NewScope = {};

					uint Max = mmax(CurrentScope.MaxRegisters, F->LastRegister);
					NewScope.Init(Max, VM->StackAllocator.Push(Max * sizeof(value)));
					CopyRegisters(VM, NewScope);

					VM->Registers = NewScope;

					interpret_result Result = InterpretFunction(VM, *F, SliceFromArray(Args));

					VM->Registers = CurrentScope;
					VM->Executing = Executing;

					VM->Registers.AddValue(I.Result, Result.Result);

					Args.Free();
					VM->StackAllocator.Pop();
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
				if(!OptionalArgs.IsValid())
					return { INTERPRET_RUNTIME_ERROR };
				int Index = I.BigRegister;
				if(VM->IsCurrentFnRetInPtr)
					Index++;

				//int TypeSize = GetTypeSize(I.Type);
				//void *Memory = VM->Stack.Peek().Allocate(TypeSize);
				//value Value = {};
				//Value.Type = GetPointerTo(I.Type);
				//Value.ptr = Memory;
				//Store(VM, &Value, &OptionalArgs.Data[Index], I.Type);
				VM->Registers.AddValue(I.Result, OptionalArgs[Index]);
			} break;
			case OP_IF:
			{
				if(!OptionalBlocks.IsValid())
					return { INTERPRET_RUNTIME_ERROR };

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
					return { INTERPRET_RUNTIME_ERROR };

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
				if(InterpreterTrace)
				{
					ir_debug_info *Info = (ir_debug_info *)I.BigRegister;
					if(Info->type == IR_DBG_LOCATION)
						LINFO("%s at line: %d", VM->CurrentFnName.Data, Info->loc.LineNo);
				}
			} break;
			default:
			{
				LERROR("Unsupported Interpreter OP: (%d/%d)", I.Op, OP_COUNT-1);
				return { INTERPRET_RUNTIME_ERROR };
			} break;
		}
	}
	return { INTERPRET_NORETURN, *VM->Registers.GetValue(VM->Registers.LastAdded)};
}

interpreter MakeInterpreter(slice<module*> Modules, u32 MaxRegisters, DLIB *DLLs, u32 DLLCount)
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

	interpreter VM = {};
	InitArenaMem(&VM.Arena, GB(64), MB(1));
	VM.Registers.Init(MaxRegisters, VM.StackAllocator.Push(MaxRegisters * sizeof(value)));

	ForArray(MIdx, Modules)
	{
		module *m = Modules[MIdx];
		ForArray(Idx, m->Globals.Data)
		{
			symbol *s = m->Globals.Data[Idx];

			value Value = {};
			Value.Type = s->Type;
			if(s->LinkName)
			{
				static const string GlobalInits = STR_LIT("base.global_initializers");
				static const string Main = STR_LIT("main.main");
				if(*s->LinkName == GlobalInits || *s->LinkName == Main)
					continue;
			}

			const type *T = GetType(s->Type);
			if(T->Kind == TypeKind_Function)
			{
				if(s->Flags & SymbolFlag_Extern)
				{
					Value.Type |= 1 << 31;
					for(int Idx = 0; Idx < DLLCount; ++Idx)
					{
						void *Proc = GetSymLibrary(DLLs[Idx], s->LinkName->Data);
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
				}
			}
			else
			{
				Value.ptr = ArenaAllocate(&VM.Arena, GetTypeSize(T));
			}
			VM.Registers.AddValue(s->Register, Value);
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
				const char *sub = strstr(fn.Name->Data, "__GlobalInitializerFunction");
				if(sub)
				{
					//LDEBUG("Module: %s", m->Name.Data);
					InterpretFunction(&VM, fn, {});
				}
				else
				{
					if(*fn.Name == TypeTableInitName)
					{
						auto Result = InterpretFunction(&VM, fn, {});
						Assert(Result.Kind == INTERPRET_OK);
					}
				}
			}
		}
	}

	return VM;
}

interpret_result InterpretFunction(interpreter *VM, function Function, slice<value> Args, b32 NoFree)
{
	binary_stack Stack = {};
	Stack.Memory = VM->StackAllocator.Push(MB(1));

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

	VM->IsCurrentFnRetInPtr = IsRetTypePassInPointer(ReturnsToType(GetType(Function.Type)->Function.Returns));
	VM->Stack.Push(Stack);

	interpret_result Result = {};
	code_chunk Chunk;
	Chunk.Code = SliceFromArray(Function.Blocks[0].Code);
	VM->Executing = &Chunk;
	DoAllocationForBlocks(VM, SliceFromArray(Function.Blocks));
	Result = Run(VM, SliceFromArray(Function.Blocks), Args);

	if(!NoFree)
	{
		VM->StackAllocator.Pop();
		VM->Stack.Pop();
	}

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
				code_chunk Chunk = {};
				Chunk.Code = it->Evaluate;
				VM->Executing = &Chunk;
				DoAllocationForInstructions(VM, it->Evaluate);
				interpret_result Result = Run(VM, {}, {});
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

