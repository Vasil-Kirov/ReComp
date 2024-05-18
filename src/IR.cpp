#include "IR.h"
#include "Parser.h"
#include "Type.h"

inline instruction Instruction(op Op, u64 Val, u32 Type, block_builder *Builder)
{
	instruction Result;
	Result.BigRegister = Val;
	Result.Op = Op;
	Result.Type = Type;
	Result.Result = Builder->LastRegister++;
	return Result;
}

inline instruction InstructionStore(u32 Left, u32 Right, u32 Type)
{
	instruction Result;
	Result.Left = Left;
	Result.Right = Right;
	Result.Op = OP_STORE;
	Result.Type = Type;
	Result.Result = Left;
	return Result;
}

inline instruction Instruction(op Op, u32 Left, u32 Right, u32 ResultRegister, u32 Type)
{
	instruction Result;
	Result.Left = Left;
	Result.Right = Right;
	Result.Op = Op;
	Result.Type = Type;
	Result.Result = ResultRegister;
	return Result;
}

inline instruction Instruction(op Op, u32 Left, u32 Right, u32 Type, block_builder *Builder)
{
	instruction Result;
	Result.Left = Left;
	Result.Right = Right;
	Result.Op = Op;
	Result.Type = Type;
	Result.Result = Builder->LastRegister++;
	return Result;
}

u32 PushInstruction(block_builder *Builder, instruction I)
{
	Assert(!Builder->CurrentBlock->HasTerminator);
	ArrPush(Builder->CurrentBlock->Code, I);
	Builder->CurrentBlock->InstructionCount++;
	return I.Result;
}

basic_block *AllocateBlock(block_builder *Builder)
{
	basic_block Block;
	Block.Code = ArrCreate(instruction);
	Block.InstructionCount = 0;
	Block.ID = Builder->Function->BlockCount;
	Block.HasTerminator = false;

	ArrPush(Builder->Function->Blocks, Block);
	return &Builder->Function->Blocks[Builder->Function->BlockCount++];
}

void PushIRLocal(function *Function, const string *Name, u32 Register, u32 Type)
{
	ir_local Local;
	Local.Register = Register;
	Local.Name = Name;
	Local.Type = Type;
	Function->Locals[Function->LocalCount++] = Local;
}

const ir_local *GetIRLocal(function *Function, const string *Name)
{
	for(int I = 0; I < Function->LocalCount; ++I)
	{
		if(*Function->Locals[I].Name == *Name)
		{
			return &Function->Locals[I];
		}
	}
	Assert(false);
	return NULL;
}

u32 BuildIRFromAtom(block_builder *Builder, node *Node, b32 IsLHS)
{
	u32 Result = -1;
	switch(Node->Type)
	{
		case AST_ID:
		{
			const ir_local *Local = GetIRLocal(Builder->Function, Node->ID.Name);
			Result = Local->Register;
			if(!IsLHS)
				Result = PushInstruction(Builder, Instruction(OP_LOAD, Local->Register, Local->Type, Builder));
		} break;
		case AST_NUMBER:
		{
			u32 Type = Node->Number.IsFloat ? Basic_UntypedFloat : Basic_UntypedInteger;
			instruction I = Instruction(OP_CONST, Node->Number.Bytes, Type, Builder);
			Result = PushInstruction(Builder, I);
		} break;
		case AST_CAST:
		{
			u32 Expression = BuildIRFromExpression(Builder, Node->Cast.Expression, false);
			instruction I = Instruction(OP_CAST, Expression, Node->Cast.FromType, Node->Cast.ToType, Builder);
			Result = PushInstruction(Builder, I);
		} break;
		default:
		{
			Assert(false);
		} break;
	}
	return Result;
}

u32 BuildIRFromUnary(block_builder *Builder, node *Node, b32 IsLHS)
{
	u32 Result = -1;
	switch(Node->Type)
	{
		default:
		{
			Result = BuildIRFromAtom(Builder, Node, IsLHS);
		} break;
	}
	return Result;
}

u32 BuildIRFromExpression(block_builder *Builder, node *Node, b32 IsLHS = false)
{
	if(Node->Type == AST_BINARY)
	{
		b32 IsLeftLHS = IsLHS;
		b32 IsRightLHS = IsLHS;
		if(Node->Binary.Op == T_EQ)
		{
			IsLeftLHS = true;
			IsRightLHS = false;
		}
		u32 Left = BuildIRFromExpression(Builder, Node->Binary.Left, IsLeftLHS);
		u32 Right = BuildIRFromExpression(Builder, Node->Binary.Right, IsRightLHS);

		instruction I;
		switch((int)Node->Binary.Op)
		{
			case '+':
			{
				I = Instruction(OP_ADD, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case '-':
			{
				I = Instruction(OP_SUB, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case '*':
			{
				I = Instruction(OP_MUL, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case '/':
			{
				I = Instruction(OP_DIV, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case '%':
			{
				I = Instruction(OP_DIV, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case '=':
			{
				I = InstructionStore(Left, Right, Node->Binary.ExpressionType);
				if(!IsLHS)
				{
					PushInstruction(Builder, I);
					I = Instruction(OP_LOAD, I.Result, I.Type, Builder);
				}
			} break;
			default:
			{
				Assert(false);
			} break;
		};
		PushInstruction(Builder, I);
		return I.Result;
	}
	return BuildIRFromUnary(Builder, Node, IsLHS);
}

void Terminate(block_builder *Builder, basic_block *GoTo)
{
	Builder->CurrentBlock->HasTerminator = true;
	Builder->CurrentBlock = GoTo;
}

void BuildIRFunctionLevel(block_builder *Builder, node *Node)
{
	switch(Node->Type)
	{
		case AST_DECL:
		{
			u32 ExpressionRegister = BuildIRFromExpression(Builder, Node->Decl.Expression);
			u32 LocalRegister = PushInstruction(Builder, Instruction(OP_ALLOC, -1, Node->Decl.TypeIndex, Builder));
			PushInstruction(Builder, InstructionStore(LocalRegister, ExpressionRegister, Node->Decl.TypeIndex));
			PushIRLocal(Builder->Function, Node->Decl.ID->ID.Name, LocalRegister, Node->Decl.TypeIndex);
		} break;
		case AST_RETURN:
		{
			u32 Expression = BuildIRFromExpression(Builder, Node->Return.Expression);
			PushInstruction(Builder, Instruction(OP_RET, Expression, 0, Node->Return.TypeIdx, Builder));
			Builder->CurrentBlock->HasTerminator = true;
		} break;
		case AST_IF:
		{
			u32 IfExpression = BuildIRFromExpression(Builder, Node->If.Expression);
			basic_block *IfBlock    = AllocateBlock(Builder);
			basic_block *AfterBlock = AllocateBlock(Builder);
			PushInstruction(Builder, Instruction(OP_IF, IfBlock->ID, AfterBlock->ID, IfExpression, Basic_bool));
			Terminate(Builder, IfBlock);
			for(int Idx = 0; Idx < Node->If.Body.Count; ++Idx)
			{
				BuildIRFunctionLevel(Builder, Node->If.Body[Idx]);
			}
			Terminate(Builder, AfterBlock);
		} break;
		default:
		{
			BuildIRFromExpression(Builder, Node);
		} break;
	}
}

function BuildFunctionIR(dynamic<node *> &Body, const string *Name)
{
	function Function;
	Function.Blocks = NULL;
	Function.BlockCount = 0;
	Function.Name = Name;
	if(Body.IsValid())
	{
		Function.Blocks = ArrCreate(basic_block);
		Function.Locals = (ir_local *)VAlloc(MB(1) * sizeof(ir_local));
		Function.LocalCount = 0;
		block_builder Builder;
		Builder.Function = &Function;
		Builder.CurrentBlock = AllocateBlock(&Builder);
		Builder.LastRegister = 0;

		for(int I = 0; I < Body.Count; ++I)
		{
			BuildIRFunctionLevel(&Builder, Body[I]);
		}
		Function.LastRegister = Builder.LastRegister;
		VFree(Function.Locals);
	}
	return Function;
}

function GlobalLevelIR(node *Node)
{
	switch(Node->Type)
	{
		case AST_DECL:
		{
			if(Node->Decl.Expression->Type == AST_FN)
			{
				Assert(Node->Decl.ID->Type == AST_ID);
				Assert(Node->Decl.IsConst);
				const string *Name = Node->Decl.ID->ID.Name;

				function Fn = BuildFunctionIR(Node->Decl.Expression->Fn.Body, Name);
				Fn.Type = Node->Decl.Expression->Fn.TypeIdx;
				return Fn;
			}
		} break;
		default:
		{
			Assert(false);
		} break;
	}
	return {};
}

ir BuildIR(node **Nodes)
{
	ir IR;
	IR.Functions = ArrCreate(function);
	u32 NodeCount = ArrLen(Nodes);
	for(int I = 0; I < NodeCount; ++I)
	{
		function MaybeFunction = GlobalLevelIR(Nodes[I]);
		if(MaybeFunction.Name)
			ArrPush(IR.Functions, MaybeFunction);
	}
	return IR;
}

void DissasembleBasicBlock(string_builder *Builder, basic_block *Block)
{
	for(int I = 0; I < Block->InstructionCount; ++I)
	{
		instruction Instr = Block->Code[I];
		const type *Type = NULL;
		if(Instr.Type != INVALID_TYPE)
			Type = GetType(Instr.Type);

		PushBuilder(Builder, '\t');
		switch(Instr.Op)
		{
			case OP_NOP:
			{
				PushBuilder(Builder, "NOP");
			} break;
			case OP_CONST:
			{
				PushBuilderFormated(Builder, "%%%d = %s %d", Instr.Result, GetTypeName(Type), Instr.BigRegister);
			} break;
			case OP_ADD:
			{
				PushBuilderFormated(Builder, "%%%d = %s %%%d + %%%d", Instr.Result, GetTypeName(Type),
						Instr.Left, Instr.Right);
			} break;
			case OP_SUB:
			{
				PushBuilderFormated(Builder, "%%%d = %s %%%d - %%%d", Instr.Result, GetTypeName(Type),
						Instr.Left, Instr.Right);
			} break;
			case OP_MUL:
			{
				PushBuilderFormated(Builder, "%%%d = %s %%%d * %%%d", Instr.Result, GetTypeName(Type),
						Instr.Left, Instr.Right);
			} break;
			case OP_DIV:
			{
				PushBuilderFormated(Builder, "%%%d = %s %%%d / %%%d", Instr.Result, GetTypeName(Type),
						Instr.Left, Instr.Right);
			} break;
			case OP_MOD:
			{
				PushBuilderFormated(Builder, "%%%d = %s %%%d % %%%d", Instr.Result, GetTypeName(Type),
						Instr.Left, Instr.Right);
			} break;
			case OP_LOAD:
			{
				PushBuilderFormated(Builder, "%%%d = LOAD %s %%%d", Instr.Result, GetTypeName(Type),
						Instr.BigRegister);
			} break;
			case OP_STORE:
			{
				PushBuilderFormated(Builder, "%%%d = STORE %s %%%d", Instr.Result, GetTypeName(Type),
						Instr.Right);
			} break;
			case OP_CAST:
			{
				const type *FromType = GetType(Instr.Right);
				PushBuilderFormated(Builder, "%%%d = CAST %s to %s %%%d", Instr.Result, GetTypeName(FromType), GetTypeName(Type), Instr.Left);
			} break;
			case OP_ALLOC:
			{
				PushBuilderFormated(Builder, "%%%d = ALLOC %s", Instr.Result, GetTypeName(Type));
			} break;
			case OP_RET:
			{
				PushBuilderFormated(Builder, "RET %s %%%d", GetTypeName(Type), Instr.Left);
			} break;
			case OP_IF:
			{
				PushBuilderFormated(Builder, "IF %%%d goto %d, after goto %d", Instr.Result, Instr.Left, Instr.Right);
			} break;
		}
		PushBuilder(Builder, '\n');
	}
	PushBuilder(Builder, '\n');
}

string Dissasemble(function *Fn)
{
	string_builder Builder = MakeBuilder();
	PushBuilderFormated(&Builder, "\nfn %s:\n", Fn->Name->Data);
	for(int I = 0; I < Fn->BlockCount; ++I)
	{
		DissasembleBasicBlock(&Builder, &Fn->Blocks[I]);
	}
	return MakeString(Builder);
}


