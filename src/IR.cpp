#include "IR.h"
#include "Semantics.h"
#include "Dynamic.h"
#include "Memory.h"
#include "Parser.h"
#include "Type.h"
#include "VString.h"
#include "vlib.h"
#include "Log.h"

inline u32 PushAlloc(u32 Type, block_builder *Builder)
{
	return PushInstruction(Builder,
			Instruction(OP_ALLOC, -1, Type, Builder));
}


inline instruction Instruction(op Op, u64 Val, u32 Type, block_builder *Builder)
{
	instruction Result;
	Result.BigRegister = Val;
	Result.Op = Op;
	Result.Type = Type;
	Result.Result = Builder->LastRegister++;
	return Result;
}

inline instruction InstructionDebugInfo(ir_debug_info *Info)
{
	instruction Result;
	Result.Op = OP_DEBUGINFO;
	Result.BigRegister = (u64)Info;
	Result.Type = INVALID_TYPE;

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
	//LDEBUG("Pushing: %s", Name->Data);
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

u32 AllocateAndCopy(block_builder *Builder, u32 Type, u32 Expr)
{
	u32 Ptr = PushAlloc(Type, Builder);
	Expr = PushInstruction(Builder,
			InstructionStore(Ptr, Expr, Type));
	return Expr;
}

void FixCallWithComplexParameter(block_builder *Builder, dynamic<u32> &Args, u32 ArgTypeIdx, node *Expr, b32 IsLHS)
{
	const type *ArgType = GetType(ArgTypeIdx);
	if(ArgType->Kind == TypeKind_Array)
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
		Args.Push(AllocateAndCopy(Builder, ArgTypeIdx, Res));
		return;
	}
	else if(ArgType->Kind == TypeKind_Function)
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
		Args.Push(Res);
		return;
	}
	Assert(ArgType->Kind == TypeKind_Struct);
	int Size = GetTypeSize(ArgType);

	if(Size > MAX_PARAMETER_SIZE)
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
		Args.Push(AllocateAndCopy(Builder, ArgTypeIdx, Res));
		return;
	}

	u32 Pass = -1;
	switch(Size)
	{
		case 8:
		{
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			Pass = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u64, Builder));
		} break;
		case 4:
		{
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			Pass = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u32, Builder));
		} break;
		case 2:
		{
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			Pass = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u16, Builder));
		} break;
		case 1:
		{
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			Pass = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u8, Builder));
		} break;
		default:
		{
			if(Size > 8)
			{
				LFATAL("Passing struct of size %d to function is not currently supported", Size);
			}
			u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
			Args.Push(AllocateAndCopy(Builder, ArgTypeIdx, Res));
			return;
		} break;
	}
	Args.Push(Pass);
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
		case AST_RESERVED:
		{
			const_value *Val = NewType(const_value);
			Val->Type = const_type::Integer;
			using rs = reserved;
			switch(Node->Reserved.ID)
			{
				case rs::False:
				case rs::Null:
				{
					Val->Int.IsSigned = false;
					Val->Int.Unsigned = 0;
				} break;
				case rs::True:
				{
					Val->Int.IsSigned = false;
					Val->Int.Unsigned = 1;
				} break;
				default: unreachable;
			}
			Result = PushInstruction(Builder, 
					Instruction(OP_CONST, (u64)Val, Node->Reserved.Type, Builder));
		} break;
		case AST_SIZE:
		{
			const type *Type = GetType(Node->Size.Type);
			if(Type->Kind == TypeKind_Basic && Type->Basic.Kind == Basic_string)
			{
				u32 StringRegister = BuildIRFromExpression(Builder, Node->Size.Expression, true);
				u32 SizePtr = PushInstruction(Builder,
						Instruction(OP_INDEX, StringRegister, 1, Basic_string, Builder));
				Result = PushInstruction(Builder,
						Instruction(OP_LOAD, 0, SizePtr, Basic_int, Builder));
			}
			else
			{
				const_value *Size = NewType(const_value);
				Size->Type = const_type::Integer;
				Size->Int.IsSigned = false;
				Size->Int.Unsigned = GetTypeSize(Type);
				Result = PushInstruction(Builder, 
						Instruction(OP_CONST, (u64)Size, Basic_u64, Builder));
			}
		} break;
		case AST_CALL:
		{
			//Assert(!IsLHS);
			call_info *CallInfo = NewType(call_info);

			CallInfo->Operand = BuildIRFromExpression(Builder, Node->Call.Fn, IsLHS);

			const type *Type = GetType(Node->Call.Type);
			if(Type->Kind == TypeKind_Pointer)
				Type = GetType(Type->Pointer.Pointed);
			Assert(Type->Kind == TypeKind_Function);
			dynamic<u32> Args = {};

			u32 ResultPtr = -1;
			u32 ReturnedWrongType = -1;
			if(Type->Function.Return != INVALID_TYPE)
			{
				const type *RT = GetType(Type->Function.Return);
				if(IsRetTypePassInPointer(Type->Function.Return))
				{
					ResultPtr = PushInstruction(Builder,
							Instruction(OP_ALLOC, -1, Type->Function.Return, Builder));
					Args.Push(ResultPtr);
				}
				else if(RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array)
				{
					ReturnedWrongType = ComplexTypeToSizeType(RT);
				}
			}
			for(int Idx = 0; Idx < Node->Call.Args.Count; ++Idx)
			{
				const type *ArgType = GetType(Type->Function.Args[Idx]);
				if(!IsLoadableType(ArgType))
				{
					FixCallWithComplexParameter(Builder, Args, Type->Function.Args[Idx], Node->Call.Args[Idx], IsLHS);
				}
				else
				{
					u32 Expr = BuildIRFromExpression(Builder, Node->Call.Args[Idx], IsLHS);
					Args.Push(Expr);
				}
			}

			CallInfo->Args = SliceFromArray(Args);

			u32 CallType = Node->Call.Type;
			if(ReturnedWrongType != -1)
			{
				type *NT = AllocType(TypeKind_Function);
				*NT = *Type;
				NT->Function.Return = ReturnedWrongType;
				CallType = AddType(NT);
			}

			Result = PushInstruction(Builder, Instruction(OP_CALL, (u64)CallInfo, CallType, Builder));
			if(ResultPtr != -1)
				Result = ResultPtr;
			else if(ReturnedWrongType != -1)
			{
				u32 R = Type->Function.Return;
				u32 Alloced = PushInstruction(Builder,
						Instruction(OP_ALLOC, -1, R, Builder));
				Result = PushInstruction(Builder,
						InstructionStore(Alloced, Result, ReturnedWrongType));
			}
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
				const type *MemberType = GetType(StructType->Struct.Members[Idx].Type);
				u32 MemberIndex = Node->StructList.NameIndexes[Idx];
				u32 Expr = BuildIRFromExpression(Builder, Node->StructList.Expressions[Idx], false);
				if(MemberType->Kind == TypeKind_Basic && MemberType->Basic.Kind == Basic_string)
				{
					Expr = PushInstruction(Builder,
							Instruction(OP_LOAD, 0, Expr, StructType->Struct.Members[Idx].Type, Builder));
				}
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
			if(Node->Selector.Index == -1)
			{
				Assert(Node->Selector.Operand->Type == AST_ID);
				string SymName = *Node->Selector.Member;
				string ModuleName = *Node->Selector.Operand->ID.Name;
				string *Mangled = StructToModuleNamePtr(SymName, ModuleName);
				const ir_symbol *Sym = GetIRLocal(Builder->Function, Mangled);
				const type *Type = GetType(Node->Selector.Type);
				Result = Sym->Register;
				if(!IsLHS && IsLoadableType(Type))
				{
					Result = PushInstruction(Builder,
							Instruction(OP_LOAD, 0, Result, Node->Selector.Type, Builder));
				}
			}
			else
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

			const type *To = GetType(Node->Cast.ToType);
			const type *From = GetType(Node->Cast.FromType);

			if(From->Kind == TypeKind_Pointer && To->Kind == TypeKind_Pointer)
			{
				Result = Expression;
			}
			else
			{
				instruction I = Instruction(OP_CAST, Expression, Node->Cast.FromType, Node->Cast.ToType, Builder);
				Result = PushInstruction(Builder, I);
			}
		} break;
		case AST_FN:
		{
			function fn = BuildFunctionIR(Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, Builder->Imported, Builder->Module);

			Result = PushInstruction(Builder,
					Instruction(OP_FN, (u64)DupeType(fn, function), fn.Type, Builder));
			/*
			PushIRLocal(Builder->Function, fn.Name, Result,
					fn.Type, SymbolFlag_Function);
					*/
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

void IRPushDebugVariableInfo(block_builder *Builder, const error_info *ErrorInfo, string Name, u32 Type, u32 Location)
{
	ir_debug_info *IRInfo = NewType(ir_debug_info);
	IRInfo->type = IR_DBG_VAR;
	IRInfo->var.LineNo = ErrorInfo->Line;
	IRInfo->var.Name = Name;
	IRInfo->var.Register = Location;
	IRInfo->var.TypeID = Type;

	PushInstruction(Builder,
			InstructionDebugInfo(IRInfo));
}

u32 BuildIRStoreVariable(block_builder *Builder, u32 Expression, u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	if(ShouldCopyType(Type))
	{
		u32 LocalRegister = PushInstruction(Builder,
				Instruction(OP_ALLOC, -1, TypeIdx, Builder));
		PushInstruction(Builder,
				InstructionStore(LocalRegister, Expression, TypeIdx));
		return LocalRegister;
	}
	else
	{
		return Expression;
	}
}

void BuildIRFromDecleration(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_DECL);
	u32 ExpressionRegister = BuildIRFromExpression(Builder, Node->Decl.Expression);
	u32 Var = BuildIRStoreVariable(Builder, ExpressionRegister, Node->Decl.TypeIndex);
	IRPushDebugVariableInfo(Builder, Node->ErrorInfo, *Node->Decl.ID, Node->Decl.TypeIndex, Var);
	PushIRLocal(Builder->Function, Node->Decl.ID, Var, Node->Decl.TypeIndex);
}

void Terminate(block_builder *Builder, basic_block GoTo)
{
	Builder->CurrentBlock.HasTerminator = true;
	Builder->Function->Blocks.Push(Builder->CurrentBlock);
	Builder->CurrentBlock = GoTo;
}

void BuildIRBody(dynamic<node *> &Body, block_builder *Block, basic_block Then);

void BuildIRForLoopWhile(block_builder *Builder, node *Node, b32 HasCondition)
{
	basic_block Cond  = AllocateBlock(Builder);
	basic_block Then  = AllocateBlock(Builder);
	basic_block End   = AllocateBlock(Builder);
	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, Cond);

	if(HasCondition)
	{
		u32 CondExpr = BuildIRFromExpression(Builder, Node->For.Expr1);
		PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, CondExpr, Basic_bool));
	}
	else
	{
		PushInstruction(Builder, Instruction(OP_JMP, Then.ID, Basic_type, Builder));
	}

	Terminate(Builder, Then);
	BuildIRBody(Node->For.Body, Builder, Cond);
	Builder->CurrentBlock = End;
}

void BuildIRForIt(block_builder *Builder, node *Node)
{
	basic_block Cond  = AllocateBlock(Builder);
	basic_block Then  = AllocateBlock(Builder);
	basic_block Incr  = AllocateBlock(Builder);
	basic_block End   = AllocateBlock(Builder);

	u32 IAlloc, ItAlloc, Size, One, Array;

	// Init
	{
		const_value *ValSize = NewType(const_value);
		ValSize->Type = const_type::Integer;
		ValSize->Int.Unsigned = Node->For.ArraySize;

		Size = PushInstruction(Builder, 
				Instruction(OP_CONST, (u64)ValSize, Basic_int, Builder));

		const_value *ValOne = NewType(const_value);
		ValOne->Type = const_type::Integer;
		ValOne->Int.Unsigned = 1;

		One = PushInstruction(Builder, 
				Instruction(OP_CONST, (u64)ValOne, Basic_int, Builder));

		const_value *ValZero = NewType(const_value);
		ValZero->Type = const_type::Integer;
		ValZero->Int.Unsigned = 0;

		IAlloc = PushInstruction(Builder,
				Instruction(OP_ALLOC, -1, Basic_int, Builder));
		u32 Zero = PushInstruction(Builder, 
				Instruction(OP_CONST, (u64)ValZero, Basic_int, Builder));
		PushInstruction(Builder,
				InstructionStore(IAlloc, Zero, Basic_int));

		Array = BuildIRFromExpression(Builder, Node->For.Expr2);

		u32 ElemPtr = PushInstruction(Builder,
				Instruction(OP_INDEX, Array, Zero, Node->For.ArrayType, Builder));

		u32 Elem = PushInstruction(Builder, 
				Instruction(OP_LOAD, 0, ElemPtr, Node->For.ItType, Builder));

		ItAlloc = BuildIRStoreVariable(Builder, Elem, Node->For.ItType);
		IRPushDebugVariableInfo(Builder, Node->ErrorInfo,
				*Node->For.Expr1->ID.Name, Node->For.ItType, ItAlloc);
		PushIRLocal(Builder->Function, Node->For.Expr1->ID.Name, ItAlloc, Node->For.ItType);

		PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	}

	Terminate(Builder, Cond);
	
	// Condition
	{
		u32 I = PushInstruction(Builder, 
				Instruction(OP_LOAD, 0, IAlloc, Basic_int, Builder));
		u32 Condition = PushInstruction(Builder,
				Instruction(OP_LESS, I, Size, Basic_bool, Builder));
		PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, Condition, Basic_bool));
	}
	Terminate(Builder, Then);

	// Body
	{
		BuildIRBody(Node->For.Body, Builder, Incr);
	}

	// Increment
	{
		u32 I = PushInstruction(Builder, 
				Instruction(OP_LOAD, 0, IAlloc, Basic_int, Builder));
		
		u32 ToStore = PushInstruction(Builder, 
				Instruction(OP_ADD, I, One, Basic_int, Builder));

		PushInstruction(Builder,
				InstructionStore(IAlloc, ToStore, Basic_int));

		u32 ElemPtr = PushInstruction(Builder,
				Instruction(OP_INDEX, Array, ToStore, Node->For.ArrayType, Builder));

		u32 Elem = PushInstruction(Builder, 
				Instruction(OP_LOAD, 0, ElemPtr, Node->For.ItType, Builder));

		PushInstruction(Builder, InstructionStore(ItAlloc, Elem, Node->For.ItType));

		PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	}
	Terminate(Builder, End);
}

void BuildIRForLoopCStyle(block_builder *Builder, node *Node)
{
	basic_block Cond  = AllocateBlock(Builder);
	basic_block Then  = AllocateBlock(Builder);
	basic_block Incr  = AllocateBlock(Builder);
	basic_block End   = AllocateBlock(Builder);

	if(Node->For.Expr1)
		BuildIRFromDecleration(Builder, Node->For.Expr1);
	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, Cond);

	if(Node->For.Expr2)
	{
		u32 CondExpr = BuildIRFromExpression(Builder, Node->For.Expr2);
		PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, CondExpr, Basic_bool));
	}
	else
	{
		PushInstruction(Builder, Instruction(OP_JMP, Then.ID, Basic_type, Builder));
	}
	Terminate(Builder, Then);
	BuildIRBody(Node->For.Body, Builder, Incr);

	if(Node->For.Expr3)
		BuildIRFromExpression(Builder, Node->For.Expr3);

	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, End);
}

void BuildIRFunctionLevel(block_builder *Builder, node *Node)
{
	IRPushDebugLocation(Builder, Node->ErrorInfo);
	switch(Node->Type)
	{
		case AST_DECL:
		{
			BuildIRFromDecleration(Builder, Node);
		} break;
		case AST_RETURN:
		{
			u32 Expression = -1;
			u32 Type = Node->Return.TypeIdx;
			const type *RT = GetType(Type);
			if(Node->Return.Expression)
					Expression = BuildIRFromExpression(Builder, Node->Return.Expression);
			if(!IsRetTypePassInPointer(Type) && (RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array))
			{
				Type = ComplexTypeToSizeType(RT);
				Expression = PushInstruction(Builder,
						Instruction(OP_LOAD, 0, Expression, Type, Builder));
			}
			PushInstruction(Builder, Instruction(OP_RET, Expression, 0, Type, Builder));
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
			using ft = for_type;
			switch(Node->For.Kind)
			{
				case ft::C:
				{
					BuildIRForLoopCStyle(Builder, Node);
				} break;
				case ft::While:
				{
					BuildIRForLoopWhile(Builder, Node, true);
				} break;
				case ft::Infinite:
				{
					BuildIRForLoopWhile(Builder, Node, false);
				} break;
				case ft::It:
				{
					BuildIRForIt(Builder, Node);
				} break;
			}
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

void IRPushGlobalSymbolsForFunction(block_builder *Builder, function *Fn, slice<import> Imported, import Module)
{
	uint Count = 0;
	ForArray(GIdx, Module.Globals)
	{
		symbol *s = Module.Globals[GIdx];
		PushIRLocal(Fn, s->Name, Count++,
				s->Type, s->Flags & SymbolFlag_Function);
	}

	ForArray(Idx, Imported)
	{
		auto m = Imported[Idx];
		ForArray(GIdx, m.Globals)
		{
			symbol *s = m.Globals[GIdx];
			string sName = *s->Name;

			string *Mangled = StructToModuleNamePtr(sName, m.Name);
			PushIRLocal(Fn, Mangled, Count++,
					s->Type, s->Flags & SymbolFlag_Function);
		}
	}
	Builder->LastRegister = Count;

}

void IRPushDebugArgInfo(block_builder *Builder, const error_info *ErrorInfo, int ArgNo, u32 Location, string Name, u32 TypeID)
{
	ir_debug_info *IRInfo = NewType(ir_debug_info);
	IRInfo->type = IR_DBG_ARG;
	IRInfo->arg.ArgNo = ArgNo;
	IRInfo->arg.LineNo = ErrorInfo->Line;
	IRInfo->arg.Name = Name;
	IRInfo->arg.Register = Location;
	IRInfo->arg.TypeID = TypeID;

	PushInstruction(Builder,
			InstructionDebugInfo(IRInfo));
}

void IRPushDebugLocation(block_builder *Builder, const error_info *Info)
{
	ir_debug_info *IRInfo = NewType(ir_debug_info);
	IRInfo->type = IR_DBG_LOCATION;
	IRInfo->loc.LineNo = Info->Line;

	PushInstruction(Builder,
			InstructionDebugInfo(IRInfo));
}

function BuildFunctionIR(dynamic<node *> &Body, const string *Name, u32 TypeIdx, slice<node *> &Args, node *Node,
		slice<import> Imported, import Module)
{
	function Function = {};
	Function.Name = Name;
	Function.Type = TypeIdx;
	Function.LineNo = Node->ErrorInfo->Line;
	Function.ModuleName = Module.Name;
	if(Body.IsValid())
	{
		Function.Locals = (ir_symbol *)VAlloc(MB(1) * sizeof(ir_symbol));
		Function.LocalCount = 0;
		block_builder Builder = {};
		Builder.Function = &Function;
		Builder.CurrentBlock = AllocateBlock(&Builder);
		Builder.Imported = Imported;
		Builder.Module = Module;

		const type *FnType = GetType(TypeIdx);
		IRPushDebugLocation(&Builder, Node->ErrorInfo);

		IRPushGlobalSymbolsForFunction(&Builder, &Function, Imported, Module);
		ForArray(Idx, Args)
		{
			u32 Register = PushInstruction(&Builder,
					Instruction(OP_ARG, Idx, FnType->Function.Args[Idx], &Builder));
			PushIRLocal(&Function, Args[Idx]->Decl.ID, Register,
					FnType->Function.Args[Idx], true);
			IRPushDebugArgInfo(&Builder, Node->ErrorInfo, Idx, Register, *Args[Idx]->Decl.ID, FnType->Function.Args[Idx]);
		}

		ForArray(Idx, Body)
		{
			BuildIRFunctionLevel(&Builder, Body[Idx]);
		}
		Terminate(&Builder, {});
		Function.LastRegister = Builder.LastRegister;
		VFree(Function.Locals);
		Builder.Function = NULL;
		Function.Locals = NULL;
	}
	return Function;
}

function GlobalLevelIR(node *Node, slice<import> Imported, import Module)
{
	function Result = {};
	switch(Node->Type)
	{
		case AST_FN:
		{
			Assert(Node->Fn.Name);

			if(Node->Fn.MaybeGenric == NULL)
			{
				Result = BuildFunctionIR(Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, Imported, Module);
			}
		} break;
		case AST_DECL:
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

ir BuildIR(file *File)
{
	ir IR = {};
	u32 NodeCount = File->Nodes.Count;

	string GlobalFnName = STR_LIT("__GlobalInitializerFunction");
	function GlobalInitializers = {};
	GlobalInitializers.Name = DupeType(GlobalFnName, string);
	GlobalInitializers.Type = INVALID_TYPE;
	GlobalInitializers.Locals = (ir_symbol *)VAlloc(MB(1) * sizeof(ir_symbol));
	GlobalInitializers.LocalCount = 0;

	block_builder Builder = {};
	Builder.Function = &GlobalInitializers;
	Builder.CurrentBlock = AllocateBlock(&Builder);

	IRPushGlobalSymbolsForFunction(&Builder, &GlobalInitializers, 
			*File->Checker->Imported, *File->Checker->Module);

	for(int I = 0; I < NodeCount; ++I)
	{
		node *Node = File->Nodes[I];
		if(Node->Type == AST_DECL)
		{
			u32 Expr = BuildIRFromExpression(&Builder, Node->Decl.Expression);
			const ir_symbol *Sym =GetIRLocal(Builder.Function, Node->Decl.ID);
			Assert(Sym);
			PushInstruction(&Builder, InstructionStore(Sym->Register, Expr, Node->Decl.TypeIndex));
		}
	}
	PushInstruction(&Builder, Instruction(OP_RET, -1, 0, INVALID_TYPE, &Builder));
	Terminate(&Builder, {});

	IR.Functions.Push(GlobalInitializers);

	Builder.Function = NULL;

	VFree(GlobalInitializers.Locals);

	for(int I = 0; I < NodeCount; ++I)
	{
		function MaybeFunction = GlobalLevelIR(File->Nodes[I], *File->Checker->Imported, *File->Checker->Module);
		if(MaybeFunction.LastRegister > IR.MaxRegisters)
			IR.MaxRegisters = MaybeFunction.LastRegister;

		if(MaybeFunction.Name)
			IR.Functions.Push(MaybeFunction);
	}

	return IR;
}

void DissasembleBasicBlock(string_builder *Builder, basic_block *Block, int indent)
{
	ForArray(I, Block->Code)
	{
		instruction Instr = Block->Code[I];
		const type *Type = NULL;
		if(Instr.Type != INVALID_TYPE)
			Type = GetType(Instr.Type);

		PushBuilder(Builder, '\t');
		PushBuilder(Builder, '\t');
		for(int i = 0; i < indent; ++i)
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
			case OP_FN:
			{
				function Fn = *(function *)Instr.BigRegister;
				PushBuilderFormated(Builder, "%%%d = %s", Instr.Result, DissasembleFunction(Fn, 2).Data);
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
			case OP_DEBUGINFO:
			{
				ir_debug_info *Info = (ir_debug_info *)Instr.BigRegister;
				switch(Info->type)
				{
					case IR_DBG_ARG:
					{
						PushBuilderFormated(Builder, "DEBUG_ARG_INFO %s #%d", Info->arg.Name.Data, Info->arg.ArgNo);
					} break;
					case IR_DBG_VAR:
					{
						PushBuilderFormated(Builder, "DEBUG_VAR_INFO %s Line=%d, Type=%s", Info->var.Name.Data, Info->var.LineNo, GetTypeName(Info->var.TypeID));
					} break;
					case IR_DBG_LOCATION:
					{
						PushBuilderFormated(Builder, "DEBUG_LOC_INFO Line=%d", Info->loc.LineNo);
					} break;
					case IR_DBG_SCOPE:
					{
						*Builder += "SCOPE";
					} break;
				}
			} break;
			case OP_COUNT: unreachable;
		}
		PushBuilder(Builder, '\n');
	}
	PushBuilder(Builder, '\n');
}

string DissasembleFunction(function Fn, int indent)
{
	const type *FnType = GetType(Fn.Type);
	if(!FnType)
		return STR_LIT("");
	string_builder Builder = MakeBuilder();
	PushBuilderFormated(&Builder, "fn %s(", Fn.Name->Data);
	for(int I = 0; I < FnType->Function.ArgCount; ++I)
	{
		const type *ArgType = GetType(FnType->Function.Args[I]);
		PushBuilderFormated(&Builder, "%s %%%d", GetTypeName(ArgType), I + Fn.ModuleSymbols.Count);
		if(I + 1 != FnType->Function.ArgCount)
			PushBuilder(&Builder, ", ");
	}
	PushBuilder(&Builder, ')');
	if(FnType->Function.Return != INVALID_TYPE)
	{
		PushBuilderFormated(&Builder, " -> %s", GetTypeName(FnType->Function.Return));
	}

	if(Fn.Blocks.Count != 0)
	{
		PushBuilder(&Builder, " {\n");
		ForArray(I, Fn.Blocks)
		{
			Builder += '\n';
			for(int i = 0; i < indent; ++i)
				Builder += '\t';

			PushBuilderFormated(&Builder, "\tblock_%d:\n", I);
			DissasembleBasicBlock(&Builder, Fn.Blocks.GetPtr(I), indent);
		}
		for(int i = 0; i < indent; ++i)
			Builder += '\t';

		PushBuilder(&Builder, "}\n");
	}
	else
	{
		PushBuilder(&Builder, ";\n");
	}
	return MakeString(Builder);
}

string Dissasemble(slice<function> Functions)
{
	string_builder Builder = MakeBuilder();
	ForArray(FnIdx, Functions)
	{
		function Fn = Functions[FnIdx];
		Builder += '\n';
		Builder += DissasembleFunction(Fn, 0);
	}
	return MakeString(Builder);
}


