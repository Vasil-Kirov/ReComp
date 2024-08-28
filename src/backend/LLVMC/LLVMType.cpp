#include "LLVMType.h"
#include "Log.h"
#include "Memory.h"
#include "Type.h"
#include "backend/LLVMC/LLVMBase.h"
#include "llvm-c/Core.h"
#include "llvm-c/DebugInfo.h"

dynamic<LLVMTypeEntry> LLVMTypeMap;
dynamic<LLVMDebugMetadataEntry> LLVMDebugTypeMap;

LLVMTypeRef LLVMFindMapType(u32 ToFind)
{
	ForArray(Idx, LLVMTypeMap)
	{
		if(LLVMTypeMap[Idx].TypeID == ToFind)
		{
			return LLVMTypeMap[Idx].LLVMRef;
		}
	}
	return NULL;
}

void LLVMMapType(u32 TypeID, LLVMTypeRef LLVMType)
{
	LLVMTypeEntry Entry;
	Entry.TypeID = TypeID;
	Entry.LLVMRef = LLVMType;
	LLVMTypeMap.Push(Entry);
}

void LLVMClearTypeMap()
{
	LLVMTypeMap.Count = 0;
}

LLVMMetadataRef LLVMDebugFindMapType(u32 ToFind)
{
	ForArray(Idx, LLVMDebugTypeMap)
	{
		if(LLVMDebugTypeMap[Idx].TypeID == ToFind)
		{
			return LLVMDebugTypeMap[Idx].Ref;
		}
	}
	return NULL;
}

void LLVMDebugMapType(u32 TypeID, LLVMMetadataRef LLVMType)
{
	LLVMDebugMetadataEntry Entry;
	Entry.TypeID = TypeID;
	Entry.Ref = LLVMType;
	LLVMDebugTypeMap.Push(Entry);
}

void LLVMDebugReMapType(u32 TypeID, LLVMMetadataRef LLVMType)
{
	ForArray(Idx, LLVMDebugTypeMap)
	{
		if(LLVMDebugTypeMap[Idx].TypeID == TypeID)
		{
			LLVMDebugTypeMap.Data[Idx].Ref = LLVMType;
			return;
		}
	}
	unreachable;
}

void LLVMDebugClearTypeMap()
{
	LLVMDebugTypeMap.Count = 0;
}

// I am a week man, I used ChatGPT as I didn't want to write my 4th custom type to LLVMTypeRef conversion function
LLVMTypeRef ConvertToLLVMType(LLVMContextRef Context, u32 TypeID) {
	if(TypeID == INVALID_TYPE)
		return LLVMVoidTypeInContext(Context);
	const type *CustomType = GetType(TypeID);

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
					return LLVMFindMapType(TypeID);
                case Basic_cstring:
                    return LLVMPointerType(LLVMInt8TypeInContext(Context), 0);
                default:
                    return NULL;
            }
		} break;

		case TypeKind_Array:
		{
			return LLVMArrayType(ConvertToLLVMType(Context, CustomType->Array.Type), CustomType->Array.MemberCount);
		} break;

        case TypeKind_Function:
		{
			return LLVMCreateFunctionType(Context, TypeID);
        } break;

        case TypeKind_Struct:
		{
			return LLVMFindMapType(TypeID);
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

        case TypeKind_Pointer:
		{
			if(CustomType->Pointer.Pointed == INVALID_TYPE)
				return LLVMPointerType(LLVMVoidTypeInContext(Context), 0);
            return LLVMPointerType(ConvertToLLVMType(Context, CustomType->Pointer.Pointed), 0);
		} break;

        case TypeKind_Invalid:
        default:
            return NULL;
    }
	Assert(false);
	return NULL;
}

LLVMMetadataRef ToDebugTypeLLVM(generator *gen, u32 TypeID)
{
	LLVMMetadataRef Found = LLVMDebugFindMapType(TypeID);
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
				case Basic_cstring:
				{
					Made = LLVMDIBuilderCreatePointerType
					(
						 gen->dbg,
						 ToDebugTypeLLVM(gen, Basic_u8),
						 Size, 0, 0,
						 "*u8", 3);
				} break;
				default: unreachable;
			}
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
			if(CustomType->Function.Return != INVALID_TYPE)
			{
				const type *RetType = GetType(CustomType->Function.Return);
				if(RetType->Kind == TypeKind_Function)
				{
					u32 FnPtr = GetPointerTo(INVALID_TYPE);
					ArgTypes[0] = ToDebugTypeLLVM(gen,  FnPtr);
				}
				else
					ArgTypes[0] = ToDebugTypeLLVM(gen, CustomType->Function.Return);
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
		case TypeKind_Struct:
		{
			LERROR("No debug info for struct %s", GetTypeName(CustomType));
			Assert(false);
		} break;
		default: unreachable;
	}
	Assert(Made);
	LLVMDebugMapType(TypeID, Made);
	return Made;
}

void LLMVDebugOpaqueStruct(generator *gen, u32 TypeID)
{
	const type *CustomType = GetType(TypeID);
	string Name = GetTypeNameAsString(CustomType);
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

	LLVMDebugMapType(TypeID, Made);
}

LLVMMetadataRef LLMVDebugDefineStruct(generator *gen, u32 TypeID)
{
	const type *CustomType = GetType(TypeID);
	LLVMMetadataRef *Members = (LLVMMetadataRef *)VAlloc(sizeof(LLVMMetadataRef) * CustomType->Struct.Members.Count);
	ForArray(Idx, CustomType->Struct.Members)
	{
		auto Member = CustomType->Struct.Members[Idx];
		int Size = GetTypeSize(Member.Type) * 8;
		int Alignment = GetTypeAlignment(Member.Type) * 8;
		int Offset = GetStructMemberOffset(CustomType, Idx) * 8;
		Members[Idx] = LLVMDIBuilderCreateMemberType(gen->dbg, gen->f_dbg, Member.ID.Data, Member.ID.Size, gen->f_dbg, 0, Size, Alignment, Offset, LLVMDIFlagZero, ToDebugTypeLLVM(gen, Member.Type));
	}
	string Name = GetTypeNameAsString(CustomType);
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

	LLVMDebugReMapType(TypeID, Made);
	return Made;
}

void LLVMCreateOpaqueStructType(LLVMContextRef Context, u32 TypeID)
{
	const type *Type = GetType(TypeID);
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	Assert(Type);
	LLVMTypeRef Opaque = LLVMStructCreateNamed(Context, Type->Struct.Name.Data);
	LLVMMapType(TypeID, Opaque);
}

LLVMTypeRef LLVMDefineStructType(LLVMContextRef Context, u32 TypeID)
{
	const type *Type = GetType(TypeID);
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	Assert(Type);

	LLVMTypeRef Opaque = LLVMFindMapType(TypeID);
	Assert(Opaque);

	auto MemberCount = Type->Struct.Members.Count;
	LLVMTypeRef *MemberTypes = (LLVMTypeRef *)VAlloc(MemberCount * sizeof(LLVMTypeRef));
	for (size_t Idx = 0; Idx < MemberCount; ++Idx) {
		u32 MemberType = Type->Struct.Members[Idx].Type;
		MemberTypes[Idx] = ConvertToLLVMType(Context, MemberType);
	}
	LLVMStructSetBody(Opaque, MemberTypes, MemberCount, Type->Struct.Flags & StructFlag_Packed);
	VFree(MemberTypes);
	return Opaque;
}

void LLVMFixFunctionComplexParameter(LLVMContextRef Context, u32 ArgTypeIdx, const type *ArgType, LLVMTypeRef *Result, int *IdxOut)
{
	if(ArgType->Kind == TypeKind_Array)
	{
		Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(Context, ArgTypeIdx), 0);
		*IdxOut = *IdxOut + 1;
		return;
	}
	else if(ArgType->Kind == TypeKind_Function)
	{
		Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(Context, ArgTypeIdx), 0);
		*IdxOut = *IdxOut + 1;
		return;
	}

	Assert(ArgType->Kind == TypeKind_Struct);
	int Size = GetTypeSize(ArgType);
	if(Size > MAX_PARAMETER_SIZE)
	{
		Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(Context, ArgTypeIdx), 0);
		*IdxOut = *IdxOut + 1;
		return;
	}

	b32 AllFloats = IsStructAllFloats(ArgType);
	if(AllFloats)
	{
		type *Type = AllocType(TypeKind_Vector);
		Type->Vector.Kind = Vector_Float;
		Type->Vector.ElementCount = 2;
		u32 FloatVector = AddType(Type);

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
		return;
	}

	switch(Size)
	{
		case 8:
		{
			Result[*IdxOut] = ConvertToLLVMType(Context, Basic_u64);
		} break;
		case 4:
		{
			Result[*IdxOut] = ConvertToLLVMType(Context, Basic_u32);
		} break;
		case 2:
		{
			Result[*IdxOut] = ConvertToLLVMType(Context, Basic_u16);
		} break;
		case 1:
		{
			Result[*IdxOut] = ConvertToLLVMType(Context, Basic_u8);
		} break;
		default:
		{
			if(Size > 8)
			{
				LFATAL("Passing struct of size %d to function is not currently supported", Size);
			}
			Result[*IdxOut] = LLVMPointerType(ConvertToLLVMType(Context, ArgTypeIdx), 0);
		} break;
	}
	*IdxOut = *IdxOut + 1;
}

LLVMTypeRef LLVMCreateFunctionType(LLVMContextRef Context, u32 TypeID)
{
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	LLVMTypeRef Found = LLVMFindMapType(TypeID);
	if(Found)
		return Found;

	const type *Type = GetType(TypeID);
	Assert(Type);


	LLVMTypeRef *ArgTypes = (LLVMTypeRef *)VAlloc((Type->Function.ArgCount+1) * sizeof(LLVMTypeRef) * 2);
	int ArgCount = 0;

	LLVMTypeRef ReturnType;
	if(Type->Function.Return != INVALID_TYPE)
	{
		const type *RT = GetType(Type->Function.Return);
		if(IsRetTypePassInPointer(Type->Function.Return))
		{
					ReturnType = LLVMVoidTypeInContext(Context);
					ArgTypes[ArgCount++] = LLVMPointerType(ConvertToLLVMType(Context, Type->Function.Return), 0);

		}
		else if(RT->Kind == TypeKind_Struct || RT->Kind == TypeKind_Array)
		{
			if(RT->Kind == TypeKind_Struct && IsStructAllFloats(RT))
				ReturnType = ConvertToLLVMType(Context, AllFloatsStructToReturnType(RT));
			else
				ReturnType = ConvertToLLVMType(Context, ComplexTypeToSizeType(RT));
		}
		else if(RT->Kind == TypeKind_Function)
		{
			ReturnType = LLVMPointerType(LLVMVoidTypeInContext(Context), 0);
		}
		else
		{
			ReturnType = ConvertToLLVMType(Context, Type->Function.Return);
		}
	}
	else
		ReturnType = LLVMVoidTypeInContext(Context);

	for (int i = 0; i < Type->Function.ArgCount; ++i) {
		const type *ArgType = GetType(Type->Function.Args[i]);
		if(!IsLoadableType(ArgType))
			LLVMFixFunctionComplexParameter(Context, Type->Function.Args[i], ArgType, ArgTypes, &ArgCount);
		else
			ArgTypes[ArgCount++] = ConvertToLLVMType(Context, Type->Function.Args[i]);
	}
	LLVMTypeRef FuncType = LLVMFunctionType(ReturnType, ArgTypes, ArgCount, false);
	LLVMMapType(TypeID, FuncType);
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
			// @TODO: other pointer casts ?
			// ???
			// It should probably not do anything here, pointer to pointer shouldn't be
			// outputed to the backend
			Assert(false);
		}
	}
	else
	{
		Assert(false);
	}
	Assert(false);
	return LLVMCatchSwitch;
}


