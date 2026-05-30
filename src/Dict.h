#pragma once
#include "VString.h"
#include "Dynamic.h"
#include <utility>

// @Note: Stolen from wikipedia implementations of murmur3
static inline uint32_t murmur_32_scramble(uint32_t k) {
	k *= 0xcc9e2d51;
	k = (k << 15) | (k >> 17);
	k *= 0x1b873593;
	return k;
}

static uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
{
	uint32_t h = seed;
	uint32_t k;
	/* Read in groups of 4. */
	for (size_t i = len >> 2; i; i--) {
		// Here is a source of differing results across endiannesses.
		// A swap here has no effects on hash properties though.
		memcpy(&k, key, sizeof(uint32_t));
		key += sizeof(uint32_t);
		h ^= murmur_32_scramble(k);
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
	}
	/* Read the rest. */
	k = 0;
	for (size_t i = len & 3; i; i--) {
		k <<= 8;
		k |= key[i - 1];
	}
	// A swap is *not* necessary here because the preceding loop already
	// places the low bytes in the low places according to whatever endianness
	// we use. Swaps only apply when the memory is copied in a chunk.
	h ^= murmur_32_scramble(k);
	/* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

struct key
{
	u32 Hash;
	string N;
};

const int SEED = 980346;

template <typename T>
struct dict {
	static constexpr int BucketCount = 32;
	dynamic<u8> Control = {BucketCount, BucketCount};
	dynamic<key> Keys = {BucketCount, BucketCount};
	dynamic<T> Data_ = {BucketCount, BucketCount};
	size_t Used = 0;
	T Default = T{};
	int Bottom = 0;

	dict(){}
	dict(T Default) : Default(Default) {}


    struct iterator
    {
        dict* Dict;
        size_t Idx;

        void SkipToValid()
        {
            size_t Capacity = Dict->Data_.Capacity;
			for(;Idx < Capacity && ((Dict->Control[Idx] & 1) == 0); ++Idx)
				;
        }

        std::pair<string, T&> operator*() const
        {
            return { Dict->Keys.Data[Idx].N, Dict->Data_.Data[Idx] };
        }

        iterator& operator++()
        {
            ++Idx;
            SkipToValid();
            return *this;
        }

        bool operator!=(const iterator& other) const
        {
            return Idx != other.Idx || Dict != other.Dict;
        }
    };

    iterator begin()
    {
        iterator it{ this, 0 };
        it.SkipToValid();
        return it;
    }


    iterator end()
    {
        return iterator{ this, Data_.Capacity };
    }

	void ReHash()
	{
		dynamic<u8> OldControl = Control;
		dynamic<key> OldKeys = Keys;
		dynamic<T> OldData = Data_;

		Control = {Control.Capacity*2, Control.Capacity*2};
		Data_ = {Data_.Capacity*2, Data_.Capacity*2};
		Keys = {Keys.Capacity*2, Keys.Capacity*2};
		Used = 0;
		for(size_t Idx = 0; Idx < OldKeys.Count; ++Idx)
			if(OldControl.Data[Idx] & 0b1)
				Add_(OldKeys.Data[Idx].N, OldKeys.Data[Idx].Hash, OldData.Data[Idx]);

		OldControl.Free();
		OldKeys.Free();
		OldData.Free();
	}

	inline bool Add_(const string &Key, u32 Hash, T Item)
	{
		size_t Mask = Data_.Capacity-1;
		size_t Idx = Hash & Mask;
		size_t Start = Idx;
		do {
			const key &It = Keys.Data[Idx];
			if((Control[Idx] & 0b1) == 0)
				break;
			else if(It.Hash == Hash && It.N == Key)
			{
				return false;
			}
			Idx = (Idx + 1) & Mask;
		} while(Idx != Start);
		Assert((Control[Idx] & 0b1) == 0);
		Keys.Data[Idx] = {Hash, Key};
		Data_.Data[Idx] = Item;
		Control.Data[Idx] |= 0b1;
		Used++;
		return true;
	}

	bool Add(string Key, T Item)
	{
		if((Used + 1) * 10 > Data_.Capacity * 8)
			ReHash();
		// 2^n
		Assert((Data_.Capacity & (Data_.Capacity - 1)) == 0);

		u32 Hash = murmur3_32((const u8 *)Key.Data, Key.Size, SEED);
		return Add_(Key, Hash, Item);
	}
	bool Contains(const string &Key, u32 Hash)
	{
		size_t Mask = Data_.Capacity-1;
		size_t Idx = Hash & Mask;
		size_t Start = Idx;
		do {
			const key &It = Keys.Data[Idx];
			if(It.Hash == Hash && It.N == Key)
			{
				return true;
			}
			Idx = (Idx + 1) & Mask;
		} while(Idx != Start);

		return false;
	}
	bool Contains(const string &Key)
	{
		u32 Hash = murmur3_32((const u8 *)Key.Data, Key.Size, SEED);
		return Contains(Key, Hash);
	}
	T* GetUnstablePtr(const string &Key)
	{
		u32 Hash = murmur3_32((const u8 *)Key.Data, Key.Size, SEED);
		size_t Mask = Data_.Capacity-1;
		size_t Idx = Hash & Mask;
		size_t Start = Idx;
		do {
			const key &It = Keys.Data[Idx];
			if((Control[Idx] & 0b1) == 0)
				return NULL;
			if(It.Hash == Hash && It.N == Key)
			{
				return &Data_.Data[Idx];
			}
			Idx = (Idx + 1) & Mask;
		} while(Idx != Start);

		return NULL;
	}
	T operator[](const string &Key)
	{
		T* Ptr = GetUnstablePtr(Key);
		if(Ptr)
			return *Ptr;
		return Default;
	}
	void Free()
	{
		Keys.Free();
		Data_.Free();
		Control.Free();
	}
};

template <typename T>
struct map_int {
	dynamic<int> Keys;
	dynamic<T> Data;
	T Default = T{};
	int Bottom = 0;

	bool Add(int Key, T Item)
	{
		if(Contains(Key))
			return false;

		Keys.Push(Key);
		Data.Push(Item);
		return true;
	}
	bool Contains(int Key)
	{
		ForArray(Idx, Keys)
		{
			if(Keys[Idx] == Key)
				return true;
		}
		return false;
	}
	T* GetUnstablePtr(int Key)
	{
		ForArray(Idx, Keys)
		{
			if(Keys[Idx] == Key)
			{
				return &Data.Data[Idx];
			}
		}
		return NULL;
	}
	T operator[](int Key) const
	{
		T* Ptr = GetUnstablePtr(Key);
		if(Ptr)
			return *Ptr;
		return Default;
	}
	T& operator[](int Key)
	{
		ForArray(Idx, Keys)
		{
			if(Keys[Idx] == Key)
			{
				return Data.Data[Idx];
			}
		}
		unreachable;
	}
	void Free()
	{
		Keys.Free();
		Data.Free();
	}
	void Clear()
	{
		Keys.Count = Bottom;
		Data.Count = Bottom;
	}
};


