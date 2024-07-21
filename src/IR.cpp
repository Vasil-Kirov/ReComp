#include "IR.h"
#include "ConstVal.h"
#include "Dynamic.h"
#include "Memory.h"
#include "Parser.h"
#include "Type.h"
#include "VString.h"
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

inline instruction InstructionMemset(u32 Ptr, u32 Type)
{
	instruction Result;
	Result.Left  = 0;
	Result.Right = Ptr;
	Result.Op = OP_MEMSET;
	Result.Type = Type;
	Result.Result = Ptr;
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
	Assert(!Builder->CurrentBlock.HasTerminator);
	Builder->CurrentBlock.Code.Push(I);
	return I.Result;
}

basic_block AllocateBlock(block_builder *Builder)
{
	basic_block Block = {};
	Block.ID = Builder->LastBlock++;
	Block.HasTerminator = false;

	return Block;
}

// @TODO: no? like why no scope tracking
void PushIRLocal(function *Function, const string *Name, u32 Register, u32 Type, b32 IsArg = false)
{
	ir_symbol Local;
	Local.Register = Register;
	Local.Name  = Name;
	Local.Type  = Type;
	Local.IsArg = IsArg;
	Function->Locals[Function->LocalCount++] = Local;
}

// @TODO: This is bad
const ir_symbol *GetIRLocal(function *Function, const string *Name)
{
	ir_symbol *Found = NULL;
	for(int I = 0; I < Function->LocalCount; ++I)
	{
		if(*Function->Locals[I].Name == *Name)
		{
			Found = &Function->Locals[I];
		}
	}
	if(!Found)
	{
		LDEBUG("%s\n", Name->Data);
		Assert(false);
	}
	return Found;
}

u32 BuildIRFromAtom(block_builder *Builder, node *Node, b32 IsLHS)
{
	u32 Result = -1;
	switch(Node->Type)
	{
		case AST_ID:
		{
			const ir_symbol *Local = GetIRLocal(Builder->Function, Node->ID.Name);
			Result = Local->Register;

			b32 ShouldLoad = true;
			if(IsLHS || Local->IsArg)
				ShouldLoad = false;

			const type *Type = GetType(Local->Type);
			if(Type->Kind != TypeKind_Basic && Type->Kind != TypeKind_Pointer)
			{
				ShouldLoad = false;
			}
			else
			{
				if(Type->Kind == TypeKind_Basic && Type->Basic.Kind == Basic_string)
					ShouldLoad = false;
			}
			
			if(ShouldLoad)
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Local->Register, Local->Type, Builder));
		} break;
		case AST_CALL:
		{
			Assert(!IsLHS);
			call_info *CallInfo = NewType(call_info);

			CallInfo->Operand = BuildIRFromExpression(Builder, Node->Call.Fn, IsLHS);

			dynamic<u32> Args{};
			for(int Idx = 0; Idx < Node->Call.Args.Count; ++Idx)
			{
				Args.Push(BuildIRFromExpression(Builder, Node->Call.Args[Idx], IsLHS));
			}

			CallInfo->Args = SliceFromArray(Args);

			Result = PushInstruction(Builder, Instruction(OP_CALL, (u64)CallInfo, Node->Call.Type, Builder));
		} break;
		case AST_ARRAYLIST:
		{
			Assert(!IsLHS);
			if(Node->ArrayList.IsEmpty)
			{
				Result = PushInstruction(Builder,
						Instruction(OP_ALLOC, -1, Node->ArrayList.Type, Builder));

				PushInstruction(Builder, InstructionMemset(Result, Node->ArrayList.Type));
			}
			else
			{
				u32 *Registers = (u32 *)VAlloc((Node->ArrayList.Expressions.Count + 1) * sizeof(u32));
				ForArray(Idx, Node->ArrayList.Expressions)
				{
					u32 Register = BuildIRFromExpression(Builder, Node->ArrayList.Expressions[Idx], IsLHS);
					Registers[Idx] = Register;
				}
				Registers[Node->ArrayList.Expressions.Count] = -1;
				instruction I = Instruction(OP_ARRAYLIST, (u64)Registers, Node->ArrayList.Type, Builder);
				Result = PushInstruction(Builder, I);
			}
		} break;
		case AST_STRUCTLIST:
		{
			Assert(!IsLHS);
			const type *StructType = GetType(Node->StructList.Type);

			u32 Alloc = PushInstruction(Builder, Instruction(OP_ALLOC, -1, Node->StructList.Type, Builder));

			ForArray(Idx, Node->StructList.Expressions)
			{
				u32 MemberIndex = Node->StructList.NameIndexes[Idx];
				u32 Expr = BuildIRFromExpression(Builder, Node->StructList.Expressions[Idx], false);
				u32 Location = PushInstruction(Builder,
						Instruction(OP_INDEX, Alloc, MemberIndex, Node->StructList.Type, Builder));
				PushInstruction(Builder,
						InstructionStore(Location, Expr, StructType->Struct.Members[MemberIndex].Type));
			}

			VFree(Node->StructList.NameIndexes.Data);
			Node->StructList.NameIndexes.Data = NULL;
			Result = Alloc;
		} break;
		case AST_INDEX:
		{
			b32 ShouldNotLoad = IsLHS;
			if(GetType(Node->Index.OperandType)->Kind == TypeKind_Pointer)
				ShouldNotLoad = false;

			u32 Operand = BuildIRFromExpression(Builder, Node->Index.Operand, ShouldNotLoad);
			u32 Index = BuildIRFromExpression(Builder, Node->Index.Expression, false);
			Result = PushInstruction(Builder, Instruction(OP_INDEX, Operand, Index, Node->Index.OperandType, Builder));
			if(!IsLHS && !Node->Index.ForceNotLoad)
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Node->Index.IndexedType, Builder));
		} break;
		case AST_SELECTOR:
		{
			u32 Operand = BuildIRFromExpression(Builder, Node->Selector.Operand, true);

			Result = PushInstruction(Builder, 
					Instruction(OP_INDEX, Operand, Node->Selector.Index, Node->Selector.Type, Builder));

			const type *Type = GetType(Node->Selector.Type);
			u32 MemberTypeIdx = Type->Struct.Members[Node->Selector.Index].Type;
			const type *MemberType = GetType(MemberTypeIdx);
			if(!IsLHS && IsLoadableType(MemberType))
			{
				Result = PushInstruction(Builder,
						Instruction(OP_LOAD, 0, Result, Type->Struct.Members[Node->Selector.Index].Type, Builder));
			}

		} break;
		case AST_CONSTANT:
		{
			instruction I;
			I = Instruction(OP_CONST, (u64)&Node->Constant.Value, Node->Constant.Type, Builder);

			Result = PushInstruction(Builder, I);
		} break;
		case AST_CAST:
		{
			u32 Expression = BuildIRFromExpression(Builder, Node->Cast.Expression, IsLHS);
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
		case AST_UNARY:
		{
			switch(Node->Unary.Op)
			{
				case T_PTR:
				{
					Result = BuildIRFromExpression(Builder, Node->Unary.Operand, false);
					if(!IsLHS)
					{
						if(IsLoadableType(Node->Unary.Type))
						{
							instruction I = Instruction(OP_LOAD, 0, Result, Node->Unary.Type, Builder);
							Result = PushInstruction(Builder, I);
						}
					}
				} break;
				case T_ADDROF:
				{
					Assert(!IsLHS);
					Result = BuildIRFromExpression(Builder, Node->Unary.Operand, true);
				} break;
				default: unreachable;
			}
		} break;
		default:
		{
			Result = BuildIRFromAtom(Builder, Node, IsLHS);
		} break;
	}
	return Result;
}

u32 BuildIRFromExpression(block_builder *Builder, node *Node, b32 IsLHS, b32 NeedResult)
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
				if(NeedResult && !IsLHS && IsLoadableType(Node->Binary.ExpressionType))
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
	const type *Type = GetType(Node->Decl.TypeIndex);
	if(ShouldCopyType(Type))
	{
		u32 LocalRegister = PushInstruction(Builder, Instruction(OP_ALLOC, -1, Node->Decl.TypeIndex, Builder));
		PushInstruction(Builder, InstructionStore(LocalRegister, ExpressionRegister, Node->Decl.TypeIndex));
		PushIRLocal(Builder->Function, Node->Decl.ID, LocalRegister, Node->Decl.TypeIndex);
	}
	else
	{
		PushIRLocal(Builder->Function, Node->Decl.ID, ExpressionRegister, Node->Decl.TypeIndex);
	}
}

void Terminate(block_builder *Builder, basic_block GoTo)
{
	Builder->CurrentBlock.HasTerminator = true;
	Builder->Function->Blocks.Push(Builder->CurrentBlock);
	Builder->CurrentBlock = GoTo;
}

void BuildIRBody(dynamic<node *> &Body, block_builder *Block, basic_block Then);

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
			u32 Expression = -1;
			if(Node->Return.Expression)
					Expression = BuildIRFromExpression(Builder, Node->Return.Expression);
			PushInstruction(Builder, Instruction(OP_RET, Expression, 0, Node->Return.TypeIdx, Builder));
			Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_IF:
		{
			u32 IfExpression = BuildIRFromExpression(Builder, Node->If.Expression);
			basic_block ThenBlock = AllocateBlock(Builder);
			basic_block ElseBlock = AllocateBlock(Builder);
			basic_block EndBlock  = AllocateBlock(Builder);
			PushInstruction(Builder, Instruction(OP_IF, ThenBlock.ID, ElseBlock.ID, IfExpression, Basic_bool));
			Terminate(Builder, ThenBlock);
			BuildIRBody(Node->If.Body, Builder, EndBlock);

			Builder->CurrentBlock = ElseBlock;
			if(Node->If.Else.IsValid())
			{
				BuildIRBody(Node->If.Else, Builder, EndBlock);
			}
			else
			{
				PushInstruction(Builder, Instruction(OP_JMP, EndBlock.ID, Basic_type, Builder));
				Terminate(Builder, EndBlock);
			}
		} break;
		case AST_FOR:
		{
			basic_block Cond  = AllocateBlock(Builder);
			basic_block Then  = AllocateBlock(Builder);
			basic_block Incr  = AllocateBlock(Builder);
			basic_block End   = AllocateBlock(Builder);

			if(Node->For.Init)
				BuildIRFromDecleration(Builder, Node->For.Init);
			PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
			Terminate(Builder, Cond);

			if(Node->For.Expr)
			{
				u32 CondExpr = BuildIRFromExpression(Builder, Node->For.Expr);
				PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, CondExpr, Basic_bool));
			}
			else
			{
				PushInstruction(Builder, Instruction(OP_JMP, Then.ID, Basic_type, Builder));
			}
			Terminate(Builder, Then);
			BuildIRBody(Node->For.Body, Builder, Incr);

			if(Node->For.Incr)
				BuildIRFromExpression(Builder, Node->For.Incr);

			PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
			Terminate(Builder, End);
		} break;
		default:
		{
			BuildIRFromExpression(Builder, Node, false, false);
		} break;
	}
}

void BuildIRBody(dynamic<node *> &Body, block_builder *Builder, basic_block Then)
{
	for(int Idx = 0; Idx < Body.Count; ++Idx)
	{
		BuildIRFunctionLevel(Builder, Body[Idx]);
	}
	if(!Builder->CurrentBlock.HasTerminator)
	{
		PushInstruction(Builder, Instruction(OP_JMP, Then.ID, Basic_type, Builder));
	}
	Terminate(Builder, Then);
	Builder->CurrentBlock = Then;
}

function BuildFunctionIR(dynamic<node *> &Body, const string *Name, u32 TypeIdx, slice<node *> &Args, node *Node,
		slice<ir_symbol> ModuleSymbols)
{
	function Function = {};
	Function.Name = Name;
	Function.Type = TypeIdx;
	Function.FnNode = Node;
	Function.ModuleSymbols = ModuleSymbols;
	if(Body.IsValid())
	{
		Function.Locals = (ir_symbol *)VAlloc(MB(1) * sizeof(ir_symbol));
		Function.LocalCount = 0;
		block_builder Builder = {};
		Builder.Function = &Function;
		Builder.CurrentBlock = AllocateBlock(&Builder);

		const type *FnType = GetType(TypeIdx);

		ForArray(Idx, ModuleSymbols)
		{
			PushIRLocal(&Function, ModuleSymbols[Idx].Name, ModuleSymbols[Idx].Register,
					ModuleSymbols[Idx].Type, true);
		}
		Builder.LastRegister = ModuleSymbols.Count;

		ForArray(Idx, Args)
		{
			u32 Register = PushInstruction(&Builder,
					Instruction(OP_ARG, Idx, FnType->Function.Args[Idx], &Builder));
			PushIRLocal(&Function, Args[Idx]->Decl.ID, Register,
					FnType->Function.Args[Idx], true);
		}


		ForArray(Idx, Body)
		{
			BuildIRFunctionLevel(&Builder, Body[Idx]);
		}
		Terminate(&Builder, {});
		Function.LastRegister = Builder.LastRegister;
		VFree(Function.Locals);
		Function.Locals = NULL;
	}
	return Function;
}

function GlobalLevelIR(node *Node, slice<ir_symbol> ModuleSymbols)
{
	function Result = {};
	switch(Node->Type)
	{
		case AST_FN:
		{
			Assert(Node->Fn.Name);

			Result = BuildFunctionIR(Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, ModuleSymbols);
		} break;
		case AST_STRUCTDECL:
		{
			// do nothing
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
	ir IR = {};
	u32 NodeCount = ArrLen(Nodes);

	dynamic<ir_symbol> ModuleSymbols = {};
	for(int I = 0; I < NodeCount; ++I)
	{
		if(Nodes[I]->Type == AST_FN)
		{
			ir_symbol Sym = {};
			Sym.Type = Nodes[I]->Fn.TypeIdx;
			Sym.Name = Nodes[I]->Fn.Name;
			Sym.Register = ModuleSymbols.Count;
			if(Nodes[I]->Fn.Body.Count == 0)
				Sym.Flags = IRSymbol_ExternFn;
			ModuleSymbols.Push(Sym);
		}
	}

	IR.MaxRegisters = ModuleSymbols.Count;
	slice<ir_symbol> ModuleSymbolsSlice = SliceFromArray(ModuleSymbols);
	for(int I = 0; I < NodeCount; ++I)
	{
		function MaybeFunction = GlobalLevelIR(Nodes[I], ModuleSymbolsSlice);
		if(MaybeFunction.LastRegister > IR.MaxRegisters)
			IR.MaxRegisters = MaybeFunction.LastRegister;

		if(MaybeFunction.Name)
			IR.Functions.Push(MaybeFunction);
	}

	IR.GlobalSymbols = ModuleSymbolsSlice;
	return IR;
}

void DissasembleBasicBlock(string_builder *Builder, basic_block *Block)
{
	ForArray(I, Block->Code)
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
				const_value *Val = (const_value *)Instr.BigRegister;
				using ct = const_type;
				switch(Val->Type)
				{
					case ct::String:
					{
						PushBuilderFormated(Builder, "%%%d = %s \"%.*s\"", Instr.Result, GetTypeName(Type),
								Val->String.Data->Size, Val->String.Data->Data);
					} break;
					case ct::Integer:
					{
						if(Val->Int.IsSigned)
						{
							PushBuilderFormated(Builder, "%%%d = %s %lld", Instr.Result, GetTypeName(Type),
									Val->Int.Signed);
						}
						else
						{
							PushBuilderFormated(Builder, "%%%d = %s %llu", Instr.Result, GetTypeName(Type),
									Val->Int.Unsigned);
						}
					} break;
					case ct::Float:
					{
						PushBuilderFormated(Builder, "%%%d = %s %f", Instr.Result, GetTypeName(Type), Val->Float);
					} break;
				}
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
						Instr.Right);
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
				if(Instr.Left != -1)
					PushBuilderFormated(Builder, "RET %s %%%d", GetTypeName(Type), Instr.Left);
				else
					PushBuilderFormated(Builder, "RET");
			} break;
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)Instr.BigRegister;
				//PushBuilderFormated(Builder, "%%%d = CALL %s", Instr.Result, CallInfo->FnName->Data);
				PushBuilderFormated(Builder, "%%%d = CALL %d", Instr.Result, CallInfo->Operand);
			} break;
			case OP_IF:
			{
				PushBuilderFormated(Builder, "IF %%%d goto %d, else goto %d", Instr.Result, Instr.Left, Instr.Right);
			} break;
			case OP_JMP:
			{
				PushBuilderFormated(Builder, "JMP %d", Instr.BigRegister);
			} break;
			case OP_INDEX:
			{
				PushBuilderFormated(Builder, "%%%d = %%%d[%%%d]", Instr.Result, Instr.Left, Instr.Right);
			} break;
			case OP_ARRAYLIST:
			{
				PushBuilderFormated(Builder, "%%%d = ARRAYLIST (cba)", Instr.Result);
			} break;
			case OP_MEMSET:
			{
				PushBuilderFormated(Builder, "MEMSET %%%d", Instr.Right);
			} break;
			case OP_ARG:
			{
				PushBuilderFormated(Builder, "%%%d = ARG #%d", Instr.Result, Instr.BigRegister);
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
			case OP_COUNT: unreachable;
		}
		PushBuilder(Builder, '\n');
	}
	PushBuilder(Builder, '\n');
}

string Dissasemble(slice<function> Functions)
{
	string_builder Builder = MakeBuilder();
	ForArray(FnIdx, Functions)
	{
		function Fn = Functions[FnIdx];
		const type *FnType = GetType(Fn.Type);
		PushBuilderFormated(&Builder, "\nfn %s(", Fn.Name->Data);
		for(int I = 0; I < FnType->Function.ArgCount; ++I)
		{
			const type *ArgType = GetType(FnType->Function.Args[I]);
			PushBuilderFormated(&Builder, "%s %%%d", GetTypeName(ArgType), I + Fn.ModuleSymbols.Count);
			if(I + 1 != FnType->Function.ArgCount)
				PushBuilder(&Builder, ", ");
		}
		PushBuilder(&Builder, ')');

		if(Fn.Blocks.Count != 0)
		{
			PushBuilder(&Builder, " {\n");
			ForArray(I, Fn.Blocks)
			{
				PushBuilderFormated(&Builder, "\n\tblock_%d:\n", I);
				DissasembleBasicBlock(&Builder, Fn.Blocks.GetPtr(I));
			}
			PushBuilder(&Builder, "}\n");
		}
		else
		{
			PushBuilder(&Builder, ";\n");
		}
	}
	return MakeString(Builder);
}


