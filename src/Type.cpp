#include "Type.h"
#include "Semantics.h"
#include "Memory.h"
#include "VString.h"
#include "Basic.h"
#include "Log.h"
#include "Dict.h"
#include <mutex>

platform_target PTarget = platform_target::Windows;

#define NOT_DEFINED ((uint)-1)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
type BasicTypes[] = {
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_bool,   BasicFlag_Boolean | BasicFlag_Unsigned,        1, STR_LIT("bool")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_string, BasicFlag_String,                             16, STR_LIT("string")}},
	//{TypeKind_Basic, {Basic_cINVALID_TYPE, string,BasicFlag_CString,                            -1, STR_LIT("cstring")}},

	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_u8,   BasicFlag_Integer | BasicFlag_Unsigned,          1, STR_LIT("u8")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_u16,  BasicFlag_Integer | BasicFlag_Unsigned,          2, STR_LIT("u16")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_u32,  BasicFlag_Integer | BasicFlag_Unsigned,          4, STR_LIT("u32")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_u64,  BasicFlag_Integer | BasicFlag_Unsigned,          8, STR_LIT("u64")}},

	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_i8,   BasicFlag_Integer,                               1, STR_LIT("i8")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_i16,  BasicFlag_Integer,                               2, STR_LIT("i16")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_i32,  BasicFlag_Integer,                               4, STR_LIT("i32")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_i64,  BasicFlag_Integer,                               8, STR_LIT("i64")}},

	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_f32,  BasicFlag_Float,                                 4, STR_LIT("f32")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_f64,  BasicFlag_Float,                                 8, STR_LIT("f64")}},

	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_UntypedInteger, BasicFlag_Integer | BasicFlag_Untyped, 0, STR_LIT("untyped integer")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_UntypedFloat,   BasicFlag_Float   | BasicFlag_Untyped, 0, STR_LIT("untyped float")}},

	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_int,   BasicFlag_Integer,                             -1, STR_LIT("int")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_uint,  BasicFlag_Integer | BasicFlag_Unsigned,        -1, STR_LIT("uint")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_type,  BasicFlag_TypeID,                              -1, STR_LIT("type")}},

	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_auto,   0,                                            -1, STR_LIT("auto")}},
	{NOT_DEFINED, NOT_DEFINED, INVALID_TYPE, TypeKind_Basic, {Basic_module, 0,                                            -1, STR_LIT("module")}},
};

#pragma clang diagnostic pop

const int BasicTypesCount = (sizeof(BasicTypes) / sizeof(BasicTypes[0]));

//const type *BasicBool      = &BasicTypes[Basic_bool];
//const type *UntypedInteger = &BasicTypes[Basic_UntypedInteger];
//const type *UntypedFloat   = &BasicTypes[Basic_UntypedFloat];
//const type *BasicInt       = &BasicTypes[Basic_int];
//const type *BasicUint      = &BasicTypes[Basic_uint];
//const type *BasicF32       = &BasicTypes[Basic_f32];
//const type *BasicU8        = &BasicTypes[Basic_u8];

uint TypeCount = 0;
const size_t MAX_TYPES = MB(16);
type **InitializeTypeTable()
{
	type **Types = (type **)AllocateVirtualMemory(sizeof(type *) * MAX_TYPES);
	for(int I = 0; I < BasicTypesCount; ++I)
	{
		Types[TypeCount++] = &BasicTypes[I];
	}

	return Types;
}

void AddVectorTypes()
{
	type *FloatV2 = AllocType(TypeKind_Vector);
	FloatV2->Vector.Kind = Vector_Float;
	FloatV2->Vector.ElementCount = 2;
	AddTypeWithName(FloatV2, STR_LIT("v2"));

	//type *FloatV3 = AllocType(TypeKind_Vector);
	//FloatV3->Vector.Kind = Vector_Float;
	//FloatV3->Vector.ElementCount = 3;
	//AddTypeWithName(FloatV3, STR_LIT("v3"));

	type *FloatV4 = AllocType(TypeKind_Vector);
	FloatV4->Vector.Kind = Vector_Float;
	FloatV4->Vector.ElementCount = 4;
	AddTypeWithName(FloatV4, STR_LIT("v4"));

	type *IntV2 = AllocType(TypeKind_Vector);
	IntV2->Vector.Kind = Vector_Int;
	IntV2->Vector.ElementCount = 2;
	AddTypeWithName(IntV2, STR_LIT("iv2"));

	//type *IntV3 = AllocType(TypeKind_Vector);
	//IntV3->Vector.Kind = Vector_Int;
	//IntV3->Vector.ElementCount = 3;
	//AddTypeWithName(IntV3, STR_LIT("iv3"));

	type *IntV4 = AllocType(TypeKind_Vector);
	IntV4->Vector.Kind = Vector_Int;
	IntV4->Vector.ElementCount = 4;
	AddTypeWithName(IntV4, STR_LIT("iv4"));

	// type *UIntV2 = AllocType(TypeKind_Vector);
	// UIntV2->Vector.Kind = Vector_UInt;
	// UIntV2->Vector.ElementCount = 2;
	// AddTypeWithName(UIntV2, STR_LIT("uv2"));

	// type *UIntV3 = AllocType(TypeKind_Vector);
	// UIntV3->Vector.Kind = Vector_UInt;
	// UIntV3->Vector.ElementCount = 3;
	// AddTypeWithName(UIntV3, STR_LIT("uv3"));

	// type *UIntV4 = AllocType(TypeKind_Vector);
	// UIntV4->Vector.Kind = Vector_UInt;
	// UIntV4->Vector.ElementCount = 4;
	// AddTypeWithName(UIntV4, STR_LIT("uv4"));
}

type **TypeTable = InitializeTypeTable();
dict<u32> TypeMap = { .Default = INVALID_TYPE };
std::mutex TypeMutex = {};
u32 NULLType = GetPointerTo(INVALID_TYPE, PointerFlag_Optional);

void AddNameToTypeMap(const string *Name, u32 T)
{
	TypeMap.Add(*Name, T);
}

u32 LookupNameOnTypeMap(const string *Name)
{
	return TypeMap[*Name];
}

u32 VarArgArrayType(u32 ElemCount, u32 ArgT)
{
	type *T = AllocType(TypeKind_Array);
	T->Array.Type = ArgT;
	T->Array.MemberCount = ElemCount;

	return AddType(T);
}

b32 IsUntyped(const type *Type)
{
	return (Type->Kind == TypeKind_Basic) && (Type->Basic.Flags & BasicFlag_Untyped);
}

b32 IsUntyped(u32 T)
{
	return IsUntyped(GetType(T));
}

u32 FindStruct(string Name)
{
	for(int i = 0; i < TypeCount; ++i)
	{
		if(TypeTable[i]->Kind == TypeKind_Struct)
		{
			if(TypeTable[i]->Struct.Name == Name)
				return i;
		}
	}
	unreachable;
}

u32 FindEnum(string Name)
{
	for(int i = 0; i < TypeCount; ++i)
	{
		if(TypeTable[i]->Kind == TypeKind_Enum)
		{
			if(TypeTable[i]->Enum.Name == Name)
				return i;
		}
	}
	unreachable;
}

dynamic<generic_replacement> GenericReplacements = {};

size_t AddGenericReplacement(string Generic, u32 ToReplace)
{
	auto g = generic_replacement {.Generic = Generic, .TypeID = ToReplace};
	GenericReplacements.Push(g);
	return GenericReplacements.Count - 1;
}

void ClearGenericReplacement(size_t To)
{
	GenericReplacements.Count = To;
}

inline const type *GetTypeRaw(u32 TypeIdx)
{
	return TypeTable[TypeIdx];
}

inline const type *GetType(u32 TypeIdx)
{
	if(TypeIdx == INVALID_TYPE)
	{
		static type Invalid = {.Kind = TypeKind_Invalid};
		return &Invalid;
	}

	const type *Type = TypeTable[TypeIdx];
	if(Type->Kind == TypeKind_Generic || (Type->Kind == TypeKind_Struct && Type->Struct.Flags & StructFlag_Generic))
	{
		string Name;
		if(Type->Kind == TypeKind_Generic)
			Name = Type->Generic.Name;
		else
			Name = GetTypeNameAsString(Type);

		for(int i = GenericReplacements.Count-1; i >= 0; --i)
		{
			if(GenericReplacements[i].Generic == Name)
			{
				Type = TypeTable[GenericReplacements[i].TypeID];
				break;
			}
		}
	}
	return Type;
}

uint GetTypeCount()
{
	return TypeCount;
}

void WriteFunctionReturnType(string_builder *b, slice<u32> Returns)
{
	if(Returns.Count == 0)
		return;

	if(Returns.Count == 1)
	{
		*b += GetTypeNameAsString(Returns[0]);
		return;
	}

	*b += "{ ";
	ForArray(Idx, Returns)
	{
		if(Idx != 0)
			*b +=", ";
		*b += GetTypeNameAsString(Returns[Idx]);
	}
	*b += " }";
}

u32 GetTypeForMultiReturn(slice<u32> Returns)
{
	string_builder b = MakeBuilder();
	b += "return ";
	WriteFunctionReturnType(&b, Returns);

	TypeMutex.lock();

	string Lookup = MakeString(b);
	u32 T = TypeMap[Lookup];

	TypeMutex.unlock();
	if(T != INVALID_TYPE)
		return T;

	array<struct_member> Members(Returns.Count);

	ForArray(Idx, Returns)
	{
		Members[Idx] = struct_member {
			.ID = STR_LIT(":arg:"),
			.Type = Returns[Idx],
		};
	}

	type *NewT = AllocType(TypeKind_Struct);
	NewT->Struct.Name = Lookup;
	NewT->Struct.Members = SliceFromArray(Members);
	NewT->Struct.Flags = StructFlag_FnReturn;

	return AddType(NewT);
}

u32 GetVecElemType(u32 TIdx)
{
	return GetVecElemType(GetType(TIdx));
}

u32 GetVecElemType(const type *T)
{
	Assert(T->Kind == TypeKind_Vector);
	switch(T->Vector.Kind)
	{
		case Vector_Float:
		return Basic_f32;
		case Vector_Int:
		return Basic_i32;
	}

	Assert(false);
}

u32 ReturnsToType(slice<u32> Returns)
{
	if(Returns.Count == 0)
		return INVALID_TYPE;
	if(Returns.Count == 1)
		return Returns[0];

	return GetTypeForMultiReturn(Returns);
}

u32 GetReturnType(const type *Type)
{
	Assert(Type->Kind == TypeKind_Function);
	return ReturnsToType(Type->Function.Returns);
}

u32 AddTypeWithName(type *Type, string Name)
{
	TypeMutex.lock();
	
	u32 Lookup = TypeMap[Name];
	if(Lookup != INVALID_TYPE && Type->Kind != TypeKind_Generic) {

		TypeMutex.unlock();
		return Lookup;
	}

	TypeTable[TypeCount++] = Type;
	Assert(TypeCount < MAX_TYPES);
	u32 Result = TypeCount - 1;

	Type->CachedSize = -1;
	Type->CachedAlignment = -1;
	Type->CachedAsPointer = INVALID_TYPE;
	if(Type->Kind != TypeKind_Generic)
	{
		Type->CachedSize = GetTypeSize(Type);
		Type->CachedAlignment = GetTypeAlignment(Type);
	}
	//if(Type->Kind == TypeKind_Struct)
	//{
	//	LDEBUG("Adding: %s at %d", Type->Struct.Name.Data, Result);
	//}

	TypeMap.Add(Name, Result);

	TypeMutex.unlock();
	return Result;
}

u32 AddType(type *Type)
{
	string TypeString = GetTypeNameAsString(Type);
	return AddTypeWithName(Type, TypeString);
}

void FillOpaqueStruct(u32 TypeIdx, type T)
{
	TypeMutex.lock();
	Assert(TypeTable[TypeIdx]->Kind == TypeKind_Struct);
	T.CachedSize = -1;
	T.CachedAlignment = -1;
	T.CachedAsPointer = INVALID_TYPE;
	//T.Size = GetTypeSize(&T);
	//T.Alignment = GetTypeAlignment(&T);
	*TypeTable[TypeIdx] = T;
	TypeMutex.unlock();
}

void SetStructCache(u32 TypeIdx)
{
	TypeTable[TypeIdx]->CachedSize = GetTypeSize(TypeTable[TypeIdx]);
	TypeTable[TypeIdx]->CachedAlignment = GetTypeAlignment(TypeTable[TypeIdx]);
}

int GetRegisterTypeSize()
{
	// @TODO: Other platforms :|
	return 64;
}

// @TODO: Non basic type size calculation and arch dependant type sizes
int GetBasicTypeSize(const type *Type)
{
	if(Type->Basic.Size != -1)
		return Type->Basic.Size;
	else if(Type->Basic.Kind == Basic_int || Type->Basic.Kind == Basic_uint)
		return GetRegisterTypeSize() / 8;
	else if(Type->Basic.Kind == Basic_string)
		return GetRegisterTypeSize() / 4;
	else
		return GetRegisterTypeSize() / 8;
	unreachable;
}

int GetStructSize(const type *Type)
{
	Assert(Type->Kind == TypeKind_Struct);
	if(Type->Struct.Members.Count == 0)
		return 0;

	int Result = 0;
	int BiggestMember = 0;

#if 0
	if(Type->Struct.SubType != INVALID_TYPE)
	{
		Result = GetTypeSize(Type->Struct.SubType);
		BiggestMember = Result;
	}
#endif

	ForArray(Idx, Type->Struct.Members)
	{
		const type *m = GetType(Type->Struct.Members[Idx].Type);
		int MemberSize = GetTypeSize(m);
		int Alignment = GetTypeAlignment(m);
		if(Alignment != 0)
			Result = AlignTo(Result, Alignment);
		Result += MemberSize;
		if(MemberSize > BiggestMember)
			BiggestMember = MemberSize;
	}
	if(Type->Struct.Flags & StructFlag_Union)
		return BiggestMember;
	auto sa = GetTypeAlignment(Type);
	Result = AlignTo(Result, sa);
	return Result;
}

int GetStructMemberOffset(const type *Type, uint Member)
{
	Assert(Type->Kind == TypeKind_Struct);
	if(Type->Struct.Members.Count == 0)
		return 0;

	if(Type->Struct.Flags & StructFlag_Union)
		return 0;

	int Result = 0;
#if 0
	if(Type->Struct.SubType != INVALID_TYPE)
	{
		Result = GetTypeSize(Type->Struct.SubType);
	}
#endif

	for(int Idx = 0; Idx <= Member; ++Idx)
	{
		const type *m = GetType(Type->Struct.Members[Idx].Type);
		int MemberSize = GetTypeSize(m);
		int Alignment = GetTypeAlignment(m);
		if(Alignment != 0)
			Result = AlignTo(Result, Alignment);
		Result += MemberSize;
	}
	const type *m = GetType(Type->Struct.Members[Member].Type);
	Result -= GetTypeSize(m);
	return Result;
}

int GetStructAlignment(const type *Type)
{
	Assert(Type->Kind == TypeKind_Struct);
	if(Type->Struct.Members.Count == 0)
		return 1;

	int BiggestMember = 0;
	int CurrentAlignment = 1;

#if 0
	if(Type->Struct.SubType != INVALID_TYPE)
	{
		BiggestMember = GetTypeSize(Type->Struct.SubType);
		CurrentAlignment = GetTypeAlignment(Type->Struct.SubType);
	}
#endif

	ForArray(Idx, Type->Struct.Members)
	{
		const type *m = GetType(Type->Struct.Members[Idx].Type);
		int MemberSize = GetTypeSize(m);
		if(MemberSize > BiggestMember)
		{
			BiggestMember = MemberSize;
			CurrentAlignment = GetTypeAlignment(m);
		}
	}

	return CurrentAlignment;
}

int GetStructMemberOffset(u32 TypeIdx, uint Member)
{
	const type *Type = GetType(TypeIdx);
	return GetStructMemberOffset(Type, Member);
}

// In bytes
int GetTypeSize(const type *Type)
{
	if(Type->CachedSize != -1)
		return Type->CachedSize;

	if(Type->Kind == TypeKind_Enum)
		return GetTypeSize(Type->Enum.Type);

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
		case TypeKind_Slice:
		{
			return GetRegisterTypeSize() / 4;
		} break;
		case TypeKind_Struct:
		{
			return GetStructSize(Type);
		} break;
		case TypeKind_Enum:
		{
			return GetTypeSize(Type->Enum.Type);
		} break;
		case TypeKind_Vector:
		{
			return Type->Vector.ElementCount * 4;
		} break;
		default: {};
	}
	LDEBUG("%s", GetTypeName(Type));
	unreachable;
}

int GetTypeAlignment(const type *Type)
{
	if(Type->Kind == TypeKind_Enum)
		return GetTypeAlignment(Type->Enum.Type);
	if(Type->CachedAlignment != -1)
		return Type->CachedAlignment;

	switch(Type->Kind)
	{
		case TypeKind_Basic:
		if(IsString(Type))
			return 8;
		return GetBasicTypeSize(Type);
		case TypeKind_Slice:
		case TypeKind_Function:
		case TypeKind_Pointer:
		return GetRegisterTypeSize() / 8;
		case TypeKind_Array:
		{
			return GetTypeAlignment(Type->Array.Type);
		} break;
		case TypeKind_Struct:
		{
			return GetStructAlignment(Type);
		} break;
		case TypeKind_Enum:
		{
			return GetTypeAlignment(Type->Enum.Type);
		} break;
		case TypeKind_Vector:
		{
			return Type->Vector.ElementCount * 4;
		} break;
		default: {};
	}
	LDEBUG("%d", Type->Kind);
	unreachable;
}

int GetTypeAlignment(u32 Type)
{
	return GetTypeAlignment(GetType(Type));
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

b32 IsSigned(const type *T)
{
	if(T->Kind == TypeKind_Vector)
	{
		return true;
	}
	Assert(HasBasicFlag(T, BasicFlag_Integer));
	return !HasBasicFlag(T, BasicFlag_Unsigned);
}

b32 IsCallable(const type *Type)
{
	if(Type->Kind == TypeKind_Function)
		return true;
	if(Type->Kind == TypeKind_Pointer)
	{
		if(Type->Pointer.Pointed == INVALID_TYPE)
			return false;

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

b32 EitherIsReservedType(const type *Left, const type *Right)
{
	if(Left->Kind == TypeKind_Basic && Left->Basic.Kind == Basic_module)
		return true;
	if(Right->Kind == TypeKind_Basic && Right->Basic.Kind == Basic_module)
		return true;
	return false;
}

b32 CheckBasicTypes(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment)
{
	if(Left->Basic.Kind == Basic_string && Right->Basic.Kind == Basic_string)
		return true;

	int LeftFlags = Left->Basic.Flags;
	int RightFlags = Right->Basic.Flags;
	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_TypeID))
		return false;

	if(CheckMissmatch(LeftFlags, RightFlags, BasicFlag_String))
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
	if(EitherIsReservedType(From, To))
		return false;

	if(From->Kind == TypeKind_Vector || To->Kind == TypeKind_Vector)
		return false;

	if(From->Kind == TypeKind_Pointer && To->Kind == TypeKind_Basic)
	{
		return GetTypeSize(To) == GetRegisterTypeSize() / 8;
	}

	if(From->Kind == TypeKind_Basic && To->Kind == TypeKind_Pointer)
	{
		if(IsUntyped(From) && HasBasicFlag(From, BasicFlag_Integer))
			return true;

		return GetTypeSize(From) == GetRegisterTypeSize() / 8;
	}

	if(From->Kind == TypeKind_Generic || To->Kind == TypeKind_Generic)
		return true;

	if(From->Kind == TypeKind_Struct || To->Kind == TypeKind_Struct)
		return false;

	if(From->Kind == TypeKind_Enum && To->Kind != TypeKind_Enum)
	{
		const type *T = GetType(From->Enum.Type);
		return IsCastValid(T, To);
	}
	else if(To->Kind == TypeKind_Enum && From->Kind != TypeKind_Enum)
	{
		const type *T = GetType(To->Enum.Type);
		return IsCastValid(T, From);
	}

	if(From->Kind == TypeKind_Pointer && To->Kind == TypeKind_Pointer)
	{
		if(From->Pointer.Flags & PointerFlag_Optional)
		{
			return To->Pointer.Flags & PointerFlag_Optional;
		}
	}

	if(From->Kind != To->Kind)
		return false;

	return true;
}

b32 TypesMustMatch(const type *Left, const type *Right)
{
	// I think this is fine?
	if(Left->Kind == TypeKind_Generic || Right->Kind == TypeKind_Generic)
		return true;

	if(Left->Kind != Right->Kind)
		return false;

	if(EitherIsReservedType(Left, Right))
		return false;

	switch(Left->Kind)
	{
		case TypeKind_Enum:
		{
			return Left->Enum.Name == Right->Enum.Name;
		} break;
		case TypeKind_Basic:
		{
			int LeftSize  = GetBasicTypeSize(Left);
			int RightSize = GetBasicTypeSize(Right);
			if(LeftSize != RightSize)
				return false;
			return Left->Basic.Kind == Right->Basic.Kind;
		} break;
		case TypeKind_Function:
		{
			if(Left->Function.ArgCount != Right->Function.ArgCount)
				return false;

			if(Left->Function.Returns.Count != Right->Function.Returns.Count)
				return false;

			if(Left->Function.Returns.Count != 0)
			{
				const type *RetLeft  = GetType(ReturnsToType(Left->Function.Returns));
				const type *RetRight = GetType(ReturnsToType(Right->Function.Returns));
				if(!TypesMustMatch(RetLeft, RetRight))
					return false;
			}

			for(int i = 0; i < Left->Function.ArgCount; ++i)
			{
				const type *LeftArg = GetType(Left->Function.Args[i]);
				const type *RightArg = GetType(Right->Function.Args[i]);
				if(!TypesMustMatch(LeftArg, RightArg))
					return false;
			}
			return true;
		} break;
		case TypeKind_Pointer:
		{
			return TypeCheckPointers(Left, Right, false);
		} break;
		case TypeKind_Struct:
		{
			return Left->Struct.Name == Right->Struct.Name;
		} break;
		case TypeKind_Array:
		{
			if(Left->Array.MemberCount != Right->Array.MemberCount)
				return false;
			const type *LeftArray  = GetType(Left->Array.Type);
			const type *RightArray = GetType(Right->Array.Type);
			return TypesMustMatch(LeftArray, RightArray);
		} break;
		case TypeKind_Slice:
		{
			const type *LeftSlice  = GetType(Left->Slice.Type);
			const type *RightSlice = GetType(Right->Slice.Type);
			return TypesMustMatch(LeftSlice, RightSlice);
		} break;
		case TypeKind_Vector:
		{
			return Left->Vector.Kind == Right->Vector.Kind && Left->Vector.ElementCount == Right->Vector.ElementCount;
		} break;
		case TypeKind_Invalid:
		case TypeKind_Generic:
		{
			unreachable;
		} break;
#if 0
		default:
		{
			LERROR("Unknown type kind: %d", Left->Kind);
			Assert(false);
			return false;
		} break;
#endif
	}
	unreachable;
}

b32 TypeCheckPointers(const type *L, const type *R, b32 IsAssignment)
{
	if(HAS_FLAG(L->Pointer.Flags, PointerFlag_Optional) &&
			!HAS_FLAG(R->Pointer.Flags, PointerFlag_Optional) && !IsAssignment)
		return false;

	if(HAS_FLAG(R->Pointer.Flags, PointerFlag_Optional) &&
			!HAS_FLAG(L->Pointer.Flags, PointerFlag_Optional) && IsAssignment)
		return false;

	if(L->Pointer.Pointed == INVALID_TYPE || R->Pointer.Pointed == INVALID_TYPE)
		return true;

	return TypesMustMatch(GetType(L->Pointer.Pointed), GetType(R->Pointer.Pointed));
}

b32 IsTypeCompatible(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment)
{
	if(Left->Kind == TypeKind_Generic || Right->Kind == TypeKind_Generic)
	{
		if(Left->Kind == Right->Kind)
			return TypesMustMatch(Left, Right);
		return true;
	}

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
		case TypeKind_Enum:
		{
			return TypesMustMatch(Left, Right);
		} break;
		case TypeKind_Basic:
		{
			return CheckBasicTypes(Left, Right, PotentialPromotion, IsAssignment);
		} break;
		case TypeKind_Pointer:
		{
			return TypeCheckPointers(Left, Right, IsAssignment);
		} break;
		case TypeKind_Slice:
		{
			return TypesMustMatch(GetType(Left->Slice.Type), GetType(Right->Slice.Type));
		} break;
		case TypeKind_Array:
		{
			if(!IsAssignment)
				return false;

			const type *LeftT = GetType(Left->Array.Type);
			const type *RightT = GetType(Right->Array.Type);
			if(IsUntyped(RightT))
			{
				*PotentialPromotion = Left;
			}
			else if(!TypesMustMatch(LeftT, RightT))
				return false;

			return Left->Array.MemberCount == Right->Array.MemberCount;
		} break;
		case TypeKind_Struct:
		{
			if(!IsAssignment)
				return false;

			string LeftName = Left->Struct.Name;
			string RightName = Right->Struct.Name;
			if(Left->Struct.Flags & StructFlag_Generic)
			{
				if(Right->Struct.Flags & StructFlag_Generic)
				{}
				else
				{
					split Split = SplitAt(RightName, '<');
					if(Split.first.Data == NULL)
						return false;
					RightName = Split.first;
				}
			}
			if(Right->Struct.Flags & StructFlag_Generic)
			{
				if(Left->Struct.Flags & StructFlag_Generic)
				{}
				else
				{
					split Split = SplitAt(LeftName, '<');
					if(Split.first.Data == NULL)
						return false;
					LeftName = Split.first;
				}
			}

			return LeftName == RightName;
		} break;
		case TypeKind_Function:
		{
			if(Left->Function.ArgCount != Right->Function.ArgCount)
				return false;
			if(Left->Function.Returns.Count != Right->Function.Returns.Count)
				return false;

			if(Left->Function.Returns.Count != 0)
			{
				const type *LeftReturn = GetType( ReturnsToType(Left->Function.Returns));
				const type *RightReturn = GetType(ReturnsToType(Right->Function.Returns));
				if((LeftReturn == NULL) != (RightReturn == NULL))
					return false;
				if(!TypesMustMatch(LeftReturn, RightReturn))
					return false;
			}

			for(int Idx = 0; Idx < Left->Function.ArgCount; ++Idx)
			{
				const type *LeftArg  = GetType(Left->Function.Args[Idx]);
				const type *RightArg = GetType(Left->Function.Args[Idx]);
				if(!TypesMustMatch(LeftArg, RightArg))
					return false;
			}
			return true;
		} break;
		case TypeKind_Vector:
		{
			return TypesMustMatch(Left, Right);
		} break;
		default:
		{
			Assert(false);
			return false;
		} break;
	}

	return false;
}

b32 CanTypePerformBinExpression(const type *T, token_type Op)
{
	switch(T->Kind)
	{
		case TypeKind_Basic:
		{
			if(T->Basic.Kind == Basic_module)
				return false;
			if(T->Basic.Kind == Basic_string)
			{
				// Allowed string ops:
				// ==, !=, =
				if(Op != T_EQEQ && Op != T_NEQ && Op != T_EQ)
					return false;
			}
			return true;
		} break;
		case TypeKind_Pointer:
		{
			// Allowed pointer ops:
			// ?, +, -, !=, ==, >=, <=, >, <, +, -, =
			slice<token_type> Allowed = SliceFromConst({
				T_QMARK, T_PLUS, T_MIN, T_PEQ, T_MEQ, T_NEQ, T_EQEQ, T_LESS, T_GREAT, T_LEQ, T_GEQ, T_EQ
			});

			For(Allowed)
			{
				if(*it == Op)
					return true;
			}
			return false;
		} break;
		case TypeKind_Vector:
		{
			slice<token_type> Allowed = SliceFromConst({
				T_PLUS, T_MIN, T_PTR /* times */, T_DIV, T_EQ,
			});

			For(Allowed)
			{
				if(*it == Op)
					return true;
			}
			return false;
		} break;
		case TypeKind_Struct:
		case TypeKind_Array:
		case TypeKind_Slice:
		return Op == T_EQ;

		case TypeKind_Invalid:
		return false;
		case TypeKind_Enum:
		{
			// Allowed enum ops:
			// |, &, ==, !=, =
			slice<token_type> Allowed = SliceFromConst({
				T_OR, T_AND, T_OREQ, T_ANDEQ, T_EQEQ, T_NEQ, T_EQ
			});

			For(Allowed)
			{
				if(*it == Op)
					return true;
			}

			return false;
		}

		default: return false;
	}
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
			case TypeKind_Generic:
			{
				return From->Generic.Name == To->Generic.Name && From->Generic.Scope == To->Generic.Scope;
			} break;
			case TypeKind_Enum:
			{
				return TypesMustMatch(From, To);
			} break;
			case TypeKind_Struct:
			{
				return From->Struct.Name == To->Struct.Name;
			} break;
			case TypeKind_Vector:
			{
				return false;
			} break;
			default:
			{
				LDEBUG("KIND: %d", From->Kind);
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
	return Type->Kind != TypeKind_Array && Type->Kind != TypeKind_Struct && Type->Kind != TypeKind_Function && Type->Kind != TypeKind_Slice && !HasBasicFlag(Type, BasicFlag_String);
}

b32 IsCString(const type *T)
{
	return T->Kind == TypeKind_Pointer && T->Pointer.Pointed == Basic_u8;
}

b32 IsString(const type *T, b32 OrCString)
{
	if(HasBasicFlag(T, BasicFlag_String))
		return true;

	if(OrCString)
		return IsCString(T);

	return false;
}

b32 IsFn(const type *T)
{
	return T->Kind == TypeKind_Function;
}

b32 IsFnOrPtr(const type *T)
{
	if(IsFn(T))
		return true;
	if(T->Kind == TypeKind_Pointer)
	{
		if(T->Pointer.Pointed == INVALID_TYPE)
			return false;
		const type *P = GetType(T->Pointer.Pointed);
		return IsFn(P);
	}
	return false;
}

b32 IsLoadableType(u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	return IsLoadableType(Type);
}

string GetTypeNameAsString(u32 Type)
{
	if(Type == INVALID_TYPE)
		return STR_LIT("");

	return GetTypeNameAsString(GetType(Type));
}

string GetTypeNameAsString(const type *Type)
{
	switch (Type->Kind)
	{
		case TypeKind_Invalid:
		{
			return STR_LIT("void");
		} break;
		case TypeKind_Enum:
		{
			return Type->Enum.Name;
		} break;
		case TypeKind_Vector:
		{
			string TypeString;
			switch(Type->Vector.Kind)
			{
				case Vector_Float:
				{
					TypeString = STR_LIT("f32");
				} break;
				case Vector_Int:
				{
					TypeString = STR_LIT("i32");
				} break;
			}
			string_builder Builder = MakeBuilder();
			PushBuilderFormated(&Builder, "<%d x %s>", Type->Vector.ElementCount, TypeString.Data);
			return MakeString(Builder);
		} break;
		case TypeKind_Basic:
		{
			return Type->Basic.Name;
		} break;
		case TypeKind_Slice:
		{
			string_builder Builder = MakeBuilder();
			PushBuilderFormated(&Builder, "[]%s", GetTypeName(GetType(Type->Slice.Type)));
			return MakeString(Builder);
		} break;
		case TypeKind_Array:
		{
			string_builder Builder = MakeBuilder();
			PushBuilderFormated(&Builder, "[%d]%s", Type->Array.MemberCount, GetTypeName(GetType(Type->Array.Type)));
			return MakeString(Builder);
		} break;
		case TypeKind_Pointer:
		{
			string_builder Builder = MakeBuilder();
			if(Type->Pointer.Flags & PointerFlag_Optional)
				Builder += '?';

			if(Type->Pointer.Pointed != INVALID_TYPE)
				PushBuilderFormated(&Builder, "*%s", GetTypeName(GetType(Type->Pointer.Pointed)));
			else
				Builder += '*';
			return MakeString(Builder);
		} break;
		case TypeKind_Struct:
		{
			return Type->Struct.Name;
		} break;
		case TypeKind_Function:
		{
			string_builder Builder = MakeBuilder();


			PushBuilder(&Builder, "fn(");
			for(int i = 0; i < Type->Function.ArgCount; ++i)
			{
				PushBuilderFormated(&Builder, "%s", GetTypeName(Type->Function.Args[i]));
				For(Type->Function.DefaultValues)
				{
					if((*it).Idx == i)
					{
						PushBuilder(&Builder, "=<default>");
					}
				}
				if(i + 1 != Type->Function.ArgCount)
					PushBuilder(&Builder, ", ");
			}
			PushBuilder(&Builder, ')');
			if(Type->Function.Returns.Count != 0)
			{
				PushBuilderFormated(&Builder, " -> ");
				WriteFunctionReturnType(&Builder, Type->Function.Returns);
			}
			if(Type->Function.Flags & SymbolFlag_Intrinsic)
				Builder += " #intrinsic";
			if(Type->Function.Flags & SymbolFlag_VarFunc)
				Builder += " #var_arg";
			if(Type->Function.Flags & SymbolFlag_Foreign)
				Builder += " #foreign";
			if(Type->Function.Flags & SymbolFlag_Inline)
				Builder += " #inline";

			return MakeString(Builder);
		} break;
		case TypeKind_Generic:
		{
			string_builder Builder = MakeBuilder();
			PushBuilderFormated(&Builder, "$%s.%d", Type->Generic.Name.Data, Type->Generic.ID);
			return MakeString(Builder);
		} break;
	}
	unreachable;
}

const char *GetTypeName(u32 TypeIdx)
{
	const type *Type = GetType(TypeIdx);
	return GetTypeName(Type);
}

const char *GetTypeName(const type *Type)
{
	return GetTypeNameAsString(Type).Data;
}

u32 GetPointerTo(u32 TypeIdx, u32 Flags)
{
	if(TypeIdx != INVALID_TYPE)
	{
		if(Flags == 0)
		{
			const type *T = GetType(TypeIdx);
			if(T->CachedAsPointer != INVALID_TYPE)
				return T->CachedAsPointer;
		}

		scratch_arena Scratch = {};

		string_builder Builder = MakeBuilder();
		string BaseType = GetTypeNameAsString(TypeIdx);
		if(Flags & PointerFlag_Optional)
			Builder += '?';
		Builder += "*";
		Builder += BaseType;
		string Lookup = MakeString(Builder, Scratch.Allocate(Builder.Size+1));

		TypeMutex.lock();
		u32 TLookUp = TypeMap[Lookup];
		if(TLookUp != INVALID_TYPE)
		{
			if(Flags == 0)
			{
				type *T = TypeTable[TypeIdx];
				T->CachedAsPointer = TLookUp;
			}
			TypeMutex.unlock();
			return TLookUp;
		}
		TypeMutex.unlock();
	}
	else
	{
		TypeMutex.lock();
		u32 T = INVALID_TYPE;
		if(Flags & PointerFlag_Optional)
			T = TypeMap[STR_LIT("?*")];
		else
			T = TypeMap[STR_LIT("*")];
		TypeMutex.unlock();

		if(T != INVALID_TYPE)
			return T;
	}

	type *New = AllocType(TypeKind_Pointer);
	New->Pointer.Pointed = TypeIdx;
	New->Pointer.Flags = Flags;
	u32 Result = AddType(New);

	if(TypeIdx != INVALID_TYPE && Flags == 0)
	{
		TypeMutex.lock();

		type *T = TypeTable[TypeIdx];
		T->CachedAsPointer = Result;

		TypeMutex.unlock();
	}

	return Result;
}

u32 GetSliceType(u32 Type)
{
	scratch_arena Scratch = {};

	string_builder Builder = MakeBuilder();
	string BaseType = GetTypeNameAsString(Type);
	Builder += "[]";
	Builder += BaseType;

	string Lookup = MakeString(Builder, Scratch.Allocate(Builder.Size+1));

	TypeMutex.lock();
	u32 T = TypeMap[Lookup];
	TypeMutex.unlock();
	if(T != INVALID_TYPE)
		return T;

	type *SliceType = AllocType(TypeKind_Slice);
	SliceType->Slice.Type = Type;

	return AddType(SliceType);
}

u32 GetArrayType(u32 Type, u32 ElemCount)
{
	scratch_arena Scratch = {};

	string_builder Builder = MakeBuilder();
	string BaseType = GetTypeNameAsString(Type);
	PushBuilderFormated(&Builder, "[%d]%s", ElemCount, BaseType);

	string Lookup = MakeString(Builder, Scratch.Allocate(Builder.Size+1));

	TypeMutex.lock();
	u32 T = TypeMap[Lookup];
	TypeMutex.unlock();
	if(T != INVALID_TYPE)
		return T;

	type *ArrayType = AllocType(TypeKind_Array);
	ArrayType->Array.Type = Type;
	ArrayType->Array.MemberCount = ElemCount;

	return AddType(ArrayType);
}

u32 GetOptional(const type *Pointer)
{
	Assert(Pointer);
	scratch_arena Scratch = {};

	if(Pointer->Pointer.Pointed != INVALID_TYPE)
	{
		string_builder Builder = MakeBuilder();
		string BaseType = GetTypeNameAsString(Pointer->Pointer.Pointed);
		Builder += "?*";
		Builder += BaseType;

		string Lookup = MakeString(Builder, Scratch.Allocate(Builder.Size+1));

		TypeMutex.lock();
		u32 T = TypeMap[Lookup];
		TypeMutex.unlock();
		if(T != INVALID_TYPE)
			return T;
	}
	else
	{
		TypeMutex.lock();
		u32 T = TypeMap[STR_LIT("?*")];
		TypeMutex.unlock();
		if(T != INVALID_TYPE)
			return T;
	}

	type *New = NewType(type);
	*New = *Pointer;
	New->Pointer.Flags |= PointerFlag_Optional;
	return AddType(New);
}

u32 GetNonOptional(const type *OptionalPointer)
{
	Assert(OptionalPointer);
	scratch_arena Scratch = {};

	if(OptionalPointer->Pointer.Pointed != INVALID_TYPE)
	{
		string_builder Builder = MakeBuilder();
		string BaseType = GetTypeNameAsString(OptionalPointer->Pointer.Pointed);
		Builder += "*";
		Builder += BaseType;

		string Lookup = MakeString(Builder, Scratch.Allocate(Builder.Size+1));

		TypeMutex.lock();
		u32 T = TypeMap[Lookup];
		TypeMutex.unlock();
		if(T != INVALID_TYPE)
			return T;
	}
	else
	{
		TypeMutex.lock();
		u32 T = TypeMap[STR_LIT("*")];
		TypeMutex.unlock();
		if(T != INVALID_TYPE)
			return T;
	}

	type *New = NewType(type);
	*New = *OptionalPointer;
	New->Pointer.Flags = New->Pointer.Flags & ~PointerFlag_Optional;
	return AddType(New);
}

b32 IsRetTypePassInPointer(u32 Type)
{
	if(Type == INVALID_TYPE)
		return false;

	const type *RetType = GetType(Type);
	if(RetType->Kind == TypeKind_Slice)
		return true;

	b32 IsComplex = (!IsLoadableType(RetType) && RetType->Kind != TypeKind_Function);
	if(!IsComplex)
		return false;
	int Size = GetTypeSize(RetType);
	switch(Size)
	{
		case 8:
		case 4:
		case 2:
		case 1:
		return false;
		default:
		return true;
	}
}

b32 IsPassInAsIntType(const type *Type)
{
	if(Type->Kind != TypeKind_Struct)
		return false;
	int Size = GetTypeSize(Type);
	switch(Size)
	{
		case 8:
		case 4:
		case 2:
		case 1:
		return true;
		default:
		return false;
	}
}

u32 ToNonGeneric(u32 TypeID, u32 Resolve, u32 ArgResolve)
{
	const type *Type = GetType(TypeID);
	const type *AR = GetType(ArgResolve);
	if(Type->Kind != TypeKind_Generic)
		Assert(Type->Kind == AR->Kind);
	u32 Result = TypeID;
	switch(Type->Kind)
	{
		case TypeKind_Enum:
		case TypeKind_Vector:
		case TypeKind_Basic:
		{
			return TypeID;
		} break;
		case TypeKind_Pointer:
		{
			if(Type->Pointer.Pointed == INVALID_TYPE)
				break;
			u32 Pointed = ToNonGeneric(Type->Pointer.Pointed, Resolve, AR->Pointer.Pointed);
			if(Pointed != Type->Pointer.Pointed)
			{
				Result = GetPointerTo(Pointed, Type->Pointer.Flags);
			}
		} break;
		case TypeKind_Array:
		{
			u32 AT = ToNonGeneric(Type->Array.Type, Resolve, AR->Array.Type);
			if(AT != Type->Array.Type)
			{
				Result = GetArrayType(AT, Type->Array.MemberCount);
			}
		} break;
		case TypeKind_Slice:
		{
			u32 AT = ToNonGeneric(Type->Slice.Type, Resolve, AR->Array.Type);
			if(AT != Type->Slice.Type)
			{
				Result = GetSliceType(AT);
			}
		} break;
		case TypeKind_Struct:
		{
			Result = Resolve;
		} break;
		case TypeKind_Function:
		{
			uint ArgCount = Type->Function.ArgCount;
			u32 *NArgs = (u32 *)AllocatePermanent(sizeof(u32) * ArgCount);
			b32 NeedsNew = false;
			for(int i = 0; i < ArgCount; ++i)
			{
				NArgs[i] = ToNonGeneric(Type->Function.Args[i], Resolve, AR->Function.Args[i]);
				if(NArgs[i] != Type->Function.Args[i])
					NeedsNew = true;
			}

			ForArray(Idx, Type->Function.Returns)
			{
				u32 RetTypeIdx = ToNonGeneric(Type->Function.Returns[Idx], Resolve, AR->Function.Returns[Idx]);
				if(RetTypeIdx != Type->Function.Returns[Idx])
				{
					NeedsNew = true;
					break;
				}
			}

			if(NeedsNew)
			{
				array<u32> Returns = Type->Function.Returns.Count;
				u32 At = 0;
				For(Type->Function.Returns)
				{
					Returns[At] = ToNonGeneric(*it, Resolve, AR->Function.Returns[At]);
					At++;
				}

				type *NT = AllocType(TypeKind_Function);
				*NT = *Type;
				NT->Function.Args = NArgs;
				NT->Function.Returns = SliceFromArray(Returns);
				NT->Function.Flags = Type->Function.Flags & ~SymbolFlag_Generic;
				Result = AddType(NT);
			}
		} break;
		case TypeKind_Generic:
		{
			Result = Resolve;
		} break;
		case TypeKind_Invalid: unreachable;
	}
	return Result;
}

b32 IsGeneric(u32 Type)
{
	if(Type == INVALID_TYPE)
		return false;

	return IsGeneric(GetTypeRaw(Type));
}

u32 GetGenericPart(u32 Resolved, u32 GenericID)
{
	u32 Result = INVALID_TYPE;
	const type *T = GetType(Resolved);
	const type *G = GetTypeRaw(GenericID);
	if(G->Kind != TypeKind_Generic && T->Kind != G->Kind)
		return INVALID_TYPE;

	switch(G->Kind)
	{
		case TypeKind_Enum:
		case TypeKind_Vector:
		case TypeKind_Basic:
		{
			unreachable;
		} break;
		case TypeKind_Pointer:
		{
			Result = GetGenericPart(T->Pointer.Pointed, G->Pointer.Pointed);
		} break;
		case TypeKind_Slice:
		{
			Result = GetGenericPart(T->Slice.Type, G->Slice.Type);
		} break;
		case TypeKind_Array:
		{
			Result = GetGenericPart(T->Array.Type, G->Array.Type);
		} break;
		case TypeKind_Struct:
		{
			if(G->Struct.Flags & StructFlag_Generic)
			{
				Result = Resolved;
			}
			else
			{
				ForArray(Idx, G->Struct.Members)
				{
					if(IsGeneric(G->Struct.Members[Idx].Type))
					{
						Result = GetGenericPart(T->Struct.Members[Idx].Type, G->Struct.Members[Idx].Type);
						break;
					}
				}
			}
		} break;
		case TypeKind_Function:
		{
			uint ArgCount = G->Function.ArgCount;
			for(int i = 0; i < ArgCount; ++i)
			{
				if(IsGeneric(G->Function.Args[i]))
				{
					Result = GetGenericPart(T->Function.Args[i], G->Function.Args[i]);
					break;
				}
			}
			if(Result == INVALID_TYPE)
			{
				ForArray(Idx, T->Function.Returns)
				{
					if(IsGeneric(G->Function.Returns[Idx]))
					{
						Result = GetGenericPart(T->Function.Returns[Idx], G->Function.Returns[Idx]);
						break;
					}
				}
			}
		} break;
		case TypeKind_Generic:
		{
			Result = Resolved;
		} break;
		case TypeKind_Invalid: unreachable;
	}
	return Result;
}

b32 IsGeneric(const type *Type)
{
	b32 Result = false;
	switch(Type->Kind)
	{
		case TypeKind_Enum:
		case TypeKind_Vector:
		case TypeKind_Basic:
		{
			Result = false;
		} break;
		case TypeKind_Pointer:
		{
			if(Type->Pointer.Pointed == INVALID_TYPE)
				return false;
			Result = IsGeneric(Type->Pointer.Pointed);
		} break;
		case TypeKind_Array:
		{
			Result = IsGeneric(Type->Array.Type);
		} break;
		case TypeKind_Slice:
		{
			Result = IsGeneric(Type->Slice.Type);
		} break;
		case TypeKind_Struct:
		{
			Result = (Type->Struct.Flags & StructFlag_Generic) != 0;
		} break;
		case TypeKind_Function:
		{
			uint ArgCount = Type->Function.ArgCount;
			for(int i = 0; i < ArgCount; ++i)
			{
				if(IsGeneric(Type->Function.Args[i]))
				{
					Result = true;
					break;
				}
			}
			if(!Result)
			{
				ForArray(Idx, Type->Function.Returns)
				{
					if(IsGeneric(Type->Function.Returns[Idx]))
					{
						Result = true;
						break;
					}
				}
			}
		} break;
		case TypeKind_Generic:
		{
			Result = true;
		} break;
		case TypeKind_Invalid: unreachable;

	}
	return Result;
}

type *AllocType(type_kind Kind)
{
	type *T = NewType(type);
	T->Kind = Kind;
	return T;
}

u32 MakeGeneric(scope *Scope, string Name)
{
	static size_t Counter = 0;
	type *T = AllocType(TypeKind_Generic);
	T->Generic.ID = Counter++;//Scope->LastGeneric++;
	T->Generic.Name = Name;
	T->Generic.Scope = Scope;
	return AddType(T);
}

u32 ComplexTypeToSizeType(const type *T)
{
	int Size = GetTypeSize(T);
	switch(Size)
	{
		case 8:
		return Basic_u64;
		case 4:
		return Basic_u32;
		case 2:
		return Basic_u16;
		case 1:
		return Basic_u8;
		default: unreachable;
	}
}

u32 AllFloatsStructToReturnType(const type *T)
{
	if(PTarget == platform_target::Windows)
		return ComplexTypeToSizeType(T);

	if(T->Struct.Members.Count == 1)
	{
		u32 Mem1 = T->Struct.Members[0].Type;
		return Mem1;
	}

	if(T->Struct.Members.Count == 2)
	{
		u32 Mem1 = T->Struct.Members[0].Type;
		u32 Mem2 = T->Struct.Members[1].Type;
		const type *M1T = GetType(Mem1);
		if(Mem1 == Mem2 && M1T->Basic.Kind == Basic_f32)
		{
			type *Type = AllocType(TypeKind_Vector);
			Type->Vector.Kind = Vector_Float;
			Type->Vector.ElementCount = 2;
			return AddType(Type);
		}
		else
		{
			dynamic<struct_member> Members = {};
			Members.Push((struct_member){.ID = STR_LIT("!_mem1"), .Type = Mem1});
			Members.Push((struct_member){.ID = STR_LIT("!_mem2"), .Type = Mem2});
			type *Type = AllocType(TypeKind_Struct);
			Type->Struct.Name = STR_LIT("!_return_struct");
			Type->Struct.Members = SliceFromArray(Members);

			return AddType(Type);
		}
	}

	Assert(false);
	return INVALID_TYPE;
}

u32 ComplexTypeToSizeType(u32 Complex)
{
	const type *T = GetType(Complex);
	return ComplexTypeToSizeType(T);
}

const type *OneIsXAndTheOtherY(const type *L, const type *R, type_kind X, type_kind Y)
{
	if(L->Kind == X && R->Kind == Y)
		return L;
	if(R->Kind == X && L->Kind == Y)
		return R;

	return NULL;
}

b32 IsStructAllFloats(const type *T)
{
	b32 AllFloats = true;
	ForArray(Idx, T->Struct.Members)
	{
		u32 TypeIdx = T->Struct.Members[Idx].Type;
		const type *MemberType = GetType(TypeIdx);
		if(!HasBasicFlag(MemberType, BasicFlag_Float))
		{
			AllFloats = false;
			break;
		}
	}
	return AllFloats;
}

b32 VerifyStructMemberNoRecursion(u32 OgType, const type *T, int *FailedIdx)
{
	Assert(T->Kind == TypeKind_Struct);
	ForArray(Idx, T->Struct.Members)
	{
		auto it = T->Struct.Members[Idx];
		if(it.Type == OgType)
		{
			*FailedIdx = Idx;
			return false;
		}

		const type *MemT = GetType(it.Type);
		if(MemT->Kind == TypeKind_Struct)
			if (!VerifyStructMemberNoRecursion(OgType, MemT, FailedIdx))
			{
				*FailedIdx = Idx;
				return false;
			}
	}
	return true;
}

b32 VerifyNoStructRecursion(u32 TIdx, int *FailedIdx)
{
	const type *T = GetType(TIdx);
	Assert(T->Kind == TypeKind_Struct);

	return VerifyStructMemberNoRecursion(TIdx, T, FailedIdx);
}

void FillOpaqueEnum(string Name, slice<enum_member> Members, u32 Type, u32 Original)
{
	TypeMutex.lock();
	type T = {};
	T.Kind = TypeKind_Enum;
	T.Enum.Name = Name;
	T.Enum.Members = Members;
	T.Enum.Type = Type;
	T.CachedAsPointer	= INVALID_TYPE;
	T.CachedAlignment	= -1;
	T.CachedSize		= -1;

	*TypeTable[Original] = T;
	TypeMutex.unlock();
}

b32 IsTypeMatchable(const type *T)
{
	if(T->Kind == TypeKind_Basic)
	{
		return HasBasicFlag(T, BasicFlag_Integer) || HasBasicFlag(T, BasicFlag_Float);
	}

	return T->Kind == TypeKind_Enum;
}

b32 IsTypeMultiReturn(const type *T)
{
	return T->Kind == TypeKind_Struct && ((T->Struct.Flags & StructFlag_FnReturn) != 0);
}

b32 IsTypeIterable(const type *T)
{
	return T->Kind == TypeKind_Array || T->Kind == TypeKind_Slice ||
		HasBasicFlag(T, BasicFlag_Integer) || 
		HasBasicFlag(T, BasicFlag_String);
}

u32 UntypedGetType(u32 TIdx)
{
	return UntypedGetType(GetType(TIdx));
}

u32 UntypedGetType(const type *T)
{
	Assert(IsUntyped(T));
	if(HasBasicFlag(T, BasicFlag_Integer))
	{
		return Basic_int;
	}
	else if(HasBasicFlag(T, BasicFlag_Float))
	{
		return Basic_f32;
	}
	else
	{
		unreachable;
	}
}

b32 IsForeign(const type *T)
{
	Assert(T->Kind == TypeKind_Function);
	return (T->Function.Flags & SymbolFlag_Foreign) != 0;
}

string GetGenericResolvedStructName(const type *T)
{
	Assert(T->Kind == TypeKind_Struct);
	string_builder b = MakeBuilder();
	b += T->Struct.Name;
	b += '<';

	bool First = true;
	For(T->Struct.Members)
	{
		if(First)
			First = false;
		else
			b += '.';
		b += GetTypeNameAsString(it->Type);
	}

	b += '>';

	return MakeString(b);
}

u32 MakeStruct(slice<struct_member> Members, string Name, u32 Flags)
{
	type *T = AllocType(TypeKind_Struct);
	T->Struct.Members = Members;
	T->Struct.Name    = Name;
	T->Struct.Flags   = Flags;

	T->Struct.Name = GetGenericResolvedStructName(T);

	return AddType(T);
}

