#pragma once
#include "Basic.h"
#include "IR.h"
#include "Lexer.h"
#include "VString.h"
#include "Dynamic.h"

extern u32 NULLType;

struct type;
struct scope;
#define INVALID_TYPE UINT32_MAX

//#if _WIN32
//#define MAX_PARAMETER_SIZE 8
//#else
#define MAX_PARAMETER_SIZE 16
//#endif

enum class platform_target
{
	Windows,
	UnixBased,
	Wasm,
};

enum type_kind
{
	TypeKind_Invalid ,

	TypeKind_Basic   ,
	TypeKind_Function,
	TypeKind_Struct  ,
	TypeKind_Pointer ,
	TypeKind_Array   ,
	TypeKind_Slice   ,
	TypeKind_Vector  ,
	TypeKind_Enum    ,
	TypeKind_Generic ,
};

enum basic_kind
{
	Basic_bool,
	Basic_string,
	//Basic_cstring,

	Basic_u8,
	Basic_u16,
	Basic_u32,
	Basic_u64,

	Basic_i8,
	Basic_i16,
	Basic_i32,
	Basic_i64,

	Basic_f32,
	Basic_f64,

	Basic_UntypedInteger,
	Basic_UntypedFloat,

	Basic_int,
	Basic_uint,

	Basic_type,
	Basic_auto,
	Basic_module,
};

enum basic_flags
{
	BasicFlag_Boolean = BIT(0),
	BasicFlag_Integer = BIT(1),
	BasicFlag_Float   = BIT(2),
	BasicFlag_String  = BIT(3),
	//BasicFlag_CString = BIT(4),
	BasicFlag_Untyped = BIT(5),
	BasicFlag_Unsigned= BIT(6),
	BasicFlag_TypeID  = BIT(7),

	BasicFlag_Numeric = BasicFlag_Integer | BasicFlag_Float,
};

enum struct_flags
{
	StructFlag_Packed = BIT(0),
	StructFlag_Generic= BIT(1),
	StructFlag_Union  = BIT(2),
	StructFlag_FnReturn=BIT(3),
};

struct basic_type
{
	basic_kind Kind;
	int Flags;
	int Size;  // -1 if it's arch dependant
	string Name;
};

struct struct_member
{
	string ID;
	u32 Type;
};

struct struct_type
{
	slice<struct_member> Members;
	string Name;
	u32 Flags;
	u32 SubType;
};

struct slice_type
{
	u32 Type;
};

struct default_value
{
	int Idx;
	node *Default;
};

struct function_type
{
	slice<u32> Returns;
	u32 *Args;
	slice<default_value> DefaultValues;
	u32 Flags;
	int ArgCount;
};

enum pointer_flags
{
	PointerFlag_Optional = BIT(0)
};

struct pointer
{
	u32 Pointed;
	u32 Flags;
};

struct array_type
{
	u32 Type;
	u32 MemberCount;
};

enum vector_kind
{
	Vector_Float,
	Vector_Int,
};

struct vector_type
{
	vector_kind Kind;
	int ElementCount;
};

struct generic_type
{
	u32 ID;
	string Name;
	scope *Scope;
};

struct enum_member
{
	string Name;
	node *Expr;
	slice<instruction> Evaluate;
	const_value Value;
};

struct enum_type
{
	string Name;
	module *Module;        // Module where the enum is defined, for building IR
	slice<import> Imports; // File imports that the enum can use when building IR
	slice<enum_member> Members;
	u32 Type;
};

struct type
{
	uint CachedSize;
	uint CachedAlignment;
	u32 CachedAsPointer;
	type_kind Kind;
	union
	{
		basic_type Basic;
		struct_type Struct;
		function_type Function;
		pointer Pointer;
		array_type Array;
		slice_type Slice;
		enum_type Enum;
		vector_type Vector;
		generic_type Generic;
	};
};

u32 AddType(type *Type);
u32 AddTypeWithName(type *Type, string Name);
void FillOpaqueStruct(u32 TypeIdx, type T);

const type *GetType(u32 TypeIdx);

const type *GetTypeRaw(u32 TypeIdx);

const char *GetTypeName(const type *Type);
const char *GetTypeName(u32 TypeIdx);

string GetTypeNameAsString(const type *Type);
string GetTypeNameAsString(u32 Type);

u32 GetReturnType(const type *Type);

int GetBasicTypeSize(const type *Type);
int GetTypeSize(const type *Type);
int GetTypeSize(u32 Type);
int GetTypeAlignment(const type *Type);
int GetTypeAlignment(u32 Type);
int GetStructMemberOffset(const type *Type, uint Member);
int GetStructMemberOffset(u32 Type, uint Member);
int GetRegisterTypeSize();
int GetHostRegisterTypeSize();

b32 CanTypePerformBinExpression(const type *T, token_type Op);
b32 IsTypeCompatible(const type *Left, const type *Right, const type **PotentialPromotion, b32 IsAssignment);
b32 TypesMustMatch(const type *Left, const type *Right);
b32 IsUntyped(const type *Type);
b32 IsUntyped(u32 T);
b32 IsCastValid(const type *From, const type *To);
b32 IsCallable(const type *Type);
b32 IsCastRedundant(const type *From, const type *To);
b32 ShouldCopyType(const type *Type);
b32 HasBasicFlag(const type *Type, u32 FlagMask); // Checks if the type is basic too
b32 IsLoadableType(u32 Type);
b32 IsLoadableType(const type *Type);
u32 GetPointerTo(u32 Type, u32 Flags = 0);
u32 GetArrayType(u32 Type, u32 ElemCount);
u32 GetSliceType(u32 Type);
u32 GetNonOptional(const type *OptionalPointer);
u32 GetOptional(const type *Pointer);
uint GetTypeCount();
b32 IsRetTypePassInPointer(u32 Type);
b32 IsPassInAsIntType(const type *Type);
type *AllocType(type_kind Kind);
u32 MakeGeneric(scope *Scope, string Name);
b32 IsGeneric(const type *Type);
b32 IsGeneric(u32 Type);
u32 ToNonGeneric(u32 TypeID, u32 Resolve, u32 ArgResolve);
u32 GetGenericPart(u32 Resolved, u32 GenericID);
u32 ComplexTypeToSizeType(u32 Complex);
u32 ComplexTypeToSizeType(const type *T);
b32 TypeCheckPointers(const type *L, const type *R, b32 IsAssignment);
u32 AllFloatsStructToReturnType(const type *T);
u32 FindStruct(string Name);
u32 FindEnum(string Name);
u32 VarArgArrayType(u32 ElemCount, u32 ArgT);
u32 MakeStruct(slice<struct_member> Members, string Name, u32 Flags);
void FillOpaqueEnum(string Name, slice<enum_member> Members, u32 Type, u32 Original, slice<import> Imports, module *Module);
b32 IsFn(const type *T);
b32 IsFnOrPtr(const type *T);
b32 IsString(const type *T, b32 OrCString = false);
b32 IsTypeMatchable(const type *T);
b32 IsTypeIterable(const type *T);
b32 IsForeign(const type *T);
b32 IsCString(const type *T);
b32 IsTypeMultiReturn(const type *T);
u32 UntypedGetType(u32 TIdx);
u32 UntypedGetType(const type *T);
u32 ReturnsToType(slice<u32> Returns);
void SetStructCache(u32 TypeIdx);
void WriteFunctionReturnType(string_builder *b, slice<u32> Returns);
b32 VerifyNoStructRecursion(u32 TIdx, int *FailedIdx);
void AddNameToTypeMap(const string *Name, u32 T);
u32 LookupNameOnTypeMap(const string *Name);

const type *OneIsXAndTheOtherY(const type *L, const type *R, type_kind X, type_kind Y);

size_t AddGenericReplacement(string Generic, u32 ToReplace);
void ClearGenericReplacement(size_t To);

b32 IsStructAllFloats(const type *T);
u32 ResolveGenericStruct(u32 Type, u32 ResolvedStruct);
b32 IsSigned(const type *T);
void AddVectorTypes();

u32 GetVecElemType(u32 TIdx);
u32 GetVecElemType(const type *T);

struct generic_replacement
{
	string Generic;
	u32 TypeID;
};

extern platform_target PTarget;
extern dynamic<generic_replacement> GenericReplacements;

inline bool IsUnix()
{
	return PTarget == platform_target::UnixBased;
}

inline bool IsFloatOrVec(const type *T)
{
	return HasBasicFlag(T, BasicFlag_Float) || (T->Kind == TypeKind_Vector && T->Vector.Kind == Vector_Float);
}

