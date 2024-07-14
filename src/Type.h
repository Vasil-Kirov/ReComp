#pragma once
#include "Basic.h"
#include "VString.h"
#include "Dynamic.h"

// @Note: heavily inspired by the Odin type system

struct type;
#define INVALID_TYPE UINT32_MAX


enum type_kind
{
	TypeKind_Invalid,

	TypeKind_Basic,
	TypeKind_Function,
	TypeKind_Struct,
	TypeKind_Pointer,
};

enum basic_kind
{
	Basic_bool,
	Basic_string,

	Basic_u8,
	Basic_u16,
	Basic_u32,
	Basic_u64,

	Basic_i8,
	Basic_i16,
	Basic_i32,
	Basic_i64,

	Basic_f32,
	Basic_f64,

	Basic_UntypedInteger,
	Basic_UntypedFloat,

	Basic_int,
	Basic_uint,

	Basic_type,
	Basic_auto,
};

enum basic_flags
{
	BasicFlag_Boolean = BIT(0),
	BasicFlag_Integer = BIT(1),
	BasicFlag_Float   = BIT(2),
	BasicFlag_String  = BIT(3),
	BasicFlag_Untyped = BIT(4),
	BasicFlag_Unsigned= BIT(5),
	BasicFlag_TypeID  = BIT(6),

	BasicFlag_Numeric = BasicFlag_Integer | BasicFlag_Float,
};

enum struct_flags
{
	StructFlag_Packed = BIT(0),
};

struct basic_type
{
	basic_kind Kind;
	int Flags;
	int Size;  // -1 if it's arch dependant
	string Name;
};

struct struct_member
{
	string ID;
	u32 Type;
};

struct struct_type
{
	slice<struct_member> Members;
	string Name;
	u32 Flags;
};

struct function_type
{
	u32 Return;
	u32 *Args;
	int ArgCount;
};

struct pointer
{
	u32 Pointed;
};

struct type
{
	type_kind Kind;
	union
	{
		basic_type Basic;
		struct_type Struct;
		function_type Function;
		pointer Pointer;
	};
};

u32 AddType(type *Type);

// @Note: I don't know if I need this but I'm wondering if later on accessing a global variable could
// have some problems with threading, shouldn't hurt to have it for now
const type *GetType(u32 TypeIdx);

const char *GetTypeName(const type *Type);

u32 GetReturnType(const type *Type);

int GetBasicTypeSize(const type *Type);
int GetTypeSize(const type *Type);
int GetRegisterTypeSize();

b32 IsTypeCompatible(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment);
b32 TypesMustMatch(const type *Left, const type *Right);
b32 IsUntyped(const type *Type);
b32 IsCastValid(const type *From, const type *To);
b32 IsCallable(const type *Type);
b32 IsCastRedundant(const type *From, const type *To);

