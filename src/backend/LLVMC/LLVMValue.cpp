#include "LLVMValue.h"
#include "Basic.h"
#include "llvm-c/Types.h"
#include "Log.h"

void value_map::Add(u32 Register, LLVMValueRef Value)
{
	Data.Add(Register, Value);
}

LLVMValueRef value_map::Get(u32 Register)
{
	auto r = Data[Register];
	if(!r)
	{
		LERROR("%d", Register);
		Assert(false);
	}
	return r;
}

void value_map::Clear()
{
	Data.Free();
	Data = {};
}
