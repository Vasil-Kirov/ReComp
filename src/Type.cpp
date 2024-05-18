#include "Type.h"
#include "String.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
const type BasicTypes[] = {
	{TypeKind_Basic, {Basic_bool,   BasicFlag_Boolean,                             4, STR_LIT("bool")}},
	{TypeKind_Basic, {Basic_string, BasicFlag_String,                             -1, STR_LIT("string")}},

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
};
#pragma clang diagnostic pop

static const int BasicTypesCount = (sizeof(BasicTypes) / sizeof(BasicTypes[0]));

const type *BasicBool      = &BasicTypes[Basic_bool];
const type *UntypedInteger = &BasicTypes[Basic_UntypedInteger];
const type *UntypedFloat   = &BasicTypes[Basic_UntypedFloat];
const type *BasicInt       = &BasicTypes[Basic_int];
const type *BasicUint      = &BasicTypes[Basic_uint];
const type *BasicF32       = &BasicTypes[Basic_f32];


u32 TypeCount = 0;
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
	return (Type->Kind & TypeKind_Basic) && (Type->Basic.Flags & BasicFlag_Untyped);
}
inline const type *GetType(u32 TypeIdx)
{
	return TypeTable[TypeIdx];
}

// @Note: Does this really need to lock and unlock mutex, it's like 2 instructions
// I guess there is a chance that the function gets called at the same time by 2 threads
u32 AddType(type *Type)
{
	LockMutex();

	TypeTable[TypeCount++] = Type;
	u32 Result = TypeCount - 1;

	UnlockMutex();
	return Result;
}

// @TODO: Non basic type size calculation and arch dependant type sizes
int GetBasicTypeSize(const type *Type)
{
	if(Type->Basic.Size != -1)
		return Type->Basic.Size;
	Assert(false);
}

int GetTypeSize(const type *Type)
{
	if(Type->Kind == TypeKind_Basic)
		return GetBasicTypeSize(Type);
	Assert(false);
}

b32 CheckMissmatch(int LeftFlags, int RightFlags, basic_flags Flag)
{
	if((LeftFlags & Flag) != (RightFlags & Flag))
		return true;
	return false;
}

b32 CheckBasicTypes(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment)
{
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
		if(IsAssignment)
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
			return TypesMustMatch(Left->Pointer.Pointed, Right->Pointer.Pointed);
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
			return TypesMustMatch(Left->Pointer.Pointed, Right->Pointer.Pointed);
		} break;
		default:
		{
			Assert(false);
			return false;
		} break;
	}


	return false;
}

const char *GetTypeName(const type *Type)
{
	switch (Type->Kind)
	{
		case TypeKind_Basic:
		{
			return Type->Basic.Name.Data;
		} break;
		default:
		{
			return "Error! Unkown type name";
		} break;
	}
}

