#include "LLVMType.h"
#include "LLVMBase.h"
#include "llvm-c/Core.h"

dynamic<LLVMTypeEntry> LLVMTypeMap;

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

// I am a week man, I used ChatGPT as I didn't want to write my 4th custom type to LLVMTypeRef conversion function
LLVMTypeRef ConvertToLLVMType(LLVMContextRef Context, u32 TypeID) {
	const type *CustomType = GetType(TypeID);

    Assert(Context);
    Assert(TypeID != INVALID_TYPE);
	Assert(CustomType);

    switch (CustomType->Kind) {
        case TypeKind_Basic:
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
                case Basic_string:
					return LLVMFindMapType(TypeID);
                case Basic_cstring:
                    return LLVMPointerType(LLVMInt8TypeInContext(Context), 0);
                default:
                    return NULL;
            }

        case TypeKind_Function: {
			return LLVMFindMapType(TypeID);
        }

        case TypeKind_Struct: {
			return LLVMFindMapType(TypeID);
        }

        case TypeKind_Pointer:
            return LLVMPointerType(ConvertToLLVMType(Context, CustomType->Pointer.Pointed), 0);

        case TypeKind_Invalid:
        default:
            return NULL;
    }
}

void LLVMCreateOpaqueStringStructType(LLVMContextRef Context, u32 TypeID)
{
	const type *Type = GetType(TypeID);
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	Assert(Type);
	LLVMTypeRef Opaque = LLVMStructCreateNamed(Context, Type->Struct.Name.Data);
	LLVMMapType(Basic_string, Opaque);
	LLVMMapType(TypeID, Opaque);
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

void LLVMDefineStructType(LLVMContextRef Context, u32 TypeID)
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
}

void LLVMCreateFunctionType(LLVMContextRef Context, u32 TypeID)
{
	const type *Type = GetType(TypeID);
	Assert(Context);
	Assert(TypeID != INVALID_TYPE);
	Assert(Type);

	LLVMTypeRef ReturnType;
	if(Type->Function.Return != INVALID_TYPE)
		ReturnType = ConvertToLLVMType(Context, Type->Function.Return);
	else
		ReturnType = LLVMVoidTypeInContext(Context);

	LLVMTypeRef *ArgTypes = (LLVMTypeRef *)VAlloc(Type->Function.ArgCount * sizeof(LLVMTypeRef));
	for (int i = 0; i < Type->Function.ArgCount; ++i) {
		ArgTypes[i] = ConvertToLLVMType(Context, Type->Function.Args[i]);
	}
	LLVMTypeRef FuncType = LLVMFunctionType(ReturnType, ArgTypes, Type->Function.ArgCount, false);
	LLVMMapType(TypeID, FuncType);
	VFree(ArgTypes);

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
		// @TODO: pointers
		Assert(false);
	}
	else
	{
		Assert(false);
	}
	Assert(false);
	return LLVMCatchSwitch;
}

