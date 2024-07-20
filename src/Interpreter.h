#pragma once
#include <IR.h>
#include <Stack.h>

enum interpret_result_kind
{
	INTERPRET_OK,
	INTERPRET_NORETURN,
	INTERPRET_RUNTIME_ERROR
};

struct value
{
	u32 Type;
	union
	{
		u64 u64;
		i64 i64;
		f64 f64;
		f32 f32;
		void *ptr;
	};
};

struct interpret_result
{
	interpret_result_kind Kind;
	value Result;
	void *ToFreeStackMemory; // This needs to be freed by the caller after they're done using everything
};

struct code_chunk
{
	slice<instruction> Code;
};

struct interpreter_scope
{
	u32 LastRegister;
	value *Registers;
	void AddValue(uint Register, value Value)
	{
		LastRegister = Register;
		Registers[Register] = Value;
	}
	value *GetValue(uint Register)
	{
		return &Registers[Register];
	}
	void Init(uint MaxRegisterCount)
	{
		LastRegister = 0;
		Registers = (value *)VAlloc(MaxRegisterCount * sizeof(value));
	}
};

struct binary_stack
{
	void *Memory;
	uint Used;
	void *Allocate(uint Size)
	{
		void *Result = ((u8 *)Memory)+Used;
		Used += Size;
		return Result;
	}
};

struct interpreter
{
	code_chunk *Executing;
	interpreter_scope Registers;
	stack<binary_stack> Stack;
};

interpret_result InterpretFunction(interpreter *VM, function Function);
interpret_result Interpret(code_chunk Chunk);
// @TODO: Don't use windows functions directly, some dll abstraction
interpreter MakeInterpreter(slice<ir_symbol> GlobalSymbols, u32 MaxRegisters, HMODULE *DLLs, u32 DLLCount);

#define BIN_OP(OP, o) case OP_##OP: \
			{\
				const type *Type = GetType(I.Type); \
				value Result = {}; \
				Result.Type = I.Type; \
				value *Left  = VM->Registers.GetValue(I.Left); \
				value *Right = VM->Registers.GetValue(I.Left); \
				switch(Type->Basic.Kind) \
				{ \
					case Basic_bool: \
					case Basic_u8: \
					case Basic_u16: \
					case Basic_u32: \
					case Basic_u64: \
					case Basic_uint: \
					{ \
						Result.u64 = Left->u64 o Right->u64; \
					} break; \
					case Basic_i8: \
					case Basic_i16: \
					case Basic_i32: \
					case Basic_i64: \
					case Basic_int: \
					{ \
						Result.i64 = Left->i64 o Right->i64; \
					} break; \
					case Basic_f32: \
					{ \
						Result.f32 = Left->f32 o Right->f32; \
					} break; \
					case Basic_f64: \
					{ \
						Result.f64 = Left->f64 o Right->f64; \
					} break; \
					default: unreachable; \
				} \
				VM->Registers.AddValue(I.Result, Result); \
			} break

