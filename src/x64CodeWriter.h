#pragma once
#include <Basic.h>

enum register_
{
	reg_a  = 0b000,
	reg_c  = 0b001,
	reg_d  = 0b010,
	reg_b  = 0b011,
	reg_sp = 0b100,
	reg_bp = 0b101,
	reg_si = 0b110,
	reg_di = 0b111,
	reg_r8 = 8,
	reg_r9 = 9,
	reg_r10 = 10,
	reg_r11 = 11,
	reg_r12 = 12,
	reg_r13 = 13,
	reg_r14 = 14,
	reg_r15 = 15,
	// XMM registers are encoded
	// with the wrong value and are 
	// fixed during code generation so that
	// they can be distinct from other registers
	reg_xmm0 = 0 +   reg_r15 + 1,
	reg_xmm1 = 1 +   reg_r15 + 1,
	reg_xmm2 = 2 +   reg_r15 + 1,
	reg_xmm3 = 3 +   reg_r15 + 1,
	reg_xmm4 = 4 +   reg_r15 + 1,
	reg_xmm5 = 5 +   reg_r15 + 1,
	reg_xmm6 = 6 +   reg_r15 + 1,
	reg_xmm7 = 7 +   reg_r15 + 1,
	reg_xmm8 = 8 +   reg_r15 + 1,
	reg_xmm9 = 9 +   reg_r15 + 1,
	reg_xmm10 = 10 + reg_r15 + 1,
	reg_xmm11 = 11 + reg_r15 + 1,
	reg_xmm12 = 12 + reg_r15 + 1,
	reg_xmm13 = 13 + reg_r15 + 1,
	reg_xmm14 = 14 + reg_r15 + 1,
	reg_xmm15 = 15 + reg_r15 + 1,
	reg_invalid
};

enum MOD {
	MOD_displacement_0   = 0b00,
	MOD_displacement_i8  = 0b01,
	MOD_displacement_i32 = 0b10,
	MOD_register         = 0b11,
};

enum REX {
	REX_none = 0b0100'0000,
	REX_W    = 0b0100'1000,
	REX_R    = 0b0100'0100,
	REX_X    = 0b0100'0010,
	REX_B    = 0b0100'0001,
};

enum operand_type
{
	operand_register,
	operand_offset,
	operand_constant,
};

struct operand
{
	operand_type Type;
	union
	{
		register_ Register;
		struct {
			register_ Register;
			u8 Constant;
		} Offset;
		u32 Constant;
	};
	u8 GetRegisterEncoding();
};

struct assembler
{
	void *Code;
	uint CurrentOffset;
	uint Size;
	void Sub(operand Dst, operand Src);
	void Push(operand Op);
	void Pop(operand Op);
	void Ret();
	void Xor(operand A, operand B);
	void Lea64(operand Dst, operand Src);
	void Mov64(operand Dst, operand Src);
	void Call(operand Fn);
	void PushByte(u8 Byte);
	void PushU64(u64 QWORD);
	void PushU32(u32 QWORD);
	void EncodeOperands(operand A, operand B);
	void EncodePrefix(operand Dst, operand Src);
};

assembler MakeAssembler(uint MemorySize);
operand RegisterOperand(register_ Reg);
operand OffsetOperand(register_ Reg, u8 Offset);
operand ConstantOperand(u32 Constant);

