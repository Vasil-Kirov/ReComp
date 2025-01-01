#pragma once
#include "VString.h"
#include "Dynamic.h"

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
	string N;
	u32 Hash;
};

const int SEED = 980346;

template <typename T>
struct dict {
	dynamic<key> Keys;
	dynamic<T> Data;
	T Default = T{};
	int Bottom = 0;

	bool Add(string Key, T Item)
	{
		if(Contains(Key))
			return false;

		u32 Hash = murmur3_32((const u8 *)Key.Data, Key.Size, SEED);
		key AddKey = key { Key, Hash };
		Keys.Push(AddKey);
		Data.Push(Item);
		return true;
	}
	bool Contains(string Key)
	{
		u32 Hash = murmur3_32((const u8 *)Key.Data, Key.Size, SEED);
		ForArray(Idx, Keys)
		{
			if(Keys[Idx].Hash == Hash)
			{
				if(Keys[Idx].N == Key)
					return true;
			}
		}

		return false;
	}
	T* GetUnstablePtr(string Key)
	{
		u32 Hash = murmur3_32((const u8 *)Key.Data, Key.Size, SEED);
		ForArray(Idx, Keys)
		{
			if(Keys[Idx].Hash == Hash)
			{
				if(Keys[Idx].N == Key)
					return &Data.Data[Idx];
			}
		}
		return NULL;
	}
	T operator[](string Key)
	{
		T* Ptr = GetUnstablePtr(Key);
		if(Ptr)
			return *Ptr;
		return Default;
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



