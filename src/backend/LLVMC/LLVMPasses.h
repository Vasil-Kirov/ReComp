#pragma once
#include "LLVMBase.h"
#include <CommandLine.h>

void RunOptimizationPasses(generator *g, LLVMTargetMachineRef T, int Level, u32 Flags);

