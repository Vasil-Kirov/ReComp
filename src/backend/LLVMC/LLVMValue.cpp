#include "LLVMValue.h"
#include "Basic.h"
#include "llvm-c/Types.h"
#include "Log.h"


void value_map::Add(u32 Register, LLVMValueRef Value)
{
	value_entry Entry;
	Entry.Value = Value;
	Entry.Register = Register;
	Data.Push(Entry);
}

LLVMValueRef value_map::Get(u32 Register)
{
	ForArray(Idx, Data)
	{
		if(Data[Idx].Register == Register)
		{
			return Data[Idx].Value;
		}
	}
	LERROR("%d", Register);
	Assert(false);
	return NULL;
}

void value_map::Clear()
{
	Data.Count = 0;
}
