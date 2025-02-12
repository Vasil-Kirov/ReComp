#pragma once
#include "Type.h"


template<typename T>
void DoAdd(T *Result, T Left, T Right, const type *Type);

template<typename T>
void DoSub(T *Result, T Left, T Right, const type *Type);

template<typename T>
void DoMul(T *Result, T Left, T Right, const type *Type);

template<typename T>
void DoDiv(T *Result, T Left, T Right, const type *Type);

template<typename T>
void DoMod(T *Result, T Left, T Right, const type *Type);
