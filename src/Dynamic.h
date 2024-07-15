#pragma once
#include "Basic.h"

// Dynamic array that doesn't need / use constructors and destructors
#include <cstddef>
#include <initializer_list>
template <typename T>
struct dynamic {
	T *Data;
	size_t Count;
	size_t Capacity;
	const short INIT_CAPACITY = 8;
	void EnsureCapacity()
	{
		if(!Data)
		{
			Data = (T *)VAlloc(sizeof(T) * INIT_CAPACITY);
			Capacity = INIT_CAPACITY;
		}
		while(Count >= Capacity)
		{
			Capacity = (Capacity + 2) * 1.5;
			T *NewData = (T *)VAlloc(sizeof(T) * Capacity);
			memcpy(NewData, Data, sizeof(T) * Count);
			VFree(Data);
			Data = NewData;
		}
	}
	T operator[](size_t Index) const
	{
		Assert(Index < Count);
		return Data[Index];
	}
	void Push(T Value)
	{
		EnsureCapacity();
		Data[Count++] = Value;
	}
	b32 IsValid() { return Data != NULL; }
};

template <typename T>
struct slice {
	T *Data;
	size_t Count;

	T operator[](size_t Index) const
	{
		Assert(Index < Count);
		return Data[Index];
	}
	b32 IsValid() { return Data != NULL; }
};

template <typename T>
slice<T> SliceFromArray(dynamic<T> Array)
{
	return {Array.Data, Array.Count};
}

template <typename T>
slice<T> ZeroSlice()
{
	return {NULL, 0};
}

template <typename T>
slice<T> SliceFromConst(std::initializer_list<T> List)
{
	size_t Size = List.size();
	T *Data = (T *)VAlloc(Size * sizeof(T));
	memcpy(Data, List.begin(), Size * sizeof(T));
	slice<T> Result;
	Result.Data = Data;
	Result.Count = Size;
	return Result;
}

#define ForArray(_Index, _Array) for(int _Index = 0; _Index < (_Array).Count; ++_Index)

