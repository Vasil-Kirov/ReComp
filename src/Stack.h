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

	T& Peek()
	{
		Assert(Data.Count > 0);
		T& Result = Data.Data[Data.Count - 1];
		return Result;
	}


	T Pop()
	{
		T Result = Peek();
		Data.Count--;
		return Result;
	}

	b32 IsEmpty() { return Data.Count == 0; }
};

