#pragma once
#include "Basic.h"
#include "Log.h"
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
	T *GetLast() const
	{
		Assert(Count != 0);
		return &Data[Count-1];
	};
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
	ssize_t Find(T Value)
	{
		for(size_t Idx = 0; Idx < Count; ++Idx)
		{
			if(Data[Idx] == Value)
				return (ssize_t)Idx;
		}

		return -1;
	}
	void Clear()
	{
		Count = 0;
	}
	void Push(T Value)
	{
		EnsureCapacity();
		Data[Count++] = Value;
	}
	void Free()
	{
		if(Data)
		{
			VFree(Data);
		}
		Data = NULL;
		Count = 0;
		Capacity = 0;
	}
	b32 IsValid() const { return Data != NULL; }
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
	b32 IsValid() const { return Data != NULL; }
	T Last()
	{
		if(Count == 0)
			return NULL;
		return Data[Count-1];
	}
};

typedef int (*sort_fn)(void *, const void*, const void*);
template <typename T>
struct array {
	T *Data;
	size_t Count;
	array() = default;
	array(size_t Count_)
	{
		Count = Count_;
		Data = (T *)VAlloc(Count * sizeof(T));
	}
	array(void *Mem, size_t Count_)
	{
		Count = Count_;
		Data = (T *)Mem;
	}
	array(dynamic<T> Dynamic)
	{
		Data = Dynamic.Data;
		Count = Dynamic.Count;
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
	void Sort(sort_fn Fn, void *Ctx)
	{
		qsort_s(Data, Count, sizeof(T), Fn, Ctx);
	}
	void Reverse()
	{
		if(Count < 2)
			return;

		for(size_t i = Count-1; i >= Count/2; --i)
		{
			T tmp = Data[Count-i-1];
			Data[Count-i-1] = Data[i];
			Data[i] = tmp;
		}
	}
	void Free()
	{
		if(Data)
			VFree(Data);
		Data = NULL;
		Count = 0;
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

template <typename T>
array<T> ArrayFromConst(std::initializer_list<T> List)
{
	size_t Size = List.size();
	T *Data = (T *)VAlloc(Size * sizeof(T));
	memcpy(Data, List.begin(), Size * sizeof(T));
	array<T> Result;
	Result.Data = Data;
	Result.Count = Size;
	return Result;
}

#define ForArray(_Index, _Array) for(size_t _Index = 0; _Index < (_Array).Count; ++_Index)
#define For(_Array) for(auto *it = (_Array).Data; (size_t)(it - (_Array).Data) < (_Array).Count; ++it)
#define ForN(_Array, Name) for(auto *Name = (_Array).Data; (size_t)(Name - (_Array).Data) < (_Array).Count; ++Name)
#define ForReverse(_Array) for(auto *it = (_Array).Data+(_Array).Count-1; it >= _Array.Data; --it)

