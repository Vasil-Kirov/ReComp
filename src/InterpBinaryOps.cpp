#include "Type.h"
#include <type_traits>
#include <xmmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

template <typename T>
struct is_simd : std::false_type {};

// Define specializations for known SIMD types
template <> struct is_simd<__m128i>  : std::true_type {};
template <> struct is_simd<__m128>  : std::true_type {};
//template <> struct is_simd<__m256>  : std::true_type {};
//template <> struct is_simd<__m512>  : std::true_type {};

template<typename T>
void DoAdd(T *Result, T Left, T Right, const type *Type)
{
	if constexpr (is_simd<T>::value)
	{
		Assert(Type->Kind == TypeKind_Vector);
		switch(Type->Vector.Kind)
		{
			case Vector_Int:
			{
				_mm_store_si128((__m128i *)Result, _mm_add_epi32(Left, Right));
			} break;
			case Vector_Float:
			{
				_mm_store_ps((float *)Result, _mm_add_ps(Left, Right));
			} break;
		}
	}
	else
	{
		*Result = Left + Right;
	}
}

template<typename T>
void DoSub(T *Result, T Left, T Right, const type *Type)
{
	if constexpr (is_simd<T>::value)
	{
		Assert(Type->Kind == TypeKind_Vector);
		switch(Type->Vector.Kind)
		{
			case Vector_Int:
			{
				_mm_store_si128((__m128i *)Result, _mm_sub_epi32(Left, Right));
			} break;
			case Vector_Float:
			{
				_mm_store_ps((float *)Result, _mm_sub_ps(Left, Right));
			} break;
		}
	}
	else
	{
		*Result = Left - Right;
	}
}

template<typename T>
void DoDiv(T *Result, T Left, T Right, const type *Type)
{
	if constexpr (is_simd<T>::value)
	{
		Assert(Type->Kind == TypeKind_Vector);
		switch(Type->Vector.Kind)
		{
			case Vector_Int:
			{
				__m128 l = _mm_cvtepi32_ps(Left);
				__m128 r = _mm_cvtepi32_ps(Right);
				__m128 f = _mm_div_ps(l, r);
				_mm_store_si128((__m128i *)Result, _mm_cvtps_epi32(f));
			} break;
			case Vector_Float:
			{
				_mm_store_ps((float *)Result, _mm_div_ps(Left, Right));
			} break;
		}
	}
	else
	{
		*Result = Left / Right;
	}
}

template<typename T>
void DoMul(T *Result, T Left, T Right, const type *Type)
{
	if constexpr (is_simd<T>::value)
	{
		Assert(Type->Kind == TypeKind_Vector);
		switch(Type->Vector.Kind)
		{
			case Vector_Int:
			{
				_mm_store_si128((__m128i *)Result, _mm_mul_epi32(Left, Right));
			} break;
			case Vector_Float:
			{
				_mm_store_ps((float *)Result, _mm_mul_ps(Left, Right));
			} break;
		}
	}
	else
	{
		*Result = Left * Right;
	}
}

__m128 rem_ps(__m128 l, __m128 r)
{
	// return l - trunc(l / r) * r;
	__m128 div = _mm_div_ps(l, r);
	__m128 trunc = _mm_round_ps(div, _MM_FROUND_TO_ZERO);
	__m128 rhs = _mm_mul_ps(trunc, r);
	return _mm_sub_ps(l, rhs);
}

template<typename T>
void DoMod(T *Result, T Left, T Right, const type *Type)
{
	if constexpr (is_simd<T>::value)
	{
		Assert(Type->Kind == TypeKind_Vector);
		switch(Type->Vector.Kind)
		{
			case Vector_Int:
			{
				__m128 l = _mm_cvtepi32_ps(Left);
				__m128 r = _mm_cvtepi32_ps(Right);
				__m128 f = rem_ps(l, r);
				_mm_store_si128((__m128i *)Result, _mm_cvtps_epi32(f));
			} break;
			case Vector_Float:
			{
				_mm_store_ps((float *)Result, rem_ps(Left, Right));
			} break;
		}
	}
	else if constexpr (std::is_floating_point<T>::value)
	{
		Assert(HasBasicFlag(Type, BasicFlag_Float));
		if(Type->Basic.Kind == Basic_f64)
		{
			*Result = fmod(Left, Right);
		}
		else
		{
			*Result = fmodf(Left, Right);
		}
	}
	else
	{
		*Result = Left % Right;
	}
}


