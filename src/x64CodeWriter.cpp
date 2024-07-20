#include "x64CodeWriter.h"
#include <Basic.h>


assembler MakeAssembler(uint MemorySize)
{
	assembler Result = {};
	Result.Code = AllocateExecutableVirtualMemory(MemorySize);
	Result.Size = MemorySize;

	return Result;
}

operand RegisterOperand(register_ Reg)
{
	return {operand_register, Reg};
}

operand OffsetOperand(register_ Reg, u8 Offset)
{
	return {operand_offset, {.Offset = {Reg, Offset}}};
}

operand ConstantOperand(u32 Constant)
{
	return {operand_constant, {.Constant = Constant}};
}

void assembler::PushByte(u8 Byte)
{
	((u8 *)Code)[CurrentOffset] = Byte;
	CurrentOffset++;
}

void assembler::PushU32(u32 QWORD)
{
	u8 *Bytes = (u8 *)&QWORD;
	for(int i = 0; i < 4; i++)
	{
		PushByte(Bytes[i]);
	}
}

void assembler::PushU64(u64 QWORD)
{
	u8 *Bytes = (u8 *)&QWORD;
	for(int i = 0; i < 8; i++)
	{
		PushByte(Bytes[i]);
	}
}

void assembler::Push(operand Operand)
{
	Assert(Operand.Type == operand_register);
	PushByte(0x50 | Operand.Register);
}

void assembler::Sub(operand Dst, operand Src)
{
	Assert(Dst.Type == operand_register);
	Assert(Src.Type == operand_constant);
	PushByte(REX_W);
	PushByte(0x81);
	PushByte((MOD_register << 6) | (5 << 3) | Dst.Register);
	PushU32(Src.Constant);
}

void assembler::Pop(operand Operand)
{
	Assert(Operand.Type == operand_register);
	PushByte(0x58 | Operand.Register);
}

void assembler::Xor(operand A, operand B)
{
	Assert(A.Type == operand_register);
	Assert(B.Type == operand_register);
	PushByte(REX_W);
	PushByte(0x31);
	EncodeOperands(A, B);
}

void assembler::Call(operand Fn)
{
	Assert(Fn.Type == operand_register);
	PushByte(0xFF);
	PushByte((MOD_register << 6) | (0b010 << 3) | Fn.Register);
}

void assembler::Ret()
{
	PushByte(0xC3);
}

void assembler::EncodeOperands(operand Dst, operand Src)
{
	MOD mod = MOD_displacement_0;
	u8 reg = 0;
	u8 rm = 0;
	switch(Dst.Type)
	{
		case operand_register:
		{
			switch(Src.Type)
			{
				case operand_register:
				{
					mod = MOD_register;
					reg = Dst.Register;
					rm = Src.Register;
				} break;
				case operand_offset:
				{
					if(Src.Offset.Constant == 0)
						mod = MOD_displacement_0;
					else
						mod = MOD_displacement_i8;
					reg = Dst.Register;
					rm = Src.Offset.Register;
				} break;
				default: unreachable;
			}
			//00     001    011 mov [rbx], rcx
			//MOD 0  reg_c  reg_b
			//00     011    001 mov [rcx], rbx
			//MOD 0  reg_b  reg_c
		} break;
		case operand_offset:
		{
			switch(Src.Type)
			{
				case operand_register:
				{
					if(Dst.Offset.Constant == 0)
						mod = MOD_displacement_0;
					else
						mod = MOD_displacement_i8;
					reg = Src.Register;
					rm = Dst.Offset.Register;
				} break;
				default: unreachable;
			}
		} break;
		default: unreachable;
	}
	PushByte((mod << 6) | (reg << 3) | rm);
	if(Dst.Type == operand_offset && mod != MOD_displacement_0)
		PushByte(Dst.Offset.Constant);
	if(Src.Type == operand_offset && mod != MOD_displacement_0)
		PushByte(Src.Offset.Constant);
}

void assembler::Mov64(operand Dst, operand Src)
{
	switch(Dst.Type)
	{
		case operand_register:
		{
			switch(Src.Type)
			{
				case operand_register:
				case operand_offset:
				{
					PushByte(REX_W);
					PushByte(0x8B);
					EncodeOperands(Dst, Src);
				} break;
				case operand_constant:
				{
					Assert(false); // don't think it works
					PushByte(REX_W);
					PushByte(0xC7);
					EncodeOperands(Dst, Src);
				} break;
				default: unreachable;
			}
		} break;
		case operand_offset:
		{
			switch(Src.Type)
			{
				case operand_register:
				{
					PushByte(REX_W);
					PushByte(0x89);
					EncodeOperands(Dst, Src);
				} break;
				default: unreachable;
			}
		} break;
		default: unreachable;
	}
}

void assembler::Lea64(operand Dst, operand Src)
{
	switch(Dst.Type)
	{
		case operand_register:
		{
			switch(Src.Type)
			{
				case operand_offset:
				{
					PushByte(REX_W);
					PushByte(0x8D);
					EncodeOperands(Dst, Src);
				} break;
				default: unreachable;
			}
		} break;
		default: unreachable;
	}
}

