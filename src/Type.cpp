#include "Type.h"
#include "Memory.h"
#include "VString.h"
#include "Basic.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
const type BasicTypes[] = {
	{TypeKind_Basic, {Basic_bool,   BasicFlag_Boolean,                             4, STR_LIT("bool")}},
	{TypeKind_Basic, {Basic_string, BasicFlag_String,                             -1, STR_LIT("string")}},
	{TypeKind_Basic, {Basic_cstring,BasicFlag_CString,                            -1, STR_LIT("cstring")}},

	{TypeKind_Basic, {Basic_u8,   BasicFlag_Integer | BasicFlag_Unsigned,          1, STR_LIT("u8")}},
	{TypeKind_Basic, {Basic_u16,  BasicFlag_Integer | BasicFlag_Unsigned,          2, STR_LIT("u16")}},
	{TypeKind_Basic, {Basic_u32,  BasicFlag_Integer | BasicFlag_Unsigned,          4, STR_LIT("u32")}},
	{TypeKind_Basic, {Basic_u64,  BasicFlag_Integer | BasicFlag_Unsigned,          8, STR_LIT("u64")}},

	{TypeKind_Basic, {Basic_i8,   BasicFlag_Integer,                               1, STR_LIT("i8")}},
	{TypeKind_Basic, {Basic_i16,  BasicFlag_Integer,                               2, STR_LIT("i16")}},
	{TypeKind_Basic, {Basic_i32,  BasicFlag_Integer,                               4, STR_LIT("i32")}},
	{TypeKind_Basic, {Basic_i64,  BasicFlag_Integer,                               8, STR_LIT("i64")}},

	{TypeKind_Basic, {Basic_f32,  BasicFlag_Float,                                 4, STR_LIT("f32")}},
	{TypeKind_Basic, {Basic_f64,  BasicFlag_Float,                                 8, STR_LIT("f64")}},

	{TypeKind_Basic, {Basic_UntypedInteger, BasicFlag_Integer | BasicFlag_Untyped, 0, STR_LIT("untyped integer")}},
	{TypeKind_Basic, {Basic_UntypedFloat,   BasicFlag_Float   | BasicFlag_Untyped, 0, STR_LIT("untyped float")}},

	{TypeKind_Basic, {Basic_uint,  BasicFlag_Integer | BasicFlag_Unsigned,        -1, STR_LIT("uint")}},
	{TypeKind_Basic, {Basic_int,   BasicFlag_Integer,                             -1, STR_LIT("int")}},
	{TypeKind_Basic, {Basic_type,  BasicFlag_TypeID,                              -1, STR_LIT("type")}},

	{TypeKind_Basic, {Basic_auto, 0,                                              -1, STR_LIT("auto")}},
};

//#pragma clang diagnostic pop

static const int BasicTypesCount = (sizeof(BasicTypes) / sizeof(BasicTypes[0]));

const type *BasicBool      = &BasicTypes[Basic_bool];
const type *UntypedInteger = &BasicTypes[Basic_UntypedInteger];
const type *UntypedFloat   = &BasicTypes[Basic_UntypedFloat];
const type *BasicInt       = &BasicTypes[Basic_int];
const type *BasicUint      = &BasicTypes[Basic_uint];
const type *BasicF32       = &BasicTypes[Basic_f32];
const type *BasicU8        = &BasicTypes[Basic_u8];

uint TypeCount = 0;
const size_t MAX_TYPES = MB(1);
const type **InitializeTypeTable()
{
	const type **Types = (const type **)AllocateVirtualMemory(sizeof(type *) * MAX_TYPES);
	for(int I = 0; I < BasicTypesCount; ++I)
	{
		Types[TypeCount++] = &BasicTypes[I];
	}

	return Types;
}

const type **TypeTable = InitializeTypeTable();

b32 IsUntyped(const type *Type)
{
	return (Type->Kind == TypeKind_Basic) && (Type->Basic.Flags & BasicFlag_Untyped);
}

inline const type *GetType(u32 TypeIdx)
{
	// Bad?
#if defined(DEBUG)
	if(TypeIdx == INVALID_TYPE)
		return NULL;
#endif

	return TypeTable[TypeIdx];
}

uint GetTypeCount()
{
	return TypeCount;
}

u32 GetReturnType(const type *Type)
{
	Assert(Type->Kind == TypeKind_Function);
	return Type->Function.Return;
}

u32 AddType(type *Type)
{
	LockMutex();

	TypeTable[TypeCount++] = Type;
	u32 Result = TypeCount - 1;

	UnlockMutex();
	return Result;
}

int GetRegisterTypeSize()
{
	// @TODO: Other platforsm :|
	return 64;
}

// @TODO: Non basic type size calculation and arch dependant type sizes
int GetBasicTypeSize(const type *Type)
{
	if(Type->Basic.Size != -1)
		return Type->Basic.Size;
	else if(Type->Basic.Kind == Basic_int || Type->Basic.Kind == Basic_uint)
		return GetRegisterTypeSize() / 8;
	else
		return GetRegisterTypeSize() / 8;
	unreachable;
}

int GetStructSize(const type *Type)
{
	if(Type->Struct.Members.Count == 0)
		return 0;
	int Result = 0;
	int MaxMemberSize = 0;
	ForArray(Idx, Type->Struct.Members)
	{
		int MemberSize = GetTypeSize(GetType(Type->Struct.Members[Idx].Type));
		if(MemberSize > MaxMemberSize)
			MaxMemberSize = MemberSize;
		Result += MemberSize;
		if(Idx + 1 != Type->Struct.Members.Count)
		{
			int AlignSize = GetTypeSize(GetType(Type->Struct.Members[Idx+1].Type));
			if(MemberSize < AlignSize)
				Result += AlignSize - MemberSize;
		}
	}
	Result += Result % MaxMemberSize;
	return Result;
}

int GetStructMemberOffset(const type *Type, uint Member)
{
	if(Type->Struct.Members.Count == 0)
		return 0;
	int Result = 0;
	for(int Idx = 0; Idx < Member; ++Idx)
	{
		int MemberSize = GetTypeSize(GetType(Type->Struct.Members[Idx].Type));

		Result += MemberSize;
		if(Idx + 1 != Type->Struct.Members.Count)
		{
			int AlignSize = GetTypeSize(GetType(Type->Struct.Members[Idx+1].Type));
			if(MemberSize < AlignSize)
				Result += AlignSize - MemberSize;
		}
	}
	return Result;
}

int GetStructMemberOffset(u32 TypeIdx, uint Member)
{
	const type *Type = GetType(TypeIdx);
	return GetStructMemberOffset(Type, Member);
}

// In bytes
int GetTypeSize(const type *Type)
{
	switch(Type->Kind)
	{
		case TypeKind_Basic:
		return GetBasicTypeSize(Type);
		case TypeKind_Function:
		case TypeKind_Pointer:
		return GetRegisterTypeSize() / 8;
		case TypeKind_Array:
		{
			return GetTypeSize(GetType(Type->Array.Type)) * Type->Array.MemberCount;
		} break;
		case TypeKind_Struct:
		{
			return GetStructSize(Type);
		} break;
		default: {};
	}
	unreachable;
}

int GetTypeSize(u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	return GetTypeSize(Type);
}

b32 HasBasicFlag(const type *Type, u32 FlagMask)
{
	return (Type->Kind == TypeKind_Basic) && (Type->Basic.Flags & FlagMask);
}

b32 IsCallable(const type *Type)
{
	if(Type->Kind == TypeKind_Function)
		return true;
	if(Type->Kind == TypeKind_Pointer)
	{
		const type *Pointed = GetType(Type->Pointer.Pointed);
		return Pointed->Kind == TypeKind_Function;
	}
	return false;
}

b32 CheckMissmatch(int LeftFlags, int RightFlags, basic_flags Flag)
{
	if((LeftFlags & Flag) != (RightFlags & Flag))
		return true;
	return false;
}

b32 CheckBasicTypes(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment)
{
	if(Left->Basic.Kind == Basic_string && Right->Basic.Kind == Basic_string)
		return true;

	if(Left->Basic.Kind == Basic_cstring && Right->Basic.Kind == Basic_cstring)
		return true;

	int LeftFlags = Left->Basic.Flags;
	int RightFlags = Right->Basic.Flags;
	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_TypeID))
		return false;

	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_String))
		return false;

	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_CString))
		return false;

	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_Unsigned))
	{
		if(LeftFlags & BasicFlag_Untyped || RightFlags & BasicFlag_Untyped)
		{}
		else
			return false;
	}

	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_Float))
	{
		if(IsAssignment && (RightFlags & BasicFlag_Float))
			return false;
		// @Note: Only promote when one is an integer and the other is float
		if(!CheckMissmatch(LeftFlags, RightFlags, BasicFlag_Integer))
			return false;
		if(PotentialPromotion)
		{
			if(LeftFlags & BasicFlag_Float)
				*PotentialPromotion = Left;
			else
				*PotentialPromotion = Right;
		}
		else return false;
	}
	int LeftSize  = GetBasicTypeSize(Left);
	int RightSize = GetBasicTypeSize(Right);
	if(LeftSize < RightSize)
	{
		if(IsAssignment)
			return false;
		if(PotentialPromotion)
		{
			// @Note: Trying to promote to smaller type
			if(*PotentialPromotion == Left)
				return false;
			*PotentialPromotion = Right;
		}
		else return false;
	}
	else if(LeftSize > RightSize)
	{
		if(PotentialPromotion)
		{
			// @Note: Trying to promote to smaller type
			if(*PotentialPromotion == Right)
				return false;
			*PotentialPromotion = Left;
		}
		else return false;
	}
	return true;
}

b32 IsCastValid(const type *From, const type *To)
{
	if(From->Kind == TypeKind_Pointer && To->Kind == TypeKind_Basic)
	{
		return GetTypeSize(To) == GetRegisterTypeSize() / 8;
	}

	if(From->Kind == TypeKind_Basic && To->Kind == TypeKind_Pointer)
	{
		return GetTypeSize(From) == GetRegisterTypeSize() / 8;
	}

	if(From->Kind != To->Kind)
		return false;

	if(From->Kind == TypeKind_Basic && To->Kind == TypeKind_Basic)
	{
		if(From->Basic.Kind == Basic_string && To->Basic.Kind == Basic_cstring)
		{
			return true;
		}
	}

	return true;
}

b32 TypesMustMatch(const type *Left, const type *Right)
{
	if(Left->Kind != Right->Kind)
		return false;

	switch(Left->Kind)
	{
		case TypeKind_Basic:
		{
			int LeftSize  = GetBasicTypeSize(Left);
			int RightSize = GetBasicTypeSize(Right);
			if(LeftSize != RightSize)
				return false;
			return Left->Basic.Kind == Right->Basic.Kind;
		} break;
		case TypeKind_Pointer:
		{
			if(Left->Pointer.Pointed == INVALID_TYPE || Right->Pointer.Pointed == INVALID_TYPE)
			{
				return (Left->Pointer.Pointed == INVALID_TYPE) == (Right->Pointer.Pointed == INVALID_TYPE);
			}
			return TypesMustMatch(GetType(Left->Pointer.Pointed), GetType(Right->Pointer.Pointed));
		} break;
		default:
		{
			Assert(false);
			return false;
		} break;
	}
}

b32 IsTypeCompatible(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment)
{
	if(Left->Kind == TypeKind_Pointer && HasBasicFlag(Right, BasicFlag_Integer))
	{
		*PotentialPromotion = Left;
		return true;
	}
	if(Left->Kind == TypeKind_Function || Right->Kind == TypeKind_Function)
	{
		if(Left->Kind == TypeKind_Pointer)
		{
			Left = GetType(Left->Pointer.Pointed);
		}
		else if(Right->Kind == TypeKind_Pointer)
		{
			Right = GetType(Right->Pointer.Pointed);
		}
	}

	if(Left->Kind != Right->Kind)
		return false;

	switch(Left->Kind)
	{
		case TypeKind_Basic:
		{
			return CheckBasicTypes(Left, Right, PotentialPromotion, IsAssignment);
		} break;
		case TypeKind_Pointer:
		{
			if(Left->Pointer.Pointed == INVALID_TYPE || Right->Pointer.Pointed == INVALID_TYPE)
				return true;
			return TypesMustMatch(GetType(Left->Pointer.Pointed), GetType(Right->Pointer.Pointed));
		} break;
		case TypeKind_Array:
		{
			if(!IsAssignment)
				return false;

			const type *Type = GetType(Right->Array.Type);
			if(IsUntyped(Type))
			{
				*PotentialPromotion = Left;
			}
			else if(Left->Array.Type != Right->Array.Type)
				return false;

			if(Left->Array.MemberCount == 0)
			{
				if(Right->Array.MemberCount == 0)
					return false;

				if(*PotentialPromotion != NULL)
					return false;

				*PotentialPromotion = Right;
				return true;
			}
			
			return Left->Array.MemberCount == Right->Array.MemberCount;
		} break;
		case TypeKind_Struct:
		{
			if(!IsAssignment)
				return false;

			return Left->Struct.Name == Right->Struct.Name;
		} break;
		case TypeKind_Function:
		{
			if(Left->Function.ArgCount != Right->Function.ArgCount)
				return false;

			const type *LeftReturn = GetType(Left->Function.Return);
			const type *RightReturn = GetType(Right->Function.Return);
			if((LeftReturn == NULL) != (RightReturn == NULL))
				return false;
			if(!TypesMustMatch(LeftReturn, RightReturn))
				return false;

			for(int Idx = 0; Idx < Left->Function.ArgCount; ++Idx)
			{
				const type *LeftArg  = GetType(Left->Function.Args[Idx]);
				const type *RightArg = GetType(Left->Function.Args[Idx]);
				if(!TypesMustMatch(LeftArg, RightArg))
					return false;
			}
			return true;
		} break;
		default:
		{
			Assert(false);
			return false;
		} break;
	}


	return false;
}

b32 IsCastRedundant(const type *From, const type *To)
{
	if(From->Kind == To->Kind)
	{
		switch(From->Kind)
		{
			case TypeKind_Basic:
			{
				return From->Basic.Kind == To->Basic.Kind;
			} break;
			case TypeKind_Pointer:
			{
				if(From->Pointer.Pointed == INVALID_TYPE || To->Pointer.Pointed == INVALID_TYPE)
				{
					return (From->Pointer.Pointed == INVALID_TYPE) == (To->Pointer.Pointed == INVALID_TYPE);
				}
				return IsCastRedundant(GetType(From->Pointer.Pointed), GetType(To->Pointer.Pointed));
			} break;
			default:
			{
				Assert(false);
			} break;
		}
	}
	return false;
}

b32 ShouldCopyType(const type *Type)
{
	if(Type->Kind != TypeKind_Basic && Type->Kind != TypeKind_Pointer)
	{
		return false;
	}
	else
	{
		if(Type->Kind == TypeKind_Basic && Type->Basic.Kind == Basic_string)
			return false;
	}
	return true;
}

b32 IsLoadableType(const type *Type)
{
	return Type->Kind != TypeKind_Array && Type->Kind != TypeKind_Struct;
}

b32 IsLoadableType(u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	return IsLoadableType(Type);
}

const char *GetTypeName(u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	return GetTypeName(Type);
}

const char *GetTypeName(const type *Type)
{
	switch (Type->Kind)
	{
		case TypeKind_Basic:
		{
			return Type->Basic.Name.Data;
		} break;
		case TypeKind_Array:
		{
			string_builder Builder = MakeBuilder();
			PushBuilderFormated(&Builder, "%s[%d]", GetTypeName(GetType(Type->Array.Type)), Type->Array.MemberCount);
			return MakeString(Builder).Data;
		} break;
		case TypeKind_Pointer:
		{
			string_builder Builder = MakeBuilder();
			if(Type->Pointer.Pointed != INVALID_TYPE)
				PushBuilderFormated(&Builder, "*%s", GetTypeName(GetType(Type->Pointer.Pointed)));
			else
				PushBuilderFormated(&Builder, "*"); 
			return MakeString(Builder).Data;
		} break;
		case TypeKind_Struct:
		{
			return Type->Struct.Name.Data;
		} break;
		case TypeKind_Function:
		{
			string_builder Builder = MakeBuilder();
			PushBuilder(&Builder, "fn(");
			for(int i = 0; i < Type->Function.ArgCount; ++i)
			{
				PushBuilderFormated(&Builder, "%s", GetTypeName(Type->Function.Args[i]));
				if(i + 1 != Type->Function.ArgCount)
					PushBuilder(&Builder, ", ");
			}
			PushBuilder(&Builder, ')');
			if(Type->Function.Return != INVALID_TYPE)
				PushBuilderFormated(&Builder, " -> %s", GetTypeName(Type->Function.Return));
			return MakeString(Builder).Data;
		} break;
		default:
		{
			return "(Error! Unkown type name)";
		} break;
	}
}

// @TODO: Maybe try to find it first... idk
u32 GetPointerTo(u32 TypeIdx)
{
	type *New = NewType(type);
	New->Kind = TypeKind_Pointer;
	New->Pointer.Pointed = TypeIdx;
	return AddType(New);
}

