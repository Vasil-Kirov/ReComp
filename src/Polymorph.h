#pragma once
#include "Semantics.h"

symbol *GenerateFunctionFromPolymorphicCall(checker *Checker, node *Call);
slice<struct_member> ResolveGenericStruct(const type *StructT, slice<struct_generic_argument> GenArgs, dict<u32> *ParameterTypes = nullptr);

