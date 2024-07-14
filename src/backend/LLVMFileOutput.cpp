#include "LLVMFileOutput.h"
#include "../Type.h"
#include "../Platform.h"
#include "LLVMFileCast.h"
#include <stdlib.h>
#include <unordered_map>

const char *GetLLVMTypeChar(const type *Type)
{
	if(Type->Kind == TypeKind_Basic)
	{
		if(Type->Basic.Flags & BasicFlag_Boolean)
			return "i8";
		else if(Type->Basic.Flags & BasicFlag_Float)
		{
			if(Type->Basic.Kind == Basic_f32)
				return "float";
			else
				return "double";
		}
	}
#if 0
	else if(IsUntyped(Type))
	{
		i32 RegisterSize = GetRegisterTypeSize();
		string_builder Builder = MakeBuilder();
		if(Type->Basic.Flags & BasicFlag_Float)
			PushBuilderFormated(&Builder, "f%d", RegisterSize);
		else
			PushBuilderFormated(&Builder, "i%d", RegisterSize);
		return MakeString(Builder).Data;
	}
#endif
	return GetTypeName(Type);
}

string GetLLVMType(const type *Type)
{
	return MakeString(GetLLVMTypeChar(Type));
}

void FindConst(std::unordered_map<u32, u64> &Map, char &OutPrefix, u32 &InOutVal)
{
	if(Map.find(InOutVal) != Map.end())
	{
		InOutVal = Map[InOutVal];
		OutPrefix = ' ';
	}
	else
	{
		OutPrefix = '%';
	}
}

constexpr int MAX_VAR_LEN = 64;

void LLVMGetRegister(const type *Type, i32 Val, std::unordered_map<u32, u64> &ConstMap, const function &CurrentFn, char *OutBuff, char &Prefix)
{
	if(Val >= 0)
	{
		u32 OutVal = (u32) Val;
		FindConst(ConstMap, Prefix, OutVal);
		if(Prefix == ' ' && Type->Kind == TypeKind_Basic && Type->Basic.Flags & BasicFlag_Float)
		{

		}
		else
		{
			_itoa_s(OutVal, OutBuff, MAX_VAR_LEN, 10);
		}
	}
	else
	{
		Prefix = '%';
		int LocalIndex = -(Val + 1);
		auto& Args = CurrentFn.FnNode->Fn.Args;
		// Probably a big register instruction... this is so scuffed
		if(LocalIndex >= Args.Count)
			return;
		const string *Name = Args[LocalIndex]->Decl.ID->ID.Name;
		Assert(Name->Size < 64);
		memcpy(OutBuff, Name->Data, Name->Size);
	}
}

void LLVMFileDumpBlock(string_builder *Builder, basic_block Block, const function &CurrentFn)
{
	std::unordered_map<u32, u64> ConstMap{};

	for(u32 i = 0; i < Block.InstructionCount; ++i)
	{
		instruction Instr = Block.Code[i];
		const type *Type = GetType(Instr.Type);
		i32 LeftInt = Instr.Left;
		i32 RightInt = Instr.Right;

		char LeftPrefix, RightPrefix;
		char Left[MAX_VAR_LEN] = {};
		char Right[MAX_VAR_LEN] = {};

		LLVMGetRegister(LeftInt,  ConstMap, CurrentFn, Left,  LeftPrefix);
		LLVMGetRegister(RightInt, ConstMap, CurrentFn, Right, RightPrefix);

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
				PushBuilderFormated(Builder, "%%%d = %s %s %c%s, %c%s", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_SUB:
			{
				const char *op = "sub";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "fsub";
				PushBuilderFormated(Builder, "%%%d = sub %s %c%s, %c%s", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_DIV:
			{
				const char *op = "sdiv";
				if(Type->Basic.Flags & BasicFlag_Unsigned)
					op = "udiv";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "fdiv";
				PushBuilderFormated(Builder, "%%%d = %s %s %c%s, %c%s", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_MUL:
			{
				PushBuilderFormated(Builder, "%%%d = mul %s %c%s, %c%s", Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_MOD:
			{
				const char *op = "srem";
				if(Type->Basic.Flags & BasicFlag_Unsigned)
					op = "urem";
				if(Type->Basic.Flags & BasicFlag_Float)
					op = "frem";
				PushBuilderFormated(Builder, "%%%d = %s %s %c%s, %c%s", Instr.Result, op, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)Instr.BigRegister;
				Assert(CallInfo->FnName);
				PushBuilderFormated(Builder, "%%%d = call %s @%s(", Instr.Result,
						GetLLVMTypeChar(GetType(GetReturnType(Type))),
						CallInfo->FnName->Data);

				for(int Idx = 0; Idx < Type->Function.ArgCount; ++Idx)
				{
					const type *ArgType = GetType(Type->Function.Args[Idx]);
					char Prefix;
					i32 Val = CallInfo->Args[Idx];
					char BUFF[MAX_VAR_LEN] = {};
					LLVMGetRegister(Val, ConstMap, CurrentFn, BUFF, Prefix);

					PushBuilderFormated(Builder, "%s %c%s", GetLLVMTypeChar(ArgType), Prefix, BUFF);
					if(Idx + 1 != Type->Function.ArgCount)
					{
						PushBuilder(Builder, ',');
						PushBuilder(Builder, ' ');
					}
				}

				PushBuilder(Builder, ')');
			} break;
			case OP_LOAD:
			{
				PushBuilderFormated(Builder, "%%%d = load %s, ptr %%%s", Instr.Result, GetLLVMTypeChar(Type), Right);
			} break;
			case OP_ALLOC:
			{
				PushBuilderFormated(Builder, "%%%d = alloca %s", Instr.Result, GetLLVMTypeChar(Type));
			} break;
			case OP_STORE:
			{
				Assert(LeftPrefix == '%');
				PushBuilderFormated(Builder, "store %s %c%s, ptr %%%s", GetLLVMTypeChar(Type), RightPrefix, Right, Left);
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

				PushBuilderFormated(Builder, "br i1 %c%d, label %%block_%d, label %%block_%d ", ResultPrefix, Result, Instr.Left, Instr.Right);
			} break;
			case OP_JMP:
			{
				PushBuilderFormated(Builder, "br label %%block_%d", Instr.BigRegister);
			} break;
			case OP_RET:
			{
				if(Type)
				{
					PushBuilderFormated(Builder, "ret %s %c%s", GetLLVMTypeChar(Type), LeftPrefix, Left);
				}
				else
				{
					*Builder += "ret";
				}
			} break;
			case OP_NEQ:
			{
				PushBuilderFormated(Builder, "%%%d = icmp ne %s %c%s, %c%s",
						Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
			case OP_GREAT:
			{
				if(Type->Basic.Flags & BasicFlag_Float)
				{
					PushBuilderFormated(Builder, "%%%d = fcmp ogt %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else if(Type->Basic.Flags & BasicFlag_Unsigned)
				{
					PushBuilderFormated(Builder, "%%%d = icmp ugt %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else
				{
					PushBuilderFormated(Builder, "%%%d = icmp sgt %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
			} break;
			case OP_GEQ:
			{
				if(Type->Basic.Flags & BasicFlag_Float)
				{
					PushBuilderFormated(Builder, "%%%d = fcmp oge %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else if(Type->Basic.Flags & BasicFlag_Unsigned)
				{
					PushBuilderFormated(Builder, "%%%d = icmp uge %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else
				{
					PushBuilderFormated(Builder, "%%%d = icmp sge %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
			} break;
			case OP_LESS:
			{
				if(Type->Basic.Flags & BasicFlag_Float)
				{
					PushBuilderFormated(Builder, "%%%d = fcmp olt %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else if(Type->Basic.Flags & BasicFlag_Unsigned)
				{
					PushBuilderFormated(Builder, "%%%d = icmp ult %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else
				{
					PushBuilderFormated(Builder, "%%%d = icmp slt %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
			} break;
			case OP_LEQ:
			{
				if(Type->Basic.Flags & BasicFlag_Float)
				{
					PushBuilderFormated(Builder, "%%%d = fcmp ole %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else if(Type->Basic.Flags & BasicFlag_Unsigned)
				{
					PushBuilderFormated(Builder, "%%%d = icmp ule %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
				else
				{
					PushBuilderFormated(Builder, "%%%d = icmp sle %s %c%s, %c%s",
							Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
				}
			} break;
			case OP_EQEQ:
			{
				PushBuilderFormated(Builder, "%%%d = icmp eq %s %c%s, %c%s",
						Instr.Result, GetLLVMTypeChar(Type), LeftPrefix, Left, RightPrefix, Right);
			} break;
		}
		if(Instr.Op != OP_CONST && Instr.Op != OP_NOP)
			PushBuilder(Builder, '\n');
	}
}

void LLVMFileWriteFunctionSignature(string_builder *Builder, const function &Fn)
{
	const type *FnType = GetType(Fn.Type);
	const type *RetType = GetType(FnType->Function.Return);
	PushBuilderFormated(Builder, "\ndefine %s @%s(", GetLLVMTypeChar(RetType), Fn.Name->Data);
	auto& Args = Fn.FnNode->Fn.Args;
	ForArray(Idx, Args)
	{
		const auto& Arg = Args[Idx];
		const string *Name = Arg->Decl.ID->ID.Name;
		const type *Type = GetType(FnType->Function.Args[Idx]);

		PushBuilderFormated(Builder, " %s %%%s", GetLLVMTypeChar(Type), Name->Data);
		if(Idx + 1 != Args.Count)
			PushBuilder(Builder, ',');
	}
	PushBuilder(Builder, " ) {\n");
}

string LLVMFileDumpFunction(function Fn)
{
	string_builder Builder = MakeBuilder();
	LLVMFileWriteFunctionSignature(&Builder, Fn);
	for(int i = 0; i < Fn.BlockCount; ++i)
	{
		PushBuilderFormated(&Builder, "\nblock_%d:\n", Fn.Blocks[i].ID);
		LLVMFileDumpBlock(&Builder, Fn.Blocks[i], Fn);
	}
	Builder += '}';
	Builder += '\n';

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

