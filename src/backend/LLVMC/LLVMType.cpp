#include "LLVMType.h"
#include "Log.h"
#include "Memory.h"
#include "Semantics.h"
#include "Type.h"
#include "backend/LLVMC/LLVMBase.h"
#include "llvm-c/Core.h"
#include "llvm-c/DebugInfo.h"

LLVMTypeRef LLVMFindMapType(generator *g, u32 ToFind)
{
	ForArray(Idx, g->LLVMTypeMap)
	{
		if(g->LLVMTypeMap[Idx].TypeID == ToFind)
		{
			return g->LLVMTypeMap[Idx].Ref;
		}
	}
	return NULL;
}

void LLVMMapType(generator *g, u32 TypeID, LLVMTypeRef LLVMType)
{
	LLVMTypeEntry Entry;
	Entry.TypeID = TypeID;
	Entry.Ref = LLVMType;
	g->LLVMTypeMap.Push(Entry);
}

void LLVMClearTypeMap(generator *g)
{
	g->LLVMTypeMap.Count = 0;
}

LLVMMetadataRef LLVMDebugFindMapType(generator *g, u32 ToFind, b32 *OutIsForwardDecl=NULL)
{
	ForArray(Idx, g->LLVMDebugTypeMap)
	{
		if(g->LLVMDebugTypeMap[Idx].TypeID == ToFind)
		{
			if(OutIsForwardDecl)
				*OutIsForwardDecl = g->LLVMDebugTypeMap[Idx].IsForwardDecl;
			return g->LLVMDebugTypeMap[Idx].Ref;
		}
	}
	return NULL;
}

void LLVMDebugMapType(generator *g, u32 TypeID, LLVMMetadataRef LLVMType, b32 AsForwardDecl)
{
	LLVMDebugMetadataEntry Entry;
	Entry.TypeID = TypeID;
	Entry.Ref = LLVMType;
	Entry.IsForwardDecl = AsForwardDecl;
	g->LLVMDebugTypeMap.Push(Entry);
}

void LLVMDebugReMapType(generator *g, u32 TypeID, LLVMMetadataRef LLVMType)
{
	ForArray(Idx, g->LLVMDebugTypeMap)
	{
		if(g->LLVMDebugTypeMap[Idx].TypeID == TypeID)
		{
			g->LLVMDebugTypeMap.Data[Idx].Ref = LLVMType;
			g->LLVMDebugTypeMap.Data[Idx].IsForwardDecl = false;
			return;
		}
	}
	unreachable;
}

void LLVMDebugClearTypeMap(generator *g)
{
	g->LLVMDebugTypeMap.Count = 0;
}

LLVMTypeRef ConvertToLLVMType(generator *g, u32 TypeID) {
	if(TypeID == INVALID_TYPE)
		return LLVMVoidTypeInContext(g->ctx);
	const type *CustomType = GetType(TypeID);
	LLVMContextRef Context = g->ctx;

	Assert(Context);
	Assert(CustomType);

	switch (CustomType->Kind) {
		case TypeKind_Basic:
		{
			switch (CustomType->Basic.Kind) {
				case Basic_bool:
				return LLVMInt1TypeInContext(Context);
				case Basic_u8:
				return LLVMInt8TypeInContext(Context);
				case Basic_u16:
				return LLVMInt16TypeInContext(Context);
				case Basic_u32:
				return LLVMInt32TypeInContext(Context);
				case Basic_u64:
				return LLVMInt64TypeInContext(Context);
				case Basic_i8:
				return LLVMInt8TypeInContext(Context);
				case Basic_i16:
				return LLVMInt16TypeInContext(Context);
				case Basic_i32:
				return LLVMInt32TypeInContext(Context);
				case Basic_i64:
				return LLVMInt64TypeInContext(Context);
				case Basic_f32:
				return LLVMFloatTypeInContext(Context);
				case Basic_f64:
				return LLVMDoubleTypeInContext(Context);
				case Basic_int:
				return LLVMIntTypeInContext(Context, GetRegisterTypeSize());
				case Basic_uint:
				return LLVMIntTypeInContext(Context, GetRegisterTypeSize());
				case Basic_string:
				return LLVMFindMapType(g, TypeID);
				case Basic_type:
				return LLVMIntTypeInContext(Context, GetRegisterTypeSize());
				default:
				return NULL;
			}
		} break;

		case TypeKind_Array:
		{
			return LLVMArrayType2(ConvertToLLVMType(g, CustomType->Array.Type), CustomType->Array.MemberCount);
		} break;

		case TypeKind_Slice:
		{
			LLVMTypeRef Result = LLVMFindMapType(g, TypeID);
			if(Result == NULL)
			{
				LLVMTypeRef Members[] = {
					ConvertToLLVMType(g, Basic_int),
					ConvertToLLVMType(g, GetPointerTo(CustomType->Slice.Type)),
				};
				Result = LLVMStructCreateNamed(Context, "slice");
				LLVMStructSetBody(Result, Members, 2, false);
				LLVMMapType(g, TypeID, Result);
			}

			return Result;
		} break;

		case TypeKind_Function:
		{
			return LLVMCreateFunctionType(g, TypeID);
		} break;

		case TypeKind_Struct:
		{
			return LLVMFindMapType(g, TypeID);
		} break;

		case TypeKind_Vector:
		{
			switch(CustomType->Vector.Kind)
			{
				case Vector_Float:
				{
					return LLVMVectorType(LLVMFloatTypeInContext(Context), CustomType->Vector.ElementCount);
				} break;
				case Vector_Int:
				{
					return LLVMVectorType(LLVMInt32TypeInContext(Context), CustomType->Vector.ElementCount);
				} break;
			}
		} break;

		case TypeKind_Enum:
		{
			return ConvertToLLVMType(g, CustomType->Enum.Type);
		} break;

		case TypeKind_Pointer:
		{
			if(CustomType->Pointer.Pointed == INVALID_TYPE)
				return LLVMPointerType(LLVMVoidTypeInContext(Context), 0);
			LLVMTypeRef Pointed = ConvertToLLVMType(g, CustomType->Pointer.Pointed);
			return LLVMPointerType(Pointed, 0);
		} break;

		case TypeKind_Invalid:
		default:
		unreachable;
		return NULL;
	}
	Assert(false);
	return NULL;
}

LLVMMetadataRef ToDebugTypeLLVM(generator *gen, u32 TypeID)
{
	LLVMMetadataRef Found = LLVMDebugFindMapType(gen, TypeID);
	if(Found)
		return Found;

	if(TypeID == INVALID_TYPE)
		return NULL;

	const type *CustomType = GetType(TypeID);
	LLVMMetadataRef Made = NULL;

	Assert(gen);
	Assert(TypeID != INVALID_TYPE);
	Assert(CustomType);
	switch(CustomType->Kind)
	{
		case TypeKind_Basic:
		{
			int Size = GetTypeSize(CustomType) * 8;
			string Name = GetTypeNameAsString(CustomType);

			switch(CustomType->Basic.Kind)
			{
				case Basic_u8:
				case Basic_u16:
				case Basic_u32:
				case Basic_u64:
				case Basic_type:
				case Basic_uint:
				{
					Made = LLVMDIBuilderCreateBasicType(gen->dbg, Name.Data, Name.Size, Size, DW_ATE_unsigned, LLVMDIFlagZero);
				} break;
				case Basic_i8:
				case Basic_i16:
				case Basic_i32:
				case Basic_i64:
				case Basic_int:
				{
					Made = LLVMDIBuilderCreateBasicType(gen->dbg, Name.Data, Name.Size, Size, DW_ATE_signed, LLVMDIFlagZero);
				} break;
				case Basic_bool:
				{
					Made = LLVMDIBuilderCreateBasicType(gen->dbg, Name.Data, Name.Size, Size, DW_ATE_boolean, LLVMDIFlagZero);
				} break;
				case Basic_f32:
				case Basic_f64:
				{
					Made = LLVMDIBuilderCreateBasicType(gen->dbg, Name.Data, Name.Size, Size, DW_ATE_float, LLVMDIFlagZero);
				} break;
				default: unreachable;
			}
		} break;
		case TypeKind_Slice:
		{
			int Size = GetRegisterTypeSize() * 2;

			LLVMMetadataRef Mem1 = LLVMDIBuilderCreateMemberType(
					gen->dbg, gen->f_dbg,
					"count", 5,
					gen->f_dbg, 0,
					Size / 2, Size / 2,
					0, LLVMDIFlagZero,
					ToDebugTypeLLVM(gen, Basic_int));

			LLVMMetadataRef Mem2 = LLVMDIBuilderCreateMemberType(
					gen->dbg, gen->f_dbg,
					"data", 4,
					gen->f_dbg, 0,
					Size / 2, Size / 2,
					Size / 2, LLVMDIFlagZero,
					ToDebugTypeLLVM(gen, GetPointerTo(CustomType->Slice.Type)));

			LLVMMetadataRef Elements[] = {
				Mem1,
				Mem2,
			};

			Made = LLVMDIBuilderCreateStructType(gen->dbg, gen->f_dbg,
					"", 0,
					gen->f_dbg, 0,
					Size, GetRegisterTypeSize(),
					LLVMDIFlagZero, NULL,
					Elements, 2, 0,
					NULL, NULL, 0);
		} break;
		case TypeKind_Array:
		{
			int Size = GetTypeSize(CustomType) * 8;
			int Align = GetTypeAlignment(CustomType) * 8;
			LLVMMetadataRef Subrange = LLVMDIBuilderGetOrCreateSubrange(gen->dbg, 0, CustomType->Array.MemberCount);
			Made = LLVMDIBuilderCreateArrayType(gen->dbg, Size, Align, ToDebugTypeLLVM(gen, CustomType->Array.Type), &Subrange, 1);
		} break;
		case TypeKind_Pointer:
		{
			if(CustomType->Pointer.Pointed == INVALID_TYPE)
			{
				Made = LLVMDIBuilderCreatePointerType(gen->dbg, NULL, GetRegisterTypeSize(), GetRegisterTypeSize(), 0, "*void", 5);
			}
			else
			{
				string Name = GetTypeNameAsString(CustomType);
				Made = LLVMDIBuilderCreatePointerType(gen->dbg, ToDebugTypeLLVM(gen, CustomType->Pointer.Pointed), GetRegisterTypeSize(), GetRegisterTypeSize(), 0, Name.Data, Name.Size);
			}
		} break;
		case TypeKind_Function:
		{
			LLVMMetadataRef *ArgTypes = (LLVMMetadataRef *)VAlloc(sizeof(LLVMMetadataRef) * (CustomType->Function.ArgCount+1));
			if(CustomType->Function.Returns.Count != 0)
			{
				u32 Returns = ReturnsToType(CustomType->Function.Returns);
				const type *RetType = GetType(Returns);
				if(RetType->Kind == TypeKind_Function)
				{
					u32 FnPtr = GetPointerTo(INVALID_TYPE);
					ArgTypes[0] = ToDebugTypeLLVM(gen,  FnPtr);
				}
				else
					ArgTypes[0] = ToDebugTypeLLVM(gen, Returns);
			}
			else
				ArgTypes[0] = NULL;
			for(int i = 0; i < CustomType->Function.ArgCount; ++i)
			{
				ArgTypes[i+1] = ToDebugTypeLLVM(gen, CustomType->Function.Args[i]);
			}
			Made = LLVMDIBuilderCreateSubroutineType(gen->dbg, gen->f_dbg, ArgTypes, CustomType->Function.ArgCount+1, LLVMDIFlagZero);
			VFree(ArgTypes);
		} break;
		case TypeKind_Vector:
		{
			LLVMMetadataRef Type = NULL;
			switch(CustomType->Vector.Kind)
			{
				case Vector_Float:
				{
					Type = ToDebugTypeLLVM(gen, Basic_f32);
				} break;
				case Vector_Int:
				{
					Type = ToDebugTypeLLVM(gen, Basic_i32);
				} break;
			}
			LLVMMetadataRef Subrange = LLVMDIBuilderGetOrCreateSubrange(gen->dbg, 0, CustomType->Vector.ElementCount);
			Made = LLVMDIBuilderCreateVectorType(gen->dbg, CustomType->Vector.ElementCount * 32, 0, Type, &Subrange, 1);
		} break;
		case TypeKind_Struct:
		{
			LERROR("No debug info for struct %s", GetTypeName(CustomType));
			Assert(false);
		} break;
		default: unreachable;
	}
	Assert(Made);
	LLVMDebugMapType(gen, TypeID, Made);
	return Made;
}

void LLMVDebugOpaqueStruct(generator *gen, u32 TypeID)
{
	const type *CustomType = GetType(TypeID);
	string Name = GetTypeNameAsString(CustomType);

	split Split = SplitAt(Name, '.');
	if(Split.second.Data != NULL)
		Name = Split.second;

	int Size = GetTypeSize(CustomType) * 8;
	int Align = GetTypeAlignment(CustomType) * 8;

	LLVMMetadataRef Made = LLVMDIBuilderCreateForwardDecl(gen->dbg,
			DW_TAG_structure_type,
			Name.Data, Name.Size,
			gen->f_dbg, gen->f_dbg,
			0, 0,
			Size, Align,
			"", 0);

	/*
	   LLVMMetadataRef Made = LLVMDIBuilderCreateStructType(gen->dbg,
	   gen->f_dbg,
	   Name.Data,
	   Name.Size,
	   gen->f_dbg,
	   0,
	   0,
	   0,
	   LLVMDIFlagFwdDecl,
	   NULL,
	   NULL,
	   0,
	   0,
	   NULL,
	   NULL,
	   0);
	   */

	LLVMDebugMapType(gen, TypeID, Made, true);
}

LLVMMetadataRef LLVMDebugDefineEnum(generator *gen, const type *T, u32 TypeID)
{
	scratch_arena S;

	uint Size  = GetTypeSize(T->Enum.Type) * 8;
	uint Align = GetTypeAlignment(T->Enum.Type) * 8;

	LLVMMetadataRef *Elements = (LLVMMetadataRef *)S.Allocate(T->Enum.Members.Count *
			sizeof(LLVMMetadataRef));
	ForArray(Idx, T->Enum.Members)
	{
		enum_member M = T->Enum.Members[Idx];

		Elements[Idx] = LLVMDIBuilderCreateEnumerator(gen->dbg,
				M.Name.Data, M.Name.Size, M.Value.Int.Signed, !M.Value.Int.IsSigned);
	}


	LLVMMetadataRef Made = LLVMDIBuilderCreateEnumerationType(
			gen->dbg, gen->f_dbg,
			T->Enum.Name.Data, T->Enum.Name.Size,
			gen->f_dbg, 0,
			Size, Align,
			Elements, T->Enum.Members.Count,
			ToDebugTypeLLVM(gen, T->Enum.Type));
	LLVMDebugMapType(gen, TypeID, Made);
	return Made;
}

LLVMMetadataRef LLMVDebugDefineStruct(generator *gen, u32 TypeID)
{
	const type *CustomType = GetType(TypeID);
	LLVMMetadataRef *Members = (LLVMMetadataRef *)AllocatePermanent(sizeof(LLVMMetadataRef) * CustomType->Struct.Members.Count);
	ForArray(Idx, CustomType->Struct.Members)
	{
		auto Member = CustomType->Struct.Members[Idx];
		int Size = GetTypeSize(Member.Type) * 8;
		int Alignment = GetTypeAlignment(Member.Type) * 8;
		int Offset = GetStructMemberOffset(CustomType, Idx) * 8;
		
		b32 IsForwardDecl = false;
		LLVMMetadataRef DebugT = NULL;
		if(GetType(Member.Type)->Kind == TypeKind_Struct)
		{
			DebugT = LLVMDebugFindMapType(gen, Member.Type, &IsForwardDecl);
			if(IsForwardDecl)
			{
				return NULL;
			}
		}

		if(DebugT == NULL)
			DebugT = ToDebugTypeLLVM(gen, Member.Type);

		Members[Idx] = LLVMDIBuilderCreateMemberType(gen->dbg, gen->f_dbg, Member.ID.Data, Member.ID.Size, gen->f_dbg, 0, Size, Alignment, Offset, LLVMDIFlagZero, DebugT);
	}
	string Name = GetTypeNameAsString(CustomType);

	split Split = SplitAt(Name, '.');
	if(Split.second.Data != NULL)
		Name = Split.second;

	int Size = GetTypeSize(CustomType) * 8;
	LLVMMetadataRef Made = LLVMDIBuilderCreateStructType(gen->dbg,
			gen->f_dbg,
			Name.Data,
			Name.Size,
			gen->f_dbg,
			0,
			Size,
			0,
			LLVMDIFlagZero,
			NULL,
			Members,
			CustomType->Struct.Members.Count,
			0,
			NULL,
			NULL,
			0);

	LLVMDebugReMapType(gen, TypeID, Made);
	return Made;
}

void LLVMCreateOpaqueStructType(generator *g, u32 TypeID)
{
	const type *Type = GetType(TypeID);
	Assert(g);
	Assert(g->ctx);
	Assert(TypeID != INVALID_TYPE);
	Assert(Type);
	LLVMTypeRef Opaque = LLVMStructCreateNamed(g->ctx, Type->Struct.Name.Data);
	LLVMMapType(g, TypeID, Opaque);
}

LLVMTypeRef LLVMDefineStructType(generator *g, u32 TypeID)
{
	const type *Type = GetType(TypeID);
	LLVMContextRef Context = g->ctx;
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	Assert(Type);

	LLVMTypeRef Opaque = LLVMFindMapType(g, TypeID);
	Assert(Opaque);

	if(Type->Struct.Flags & StructFlag_Union)
	{
		u32 BiggestMemberTypeID = Type->Struct.Members[0].Type;
		u32 BiggetMember = GetTypeSize(BiggestMemberTypeID);
		auto MemberCount = Type->Struct.Members.Count;
		for (size_t Idx = 1; Idx < MemberCount; ++Idx) {
			u32 MemberType = Type->Struct.Members[Idx].Type;
			u32 Size = GetTypeSize(MemberType);
			if(Size > BiggetMember)
			{
				BiggetMember = Size;
				BiggestMemberTypeID = MemberType;
			}
		}
		LLVMTypeRef BiggestType = ConvertToLLVMType(g, BiggestMemberTypeID);
		LLVMStructSetBody(Opaque, &BiggestType, 1, Type->Struct.Flags & StructFlag_Packed);
	}
	else
	{
		auto MemberCount = Type->Struct.Members.Count;
		LLVMTypeRef *MemberTypes = (LLVMTypeRef *)VAlloc(MemberCount * sizeof(LLVMTypeRef));
		for (size_t Idx = 0; Idx < MemberCount; ++Idx) {
			u32 MemberType = Type->Struct.Members[Idx].Type;
			MemberTypes[Idx] = ConvertToLLVMType(g, MemberType);
		}
		LLVMStructSetBody(Opaque, MemberTypes, MemberCount, Type->Struct.Flags & StructFlag_Packed);
		VFree(MemberTypes);
	}
	return Opaque;
}

void LLVMFixFunctionComplexParameter(generator *g, u32 ArgTypeIdx, const type *ArgType, LLVMTypeRef *Result, int *IdxOut)
{
	if(ArgType->Kind != TypeKind_Struct)
	{
		Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(g, ArgTypeIdx), 0);
		*IdxOut = *IdxOut + 1;
		return;
	}

	Assert(ArgType->Kind == TypeKind_Struct);
	int Size = GetTypeSize(ArgType);
	if(Size > MAX_PARAMETER_SIZE)
	{
		Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(g, ArgTypeIdx), 0);
		*IdxOut = *IdxOut + 1;
		return;
	}

	b32 AllFloats = IsStructAllFloats(ArgType);
	if(AllFloats && PTarget != platform_target::Windows)
	{
		type *Type = AllocType(TypeKind_Vector);
		Type->Vector.Kind = Vector_Float;
		Type->Vector.ElementCount = 2;
		u32 FloatVector = AddType(Type);

		for(int i = 0; i < ArgType->Struct.Members.Count / 2; ++i)
		{
			Result[*IdxOut] = ConvertToLLVMType(g, FloatVector);
			*IdxOut = *IdxOut + 1;
		}

		if(ArgType->Struct.Members.Count % 2 != 0)
		{
			Result[*IdxOut] = ConvertToLLVMType(g, Basic_f32);
			*IdxOut = *IdxOut + 1;
		}

#if 0
		ForArray(Idx, ArgType->Struct.Members)
		{
			u32 MemTypeIdx = ArgType->Struct.Members[Idx].Type;
			if(Idx + 1 != ArgType->Struct.Members.Count)
			{
				const type *MemType = GetType(MemTypeIdx);
				u32 NextMemTypeIdx = ArgType->Struct.Members[Idx+1].Type;
				if(NextMemTypeIdx != MemTypeIdx || MemType->Basic.Kind == Basic_f64)
				{
					Result[*IdxOut] = ConvertToLLVMType(Context, MemTypeIdx);
					*IdxOut = *IdxOut + 1;
					Result[*IdxOut] = ConvertToLLVMType(Context, NextMemTypeIdx);
					*IdxOut = *IdxOut + 1;
				}
				else
				{
					Result[*IdxOut] = ConvertToLLVMType(Context, FloatVector);
					*IdxOut = *IdxOut + 1;
				}
				Idx++;
			}
			else
			{
				Result[*IdxOut] = ConvertToLLVMType(Context, MemTypeIdx);
				*IdxOut = *IdxOut + 1;
			}
		}
#endif
		return;
	}

	switch(Size)
	{
		case 8:
		{
			Result[*IdxOut] = ConvertToLLVMType(g, Basic_u64);
		} break;
		case 4:
		{
			Result[*IdxOut] = ConvertToLLVMType(g, Basic_u32);
		} break;
		case 2:
		{
			Result[*IdxOut] = ConvertToLLVMType(g, Basic_u16);
		} break;
		case 1:
		{
			Result[*IdxOut] = ConvertToLLVMType(g, Basic_u8);
		} break;
		default:
		{
			static bool WarningGiven = false;
			if(Size > 8 && !WarningGiven)
			{
				WarningGiven = true;
				LWARN("Passing structs of size %d to functions is not properly supported, if it's used to interface with other languages, it will result in unspecified behavior", Size);
			}
			Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(g, ArgTypeIdx), 0);
		} break;
	}
	*IdxOut = *IdxOut + 1;
}

LLVMTypeRef LLVMCreateFunctionType(generator *g, u32 TypeID)
{
	LLVMTypeRef Found = LLVMFindMapType(g, TypeID);
	if(Found)
		return Found;

	// @Performance
	dynamic<arg_location> Unused = {};
	b32 Unused2 = false;
	TypeID = FixFunctionTypeForCallConv(TypeID, Unused, &Unused2);
	Unused.Free();

	LLVMContextRef Context = g->ctx;
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	Found = LLVMFindMapType(g, TypeID);
	if(Found)
		return Found;

	const type *Type = GetType(TypeID);
	Assert(Type);


	LLVMTypeRef *ArgTypes = (LLVMTypeRef *)VAlloc((Type->Function.ArgCount+2) * sizeof(LLVMTypeRef) * 12);
	int ArgCount = 0;

	LLVMTypeRef ReturnType;
	if(Type->Function.Returns.Count != 0)
	{
		u32 Returns = ReturnsToType(Type->Function.Returns);
		const type *RT = GetType(Returns);
		if(IsRetTypePassInPointer(Returns))
		{
			ReturnType = LLVMVoidTypeInContext(Context);
			ArgTypes[ArgCount++] = LLVMPointerType(ConvertToLLVMType(g, Returns), 0);
		}
		else if(RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array)
		{
			if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT) && PTarget != platform_target::Windows)
				ReturnType = ConvertToLLVMType(g, AllFloatsStructToReturnType(RT));
			else
				ReturnType = ConvertToLLVMType(g, ComplexTypeToSizeType(RT));
		}
		else if(RT->Kind == TypeKind_Function)
		{
			ReturnType = LLVMPointerType(LLVMVoidTypeInContext(Context), 0);
		}
		else
		{
			ReturnType = ConvertToLLVMType(g, Returns);
		}
	}
	else
		ReturnType = LLVMVoidTypeInContext(Context);

	for (int i = 0; i < Type->Function.ArgCount; ++i) {
		const type *ArgType = GetType(Type->Function.Args[i]);
		if(!IsLoadableType(ArgType))
			LLVMFixFunctionComplexParameter(g, Type->Function.Args[i], ArgType, ArgTypes, &ArgCount);
		else
			ArgTypes[ArgCount++] = ConvertToLLVMType(g, Type->Function.Args[i]);
	}

	bool IsVarArg = false;
	if(Type->Function.Flags & SymbolFlag_VarFunc)
	{
		if(IsForeign(Type))
		{
			IsVarArg = true;
		}
		else
		{
			u32 ArgType = FindStruct(STR_LIT("base.Arg"));
			u32 VarArgType = GetPointerTo(GetSliceType(ArgType));
			ArgTypes[ArgCount++] = ConvertToLLVMType(g, VarArgType);
		}
	}

	Assert(ArgCount < (Type->Function.ArgCount+2)*12);
	LLVMTypeRef FuncType = LLVMFunctionType(ReturnType, ArgTypes, ArgCount, IsVarArg);
	LLVMMapType(g, TypeID, FuncType);
	VFree(ArgTypes);

	return FuncType;
}

LLVMOpcode RCCastTrunc(const type *From, const type *To)
{
	if(To->Basic.Flags & BasicFlag_Float)
	{
		return LLVMFPTrunc;
	}
	else
	{
		return LLVMTrunc;
	}
}

LLVMOpcode RCCastExt(const type *From, const type *To)
{
	// float
	if(To->Basic.Flags & BasicFlag_Float)
	{
		return LLVMFPExt;
	}
	// from is unsigned, zero ext
	else if(From->Basic.Flags & BasicFlag_Unsigned)
	{
		return LLVMZExt;
	}
	// from is signed, sign ext
	else
	{
		return LLVMSExt;
	}
}

// @NOTE: Assumes one is float and other isn't
LLVMOpcode RCCastFloatInt(const type *From, const type *To)
{
	if(From->Basic.Flags & BasicFlag_Float)
	{
		if(To->Basic.Flags & BasicFlag_Unsigned)
		{
			return LLVMFPToUI;
		}
		else
		{
			return LLVMFPToSI;
		}
	}
	else if(To->Basic.Flags & BasicFlag_Float)
	{
		if(From->Basic.Flags & BasicFlag_Unsigned)
		{
			return LLVMUIToFP;
		}
		else
		{
			return LLVMSIToFP;
		}
	}
	else
	{
		Assert(false);
	}
	return LLVMCatchSwitch;
}

LLVMOpcode RCCast(const type *From, const type *To)
{
	if(From->Kind == TypeKind_Basic && To->Kind == TypeKind_Basic)
	{
		int FromSize = GetTypeSize(From);
		int ToSize   = GetTypeSize(To);

		if((From->Basic.Flags & BasicFlag_Float) != (To->Basic.Flags & BasicFlag_Float))
		{
			return RCCastFloatInt(From, To);
		}
		else if(ToSize < FromSize)
		{
			return RCCastTrunc(From, To);
		}
		else if(ToSize > FromSize)
		{
			return RCCastExt(From, To);
		}
		return LLVMCatchSwitch;
	}
	else if(From->Kind == TypeKind_Pointer || To->Kind == TypeKind_Pointer)
	{
		if(From->Kind == TypeKind_Pointer && To->Kind == TypeKind_Basic)
		{
			return LLVMPtrToInt;
		}
		else if(To->Kind == TypeKind_Pointer && From->Kind == TypeKind_Basic)
		{
			return LLVMIntToPtr;
		}
		else
		{
			// @Note: other pointer casts ?
			// ???
			// It should probably not do anything here, pointer to pointer shouldn't be
			// outputed to the backend
			unreachable;
		}
	}
	else if(From->Kind == TypeKind_Enum)
	{
		return RCCast(GetType(From->Enum.Type), To);
	}
	else if(To->Kind == TypeKind_Enum)
	{
		return RCCast(From, GetType(To->Enum.Type));
	}
	else
	{
		Assert(false);
	}
	Assert(false);
	return LLVMCatchSwitch;
}


