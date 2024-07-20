#include "IR.h"
#include "vlib.h"
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

void PerformFunctionCall(interpreter *VM, call_info *Info)
{
	value *Operand = VM->Registers.GetValue(Info->Operand);

	typedef int (__stdcall *inter_fn)(void *, value *);

	assembler Asm = MakeAssembler(KB(1));
	//  rcx = operand
	//  rdx = value *args
	// 
	//  
	//  push rbp
	//  mov rbp, rsp
	//  sub rsp, 32
	//
	//  push rcx
	//  push rdx
	//  
	//  lea rax, [rdx + idx]
	//  mov rbx, [rax + offsetof(
	//
	//  pop rdx
	//  ..
	//
	//	call rcx
	//  mov rsp, rbp
	//  pop rbp
	//  ret
	//
	//
	// 
	//

	operand ConventionRegisters[] = {RegisterOperand(reg_c), RegisterOperand(reg_d), RegisterOperand(reg_r8),
		RegisterOperand(reg_r9)};
	//operand ConventionFloatRegisters[] = {RegisterOperand(reg_xmm0), RegisterOperand(reg_xmm1),
		//RegisterOperand(reg_xmm2), RegisterOperand(reg_xmm3)};

	Asm.Push(RegisterOperand(reg_bp));
	Asm.Mov64(RegisterOperand(reg_bp), RegisterOperand(reg_sp));
	Asm.Sub(RegisterOperand(reg_sp), ConstantOperand(128));

	Asm.Push(RegisterOperand(reg_c));
	Asm.Mov64(RegisterOperand(reg_b), RegisterOperand(reg_d));

	const type *FnType = GetType(Operand->Type);

	uint CurrentInt = 0;
	//uint CurrentFloat = 0;
	ForArray(Idx, Info->Args)
	{
		const type *Type = GetType(FnType->Function.Args[Idx]);
		Asm.Lea64(RegisterOperand(reg_a), OffsetOperand(reg_b, Idx * sizeof(value)));
		switch(Type->Kind)
		{
			case TypeKind_Basic:
			{
				if(CurrentInt < ARR_LEN(ConventionRegisters))
				{
					Asm.Mov64(ConventionRegisters[CurrentInt++], OffsetOperand(reg_a, offsetof(value, u64)));
				}
				else
				{
					Asm.Mov64(RegisterOperand(reg_a), OffsetOperand(reg_a, offsetof(value, u64)));
					Asm.Push(RegisterOperand(reg_a));
				}
			} break;

			default: unreachable;
		}
	}


	Asm.Pop(RegisterOperand(reg_b));
	Asm.Call(RegisterOperand(reg_b));

	Asm.Mov64(RegisterOperand(reg_sp), RegisterOperand(reg_bp));
	Asm.Pop(RegisterOperand(reg_bp));
	Asm.Ret();

	for(int Idx = 0; Idx < Asm.CurrentOffset; ++Idx)
	{
		//printf("%02x ", ((u8 *)Asm.Code)[Idx]);
	}

	inter_fn ToCall = (inter_fn)Asm.Code;

	value *Args = (value *)VAlloc(Info->Args.Count * sizeof(value));
	ForArray(Idx, Info->Args)
	{
		Args[Idx] = *VM->Registers.GetValue(Info->Args[Idx]);
	}

	ToCall(Operand->ptr, Args);
}

interpret_result Run(interpreter *VM)
{
	ForArray(InstrIdx, VM->Executing->Code)
	{
		instruction I = VM->Executing->Code[InstrIdx];
		switch(I.Op)
		{
			case OP_NOP:
			{
			} break;
			case OP_CONST:
			{
				value VMValue = {};
				VMValue.Type = I.Type;
				const_value *Val = (const_value *)I.BigRegister;
				const type *Type = GetType(I.Type);
				if(Type->Basic.Flags & BasicFlag_Float)
				{
					switch(Type->Basic.Kind)
					{
						case Basic_f64:
						{
							VMValue.f64 = Val->Float;
						} break;
						case Basic_f32:
						{
							VMValue.f32 = (f32)Val->Float;
						} break;
						default: unreachable;
					}
				}
				else if(Type->Basic.Flags & BasicFlag_Integer)
				{
					if(Val->Int.IsSigned)
						VMValue.i64 = Val->Int.Signed;
					else
						VMValue.u64 = Val->Int.Unsigned;
				}
				else if(Type->Basic.Flags & BasicFlag_String)
				{
					// @TODO;
					Assert(false);
				}
				else if(Type->Basic.Flags & BasicFlag_CString)
				{
					VMValue.ptr = InterpreterAllocateString(VM, Val->String.Data);
				}
				else
				{
					LDEBUG("%d", Type->Kind);
					unreachable;
				}
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
				Assert(Type->Kind == TypeKind_Basic);

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
				VM->Registers.AddValue(I.Result, Result);
			} break;
			case OP_STORE:
			{
#define STORE_T(T, size) case Basic_##T: \
						{ \
							*(u##size *)Left->ptr = Right->u64; \
						} break

				value *Left = VM->Registers.GetValue(I.Left);
				value *Right = VM->Registers.GetValue(I.Right);
				const type *Type = GetType(I.Type);
				Assert(Type->Kind == TypeKind_Basic);

				switch(Type->Basic.Kind)
				{
					STORE_T(bool, 8);

					STORE_T(u8, 8);
					STORE_T(u16, 16);
					STORE_T(u32, 32);
					STORE_T(u64, 64);

					STORE_T(i8, 8);
					STORE_T(i16, 16);
					STORE_T(i32, 32);
					STORE_T(i64, 64);

					STORE_T(f32, 32);
					STORE_T(f64, 64);

					STORE_T(int, 64);
					STORE_T(uint, 64);
					default: unreachable;
				}
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
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)I.BigRegister;
				PerformFunctionCall(VM, CallInfo);
			} break;
			BIN_OP(ADD, +);
			BIN_OP(SUB, -);
			BIN_OP(MUL, *);
			BIN_OP(DIV, /);
			BIN_OP(MOD, /);
			BIN_OP(GREAT, >);
			BIN_OP(LESS, <);
			BIN_OP(GEQ, >=);
			BIN_OP(LEQ, <=);
			BIN_OP(EQEQ, ==);
			default:
			{
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

	interpret_result Result = Run(&VM);
	Result.ToFreeStackMemory = Stack.Memory;


	return Result;
}

interpreter MakeInterpreter(slice<ir_symbol> GlobalSymbols, u32 MaxRegisters, HMODULE *DLLs, u32 DLLCount)
{
	interpreter VM = {};
	VM.Registers.Init(MaxRegisters);
	ForArray(Idx, GlobalSymbols)
	{
		ir_symbol Symbol = GlobalSymbols[Idx];
		value Value = {};
		Value.Type = Symbol.Type;
		if(Symbol.Flags & IRSymbol_ExternFn)
		{
			for(int Idx = 0; Idx < DLLCount; ++Idx)
			{
				FARPROC Proc = GetProcAddress(DLLs[Idx], Symbol.Name->Data);
				if(Proc)
				{
					Value.ptr = (void *)Proc;
					break;
				}
			}
			if(Value.ptr == NULL)
			{
				LERROR("Couldn't find external function %s in compiler linked DLLs", Symbol.Name);
				return {};
			}
		}
		VM.Registers.AddValue(GlobalSymbols[Idx].Register, Value);
	}

	return VM;
}

interpret_result InterpretFunction(interpreter *VM, function Function)
{
	binary_stack Stack = {};
	Stack.Memory = VAlloc(MB(1));

	VM->Stack.Push(Stack);

	interpret_result Result = {};
	ForArray(Idx, Function.Blocks)
	{
		code_chunk Chunk;
		Chunk.Code = SliceFromArray(Function.Blocks[Idx].Code);
		VM->Executing = &Chunk;
		Result = Run(VM);
	}
	Result.ToFreeStackMemory = Stack.Memory;


	return Result;
}


