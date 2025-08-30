#pragma once
#include "Basic.h"
#include <atomic>

static uint g_CompileFlags = 0;
static std::atomic<u32> g_LastAddedGlobal = 0;

