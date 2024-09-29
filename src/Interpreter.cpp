#include "DynamicLib.h"
#include "IR.h"
#include "VString.h"
#include "vlib.h"
#include "Semantics.h"
#include <Interpreter.h>
#include <Type.h>
#include <Log.h>
#include <cstddef>
#include <x64CodeWriter.h>

void *InterpreterAllocateString(interpreter *VM, const string *String)
{
	void *Memory = VM->Stack.Peek().Allocate(String->Size + 1);
	memcpy(Memory, String->Data, String->Size);
	return Memory;
}

__attribute__((no_sanitize("address")))
u64 PerformFunctionCall(interpreter *VM, call_info *Info)
{
	value *Operand = VM->Registers.GetValue(Info->Operand);

	typedef u64 (*inter_fn)(void *, value *);

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
#else
#error "Unknown calling convention"
#endif
	//operand ConventionFloatRegisters[] = {RegisterOperand(reg_xmm0), RegisterOperand(reg_xmm1),
		//RegisterOperand(reg_xmm2), RegisterOperand(reg_xmm3)};

	Asm.Push(RegisterOperand(reg_bp));
	Asm.Mov64(RegisterOperand(reg_bp), RegisterOperand(reg_sp));
	Asm.Sub(RegisterOperand(reg_sp), ConstantOperand(128));


#if _WIN32
	Asm.Push(RegisterOperand(reg_c));
	Asm.Push(RegisterOperand(reg_d));
#elif CM_LINUX
	Asm.Push(RegisterOperand(reg_di));
	Asm.Push(RegisterOperand(reg_si));
#else
#error "Unknown calling convention"
#endif

	const type *FnType = GetType(Operand->Type);

	uint CurrentInt = 0;
	//uint CurrentFloat = 0;

	ForArray(Idx, Info->Args)
	{
		const type *Type = GetType(FnType->Function.Args[Idx]);
		Asm.Peek(RegisterOperand(reg_a));
		Asm.Lea64(RegisterOperand(reg_a), OffsetOperand(reg_a, Idx * sizeof(value)));
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


	Asm.Pop(RegisterOperand(reg_a));
	Asm.Pop(RegisterOperand(reg_a));
	Asm.Call(RegisterOperand(reg_a));

	Asm.Mov64(RegisterOperand(reg_sp), RegisterOperand(reg_bp));
	Asm.Pop(RegisterOperand(reg_bp));
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

	value *Args = (value *)VAlloc(Info->Args.Count * sizeof(value));
	ForArray(Idx, Info->Args)
	{
		Args[Idx] = *VM->Registers.GetValue(Info->Args[Idx]);
	}

	u64 Result = ToCall(Operand->ptr, Args);
	FreeVirtualMemory(Asm.Code);
	return Result;
}

void Store(interpreter *VM, value *Ptr, value *Value, u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	if(Type->Kind == TypeKind_Basic)
	{
		int TypeSize = GetTypeSize(Type);
		if(Type->Basic.Kind == Basic_cstring)
			*(void **)Ptr->ptr = Value->ptr;
		else
			memcpy(Ptr->ptr, &Value->u64, TypeSize);
	}
	else if(Type->Kind == TypeKind_Pointer)
	{
		*(void **)Ptr->ptr = Value->ptr;
	}
	else
	{
		memcpy(Ptr->ptr, Value->ptr, GetTypeSize(Type));
	}

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
			case OP_CONSTINT:
			{
				u64 Val = I.BigRegister;
				value Value = {};
				Value.Type = I.Type;
				Value.u64 = Val;
				VM->Registers.AddValue(I.Result, Value);
			} break;
			case OP_CONST:
			{
				value VMValue = {};
				VMValue.Type = I.Type;
				const_value *Val = (const_value *)I.BigRegister;
				const type *Type = GetType(I.Type);
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
						// @TODO;
						Assert(false);
					}
					else if(Type->Basic.Flags & BasicFlag_CString)
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
								VMValue.ptr = InterpreterAllocateString(VM, Val->String.Data);
							} break;
						}
					}
					else
					{
						LDEBUG("%d", Type->Kind);
						unreachable;
					}
				}
				else if(Type->Kind == TypeKind_Pointer)
				{
					VMValue.ptr = (void *)Val->Int.Unsigned;
				}
				else
					Assert(false);
				VM->Registers.AddValue(I.Result, VMValue);
			} break;
			case OP_ALLOC:
			{
				const type *Type = GetType(I.Type);
				uint Size = GetTypeSize(Type);
				value Value = {};
				Value.Type = GetPointerTo(I.Type);
				Value.ptr = VM->Stack.Peek().Allocate(Size);
				VM->Registers.AddValue(I.Result, Value);
			} break;
			case OP_LOAD:
			{
#define LOAD_T(T, size) case Basic_##T: \
						{ \
							Result.u64 = *(u##size *)Value->ptr; \
						} break

				value *Value = VM->Registers.GetValue(I.Right);
				Value->Type = I.Type;
				value Result = {};
				const type *Type = GetType(I.Type);

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
						uint Size = GetTypeSize(Type);
						Result.ptr = VM->Stack.Peek().Allocate(Size);
						memcpy(Result.ptr, Value->ptr, Size);
					} break;
					default: unreachable;
				}
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_STORE:
			{
				value *Left = VM->Registers.GetValue(I.Left);
				value *Right = VM->Registers.GetValue(I.Right);
				Store(VM, Left, Right, I.Type);
			} break;
			case OP_INDEX:
			{
				value Value = {};
				value *Operand = VM->Registers.GetValue(I.Left);
				const type *Type = GetType(I.Type);
				switch(Type->Kind)
				{
					case TypeKind_Array:
					{
						value *Index = VM->Registers.GetValue(I.Right);
						int TypeSize = GetTypeSize(Type->Array.Type);
						Value.ptr = ((u8 *)Operand->ptr) + (TypeSize * Index->u64);
					} break;
					case TypeKind_Struct:
					{
						int Offset = GetStructMemberOffset(Type, I.Right);
						Value.ptr = ((u8 *)Operand->ptr) + Offset;
					} break;
					case TypeKind_Pointer:
					{
						value *Index = VM->Registers.GetValue(I.Right);
						int TypeSize = GetTypeSize(Type->Pointer.Pointed);
						Value.ptr = (*(u8 **)Operand->ptr) + (TypeSize * Index->u64);
					} break;
					case TypeKind_Basic:
					{
						if(HasBasicFlag(Type, BasicFlag_CString))
						{
							value *Index = VM->Registers.GetValue(I.Right);
							Value.ptr = (u8 *)Operand->ptr + Index->u64;
							break;
						}
						else
							unreachable;
					} break;
					default: unreachable;
				}
				VM->Registers.AddValue(I.Result, Value);
			} break;
			case OP_RET:
			{
				if(I.Left == -1)
				{
					return { INTERPRET_OK, {} };
				}
				else
				{
					return { INTERPRET_OK, *VM->Registers.GetValue(I.Left) };
				}
			} break;
			case OP_CAST:
			{
				u32 FromType = I.Right;
				u32 ToType = I.Type;
				value *Val = VM->Registers.GetValue(I.Left);
				value Result = {};
				Result.Type = ToType;
				if(FromType == Basic_string && ToType == Basic_cstring)
				{
					Result.ptr = Val->ptr;
				}
				else
				{
					// @TODO:
					Result.ptr = Val->ptr;
				}
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)I.BigRegister;
				u64 Result = PerformFunctionCall(VM, CallInfo);
				if(I.Type != INVALID_TYPE)
				{
					value Value = {};
					Value.Type = I.Type;
					Value.u64 = Result;
					VM->Registers.AddValue(I.Result, Value);
				}
			} break;
			case OP_ARG:
			{
				if(!OptionalArgs.IsValid())
					return { INTERPRET_RUNTIME_ERROR };
				int TypeSize = GetTypeSize(I.Type);
				void *Memory = VM->Stack.Peek().Allocate(TypeSize);
				value Value = {};
				Value.Type = GetPointerTo(I.Type);
				Value.ptr = Memory;
				Store(VM, &Value, &OptionalArgs.Data[I.BigRegister], I.Type);
				VM->Registers.AddValue(I.Result, OptionalArgs[I.BigRegister]);
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
				return Run(VM, OptionalBlocks, OptionalArgs);
			} break;
			case OP_JMP:
			{
				if(!OptionalBlocks.IsValid())
					return { INTERPRET_RUNTIME_ERROR };

				VM->Executing->Code = SliceFromArray(FindBlockByID(OptionalBlocks, I.BigRegister).Code);
				return Run(VM, OptionalBlocks, OptionalArgs);
			} break;
			BIN_OP(ADD, +);
			BIN_OP(SUB, -);
			BIN_OP(MUL, *);
			BIN_OP(DIV, /);
			BIN_OP(MOD, /);
			BIN_COMP_OP(GREAT, >);
			BIN_COMP_OP(LESS, <);
			BIN_COMP_OP(GEQ, >=);
			BIN_COMP_OP(LEQ, <=);
			BIN_COMP_OP(EQEQ,==);
			BIN_COMP_OP(NEQ, !=);
			BIN_COMP_OP(LAND, &&);
			case OP_DEBUGINFO:
			{} break;
			default:
			{
				LDEBUG("Unsupported Interpreter OP: (%d/%d)", I.Op, OP_COUNT-1);
				return { INTERPRET_RUNTIME_ERROR };
			} break;
		}
	}
	return { INTERPRET_NORETURN, *VM->Registers.GetValue(VM->Registers.LastRegister)};
}

interpret_result Interpret(code_chunk Chunk)
{
	binary_stack Stack = {};
	Stack.Memory = AllocateVirtualMemory(MB(1));

	interpreter VM = {};
	VM.Executing = &Chunk;
	VM.Stack.Push(Stack);

	interpret_result Result = Run(&VM, {}, {});
	Result.ToFreeStackMemory = Stack.Memory;


	return Result;
}

interpreter MakeInterpreter(slice<module> Modules, u32 MaxRegisters, DLIB *DLLs, u32 DLLCount)
{
	interpreter VM = {};
	VM.Registers.Init(MaxRegisters);
	ForArray(MIdx, Modules)
	{
		module *m = &Modules.Data[MIdx];
		ForArray(Idx, m->Globals)
		{
			symbol *s = m->Globals[Idx];
			value Value = {};
			Value.Type = s->Type;
			if(s->Flags & SymbolFlag_Function)
			{
				if(s->Flags & SymbolFlag_Extern)
				{
					for(int Idx = 0; Idx < DLLCount; ++Idx)
					{
						void *Proc = GetSymLibrary(DLLs[Idx], s->Name->Data);
						if(Proc)
						{
							Value.ptr = Proc;
							break;
						}
					}

					if(Value.ptr == NULL)
					{
						LERROR("Couldn't find external function %s in compiler linked DLLs", s->Name->Data);
						return {};
					}
				}
				else
				{
					// @TODO:
					{};
				}
			}
			else
			{
				Value.ptr = VAlloc(GetTypeSize(s->Type));
			}
			VM.Registers.AddValue(s->IRRegister, Value);
		}
	}
	ForArray(MIdx, Modules)
	{
		module *m = &Modules.Data[MIdx];
		ForArray(FIdx, m->Files)
		{
			file *f = m->Files[FIdx];
			ForArray(fnIdx, f->IR->Functions)
			{
				function fn = f->IR->Functions[fnIdx];
				if(StringEndsWith(*fn.Name, STR_LIT("__GlobalInitializerFunction")))
				{
					InterpretFunction(&VM, fn, {});
				}
			}
		}
	}

	return VM;
}

interpret_result InterpretFunction(interpreter *VM, function Function, slice<value> Args)
{
	binary_stack Stack = {};
	Stack.Memory = VAlloc(MB(1));

	VM->Stack.Push(Stack);

	interpret_result Result = {};
	code_chunk Chunk;
	Chunk.Code = SliceFromArray(Function.Blocks[0].Code);
	VM->Executing = &Chunk;
	Result = Run(VM, SliceFromArray(Function.Blocks), Args);

	Result.ToFreeStackMemory = Stack.Memory;


	return Result;
}


