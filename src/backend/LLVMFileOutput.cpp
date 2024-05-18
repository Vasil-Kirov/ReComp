#include "LLVMFileOutput.h"
#include "../Type.h"
#include "../Platform.h"
#include "LLVMFileCast.h"
#include <unordered_map>

const char *GetLLVMTypeChar(const type *Type)
{
	if(Type->Kind == TypeKind_Basic && Type->Basic.Flags & BasicFlag_Boolean)
		return "i8";
	return GetTypeName(Type);
}

string GetLLVMType(const type *Type)
{
	return MakeString(GetLLVMTypeChar(Type));
}

void LLVMFileDumpBlock(string_builder *Builder, basic_block Block)
{

	std::unordered_map<u32, u64> ConstMap{};
	for(u32 i = 0; i < Block.InstructionCount; ++i)
	{
		instruction Instr = Block.Code[i];
		const type *Type = GetType(Instr.Type);
		Assert(Type->Kind == TypeKind_Basic);
		u32 Left = Instr.Left;
		u32 Right = Instr.Right;

		char LeftPrefix = '%';
		char RightPrefix = '%';
		if(ConstMap.find(Left) != ConstMap.end())
		{
			Left = ConstMap[Left];
			LeftPrefix = ' ';
		}
		if(ConstMap.find(Right) != ConstMap.end())
		{
			Right = ConstMap[Right];
			RightPrefix = ' ';
		}

		switch(Instr.Op)
		{
			case OP_NOP: {} break;
			case OP_CONST:
			{
				ConstMap.insert({Instr.Result, Instr.BigRegister});
			} break;
			case OP_ADD:
			{
				const char *op = "add";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "fadd";
				PushBuilderFormated(Builder, "%%%d = %s %s %c%d, %c%d", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_SUB:
			{
				const char *op = "sub";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "fsub";
				PushBuilderFormated(Builder, "%%%d = sub %s %c%d, %c%d", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_DIV:
			{
				const char *op = "sdiv";
				if(Type->Basic.Flags & BasicFlag_Unsigned)
					op = "udiv";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "fdiv";
				PushBuilderFormated(Builder, "%%%d = %s %s %c%d, %c%d", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_MUL:
			{
				PushBuilderFormated(Builder, "%%%d = mul %s %c%d, %c%d", Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_MOD:
			{
				const char *op = "srem";
				if(Type->Basic.Flags & BasicFlag_Unsigned)
					op = "urem";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "frem";
				PushBuilderFormated(Builder, "%%%d = %s %s %c%d, %c%d", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_LOAD:
			{
				PushBuilderFormated(Builder, "%%%d = load %s, ptr %%%llu", Instr.Result, GetLLVMTypeChar(Type), Instr.BigRegister);
			} break;
			case OP_ALLOC:
			{
				PushBuilderFormated(Builder, "%%%d = alloca %s", Instr.Result, GetLLVMTypeChar(Type));
			} break;
			case OP_STORE:
			{
				Assert(LeftPrefix == '%');
				PushBuilderFormated(Builder, "store %s %c%d, ptr %%%d", GetLLVMTypeChar(Type), RightPrefix, Right, Left);
			} break;
			case OP_CAST:
			{
				const type *FromType = GetType(Instr.Right);
				PushBuilderFormated(Builder, "%%%d = ", Instr.Result);
				LLVMCast(Builder, Left, LeftPrefix, FromType, Type);
			} break;
			case OP_IF:
			{
				u32 Result = Instr.Result;
				char ResultPrefix = '%';
				auto ResultConst = ConstMap.find(Result);
				if(ResultConst != ConstMap.end())
				{
					Result = ResultConst->second;
					ResultPrefix = ' ';
				}

				PushBuilderFormated(Builder, "br i1 %c%d, label block_%d, label block_%d ", ResultPrefix, Result, Instr.Left, Instr.Right);
			} break;
			case OP_RET:
			{
				if(Type)
				{
					PushBuilderFormated(Builder, "ret %s %c%d", GetLLVMTypeChar(Type), LeftPrefix, Left);
				}
				else
				{
					*Builder += "ret";
				}
			} break;
			//Assert(false);
		}
		PushBuilder(Builder, '\n');
	}
}


string LLVMFileDumpFunction(function Fn)
{
	string_builder Builder = MakeBuilder();
	const type *Type = GetType(GetType(Fn.Type)->Function.Return);
	// @TODO: args
	PushBuilderFormated(&Builder, "define %s @%s() ", GetLLVMTypeChar(Type), Fn.Name->Data);

	Builder += '{';
	Builder += '\n';
	for(int i = 0; i < Fn.BlockCount; ++i)
	{
		PushBuilderFormated(&Builder, "block_%d: \n", Fn.Blocks[i].ID);
		LLVMFileDumpBlock(&Builder, Fn.Blocks[i]);
	}
	Builder += '}';

	return MakeString(Builder);
}

void LLVMFileOutput(ir *IR)
{
	PlatformDeleteFile("llvm_output.bc");
	for(int i = 0; i < ArrLen(IR->Functions); ++i)
	{
		string String = LLVMFileDumpFunction(IR->Functions[i]);
		PlatformWriteFile("llvm_output.bc", (u8 *)String.Data, String.Size);
	}
	system("clang -Wno-override-module llvm_output.bc");
}

