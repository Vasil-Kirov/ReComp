#include "IR.h"
#include "ConstVal.h"
#include "Errors.h"
#include "Module.h"
#include "Semantics.h"
#include "Dynamic.h"
#include "Memory.h"
#include "Parser.h"
#include "Type.h"
#include "VString.h"
#include "vlib.h"
#include "Log.h"

void Terminate(block_builder *Builder, basic_block GoTo)
{
	Builder->CurrentBlock.HasTerminator = true;
	Builder->Function->Blocks.Push(Builder->CurrentBlock);
	Builder->CurrentBlock = GoTo;
}

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

inline u32 PushInt(u64 Value, block_builder *Builder, u32 Type = Basic_int)
{
	return PushInstruction(Builder, Instruction(OP_CONSTINT, Value, Type, Builder));
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

block_builder MakeBlockBuilder(function *Fn, u32 StartRegister, module *Module)
{
	block_builder Builder = {};
	Builder.Scope.Push({});
	Builder.Function = Fn;
	Builder.CurrentBlock = AllocateBlock(&Builder);
	Builder.LastRegister = StartRegister;
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

const symbol *GetIRLocal(block_builder *Builder, const string *NamePtr, b32 Error = true)
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
		return s;

	split Split = SplitAt(Name, '.');
	if(Split.first.Data != NULL)
	{
		For(Builder->Imported)
		{
			if(it->M->Name == Split.first)
			{
				s = it->M->Globals[Split.second];
				if(s)
					return s;
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

const symbol *GetBuiltInFunction(block_builder *Builder, string Module, string FnName)
{
	// @TODO: Global abuse
	For(CurrentModules)
	{
		if((*it)->Name == Module)
		{
			symbol *s = (*it)->Globals[FnName];
			if(s)
				return s;
			Assert(false);
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
	if(AllFloats && PTarget != platform_target::Windows)
	{
		type *Type = AllocType(TypeKind_Vector);
		Type->Vector.Kind = Vector_Float;
		Type->Vector.ElementCount = 2;
		u32 FloatVector = AddType(Type);
		u32 StructPtr = BuildIRFromExpression(Builder, Expr, IsLHS);

		for(int i = 0; i < ArgType->Struct.Members.Count / 2; ++i)
		{
			u32 MemPtr = PushInstruction(Builder,
					Instruction(OP_INDEX, StructPtr, i * 2, ArgTypeIdx, Builder));
			u32 Mem = PushInstruction(Builder, Instruction(OP_LOAD, 0, MemPtr, FloatVector, Builder));
			Args.Push(Mem);
		}

		if(ArgType->Struct.Members.Count % 2 != 0)
		{
			u32 MemPtr = PushInstruction(Builder,
					Instruction(OP_INDEX, StructPtr, ArgType->Struct.Members.Count - 1, ArgTypeIdx, Builder));
			u32 Mem = PushInstruction(Builder, Instruction(OP_LOAD, 0, MemPtr, Basic_f32, Builder));
			Args.Push(Mem);
		}

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
			static bool WarningGiven = false;
			if(IsForeign && Size > 8 && !WarningGiven)
			{
				WarningGiven = true;
				LWARN("Passing structs of size %d to functions is not properly supported, if it's used to interface with other languages, it will result in unspecified behavior", Size);
			}
			u32 Res = BuildIRFromExpression(Builder, Expr, IsLHS);
			if(IsForeign)
				Res = AllocateAndCopy(Builder, ArgTypeIdx, Res);
			Args.Push(Res);
			return;
		} break;
	}
	Args.Push(Pass);
}

u32 BuildIRIntMatch(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_MATCH);

	u32 Result = -1;
	if(Node->Match.ReturnType != INVALID_TYPE)
		Result = PushInstruction(Builder,
			Instruction(OP_ALLOC, -1, Node->Match.ReturnType, Builder));

	u32 StartBlock = Builder->CurrentBlock.ID;

	u32 Matcher = BuildIRFromExpression(Builder, Node->Match.Expression);
	basic_block After = AllocateBlock(Builder);
	dynamic<u32> CaseBlocks = {};
	dynamic<u32> OnValues = {};
	ForArray(Idx, Node->Match.Cases)
	{
		node *Case = Node->Match.Cases[Idx];
		u32 Value = BuildIRFromExpression(Builder, Case->Case.Value);
		OnValues.Push(Value);
	}

	u32 ReturnType = Node->Match.ReturnType;
	ForArray(Idx, Node->Match.Cases)
	{
		basic_block CaseBlock = AllocateBlock(Builder);
		Terminate(Builder, CaseBlock);
		node *Case = Node->Match.Cases[Idx];
		ForArray(BodyIdx, Case->Case.Body)
		{
			node *Node = Case->Case.Body[BodyIdx];
			if(Node->Type == AST_RETURN && Node->Return.Expression)
			{
				Assert(Result != -1);
				u32 Expr = BuildIRFromExpression(Builder, Node->Return.Expression);
				PushInstruction(Builder, 
						InstructionStore(Result, Expr, ReturnType));
			}
			else
			{
				BuildIRFunctionLevel(Builder, Node);
			}
		}

		PushInstruction(Builder,
				Instruction(OP_JMP, After.ID, Basic_type, Builder));

		CaseBlocks.Push(CaseBlock.ID);
	}
	Terminate(Builder, After);

	for(int i = 0; i < Builder->Function->Blocks.Count; ++i)
	{
		if(Builder->Function->Blocks[i].ID == StartBlock)
		{
			ir_switchint *Info = NewType(ir_switchint);
			Info->OnValues = SliceFromArray(OnValues);
			Info->Cases    = SliceFromArray(CaseBlocks);
			Info->After    = After.ID;
			Info->Matcher  = Matcher;
			instruction I = Instruction(OP_SWITCHINT, (u64)Info, Node->Match.ReturnType, Builder);
			Builder->Function->Blocks.Data[i].Code.Push(I);
			break;
		}
	}

	if(Result != -1)
	{
		if(IsLoadableType(Node->Match.ReturnType))
		{
			Result = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, Result, Node->Match.ReturnType, Builder));
		}
	}
	return Result;
}

u32 BuildIRMatch(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_MATCH);
	Assert(Node->Match.Cases.Count != 0);
	//u32 Matcher = BuildIRFromExpression(Builder, Node->Match.Expression);

	u32 Result = -1;

	const type *MT = GetType(Node->Match.MatchType);
	if(HasBasicFlag(MT, BasicFlag_Integer))
	{
		Result = BuildIRIntMatch(Builder, Node);
	}
	else if(MT->Kind == TypeKind_Enum)
	{
		Node->Match.MatchType = MT->Enum.Type;
		Result = BuildIRIntMatch(Builder, Node);
	}
	else
	{
		Assert(false);
	}


	return Result;
}

u32 StructPointerToStruct(block_builder *Builder, u32 Ptr, const type *T)
{
	// @NOTE: Pointers to structs (and slices) are a bit weird here
	const type *Pointed = GetType(T->Pointer.Pointed);
	u32 LoadType = T->Pointer.Pointed;
	if(Pointed->Kind == TypeKind_Struct || Pointed->Kind == TypeKind_Slice)
		LoadType = GetPointerTo(T->Pointer.Pointed);

	return PushInstruction(Builder, 
			Instruction(OP_LOAD, 0, Ptr, LoadType, Builder));
}

u32 BuildIRFromAtom(block_builder *Builder, node *Node, b32 IsLHS)
{
	u32 Result = -1;
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
			const symbol *Local = GetIRLocal(Builder, Node->ID.Name);
			Result = Local->Register;

			b32 ShouldLoad = true;
			if(IsLHS)
				ShouldLoad = false;

			const type *Type = GetType(Local->Type);
			if(Type->Kind != TypeKind_Basic && Type->Kind != TypeKind_Pointer && Type->Kind != TypeKind_Enum)
			{
				ShouldLoad = false;
			}
			else
			{
				if(Type->Kind == TypeKind_Basic && Type->Basic.Kind == Basic_string)
					ShouldLoad = false;
			}
			
			if(ShouldLoad && IsLoadableType(Type))
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Local->Register, Local->Type, Builder));
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
			static string InitModuleName = STR_LIT("init");
			string TableID = STR_LIT("init.type_table");
			if(Builder->Module->Name == InitModuleName)
				TableID = STR_LIT("type_table");
			u32 Idx = BuildIRFromExpression(Builder, Node->TypeInfoLookup.Expression);
			const symbol *s = GetIRLocal(Builder, &TableID);

			Result = PushInstruction(Builder, Instruction(OP_TYPEINFO, s->Register, Idx, Node->TypeInfoLookup.Type, Builder));
		} break;
		case AST_MATCH:
		{
			Result = BuildIRMatch(Builder, Node);
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
		case AST_EMBED:
		{
			const_value *EmbedValue = NewType(const_value);
			EmbedValue->Type = const_type::String;
			EmbedValue->String.Data = &Node->Embed.Content;
			u32 T = Node->Embed.IsString ? Basic_string : GetPointerTo(Basic_u8);
			Result = PushInstruction(Builder, 
					Instruction(OP_CONST, (u64)EmbedValue, T, Builder));
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
					Instruction(OP_CONST, (u64)Size, Basic_u64, Builder));
		} break;
		case AST_CALL:
		{
			//Assert(!IsLHS);
			call_info *CallInfo = NewType(call_info);

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

			CallInfo->Operand = BuildIRFromExpression(Builder, Node->Call.Fn, IsLHS);

			const type *Type = GetType(Node->Call.Type);
			if(Type->Kind == TypeKind_Pointer)
				Type = GetType(Type->Pointer.Pointed);
			Assert(Type->Kind == TypeKind_Function);
			dynamic<u32> Args = {};

			u32 ResultPtr = -1;
			u32 ReturnedWrongType = -1;
			if(Type->Function.Returns.Count != 0)
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
					if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT))
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
				VarArgT          = FindStruct(STR_LIT("init.Arg"));
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

			Result = PushInstruction(Builder, Instruction(OP_CALL, (u64)CallInfo, CallType, Builder));
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
			u32 Alloc = PushInstruction(Builder, Instruction(OP_ALLOC, -1, Node->TypeList.Type, Builder));
			Result = Alloc;
			if(Node->TypeList.Items.Count == 0)
			{
				break;
			}
			const type *Type = GetType(Node->TypeList.Type);
			switch(Type->Kind)
			{
				default: unreachable;
				case TypeKind_Array:
				{
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
		}break;
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
				u32 PtrToIdxed = GetPointerTo(Node->Index.IndexedType);
				u32 DataPtr = PushInstruction(Builder, Instruction(OP_INDEX, Operand, 1, Node->Index.OperandType, Builder));
				u32 LoadedPtr = PushInstruction(Builder, Instruction(OP_LOAD, 0, DataPtr, PtrToIdxed, Builder));
				Result = PushInstruction(Builder, Instruction(OP_INDEX, LoadedPtr, Index, PtrToIdxed, Builder));
				if(!IsLHS && !Node->Index.ForceNotLoad && !DontLoadResult)
					Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Node->Index.IndexedType, Builder));
			}
			else if(HasBasicFlag(Type, BasicFlag_String))
			{
				u32 u8ptr = GetPointerTo(Basic_u8);
				Result = PushInstruction(Builder, Instruction(OP_INDEX, Operand, 1, Basic_string, Builder));
				Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, u8ptr, Builder));
				Result = PushInstruction(Builder, Instruction(OP_INDEX, Result, Index, u8ptr, Builder));
				if(!IsLHS)
					Result = PushInstruction(Builder, Instruction(OP_LOAD, 0, Result, Node->Index.IndexedType, Builder));
			}
			else
			{
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
				const symbol *Sym = GetIRLocal(Builder, Mangled, false);
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
					Result = Sym->Register;
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
				if(Node->Selector.Operand)
					Operand = BuildIRFromExpression(Builder, Node->Selector.Operand, true);
				
				switch(Type->Kind)
				{
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
					case TypeKind_Struct:
					{
						if(Type->Kind == TypeKind_Pointer)
						{
							Operand = StructPointerToStruct(Builder, Operand, Type);

							TypeIdx = Type->Pointer.Pointed;
							Type = GetType(Type->Pointer.Pointed);
							if(Type->Kind == TypeKind_Slice)
								goto BUILD_SLICE_SELECTOR;
						}

						Result = PushInstruction(Builder, 
								Instruction(OP_INDEX, Operand, Node->Selector.Index, TypeIdx, Builder));

						u32 MemberTypeIdx = Type->Struct.Members[Node->Selector.Index].Type;
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
			Result = PushInt(Node->CharLiteral.C, Builder, Basic_u32);
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
			function fn = BuildFunctionIR(Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, Builder->Imported, Builder->LastRegister);

			Result = PushInstruction(Builder,
					Instruction(OP_FN, (u64)DupeType(fn, function), fn.Type, Builder));
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
					u32 Zero = PushInt(0, Builder, Node->Unary.Type);
					instruction I = Instruction(OP_SUB, Zero, Expr, Node->Unary.Type, Builder);
					Result = PushInstruction(Builder, I);
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
	u32 RightResult = BuildIRFromExpression(Builder, Right);
	PushInstruction(Builder, 
			Instruction(OP_IF, TrueBlock.ID, FalseBlock.ID, RightResult, Basic_bool));

	Terminate(Builder, TrueBlock);
	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(1, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, FalseBlock);
	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(0, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, After);

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
	u32 RightResult = BuildIRFromExpression(Builder, Right);
	PushInstruction(Builder, 
			Instruction(OP_IF, TrueBlock.ID, FalseBlock.ID, RightResult, Basic_bool));

	Terminate(Builder, TrueBlock);
	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(1, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, FalseBlock);
	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(0, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, After);

	return PushInstruction(Builder, 
			Instruction(OP_LOAD, 0, ResultAlloc, Basic_bool, Builder));
}

u32 BuildStringCompare(block_builder *Builder, u32 Left, u32 Right, b32 IsNeq=false)
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

	if(IsNeq)
	{
		// if count == other_count {
		//		memcmp
		// } else {
		//		neq
		// }
		PushInstruction(Builder, 
				Instruction(OP_IF, EvaluateRight.ID, TrueBlock.ID, IsCount, Basic_bool));
	}
	else
	{
		PushInstruction(Builder, 
				Instruction(OP_IF, EvaluateRight.ID, FalseBlock.ID, IsCount, Basic_bool));
	}

	Terminate(Builder, EvaluateRight);
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
			Instruction(OP_MEMCMP, (u64)MemCmpInfo, Basic_bool, Builder));

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
	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(1, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, FalseBlock);
	PushInstruction(Builder, 
			InstructionStore(ResultAlloc, PushInt(0, Builder, Basic_bool), Basic_bool));
	PushInstruction(Builder, 
			Instruction(OP_JMP, After.ID, Basic_type, Builder));

	Terminate(Builder, After);

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
					return BuildStringCompare(Builder, Left, Right);
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
					return BuildStringCompare(Builder, Left, Right, true);
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
		if(Node->Decl.Expression)
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

	u32 IAlloc, ItAlloc, Size, One, Array, StringPtr;
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

			u32 Data = PushInstruction(Builder, 
					Instruction(OP_INDEX, Array, 1, Node->For.ArrayType, Builder));
			Data = PushInstruction(Builder, 
					Instruction(OP_LOAD, 0, Data, DataPtrT, Builder));

			PushInstruction(Builder, 
					InstructionStore(StringPtr, Data, DataPtrT));

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
	
	// Condition
	{
		u32 I = PushInstruction(Builder, 
				Instruction(OP_LOAD, 0, IAlloc, IType, Builder));
		u32 Condition = PushInstruction(Builder,
				Instruction(OP_LESS, I, Size, Basic_bool, Builder));
		PushInstruction(Builder, Instruction(OP_IF, Then.ID, End.ID, Condition, Basic_bool));
	}
	Terminate(Builder, Then);

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

				u32 ElemPtr = PushInstruction(Builder,
						Instruction(OP_INDEX, Array, I, Node->For.ArrayType, Builder));

				u32 Elem = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, ElemPtr, Node->For.ItType, Builder));

				ItAlloc = BuildIRStoreVariable(Builder, Elem, Node->For.ItType);
			}
			else if(T->Kind == TypeKind_Slice)
			{
				u32 I = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, IAlloc, Basic_int, Builder));

				u32 PointerTo = GetPointerTo(Node->For.ItType);
				u32 DataPtr = PushInstruction(Builder,
						Instruction(OP_INDEX, Array, 1, Node->For.ArrayType, Builder));

				u32 Data = PushInstruction(Builder, 
						Instruction(OP_LOAD, 0, DataPtr, PointerTo, Builder));

				Data = PushInstruction(Builder,
						Instruction(OP_INDEX, Data, I, PointerTo, Builder));

				if(IsLoadableType(Node->For.ItType))
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
				const symbol *DerefFn = GetBuiltInFunction(Builder, STR_LIT("str"), STR_LIT("deref"));;

				call_info *Info = NewType(call_info);
				Info->Operand = DerefFn->Register;
				Info->Args = SliceFromConst({Data});
				u32 Derefed = PushInstruction(Builder, 
						Instruction(OP_CALL, (u64)Info, DerefFn->Type, Builder));

				ItAlloc = BuildIRStoreVariable(Builder, Derefed, Node->For.ItType);
			}
			else if(HasBasicFlag(T, BasicFlag_Integer))
			{
				ItAlloc = IAlloc;
			}
			else
			{
				unreachable;
			}

			IRPushDebugVariableInfo(Builder, Node->ErrorInfo,
					*Node->For.Expr1->ID.Name, Node->For.ItType, ItAlloc);
			PushIRLocal(Builder, Node->For.Expr1->ID.Name, ItAlloc, Node->For.ItType);

			if(T->Kind == TypeKind_Array || T->Kind == TypeKind_Slice || HasBasicFlag(T, BasicFlag_String))
			{
				string *n = NewType(string);
				*n = STR_LIT("i");
				IRPushDebugVariableInfo(Builder, Node->ErrorInfo,
						*n, Basic_int, IAlloc);
				PushIRLocal(Builder, n, IAlloc, Basic_int);
			}
		}

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

		if(HasBasicFlag(T, BasicFlag_String))
		{
			u32 Data = PushInstruction(Builder,
					Instruction(OP_LOAD, 0, StringPtr, GetPointerTo(Basic_u8), Builder));

			const symbol *AdvanceFn = GetBuiltInFunction(Builder, STR_LIT("str"), STR_LIT("advance"));;

			call_info *Info = NewType(call_info);
			Info->Operand = AdvanceFn->Register;
			Info->Args = SliceFromConst({Data});
			Data = PushInstruction(Builder, 
					Instruction(OP_CALL, (u64)Info, AdvanceFn->Type, Builder));

			PushInstruction(Builder,
					InstructionStore(StringPtr, Data, GetPointerTo(Basic_u8)));
		}

		PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	}
	Terminate(Builder, End);
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

	uint CurrentBreak = Builder->BreakBlockID;
	uint CurrentContinue = Builder->ContinueBlockID;
	Builder->BreakBlockID = End.ID;
	Builder->ContinueBlockID = Incr.ID;
	BuildIRBody(Node->For.Body, Builder, Incr);

	if(Node->For.Expr3)
		BuildIRFromExpression(Builder, Node->For.Expr3);

	PushInstruction(Builder, Instruction(OP_JMP, Cond.ID, Basic_type, Builder));
	Terminate(Builder, End);
	Builder->BreakBlockID = CurrentBreak;
	Builder->ContinueBlockID = CurrentContinue;
}

void BuildAssertFailed(block_builder *Builder, const error_info *ErrorInfo)
{
	string AssertionLine = GetErrorSegment(*ErrorInfo);
	string_builder b = MakeBuilder();
	PushBuilderFormated(&b, "--- ASSERTION FAILED ---\n\n%s(%d):\n%s", ErrorInfo->FileName, ErrorInfo->Line, AssertionLine.Data);

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

	string OS = STR_LIT("os");
	const symbol *stdout = GetBuiltInFunction(Builder, OS, STR_LIT("stdout"));
	call_info *Call = NewType(call_info);
	Call = NewType(call_info);
	Call->Operand = stdout->Register;
	Call->Args = {};
	u32 Handle = PushInstruction(Builder, Instruction(OP_CALL, (u64)Call, stdout->Type, Builder));

	const symbol *s = GetBuiltInFunction(Builder, OS, STR_LIT("write"));

	Call = NewType(call_info);
	Call->Operand = s->Register;
	Call->Args = SliceFromConst({Handle, Data, Count});
	PushInstruction(Builder, Instruction(OP_CALL, (u64)Call, s->Type, Builder));

	const symbol *abort = GetBuiltInFunction(Builder, OS, STR_LIT("abort"));
	Call = NewType(call_info);
	Call->Operand = abort->Register;
	Call->Args = {};
	PushInstruction(Builder, Instruction(OP_CALL, (u64)Call, abort->Type, Builder));

	PushInstruction(Builder, Instruction(OP_UNREACHABLE, 0, Basic_type, Builder));
}

void BuildIRFunctionLevel(block_builder *Builder, node *Node)
{
	if(Node->Type != AST_SCOPE)
		IRPushDebugLocation(Builder, Node->ErrorInfo);

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
			const type *T = GetType(Node->Using.Type);
			Assert(T->Kind == TypeKind_Struct || T->Kind == TypeKind_Pointer);
			u32 Base = BuildIRFromExpression(Builder, Node->Using.Expr, true);
			u32 StructTy = Node->Using.Type;
			if(T->Kind == TypeKind_Pointer)
			{
				Base = StructPointerToStruct(Builder, Base, T);
				StructTy = T->Pointer.Pointed;
				T = GetType(T->Pointer.Pointed);
			}
			ForArray(Idx, T->Struct.Members)
			{
				auto it = T->Struct.Members[Idx];
				auto I = Instruction(OP_INDEX, Base, Idx, StructTy, Builder);
				u32 MemberPtr = PushInstruction(Builder, I);
				IRPushDebugVariableInfo(Builder, Node->ErrorInfo, it.ID, it.Type, MemberPtr);
				PushIRLocal(Builder, DupeType(it.ID, string), MemberPtr, it.Type);
				// @Cleanup: Useless DupeType? Maybe taking a pointer from T->Struct.Members is safe
			}
		} break;
		case AST_ASSERT:
		{
		    u32 Cond = BuildIRFromExpression(Builder, Node->Assert.Expr);
		    basic_block AssertFailed = AllocateBlock(Builder);
		    basic_block After = AllocateBlock(Builder);
		    PushInstruction(Builder, Instruction(OP_IF, After.ID, AssertFailed.ID, Cond, Basic_bool));
		    Terminate(Builder, AssertFailed);
		    BuildAssertFailed(Builder, Node->ErrorInfo);
		    Terminate(Builder, After);
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
		    BuildIRFromDecleration(Builder, Node);
		} break;
		case AST_RETURN:
		{
		    u32 Expression = -1;
		    u32 Type = Node->Return.TypeIdx;

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

		    if(Type != INVALID_TYPE)
		    {
		  	  const type *RT = GetType(Type);
		  	  if(Node->Return.Expression)
		  		  Expression = BuildIRFromExpression(Builder, Node->Return.Expression);
		  	  if(!IsRetTypePassInPointer(Type) && (RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array))
		  	  {
		  		  if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT))
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

		    if(Builder->Profile)
		    {
		  	  u32 Callback = BuildIRFromExpression(Builder, Builder->Profile->Callback);
		  	  u32 EndTime = PushInstruction(Builder, 
		  			  Instruction(OP_RDTSC, 0, Basic_int, Builder));

		  	  u32 Taken = PushInstruction(Builder, 
		  			  Instruction(OP_SUB, EndTime, Builder->Profile->StartTime, Basic_int, Builder));
		  	  u32 FnName = MakeIRString(Builder, *Builder->Function->Name);

		  	  call_info *Call = NewType(call_info);
		  	  Call->Operand = Callback;
		  	  Call->Args = SliceFromConst({FnName, Taken});

		  	  PushInstruction(Builder,
		  			  Instruction(OP_CALL, (u64)Call, Builder->Profile->CallbackType, Builder));
		    }

		    PushInstruction(Builder, Instruction(OP_RET, Expression, 0, Type, Builder));
		    Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_IF:
		{
		    Builder->Scope.Push({});
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
		    Builder->Scope.Pop();
		} break;
		case AST_BREAK:
		{
		    PushInstruction(Builder, Instruction(OP_JMP, Builder->BreakBlockID, Basic_type, Builder));
		    Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_CONTINUE:
		{
		    PushInstruction(Builder, Instruction(OP_JMP, Builder->ContinueBlockID, Basic_type, Builder));
		    Builder->CurrentBlock.HasTerminator = true;
		} break;
		case AST_FOR:
		{
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
		slice<import> Imported, u32 IRStartRegister)
{
	module *Module = Node->Fn.FnModule;
	string NameNoPtr = *Name;

	function Function = {};
	Function.Name = Name;
	Function.Type = TypeIdx;
	Function.LineNo = Node->ErrorInfo->Line;
	Function.ModuleName = Module->Name;
	if(Node->Fn.LinkName)
		Function.LinkName = Node->Fn.LinkName;
	else if(Node->Fn.Flags & SymbolFlag_Foreign)
		Function.LinkName = Name;
	else
		Function.LinkName = StructToModuleNamePtr(NameNoPtr, Function.ModuleName);
	if(Body.IsValid())
	{
		block_builder Builder = MakeBlockBuilder(&Function, IRStartRegister, Module);
		Builder.Imported = Imported;
		Builder.Module = Module;

		const type *FnType = GetType(TypeIdx);
		IRPushDebugLocation(&Builder, Node->ErrorInfo);

		ForArray(Idx, Args)
		{
			u32 Type = INVALID_TYPE;
			if(Idx >= FnType->Function.ArgCount)
			{
				u32 ArgType = FindStruct(STR_LIT("init.Arg"));
				Type = GetSliceType(ArgType);
			}
			else
			{
				Type = FnType->Function.Args[Idx];
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
			IRPushDebugArgInfo(&Builder, Node->ErrorInfo, Idx, Register, *Args[Idx]->Var.Name, Type);
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
	u32 TypeInfoType = FindStruct(STR_LIT("init.TypeInfo"));
	u32 TypeKindType = FindEnum(STR_LIT("init.TypeKind"));
	//u32 TypeUnionType = FindStruct(STR_LIT("__init!TypeUnion"));
	u32 BasicTypeType = FindStruct(STR_LIT("init.BasicType"));
	u32 StructTypeType   = FindStruct(STR_LIT("init.StructType"));
	u32 FunctionTypeType = FindStruct(STR_LIT("init.FunctionType"));
	u32 PointerTypeType  = FindStruct(STR_LIT("init.PointerType"));
	u32 ArrayTypeType    = FindStruct(STR_LIT("init.ArrayType"));
	u32 SliceTypeType    = FindStruct(STR_LIT("init.SliceType"));
	u32 EnumTypeType     = FindStruct(STR_LIT("init.EnumType"));
	u32 VectorTypeType   = FindStruct(STR_LIT("init.VectorType"));
	u32 GenericTypeType  = FindStruct(STR_LIT("init.GenericType"));

	u32 BasicKindType = FindEnum(STR_LIT("init.BasicKind"));
	u32 StructMemberType = FindStruct(STR_LIT("init.StructMember"));
	u32 VectorKindType = FindEnum(STR_LIT("init.VectorKind"));
	u32 SliceMemberType = GetSliceType(StructMemberType);
	u32 PointerMemberType = GetPointerTo(StructMemberType);
	u32 TypeSlice   = GetSliceType(Basic_type);
	u32 TypePointer = GetPointerTo(Basic_type);
	u32 StringPointer = GetPointerTo(Basic_string);
	u32 StringSlice = GetSliceType(Basic_string);

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
						Instruction(OP_ALLOCGLOBAL, T->Enum.Members.Count, Basic_string, Builder));
				ForArray(Idx, T->Enum.Members)
				{
					u32 MemberPtr = PushInstruction(Builder, 
							Instruction(OP_INDEX, Alloc, PushInt(Idx, Builder), StringPointer, Builder)
							);

					WriteString(Builder, MemberPtr, T->Enum.Members[Idx].Name);
				}
				BuildSlice(Builder, Alloc, PushInt(T->Enum.Members.Count, Builder, Basic_int), StringSlice, NULL, membersPtr);
			} break;
			case TypeKind_Vector:
			{
				u32 kindPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, VectorTypeType, Builder)
						);
				u32 elem_countPtr = PushInstruction(Builder, 
						Instruction(OP_INDEX, tPtr, 0, VectorTypeType, Builder)
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

function GlobalLevelIR(node *Node, slice<import> Imported, module *Module, u32 IRStartRegister)
{
	function Result = {};
	switch(Node->Type)
	{
		case AST_FN:
		{
			Assert(Node->Fn.Name);

			if((Node->Fn.Flags & SymbolFlag_Generic) == 0)
			{
				Result = BuildFunctionIR(Node->Fn.Body, Node->Fn.Name, Node->Fn.TypeIdx, Node->Fn.Args, Node, Imported, IRStartRegister);
			}
		} break;
		case AST_DECL:
		case AST_ENUM:
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

void BuildTypeTableFn(ir *IR, file *File, u32 VoidFnT, u32 StartRegister)
{
	if(File->Module->Name != STR_LIT("init"))
		return;

	string Name = STR_LIT("init.__TypeTableInit");

	function TypeTableFn = {};
	TypeTableFn.Name = DupeType(Name, string);
	TypeTableFn.Type = VoidFnT;
	TypeTableFn.LinkName = TypeTableFn.Name;
	TypeTableFn.NoDebugInfo = true;

	block_builder Builder = MakeBlockBuilder(&TypeTableFn, StartRegister, File->Module);

	string TypeTableName = STR_LIT("type_table");
	uint TypeCount = GetTypeCount();
	const symbol *Sym = GetIRLocal(&Builder, &TypeTableName);
	Assert(Sym);
	u32 TypeInfoType = FindStruct(STR_LIT("init.TypeInfo"));
	u32 ArrayType = GetArrayType(TypeInfoType, TypeCount);
	u32 Data = PushInstruction(&Builder, 
			Instruction(OP_ALLOCGLOBAL, TypeCount, TypeInfoType, &Builder));
	u32 Size = PushInt(TypeCount, &Builder);

	BuildTypeTable(&Builder, Data, ArrayType, TypeCount);
	BuildSlice(&Builder, Data, Size, Sym->Type, NULL, Sym->Register);

	PushInstruction(&Builder, Instruction(OP_RET, -1, 0, INVALID_TYPE, &Builder));
	Terminate(&Builder, {});
	TypeTableFn.LastRegister = Builder.LastRegister;

	IR->Functions.Push(TypeTableFn);
	Builder.Scope.Pop().Free();
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

ir BuildIR(file *File, u32 StartRegister)
{
	ir IR = {};
	u32 NodeCount = File->Nodes.Count;
	int FileIndex = GetFileIndex(File->Module, File);
	static u32 VoidFnT = GenerateVoidFnT();

	BuildTypeTableFn(&IR, File, VoidFnT, StartRegister);

	{
		string_builder StrBuilder = MakeBuilder();
		PushBuilderFormated(&StrBuilder, "__GlobalInitializerFunction.%d", FileIndex);
		string GlobalFnName = MakeString(StrBuilder);


		function GlobalInitializers = {};
		GlobalInitializers.Name = DupeType(GlobalFnName, string);
		GlobalInitializers.Type = VoidFnT;
		GlobalInitializers.LinkName = StructToModuleNamePtr(GlobalFnName, File->Module->Name);
		GlobalInitializers.NoDebugInfo = true;

		block_builder Builder = MakeBlockBuilder(&GlobalInitializers, StartRegister, File->Module);

		Builder.IsGlobal = true;
		for(int I = 0; I < NodeCount; ++I)
		{
			node *Node = File->Nodes[I];
			if(Node->Type == AST_DECL)
			{
				if(Node->Decl.LHS->Type == AST_ID)
				{
					const string *Name = Node->Decl.LHS->ID.Name;
					if(Node->Decl.Expression)
					{
						u32 Expr = BuildIRFromExpression(&Builder, Node->Decl.Expression);
						const symbol *Sym = GetIRLocal(&Builder, Name);
						Assert(Sym);
						PushInstruction(&Builder, InstructionStore(Sym->Register, Expr, Node->Decl.TypeIndex));
					}
				}
				else if(Node->Decl.LHS->Type == AST_LIST)
				{
					const type *T = GetType(Node->Decl.TypeIndex);

					Assert(Node->Decl.Expression);
					Assert(T->Kind == TypeKind_Struct);
					Assert(T->Struct.Flags & StructFlag_FnReturn);

					u32 Expr = BuildIRFromExpression(&Builder, Node->Decl.Expression);
					uint At = 0;
					For(Node->Decl.LHS->List.Nodes)
					{
						Assert((*it)->Type == AST_ID);
						u32 MemT = T->Struct.Members[At].Type;
						u32 MemPtr = PushInstruction(&Builder, 
								Instruction(OP_INDEX, Expr, 1, Node->Decl.TypeIndex, &Builder));
						u32 Mem = PushInstruction(&Builder,
								Instruction(OP_LOAD, 0, MemPtr, MemT, &Builder));

						const symbol *Sym = GetIRLocal(&Builder, (*it)->ID.Name);
						Assert(Sym);
						PushInstruction(&Builder, InstructionStore(Sym->Register, Mem, Node->Decl.TypeIndex));

						At++;
					}
				}
				else unreachable;
			}
		}
		PushInstruction(&Builder, Instruction(OP_RET, -1, 0, INVALID_TYPE, &Builder));
		Terminate(&Builder, {});
		GlobalInitializers.LastRegister = Builder.LastRegister;

		IR.Functions.Push(GlobalInitializers);
		Builder.Function = NULL;
		Builder.IsGlobal = false;
		Builder.Scope.Pop().Free();

	}

	for(int I = 0; I < NodeCount; ++I)
	{
		function MaybeFunction = GlobalLevelIR(File->Nodes[I], File->Checker->Imported, File->Module, StartRegister);
		if(MaybeFunction.LastRegister > IR.MaxRegisters)
			IR.MaxRegisters = MaybeFunction.LastRegister;

		if(MaybeFunction.Name)
		{
			IR.Functions.Push(MaybeFunction);
		}
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

void BuildEnumIR(slice<module *> Modules, u32 LastRegister)
{
	array<import> ImportArray{Modules.Count};
	ForArray(Idx, Modules)
	{
		ImportArray[Idx].M = Modules[Idx];
	}

	slice<import> Imports = SliceFromArray(ImportArray);
	u32 VoidFnT = GenerateVoidFnT();

	uint TC = GetTypeCount();
	for(int i = 0; i < TC; ++i)
	{ 
		type *T = TypeTable[i];
		if(T->Kind == TypeKind_Enum)
		{
			Assert(T->Enum.Members.Count > 0);
			module *M = T->Enum.Members.Data[0].Module;
			function Fn = {};
			Fn.Name = &T->Enum.Name;
			Fn.Type = VoidFnT;
			Fn.LinkName = Fn.Name;
			Fn.NoDebugInfo = true;

			block_builder Builder = MakeBlockBuilder(&Fn, LastRegister, M);
			Builder.Imported = Imports;
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

void DissasembleBasicBlock(string_builder *Builder, basic_block *Block, int indent)
{
	ForArray(I, Block->Code)
	{
		instruction Instr = Block->Code[I];
		if(Instr.Op == OP_DEBUGINFO)
			continue;

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
			case OP_ENUM_ACCESS:
			{
				const type *T = GetType(Instr.Type);
				PushBuilderFormated(Builder, "%%%d = %s.%s", Instr.Result, GetTypeName(T), T->Enum.Members[Instr.Right].Name.Data);
			} break;
			case OP_TYPEINFO:
			{
				PushBuilderFormated(Builder, "%%%d = #info %%%d", Instr.Result, Instr.Right);
			} break;
			case OP_RDTSC:
			{
				PushBuilderFormated(Builder, "%%%d = TIME", Instr.Result);
			} break;
			case OP_MEMCMP:
			{
				ir_memcmp *Info = (ir_memcmp *)Instr.BigRegister;
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
			case OP_CAST:
			{
				const type *FromType = GetType(Instr.Right);
				PushBuilderFormated(Builder, "%%%d = CAST %s to %s %%%d", Instr.Result, GetTypeName(FromType), GetTypeName(Type), Instr.Left);
			} break;
			case OP_ALLOC:
			{
				PushBuilderFormated(Builder, "%%%d = ALLOC %s", Instr.Result, GetTypeName(Type));
			} break;
			case OP_ALLOCGLOBAL:
			{
				PushBuilderFormated(Builder, "%%%d = GLOBALALLOC %d %s", Instr.Result, Instr.BigRegister, GetTypeName(Type));
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
				ir_switchint *Info = (ir_switchint *)Instr.BigRegister;
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
			case OP_IF:
			{
				PushBuilderFormated(Builder, "IF %%%d goto block_%d, else goto block_%d", Instr.Result, Instr.Left, Instr.Right);
			} break;
			case OP_JMP:
			{
				PushBuilderFormated(Builder, "JMP block_%d", Instr.BigRegister);
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
			case OP_SPILL:
			{
				PushBuilderFormated(Builder, "SPILL %%%d", Instr.Right);
			} break;
			case OP_TOPHYSICAL:
			{
				const char *Names[] = {"rax", "rbx", "rcx", "rdx"};
				const char *Name = "unknown";
				if(Instr.Result < ARR_LEN(Names))
					Name = Names[Instr.Result];
				PushBuilderFormated(Builder, "%s = TOPHY %%%d", Name, Instr.Right);
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


void GetUsedRegisters(instruction I, dynamic<u32> &out)
{
#define OP_ALL(o) case o: out.Push(I.Result); out.Push(I.Left); out.Push(I.Right); break
#define OP_RESULT(o) case o: out.Push(I.Result); out.Push(-1); out.Push(-1); break
#define OP_BR(o) case o: out.Push(I.Result); out.Push(I.BigRegister); out.Push(-1); break

	switch(I.Op)
	{
		case OP_NOP:
		{
		} break;
		case OP_ENUM_ACCESS:
		case OP_UNREACHABLE:
		case OP_TYPEINFO:
		case OP_RDTSC: Assert(false);
		OP_RESULT(OP_CONSTINT);
		OP_RESULT(OP_CONST);
		case OP_ZEROUT:
		{
			out.Push(I.Right);
		} break;
		case OP_FN:
		{
			out.Push(I.Result);
			out.Push(-1);
			out.Push(-1);
			// @TODO: Special handling
		} break;
		OP_ALL(OP_ADD);
		OP_ALL(OP_SUB);
		OP_ALL(OP_MUL);
		OP_ALL(OP_DIV);
		OP_ALL(OP_MOD);
		case OP_LOAD:
		{
			out.Push(I.Result);
			out.Push(-1);
			out.Push(I.Right);
			// Needs special handling for memcpy
		} break;
		case OP_STORE:
		{
			out.Push(I.Result);
			out.Push(-1);
			out.Push(I.Right);
			// Needs special handling for memcpy
		} break;
		case OP_CAST:
		{
			out.Push(I.Result);
			out.Push(I.Left);
			out.Push(-1);
		} break;
		OP_RESULT(OP_ALLOC);
		OP_BR(OP_ALLOCGLOBAL);
		case OP_RET:
		{
			if(I.Left != -1)
			{
				out.Push(-1);
				out.Push(I.Left);
				out.Push(-1);
			}
			else
			{
				out.Push(-1);
				out.Push(-1);
				out.Push(-1);
			}
		} break;
		case OP_MEMCMP:
		{
			// Needs special handling (maybe) (idk basic block stuff)
			ir_memcmp *Info = (ir_memcmp *)I.BigRegister;
			out.Push(Info->LeftPtr);
			out.Push(Info->RightPtr);
			out.Push(Info->Count);
		} break;
		case OP_CALL:
		{
			// Needs special handling (maybe) (idk basic block stuff)
			out.Push(I.Result);
			call_info *Info = (call_info *)I.BigRegister;
			out.Push(Info->Operand);
			ForArray(i, Info->Args)
			{
				out.Push(Info->Operand);
			}
		} break;
		case OP_SWITCHINT:
		{
			out.Push(I.Result);
			// Needs special handling (maybe) (idk basic block stuff)
		} break;
		OP_RESULT(OP_IF);
		case OP_JMP:
		{
			out.Push(-1);
			out.Push(-1);
			out.Push(-1);
		} break;
		OP_ALL(OP_INDEX);
		case OP_ARRAYLIST:
		{
			out.Push(I.Result);
			out.Push(-1);
			out.Push(-1);
			// Needs special handling
		} break;
		case OP_MEMSET:
		{
			out.Push(-1);
			out.Push(-1);
			out.Push(I.Right);
			// Needs special handling
			// function call frees all registers
		} break;
		OP_RESULT(OP_ARG);

		OP_ALL(OP_NEQ);
		OP_ALL(OP_GREAT);
		OP_ALL(OP_GEQ);
		OP_ALL(OP_LESS);
		OP_ALL(OP_LEQ);
		OP_ALL(OP_SL);
		OP_ALL(OP_SR);
		OP_ALL(OP_EQEQ);
		OP_ALL(OP_AND);
		OP_ALL(OP_OR);
		OP_ALL(OP_XOR);
		case OP_DEBUGINFO:
		{
		} break;
		OP_ALL(OP_PTRDIFF);
		case OP_SPILL:
		case OP_TOPHYSICAL:
		case OP_COUNT: unreachable;
	}
}

