#pragma once
#include "Semantics.h"

symbol *GenerateFunctionFromPolymorphicCall(checker *Checker, node *Call);
u32 ResolveGenericStruct(const type *StructT, slice<struct_generic_argument> GenArgs, dict<u32> *ParameterTypes);
u32 GenericFunctionPolymorphSignature(checker *Checker, dict<u32> &DefinedGenerics, node *Call, node *FnNode, u32 FnTi);

