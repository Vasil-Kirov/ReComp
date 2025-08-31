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

struct FlowState
{
	scratch_arena *Arena;

	array<array<bool>> NullLocations; // Accessed NullLocation[reg][block_id]
	dynamic<u32> Nulls;  // Registers containing null
	dynamic<u32> Trues;  // Registers containing true
	dynamic<u32> Falses; // Registers containing false
	dynamic<flow_branch*> AlreadyEvaluated;
	dynamic<u32> LoopingDetectionBuffer;
	flow_branch* CurrentFlow;
	const error_info *LastErrorInfo;
};

void FlowTypeFunction(function *Fn);


