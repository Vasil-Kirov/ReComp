#include "ConstVal.h"
#include "Type.h"
#include <stdlib.h>

const_value MakeConstString(const string *String)
{
	const_value Result = {};
	Result.Type = const_type::String;
	Result.String.Data = String;

	return Result;
}

f64 FloatFromString(const string *String)
{
	char Buff[256] = {};
	int BuffCount = 0;
	for(int Idx = 0; Idx < String->Size; ++Idx)
	{
		if(String->Data[Idx] != '_')
			Buff[BuffCount++] = String->Data[Idx];
	}
	return strtof(Buff, NULL);
}

i64 IntFromString(const string *String)
{
	char Buff[256] = {};
	int BuffCount = 0;
	for(int Idx = 0; Idx < String->Size; ++Idx)
	{
		if(String->Data[Idx] != '_')
			Buff[BuffCount++] = String->Data[Idx];
	}
	return strtoll(Buff, NULL, 0);
}

u64 UnsignedIntFromString(const string *String)
{
	char Buff[256] = {};
	int BuffCount = 0;
	for(int Idx = 0; Idx < String->Size; ++Idx)
	{
		if(String->Data[Idx] != '_')
			Buff[BuffCount++] = String->Data[Idx];
	}
	return strtoull(Buff, NULL, 0);
}

const_integer MakeConstInteger(const string *String, b32 IsSigned)
{
	const_integer Result = {};
	Result.IsSigned = IsSigned;
	if(IsSigned)
	{
		Result.Signed = IntFromString(String);
	}
	else
	{
		Result.Unsigned = UnsignedIntFromString(String);
	}

	return Result;
}

const_value MakeConstValue(const string *String)
{
	const_value Result = {};
	b32 IsFloat = false;
	int DigitCount = 0;
	for(int Idx = 0; Idx < String->Size; ++Idx)
	{
		if(String->Data[Idx] == '.')
		{
			IsFloat = true;
		}
		else if(isdigit(String->Data[Idx]))
		{
			DigitCount++;
		}
	}
	if(IsFloat)
	{
		Result.Type = const_type::Float;
		Result.Float = FloatFromString(String);
	}
	else
	{
		b32 IsSigned = String->Data[0] == '-' || DigitCount < 19;
		Result.Type = const_type::Integer;
		Result.Int = MakeConstInteger(String, IsSigned);
	}

	return Result;
}

u32 GetConstantType(const const_value &Value)
{
	using ct = const_type;
	switch(Value.Type)
	{
		case ct::Integer:
		{
			return Basic_UntypedInteger;
		} break;
		case ct::Float:
		{
			return Basic_UntypedFloat;
		} break;
		case ct::String:
		{
			if(Value.String.Flags & ConstString_CSTR)
				return Basic_cstring;
			else
				return Basic_string;
		} break;
	}
}

u32 GetConstantTypedType(const const_value *Value)
{
	using ct = const_type;
	switch(Value->Type)
	{
		case ct::Integer:
		{
			if(Value->Int.IsSigned)
			{
				return Basic_i64;
			}
			else
			{
				return Basic_u64;
			}
		} break;
		case ct::Float:
		{
			return Basic_f64;
		} break;
		case ct::String:
		{
			return Basic_string;
		} break;
	}
}

