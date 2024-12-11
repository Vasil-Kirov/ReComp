#pragma once
#include <IR.h>
#include <Stack.h>
#include "DynamicLib.h"

#define mmax(a, b) (a > b) ? a : b

struct interp_string
{
	const char *Data;
	size_t Count;
};

struct compile_info
{
	size_t FileCount;
	interp_string *FileNames;
	i64 Optimization;
	u32 Flags;
	interp_string TargetTriple;
};

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
	u32 MaxRegisters;
	value *Registers;
	void AddValue(uint Register, value Value)
	{
		LastRegister = mmax(LastRegister, Register);
		Registers[Register] = Value;
	}
	value *GetValue(uint Register)
	{
		return &Registers[Register];
	}
	void Init(uint MaxRegisterCount)
	{
		MaxRegisters = MaxRegisterCount;
		LastRegister = 0;
		Registers = (value *)VAlloc(MaxRegisterCount * sizeof(value));
	}
	void Free()
	{
		LastRegister = 0;
		VFree(Registers);
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
	slice<function> Imported;
	b32 IsCurrentFnRetInPtr;
};

interpret_result InterpretFunction(interpreter *VM, function Function, slice<value> Args);
interpret_result Interpret(code_chunk Chunk);
interpreter MakeInterpreter(slice<module> Modules, u32 MaxRegisters, DLIB *DLLs, u32 DLLCount);

#define BIN_OP(OP, o) case OP_##OP: \
			{\
				const type *Type = GetType(I.Type); \
				if(Type->Kind == TypeKind_Enum) { I.Type = Type->Enum.Type; Type = GetType(I.Type);  } \
				value Result = {}; \
				Result.Type = I.Type; \
				value *Left  = VM->Registers.GetValue(I.Left); \
				value *Right = VM->Registers.GetValue(I.Right); \
				if(Type->Kind == TypeKind_Basic) \
				{ \
					switch(Type->Basic.Kind) \
					{ \
						case Basic_bool: \
						case Basic_u8: \
						case Basic_u16: \
						case Basic_u32: \
						case Basic_u64: \
						case Basic_uint: \
						case Basic_type: \
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
						default: LERROR("No bin OP: %s", GetTypeName(Type)); unreachable; \
					} \
				} \
				else \
				{\
					LERROR("No bin OP: %s", GetTypeName(Type)); \
					Assert(false); \
				} \
				VM->Registers.AddValue(I.Result, Result); \
			} break

#define BIN_BIN_OP(OP, o) case OP_##OP: \
			{\
				const type *Type = GetType(I.Type); \
				if(Type->Kind == TypeKind_Enum) { I.Type = Type->Enum.Type; Type = GetType(I.Type);  } \
				value Result = {}; \
				Result.Type = I.Type; \
				value *Left  = VM->Registers.GetValue(I.Left); \
				value *Right = VM->Registers.GetValue(I.Right); \
				if(Type->Kind == TypeKind_Basic) \
				{ \
					switch(Type->Basic.Kind) \
					{ \
						case Basic_bool: \
						case Basic_u8: \
						case Basic_u16: \
						case Basic_u32: \
						case Basic_u64: \
						case Basic_uint: \
						case Basic_type: \
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
						default: unreachable; \
					} \
				} \
				else \
				{\
					Assert(false); \
				} \
				VM->Registers.AddValue(I.Result, Result); \
			} break

#define BIN_COMP_OP(OP, o) case OP_##OP: \
			{\
				const type *Type = GetType(I.Type); \
				if(Type->Kind == TypeKind_Enum) { I.Type = Type->Enum.Type; Type = GetType(I.Type);  } \
				value Result = {}; \
				Result.Type = Basic_bool; \
				value *Left  = VM->Registers.GetValue(I.Left); \
				value *Right = VM->Registers.GetValue(I.Right); \
				if(Type->Kind == TypeKind_Basic) \
				{ \
					switch(Type->Basic.Kind) \
					{ \
						case Basic_bool: \
						case Basic_u8: \
						case Basic_u16: \
						case Basic_u32: \
						case Basic_u64: \
						case Basic_uint: \
						case Basic_type: \
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
				} \
				else if(Type->Kind == TypeKind_Pointer) \
				{ \
					Result.ptr = (void *)((u8 *)Left->ptr o (u8 *)Right->ptr); \
				} \
				else \
				{\
					Assert(false); \
				} \
				VM->Registers.AddValue(I.Result, Result); \
			} break


