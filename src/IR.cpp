#include "IR.h"
#include "Memory.h"
#include "Parser.h"
#include "Type.h"
#include "vlib.h"
#include "Log.h"

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

// @TODO: no? like why no scope tracking
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
	LDEBUG("%s\n", Name->Data);
	Assert(false);
	return NULL;
}

u32 BuildIRFromAtom(block_builder *Builder, node *Node, b32 IsLHS)
{
	u32 Result = -1;
	switch(Node->Type)
	{
		case AST_CALL:
		{
			Assert(!IsLHS);
			call_info *CallInfo = NewType(call_info);

			if(Node->Call.SymName == NULL)
				CallInfo->Operand = BuildIRFromExpression(Builder, Node->Call.Fn, IsLHS);
			else
				CallInfo->FnName = Node->Call.SymName;

			dynamic<u32> Args{};
			for(int Idx = 0; Idx < Node->Call.Args.Count; ++Idx)
			{
				Args.Push(BuildIRFromExpression(Builder, Node->Call.Args[Idx], IsLHS));
			}

			CallInfo->Args = SliceFromArray(Args);

			Result = PushInstruction(Builder, Instruction(OP_CALL, (u64)CallInfo, Node->Call.Type, Builder));
		} break;
		case AST_ID:
		{
			const ir_local *Local = GetIRLocal(Builder->Function, Node->ID.Name);
			Result = Local->Register;

			// Do not load arguments, LLVM treats them as values rather than ptrs
			if(!IsLHS && (i32)Local->Register >= 0)
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Local->Register, Local->Type, Builder));
		} break;
		case AST_CONSTANT:
		{
			u32 Type = GetConstantType(Node->Constant.Value);
			instruction I;
			I = Instruction(OP_CONST, (u64)&Node->Constant.Value, Type, Builder);

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
					I = Instruction(OP_LOAD, 0, I.Result, I.Type, Builder);
				}
			} break;
			case T_GREAT:
			{
				I = Instruction(OP_GREAT, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case T_LESS:
			{
				I = Instruction(OP_LESS, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			case T_NEQ:
			case T_GEQ:
			case T_LEQ:
			case T_EQEQ:
			{
				op Op = (op)((-Node->Binary.Op - -T_NEQ) + OP_NEQ);
				I = Instruction(Op, Left, Right, Node->Binary.ExpressionType, Builder);
			} break;
			default:
			{
				LDEBUG("%c", (char)Node->Binary.Op);
				Assert(false);
			} break;
		};
		PushInstruction(Builder, I);
		return I.Result;
	}
	return BuildIRFromUnary(Builder, Node, IsLHS);
}

void BuildIRFromDecleration(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_DECL);
	u32 ExpressionRegister = BuildIRFromExpression(Builder, Node->Decl.Expression);
	u32 LocalRegister = PushInstruction(Builder, Instruction(OP_ALLOC, -1, Node->Decl.TypeIndex, Builder));
	PushInstruction(Builder, InstructionStore(LocalRegister, ExpressionRegister, Node->Decl.TypeIndex));
	PushIRLocal(Builder->Function, Node->Decl.ID->ID.Name, LocalRegister, Node->Decl.TypeIndex);
}

void Terminate(block_builder *Builder, basic_block *GoTo)
{
	Builder->CurrentBlock->HasTerminator = true;
	Builder->CurrentBlock = GoTo;
}

void BuildIRBody(dynamic<node *> &Body, block_builder *Block, basic_block *Then);

void BuildIRFunctionLevel(block_builder *Builder, node *Node)
{
	switch(Node->Type)
	{
		case AST_DECL:
		{
			BuildIRFromDecleration(Builder, Node);
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
			basic_block *ThenBlock = AllocateBlock(Builder);
			basic_block *ElseBlock = AllocateBlock(Builder);
			basic_block *EndBlock  = AllocateBlock(Builder);
			PushInstruction(Builder, Instruction(OP_IF, ThenBlock->ID, ElseBlock->ID, IfExpression, Basic_bool));
			Terminate(Builder, ThenBlock);
			BuildIRBody(Node->If.Body, Builder, EndBlock);

			Builder->CurrentBlock = ElseBlock;
			if(Node->If.Else.IsValid())
			{
				BuildIRBody(Node->If.Else, Builder, EndBlock);
			}
			else
			{
				PushInstruction(Builder, Instruction(OP_JMP, EndBlock->ID, Basic_type, Builder));
				Terminate(Builder, EndBlock);
			}
		} break;
		case AST_FOR:
		{
			basic_block *Cond  = AllocateBlock(Builder);
			basic_block *Then  = AllocateBlock(Builder);
			basic_block *Incr  = AllocateBlock(Builder);
			basic_block *End   = AllocateBlock(Builder);

			if(Node->For.Init)
				BuildIRFromDecleration(Builder, Node->For.Init);
			PushInstruction(Builder, Instruction(OP_JMP, Cond->ID, Basic_type, Builder));
			Terminate(Builder, Cond);

			if(Node->For.Expr)
			{
				u32 CondExpr = BuildIRFromExpression(Builder, Node->For.Expr);
				PushInstruction(Builder, Instruction(OP_IF, Then->ID, End->ID, CondExpr, Basic_bool));
			}
			else
			{
				PushInstruction(Builder, Instruction(OP_JMP, Then->ID, Basic_type, Builder));
			}
			Terminate(Builder, Then);
			BuildIRBody(Node->For.Body, Builder, Incr);

			if(Node->For.Incr)
				BuildIRFromExpression(Builder, Node->For.Incr);

			PushInstruction(Builder, Instruction(OP_JMP, Cond->ID, Basic_type, Builder));
			Terminate(Builder, End);
		} break;
		default:
		{
			BuildIRFromExpression(Builder, Node);
		} break;
	}
}

void BuildIRBody(dynamic<node *> &Body, block_builder *Builder, basic_block *Then)
{
	for(int Idx = 0; Idx < Body.Count; ++Idx)
	{
		BuildIRFunctionLevel(Builder, Body[Idx]);
	}
	if(!Builder->CurrentBlock->HasTerminator)
	{
		PushInstruction(Builder, Instruction(OP_JMP, Then->ID, Basic_type, Builder));
		Terminate(Builder, Then);
	}
	Builder->CurrentBlock = Then;
}

function BuildFunctionIR(dynamic<node *> &Body, const string *Name, u32 TypeIdx, slice<node *> &Args, node *Node)
{
	function Function;
	Function.Blocks = NULL;
	Function.BlockCount = 0;
	Function.Name = Name;
	Function.Type = TypeIdx;
	Function.FnNode = Node;
	if(Body.IsValid())
	{
		Function.Blocks = ArrCreate(basic_block);
		Function.Locals = (ir_local *)VAlloc(MB(1) * sizeof(ir_local));
		Function.LocalCount = 0;
		block_builder Builder;
		Builder.Function = &Function;
		Builder.CurrentBlock = AllocateBlock(&Builder);
		Builder.LastRegister = 0;

		const type *FnType = GetType(TypeIdx);

		// @NOTE: Push arguments as negative value registers so in the LLVM backend we can recognize them...
		ForArray(Idx, Args)
		{
			PushIRLocal(&Function, Args[Idx]->Decl.ID->ID.Name, -(Idx + 1), FnType->Function.Args[Idx]);
		}

		ForArray(Idx, Body)
		{
			BuildIRFunctionLevel(&Builder, Body[Idx]);
		}
		Function.LastRegister = Builder.LastRegister;
		VFree(Function.Locals);
		Function.Locals = NULL;
	}
	return Function;
}

function GlobalLevelIR(node *Node)
{
	function Result = {};
	switch(Node->Type)
	{
		case AST_FN:
		{
			Assert(Node->Fn.Name);

			Result = BuildFunctionIR(Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node);
		} break;
		default:
		{
			Assert(false);
		} break;
	}
	return Result;
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
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)Instr.BigRegister;
				PushBuilderFormated(Builder, "%%%d = CALL %s", Instr.Result, CallInfo->FnName->Data);
			} break;
			case OP_IF:
			{
				PushBuilderFormated(Builder, "IF %%%d goto %d, else goto %d", Instr.Result, Instr.Left, Instr.Right);
			} break;
			case OP_JMP:
			{
				PushBuilderFormated(Builder, "JMP %d", Instr.BigRegister);
			} break;

#define CASE_OP(op, str) \
			case op: \
			op_str = str; \
			goto INSIDE_EQ; \

			const char *op_str;
			CASE_OP(OP_NEQ,   "!=")
			CASE_OP(OP_GREAT, ">")
			CASE_OP(OP_GEQ,   ">=")
			CASE_OP(OP_LESS,  "<")
			CASE_OP(OP_LEQ,   "<=")
			CASE_OP(OP_EQEQ,  "==")
			goto INSIDE_EQ;
			{
INSIDE_EQ:
				PushBuilderFormated(Builder, "%%%d = CMP %%%d %s %%%d", Instr.Result, Instr.Left, op_str, Instr.Right);

			} break;
#undef CASE_OP
		}
		PushBuilder(Builder, '\n');
	}
	PushBuilder(Builder, '\n');
}

string Dissasemble(function *Functions, u32 FunctionCount)
{
	string_builder Builder = MakeBuilder();
	for(int FnIdx = 0; FnIdx < FunctionCount; ++FnIdx)
	{
		function *Fn = Functions + FnIdx;
		PushBuilderFormated(&Builder, "\nfn %s:{\n", Fn->Name->Data);
		for(int I = 0; I < Fn->BlockCount; ++I)
		{
			PushBuilderFormated(&Builder, "\n\tblock_%d:\n", I);
			DissasembleBasicBlock(&Builder, &Fn->Blocks[I]);
		}
		PushBuilder(&Builder, "}\n");
	}
	return MakeString(Builder);
}


