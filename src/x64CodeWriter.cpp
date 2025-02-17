#include "x64CodeWriter.h"
#include "Log.h"
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
	return {operand_register, {.Register = Reg}};
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

void assembler::Peek(operand Register)
{
	// @NOTE: Hard code it since there is no support for SIB bytes currently
	Assert(Register.Type == operand_register);
	Mov64(Register, OffsetOperand(reg_sp, 0));
	PushByte(0x24);
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

void assembler::Add(operand Dst, operand Src)
{
	Assert(Dst.Type == operand_register);
	Assert(Src.Type == operand_constant);
	PushByte(REX_W);
	PushByte(0x81);
	PushByte((MOD_register << 6) | (0 << 3) | Dst.Register);
	PushU32(Src.Constant);
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

	if(Fn.Register >= reg_r8)
	{
		PushByte(0x41);
		Fn.Register = (register_)(Fn.Register - reg_r8);
	}
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
					reg = Src.GetRegisterEncoding();
					rm = Dst.GetRegisterEncoding();
				} break;
				case operand_offset:
				{
					if(Src.Offset.Constant == 0)
						mod = MOD_displacement_0;
					else
						mod = MOD_displacement_i8;
					reg = Dst.GetRegisterEncoding();
					rm = Src.GetRegisterEncoding();
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
					reg = Src.GetRegisterEncoding();
					rm = Dst.GetRegisterEncoding();
				} break;
				default: unreachable;
			}
		} break;
		default: unreachable;
	}
	PushByte((mod << 6) | (reg << 3) | rm);

	if(rm == 0b100 && (mod == MOD_displacement_0 || mod == MOD_displacement_i8 || mod == MOD_displacement_i32))
	{
		// SIB for rsp
		PushByte(0b00100100);
	}

	if(Dst.Type == operand_offset && mod != MOD_displacement_0)
		PushByte(Dst.Offset.Constant);
	if(Src.Type == operand_offset && mod != MOD_displacement_0)
		PushByte(Src.Offset.Constant);
}

void assembler::EncodePrefix(operand Dst, operand Src)
{
	u8 Byte = REX_W;
	switch(Dst.Type)
	{
		case operand_register:
		{
			switch(Src.Type)
			{
				case operand_register:
				{
					if(Dst.Register >= reg_r8)
						Byte |= REX_B;
					if(Src.Register >= reg_r8)
						Byte |= REX_R;
				} break;
				case operand_offset:
				{
					if(Dst.Register >= reg_r8)
						Byte |= REX_R;
					if(Src.Register >= reg_r8)
						Byte |= REX_B;
				} break;
				case operand_constant:
				{
					if(Dst.Register >= reg_r8)
						Byte |= REX_B;
				} break;
			}
		} break;
		case operand_offset:
		{
			if(Src.Type == operand_register)
			{
				if(Dst.Register >= reg_r8)
					Byte |= REX_B;
				if(Src.Register >= reg_r8)
					Byte |= REX_R;
			}
			else
			{
				unreachable;
			}
		} break;
		default: unreachable;
	}
	PushByte(Byte);
}

void assembler::Mov64(operand Dst, operand Src)
{
	switch(Dst.Type)
	{
		case operand_register:
		{
			switch(Src.Type)
			{
				case operand_offset:
				{
					EncodePrefix(Dst, Src);
					PushByte(0x8B);
					EncodeOperands(Dst, Src);
				} break;

				case operand_register:
				{
					EncodePrefix(Dst, Src);
					PushByte(0x89);
					EncodeOperands(Dst, Src);
				} break;
				case operand_constant:
				{
					Assert(false); // don't think it works
					EncodePrefix(Dst, Src);
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
					EncodePrefix(Dst, Src);
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
					EncodePrefix(Dst, Src);
					PushByte(0x8D);
					EncodeOperands(Dst, Src);
				} break;
				default: unreachable;
			}
		} break;
		default: unreachable;
	}
}

u8 operand::GetRegisterEncoding()
{
	switch(this->Type)
	{
		case operand_register:
		{
			u8 Byte = Register;
			if(Byte >= reg_r8)
				Byte -= reg_r8;
			return Byte;
		} break;
		case operand_offset:
		{
			u8 Byte = Offset.Register;
			if(Byte >= reg_r8)
				Byte -= reg_r8;
			return Byte;
		} break;
		default: unreachable;
	}
}

