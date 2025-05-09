#pragma once
#include <IR.h>
#include <Stack.h>
#include <immintrin.h>
#include "DynamicLib.h"
#include "Memory.h"
#include "StackAllocator.h"
#include <unordered_map>
#include "InterpCasts.h"

#define mmax(a, b) (a > b) ? a : b

enum DebugAction
{
	DebugAction_continue,
	DebugAction_step_instruction,
	DebugAction_next_stmt,
	DebugAction_list_instructions,
	DebugAction_quit,
	DebugAction_prompt_again,
	DebugAction_break,
};

struct interp_string
{
	size_t Count;
	const char *Data;
};

struct compile_info
{
	size_t DirectoryCount;
	interp_string *Directories;
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
	// @TODO: maybe putting these in an anonymous struct would make it smaller?
	value_flag Flags;
	u32 Type;
	union
	{
		i32 ivec2[2];
		f32 fvec2[2];
		__m128i ivec;
		__m128  fvec;
		u8 u8;
		u16 u16;
		u32 u32;
		u64 u64;
		i8 i8;
		i16 i16;
		i32 i32;
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
	stack<string> FunctionStack;
	stack<const error_info *> ErrorInfo;
	slice<function> Imported;
	std::unordered_map<void *, uint> StoredGlobals;
	function CurrentFn;
	string CurrentFnName;
	DebugAction PerformingDebugAction;
	int AtInstructionIndex;
	b32 IsCurrentFnRetInPtr;
	b32 KeepTrackOfStoredGlobals;
	b32 HasSetSigHandler;
};

#include <dyncall.h>
DCaggr *MakeAggr(u32 TIdx, dynamic<DCaggr*> AggrToFree);
DCaggr *MakeAggr(const type *T, dynamic<DCaggr*> AggrToFree);

interpret_result InterpretFunction(interpreter *VM, function Function, slice<value> Args);
interpreter MakeInterpreter(slice<module> Modules, u32 MaxRegisters);
void DoRuns(interpreter *VM, ir *IR);
void EvaluateEnums(interpreter *VM);
void DoGlobals(interpreter *VM, ir *IR);
void DoDebugPrompt(interpreter *VM, slice<instruction> Instructions, int InstrIdx);

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
										 { \
											 Result.u8 = Left->u8 o Right->u8; \
										 } break; \
						case Basic_u16: \
										 { \
											 Result.u16 = Left->u16 o Right->u16; \
										 } break; \
						case Basic_u32: \
										 { \
											 Result.u32 = Left->u32 o Right->u32; \
										 } break; \
						case Basic_u64: \
						case Basic_uint: \
						case Basic_type: \
										 { \
											 Result.u64 = Left->u64 o Right->u64; \
										 } break; \
						case Basic_i8: \
										 { \
											 Result.i8 = Left->i8 o Right->i8; \
										 } break; \
						case Basic_i16: \
										 { \
											 Result.i16 = Left->i16 o Right->i16; \
										 } break; \
						case Basic_i32: \
										 { \
											 Result.i32 = Left->i32 o Right->i32; \
										 } break; \
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
										 { \
											 Result.u8 = Left->u8 o Right->u8; \
										 } break; \
						case Basic_u16: \
										 { \
											 Result.u16 = Left->u16 o Right->u16; \
										 } break; \
						case Basic_u32: \
										 { \
											 Result.u32 = Left->u32 o Right->u32; \
										 } break; \
						case Basic_u64: \
						case Basic_uint: \
						case Basic_type: \
										 { \
											 Result.u64 = Left->u64 o Right->u64; \
										 } break; \
						case Basic_i8: \
										 { \
											 Result.i8 = Left->i8 o Right->i8; \
										 } break; \
						case Basic_i16: \
										 { \
											 Result.i16 = Left->i16 o Right->i16; \
										 } break; \
						case Basic_i32: \
										 { \
											 Result.i32 = Left->i32 o Right->i32; \
										 } break; \
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
				value Result = {}; \
				Result.Type = Basic_bool; \
				value *Left  = VM->Registers.GetValue(I.Left); \
				value *Right = VM->Registers.GetValue(I.Right); \
				const type *Type = GetType(Left->Type); \
				if(Type->Kind == TypeKind_Enum) { I.Type = Type->Enum.Type; Type = GetType(I.Type);  } \
				if(Type->Kind == TypeKind_Basic) \
				{ \
					switch(Type->Basic.Kind) \
					{ \
						case Basic_bool: \
						case Basic_u8: \
										 { \
											 Result.u8 = Left->u8 o Right->u8; \
										 } break; \
						case Basic_u16: \
										 { \
											 Result.u8 = Left->u16 o Right->u16; \
										 } break; \
						case Basic_u32: \
										 { \
											 Result.u8 = Left->u32 o Right->u32; \
										 } break; \
						case Basic_u64: \
						case Basic_uint: \
										 { \
											 Result.u8 = Left->u64 o Right->u64; \
										 } break; \
						case Basic_i8: \
										 { \
											 Result.u8 = Left->i8 o Right->i8; \
										 } break; \
						case Basic_i16: \
										 { \
											 Result.u8 = Left->i16 o Right->i16; \
										 } break; \
						case Basic_i32: \
										 { \
											 Result.u8 = Left->i32 o Right->i32; \
										 } break; \
						case Basic_i64: \
						case Basic_type: \
						case Basic_int: \
										{ \
											Result.u8 = Left->i64 o Right->i64; \
										} break; \
						case Basic_f32: \
										{ \
											Result.u8 = Left->f32 o Right->f32; \
										} break; \
						case Basic_f64: \
										{ \
											Result.u8 = Left->f64 o Right->f64; \
										} break; \
						default: unreachable; \
					} \
				} \
				else if(Type->Kind == TypeKind_Pointer) \
				{ \
					Result.u8 = Left->ptr o Right->ptr; \
				} \
				else \
				{\
					Assert(false); \
				} \
				VM->Registers.AddValue(I.Result, Result); \
			} break


