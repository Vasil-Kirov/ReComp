#pragma once
#include "Dynamic.h"


template <typename T>
struct stack
{
	dynamic<T> Data;

	void Push(T Val)
	{
		Data.Push(Val);
	}

	T Pop()
	{
		Assert(Data.Count > 0);
		T Result = Data[Data.Count - 1];
		Data.Count--;
		return Result;
	}

	b32 IsEmpty() { return Data.Count == 0; }
};

