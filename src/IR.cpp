#include "IR.h"
#include "CommandLine.h"
#include "ConstVal.h"
#include "Errors.h"
#include "Globals.h"
#include "Interpreter.h"
#include "Module.h"
#include "Semantics.h"
#include "Dynamic.h"
#include "Memory.h"
#include "Parser.h"
#include "Type.h"
#include "VString.h"
#include "vlib.h"
#include "Log.h"
#include <cmath>

void Terminate(block_builder *Builder, basic_block GoTo)
{
	Builder->CurrentBlock.HasTerminator = true;
	Builder->Function->Blocks.Push(Builder->CurrentBlock);
	Builder->CurrentBlock = GoTo;
}

u32 GenerateVoidFnT()
{
	type *NT = AllocType(TypeKind_Function);
	NT->Function.Args = NULL;
	NT->Function.ArgCount = 0;
	NT->Function.Returns = {};
	NT->Function.Flags = 0;
	return AddType(NT);
}

inline void PushDebugBreak(block_builder *Builder)
{
	instruction Result = {};
	Result.Op = OP_DEBUG_BREAK;
	PushInstruction(Builder, Result);
}

inline void PushResult(u32 Register, u32 Type, block_builder *Builder)
{
	instruction Result;
	Result.Op = OP_RESULT;
	Result.Type = Type;
	Result.Right = Register;
	Result.Result = Register;
	PushInstruction(Builder, Result);
}

inline u32 PushAlloc(u32 Type, block_builder *Builder)
{
	return PushInstruction(Builder,
			Instruction(OP_ALLOC, -1, Type, Builder));
}

inline instruction Instruction(op Op, void *Ptr, u32 Type, block_builder *Builder, int Reserved)
{
	UNUSED(Reserved);

	instruction Result;
	Result.Ptr = Ptr;
	Result.Op = Op;
	Result.Type = Type;
	Result.Result = Builder->LastRegister++;
	return Result;
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

inline u32 PushInt(u64 Value, block_builder *Builder, u32 Type = Basic_int)
{
	return PushInstruction(Builder, Instruction(OP_CONSTINT, Value, Type, Builder));
}

inline instruction InstructionDebugInfo(ir_debug_info *Info)
{
	instruction Result;
	Result.Op = OP_DEBUGINFO;
	Result.Ptr = Info;
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

inline instruction InstructionZeroOut(u32 Ptr, u32 Type)
{
	instruction Result;
	Result.Left = 0;
	Result.Right = Ptr;
	Result.Op = OP_ZEROUT;
	Result.Type = Type;
	Result.Result = Ptr;
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

block_builder MakeBlockBuilder(function *Fn, module *Module, slice<import> Imported)
{
	block_builder Builder = {};
	Builder.Scope.Push({});
	Builder.Function = Fn;
	Builder.CurrentBlock = AllocateBlock(&Builder);
	Builder.Imported = Imported;
	Builder.LastRegister = 0;
	Builder.Module = Module;

	return Builder;
}

void PushIRLocal(block_builder *Builder, const string *Name, u32 Register, u32 Type)
{
	symbol Local;
	Local.Register = Register;
	Local.Name  = Name;
	Local.Type  = Type;

	Builder->Scope.Peek().Add(*Name, Local);
}

u32 MakeIRString(block_builder *Builder, string S)
{
	const_value *Val = NewType(const_value);
	*Val = MakeConstString(DupeType(S, string));
	return PushInstruction(Builder, 
			Instruction(OP_CONST, (u64)Val, Basic_string, Builder));
}

function GenerateFakeFunctionForGlobalExpression(node *Expression, module *Module, slice<import> Imported, node *ErrInfoNode)
{
	static u32 VoidFnT = GenerateVoidFnT();

	function FakeFn = {};
	if(Expression)
	{
		string NoFnName = STR_LIT("");
		FakeFn.Name = DupeType(NoFnName, string);
		FakeFn.Type = VoidFnT;
		FakeFn.LinkName = FakeFn.Name;
		FakeFn.NoDebugInfo = true;
		FakeFn.FakeFunction = true;
		block_builder Builder = MakeBlockBuilder(&FakeFn, Module, Imported);
		PushErrorInfo(&Builder, ErrInfoNode);
		PushStepLocation(&Builder, ErrInfoNode);
		BuildIRFromExpression(&Builder, Expression);
		Terminate(&Builder, {});
		FakeFn.LastRegister = Builder.LastRegister;
	}

	return FakeFn;
}

const symbol *GetIRLocal(block_builder *Builder, const string *NamePtr, b32 Error = true, b32 *OutIsGlobal=NULL)
{
	string Name = *NamePtr;
	for(int i = Builder->Scope.Data.Count-1; i >= 0; i--)
	{
		symbol *s = Builder->Scope.Data[i].GetUnstablePtr(Name);
		if(s)
			return s;
	}

	symbol *s = Builder->Module->Globals[Name];
	if(s)
	{
		if(OutIsGlobal) *OutIsGlobal = true;
		return s;
	}

	string NoNamespace = STR_LIT("*");
	For(Builder->Imported)
	{
		if(it->As == NoNamespace)
		{
			s = it->M->Globals[Name];
			if(s)
			{
				if(OutIsGlobal) *OutIsGlobal = true;
				return s;
			}
		}
	}


	split Split = SplitAt(Name, '.');
	if(Split.first.Data != NULL)
	{
		For(Builder->Imported)
		{
			if(it->M->Name == Split.first)
			{
				s = it->M->Globals[Split.second];
				if(s)
				{
					if(OutIsGlobal) *OutIsGlobal = true;
					return s;
				}
				break;
			}
		}
	}

	//StructToModuleName(Name, Builder->Module.)

	if(Error)
	{
		LDEBUG("%s\n", Name.Data);
		Assert(false);
	}
	return NULL;
}

void PushStepLocation(block_builder *Builder, u32 Line, u32 Col)
{
	ir_debug_info *Info = NewType(ir_debug_info);
	Info->type = IR_DBG_STEP_LOCATION;
	Info->step.Line = Line;
	Info->step.Column = Col;
	instruction DbgErrI = InstructionDebugInfo(Info);
	PushInstruction(Builder, DbgErrI);
}

void PushStepLocation(block_builder *Builder, node *Node)
{
	if(Node->ErrorInfo == NULL)
		return;

	PushStepLocation(Builder, Node->ErrorInfo->Range.StartLine, Node->ErrorInfo->Range.StartChar);
}

void PushErrorInfo(block_builder *Builder, node *Node)
{
	if(Node->ErrorInfo == NULL)
		return;

	ir_debug_info *Info = NewType(ir_debug_info);
	Info->type = IR_DBG_ERROR_INFO;
	Info->err_i.ErrorInfo = Node->ErrorInfo;
	instruction DbgErrI = InstructionDebugInfo(Info);
	PushInstruction(Builder, DbgErrI);
	Builder->LastErrorInfo = Node->ErrorInfo;
}

u32 BuildSlice(block_builder *Builder, u32 Ptr, u32 Size, u32 SliceTypeIdx, const type *SliceType = NULL, u32 Alloc = -1)
{
	if(SliceType == NULL)
		SliceType = GetType(SliceTypeIdx);
	if(Alloc == -1)
	{
		Alloc = PushInstruction(Builder,
				Instruction(OP_ALLOC, -1, SliceTypeIdx, Builder));
	}

	u32 CountIdx = PushInstruction(Builder,
			Instruction(OP_INDEX, Alloc, 0, SliceTypeIdx, Builder));
	u32 DataIdx = PushInstruction(Builder,
			Instruction(OP_INDEX, Alloc, 1, SliceTypeIdx, Builder));

	PushInstruction(Builder,
			InstructionStore(CountIdx, Size, Basic_int));
	PushInstruction(Builder,
			InstructionStore(DataIdx, Ptr, GetPointerTo(SliceType->Slice.Type)));

	return Alloc;
}

u32 GetBuiltInFunction(block_builder *Builder, string Module, string FnName)
{
	// @TODO: Global abuse
	For(CurrentModules)
	{
		if((*it)->Name == Module)
		{
			symbol *s = (*it)->Globals[FnName];

			if(s)
			{
				instruction GlobalI = Instruction(OP_GLOBAL, (void *)s, s->Type, Builder, 0);
				return PushInstruction(Builder, GlobalI);
			}
			unreachable;
		}
	}

	unreachable;
}

u32 AllocateAndCopy(block_builder *Builder, u32 Type, u32 Expr)
{
	u32 Ptr = PushAlloc(Type, Builder);
	Expr = PushInstruction(Builder,
			InstructionStore(Ptr, Expr, Type));
	return Expr;
}

int GetPointerPassIdx(u32 TypeIdx, uint Size)
{
	const type *T = GetType(TypeIdx);
	ForArray(Idx, T->Struct.Members)
	{
		if(GetStructMemberOffset(T, Idx) == Size)
		{
			return Idx;
		}
	}
	unreachable;
}

u32 GetPointerPassSize(block_builder *Builder, u32 StructPtr, u32 TypeIdx, uint Size)
{
	int Idx = GetPointerPassIdx(TypeIdx, Size);
	return PushInstruction(Builder,
			Instruction(OP_INDEX, StructPtr, Idx, TypeIdx, Builder));
}

void FixCallWithComplexParameter(block_builder *Builder, dynamic<u32> &Args, u32 ArgTypeIdx, node *Expr, b32 IsLHS, b32 IsForeign)
{
	const type *ArgType = GetType(ArgTypeIdx);
	if(ArgType->Kind == TypeKind_Array || IsString(ArgType))
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, true);
		if(IsForeign)
			Res = AllocateAndCopy(Builder, ArgTypeIdx, Res);
		Args.Push(Res);
		return;
	}
	else if(ArgType->Kind == TypeKind_Function)
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
		Args.Push(Res);
		return;
	}
	else if(ArgType->Kind == TypeKind_Slice)
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
		if(IsForeign)
			Res = AllocateAndCopy(Builder, ArgTypeIdx, Res);
		Args.Push(Res);
		return;
	}
	Assert(ArgType->Kind == TypeKind_Struct);
	int Size = GetTypeSize(ArgType);

	if(Size > MAX_PARAMETER_SIZE)
	{
		u32 Res = BuildIRFromExpression(Builder, Expr, true);
		//Args.Push(AllocateAndCopy(Builder, ArgTypeIdx, Res));
		if(IsForeign)
			Res = AllocateAndCopy(Builder, ArgTypeIdx, Res);
		Args.Push(Res);
		return;
	}

	b32 AllFloats = IsStructAllFloats(ArgType);
	if(AllFloats && IsUnix())
	{
		type *Type = AllocType(TypeKind_Vector);
		Type->Vector.Kind = Vector_Float;
		Type->Vector.ElementCount = 2;
		u32 FloatVector = AddType(Type);
		u32 StructPtr = BuildIRFromExpression(Builder, Expr, IsLHS);

		int i = 0;
		for(; i+1 < ArgType->Struct.Members.Count; ++i)
		{
			u32 First = ArgType->Struct.Members[i].Type;
			u32 Second = ArgType->Struct.Members[i + 1].Type;
			u32 MemPtr = PushInstruction(Builder,
					Instruction(OP_INDEX, StructPtr, i, ArgTypeIdx, Builder));
			if(First == Basic_f32 && Second == Basic_f32)
			{
				u32 Mem = PushInstruction(Builder, Instruction(OP_LOAD, 0, MemPtr, FloatVector, Builder));
				Args.Push(Mem);
				++i;
			}
			else
			{
				u32 Mem = PushInstruction(Builder, Instruction(OP_LOAD, 0, MemPtr, First, Builder));
				Args.Push(Mem);
			}
		}

		if(i < ArgType->Struct.Members.Count)
		{
			u32 MemT = ArgType->Struct.Members[i].Type;
			u32 MemPtr = PushInstruction(Builder,
					Instruction(OP_INDEX, StructPtr, i, ArgTypeIdx, Builder));
			u32 Mem = PushInstruction(Builder, Instruction(OP_LOAD, 0, MemPtr, MemT, Builder));
			Args.Push(Mem);
		}

		return;
	}

	u32 Pass = -1;
	const type *T = ArgType;
	switch(Size)
	{
		case 16:
		{
			if(PTarget == platform_target::Windows || !CanGetPointerAfterSize(T, 8))
				goto PUSH_PTR;
			u32 SecondT = Basic_u64;
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			u32 Ptr = GetPointerPassSize(Builder, Res, ArgTypeIdx, 8);
			u32 First = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u64, Builder));
			u32 Second = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, Ptr, SecondT, Builder));

			Args.Push(First);
			Args.Push(Second);
			return;
		} break;
		case 12:
		{
			if(PTarget == platform_target::Windows || !CanGetPointerAfterSize(T, 8))
				goto PUSH_PTR;
			u32 SecondT = Basic_u32;
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			u32 Ptr = GetPointerPassSize(Builder, Res, ArgTypeIdx, 8);
			u32 First = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u64, Builder));
			u32 Second = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, Ptr, SecondT, Builder));

			Args.Push(First);
			Args.Push(Second);
			return;
		} break;
		case 10:
		{
			if(PTarget == platform_target::Windows || !CanGetPointerAfterSize(T, 8))
				goto PUSH_PTR;
			u32 SecondT = Basic_u16;
			u32 Res = BuildIRFromExpression(Builder, Expr, true);
			u32 Ptr = GetPointerPassSize(Builder, Res, ArgTypeIdx, 8);
			u32 First = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Res, Basic_u64, Builder));
			u32 Second = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, Ptr, SecondT, Builder));

			Args.Push(First);
			Args.Push(Second);
			return;
		} break;
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
PUSH_PTR:
			u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
			if(IsForeign)
				Res = AllocateAndCopy(Builder, ArgTypeIdx, Res);
			Args.Push(Res);
			return;
		} break;
	}
	Args.Push(Pass);
}

struct switch_context
{
	u32 ResultReg;
	u32 Matcher;
	slice<u32> CaseBlocks;
	slice<u32> CaseValues;
	u32 Default;
	u32 StartBlock;
	basic_block After;
};

switch_context BuildIRGenericSwitchPart(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_SWITCH);
	u32 Result = -1;
	if(Node->Switch.ReturnType != INVALID_TYPE)
		Result = PushInstruction(Builder,
			Instruction(OP_ALLOC, -1, Node->Switch.ReturnType, Builder));

	u32 StartBlock = Builder->CurrentBlock.ID;

	u32 Matcher = BuildIRFromExpression(Builder, Node->Switch.Expression);
	basic_block After = AllocateBlock(Builder);
	dynamic<u32> CaseBlocks = {};
	dynamic<u32> OnValues = {};
	u32 Default = -1;
	ForArray(Idx, Node->Switch.Cases)
	{
		node *Case = Node->Switch.Cases[Idx];
		if(Case->Case.Value == NULL)
		{
			OnValues.Push(0);
			Default = OnValues.Count-1;
		}
		else if(Case->Case.Value->Type == AST_LIST)
		{
			slice<node *> Values = Case->Case.Value->List.Nodes;
			For(Values)
			{
				u32 Value = BuildIRFromExpression(Builder, *it);
				OnValues.Push(Value);
			}
		}
		else
		{
			u32 Value = BuildIRFromExpression(Builder, Case->Case.Value);
			OnValues.Push(Value);
		}
	}

	Builder->YieldReturn.Push({.ToBlockID = After.ID, .ValueStore = Result});
	ForArray(Idx, Node->Switch.Cases)
	{
		basic_block CaseBlock = AllocateBlock(Builder);
		Terminate(Builder, CaseBlock);
		node *Case = Node->Switch.Cases[Idx];
		PushStepLocation(Builder, Case);

		ForArray(BodyIdx, Case->Case.Body)
		{
			node *Node = Case->Case.Body[BodyIdx];
			BuildIRFunctionLevel(Builder, Node);
		}

		if(!Builder->CurrentBlock.HasTerminator)
		{
			if(Case->Case.Body.Count > 0)
				PushStepLocation(Builder, Case->Case.Body.Last());

			PushInstruction(Builder,
					Instruction(OP_JMP, After.ID, Basic_type, Builder));
		}
		CaseBlocks.Push(CaseBlock.ID);
		
		if(Case->Case.Value && Case->Case.Value->Type == AST_LIST)
		{
			slice<node *> Values = Case->Case.Value->List.Nodes;
			for(int i = 1; i < Values.Count; ++i)
			{
				basic_block Fallthrough = AllocateBlock(Builder);
				Terminate(Builder, Fallthrough);
				PushStepLocation(Builder, Case);

				PushInstruction(Builder,
						Instruction(OP_JMP, CaseBlock.ID, Basic_type, Builder));
				CaseBlocks.Push(Fallthrough.ID);
			}
		}
	}
	Builder->YieldReturn.Pop();
	Terminate(Builder, After);
	PushStepLocation(Builder, Node);

	return (switch_context) {Result, Matcher,
		SliceFromArray(CaseBlocks), SliceFromArray(OnValues),
		Default, StartBlock, After};
}

u32 BuildIRIntMatch(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_SWITCH);

	switch_context c = BuildIRGenericSwitchPart(Builder, Node);

	for(int i = 0; i < Builder->Function->Blocks.Count; ++i)
	{
		if(Builder->Function->Blocks[i].ID == c.StartBlock)
		{
			ir_switchint *Info = NewType(ir_switchint);
			Info->OnValues = c.CaseValues;
			Info->Cases    = c.CaseBlocks;
			Info->Default  = c.Default;
			Info->After    = c.After.ID;
			Info->Matcher  = c.Matcher;
			instruction I = Instruction(OP_SWITCHINT, (void*)Info, Node->Switch.ReturnType, Builder, 0);
			Builder->Function->Blocks.Data[i].Code.Push(I);
			break;
		}
	}

	if(c.ResultReg != -1)
	{
		if(IsLoadableType(Node->Switch.ReturnType))
		{
			c.ResultReg = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, c.ResultReg, Node->Switch.ReturnType, Builder));
		}
	}
	return c.ResultReg;
}

void BuildAssertFailed(block_builder *Builder, const error_info *ErrorInfo, string BonusMessage)
{
	string FirstPart;
	string SecondPart;
	string ThirdPart;
	GetErrorSegments(*ErrorInfo, &FirstPart, &SecondPart, &ThirdPart);
	string_builder b = MakeBuilder();
	PushBuilderFormated(&b, "--- ASSERTION FAILED ---\n\n%s(%d):\n%s%s%s\n", ErrorInfo->FileName, ErrorInfo->Range.StartLine, FirstPart.Data, SecondPart.Data, ThirdPart.Data);
	if(BonusMessage.Size != 0)
		PushBuilderFormated(&b, "%.*s\n", (int)BonusMessage.Size, BonusMessage.Data);

	string S = MakeString(b);
	string *Message = DupeType(S, string);


	const_value *String = NewType(const_value);
	String->Type = const_type::String;
	String->String.Data = Message;
	u32 MessageRegister = PushInstruction(Builder, 
			Instruction(OP_CONST, (u64)String, Basic_string, Builder));

	u32 CountPtr = PushInstruction(Builder, Instruction(OP_INDEX, MessageRegister, 0, Basic_string, Builder));
	u32 DataPtr = PushInstruction(Builder, Instruction(OP_INDEX, MessageRegister, 1, Basic_string, Builder));
	u32 Count = PushInstruction(Builder, Instruction(OP_LOAD, 0, CountPtr, Basic_int, Builder));
	u32 Data = PushInstruction(Builder, Instruction(OP_LOAD, 0, DataPtr, GetPointerTo(Basic_u8), Builder));

	string Internal = STR_LIT("internal");
	u32 StdOut = GetBuiltInFunction(Builder, Internal, STR_LIT("stdout"));
	u32 FnT = Builder->CurrentBlock.Code.GetLast()->Type;
	call_info *Call = NewType(call_info);
	Call->ErrorInfo = ErrorInfo;
	Call = NewType(call_info);
	Call->Operand = StdOut;
	Call->Args = {};
	u32 Handle = PushInstruction(Builder, Instruction(OP_CALL, (void*)Call, FnT, Builder, 0));

	u32 Write = GetBuiltInFunction(Builder, Internal, STR_LIT("write"));
	FnT = Builder->CurrentBlock.Code.GetLast()->Type;

	Call = NewType(call_info);
	Call->ErrorInfo = ErrorInfo;
	Call->Operand = Write;
	Call->Args = SliceFromConst({Handle, Data, Count});
	PushInstruction(Builder, Instruction(OP_CALL, (void*)Call, FnT, Builder, 0));

	PushDebugBreak(Builder);

	u32 Abort = GetBuiltInFunction(Builder, Internal, STR_LIT("abort"));
	FnT = Builder->CurrentBlock.Code.GetLast()->Type;
	Call = NewType(call_info);
	Call->ErrorInfo = ErrorInfo;
	Call->Operand = Abort;
	Call->Args = {};
	PushInstruction(Builder, Instruction(OP_CALL, (void*)Call, FnT, Builder, 0));

	PushInstruction(Builder, Instruction(OP_UNREACHABLE, 0, Basic_type, Builder));
}

void BuildAssertExpr(block_builder *Builder, u32 Expr, const error_info *Info, string BonusMessage=STR_LIT(""))
{
	if(g_CompileFlags & CF_DisableAssert)
		return;

	basic_block AssertFailed = AllocateBlock(Builder);
	basic_block After = AllocateBlock(Builder);
	PushInstruction(Builder, Instruction(OP_IF, After.ID, AssertFailed.ID, Expr, Basic_bool));
	Terminate(Builder, AssertFailed);
	PushStepLocation(Builder, Info->Range.StartLine, Info->Range.StartChar);

	BuildAssertFailed(Builder, Info, BonusMessage);
	Terminate(Builder, After);
	PushStepLocation(Builder, Info->Range.StartLine, Info->Range.StartChar);
}

u32 BuildIRStringMatch(block_builder *Builder, node *Node)
{
	switch_context c = BuildIRGenericSwitchPart(Builder, Node);

	basic_block Start = AllocateBlock(Builder);
	PushStepLocation(Builder, Node);

	For(Builder->Function->Blocks)
	{
		if(it->ID == c.StartBlock)
		{
			Builder->CurrentBlock = *it;
			Builder->CurrentBlock.HasTerminator = false;
			PushInstruction(Builder, Instruction(OP_JMP, Start.ID, Basic_type, Builder));
			Builder->CurrentBlock.HasTerminator = true;
			*it = Builder->CurrentBlock;

			Builder->CurrentBlock = Start;
			break;
		}
	}

	ForArray(Idx, c.CaseValues)
	{
		u32 Cmp = BuildStringCompare(Builder, c.Matcher, c.CaseValues[Idx], Node);

		basic_block After;
		if(Idx + 1 != c.CaseValues.Count)
		{
			After = AllocateBlock(Builder);
		}
		else if(c.Default != -1)
		{
			PushInstruction(Builder, Instruction(OP_IF, c.CaseBlocks[Idx], c.CaseBlocks[c.Default], Cmp, Basic_bool));
			Terminate(Builder, c.After);

			break;
		}
		else
		{
			After = c.After;
		}
		PushInstruction(Builder, Instruction(OP_IF, c.CaseBlocks[Idx], After.ID, Cmp, Basic_bool));
		Terminate(Builder, After);
	}

	if(c.ResultReg != -1)
	{
		if(IsLoadableType(Node->Switch.ReturnType))
		{
			c.ResultReg = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, c.ResultReg, Node->Switch.ReturnType, Builder));
		}
	}
	return c.ResultReg;
}

u32 BuildIRSwitch(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_SWITCH);
	Assert(Node->Switch.Cases.Count != 0);
	//u32 Matcher = BuildIRFromExpression(Builder, Node->Match.Expression);

	u32 Result = -1;

	const type *MT = GetType(Node->Switch.SwitchType);
	if(HasBasicFlag(MT, BasicFlag_Integer))
	{
		Result = BuildIRIntMatch(Builder, Node);
	}
	else if(MT->Kind == TypeKind_Enum)
	{
		Node->Switch.SwitchType = MT->Enum.Type;
		Result = BuildIRIntMatch(Builder, Node);
	}
	else if(IsString(MT))
	{
		Result = BuildIRStringMatch(Builder, Node);
	}
	else
	{
		Assert(false);
	}


	return Result;
}

u32 BuildRun(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_RUN);

	basic_block RunBlock = AllocateBlock(Builder);
	basic_block Save = Builder->CurrentBlock;
	Builder->CurrentBlock = RunBlock;
	PushStepLocation(Builder, Node);

	For(Node->Run.Body)
	{
		BuildIRFunctionLevel(Builder, *it);
	}

	{
		instruction Unreachable = {};
		Unreachable.Op = OP_UNREACHABLE;
		Unreachable.Right = true;
		PushInstruction(Builder, Unreachable);
	}

	Terminate(Builder, Save);

	Builder->RunIndexes.Push(run_location{Save.ID, (uint)Save.Code.Count});
	return PushInstruction(Builder, Instruction(OP_RUN, 0, RunBlock.ID, Node->Run.TypeIdx, Builder));
}

u32 BuildIRFromAtom(block_builder *Builder, node *Node, b32 IsLHS)
{
	u32 Result = -1;
	PushErrorInfo(Builder, Node);
	switch(Node->Type)
	{
		case AST_ID:
		{
			if(Node->ID.Type != INVALID_TYPE)
			{
				Result = PushInstruction(Builder, 
						Instruction(OP_CONSTINT, Node->ID.Type, Basic_uint, Builder));
				break;
			}

			b32 IsGlobal = false;
			const symbol *Local = GetIRLocal(Builder, Node->ID.Name, true, &IsGlobal);
			if(IsGlobal)
			{
				instruction GlobalI = Instruction(OP_GLOBAL, (void *)Local, Local->Type, Builder, 0);

				Result = PushInstruction(Builder, GlobalI);
			}
			else
			{
				Result = Local->Register;
			}

			b32 ShouldLoad = true;
			if(IsLHS)
				ShouldLoad = false;

			const type *Type = GetType(Local->Type);
#if 0
			if(Type->Kind != TypeKind_Basic && Type->Kind != TypeKind_Pointer && Type->Kind != TypeKind_Enum)
			{
				ShouldLoad = false;
			}
			else
			{
				if(Type->Kind == TypeKind_Basic && Type->Basic.Kind == Basic_string)
					ShouldLoad = false;
			}
#endif
			
			if(ShouldLoad && IsLoadableType(Type))
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Local->Type, Builder));
		} break;
		case AST_FILE_LOCATION:
		{
			const error_info *ei = Node->ErrorInfo;
			if(Builder->LastErrorInfo)
			{
				ei = Builder->LastErrorInfo;
			}
			u32 FLType = FindStruct(STR_LIT("base.FileLocation"));
			u32 Alloc = PushInstruction(Builder, 
					Instruction(OP_ALLOC, -1, FLType, Builder));
			u32 FilePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, Alloc, 0, FLType, Builder));
			u32 LinePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, Alloc, 1, FLType, Builder));

			string FN = MakeString(ei->FileName);
			string *FileName = DupeType(FN, string);
			const_value *StringValue = NewType(const_value);
			StringValue->Type = const_type::String;
			StringValue->String.Data = FileName;
			u32 FileNameString = PushInstruction(Builder, 
					Instruction(OP_CONST, (u64)StringValue, Basic_string, Builder));
			u32 LineInt = PushInt(ei->Range.StartLine, Builder);


			PushInstruction(Builder, InstructionStore(FilePtr, FileNameString, Basic_string));
			PushInstruction(Builder, InstructionStore(LinePtr, LineInt, Basic_int));

			Result = Alloc;
		} break;
		case AST_IFX:
		{
			u32 Condition = BuildIRFromExpression(Builder, Node->IfX.Expr);
		    basic_block ThenBlock = AllocateBlock(Builder);
		    basic_block ElseBlock = AllocateBlock(Builder);
		    basic_block EndBlock  = AllocateBlock(Builder);

			Result = PushAlloc(Node->IfX.TypeIdx, Builder);
		    PushInstruction(Builder, Instruction(OP_IF, ThenBlock.ID, ElseBlock.ID, Condition, Basic_bool));
			Terminate(Builder, ThenBlock);
			PushStepLocation(Builder, Node);


			u32 TrueExpr = BuildIRFromExpression(Builder, Node->IfX.True);
			PushInstruction(Builder, InstructionStore(Result, TrueExpr, Node->IfX.TypeIdx));
			PushInstruction(Builder, 
					Instruction(OP_JMP, EndBlock.ID, Basic_type, Builder));
			Terminate(Builder, ElseBlock);
			PushStepLocation(Builder, Node);

			u32 FalseExpr = BuildIRFromExpression(Builder, Node->IfX.False);
			PushInstruction(Builder, InstructionStore(Result, FalseExpr, Node->IfX.TypeIdx));
			PushInstruction(Builder, 
					Instruction(OP_JMP, EndBlock.ID, Basic_type, Builder));

			Terminate(Builder, EndBlock);
			PushStepLocation(Builder, Node);

			if(!IsLHS)
			{
				Result = PushInstruction(Builder,
						Instruction(OP_LOAD, 0, Result, Node->IfX.TypeIdx, Builder));
			}
		} break;
		case AST_POSTOP:
		{
			u32 T = Node->PostOp.TypeIdx;
			u32 Location = BuildIRFromExpression(Builder, Node->PostOp.Operand, true);
			Result = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Location, T, Builder));

			u32 ToStore = -1;
			if(Node->PostOp.Type == T_PPLUS)
			{
				if(GetType(T)->Kind == TypeKind_Pointer)
				{
					u32 One = PushInt(1, Builder, Basic_int);
					ToStore = PushInstruction(Builder, Instruction(OP_INDEX, Result, One, T, Builder));
				}
				else
				{
					u32 One = PushInt(1, Builder, T);
					ToStore = PushInstruction(Builder, Instruction(OP_ADD, Result, One, T, Builder));
				}
			}
			else
			{
				Assert(Node->PostOp.Type == T_MMIN);
				if(GetType(T)->Kind == TypeKind_Pointer)
				{
					u32 One = PushInt(-1, Builder, Basic_int);
					ToStore = PushInstruction(Builder, Instruction(OP_INDEX, Result, One, T, Builder));
				}
				else
				{
					u32 One = PushInt(1, Builder, T);
					ToStore = PushInstruction(Builder, Instruction(OP_SUB, Result, One, T, Builder));
				}
			}
			PushInstruction(Builder, InstructionStore(Location, ToStore, T));
		} break;
		case AST_RUN:
		{
			Result = BuildRun(Builder, Node);
		} break;
		case AST_LIST:
		{
			u32 ResultPtr = PushInstruction(Builder,
					Instruction(OP_ALLOC, -1, Node->List.WholeType, Builder));
			array<u32> Expressions(Node->List.Nodes.Count);
			ForArray(Idx, Node->List.Nodes)
			{
				Expressions[Idx] = BuildIRFromExpression(Builder, Node->List.Nodes[Idx], IsLHS);
			}
			ForArray(Idx, Expressions)
			{
				u32 MemPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, ResultPtr, Idx, Node->List.WholeType, Builder));
				PushInstruction(Builder, InstructionStore(MemPtr, Expressions[Idx], Node->List.Types[Idx]));
			}

			Result = ResultPtr;
		} break;
		case AST_TYPEINFO:
		{
			static string InitModuleName = STR_LIT("base");
			string TableID = STR_LIT("type_table");
			const symbol *s = NULL;
			For(CurrentModules)
			{
				if((*it)->Name == InitModuleName)
				{
					s = (*it)->Globals[TableID];
					Assert(s);
				}
			}

			u32 Idx = BuildIRFromExpression(Builder, Node->TypeInfoLookup.Expression);

			Result = PushInstruction(Builder, Instruction(OP_TYPEINFO, s->Register, Idx, Node->TypeInfoLookup.Type, Builder));
			if(IsLHS)
			{
				u32 Ptr = Result;
				Result = PushInstruction(Builder, Instruction(OP_ALLOC, -1, GetPointerTo(Node->TypeInfoLookup.Type), Builder));
				PushInstruction(Builder, InstructionStore(Result, Ptr, GetPointerTo(Node->TypeInfoLookup.Type)));
			}
			
		} break;
		case AST_SWITCH:
		{
			Result = BuildIRSwitch(Builder, Node);
		} break;
		case AST_RESERVED:
		{
			const_value *Val = NewType(const_value);
			Val->Type = const_type::Integer;
			using rs = reserved;
			switch(Node->Reserved.ID)
			{
				case rs::Null:
				{
					Result = PushInstruction(Builder, 
							Instruction(OP_NULL, (u64)0, Node->Reserved.Type, Builder));
				} break;
				case rs::False:
				{
					Val->Int.IsSigned = false;
					Val->Int.Unsigned = 0;
				} break;
				case rs::True:
				{
					Val->Int.IsSigned = false;
					Val->Int.Unsigned = 1;
				} break;
				case rs::Inf:
				{
					Val->Type = const_type::Float;
					Val->Float = INFINITY;
				} break;
				case rs::NaN:
				{
					Val->Type = const_type::Float;
					Val->Float = NAN;
				} break;
				default: unreachable;
			}
			if(Node->Reserved.ID != rs::Null)
			{
				Result = PushInstruction(Builder, 
						Instruction(OP_CONST, (u64)Val, Node->Reserved.Type, Builder));
			}
		} break;
		case AST_EMBED:
		{
			if(Node->Embed.IsString)
			{
				const_value *EmbedValue = NewType(const_value);
				EmbedValue->Type = const_type::String;
				EmbedValue->String.Data = &Node->Embed.Content;
				Result = PushInstruction(Builder, 
						Instruction(OP_CONST, (u64)EmbedValue, Basic_string, Builder));
			}
			else
			{
				interp_slice *Slice = NewType(interp_slice);
				Slice->Count = Node->Embed.Content.Size;
				Slice->Data = (void *)Node->Embed.Content.Data;

				const_value *EmbedValue = NewType(const_value);
				EmbedValue->Type = const_type::Aggr;
				EmbedValue->Struct.Ptr = Slice;
				Result = PushInstruction(Builder, 
						Instruction(OP_CONST, (u64)EmbedValue, GetSliceType(Basic_u8), Builder));
			}
		} break;
		case AST_TYPEOF:
		{
			Result = PushInstruction(Builder, 
					Instruction(OP_CONSTINT, Node->TypeOf.Type, Basic_int, Builder));
		} break;
		case AST_SIZE:
		{
			const type *Type = GetType(Node->Size.Type);
			const_value *Size = NewType(const_value);
			Size->Type = const_type::Integer;
			Size->Int.IsSigned = false;
			Size->Int.Unsigned = GetTypeSize(Type);
			Result = PushInstruction(Builder, 
					Instruction(OP_CONST, (u64)Size, Basic_int, Builder));
		} break;
		case AST_CALL:
		{
			//Assert(!IsLHS);
			call_info *CallInfo = NewType(call_info);
			CallInfo->ErrorInfo = Node->ErrorInfo;

#if 0
			if(Node->Call.Fn->Type == AST_ID || Node->Call.Fn->Type == AST_FN)
			{
				static const string Intrinsics[] = {
					STR_LIT("__raw_slice"),
				};

				string Name = {};
				if(Node->Call.Fn->Type == AST_ID) {
					Name = *Node->Call.Fn->ID.Name;
				}
				else if(Node->Call.Fn->Type == AST_FN) {
					Name = *Node->Call.Fn->Fn.Name;
				}
				else {
					unreachable;
				}

				Name = MakeNonGenericName(Name);
				int Size = ARR_LEN(Intrinsics);
				b32 Found = false;
				for(int i = 0; i < Size; ++i)
				{
					if(Name == Intrinsics[i])
					{
						Found = true;
						switch(i)
						{
							case 0:
							{
								u32 Type = GetSliceType(Node->Call.GenericTypes[0]);
								u32 Ptr = BuildIRFromExpression(Builder, Node->Call.Args[1]);
								u32 Size = BuildIRFromExpression(Builder, Node->Call.Args[2]);
								Result = BuildSlice(Builder, Ptr, Size, Type);
							} break;
							default: unreachable;
						}
					}
				}
				if(Found) break;
			}
#endif
			op Op = OP_CALL;
			if(Node->Call.SymName.Data != NULL)
			{
				if(Node->Call.SymName == "compare_exchange")
				{
					Op = OP_CMPXCHG;
				}
				else if(Node->Call.SymName == "debug_break")
				{
					Op = OP_DEBUG_BREAK;
				}
				else if(Node->Call.SymName == "fence")
				{
					Op = OP_FENCE;
				}
				else if(Node->Call.SymName == "atomic_load")
				{
					Op = OP_ATOMIC_LOAD;
				}
				else if(Node->Call.SymName == "atomic_add")
				{
					Op = OP_ATOMIC_ADD;
				}
			}
			else
			{
				CallInfo->Operand = BuildIRFromExpression(Builder, Node->Call.Fn, IsLHS);
			}

			const type *Type = GetType(Node->Call.Type);
			if(Type->Kind == TypeKind_Pointer)
				Type = GetType(Type->Pointer.Pointed);
			Assert(Type->Kind == TypeKind_Function);
			dynamic<u32> Args = {};

			u32 ResultPtr = -1;
			u32 ReturnedWrongType = -1;
			if(Type->Function.Returns.Count != 0 && Op != OP_CMPXCHG)
			{
				u32 RTID = ReturnsToType(Type->Function.Returns);
				const type *RT = GetType(RTID);
				if(IsRetTypePassInPointer(RTID))
				{
					ResultPtr = PushInstruction(Builder,
							Instruction(OP_ALLOC, -1, RTID, Builder));
					Args.Push(ResultPtr);
				}
				else if(RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array)
				{
					if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT) && IsUnix())
						ReturnedWrongType = AllFloatsStructToReturnType(RT);
					else
						ReturnedWrongType = ComplexTypeToSizeType(RT);
				}
			}
			u32 VarArgSliceType = INVALID_TYPE;
			u32 VarArgArrayT = INVALID_TYPE;
			u32 VarArgT = INVALID_TYPE;
			u32 VarArgAlloc = -1;
			u32 VarArgsArray = -1;
			u32 PassedVarArgs = 0;
			if(Type->Function.Flags & SymbolFlag_VarFunc &&
					!IsForeign(Type))
			{
				VarArgT          = FindStruct(STR_LIT("base.Arg"));
				VarArgSliceType  = GetSliceType(VarArgT);
				VarArgAlloc = PushInstruction(Builder, 
						Instruction(OP_ALLOC, -1, VarArgSliceType, Builder));
				u32 CountMemberLocation = PushInstruction(Builder, 
						Instruction(OP_INDEX, VarArgAlloc, 0, VarArgSliceType, Builder));
				u32 ArrayMemberLocation = PushInstruction(Builder, 
						Instruction(OP_INDEX, VarArgAlloc, 1, VarArgSliceType, Builder));

				u32 PassedArgs = Node->Call.Args.Count - Type->Function.ArgCount;;
				u32 Count = PushInt(PassedArgs, Builder);
				VarArgArrayT = VarArgArrayType(PassedArgs, VarArgT);

				VarArgsArray = PushInstruction(Builder,
						Instruction(OP_ALLOC, -1, VarArgArrayT, Builder));

				PushInstruction(Builder,
						InstructionStore(CountMemberLocation, Count, Basic_int));
				PushInstruction(Builder,
						InstructionStore(ArrayMemberLocation, VarArgsArray, GetPointerTo(VarArgT)));
			}
			for(int Idx = 0; Idx < Node->Call.Args.Count; ++Idx)
			{
				if(Idx >= Type->Function.ArgCount)
				{
					if(IsForeign(Type))
					{
						u32 ArgType = Node->Call.ArgTypes[Idx];
						if(!IsLoadableType(ArgType))
						{
							FixCallWithComplexParameter(Builder, Args, ArgType, Node->Call.Args[Idx], IsLHS, Type->Function.Flags & SymbolFlag_Foreign);
						}
						else
						{
							u32 Expr = BuildIRFromExpression(Builder, Node->Call.Args[Idx], IsLHS);
							Args.Push(Expr);
						}
					}
					else
					{
						// Var args
						u32 ArgType = Node->Call.ArgTypes[Idx];
						u32 Expr = BuildIRFromExpression(Builder, Node->Call.Args[Idx]);
						u32 Index = PushInt(PassedVarArgs++, Builder);

						/*
						 * %0 = EXPR
						 * %1 = alloc(ExprType)
						 * %1 = store %0
						 */
						u32 ExprLocation = PushInstruction(Builder,
								Instruction(OP_ALLOC, -1, ArgType, Builder));
						PushInstruction(Builder, InstructionStore(ExprLocation, Expr, Node->Call.ArgTypes[Idx]));

						/*
						 * %2 = INDEX %VarArgs idx
						 * %3 = INDEX %2 0
						 * %4 = INDEX %2 1
						 * %3 = store type
						 * %4 = store %1
						 */
						u32 ArgLocation = PushInstruction(Builder, Instruction(OP_INDEX, VarArgsArray, Index, VarArgArrayT, Builder));

						u32 TypeLocation = PushInstruction(Builder, Instruction(OP_INDEX, ArgLocation, 0, VarArgT, Builder));
						u32 ValLocation = PushInstruction(Builder, Instruction(OP_INDEX, ArgLocation, 1, VarArgT, Builder));
						PushInstruction(Builder, InstructionStore(TypeLocation, PushInt(ArgType, Builder), Basic_type));
						PushInstruction(Builder, InstructionStore(ValLocation, ExprLocation, GetPointerTo(ArgType)));
					}
				}
				else
				{
					const type *ArgType = GetType(Type->Function.Args[Idx]);
					if(!IsLoadableType(ArgType))
					{
						FixCallWithComplexParameter(Builder, Args, Type->Function.Args[Idx], Node->Call.Args[Idx], IsLHS, Type->Function.Flags & SymbolFlag_Foreign);
					}
					else
					{
						u32 Expr = BuildIRFromExpression(Builder, Node->Call.Args[Idx], false);
						Args.Push(Expr);
					}
				}
			}

			// Default args
			for(int Idx = Node->Call.Args.Count; Idx < Type->Function.ArgCount; ++Idx)
			{
				default_value Default = {};
				For(Type->Function.DefaultValues)
				{
					if(it->Idx == Idx) Default = *it;
				}
				Assert(Default.Default);
				u32 Expr = BuildIRFromExpression(Builder, Default.Default);
				Args.Push(Expr);
			}

			if(Type->Function.Flags & SymbolFlag_VarFunc && !IsForeign(Type))
				Args.Push(VarArgAlloc);

			CallInfo->Args = SliceFromArray(Args);

			u32 CallType = Node->Call.Type;
			if(ReturnedWrongType != -1)
			{
				array<u32> Returns(1);
				Returns[0] = ReturnedWrongType;
				type *NT = AllocType(TypeKind_Function);
				*NT = *Type;
				NT->Function.Returns = SliceFromArray(Returns);
				CallType = AddType(NT);
			}

			Result = PushInstruction(Builder, Instruction(Op, (u64)CallInfo, CallType, Builder));
			if(ResultPtr != -1)
				Result = ResultPtr;
			else if(ReturnedWrongType != -1)
			{
				u32 R = ReturnsToType(Type->Function.Returns);
				u32 Alloced = PushInstruction(Builder,
						Instruction(OP_ALLOC, -1, R, Builder));
				Result = PushInstruction(Builder,
						InstructionStore(Alloced, Result, ReturnedWrongType));
			}
		} break;
		case AST_TYPELIST:
		{
			u32 TypeIdx = Node->TypeList.Type;
			const type *Type = GetType(Node->TypeList.Type);
			if(Type->Kind == TypeKind_Vector)
			{
				// Zero vector
				Result = PushInstruction(Builder, Instruction(OP_INSERT, NULL, TypeIdx, Builder, 0));
			}
			else
			{
				u32 Alloc = PushInstruction(Builder, Instruction(OP_ALLOC, -1, Node->TypeList.Type, Builder));
				PushInstruction(Builder, InstructionMemset(Alloc, Node->TypeList.Type));
				Result = Alloc;
				if(Node->TypeList.Items.Count == 0)
				{
					break;
				}
			}
			switch(Type->Kind)
			{
				default: unreachable;
				case TypeKind_Vector:
				{
					if(Node->TypeList.Items.Count == 1)
					{
						u32 Register = BuildIRFromExpression(Builder, Node->TypeList.Items[0]->Item.Expression, IsLHS);
						for(int Idx = 0; Idx < Type->Vector.ElementCount; Idx++)
						{
							ir_insert *Ins = NewType(ir_insert);
							Ins->Idx = Idx;
							Ins->Register = Result;
							Ins->ValueRegister = Register;

							Result = PushInstruction(Builder, Instruction(OP_INSERT, Ins, TypeIdx, Builder, 0));
						}
					}
					else
					{
						ForArray(Idx, Node->TypeList.Items)
						{
							u32 Register = BuildIRFromExpression(Builder, Node->TypeList.Items[Idx]->Item.Expression, IsLHS);

							ir_insert *Ins = NewType(ir_insert);
							Ins->Idx = Idx;
							Ins->Register = Result;
							Ins->ValueRegister = Register;

							Result = PushInstruction(Builder, Instruction(OP_INSERT, Ins, TypeIdx, Builder, 0));
						}
					}
				} break;
				case TypeKind_Array:
				{
					u32 Alloc = Result;

					array_list_info *Info = NewType(array_list_info);
					u32 *Registers = (u32 *)VAlloc(Node->TypeList.Items.Count * sizeof(u32));
					ForArray(Idx, Node->TypeList.Items)
					{
						u32 Register = BuildIRFromExpression(Builder, Node->TypeList.Items[Idx]->Item.Expression, IsLHS);
						Registers[Idx] = Register;
					}
					Info->Alloc = Alloc;
					Info->Registers = Registers;
					Info->Count = Node->TypeList.Items.Count;
					instruction I = Instruction(OP_ARRAYLIST, (u64)Info, Node->TypeList.Type, Builder);
					PushInstruction(Builder, I);
				} break;
				case TypeKind_Slice:
				{
					u32 Alloc = Result;

					array_list_info *Info = NewType(array_list_info);
					u32 *Registers = (u32 *)VAlloc(Node->TypeList.Items.Count * sizeof(u32));
					ForArray(Idx, Node->TypeList.Items)
					{
						u32 Register = BuildIRFromExpression(Builder, Node->TypeList.Items[Idx]->Item.Expression, IsLHS);
						Registers[Idx] = Register;
					}
					u32 ArrayType = GetArrayType(Type->Slice.Type, Node->TypeList.Items.Count);
					u32 ArrayPtr = -1;
					if(Builder->IsGlobal)
					{
						ArrayPtr = PushInstruction(Builder,
								Instruction(OP_ALLOCGLOBAL, Node->TypeList.Items.Count, Type->Slice.Type, Builder));
					}
					else
					{
						ArrayPtr = PushInstruction(Builder,
								Instruction(OP_ALLOC, -1, ArrayType, Builder));
					}

					Info->Alloc = ArrayPtr;
					Info->Registers = Registers;
					Info->Count = Node->TypeList.Items.Count;
					instruction I = Instruction(OP_ARRAYLIST, (u64)Info, ArrayType, Builder);
					PushInstruction(Builder, I);
					BuildSlice(Builder, ArrayPtr,
							PushInt(Node->TypeList.Items.Count, Builder),
							Node->TypeList.Type, Type, Alloc);
				} break;
				case TypeKind_Struct:
				{
					u32 Alloc = Result;

					ForArray(Idx, Node->TypeList.Items)
					{
						node *Item = Node->TypeList.Items[Idx];
						const string *NamePtr = Item->Item.Name;
						int MemberIdx = Idx;
						if(NamePtr)
						{
							string Name = *NamePtr;
							int Found = -1;
							ForArray(MIdx, Type->Struct.Members)
							{
								struct_member Mem = Type->Struct.Members[MIdx];
								if(Mem.ID == Name)
								{
									Found = MIdx;
									break;
								}
							}
							if(Found == -1)
							{
								unreachable;
							}
							MemberIdx = Found;
						}
						struct_member Mem = Type->Struct.Members[MemberIdx];
						const type *MemberType = GetType(Mem.Type);
						u32 Expr = BuildIRFromExpression(Builder, Item->Item.Expression, false);
						if(MemberType->Kind == TypeKind_Basic && MemberType->Basic.Kind == Basic_string)
						{
							Expr = PushInstruction(Builder,
									Instruction(OP_LOAD, 0, Expr, Mem.Type, Builder));
						}
						u32 Location = PushInstruction(Builder,
								Instruction(OP_INDEX, Alloc, MemberIdx, Node->TypeList.Type, Builder));
						PushInstruction(Builder,
								InstructionStore(Location, Expr, Mem.Type));
					}
				} break;
				case TypeKind_Basic:
				{
					u32 Alloc = Result;

					Assert(IsString(Type));
					ForArray(Idx, Node->TypeList.Items)
					{
						node *Item = Node->TypeList.Items[Idx];
						const string *NamePtr = Item->Item.Name;
						int MemberIdx = Idx;
						if(NamePtr)
						{
							string Name = *NamePtr;
							if(Name == STR_LIT("data"))
							{
								MemberIdx = 1;
							}
							else if(Name == STR_LIT("count"))
							{
								MemberIdx = 0;
							}
							else
							{
								unreachable;
							}
						}
						u32 Expr = BuildIRFromExpression(Builder, Item->Item.Expression, false);
						u32 MemType = INVALID_TYPE;
						if(MemberIdx == 0)
							MemType = GetPointerTo(Basic_u8);
						else
							MemType = Basic_int;
						u32 Location = PushInstruction(Builder,
								Instruction(OP_INDEX, Alloc, MemberIdx, Node->TypeList.Type, Builder));
						PushInstruction(Builder,
								InstructionStore(Location, Expr, MemType));
					}
				} break;
			}
			PushResult(Result, Node->TypeList.Type, Builder);
		} break;
		case AST_PTRDIFF:
		{
			u32 Left = BuildIRFromExpression(Builder, Node->PtrDiff.Left);
			u32 Right = BuildIRFromExpression(Builder, Node->PtrDiff.Right);
			Result = PushInstruction(Builder, Instruction(OP_PTRDIFF, Left, Right, Node->PtrDiff.Type, Builder));
		} break;
		case AST_INDEX:
		{
			const type *Type = GetType(Node->Index.OperandType);
			b32 ShouldNotLoad = IsLHS;
			// @TODO: see if this is removable
			if(Type->Kind == TypeKind_Pointer)
				ShouldNotLoad = false;
			u32 Operand = BuildIRFromExpression(Builder, Node->Index.Operand, ShouldNotLoad);
			u32 Index = BuildIRFromExpression(Builder, Node->Index.Expression, false);
			b32 DontLoadResult = !IsLoadableType(Node->Index.IndexedType);
			if(Type->Kind == TypeKind_Slice)
			{
				if((g_CompileFlags & CF_DisableAssert) == 0)
				{
					u32 IndexV = PushInstruction(Builder, Instruction(OP_CAST, Index, Node->Index.IndexExprType, Basic_int, Builder));

					u32 CountPtr = PushInstruction(Builder, Instruction(OP_INDEX, Operand, 0, Node->Index.OperandType, Builder));
					u32 LoadedCount = PushInstruction(Builder, Instruction(OP_LOAD, 0, CountPtr, Basic_int, Builder));
					u32 AssertCond = PushInstruction(Builder, Instruction(OP_LESS, IndexV, LoadedCount, Basic_bool, Builder));
					BuildAssertExpr(Builder, AssertCond, Node->ErrorInfo, STR_LIT("Trying to index slice out of bounds!"));
				}

				u32 PtrToIdxed = GetPointerTo(Node->Index.IndexedType);
				u32 DataPtr = PushInstruction(Builder, Instruction(OP_INDEX, Operand, 1, Node->Index.OperandType, Builder));
				u32 LoadedPtr = PushInstruction(Builder, Instruction(OP_LOAD, 0, DataPtr, PtrToIdxed, Builder));
				Result = PushInstruction(Builder, Instruction(OP_INDEX, LoadedPtr, Index, PtrToIdxed, Builder));
				if(!IsLHS && !Node->Index.ForceNotLoad && !DontLoadResult)
					Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Node->Index.IndexedType, Builder));
			}
			else if(HasBasicFlag(Type, BasicFlag_String))
			{
				if((g_CompileFlags & CF_DisableAssert) == 0)
				{
					u32 IndexV = PushInstruction(Builder, Instruction(OP_CAST, Index, Node->Index.IndexExprType, Basic_int, Builder));

					u32 CountPtr = PushInstruction(Builder, Instruction(OP_INDEX, Operand, 0, Basic_string, Builder));
					u32 LoadedCount = PushInstruction(Builder, Instruction(OP_LOAD, 0, CountPtr, Basic_int, Builder));
					u32 AssertCond = PushInstruction(Builder, Instruction(OP_LESS, IndexV, LoadedCount, Basic_bool, Builder));
					BuildAssertExpr(Builder, AssertCond, Node->ErrorInfo, STR_LIT("Trying to index string out of bounds!"));
				}

				u32 u8ptr = GetPointerTo(Basic_u8);
				Result = PushInstruction(Builder, Instruction(OP_INDEX, Operand, 1, Basic_string, Builder));
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, u8ptr, Builder));
				Result = PushInstruction(Builder, Instruction(OP_INDEX, Result, Index, u8ptr, Builder));
				if(!IsLHS)
					Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Node->Index.IndexedType, Builder));
			}
			else
			{
				if((g_CompileFlags & CF_DisableAssert) == 0 && Type->Kind == TypeKind_Array)
				{
					u32 IndexV = PushInstruction(Builder, Instruction(OP_CAST, Index, Node->Index.IndexExprType, Basic_int, Builder));

					u32 Count = PushInt(Type->Array.MemberCount, Builder);
					u32 AssertCond = PushInstruction(Builder, Instruction(OP_LESS, IndexV, Count, Basic_bool, Builder));
					BuildAssertExpr(Builder, AssertCond, Node->ErrorInfo, STR_LIT("Trying to index slice out of bounds!"));
				}

				Result = PushInstruction(Builder, Instruction(OP_INDEX, Operand, Index, Node->Index.OperandType, Builder));
				if(!IsLHS && !Node->Index.ForceNotLoad && !DontLoadResult)
					Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Node->Index.IndexedType, Builder));
			}
		} break;
		case AST_SELECTOR:
		{
			if(Node->Selector.Index == -1)
			{
				Assert(Node->Selector.Operand->Type == AST_ID);
				string SymName = *Node->Selector.Member;
				string ModuleName = *Node->Selector.Operand->ID.Name;
				string *Mangled = StructToModuleNamePtr(SymName, ModuleName);
				// @TODO: This is for selecting enums from modules
				// Ex. my_mod.my_enum.VALUE
				// but it's kinda hacky, find a better way to do this
				b32 IsGlobal = false;
				const symbol *Sym = GetIRLocal(Builder, Mangled, false, &IsGlobal);
				if(!Sym)
				{
					string Name = *Mangled;
					Result = -1;
					u32 TC = GetTypeCount();
					for(int I = 0; I < TC; ++I)
					{
						const type *Type = GetType(I);
						switch(Type->Kind)
						{
							case TypeKind_Struct:
							{
								if(Type->Struct.Name == Name)
								{
									Result = PushInstruction(Builder, 
											Instruction(OP_CONSTINT, I, Basic_int, Builder));
									goto SEARCH_TYPE_DONE;
								}

							} break;
							case TypeKind_Enum:
							{
								if(Type->Enum.Name == Name)
								{
									Result = PushInstruction(Builder, 
											Instruction(OP_CONSTINT, I, Basic_int, Builder));

									goto SEARCH_TYPE_DONE;
								}
							} break;
							default: continue;
						}
					}
SEARCH_TYPE_DONE:
					Assert(Result != -1);
				}
				else
				{
					const type *Type = GetType(Node->Selector.Type);
					if(IsGlobal)
					{
						instruction GlobalI = Instruction(OP_GLOBAL, (void *)Sym, Sym->Type, Builder, 0);

						Result = PushInstruction(Builder, GlobalI);
					}
					else
					{
						Result = Sym->Register;
					}
					if(!IsLHS && !IsFn(Type))
					{
						Result = PushInstruction(Builder,
								Instruction(OP_LOAD, 0, Result, Node->Selector.Type, Builder));
					}
				}
			}
			else
			{
				const type *Type = GetType(Node->Selector.Type);
				u32 TypeIdx = Node->Selector.Type;
				u32 Operand = -1;
				if(Type->Kind != TypeKind_Enum && Type->Kind != TypeKind_Pointer && Type->Kind != TypeKind_Vector)
				{
					Assert(Node->Selector.Operand);
					Operand = BuildIRFromExpression(Builder, Node->Selector.Operand, true);
				}
				
				switch(Type->Kind)
				{
					case TypeKind_Vector:
					{
						if(IsLHS)
						{
							Operand = BuildIRFromExpression(Builder, Node->Selector.Operand, true);
							u32 Index = PushInt(Node->Selector.Index, Builder);
							u32 ElemT = GetVecElemType(Type);
							Result = PushInstruction(Builder, 
									Instruction(OP_INDEX, Operand, Index, GetPointerTo(ElemT), Builder));
						}
						else
						{
							Operand = BuildIRFromExpression(Builder, Node->Selector.Operand, false);
							Result = PushInstruction(Builder, Instruction(OP_EXTRACT, Operand, Node->Selector.Index, TypeIdx, Builder));
						}
					} break;
					case TypeKind_Basic:
					{
						Assert(IsString(Type));
						Result = PushInstruction(Builder, 
								Instruction(OP_INDEX, Operand, Node->Selector.Index, TypeIdx, Builder));
						if(!IsLHS)
						{
							u32 T = Basic_int;
							if(Node->Selector.Index == 1)
								T = GetPointerTo(Basic_u8);
							Result = PushInstruction(Builder, 
									Instruction(OP_LOAD, 0, Result, T, Builder));
						}
					} break;
					case TypeKind_Slice: 
					{
BUILD_SLICE_SELECTOR:
						u32 SelectType = Basic_int;
						if(Node->Selector.Index == 1)
							SelectType = GetPointerTo(Type->Slice.Type);

						Result = PushInstruction(Builder, 
								Instruction(OP_INDEX, Operand, Node->Selector.Index, TypeIdx, Builder));
						if(!IsLHS)
						{
							Result = PushInstruction(Builder, 
									Instruction(OP_LOAD, 0, Result, SelectType, Builder));
						}
					} break;
					case TypeKind_Enum:
					{
						Result = PushInstruction(Builder, 
								Instruction(OP_ENUM_ACCESS, 0, Node->Selector.Index, TypeIdx, Builder));
					} break;
					case TypeKind_Pointer:
					Assert(Node->Selector.Operand);
					case TypeKind_Struct:
					{
						if(Type->Kind == TypeKind_Pointer)
						{
							Operand = BuildIRFromExpression(Builder, Node->Selector.Operand, false);
							//Operand = StructPointerToStruct(Builder, Operand, Type);

							TypeIdx = Type->Pointer.Pointed;
							Type = GetType(Type->Pointer.Pointed);
							if(Type->Kind == TypeKind_Slice)
								goto BUILD_SLICE_SELECTOR;
						}

						Result = PushInstruction(Builder, 
								Instruction(OP_INDEX, Operand, Node->Selector.Index, TypeIdx, Builder));

						u32 MemberTypeIdx = Type->Struct.Members[Node->Selector.Index].Type;
						if(Node->Selector.SubIndex != -1)
						{
							Result = PushInstruction(Builder, 
									Instruction(OP_INDEX, Result, Node->Selector.SubIndex, MemberTypeIdx, Builder));
							MemberTypeIdx = GetType(MemberTypeIdx)->Struct.Members[Node->Selector.SubIndex].Type;
						}

						if(!IsLHS && IsLoadableType(MemberTypeIdx))
						{
							Result = PushInstruction(Builder,
									Instruction(OP_LOAD, 0, Result, MemberTypeIdx, Builder));
						}
					} break;
					default: unreachable;
				}
			}

		} break;
		case AST_CHARLIT:
		{
			u32 T = INVALID_TYPE;
			if((Node->CharLiteral.C & 0xFF) == Node->CharLiteral.C)
			{
				T = Basic_u8;
			}
			else
			{
				T = Basic_u32;
			}
			Result = PushInt(Node->CharLiteral.C, Builder, T);
		} break;
		case AST_CONSTANT:
		{
			instruction I;
			I = Instruction(OP_CONST, (u64)&Node->Constant.Value, Node->Constant.Type, Builder);

			Result = PushInstruction(Builder, I);
		} break;
		case AST_CAST:
		{

			const type *To = GetType(Node->Cast.ToType);
			const type *From = GetType(Node->Cast.FromType);

			if(From->Kind == TypeKind_Pointer && To->Kind == TypeKind_Pointer)
			{
				u32 Expression = BuildIRFromExpression(Builder, Node->Cast.Expression, IsLHS);
				Result = PushInstruction(Builder, Instruction(OP_PTRCAST, 0, Expression, Node->Cast.ToType, Builder));
			}
			else
			{
				if(Node->Cast.IsBitCast)
				{
					if(!IsLoadableType(Node->Cast.FromType))
					{
						Result = PushAlloc(Node->Cast.ToType, Builder);
						u32 Ptr = BuildIRFromExpression(Builder, Node->Cast.Expression, true);
						PushInstruction(Builder, Instruction(OP_MEMCPY, Result, Ptr, Result, Node->Cast.FromType));
					}
					else
					{
						u32 Expression = BuildIRFromExpression(Builder, Node->Cast.Expression, IsLHS);
						instruction I = Instruction(OP_BITCAST, Node->Cast.FromType, Expression, Node->Cast.ToType, Builder);
						Result = PushInstruction(Builder, I);
					}
				}
				else
				{
					u32 Expression = BuildIRFromExpression(Builder, Node->Cast.Expression, IsLHS);
					instruction I = Instruction(OP_CAST, Expression, Node->Cast.FromType, Node->Cast.ToType, Builder);
					Result = PushInstruction(Builder, I);
				}
			}
		} break;
		case AST_FN:
		{
			function fn = BuildFunctionIR(Builder->Function->IR, Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, Builder->Imported);

			Result = PushInstruction(Builder,
					Instruction(OP_FN, DupeType(fn, function), fn.Type, Builder, 0));
			/*
			   PushIRLocal(Builder->Function, fn.Name, Result,
			   fn.Type, SymbolFlag_Function);
			   */
		} break;
		case AST_PTRTYPE:
		{
			Result = PushInt(Node->PointerType.Analyzed, Builder);
		} break;
		case AST_ARRAYTYPE:
		{
			Result = PushInt(Node->ArrayType.Analyzed, Builder);
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
				case T_MINUS:
				{
					u32 Expr = BuildIRFromExpression(Builder, Node->Unary.Operand, false);
					u32 Zero;
					const type *T = GetType(Node->Unary.Type);
					if(HasBasicFlag(T, BasicFlag_Integer) || HasBasicFlag(T, BasicFlag_TypeID))
						Zero = PushInt(0, Builder, Node->Unary.Type);
					else
					{
						Assert(HasBasicFlag(T, BasicFlag_Float));
						const_value *Val = NewType(const_value);
						Val->Type = const_type::Float;
						Val->Float = 0;
						Zero = PushInstruction(Builder, Instruction(OP_CONST, (u64)Val, Node->Unary.Type, Builder));
					}
					instruction I = Instruction(OP_SUB, Zero, Expr, Node->Unary.Type, Builder);
					Result = PushInstruction(Builder, I);
				} break;
				case T_MMIN:
				case T_PPLUS:
				{
					u32 T = Node->Unary.Type;
					u32 Location = BuildIRFromExpression(Builder, Node->Unary.Operand, true);
					Result = PushInstruction(Builder,
							Instruction(OP_LOAD, 0, Location, T, Builder));

					if(Node->Unary.Op == T_PPLUS)
					{
						if(GetType(T)->Kind == TypeKind_Pointer)
						{
							u32 One = PushInt(1, Builder, Basic_int);
							Result = PushInstruction(Builder, Instruction(OP_INDEX, Result, One, T, Builder));
						}
						else
						{
							u32 One = PushInt(1, Builder, T);
							Result = PushInstruction(Builder, Instruction(OP_ADD, Result, One, T, Builder));
						}
					}
					else
					{
						Assert(Node->Unary.Op == T_MMIN);
						if(GetType(T)->Kind == TypeKind_Pointer)
						{
							u32 One = PushInt(-1, Builder, Basic_int);
							Result = PushInstruction(Builder, Instruction(OP_INDEX, Result, One, T, Builder));
						}
						else
						{
							u32 One = PushInt(1, Builder, T);
							Result = PushInstruction(Builder, Instruction(OP_SUB, Result, One, T, Builder));
						}
					}

					PushInstruction(Builder, InstructionStore(Location, Result, T));
				} break;
				case T_BITNOT:
				{
					u32 Expr = BuildIRFromExpression(Builder, Node->Unary.Operand, false);
					Result = PushInstruction(Builder, Instruction(OP_BITNOT, 0, Expr, Node->Unary.Type, Builder));
				} break;
				case T_BANG:
				{
					u32 Expr = BuildIRFromExpression(Builder, Node->Unary.Operand, false);
					u32 False = PushInt(0, Builder, Basic_bool);
					instruction I = Instruction(OP_EQEQ, Expr, False, Basic_bool, Builder);
					Result = PushInstruction(Builder, I);
				} break;
				case T_PTR:
				{
					Result = BuildIRFromExpression(Builder, Node->Unary.Operand, false);
					if(!IsLHS)
					{
						instruction I = Instruction(OP_LOAD, 0, Result, Node->Unary.Type, Builder);
						Result = PushInstruction(Builder, I);
					}
				} break;
				case T_ADDROF:
				{
					Assert(!IsLHS);
					Result = BuildIRFromExpression(Builder, Node->Unary.Operand, true);
				} break;
				case T_QMARK:
				{
					// @This is only for type checking, it doesn't actually do anything
					Result = BuildIRFromExpression(Builder, Node->Unary.Operand, IsLHS);
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

u32 BuildLogicalAnd(block_builder *Builder, node *Left, node *Right)
{
	basic_block TrueBlock = AllocateBlock(Builder);
	basic_block FalseBlock = AllocateBlock(Builder);
	basic_block EvaluateRight = AllocateBlock(Builder);
	basic_block After = AllocateBlock(Builder);
	u32 ResultAlloc = PushInstruction(Builder, 
			Instruction(OP_ALLOC, -1, Basic_bool, Builder));

	u32 LeftResult = BuildIRFromExpression(Builder, Left);
	PushInstruction(Builder, 
			Instruction(OP_IF, EvaluateRight.ID, FalseBlock.ID, LeftResult, Basic_bool));

	Terminate(Builder, EvaluateRight);
	PushStepLocation(Builder, Left);

	u32 RightResult = BuildIRFromExpression(Builder, Right);
	PushInstruction(Builder, 
			Instruction(OP_IF, TrueBlock.ID, FalseBlock.ID, RightResult, Basic_bool));

	Terminate(Builder, TrueBlock);
	PushStepLocation(Builder, Left);

	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(1, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, FalseBlock);
	PushStepLocation(Builder, Left);

	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(0, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, After);
	PushStepLocation(Builder, Left);

	return PushInstruction(Builder, 
			Instruction(OP_LOAD, 0, ResultAlloc, Basic_bool, Builder));
}

u32 BuildLogicalOr(block_builder *Builder, node *Left, node *Right)
{
	basic_block TrueBlock = AllocateBlock(Builder);
	basic_block FalseBlock = AllocateBlock(Builder);
	basic_block EvaluateRight = AllocateBlock(Builder);
	basic_block After = AllocateBlock(Builder);
	u32 ResultAlloc = PushInstruction(Builder, 
			Instruction(OP_ALLOC, -1, Basic_bool, Builder));

	u32 LeftResult = BuildIRFromExpression(Builder, Left);
	PushInstruction(Builder, 
			Instruction(OP_IF, TrueBlock.ID, EvaluateRight.ID, LeftResult, Basic_bool));

	Terminate(Builder, EvaluateRight);
	PushStepLocation(Builder, Left);

	u32 RightResult = BuildIRFromExpression(Builder, Right);
	PushInstruction(Builder, 
			Instruction(OP_IF, TrueBlock.ID, FalseBlock.ID, RightResult, Basic_bool));

	Terminate(Builder, TrueBlock);
	PushStepLocation(Builder, Left);

	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(1, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, FalseBlock);
	PushStepLocation(Builder, Left);

	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(0, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, After);
	PushStepLocation(Builder, Left);


	return PushInstruction(Builder, 
			Instruction(OP_LOAD, 0, ResultAlloc, Basic_bool, Builder));
}

u32 BuildStringCompare(block_builder *Builder, u32 Left, u32 Right, node *Node, b32 IsNeq)
{
	u32 LeftCount = PushInstruction(Builder, 
			Instruction(OP_INDEX, Left, 0, Basic_string, Builder));
	u32 LeftData = PushInstruction(Builder, 
			Instruction(OP_INDEX, Left, 1, Basic_string, Builder));
	u32 RightCount = PushInstruction(Builder, 
			Instruction(OP_INDEX, Right, 0, Basic_string, Builder));
	u32 RightData = PushInstruction(Builder, 
			Instruction(OP_INDEX, Right, 1, Basic_string, Builder));

	LeftCount = PushInstruction(Builder,
			Instruction(OP_LOAD, 0, LeftCount, Basic_int, Builder));
	RightCount = PushInstruction(Builder,
			Instruction(OP_LOAD, 0, RightCount, Basic_int, Builder));

	basic_block TrueBlock = AllocateBlock(Builder);
	basic_block FalseBlock = AllocateBlock(Builder);
	basic_block EvaluateRight = AllocateBlock(Builder);
	basic_block After = AllocateBlock(Builder);

	u32 ResultAlloc = PushInstruction(Builder, 
			Instruction(OP_ALLOC, -1, Basic_bool, Builder));

	u32 IsCount = PushInstruction(Builder, 
			Instruction(OP_EQEQ, LeftCount, RightCount, Basic_int, Builder));

	// if count == other_count {
	//		memcmp
	// } else {
	//		neq
	// }
	if(IsNeq)
	{
		PushInstruction(Builder, 
				Instruction(OP_IF, EvaluateRight.ID, TrueBlock.ID, IsCount, Basic_bool));
	}
	else
	{
		PushInstruction(Builder, 
				Instruction(OP_IF, EvaluateRight.ID, FalseBlock.ID, IsCount, Basic_bool));
	}

	Terminate(Builder, EvaluateRight);
	PushStepLocation(Builder, Node);

	u32 u8ptr = GetPointerTo(Basic_u8);
	LeftData = PushInstruction(Builder,
			Instruction(OP_LOAD, 0, LeftData, u8ptr, Builder));
	RightData = PushInstruction(Builder,
			Instruction(OP_LOAD, 0, RightData, u8ptr, Builder));
	ir_memcmp *MemCmpInfo = NewType(ir_memcmp);
	MemCmpInfo->LeftPtr = LeftData;
	MemCmpInfo->RightPtr= RightData;
	MemCmpInfo->Count	= RightCount;

	u32 MemCmpResult = PushInstruction(Builder, 
			Instruction(OP_MEMCMP, (void*)MemCmpInfo, Basic_bool, Builder, 0));

	if(IsNeq)
	{
		PushInstruction(Builder, 
				Instruction(OP_IF, FalseBlock.ID, TrueBlock.ID, MemCmpResult, Basic_bool));
	}
	else
	{
		PushInstruction(Builder, 
				Instruction(OP_IF, TrueBlock.ID, FalseBlock.ID, MemCmpResult, Basic_bool));
	}

	Terminate(Builder, TrueBlock);
	PushStepLocation(Builder, Node);

	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(1, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, FalseBlock);
	PushStepLocation(Builder, Node);

	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(0, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, After);
	PushStepLocation(Builder, Node);


	return PushInstruction(Builder, 
							Instruction(OP_LOAD, 0, ResultAlloc, Basic_bool, Builder));

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
		else if(Node->Binary.Op == T_LAND)
		{
			return BuildLogicalAnd(Builder, Node->Binary.Left, Node->Binary.Right);
		}
		else if(Node->Binary.Op == T_LOR)
		{
			return BuildLogicalOr(Builder, Node->Binary.Left, Node->Binary.Right);
		}

		u32 Left = BuildIRFromExpression(Builder, Node->Binary.Left, IsLeftLHS);
		u32 Right = BuildIRFromExpression(Builder, Node->Binary.Right, IsRightLHS);
		u32 Type = Node->Binary.ExpressionType;
		const type *T = GetType(Node->Binary.ExpressionType);
		if(T->Kind == TypeKind_Enum)
		{
			Type = T->Enum.Type;
			T = GetType(Type);
		}

		instruction I;
		switch((int)Node->Binary.Op)
		{
			case '+':
			{
				I = Instruction(OP_ADD, Left, Right, Type, Builder);
			} break;
			case '-':
			{
				I = Instruction(OP_SUB, Left, Right, Type, Builder);
			} break;
			case '*':
			{
				I = Instruction(OP_MUL, Left, Right, Type, Builder);
			} break;
			case '/':
			{
				I = Instruction(OP_DIV, Left, Right, Type, Builder);
			} break;
			case '%':
			{
				I = Instruction(OP_MOD, Left, Right, Type, Builder);
			} break;
			case '=':
			{
				I = InstructionStore(Left, Right, Type);
				if(NeedResult && !IsLHS)
				{
					PushInstruction(Builder, I);
					I = Instruction(OP_LOAD, 0, I.Result, I.Type, Builder);
				}
			} break;
			case '&':
			{
				I = Instruction(OP_AND, Left, Right, Type, Builder);
			} break;
			case '|':
			{
				I = Instruction(OP_OR, Left, Right,  Type, Builder);
			} break;
			case '^':
			{
				I = Instruction(OP_XOR, Left, Right, Type, Builder);
			} break;
			case T_SLEFT:
			{
				I = Instruction(OP_SL, Left, Right, Type, Builder);
			} break;
			case T_SRIGHT:
			{
				I = Instruction(OP_SR, Left, Right, Type, Builder);
			} break;
			case T_GREAT:
			{
				I = Instruction(OP_GREAT, Left, Right, Type, Builder);
			} break;
			case T_LESS:
			{
				I = Instruction(OP_LESS, Left, Right,  Type, Builder);
			} break;
			case T_GEQ:
			{
				I = Instruction(OP_GEQ, Left, Right, Type, Builder);
			} break;
			case T_LEQ:
			{
				I = Instruction(OP_LEQ, Left, Right, Type, Builder);
			} break;
			case T_EQEQ:
			{
				if(IsString(T))
				{
					return BuildStringCompare(Builder, Left, Right, Node);
				}
				else
				{
					I = Instruction(OP_EQEQ, Left, Right, Type, Builder);
				}
			} break;
			case T_NEQ:
			{
				if(IsString(T))
				{
					return BuildStringCompare(Builder, Left, Right, Node, true);
				}
				else
				{
					I = Instruction(OP_NEQ, Left, Right, Type, Builder);
				}
			} break;
			default:
			{
				LDEBUG("%d", (int)Node->Binary.Op);
				Assert(false);
			} break;
		};

		PushErrorInfo(Builder, Node);
		PushInstruction(Builder, I);
		return I.Result;
	}
	return BuildIRFromUnary(Builder, Node, IsLHS);
}

void IRPushDebugVariableInfo(block_builder *Builder, const error_info *ErrorInfo, string Name, u32 Type, u32 Location)
{
	ir_debug_info *IRInfo = NewType(ir_debug_info);
	IRInfo->type = IR_DBG_VAR;
	IRInfo->var.LineNo = ErrorInfo->Range.StartLine;
	IRInfo->var.Name = Name;
	IRInfo->var.Register = Location;
	IRInfo->var.TypeID = Type;

	PushInstruction(Builder,
			InstructionDebugInfo(IRInfo));
}

u32 BuildIRStoreVariable(block_builder *Builder, u32 Expression, u32 TypeIdx)
{
	u32 LocalRegister = PushInstruction(Builder,
			Instruction(OP_ALLOC, -1, TypeIdx, Builder));
	PushInstruction(Builder,
			InstructionStore(LocalRegister, Expression, TypeIdx));
	return LocalRegister;
}

void BuildIRFromDecleration(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_DECL);
	if(Node->Decl.LHS->Type == AST_ID)
	{
		u32 Var = -1;
		if(Node->Decl.Flags & SymbolFlag_LocalStatic)
		{
			function FakeFn = GenerateFakeFunctionForGlobalExpression(Node->Decl.Expression, Builder->Module, Builder->Imported, Node);
			ir *IR = Builder->Function->IR;

			// forward decl
			string *MakeAnonStructName(const error_info *e);

			string *JumbledName = MakeAnonStructName(Node->ErrorInfo);
			symbol *Sym = NewType(symbol);
			Sym->Register = g_LastAddedGlobal++;
			Sym->Node    = Node;
			Sym->Name    = JumbledName;
			Sym->LinkName= JumbledName;
			Sym->Type    = Node->Decl.TypeIndex;
			Sym->Flags   = Node->Decl.Flags;
			Sym->Checker = NULL; // hope this doesn't cause any issues :)

			IR->Globals.Push(ir_global{.s = Sym, .Init=FakeFn});
			instruction GlobalI = Instruction(OP_GLOBAL, (void *)Sym, Sym->Type, Builder, 0);
			Var = PushInstruction(Builder, GlobalI);
		}
		else if(Node->Decl.Expression)
		{
			u32 ExpressionRegister = BuildIRFromExpression(Builder, Node->Decl.Expression);
			Var = BuildIRStoreVariable(Builder, ExpressionRegister, Node->Decl.TypeIndex);
		}
		else
		{
			Var = PushInstruction(Builder,
					Instruction(OP_ALLOC, -1, Node->Decl.TypeIndex, Builder));
			PushInstruction(Builder,
					InstructionZeroOut(Var, Node->Decl.TypeIndex));
		}
		IRPushDebugVariableInfo(Builder, Node->ErrorInfo, *Node->Decl.LHS->ID.Name, Node->Decl.TypeIndex, Var);
		PushIRLocal(Builder, Node->Decl.LHS->ID.Name, Var, Node->Decl.TypeIndex);
	}
	else if(Node->Decl.LHS->Type == AST_LIST)
	{
		const type *T = GetType(Node->Decl.TypeIndex);

		Assert(Node->Decl.Expression);
		Assert(T->Kind == TypeKind_Struct);
		Assert(T->Struct.Flags & StructFlag_FnReturn);

		u32 ExpressionRegister = BuildIRFromExpression(Builder, Node->Decl.Expression);
		uint At = 0;
		For(Node->Decl.LHS->List.Nodes)
		{
			Assert((*it)->Type == AST_ID);
			u32 MemT = T->Struct.Members[At].Type;
			u32 MemPtr = PushInstruction(Builder, 
					Instruction(OP_INDEX, ExpressionRegister, At, Node->Decl.TypeIndex, Builder));
			u32 Mem = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, MemPtr, MemT, Builder));

			u32 Var = BuildIRStoreVariable(Builder, Mem, MemT);
			IRPushDebugVariableInfo(Builder, Node->ErrorInfo, *(*it)->ID.Name, MemT, Var);
			PushIRLocal(Builder, (*it)->ID.Name, Var, MemT);

			At++;
		}
	}
	else unreachable;
}

void BuildIRBody(dynamic<node *> &Body, block_builder *Block, basic_block Then);

void BuildIRForLoopWhile(block_builder *Builder, node *Node, b32 HasCondition)
{
	basic_block Cond  = AllocateBlock(Builder);
	basic_block Then  = AllocateBlock(Builder);
	basic_block End   = AllocateBlock(Builder);
	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, Cond);
	PushStepLocation(Builder, Node);

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
	PushStepLocation(Builder, Node);

	uint CurrentBreak = Builder->BreakBlockID;
	uint CurrentContinue = Builder->ContinueBlockID;
	Builder->BreakBlockID = End.ID;
	Builder->ContinueBlockID = Cond.ID;
	BuildIRBody(Node->For.Body, Builder, Cond);
	Builder->CurrentBlock = End;
	Builder->BreakBlockID = CurrentBreak;
	Builder->ContinueBlockID = CurrentContinue;
}

void BuildIRForIt(block_builder *Builder, node *Node)
{
	basic_block Cond  = AllocateBlock(Builder);
	basic_block Then  = AllocateBlock(Builder);
	basic_block Incr  = AllocateBlock(Builder);
	basic_block End   = AllocateBlock(Builder);

	u32 IAlloc, ItAlloc, Size, One, Array, StringPtr, StartString;
	u32 IType = Basic_int;

	const type *T = GetType(Node->For.ArrayType);
	u32 Zero = PushInt(0, Builder);

	// Init
	{

		if(T->Kind == TypeKind_Array)
		{
			Array = BuildIRFromExpression(Builder, Node->For.Expr2, true);
			Size = PushInt(T->Array.MemberCount, Builder);
		}
		else if(T->Kind == TypeKind_Slice)
		{
			Array = BuildIRFromExpression(Builder, Node->For.Expr2, true);
			Size = PushInstruction(Builder, 
					Instruction(OP_INDEX, Array, 0, Node->For.ArrayType, Builder));
			Size = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, Size, Basic_int, Builder));
		}
		else if(HasBasicFlag(T, BasicFlag_String))
		{
			u32 DataPtrT = GetPointerTo(Basic_u8);
			Array = BuildIRFromExpression(Builder, Node->For.Expr2, true);
			Size = PushInstruction(Builder, 
					Instruction(OP_INDEX, Array, 0, Node->For.ArrayType, Builder));
			Size = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, Size, Basic_int, Builder));
			StringPtr = PushInstruction(Builder, 
					Instruction(OP_ALLOC, -1, DataPtrT, Builder));

			StartString = PushInstruction(Builder, 
					Instruction(OP_INDEX, Array, 1, Node->For.ArrayType, Builder));
			StartString = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, StartString, DataPtrT, Builder));

			PushInstruction(Builder, 
					InstructionStore(StringPtr, StartString, DataPtrT));

		}
		else if(HasBasicFlag(T, BasicFlag_Integer))
		{
			Array = BuildIRFromExpression(Builder, Node->For.Expr2);
			Size = Array;
			IType = Node->For.ArrayType;
			Zero = PushInt(0, Builder, IType);
		}
		else
			Assert(false);

		One = PushInt(1, Builder);

		IAlloc = PushInstruction(Builder,
				Instruction(OP_ALLOC, -1, IType, Builder));
		PushInstruction(Builder,
				InstructionStore(IAlloc, Zero, IType));


		PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	}

	Terminate(Builder, Cond);
	PushStepLocation(Builder, Node);
	
	// Condition
	{
		if(HasBasicFlag(T, BasicFlag_String))
		{
			u32 u8P = GetPointerTo(Basic_u8);
			u32 At = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, StringPtr, u8P, Builder));
			u32 Passed = PushInstruction(Builder,
					Instruction(OP_PTRDIFF, At, StartString, u8P, Builder));
			u32 Condition = PushInstruction(Builder,
					Instruction(OP_LESS, Passed, Size, Basic_bool, Builder));
			PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, Condition, Basic_bool));
		}
		else
		{
			u32 I = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, IAlloc, IType, Builder));
			u32 Condition = PushInstruction(Builder,
					Instruction(OP_LESS, I, Size, Basic_bool, Builder));
			PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, Condition, Basic_bool));
		}
	}
	Terminate(Builder, Then);
	PushStepLocation(Builder, Node);

	uint CurrentBreak = Builder->BreakBlockID;
	uint CurrentContinue = Builder->ContinueBlockID;
	// Body
	{
		Builder->BreakBlockID = End.ID;
		Builder->ContinueBlockID = Incr.ID;

		// Set It
		{
			if(T->Kind == TypeKind_Array)
			{
				u32 I = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, IAlloc, Basic_int, Builder));

				u32 Elem = PushInstruction(Builder,
						Instruction(OP_INDEX, Array, I, Node->For.ArrayType, Builder));

				if(!Node->For.ItByRef)
				{
					Elem = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, Elem, Node->For.ItType, Builder));
				}

				ItAlloc = BuildIRStoreVariable(Builder, Elem, Node->For.ItType);
			}
			else if(T->Kind == TypeKind_Slice)
			{
				u32 I = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, IAlloc, Basic_int, Builder));

				u32 PointerTo = Node->For.ItType;
				if(!Node->For.ItByRef)
					PointerTo = GetPointerTo(PointerTo);
				u32 DataPtr = PushInstruction(Builder,
						Instruction(OP_INDEX, Array, 1, Node->For.ArrayType, Builder));

				u32 Data = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, DataPtr, PointerTo, Builder));

				Data = PushInstruction(Builder,
						Instruction(OP_INDEX, Data, I, PointerTo, Builder));

				if(!Node->For.ItByRef)
				{
					Data = PushInstruction(Builder, 
							Instruction(OP_LOAD, 0, Data, Node->For.ItType, Builder));
				}

				ItAlloc = BuildIRStoreVariable(Builder, Data, Node->For.ItType);
			}
			else if(HasBasicFlag(T, BasicFlag_String))
			{
				u32 Data = PushInstruction(Builder,
						Instruction(OP_LOAD, 0, StringPtr, GetPointerTo(Basic_u8), Builder));
				if(!Node->For.ItByRef)
				{
					u32 DerefFn = GetBuiltInFunction(Builder, STR_LIT("internal"), STR_LIT("deref"));;
					u32 FnT = Builder->CurrentBlock.Code.GetLast()->Type;

					call_info *Info = NewType(call_info);
					Info->ErrorInfo = Node->ErrorInfo;
					Info->Operand = DerefFn;
					Info->Args = SliceFromConst({Data});
					u32 Derefed = PushInstruction(Builder, 
							Instruction(OP_CALL, (void*)Info, FnT, Builder, 0));

					ItAlloc = BuildIRStoreVariable(Builder, Derefed, Node->For.ItType);
				}
				else
				{
					ItAlloc = BuildIRStoreVariable(Builder, Data, Node->For.ItType);
				}
			}
			else if(HasBasicFlag(T, BasicFlag_Integer))
			{
				ItAlloc = IAlloc;
			}
			else
			{
				unreachable;
			}

			if(Node->For.Expr1->Type == AST_ID)
			{
				IRPushDebugVariableInfo(Builder, Node->ErrorInfo,
						*Node->For.Expr1->ID.Name, Node->For.ItType, ItAlloc);
				PushIRLocal(Builder, Node->For.Expr1->ID.Name, ItAlloc, Node->For.ItType);
			}
			else if(Node->For.Expr1->Type == AST_UNARY)
			{
				auto it = Node->For.Expr1->Unary.Operand;
				Assert(it->Type == AST_ID);
				IRPushDebugVariableInfo(Builder, Node->ErrorInfo,
						*it->ID.Name, Node->For.ItType, ItAlloc);
				PushIRLocal(Builder, it->ID.Name, ItAlloc, Node->For.ItType);
			}
			else
			{
				Assert(Node->For.Expr1->Type == AST_LIST);
				auto List = Node->For.Expr1->List;

				// Index
				IRPushDebugVariableInfo(Builder, List.Nodes[0]->ErrorInfo, *List.Nodes[0]->ID.Name, Basic_int, IAlloc);
				PushIRLocal(Builder, List.Nodes[0]->ID.Name, IAlloc, Basic_int);

				// It
				auto it = Node->For.Expr1->List.Nodes[1];
				if(it->Type == AST_UNARY)
				{
					it = it->Unary.Operand;
				}
				Assert(it->Type == AST_ID);
				IRPushDebugVariableInfo(Builder, it->ErrorInfo, *it->ID.Name, Node->For.ItType, ItAlloc);
				PushIRLocal(Builder, it->ID.Name, ItAlloc, Node->For.ItType);

			}
		}

		BuildIRBody(Node->For.Body, Builder, Incr);
	}

	// Increment
	{
		PushStepLocation(Builder, Node);

		u32 I = PushInstruction(Builder, 
				Instruction(OP_LOAD, 0, IAlloc, Basic_int, Builder));
		
		u32 ToStore = PushInstruction(Builder, 
				Instruction(OP_ADD, I, One, Basic_int, Builder));

		PushInstruction(Builder,
				InstructionStore(IAlloc, ToStore, Basic_int));

		if(HasBasicFlag(T, BasicFlag_String))
		{
			u32 Data = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, StringPtr, GetPointerTo(Basic_u8), Builder));

			u32 AdvanceFn = GetBuiltInFunction(Builder, STR_LIT("internal"), STR_LIT("advance"));;
			u32 FnT = Builder->CurrentBlock.Code.GetLast()->Type;

			call_info *Info = NewType(call_info);
			Info->ErrorInfo = Node->ErrorInfo;
			Info->Operand = AdvanceFn;
			Info->Args = SliceFromConst({Data});
			Data = PushInstruction(Builder, 
					Instruction(OP_CALL, (void*)Info, FnT, Builder, 0));

			PushInstruction(Builder,
					InstructionStore(StringPtr, Data, GetPointerTo(Basic_u8)));
		}

		PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	}
	Terminate(Builder, End);
	PushStepLocation(Builder, Node);

	Builder->BreakBlockID = CurrentBreak;
	Builder->ContinueBlockID = CurrentContinue;
}

void BuildIRForLoopCStyle(block_builder *Builder, node *Node)
{
	basic_block Cond  = AllocateBlock(Builder);
	basic_block Then  = AllocateBlock(Builder);
	basic_block Incr  = AllocateBlock(Builder);
	basic_block End   = AllocateBlock(Builder);

	if(Node->For.Expr1)
		BuildIRFunctionLevel(Builder, Node->For.Expr1);
	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, Cond);
	PushStepLocation(Builder, Node);

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
	PushStepLocation(Builder, Node);

	uint CurrentBreak = Builder->BreakBlockID;
	uint CurrentContinue = Builder->ContinueBlockID;
	Builder->BreakBlockID = End.ID;
	Builder->ContinueBlockID = Incr.ID;
	BuildIRBody(Node->For.Body, Builder, Incr);

	if(Node->For.Expr3)
	{
		PushStepLocation(Builder, Node);
		BuildIRFromExpression(Builder, Node->For.Expr3);
	}

	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, End);
	PushStepLocation(Builder, Node);
	Builder->BreakBlockID = CurrentBreak;
	Builder->ContinueBlockID = CurrentContinue;
}

void PushDefferedInstructions(block_builder *Builder)
{
	ForArray(Idx, Builder->Defered.Data)
	{
		int ActualIdx = Builder->Defered.Data.Count - 1 - Idx;
		auto s = Builder->Defered.Data[ActualIdx];

		ForArray(SIdx, s.Expressions)
		{
			int ActualSIdx = s.Expressions.Count - 1 - SIdx;
			auto ExprBody = s.Expressions[ActualSIdx];
			For(ExprBody)
			{
				BuildIRFunctionLevel(Builder, (*it));
			}
		}
	}
}

void BuildIRFunctionLevel(block_builder *Builder, node *Node)
{
	if(Node->Type != AST_SCOPE)
	{
		PushErrorInfo(Builder, Node);
	}

	switch(Node->Type)
	{
		case AST_NOP: {};
		case AST_SCOPE:
		{
		    if(!Node->ScopeDelimiter.IsUp)
		    {
		  	  auto s = Builder->Defered.Pop();

		  	  if(!Builder->CurrentBlock.HasTerminator)
		  	  {
		  		  ForArray(Idx, s.Expressions)
		  		  {
		  			  int ActualIdx = s.Expressions.Count - 1 - Idx;
		  			  auto ExprBody = s.Expressions[ActualIdx];
		  			  For(ExprBody)
		  			  {
		  				  BuildIRFunctionLevel(Builder, (*it));
		  			  }
		  		  }
		  	  }

		  	  ForArray(Idx, s.Expressions)
		  	  {
		  		  s.Expressions[Idx].Free();
		  	  }
		  	  s.Expressions.Free();
		  	  Builder->Scope.Pop().Free();
		    }
		    else
		    {
		  	  Builder->Scope.Push({});
		  	  Builder->Defered.Push({});
		    }
		} break;
		case AST_USING:
		{
			const type *T = GetType(Node->TypedExpr.TypeIdx);
			Assert(T->Kind == TypeKind_Struct || T->Kind == TypeKind_Pointer || T->Kind == TypeKind_Enum);
			u32 Base = -1;
			u32 StructTy = Node->TypedExpr.TypeIdx;
			if(T->Kind == TypeKind_Pointer)
			{
				Base = BuildIRFromExpression(Builder, Node->TypedExpr.Expr, false);
				StructTy = T->Pointer.Pointed;
				T = GetType(T->Pointer.Pointed);
			}
			else
			{
				Base = BuildIRFromExpression(Builder, Node->TypedExpr.Expr, true);
			}

			if(T->Kind == TypeKind_Enum)
			{
				ForArray(Idx, T->Enum.Members)
				{
					auto it = T->Enum.Members[Idx];
					auto I = Instruction(OP_ALLOC, -1, Node->TypedExpr.TypeIdx, Builder);
					u32 Var = PushInstruction(Builder, I);
					u32 Value = PushInstruction(Builder, Instruction(OP_ENUM_ACCESS, 0, Idx, Node->TypedExpr.TypeIdx, Builder));
					PushInstruction(Builder, InstructionStore(Var, Value, Node->TypedExpr.TypeIdx));
					PushIRLocal(Builder, DupeType(it.Name, string), Var, Node->TypedExpr.TypeIdx);
				}
			}
			else
			{
				ForArray(Idx, T->Struct.Members)
				{
					auto it = T->Struct.Members[Idx];
					auto I = Instruction(OP_INDEX, Base, Idx, StructTy, Builder);
					u32 MemberPtr = PushInstruction(Builder, I);
					IRPushDebugVariableInfo(Builder, Node->ErrorInfo, it.ID, it.Type, MemberPtr);
					PushIRLocal(Builder, DupeType(it.ID, string), MemberPtr, it.Type);
					// @Cleanup: Useless DupeType? Maybe taking a pointer from T->Struct.Members is safe
				}
			}
		} break;
		case AST_ASSERT:
		{
			if((g_CompileFlags & CF_DisableAssert) == 0)
			{
				PushStepLocation(Builder, Node);

				u32 Cond = BuildIRFromExpression(Builder, Node->Assert.Expr);
				BuildAssertExpr(Builder, Cond, Node->ErrorInfo);
			}
		} break;
		case AST_DEFER:
		{
		    dynamic<node *> Defer = {};
		    ForArray(Idx, Node->Defer.Body)
		    {
		  	  Defer.Push(Node->Defer.Body[Idx]);
		    }
		    Builder->Defered.Peek().Expressions.Push(Defer);
		} break;
		case AST_DECL:
		{
			PushStepLocation(Builder, Node);

		    BuildIRFromDecleration(Builder, Node);
		} break;
		case AST_RETURN:
		{
			PushStepLocation(Builder, Node);

		    u32 Expression = -1;
		    u32 Type = Node->Return.TypeIdx;

		    if(Type != INVALID_TYPE)
		    {
		  	  const type *RT = GetType(Type);
		  	  if(Node->Return.Expression)
		  		  Expression = BuildIRFromExpression(Builder, Node->Return.Expression);
		  	  if(!IsRetTypePassInPointer(Type) && (RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array))
		  	  {
		  		  if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT) && IsUnix())
		  		  {
		  			  Type = AllFloatsStructToReturnType(RT);
		  			  Expression = PushInstruction(Builder,
		  					  Instruction(OP_LOAD, 0, Expression, Type, Builder));
		  		  }
		  		  else
		  		  {
		  			  Type = ComplexTypeToSizeType(RT);
		  			  Expression = PushInstruction(Builder,
		  					  Instruction(OP_LOAD, 0, Expression, Type, Builder));
		  		  }
		  	  }
		    }

			PushDefferedInstructions(Builder);

		    if(Builder->Profile)
		    {
		  	  u32 Callback = BuildIRFromExpression(Builder, Builder->Profile->Callback);
		  	  u32 EndTime = PushInstruction(Builder, 
		  			  Instruction(OP_RDTSC, 0, Basic_int, Builder));

		  	  u32 Taken = PushInstruction(Builder, 
		  			  Instruction(OP_SUB, EndTime, Builder->Profile->StartTime, Basic_int, Builder));
		  	  u32 FnName = MakeIRString(Builder, *Builder->Function->Name);

		  	  call_info *Call = NewType(call_info);
			  Call->ErrorInfo = Node->ErrorInfo;
		  	  Call->Operand = Callback;
		  	  Call->Args = SliceFromConst({FnName, Taken});

		  	  PushInstruction(Builder,
		  			  Instruction(OP_CALL, (void*)Call, Builder->Profile->CallbackType, Builder, 0));
		    }

		    PushInstruction(Builder, Instruction(OP_RET, Expression, 0, Type, Builder));
		    Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_YIELD:
		{
			PushStepLocation(Builder, Node);

			yield_info Info = Builder->YieldReturn.Peek();
			if(Node->TypedExpr.Expr)
			{
				Assert(Info.ValueStore != -1);
				u32 Expr = BuildIRFromExpression(Builder, Node->TypedExpr.Expr);
				PushInstruction(Builder, 
						InstructionStore(Info.ValueStore, Expr, Node->TypedExpr.TypeIdx));
			}
			PushInstruction(Builder, Instruction(OP_JMP, Info.ToBlockID, Basic_type, Builder));
			Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_IF:
		{
			PushStepLocation(Builder, Node);

		    Builder->Scope.Push({});
		    u32 IfExpression = BuildIRFromExpression(Builder, Node->If.Expression);
		    basic_block ThenBlock = AllocateBlock(Builder);
		    basic_block ElseBlock = AllocateBlock(Builder);
		    basic_block EndBlock  = AllocateBlock(Builder);
		    PushInstruction(Builder, Instruction(OP_IF, ThenBlock.ID, ElseBlock.ID, IfExpression, Basic_bool));
		    Terminate(Builder, ThenBlock);
			PushStepLocation(Builder, Node);
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
			  PushStepLocation(Builder, Node);
		    }
		    Builder->Scope.Pop();
		} break;
		case AST_BREAK:
		{
			PushStepLocation(Builder, Node);

			PushDefferedInstructions(Builder);
		    PushInstruction(Builder, Instruction(OP_JMP, Builder->BreakBlockID, Basic_type, Builder));
		    Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_CONTINUE:
		{
			PushStepLocation(Builder, Node);

			PushDefferedInstructions(Builder);
		    PushInstruction(Builder, Instruction(OP_JMP, Builder->ContinueBlockID, Basic_type, Builder));
		    Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_FOR:
		{
			PushStepLocation(Builder, Node);

		    Builder->Scope.Push({});
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
		    Builder->Scope.Pop();
		} break;
		default:
		{
			PushStepLocation(Builder, Node);

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
		if(Body.Count > 0)
			PushStepLocation(Builder, Body[Body.Count-1]);
		PushInstruction(Builder, Instruction(OP_JMP, Then.ID, Basic_type, Builder));
	}
	Terminate(Builder, Then);
	Builder->CurrentBlock = Then;
}

b32 CanGetPointerAfterSize(const type *T, int Size)
{
	ForArray(Idx, T->Struct.Members)
	{
		if(GetStructMemberOffset(T, Idx) == Size)
		{
			return true;
		}
	}
	return false;
}

void FixFunctionComplexParameter(u32 TIdx, const type *T, dynamic<u32>& Args, load_as *As)
{
	if(T->Kind != TypeKind_Struct)
	{
		*As = LoadAs_Normal;
		Args.Push(GetPointerTo(TIdx));
		return;
	}

	Assert(T->Kind == TypeKind_Struct);
	Assert(T->Struct.Members.Count > 0);
	int Size = GetTypeSize(T);
	if(Size > MAX_PARAMETER_SIZE)
	{
		*As = LoadAs_Normal;
		Args.Push(GetPointerTo(TIdx));
		return;
	}
	if(PTarget == platform_target::Wasm)
	{
		if(T->Struct.Members.Count > 1 || Size > 8 || !IsLoadableType(T->Struct.Members[0].Type))
		{
			*As = LoadAs_Normal;
			Args.Push(GetPointerTo(TIdx));
			return;
		}
		*As = LoadAs_Int;
		Args.Push(T->Struct.Members[0].Type);
		return;
	}

	// @TODO: Works for now, but it doesn't handle mixed types:
	// sturct example {
	//	a: i32,
	//	b: f32,
	//	c: f32,
	//	d: f32,
	// }
	// should be passes as call(int64, <2 x float>)
	b32 AllFloats = IsStructAllFloats(T);
	if(AllFloats && IsUnix())
	{
		type *Type = AllocType(TypeKind_Vector);
		Type->Vector.Kind = Vector_Float;
		Type->Vector.ElementCount = 2;
		u32 FloatVector = AddType(Type);

		int i = 0;
		for(; i+1 < T->Struct.Members.Count; ++i)
		{
			u32 First = T->Struct.Members[i].Type;
			u32 Second = T->Struct.Members[i + 1].Type;
			if(First == Basic_f32 && Second == Basic_f32)
			{
				Args.Push(FloatVector);
				++i;
			}
			else
			{
				Args.Push(First);
			}
		}

		if(i < T->Struct.Members.Count)
		{
			u32 Last = T->Struct.Members[i].Type;
			Args.Push(Last);
		}

		*As = LoadAs_Floats;
		return;
	}

	switch(Size)
	{
		case 16:
		{
			if(PTarget == platform_target::Windows || !CanGetPointerAfterSize(T, 8))
				goto PUSH_PTR;
			*As = LoadAs_MultiInt;
			Args.Push(Basic_u64);
			Args.Push(Basic_u64);
		} break;
		case 12:
		{
			if(PTarget == platform_target::Windows || !CanGetPointerAfterSize(T, 8))
				goto PUSH_PTR;
			*As = LoadAs_MultiInt;
			Args.Push(Basic_u64);
			Args.Push(Basic_u32);
		} break;
		case 10:
		{
			if(PTarget == platform_target::Windows || !CanGetPointerAfterSize(T, 8))
				goto PUSH_PTR;
			*As = LoadAs_MultiInt;
			Args.Push(Basic_u64);
			Args.Push(Basic_u16);
		} break;
		case 8:
		{
			*As = LoadAs_Int;
			Args.Push(Basic_u64);
		} break;
		case 4:
		{
			*As = LoadAs_Int;
			Args.Push(Basic_u32);
		} break;
		case 2:
		{
			*As = LoadAs_Int;
			Args.Push(Basic_u16);
		} break;
		case 1:
		{
			*As = LoadAs_Int;
			Args.Push(Basic_u8);
		} break;
		default:
		{
PUSH_PTR:
			*As = LoadAs_Normal;
			Args.Push(GetPointerTo(TIdx));
		} break;
	}
}

u32 FixFunctionTypeForCallConv(u32 TIdx, dynamic<arg_location> &Loc, b32 *RetInPtr)
{
	*RetInPtr = false;

	const type *T = GetType(TIdx);
	Assert(T->Kind == TypeKind_Function);
	if(T->Function.Returns.Count == 0 || T->Function.Returns.Count == 1)
	{
		if(T->Function.Returns.Count == 1 && IsLoadableType(T->Function.Returns[0]))
		{
			b32 NoNeedToFix = true;
			for(int i = 0; i < T->Function.ArgCount; ++i)
			{
				if(!IsLoadableType(T->Function.Args[i]))
				{
					NoNeedToFix = false;
					break;
				}
			}
			if(NoNeedToFix)
			{
				for(int i = 0; i < T->Function.ArgCount; ++i)
				{
					Loc.Push(arg_location{.Load = LoadAs_Normal, .Start = i, .Count = 1});
				}
				return TIdx;
			}
		}
	}
	dynamic<u32> FnArgs = {};
	u32 Return = INVALID_TYPE;
	if(T->Function.Returns.Count != 0)
	{
		u32 Returns = ReturnsToType(T->Function.Returns);
		const type *RT = GetType(Returns);
		if(IsRetTypePassInPointer(Returns))
		{
			*RetInPtr = true;
			FnArgs.Push(GetPointerTo(Returns));
		}
		else if(RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array)
		{
			if(PTarget == platform_target::Wasm)
			{
				if(RT->Kind == TypeKind_Struct)
				{
					if(RT->Struct.Members.Count == 1 && IsLoadableType(RT->Struct.Members[0].Type))
					{
						Return = RT->Struct.Members[0].Type;
					}
					else
					{
						*RetInPtr = true;
						FnArgs.Push(GetPointerTo(Returns));
					}
				}
				else
				{
					*RetInPtr = true;
					FnArgs.Push(GetPointerTo(Returns));
				}
			}
			else if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT) && IsUnix())
			{
				Return = AllFloatsStructToReturnType(RT);
			}
			else
			{
				Return = ComplexTypeToSizeType(RT);
			}
		}
		else if(RT->Kind == TypeKind_Function)
		{
			Return = GetPointerTo(INVALID_TYPE);
		}
		else
		{
			Return = Returns;
		}
	}

	for (int i = 0; i < T->Function.ArgCount; ++i)
	{
		const type *ArgType = GetType(T->Function.Args[i]);
		arg_location Location;
		Location.Start = FnArgs.Count;
		if(!IsLoadableType(ArgType))
		{
			FixFunctionComplexParameter(T->Function.Args[i], ArgType, FnArgs, &Location.Load);
		}
		else
		{
			Location.Load = LoadAs_Normal;
			FnArgs.Push(T->Function.Args[i]);
		}
		Location.Count = FnArgs.Count - Location.Start;
		Loc.Push(Location);
	}

	type *NewT = AllocType(TypeKind_Function);
	NewT->Function.ArgCount = FnArgs.Count;
	NewT->Function.Args = FnArgs.Data;
	if(Return == INVALID_TYPE)
		NewT->Function.Returns = {};
	else
		NewT->Function.Returns = SliceFromConst({Return});
	NewT->Function.Flags = T->Function.Flags;

	return AddType(NewT);
}

function BuildFunctionIR(ir *IR, dynamic<node *> &Body, const string *Name, u32 TypeIdx, slice<node *> &Args, node *Node,
		slice<import> Imported)
{
	module *Module = Node->Fn.FnModule;
	string NameNoPtr = *Name;

	b32 RetInPtr = false;
	const type *OgType = GetType(TypeIdx);
	dynamic<arg_location> Locations = {};
	TypeIdx = FixFunctionTypeForCallConv(TypeIdx, Locations, &RetInPtr);

	function Function = {};
	Function.IR = IR; // Needed for #static variables
	Function.Name = Name;
	Function.Type = TypeIdx;
	Function.Args = SliceFromArray(Locations);
	Function.LineNo = Node->ErrorInfo->Range.StartLine;
	Function.ModuleName = Module->Name;
	if(Node->Fn.LinkName)
		Function.LinkName = Node->Fn.LinkName;
	else if(Node->Fn.Flags & SymbolFlag_Foreign)
		Function.LinkName = Name;
	else
		Function.LinkName = StructToModuleNamePtr(NameNoPtr, Function.ModuleName);
	Function.ReturnPassedInPtr = RetInPtr;

	if(Body.IsValid())
	{
		block_builder Builder = MakeBlockBuilder(&Function, Module, Imported);
		Builder.Module = Module;

		PushErrorInfo(&Builder, Node);
		PushStepLocation(&Builder, Node);

		ForArray(Idx, Args)
		{
			u32 Type = INVALID_TYPE;
			if(Idx >= OgType->Function.ArgCount)
			{
				u32 ArgType = FindStruct(STR_LIT("base.Arg"));
				Type = GetSliceType(ArgType);
			}
			else
			{
				Type = OgType->Function.Args[Idx];
			}
			u32 Register = PushInstruction(&Builder,
					Instruction(OP_ARG, Idx, Type, &Builder));
			u32 Alloc = PushInstruction(&Builder, 
					Instruction(OP_ALLOC, -1, Type, &Builder));
			Register = PushInstruction(&Builder,
					InstructionStore(Alloc, Register, Type));

			Assert(Args[Idx]->Type == AST_VAR);
			PushIRLocal(&Builder, Args[Idx]->Var.Name, Register,
					Type);
			IRPushDebugVariableInfo(&Builder, Node->ErrorInfo, *Args[Idx]->Var.Name, Type, Alloc);
			//IRPushDebugArgInfo(&Builder, Node->ErrorInfo, Idx, Register, *Args[Idx]->Var.Name, Type);
		}

		u32 StartTime = -1;
		if(Node->Fn.ProfileCallback)
		{
			StartTime = PushInstruction(&Builder, 
					Instruction(OP_RDTSC, 0, Basic_int, &Builder));
			Builder.Profile = NewType(profiling);
			Builder.Profile->StartTime = StartTime;
			Builder.Profile->Callback = Node->Fn.ProfileCallback;
			Builder.Profile->CallbackType = Node->Fn.CallbackType;
		}

		ForArray(Idx, Body)
		{
			BuildIRFunctionLevel(&Builder, Body[Idx]);
		}

		Terminate(&Builder, {});
		Function.Runs = SliceFromArray(Builder.RunIndexes);
		Function.LastRegister = Builder.LastRegister;
		Builder.Function = NULL;
		Builder.Scope.Pop().Free();
	}
	return Function;
}

void WriteString(block_builder *Builder, u32 Ptr, string S)
{
	u32 Const = MakeIRString(Builder, S);

	PushInstruction(Builder, 
			InstructionStore(Ptr, Const, Basic_string));
}

void BuildTypeTable(block_builder *Builder, u32 TablePtr, u32 TableType, u32 TypeCount)
{
	u32 TypeInfoType = FindStruct(STR_LIT("base.TypeInfo"));
	u32 TypeKindType = FindEnum(STR_LIT("base.TypeKind"));
	//u32 TypeUnionType = FindStruct(STR_LIT("__init!TypeUnion"));
	u32 BasicTypeType = FindStruct(STR_LIT("base.BasicType"));
	u32 StructTypeType   = FindStruct(STR_LIT("base.StructType"));
	u32 FunctionTypeType = FindStruct(STR_LIT("base.FunctionType"));
	u32 PointerTypeType  = FindStruct(STR_LIT("base.PointerType"));
	u32 ArrayTypeType    = FindStruct(STR_LIT("base.ArrayType"));
	u32 SliceTypeType    = FindStruct(STR_LIT("base.SliceType"));
	u32 EnumTypeType     = FindStruct(STR_LIT("base.EnumType"));
	u32 VectorTypeType   = FindStruct(STR_LIT("base.VectorType"));
	u32 GenericTypeType  = FindStruct(STR_LIT("base.GenericType"));

	u32 BasicKindType = FindEnum(STR_LIT("base.BasicKind"));
	u32 EnumMemberType = FindStruct(STR_LIT("base.EnumMember"));
	u32 StructMemberType = FindStruct(STR_LIT("base.StructMember"));
	u32 VectorKindType = FindEnum(STR_LIT("base.VectorKind"));
	u32 SliceMemberType = GetSliceType(StructMemberType);
	u32 SliceEnumMemberType = GetSliceType(EnumMemberType);
	u32 PointerMemberType = GetPointerTo(StructMemberType);
	u32 PointerEnumMemberType = GetPointerTo(EnumMemberType);
	u32 TypeSlice   = GetSliceType(Basic_type);
	u32 TypePointer = GetPointerTo(Basic_type);
	//u32 StringPointer = GetPointerTo(Basic_string);
	//u32 StringSlice = GetSliceType(Basic_string);

	for(int i = 0; i < TypeCount; ++i)
	{
		const type *T = GetType(i);
		if(T->Kind == TypeKind_Generic)
			continue;

		u32 MemberPtr = PushInstruction(Builder, 
				Instruction(OP_INDEX, TablePtr, PushInt(i, Builder), TableType, Builder)
				);

		u32 KindPtr = PushInstruction(Builder, 
				Instruction(OP_INDEX, MemberPtr, 0, TypeInfoType, Builder)
				);
		PushInstruction(Builder, 
				InstructionStore(KindPtr, PushInt(T->Kind, Builder, TypeKindType), TypeKindType)
				);

		u32 tPtr = PushInstruction(Builder, 
				Instruction(OP_INDEX, MemberPtr, 1, TypeInfoType, Builder)
				);
		switch(T->Kind)
		{
			case TypeKind_Invalid:{} break;
			case TypeKind_Basic:
			{
				u32 kindPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, BasicTypeType, Builder)
						);
				u32 flagsPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, BasicTypeType, Builder)
						);
				u32 sizePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 2, BasicTypeType, Builder)
						);
				u32 namePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 3, BasicTypeType, Builder)
						);

				PushInstruction(Builder, 
						InstructionStore(kindPtr, PushInt(T->Basic.Kind, Builder, BasicKindType), BasicKindType)
						);
				PushInstruction(Builder, 
						InstructionStore(flagsPtr, PushInt(T->Basic.Flags, Builder, Basic_u32), Basic_u32)
						);
				PushInstruction(Builder, 
						InstructionStore(sizePtr, PushInt(T->Basic.Size, Builder, Basic_u32), Basic_u32)
						);

				WriteString(Builder, namePtr, T->Basic.Name);
			} break;
			case TypeKind_Struct:
			{
				u32 membersPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, StructTypeType, Builder)
						);
				u32 namePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, StructTypeType, Builder)
						);
				u32 flagsPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 2, StructTypeType, Builder)
						);

				
				u32 SliceSize = PushInstruction(Builder, 
						Instruction(OP_INDEX, membersPtr, 0, SliceMemberType, Builder)
						);
				u32 SliceData = PushInstruction(Builder, 
						Instruction(OP_INDEX, membersPtr, 1, SliceMemberType, Builder)
						);

				u32 Alloc = PushInstruction(Builder, 
						Instruction(OP_ALLOCGLOBAL, T->Struct.Members.Count, StructMemberType, Builder));
				ForArray(Idx, T->Struct.Members)
				{
					u32 MemberPtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, Alloc, PushInt(Idx, Builder), PointerMemberType, Builder)
							);

					u32 namePtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, MemberPtr, 0, StructMemberType, Builder)
							);
					u32 _tPtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, MemberPtr, 1, StructMemberType, Builder)
							);

					WriteString(Builder, namePtr, T->Struct.Members[Idx].ID);
					PushInstruction(Builder, 
							InstructionStore(_tPtr, PushInt(T->Struct.Members[Idx].Type, Builder, Basic_type), Basic_type)
							);
				}
				PushInstruction(Builder, 
						InstructionStore(SliceSize, PushInt(T->Struct.Members.Count, Builder, Basic_int), Basic_int)
						);
				PushInstruction(Builder, 
						InstructionStore(SliceData, Alloc, PointerMemberType)
						);

				WriteString(Builder, namePtr, T->Struct.Name);
				PushInstruction(Builder, 
						InstructionStore(flagsPtr, PushInt(T->Struct.Flags, Builder, Basic_u32), Basic_u32)
						);
			} break;
			case TypeKind_Function:
			{
				u32 return_Ptr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, FunctionTypeType, Builder)
						);
				u32 args_tPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, FunctionTypeType, Builder)
						);

				{
					u32 SliceSize = PushInstruction(Builder, 
							Instruction(OP_INDEX, return_Ptr, 0, TypeSlice, Builder)
							);
					u32 SliceData = PushInstruction(Builder, 
							Instruction(OP_INDEX, return_Ptr, 1, TypeSlice, Builder)
							);
					u32 Alloc = PushInstruction(Builder, 
							Instruction(OP_ALLOCGLOBAL, T->Function.Returns.Count, Basic_type, Builder));
					for(int Idx = 0; Idx < T->Function.Returns.Count; ++Idx)
					{
						u32 MemberPtr = PushInstruction(Builder, 
								Instruction(OP_INDEX, Alloc, PushInt(Idx, Builder), TypePointer, Builder)
								);

						PushInstruction(Builder, 
								InstructionStore(MemberPtr, PushInt(T->Function.Returns[Idx], Builder, Basic_type), Basic_type)
								);
					}
					PushInstruction(Builder, 
							InstructionStore(SliceSize, PushInt(T->Function.Returns.Count, Builder, Basic_int), Basic_int)
							);
					PushInstruction(Builder, 
							InstructionStore(SliceData, Alloc, TypePointer)
							);
				}

				{
					u32 SliceSize = PushInstruction(Builder, 
							Instruction(OP_INDEX, args_tPtr, 0, TypeSlice, Builder)
							);
					u32 SliceData = PushInstruction(Builder, 
							Instruction(OP_INDEX, args_tPtr, 1, TypeSlice, Builder)
							);
					u32 Alloc = PushInstruction(Builder, 
							Instruction(OP_ALLOCGLOBAL, T->Function.ArgCount, Basic_type, Builder));
					for(int Idx = 0; Idx < T->Function.ArgCount; ++Idx)
					{
						u32 MemberPtr = PushInstruction(Builder, 
								Instruction(OP_INDEX, Alloc, PushInt(Idx, Builder), TypePointer, Builder)
								);

						PushInstruction(Builder, 
								InstructionStore(MemberPtr, PushInt(T->Function.Args[Idx], Builder, Basic_type), Basic_type)
								);
					}
					PushInstruction(Builder, 
							InstructionStore(SliceSize, PushInt(T->Function.ArgCount, Builder, Basic_int), Basic_int)
							);
					PushInstruction(Builder, 
							InstructionStore(SliceData, Alloc, TypePointer)
							);
				}

			} break;
			case TypeKind_Pointer:
			{
				u32 pointeePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, PointerTypeType, Builder)
						);
				u32 is_optionalPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, PointerTypeType, Builder)
						);
				PushInstruction(Builder, 
						InstructionStore(pointeePtr, PushInt(T->Pointer.Pointed, Builder, Basic_type), Basic_type)
						);
				PushInstruction(Builder, 
						InstructionStore(is_optionalPtr,
							PushInt((T->Pointer.Flags & PointerFlag_Optional) != 0, Builder, Basic_bool),
							Basic_bool)
						);
			} break;
			case TypeKind_Array:
			{
				u32 _tPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, ArrayTypeType, Builder)
						);
				u32 member_countPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, ArrayTypeType, Builder)
						);

				PushInstruction(Builder, 
						InstructionStore(_tPtr, PushInt(T->Array.Type, Builder, Basic_type), Basic_type)
						);
				PushInstruction(Builder, 
						InstructionStore(member_countPtr, PushInt(T->Array.MemberCount, Builder, Basic_u32), Basic_u32)
						);
			} break;
			case TypeKind_Slice:
			{
				u32 _tPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, SliceTypeType, Builder)
						);
				PushInstruction(Builder, 
						InstructionStore(_tPtr, PushInt(T->Slice.Type, Builder, Basic_type), Basic_type)
						);

			} break;
			case TypeKind_Enum:
			{
				u32 namePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, EnumTypeType, Builder)
						);
				u32 membersPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, EnumTypeType, Builder)
						);
				u32 _tPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 2, EnumTypeType, Builder)
						);

				WriteString(Builder, namePtr, T->Enum.Name);
				PushInstruction(Builder, 
						InstructionStore(_tPtr, PushInt(T->Enum.Type, Builder, Basic_type), Basic_type)
						);

				u32 Alloc = PushInstruction(Builder, 
						Instruction(OP_ALLOCGLOBAL, T->Enum.Members.Count, EnumMemberType, Builder));
				ForArray(Idx, T->Enum.Members)
				{
					// struct EnumMember {
					//     name: string,
					//     value: int,
					// }

					u32 MemberPtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, Alloc, PushInt(Idx, Builder), PointerEnumMemberType, Builder)
							);

					u32 namePtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, MemberPtr, 0, EnumMemberType, Builder)
							);
					u32 valuePtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, MemberPtr, 1, EnumMemberType, Builder)
							);

					WriteString(Builder, namePtr, T->Enum.Members[Idx].Name);
					u32 MemValue = PushInstruction(Builder, Instruction(OP_ENUM_ACCESS, 0, Idx, i, Builder));
					PushInstruction(Builder, InstructionStore(valuePtr, MemValue, T->Enum.Type));
				}
				BuildSlice(Builder, Alloc, PushInt(T->Enum.Members.Count, Builder, Basic_int), SliceEnumMemberType, NULL, membersPtr);
			} break;
			case TypeKind_Vector:
			{
				u32 kindPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, VectorTypeType, Builder)
						);
				u32 elem_countPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 1, VectorTypeType, Builder)
						);
				PushInstruction(Builder, 
						InstructionStore(kindPtr, PushInt(T->Vector.Kind, Builder, VectorKindType), VectorKindType)
						);
				PushInstruction(Builder, 
						InstructionStore(elem_countPtr, PushInt(T->Vector.ElementCount, Builder, Basic_u32), Basic_u32)
						);
			} break;
			case TypeKind_Generic:
			{
				u32 namePtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, GenericTypeType, Builder)
						);
				WriteString(Builder, namePtr, T->Generic.Name);
			} break;
		}
	}
}

void GlobalLevelIR(ir *IR, node *Node, slice<import> Imported, module *Module)
{
	static u32 VoidFnT = GenerateVoidFnT();
	switch(Node->Type)
	{
		case AST_FN:
		{
			Assert(Node->Fn.Name);

			if((Node->Fn.Flags & SymbolFlag_Generic) == 0)
			{
				function Fn = BuildFunctionIR(IR, Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, Imported);
				if(Fn.LastRegister > IR->MaxRegisters)
					IR->MaxRegisters = Fn.LastRegister;

				IR->Functions.Push(Fn);
			}
		} break;
		case AST_RUN:
		{
			string NoFnName = STR_LIT("");
			function FakeFn = {};
			FakeFn.Name = DupeType(NoFnName, string);
			FakeFn.Type = VoidFnT;
			FakeFn.LinkName = FakeFn.Name;
			FakeFn.NoDebugInfo = true;
			FakeFn.FakeFunction = true;
			block_builder Builder = MakeBlockBuilder(&FakeFn, Module, Imported);
			BuildRun(&Builder, Node);
			Terminate(&Builder, {});
			FakeFn.LastRegister = Builder.LastRegister;
			IR->GlobalRuns.Push(FakeFn);
		} break;
		case AST_DECL:
		{
			function FakeFn = GenerateFakeFunctionForGlobalExpression(Node->Decl.Expression, Module, Imported, Node);

			Assert(Node->Decl.LHS->Type == AST_ID);
			const string *Name = Node->Decl.LHS->ID.Name;
			const symbol *Sym = Module->Globals[*Name];
			Assert(Sym);
			IR->Globals.Push(ir_global{.s = Sym, .Init=FakeFn});
		} break;
		case AST_ENUM:
		case AST_STRUCTDECL:
		{
			// do nothing
		} break;
		case AST_NOP:{} break;
		default:
		{
			Assert(false);
		} break;
	}
}

void BuildTypeTableFn(ir *IR, file *File, u32 VoidFnT)
{
	if(File->Module->Name != STR_LIT("base"))
		return;

	string Name = STR_LIT("base.__TypeTableInit");

	function TypeTableFn = {};
	TypeTableFn.Name = DupeType(Name, string);
	TypeTableFn.Type = VoidFnT;
	TypeTableFn.LinkName = TypeTableFn.Name;
	TypeTableFn.NoDebugInfo = true;
	TypeTableFn.FakeFunction = true;

	block_builder Builder = MakeBlockBuilder(&TypeTableFn, File->Module, {});

	string TypeTableName = STR_LIT("type_table");
	uint TypeCount = GetTypeCount();
	const symbol *Sym = GetIRLocal(&Builder, &TypeTableName);
	Assert(Sym);
	u32 TypeInfoType = FindStruct(STR_LIT("base.TypeInfo"));
	u32 ArrayType = GetArrayType(TypeInfoType, TypeCount);
	u32 Data = PushInstruction(&Builder, 
			Instruction(OP_ALLOCGLOBAL, TypeCount, TypeInfoType, &Builder));
	u32 Size = PushInt(TypeCount, &Builder);

	BuildTypeTable(&Builder, Data, ArrayType, TypeCount);

	instruction GlobalI = Instruction(OP_GLOBAL, (void *)Sym, Sym->Type, &Builder, 0);
	u32 LocalRegister = PushInstruction(&Builder, GlobalI);
	BuildSlice(&Builder, Data, Size, Sym->Type, NULL, LocalRegister);

	PushInstruction(&Builder, Instruction(OP_RET, -1, 0, INVALID_TYPE, &Builder));
	Terminate(&Builder, {});
	TypeTableFn.LastRegister = Builder.LastRegister;

	IR->Functions.Push(TypeTableFn);
	IR->Globals.Push(ir_global{.s = Sym, .Init = {}});
	Builder.Scope.Pop().Free();
}

ir BuildIR(file *File)
{
	ir IR = {};
	u32 NodeCount = File->Nodes.Count;
	static u32 VoidFnT = GenerateVoidFnT();

	BuildTypeTableFn(&IR, File, VoidFnT);

	for(int I = 0; I < NodeCount; ++I)
	{
		GlobalLevelIR(&IR, File->Nodes[I], File->Checker->Imported, File->Module);
	}
	for(int I = 0; I < NodeCount; ++I)
	{
		if(File->Nodes[I]->Type == AST_FN)
		{
			ForArray(Idx, IR.Functions)
			{
				if(*IR.Functions[Idx].Name == *File->Nodes[I]->Fn.Name)
				{
					File->Nodes[I]->Fn.IR = &IR.Functions.Data[Idx];
				}
			}
		}
	}

	return IR;
}

extern type **TypeTable;

void BuildEnumIR()
{
	u32 VoidFnT = GenerateVoidFnT();

	uint TC = GetTypeCount();
	for(int i = 0; i < TC; ++i)
	{ 
		type *T = TypeTable[i];
		if(T->Kind == TypeKind_Enum)
		{
			module *M = T->Enum.Module;
			slice<import> Imports = T->Enum.Imports;

			function Fn = {};
			Fn.Name = &T->Enum.Name;
			Fn.Type = VoidFnT;
			Fn.LinkName = Fn.Name;
			Fn.NoDebugInfo = true;

			block_builder Builder = MakeBlockBuilder(&Fn, M, Imports);
			For(T->Enum.Members)
			{
				Builder.CurrentBlock = AllocateBlock(&Builder);
				basic_block *StartBlock = &Builder.CurrentBlock;
				BuildIRFromExpression(&Builder, it->Expr);
				it->Evaluate = SliceFromArray(StartBlock->Code);
			}
			Builder.Scope.Pop().Free();
		}
	}
}

void DissasembleInstruction(string_builder *Builder, instruction Instr)
{
	const type *Type = NULL;
	if(Instr.Type != INVALID_TYPE)
		Type = GetType(Instr.Type);

	switch(Instr.Op)
	{
		default:
		{
			LERROR("UNKNOWN OP: %d", Instr.Op);
		} break;
		case OP_NOP:
		{
			PushBuilder(Builder, "NOP");
		} break;
		case OP_INSERT:
		{
			if(Instr.Ptr)
			{
				ir_insert *Ins = (ir_insert *)Instr.Ptr;
				PushBuilderFormated(Builder, "%%%d = insert %s %%%d in %%%d at %d",
						Instr.Result, GetTypeName(Instr.Type), Ins->ValueRegister, Ins->Register, Ins->Idx);
			}
			else
			{
				PushBuilderFormated(Builder, "%%%d = %s {}", Instr.Result, GetTypeName(Instr.Type));
			}
		} break;
		case OP_EXTRACT:
		{
			PushBuilderFormated(Builder, "%%%d = extract %%%d at %d", Instr.Result, Instr.Left, Instr.Right);
		} break;
		case OP_DEBUG_BREAK:
		{
			*Builder += "debug_break()";
		} break;
		case OP_CMPXCHG:
		{
			call_info *ci = (call_info *)Instr.BigRegister;
			PushBuilderFormated(Builder, "cmp_xchg(%%%d, %%%d, %%%d)", ci->Args[0], ci->Args[1], ci->Args[2]);
		} break;
		case OP_FENCE:
		{
			PushBuilderFormated(Builder, "fence");
		} break;
		case OP_ATOMIC_LOAD:
		{
			call_info *ci = (call_info *)Instr.BigRegister;
			PushBuilderFormated(Builder, "%%%d = atomic load %%%d", Instr.Result, ci->Args[0]);
		} break;
		case OP_ATOMIC_ADD:
		{
			call_info *ci = (call_info *)Instr.BigRegister;
			PushBuilderFormated(Builder, "%%%d = atomic add %%%d, %%%d", Instr.Result, ci->Args[0], ci->Args[1]);
		} break;
		case OP_GLOBAL:
		{
			const symbol *s = (const symbol *)Instr.Ptr;
			PushBuilderFormated(Builder, "%%%d = GLOBAL %s", Instr.Result, s->LinkName->Data);
		} break;
		case OP_RESULT:
		{
			PushBuilderFormated(Builder, "%%%d = EXPR RESULT", Instr.Result);
		} break;
		case OP_ENUM_ACCESS:
		{
			PushBuilderFormated(Builder, "%%%d = %s.%s", Instr.Result, GetTypeName(Type), Type->Enum.Members[Instr.Right].Name.Data);
		} break;
		case OP_TYPEINFO:
		{
			PushBuilderFormated(Builder, "%%%d = type_info %%%d", Instr.Result, Instr.Right);
		} break;
		case OP_RDTSC:
		{
			PushBuilderFormated(Builder, "%%%d = TIME", Instr.Result);
		} break;
		case OP_MEMCMP:
		{
			ir_memcmp *Info = (ir_memcmp *)Instr.Ptr;
			PushBuilderFormated(Builder, "%%%d = MEMCMP (PTR %%%d, PTR %%%d, COUNT %%%d)", Instr.Result, Info->LeftPtr, Info->RightPtr, Info->Count);
		} break;
		case OP_ZEROUT:
		{
			PushBuilderFormated(Builder, "ZEROUT %%%d %s", Instr.Right, GetTypeName(Type));
		} break;
		case OP_CONSTINT:
		{
			PushBuilderFormated(Builder, "%%%d = %s %llu", Instr.Result, GetTypeName(Type), Instr.BigRegister);
		} break;
		case OP_NULL:
		{
			PushBuilderFormated(Builder, "%%%d = null", Instr.Result);
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
							(int)Val->String.Data->Size, Val->String.Data->Data);
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
				case ct::Aggr:
				{
					PushBuilderFormated(Builder, "%%%d = %s {...}", Instr.Result, GetTypeName(Type));
				} break;
				case ct::Vector:
				{
					PushBuilderFormated(Builder, "%%%d = <>", Instr.Result);
				} break;
			}
		} break;
		case OP_FN:
		{
			function Fn = *(function *)Instr.Ptr;
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
			PushBuilderFormated(Builder, "%%%d = %s %%%d %% %%%d", Instr.Result, GetTypeName(Type),
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
		case OP_UNREACHABLE:
		{
			PushBuilderFormated(Builder, "UNREACHABLE");
		} break;
		case OP_PTRCAST:
		{
			PushBuilderFormated(Builder, "%%%d = ptr_cast %%%d to %s ", Instr.Result, Instr.Right, GetTypeName(Instr.Type));
		} break;
		case OP_MEMCPY:
		{
			PushBuilderFormated(Builder, "memcpy(%%%d, %%%d, %s)", Instr.Left, Instr.Right, GetTypeName(Instr.Type));
		} break;
		case OP_BITCAST:
		{
			PushBuilderFormated(Builder, "%%%d = bit_cast %%%d to %s ", Instr.Result, Instr.Right, GetTypeName(Instr.Type));
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
		case OP_BITNOT:
		{
			PushBuilderFormated(Builder, "%%%d = %s ~%%%d", Instr.Result, GetTypeName(Type), Instr.Right);
		} break;
		case OP_ALLOCGLOBAL:
		{
			PushBuilderFormated(Builder, "%%%d = GLOBALALLOC %llu %s", Instr.Result, Instr.BigRegister, GetTypeName(Type));
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
			call_info *CallInfo = (call_info *)Instr.Ptr;
			//PushBuilderFormated(Builder, "%%%d = CALL %s", Instr.Result, CallInfo->FnName->Data);
			PushBuilderFormated(Builder, "%%%d = CALL %%%d(", Instr.Result, CallInfo->Operand);
			ForArray(AIdx, CallInfo->Args)
			{
				if(AIdx != 0)
					*Builder += ", ";
				PushBuilderFormated(Builder, "%%%d", CallInfo->Args[AIdx]);
			}
			*Builder += ")";
		} break;
		case OP_SWITCHINT:
		{
			ir_switchint *Info = (ir_switchint *)Instr.Ptr;
			PushBuilderFormated(Builder, "%%%d = switch %%%d [", Instr.Result, Info->Matcher);
			ForArray(Idx, Info->Cases)
			{
				u32 Case = Info->Cases[Idx];
				u32 Val  = Info->OnValues[Idx];
				PushBuilderFormated(Builder, "%%%d block_%d", Val, Case);
				if(Idx + 1 != Info->Cases.Count)
				{
					*Builder += ", ";
				}
			}
			*Builder += ']';
		} break;
		case OP_RUN:
		{
			PushBuilderFormated(Builder, "%%%d = compile_time block_%d", Instr.Result, Instr.Right);
		} break;
		case OP_IF:
		{
			PushBuilderFormated(Builder, "IF %%%d goto block_%d, else goto block_%d", Instr.Result, Instr.Left, Instr.Right);
		} break;
		case OP_JMP:
		{
			PushBuilderFormated(Builder, "JMP block_%llu", Instr.BigRegister);
		} break;
		case OP_INDEX:
		{
			PushBuilderFormated(Builder, "%%%d = %%%d[%%%d] %s", Instr.Result, Instr.Left, Instr.Right,
					GetTypeName(Instr.Type));
		} break;
		case OP_ARRAYLIST:
		{
			PushBuilderFormated(Builder, "%%%d = ARRAYLIST (cba)", Instr.Result);
		} break;
		case OP_MEMSET:
		{
			PushBuilderFormated(Builder, "MEMSET %%%d", Instr.Right);
		} break;
		case OP_PTRDIFF:
		{
			PushBuilderFormated(Builder, "%%%d = PTR DIFF %%%d, %%%d", Instr.Result, Instr.Left, Instr.Right);
		} break;
		case OP_ARG:
		{
			PushBuilderFormated(Builder, "%%%d = ARG #%llu", Instr.Result, Instr.BigRegister);
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
			CASE_OP(OP_SL,    "<<")
			CASE_OP(OP_SR,    ">>")
			CASE_OP(OP_EQEQ,  "==")
			CASE_OP(OP_AND,   "&")
			CASE_OP(OP_OR,    "|")
			CASE_OP(OP_XOR,   "^")
			goto INSIDE_EQ;
		{
INSIDE_EQ:
			PushBuilderFormated(Builder, "%%%d = %%%d %s %%%d", Instr.Result, Instr.Left, op_str, Instr.Right);

		} break;
#undef CASE_OP
		case OP_DEBUGINFO:
		{
			ir_debug_info *Info = (ir_debug_info *)Instr.Ptr;
			switch(Info->type)
			{
				case IR_DBG_VAR:
				{
					PushBuilderFormated(Builder, "DEBUG_VAR_INFO %s Line=%d, Type=%s", Info->var.Name.Data, Info->var.LineNo, GetTypeName(Info->var.TypeID));
				} break;
				case IR_DBG_ERROR_INFO: break;
				case IR_DBG_STEP_LOCATION:
				{
					auto s = Info->step;
					PushBuilderFormated(Builder, "DEBUG_LOC_INFO Line=%d Col=%d", s.Line, s.Column);
				} break;
				case IR_DBG_SCOPE:
				{
					*Builder += "SCOPE";
				} break;
			}

		} break;
		case OP_SPILL:
		{
			PushBuilderFormated(Builder, "SPILL %%%d", Instr.Right);
		} break;
		case OP_COPYTOPHYSICAL:
		{
			const char *Name = phyregs[Instr.Result];
			PushBuilderFormated(Builder, "%s = %%%d", Name, Instr.Right);
		} break;
		case OP_COPYPHYSICAL:
		{
			const char *Name = phyregs[Instr.Left];
			PushBuilderFormated(Builder, "%%%d = %s", Instr.Result, Name);
		} break;
		case OP_COUNT: unreachable;
	}
}

void DissasembleBasicBlock(string_builder *Builder, basic_block *Block, int indent)
{
	ForArray(I, Block->Code)
	{
		instruction Instr = Block->Code[I];
		//if(Instr.Op == OP_DEBUGINFO)
		//	continue;

		PushBuilder(Builder, '\t');
		PushBuilder(Builder, '\t');
		for(int i = 0; i < indent; ++i)
			PushBuilder(Builder, '\t');

		DissasembleInstruction(Builder, Instr);

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
		PushBuilderFormated(&Builder, "%s #%zu", GetTypeName(ArgType), I + Fn.ModuleSymbols.Count);
		if(I + 1 != FnType->Function.ArgCount)
			PushBuilder(&Builder, ", ");
	}
	PushBuilder(&Builder, ')');
	if(FnType->Function.Returns.Count != 0)
	{
		Builder += " -> ";
		WriteFunctionReturnType(&Builder, FnType->Function.Returns);
	}

	if(Fn.Blocks.Count != 0)
	{
		PushBuilder(&Builder, " {\n");
		ForArray(I, Fn.Blocks)
		{
			basic_block Block = Fn.Blocks[I];
			Builder += '\n';
			for(int i = 0; i < indent; ++i)
				Builder += '\t';

			PushBuilderFormated(&Builder, "\tblock_%d:\n", Block.ID);
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

string Dissasemble(ir *IR)
{
	string_builder Builder = MakeBuilder();
	For(IR->Globals)
	{
		Builder += *it->s->Name;
		if(it->Init.LinkName == NULL)
		{
			Builder += ";\n";
			continue;
		}
		Builder += " = {\n";
		DissasembleBasicBlock(&Builder, it->Init.Blocks.Data, 0);
		Builder += "}\n";
	}
	For(IR->Functions)
	{
		Builder += DissasembleFunction(*it, 0);
		Builder += '\n';
	}
	For(IR->GlobalRuns)
	{
		Builder += "#run {";
		ForArray(Idx, it->Blocks)
		{
			if(Idx != 0)
				Builder += "\n";
			DissasembleBasicBlock(&Builder, it->Blocks.Data + Idx, 0);
		}
		Builder += "}\n";
	}
	return MakeString(Builder);
}


void GetUsedRegisters(instruction I, u32 *out, size_t *count)
{
#define OP_L(o) case o: out[(*count)++] = I.Left;  break
#define OP_R(o) case o: out[(*count)++] = I.Right; break
#define OP_LR(o) case o: out[(*count)++] = I.Left; out[(*count)++] = I.Right; break
#define OP_BR(o) case o: out[(*count)++] = I.BigRegister; break

	switch(I.Op)
	{
		OP_LR(OP_MEMCPY);
		OP_LR(OP_EXTRACT);
		case OP_INSERT:
		{
			if(I.Ptr)
			{
				ir_insert *ins = (ir_insert *)I.Ptr;
				out[(*count)++] = ins->Register;
				out[(*count)++] = ins->ValueRegister;
			}
			else
			{
			}
		} break;
		case OP_CMPXCHG:
		{
			call_info *ci = (call_info *)I.BigRegister;
			out[(*count)++] = ci->Args[0];
			out[(*count)++] = ci->Args[1];
			out[(*count)++] = ci->Args[2];
		} break;
		case OP_ATOMIC_LOAD:
		{
			call_info *ci = (call_info *)I.BigRegister;
			out[(*count)++] = ci->Args[0];
		} break;
		case OP_ATOMIC_ADD:
		{
			call_info *ci = (call_info *)I.BigRegister;
			out[(*count)++] = ci->Args[0];
			out[(*count)++] = ci->Args[1];
		} break;
		case OP_FENCE:
		case OP_SWITCHINT:
		{
			ir_switchint *Info = (ir_switchint*)I.Ptr;
			out[(*count)++] = Info->Matcher;
			For(Info->OnValues)
			{
				out[(*count)++] = *it;
			}
		} break;
		OP_R(OP_BITNOT);
		case OP_DEBUG_BREAK:
		case OP_GLOBAL:
		case OP_ARG:
		case OP_ALLOC:
		case OP_ALLOCGLOBAL:
		case OP_NULL:
		case OP_NOP:
		{
		} break;
		case OP_RESULT:
		case OP_RUN:
		case OP_ENUM_ACCESS:
		case OP_UNREACHABLE:
		case OP_TYPEINFO:
		case OP_RDTSC:
		Assert(false);

		case OP_ARRAYLIST:
		case OP_FN:
		case OP_CONSTINT:
		case OP_CONST:
		case OP_IF:
		case OP_JMP:
		case OP_DEBUGINFO:
		case OP_COPYPHYSICAL:
		break;

		OP_R(OP_ZEROUT);

		OP_LR(OP_ADD);
		OP_LR(OP_SUB);
		OP_LR(OP_MUL);
		OP_LR(OP_DIV);
		OP_LR(OP_MOD);

		OP_R(OP_LOAD);
		OP_R(OP_STORE);
		OP_R(OP_BITCAST);
		OP_R(OP_PTRCAST);
		OP_L(OP_CAST);
		case OP_RET:
		{
			if(I.Left != -1)
			{
				out[(*count)++] = I.Left;
			}
		} break;
		case OP_MEMCMP:
		{
			ir_memcmp *Info = (ir_memcmp *)I.Ptr;
			out[(*count)++] = Info->LeftPtr;
			out[(*count)++] = Info->RightPtr;
		} break;
		case OP_CALL:
		{
			// Needs special handling (maybe) (idk basic block stuff)
			call_info *Info = (call_info *)I.Ptr;
			out[(*count)++] = Info->Operand;
			For(Info->Args)
			{
				out[(*count)++] = *it;
			}
		} break;
		OP_LR(OP_INDEX);
		OP_R(OP_MEMSET);

		OP_LR(OP_NEQ);
		OP_LR(OP_GREAT);
		OP_LR(OP_GEQ);
		OP_LR(OP_LESS);
		OP_LR(OP_LEQ);
		OP_LR(OP_SL);
		OP_LR(OP_SR);
		OP_LR(OP_EQEQ);
		OP_LR(OP_AND);
		OP_LR(OP_OR);
		OP_LR(OP_XOR);
		OP_LR(OP_PTRDIFF);
		OP_L(OP_COPYTOPHYSICAL);

		case OP_SPILL:
		case OP_COUNT: unreachable;
	}
}

