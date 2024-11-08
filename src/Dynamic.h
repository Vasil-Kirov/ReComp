#pragma once
#include "Basic.h"
#include "Log.h"

// Dynamic array that doesn't need / use constructors and destructors
#include <initializer_list>

// Putting this in the class deletes the default copy constructor
const short INIT_CAPACITY = 8;

template <typename T>
struct dynamic {
	T *Data;
	size_t Count;
	size_t Capacity;
	void EnsureCapacity()
	{
		if(!Data)
		{
			Data = (T *)VAlloc(sizeof(T) * INIT_CAPACITY);
			Capacity = INIT_CAPACITY;
		}
		while(Count >= Capacity)
		{
			size_t NewCapacity = (Capacity + 2) * 1.5;
			Data = (T *)VRealloc(Data, Capacity * sizeof(T), NewCapacity * sizeof(T));
			Capacity = NewCapacity;
		}
	}
	T *GetPtr(size_t Index) const
	{
		Assert(Index < Count);
		return Data + Index;
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
	void Free()
	{
		VFree(Data);
		Data = NULL;
		Count = 0;
		Capacity = 0;
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
struct array {
	T *Data;
	size_t Count;
	array(size_t _Count)
	{
		Count = _Count;
		Data = (T *)VAlloc(Count * sizeof(T));
	}
	array(void *Mem, size_t _Count)
	{
		Count = _Count;
		Data = (T *)Mem;
	}
	T operator[](size_t Index) const
	{
		Assert(Index < Count);
		return Data[Index];
	}
	T& operator[](size_t Index)
	{
		Assert(Index < Count);
		return Data[Index];
	}
};

template <typename T>
slice<T> SliceFromArray(array<T> Array)
{
	return {Array.Data, Array.Count};
}

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

