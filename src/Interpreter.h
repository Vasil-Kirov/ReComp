#pragma once
#include <IR.h>
#include <Stack.h>
#include <immintrin.h>
#include "DynamicLib.h"
#include "Memory.h"
#include "StackAllocator.h"
#include <unordered_map>

#define mmax(a, b) (a > b) ? a : b

struct interp_string
{
	size_t Count;
	const char *Data;
};

struct compile_info
{
	size_t FileCount;
	interp_string *FileNames;
	i64 Optimization;
	u32 Flags;
	interp_string TargetTriple;
	u32 Arch;
	interp_string Link;
	interp_string InternalFile;
};

enum interpret_result_kind
{
	INTERPRET_OK,
	INTERPRET_NORETURN,
	INTERPRET_RUNTIME_ERROR
};

enum class value_flag: u32
{
	Global = BIT(0),
};

inline value_flag operator|(value_flag a, value_flag b)
{
    return static_cast<value_flag>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline u32 operator&(value_flag a, value_flag b)
{
    return (u32) (static_cast<value_flag>(static_cast<u32>(a) & static_cast<u32>(b)));
}

inline value_flag& operator|=(value_flag &a, value_flag b)
{
	return a = a | b;
}

struct value
{
	union
	{
		__m64 ivec2;
		__m64 fvec2;
		u64 u64;
		i64 i64;
		f64 f64;
		f32 f32;
		void *ptr;
	};
	// @TODO: maybe putting these in an anonymous struct would make it smaller?
	value_flag Flags;
	u32 Type;
};

struct interpret_result
{
	interpret_result_kind Kind;
	value Result;
};

struct code_chunk
{
	slice<instruction> Code;
};

struct global_link
{
	u32 GlobalRegister;
	u32 LocalRegister;
};

struct interpreter_scope
{
	u32 LastRegister;
	u32 MaxRegisters;
	u32 LastAdded;
	value *Registers;
	dynamic<global_link> Links = {};
	void AddValue(uint Register, value Value)
	{
		LastAdded = Register;
		LastRegister = mmax(LastRegister, Register);
		Registers[Register] = Value;
	}
	value *GetValue(uint Register)
	{
		return &Registers[Register];
	}
	void Init(uint MaxRegisterCount, void *Mem)
	{
		MaxRegisters = MaxRegisterCount;
		LastRegister = 0;
		Registers = (value *)Mem;//(value *)VAlloc(MaxRegisterCount * sizeof(value));
		Links = {};
	}
	void DeInit()
	{
		Links.Free();
	}
	//void Free()
	//{
	//	LastRegister = 0;
	//	VFree(Registers);
	//}
};

struct binary_stack
{
	void *Memory;
	u8 *At = NULL;

	void *Allocate(uint Size)
	{
		if(At == NULL)
			At = (u8 *)Memory;
		At = (u8 *)Align16(At);
		void *Result = At;
		At += Size;
		if(At - (u8 *)Memory >= MB(1))
		{
			LogCompilerError("Error: Interpreter stack ran out of memory\n");
			abort();
		}

		return Result;
	}
};

struct stored_global
{
	void *Ptr;
	uint Register;
};

struct interpreter
{
	code_chunk *Executing;
	interpreter_scope Globals;
	interpreter_scope Registers;
	ap_memory Arena;
	stack_alloc StackAllocator;
	stack<binary_stack> Stack;
	stack<const error_info *> ErrorInfo;
	slice<function> Imported;
	std::unordered_map<void *, uint> StoredGlobals;
	function CurrentFn;
	string CurrentFnName;
	b32 IsCurrentFnRetInPtr;
	b32 KeepTrackOfStoredGlobals;
};

#include <dyncall.h>
DCaggr *MakeAggr(u32 TIdx, dynamic<DCaggr*> AggrToFree);
DCaggr *MakeAggr(const type *T, dynamic<DCaggr*> AggrToFree);

interpret_result InterpretFunction(interpreter *VM, function Function, slice<value> Args);
interpreter MakeInterpreter(slice<module> Modules, u32 MaxRegisters);
void DoRuns(interpreter *VM, ir *IR);
void EvaluateEnums(interpreter *VM);
void DoGlobals(interpreter *VM, ir *IR);

extern dynamic<DLIB> DLs;

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


