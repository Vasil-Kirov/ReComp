#pragma once
#include <IR.h>

struct loaded_nullable
{
	u32 Loaded;
	u32 Location;
};

struct flow_branch
{
	u32 From;
	u32 To;
	flow_branch *Prev;
};

struct non_nullable_location
{
	const error_info *ErrorInfo;
	bool Valid;
};

enum NullComparisonType
{
	NullCmp_EqEq,
	NullCmp_Neq,
};

struct NullComparison
{
	u32 ResultRegister;
	u32 Allocation;
	NullComparisonType Cmp;
};

struct flow_block_info
{
	basic_block *Block;
	map_int<bool> IsNotNull;
	map_int<non_nullable_location> CannotBeNull;
	map_int<non_nullable_location> MustBeNullable;
	map_int<map_int<bool>> State; // [reg][pred]
	slice<NullComparison> Cmps;
};

struct flow_state
{
	scratch_arena *Arena;

	array<flow_block_info> Blocks;
	array<bool> NullLocations; // Accessed NullLocation[reg]
	dynamic<u32> Nulls;  // Registers containing null
	dynamic<u32> Trues;  // Registers containing true
	dynamic<u32> Falses; // Registers containing false
};

void FlowTypeFunction(function *Fn);


