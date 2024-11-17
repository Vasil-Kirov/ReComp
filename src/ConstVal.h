#pragma once
#include "Basic.h"
#include "VString.h"

enum class const_type
{
	Integer,
	Float,
	String,
};

enum const_string_flags
{
	ConstString_CSTR = BIT(1),
};

struct const_integer
{
	b32 IsSigned;
	union {
		i64 Signed;
		u64 Unsigned;
	};
};

struct const_string
{
	const string *Data;
	int Flags;
};

struct const_value
{
	const_type Type;
	union {
		const_integer Int;
		f64 Float;
		const_string String;
	};
};


const_value MakeConstString(const string *String);
size_t GetUTF8Count(const string *String);
const_value MakeConstValue(const string *String);
u32 GetConstantType(const const_value &Value);
u32 GetConstantTypedType(const const_value *Value);

