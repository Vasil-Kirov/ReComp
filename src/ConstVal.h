#pragma once
#include "String.h"

enum class const_type
{
	Integer,
	Float,
	String,
};

struct const_integer
{
	b32 IsSigned;
	union {
		i64 Signed;
		u64 Unsigned;
	};
};

struct const_value
{
	const_type Type;
	union {
		const_integer Int;
		f64 Float;
		const string *String;
	};
};


const_value MakeConstString(const string *String);
const_value MakeConstValue(const string *String);
u32 GetConstantType(const const_value &Value);
