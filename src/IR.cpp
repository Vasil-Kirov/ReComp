#include "IR.h"

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

void AddInstruction(block_builder *Builder, instruction I)
{
	ArrPush(Builder->CurrentBlock->Code, I);
	Builder->CurrentBlock->InstructionCount++;
}

basic_block *AllocateBlock(block_builder *Builder)
{
	basic_block Block;
	Block.Code = ArrCreate(instruction);
	Block.InstructionCount = 0;

	function *Function = Builder->Function;
	ArrPush(Function->Blocks, Block);
	return &Function->Blocks[Function->BlockCount++];
}

void PushIRLocal(function *Function, const string *Name, u32 Register)
{
	ir_local Local;
	Local.Register = Register;
	Local.Name = Name;
	Function->Locals[Function->LocalCount++] = Local;
}

u32 GetIRLocal(function *Function, const string *Name)
{
	for(int I = 0; I < Function->LocalCount; ++I)
	{
		if(*Function->Locals[I].Name == *Name)
		{
			return Function->Locals[I].Register;
		}
	}
	Assert(false);
	return NULL;
}

u32 BuildIRFromAtom(block_builder *Builder, node *Node)
{
	switch(Node->Type)
	{
		case AST_ID:
		{
			return GetIRLocal(Builder->Function, Node->ID.Name);
		} break;
		default:
		{
			Assert(false);
		} break;
	}
}

u32 BuildIRFromUnary(block_builder *Builder, node *Node)
{
	switch(Node->Type)
	{
		default:
		{
			return BuildIRFromAtom(Builder, Node);
		} break;
	}
}

u32 BuildIRFromExpression(block_builder *Builder, node *Node)
{
	if(Node->Type == AST_BINARY)
	{
		u32 Left = BuildIRFromExpression(Builder, Node->Binary.Left);
		u32 Right = BuildIRFromExpression(Builder, Node->Binary.Right);
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
			default:
			{
				Assert(false);
			} break;
		};
		AddInstruction(Builder, I);
		return I.Result;
	}
	return BuildIRFromUnary(Builder, Node);
}

void BuildIRFunctionLevel(block_builder *Builder, node *Node)
{
	switch(Node->Type)
	{
		case AST_DECL:
		{
			u32 LocalRegister = BuildIRFromExpression(Builder, Node->Decl.Expression);
			PushIRLocal(Builder->Function, Node->Decl.ID->ID.Name, LocalRegister);
		} break;
		default:
		{
			BuildIRFromExpression(Builder, Node);
		} break;
	}
}

function BuildFunctionIR(node **Body, const string *Name)
{
	function Function;
	Function.Blocks = NULL;
	Function.BlockCount = 0;
	Function.Name = Name;
	if(Body)
	{
		Function.Blocks = ArrCreate(basic_block);
		Function.Locals = (ir_local *)VAlloc(MB(1) * sizeof(ir_local));
		Function.LocalCount = 0;
		block_builder Builder;
		Builder.Function = &Function;
		Builder.CurrentBlock = AllocateBlock(&Builder);
		Builder.LastRegister = 0;

		int BodyCount = ArrLen(Body);
		for(int I = 0; I < BodyCount; ++I)
		{
			BuildIRFunctionLevel(&Builder, Body[I]);
		}
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
				return BuildFunctionIR(Node->Decl.Expression->Fn.Body, Name);
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




