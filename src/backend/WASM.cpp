#include "WASM.h"
#include "../Type.h"
#include "../Platform.h"

inline void WriteByte(wasm_output *Code, u8 Byte)
{
	Code->Code[Code->Size++] = Byte;
}

inline void WriteWord(wasm_output *Code, u16 Word)
{
	u8 *Write = &Code->Code[Code->Size];
	*(u16 *)Write = Word;
	Code->Size += 2;
}

inline void WriteDouble(wasm_output *Code, u32 DoubleWord)
{
	u8 *Write = &Code->Code[Code->Size];
	*(u32 *)Write = DoubleWord;
	Code->Size += 4;
}

inline void WriteQuad(wasm_output *Code, u64 QuadWord)
{
	u8 *Write = &Code->Code[Code->Size];
	*(u64 *)Write = QuadWord;
	Code->Size += 8;
}

i32 EncodeNumber(i32 In)
{
	return In;
}

void ConvertToMinimumI32(wasm_output *Code, const type *Type)
{
	// @TODO: Support other types
	Assert(Type->Kind == TypeKind_Basic);
	if(Type->Basic.Flags & BasicFlag_Float)
		return;
	if((Type->Basic.Flags & BasicFlag_Unsigned) == 0)
	{
		int TypeSize = GetBasicTypeSize(Type);
		if(TypeSize == 1)
		{
				WriteByte(Code, WASM_i32Extend8);
		}
		else if(TypeSize == 2)
		{
			WriteByte(Code, WASM_i32Extend16);
		}
	}
}

void ConvertToMinimumI32(wasm_output *Code, u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	ConvertToMinimumI32(Code, Type);
}

void WriteType(wasm_output *Code, u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	// @TODO: Support other types
	Assert(Type->Kind == TypeKind_Basic);

	ConvertToMinimumI32(Code, Type);

	int TypeSize = GetBasicTypeSize(Type);

	if(Type->Basic.Flags & BasicFlag_Float)
	{
		if(TypeSize == 4)
		{
			WriteByte(Code, WASM_f32);
		}
		else if(TypeSize == 8)
		{
			WriteByte(Code, WASM_f64);
		}
		else
			Assert(false);

		return;
	}

	switch(TypeSize)
	{
		case 1:
		case 2:
		case 4:
		{
			WriteByte(Code, WASM_i32);
		} break;
		case 8:
		{
			WriteByte(Code, WASM_i64);
		} break;
		default:
		{
			Assert(false);
		} break;
	}
}

void WriteInstructionBasedOnType(wasm_output *Code, u8 I32, u8 I64, u8 F32, u8 F64, const type *Type)
{
	Assert(Type->Kind == TypeKind_Basic);
	int TypeSize = GetBasicTypeSize(Type);
	if(Type->Basic.Flags & BasicFlag_Float)
	{
		if(TypeSize == 4)
		{
			WriteByte(Code, F32);
		}
		else if(TypeSize == 8)
		{
			WriteByte(Code, F64);
		}
		else
		{
			Assert(false);
		}
		return;
	}
	if(TypeSize < 8)
		WriteByte(Code, I32);
	else
		WriteByte(Code, I64);
}

void WriteInstructionBasedOnType(wasm_output *Code, u8 I32, u8 I64, u8 F32, u8 F64, i32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	WriteInstructionBasedOnType(Code, I32, I64, F32, F64, Type);
}

void GenerateInstruction(wasm_output *Code, instruction I, wasm_builder *WASM)
{
	switch(I.Op)
	{

		case OP_NOP:
		{
			WriteByte(Code, WASM_NOP);
		} break;
		case OP_CONST:
		{
			const type *Type = GetType(I.Type);
			ConvertToMinimumI32(Code, Type);
			WRITE_INSTRUCTION_TYPE(Code, Type, Const);
		} break;
		case OP_ADD:
		{
			WRITE_INSTRUCTION_TYPE(Code, I.Type, Add);
		} break;
		case OP_SUB:
		{
			WRITE_INSTRUCTION_TYPE(Code, I.Type, Sub);
		} break;
		case OP_MUL:
		{
			WRITE_INSTRUCTION_TYPE(Code, I.Type, Mul);
		} break;
		case OP_DIV:
		{
			const type *Type = GetType(I.Type);
			Assert(Type->Kind == TypeKind_Basic);
			if(Type->Basic.Flags & BasicFlag_Unsigned)
			{
				WRITE_INSTRUCTION_TYPE(Code, Type, DivU);
			}
			else
			{
				WRITE_INSTRUCTION_TYPE(Code, Type, Div);
			}
		} break;
		case OP_MOD:
		{
			const type *Type = GetType(I.Type);
			Assert(Type->Kind == TypeKind_Basic);
			if(Type->Basic.Flags & BasicFlag_Float)
			{
				LERROR("FMOD is not implemented in WASM");
				Assert(false);
			}
			if(Type->Basic.Flags & BasicFlag_Unsigned)
			{
				WRITE_INSTRUCTION_TYPE(Code, Type, ModU);
			}
			else
			{
				WRITE_INSTRUCTION_TYPE(Code, Type, Mod);
			}
		} break;
		case OP_STORE:
		{
			WriteByte(Code, WASM_LocalSet);
			WriteQuad(Code, WASM->Locals[I.Left]);
		} break;
		case OP_ALLOC:
		{
			WASM->Locals[I.Result] = WASM->LastLocal++;
		} break;
		case OP_LOAD:
		{
			WriteByte(Code, WASM_LocalGet);
			WriteQuad(Code, WASM->Locals[I.BigRegister]);
		} break;
		case OP_CAST:
		{
		} break;
		default:
		{
			Assert(false);
		} break;
	}
}

void GenerateFunction(wasm_output *Code, function *Function)
{
	wasm_builder WASM;
	WASM.Locals = (u32 *)VAlloc(sizeof(u32) * Function->LocalCount);
#if defined(DEBUG)
	memset(WASM.Locals, 0xFF, sizeof(u32) * Function->LocalCount);
#endif
	WASM.LastLocal = 0;
	for(int BlockI = 0; BlockI < Function->BlockCount; ++BlockI)
	{
		basic_block *Block = &Function->Blocks[BlockI];
		size_t InstructionCount = Block->InstructionCount;
		for(int InstrI = 0; InstrI < InstructionCount; ++InstrI)
		{
			GenerateInstruction(Code, Block->Code[InstrI], &WASM);
		}
	}
	VFree(WASM.Locals);
}

void OutputWasm(ir IR)
{
	wasm_output Code;
	Code.Code = (u8 *)AllocateVirtualMemory(GB(1));
	Code.Size = 0;
	size_t FnCount = ArrLen(IR.Functions);
	for(int I = 0; I < FnCount; ++I)
	{
		GenerateFunction(&Code, &IR.Functions[I]);
	}
}


