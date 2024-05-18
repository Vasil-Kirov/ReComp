#include "LLVMFileCast.h"

void
LLVMCastWriteTrunc(string_builder *Builder, u32 Value, char Prefix, const type *From, const type *To)
{
	if(To->Basic.Flags & BasicFlag_Float)
	{
		PushBuilderFormated(Builder, "fptrunc %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
	}
	else
	{
		PushBuilderFormated(Builder, "trunc %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
	}
}

void
LLVMCastWriteExt(string_builder *Builder, u32 Value, char Prefix, const type *From, const type *To)
{
	// float
	if(To->Basic.Flags & BasicFlag_Float)
	{
		PushBuilderFormated(Builder, "fpext %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
	}
	// from is unsigned, zero ext
	else if(From->Basic.Flags & BasicFlag_Unsigned)
	{
		PushBuilderFormated(Builder, "zext %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
	}
	// from is signed, sign ext
	else
	{
		PushBuilderFormated(Builder, "sext %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
	}
}

// @NOTE: Assumes one is float and other isn't
void
LLVMCastWriteFloatInt(string_builder *Builder, u32 Value, char Prefix, const type *From, const type *To)
{
	if(From->Basic.Flags & BasicFlag_Float)
	{
		if(To->Basic.Flags & BasicFlag_Unsigned)
		{
			PushBuilderFormated(Builder, "fptoui %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
		}
		else
		{
			PushBuilderFormated(Builder, "fptosi %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
		}
	}
	else if(To->Basic.Flags & BasicFlag_Float)
	{
		if(From->Basic.Flags & BasicFlag_Unsigned)
		{
			PushBuilderFormated(Builder, "uitofp %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
		}
		else
		{
			PushBuilderFormated(Builder, "sitofp %s %c%d to %s", GetLLVMTypeChar(From), Prefix, Value, GetLLVMTypeChar(To));
		}
	}
	else
	{
		Assert(false);
	}
}

void
LLVMCast(string_builder *Builder, u32 Value, char Prefix, const type *From, const type *To)
{
	if(From->Kind == TypeKind_Basic && To->Kind == TypeKind_Basic)
	{
		int FromSize = GetTypeSize(From);
		int ToSize   = GetTypeSize(To);

		if((From->Basic.Flags & BasicFlag_Float) != (To->Basic.Flags & BasicFlag_Float))
		{
			LLVMCastWriteFloatInt(Builder, Value, Prefix, From, To);
		}
		else if(ToSize < FromSize)
		{
			LLVMCastWriteTrunc(Builder, Value, Prefix, From, To);
		}
		else if(ToSize > FromSize)
		{
			LLVMCastWriteExt(Builder, Value, Prefix, From, To);
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
}

