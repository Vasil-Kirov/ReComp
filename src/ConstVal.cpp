#include "ConstVal.h"
#include "Memory.h"
#include "Type.h"
#include <cmath>
#include <stdlib.h>
#include <Interpreter.h>

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
	return strtoll(String->Data, NULL, 0);
}

u64 UnsignedIntFromString(const string *String)
{
	return strtoull(String->Data, NULL, 0);
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
	enum {
		Parse_Int,
		Parse_Float,
		Parse_Binary,
		Parse_Hex,
	} ParseType = Parse_Int;

	const_value Result = {};
	int DigitCount = 0;
	if(String->Size > 1 && String->Data[0] == '0' && String->Data[1] == 'b')
	{
		ParseType = Parse_Binary;
	}
	else if(String->Size > 1 && String->Data[0] == '0' && String->Data[1] == 'x')
	{
		ParseType = Parse_Hex;
	}
	else
	{
		for(int Idx = 0; Idx < String->Size; ++Idx)
		{
			if(String->Data[Idx] == '.')
			{
				ParseType = Parse_Float;
			}
			else if(isdigit(String->Data[Idx]))
			{
				DigitCount++;
			}
		}
	}
	switch(ParseType)
	{
		case Parse_Float:
		{
			Result.Type = const_type::Float;
			Result.Float = FloatFromString(String);
		} break;
		case Parse_Int:
		{
			b32 IsSigned = String->Data[0] == '-' || DigitCount < 19;
			Result.Type = const_type::Integer;
			Result.Int = MakeConstInteger(String, IsSigned);
		} break;
		case Parse_Binary:
		{
			Result.Type = const_type::Integer;
			Result.Int.IsSigned = false;
			Result.Int.Unsigned = 0;
			for(int i = 2; i < String->Size; ++i)
			{
				Result.Int.Unsigned |= (String->Data[i] - '0');
				if(i + 1 != String->Size)
					Result.Int.Unsigned <<= 1;
			}
		} break;
		case Parse_Hex:
		{
			Result.Type = const_type::Integer;
			Result.Int.IsSigned = false;
			Result.Int.Unsigned = 0;
			int Position = 0;
			for(int i = String->Size-1; i >= 2; --i)
			{
				char c = String->Data[i];
				int val = 0;
				if(isalpha(c))
				{
					val = (tolower(c) - 'a') + 10;
				}
				else
				{
					val = c - '0';
				}

				u64 mul = (u64)pow(16, Position++);
				Result.Int.Unsigned += (u64)val * mul;
			}
		} break;
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
				return GetPointerTo(Basic_u8);
			else
				return Basic_string;
		} break;
		default: unreachable;
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
		default: unreachable;
	}
}

uint GetCodepointSize(const char *ptr)
{
	static const u8 CONTINUE = 0b10000000;
	if((*ptr & CONTINUE) == 0)
		return 1;

	int size = 1;
	for(int i = 1; *ptr & (CONTINUE >> i); ++i)
	{
		size++;
	}

	return size;
}

size_t GetUTF8Count(const string *String)
{
	size_t Count = 0;
	for(size_t i = 0; i < String->Size;)
	{
		i += GetCodepointSize(&String->Data[i]);
		Count++;
	}
	return Count;
}

const_value FromInterp(value &Value)
{
	const type *T = GetType(Value.Type);
	if(T->Kind == TypeKind_Enum)
		T = GetType(T->Enum.Type);
	const_value V = {};
	switch(T->Kind)
	{
		case TypeKind_Invalid:
		case TypeKind_Enum:
		case TypeKind_Generic:
		case TypeKind_Function:
		unreachable;
		case TypeKind_Basic:
		{
			if(HasBasicFlag(T, BasicFlag_Integer | BasicFlag_TypeID))
			{
				V.Type = const_type::Integer;
				if(HasBasicFlag(T, BasicFlag_Unsigned))
				{
					V.Int.IsSigned = false;
					V.Int.Unsigned = Value.u64;
				}
				else
				{
					V.Int.IsSigned = true;
					V.Int.Signed = Value.i64;
				}
			}
			else if(HasBasicFlag(T, BasicFlag_Float))
			{
				V.Type = const_type::Float;
				if(T->Basic.Kind == Basic_f32)
				{
					V.Float = (f64)Value.f32;
				}
				else
				{
					V.Float = Value.f64;
				}
			}
			else if(IsString(T))
			{
				string TheString = {};
				TheString.Size = *(size_t *)Value.ptr;
				TheString.Data = *((const char **)Value.ptr+1);
				V.Type = const_type::String;
				V.String.Data = DupeType(TheString, string);
			}
			else
			{
				unreachable;
			}
		} break;
		case TypeKind_Pointer:
		{
			if(IsCString(T))
			{
				string CString = { .Data = (const char *)Value.ptr, .Size = 0 };
				V.Type = const_type::String;
				V.String.Data = DupeType(CString, string);
			}
			else
			{
				V.Type = const_type::Integer;
				V.Int.Unsigned = (u64)Value.ptr;
			}
		} break;
		case TypeKind_Vector:
		{
			V.Type = const_type::Vector;
			switch(T->Vector.Kind)
			{
				case Vector_Int:
				{
					V.Vector.I = Value.ivec;
				} break;
				case Vector_Float:
				{
					V.Vector.F = Value.fvec;
				} break;
			}
		} break;
		case TypeKind_Array:
		case TypeKind_Slice:
		case TypeKind_Struct:
		{
			V.Type = const_type::Aggr;
			V.Struct.Ptr = Value.ptr;
		} break;
	}
	return V;
}

