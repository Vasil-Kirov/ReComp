#include "LLVMTypeInfoGlobal.h"
#include "Type.h"
#include "backend/LLVMC/LLVMType.h"
#include "llvm-c/Core.h"
#include "llvm-c/Target.h"

LLVMValueRef RCConstSlice(generator *gen, slice<LLVMValueRef> Slice, LLVMTypeRef IntType, LLVMTypeRef BaseType, LLVMTypeRef SliceTy)
{
	LLVMValueRef Count = LLVMConstInt(IntType, Slice.Count, false);
	LLVMValueRef ArrayGlobal = LLVMAddGlobal(gen->mod, LLVMArrayType2(BaseType, Slice.Count), "");
	LLVMSetGlobalConstant(ArrayGlobal, true);
	LLVMValueRef Array = LLVMConstArray2(BaseType, Slice.Data, Slice.Count);
	LLVMSetInitializer(ArrayGlobal, Array);
	LLVMValueRef ConstVals[] = { Count, ArrayGlobal };
	return LLVMConstNamedStruct(SliceTy, ConstVals, 2);
}

LLVMValueRef RCConstSlice(generator *gen, array<LLVMValueRef> Slice, LLVMTypeRef IntType, LLVMTypeRef BaseType, LLVMTypeRef SliceTy)
{
	return RCConstSlice(gen, SliceFromArray(Slice), IntType, BaseType, SliceTy);
}

LLVMValueRef RCConstString(generator *gen, string S, LLVMTypeRef IntType, LLVMTypeRef String)
{
	LLVMValueRef Count = LLVMConstInt(IntType, S.Size, false);
	LLVMValueRef StrPtr = LLVMBuildGlobalString(gen->bld, S.Data, "");
	LLVMValueRef ConstVals[] = { Count, StrPtr };
	return LLVMConstNamedStruct(String, ConstVals, 2);
}

LLVMValueRef GenTypeInfo(generator *gen)
{
	uint TypeCount = GetTypeCount();
	u32 RCPTypeUnion = FindStruct(STR_LIT("base.TypeUnion"));
	u32 RCPTypeInfo = FindStruct(STR_LIT("base.TypeInfo"));
	LLVMTypeRef TypeInfoType		= ConvertToLLVMType(gen, RCPTypeInfo);
	//LLVMTypeRef TypeKindType		= ConvertToLLVMType(gen, FindEnum(  STR_LIT("base.TypeKind")));
	LLVMTypeRef BasicTypeType		= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.BasicType")));
	LLVMTypeRef StructTypeType   	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.StructType")));
	LLVMTypeRef FunctionTypeType 	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.FunctionType")));
	LLVMTypeRef PointerTypeType  	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.PointerType")));
	LLVMTypeRef ArrayTypeType    	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.ArrayType")));
	LLVMTypeRef SliceTypeType    	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.SliceType")));
	LLVMTypeRef EnumTypeType     	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.EnumType")));
	LLVMTypeRef VectorTypeType   	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.VectorType")));
	LLVMTypeRef GenericTypeType  	= ConvertToLLVMType(gen, FindStruct(STR_LIT("base.GenericType")));
	LLVMTypeRef TypeUnionType	  	= ConvertToLLVMType(gen, RCPTypeUnion);

	u32 RCPStructMemberType = FindStruct(STR_LIT("base.StructMember"));
	u32 RCPEnumMemberType = FindStruct(STR_LIT("base.EnumMember"));
	LLVMTypeRef StructMemberType  	= ConvertToLLVMType(gen, RCPStructMemberType);
	LLVMTypeRef EnumMemberType		= ConvertToLLVMType(gen, RCPEnumMemberType);
	LLVMTypeRef TypeType = ConvertToLLVMType(gen, Basic_type);
	LLVMTypeRef IntType = ConvertToLLVMType(gen, Basic_int);
	LLVMTypeRef U32Type = ConvertToLLVMType(gen, Basic_u32);
	LLVMTypeRef StrType = ConvertToLLVMType(gen, Basic_string);
	LLVMTypeRef BoolType = ConvertToLLVMType(gen, Basic_bool);

	LLVMTypeRef StructMemberSliceType = ConvertToLLVMType(gen, GetSliceType(RCPStructMemberType));
	LLVMTypeRef EnumMemberSliceType = ConvertToLLVMType(gen, GetSliceType(RCPEnumMemberType));
	LLVMTypeRef TypeSliceType = ConvertToLLVMType(gen, GetSliceType(Basic_type));
	//LLVMTypeRef StringSliceType = ConvertToLLVMType(gen, GetSliceType(Basic_string));

	array<LLVMTypeRef> UnionArrayType{TypeCount};
	u64 UnionSize			= LLVMABISizeOfType(gen->data, TypeUnionType);
	u64 BasicTypeSize		= LLVMABISizeOfType(gen->data, BasicTypeType);
	u64 StructTypeSize   	= LLVMABISizeOfType(gen->data, StructTypeType);
	u64 FunctionTypeSize 	= LLVMABISizeOfType(gen->data, FunctionTypeType);
	u64 PointerTypeSize  	= LLVMABISizeOfType(gen->data, PointerTypeType);
	u64 ArrayTypeSize    	= LLVMABISizeOfType(gen->data, ArrayTypeType);
	u64 SliceTypeSize    	= LLVMABISizeOfType(gen->data, SliceTypeType);
	u64 EnumTypeSize     	= LLVMABISizeOfType(gen->data, EnumTypeType);
	u64 VectorTypeSize   	= LLVMABISizeOfType(gen->data, VectorTypeType);
	u64 GenericTypeSize  	= LLVMABISizeOfType(gen->data, GenericTypeType);
	for(int i = 0; i < TypeCount; ++i)
	{
		LLVMTypeRef ETypes[3] = {
			IntType
		};
		int ETypeCount = 2;
		int ESize = 0;
		const type *T = GetType(i);
		switch(T->Kind)
		{
			case TypeKind_Invalid:
			{ Assert(false) } break;
			case TypeKind_Basic:
			{
				ESize = BasicTypeSize;
				ETypes[1] = BasicTypeType;
			} break;
			case TypeKind_Struct:
			{
				ESize = StructTypeSize;
				ETypes[1] = StructTypeType;
			} break;
			case TypeKind_Function:
			{
				ESize = FunctionTypeSize;
				ETypes[1] = FunctionTypeType;
			} break;
			case TypeKind_Pointer:
			{
				ESize = PointerTypeSize;
				ETypes[1] = PointerTypeType;
			} break;
			case TypeKind_Array:
			{
				ESize = ArrayTypeSize;
				ETypes[1] = ArrayTypeType;
			} break;
			case TypeKind_Slice:
			{
				ESize = SliceTypeSize;
				ETypes[1] = SliceTypeType;
			} break;
			case TypeKind_Enum:
			{
				ESize = EnumTypeSize;
				ETypes[1] = EnumTypeType;
			} break;
			case TypeKind_Vector:
			{
				ESize = VectorTypeSize;
				ETypes[1] = VectorTypeType;
			} break;
			case TypeKind_Generic:
			{
				ESize = GenericTypeSize;
				ETypes[1] = GenericTypeType;
			} break;
		}

		if(ESize < UnionSize)
		{
			ETypes[ETypeCount++] = LLVMArrayType2(LLVMIntTypeInContext(gen->ctx, 8), UnionSize-ESize);
		}

		UnionArrayType[i] = LLVMStructType(ETypes, ETypeCount, false);
	}

	LLVMTypeRef TypeInfoArray = LLVMStructType(UnionArrayType.Data, TypeCount, false);

	LLVMValueRef TypeTableContents = LLVMAddGlobal(gen->mod, TypeInfoArray, "base.type_table_contents");
	LLVMSetGlobalConstant(TypeTableContents, true);

	auto ArrayValues = (LLVMValueRef *)VAlloc(sizeof(LLVMValueRef) * TypeCount);

	for(int i = 0; i < TypeCount; ++i)
	{
		const type *T = GetType(i);

		LLVMValueRef TypeKind = LLVMConstInt(IntType, T->Kind, false);
		LLVMValueRef UnionConst = NULL;
		int Pad = 0;

		switch(T->Kind)
		{
			case TypeKind_Invalid:
			{
				ArrayValues[i] = LLVMConstNull(TypeInfoType);
				continue;
			} break;
			case TypeKind_Basic:
			{
				// struct BasicType {
				//     kind: BasicKind,
				//     flags: u32,
				//     size: u32,
				//     name: string,
				// }

				LLVMValueRef ConstVals[] = {
					LLVMConstInt(IntType, T->Basic.Kind, false),
					LLVMConstInt(U32Type, T->Basic.Flags, false),
					LLVMConstInt(U32Type, T->Basic.Size, false),
					RCConstString(gen, T->Basic.Name, IntType, StrType)
				};

				UnionConst = LLVMConstNamedStruct(BasicTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - BasicTypeSize;
			} break;
			case TypeKind_Struct:
			{
				//struct StructMember {
				//    name: string,
				//    t: type,
				//}
				//
				//struct StructType {
				//    members: []StructMember,
				//    name: string,
				//    flags: u32,
				//}

				array<LLVMValueRef> Members{T->Struct.Members.Count};
				ForArray(Idx, T->Struct.Members)
				{
					auto it = T->Struct.Members[Idx];
					LLVMValueRef ConstVals[] = {
						RCConstString(gen, it.ID, IntType, StrType),
						LLVMConstInt(IntType, it.Type, false),
					};
					Members[Idx] = LLVMConstNamedStruct(StructMemberType, ConstVals, ARR_LEN(ConstVals));
				}

				LLVMValueRef ConstVals[] = {
					RCConstSlice(gen, Members, IntType, StructMemberType, StructMemberSliceType),
					RCConstString(gen, T->Struct.Name, IntType, StrType),
					LLVMConstInt(U32Type, T->Struct.Flags, false),
				};

				UnionConst = LLVMConstNamedStruct(StructTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - StructTypeSize;
			} break;
			case TypeKind_Function:
			{

				//struct FunctionType {
				//    returns: []type,
				//    args_t: []type,
				//}

				array<LLVMValueRef> Returns{(size_t)T->Function.Returns.Count};
				ForArray(Idx, T->Function.Returns)
				{
					auto it = T->Function.Returns[Idx];
					Returns[Idx] = LLVMConstInt(TypeType, it, false);
				}

				array<LLVMValueRef> Args{(size_t)T->Function.ArgCount};
				for(int i = 0; i < T->Function.ArgCount; ++i)
				{
					Args[i] = LLVMConstInt(TypeType, T->Function.Args[i], false);
				}

				LLVMValueRef ConstVals[] = {
					RCConstSlice(gen, Returns, IntType, TypeType, TypeSliceType),
					RCConstSlice(gen, Args, IntType, TypeType, TypeSliceType),
				};

				UnionConst = LLVMConstNamedStruct(FunctionTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - FunctionTypeSize;
			} break;
			case TypeKind_Pointer:
			{
				//struct PointerType {
				//    pointee: type,
				//    is_optional: bool,
				//}

				LLVMValueRef ConstVals[] = {
					LLVMConstInt(TypeType, T->Pointer.Pointed, false),
					LLVMConstInt(BoolType, (T->Pointer.Flags & PointerFlag_Optional) != 0, false),
				};

				UnionConst = LLVMConstNamedStruct(PointerTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - PointerTypeSize;
			} break;
			case TypeKind_Array:
			{
				// struct ArrayType {
				//     t: type,
				//     member_count: u32,
				// }

				LLVMValueRef ConstVals[] = {
					LLVMConstInt(TypeType, T->Array.Type, false),
					LLVMConstInt(U32Type, T->Array.MemberCount, false),
				};

				UnionConst = LLVMConstNamedStruct(ArrayTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - ArrayTypeSize;
			} break;
			case TypeKind_Slice:
			{
				// struct SliceType {
				//     t: type,
				// }

				LLVMValueRef ConstVals[] = {
					LLVMConstInt(TypeType, T->Slice.Type, false),
				};

				UnionConst = LLVMConstNamedStruct(SliceTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - SliceTypeSize;
			} break;
			case TypeKind_Enum:
			{
				// struct EnumMember {
				//     name: string,
				//     value: int,
				// }
				//
				// struct EnumType {
				//     name: string,
				//     members: []EnumMember,
				//     t: type,
				// }

				auto EnumType = ConvertToLLVMType(gen, T->Enum.Type);
				array<LLVMValueRef> Members{T->Enum.Members.Count};
				ForArray(Idx, T->Enum.Members)
				{
					auto it = T->Enum.Members[Idx];
					LLVMValueRef ConstVals[] = {
						RCConstString(gen, it.Name, IntType, StrType),
						LLVMConstInt(EnumType, it.Value.Int.IsSigned ? it.Value.Int.Signed : it.Value.Int.Unsigned, false),
					};
					Members[Idx] = LLVMConstNamedStruct(EnumMemberType, ConstVals, ARR_LEN(ConstVals));
				}

				LLVMValueRef ConstVals[] = {
					RCConstString(gen, T->Enum.Name, IntType, StrType),
					RCConstSlice(gen, Members, IntType, StrType, EnumMemberSliceType),
					LLVMConstInt(TypeType, T->Enum.Type, false),
				};

				UnionConst = LLVMConstNamedStruct(EnumTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - EnumTypeSize;
			} break;
			case TypeKind_Vector:
			{
				// struct VectorType {
				//     kind: VectorKind,
				//     elem_count: u32,
				// }

				LLVMValueRef ConstVals[] = {
					LLVMConstInt(IntType, T->Vector.Kind, false),
					LLVMConstInt(U32Type, T->Vector.ElementCount, false),
				};

				UnionConst = LLVMConstNamedStruct(VectorTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - VectorTypeSize;
			} break;
			case TypeKind_Generic:
			{
				// @NOTE: not exactly valid but it is what it is
				LLVMValueRef ConstVals[] = {
					LLVMConstInt(TypeType, T->Slice.Type, false),
				};

				UnionConst = LLVMConstNamedStruct(SliceTypeType, ConstVals, ARR_LEN(ConstVals));
				Pad = UnionSize - GenericTypeSize;
			} break;
		}

		//UnionConst = LLVMBuildCast(gen->bld, LLVMBitCast, UnionConst, TypeUnionType, "");

		LLVMValueRef Constants[3] = { TypeKind, UnionConst };
		int ElemCount = 2;
		if(Pad != 0)
		{
			array<LLVMValueRef> Padding{(size_t)Pad};
			for(int j = 0; j < Pad; ++j)
			{
				Padding[j] = LLVMConstNull(LLVMIntTypeInContext(gen->ctx, 8));
			}
			Constants[ElemCount++] = LLVMConstArray2(LLVMIntTypeInContext(gen->ctx, 8), Padding.Data, Pad);
		}
		ArrayValues[i] = LLVMConstStruct(Constants, ElemCount, false);
	}
	
	LLVMValueRef ConstArray = LLVMConstNamedStruct(TypeInfoArray, ArrayValues, TypeCount);
	LLVMSetInitializer(TypeTableContents, ConstArray);

	u32 RCPTypeSlice = GetSliceType(RCPTypeInfo);
	LLVMTypeRef TypeTableType = ConvertToLLVMType(gen, RCPTypeSlice);
	LLVMValueRef TypeTable = LLVMAddGlobal(gen->mod, TypeTableType, "base.type_table");
	LLVMSetGlobalConstant(TypeTable, true);

	LLVMValueRef TypeSliceValues[] = {
		LLVMConstInt(IntType, TypeCount, false),
		TypeTableContents,
	};

	auto TypeTableInit = LLVMConstNamedStruct(TypeTableType, TypeSliceValues, 2);
	LLVMSetInitializer(TypeTable, TypeTableInit);

	VFree(ArrayValues);
	return TypeTable;
}

