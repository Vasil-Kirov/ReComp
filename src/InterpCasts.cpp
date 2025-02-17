#include "Interpreter.h"
#include "Type.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
void i8_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->i8 = (i8)V->u8;
				} break;
				case Basic_u16:
				{
					R->i8 = (i8)V->u16;
				} break;
				case Basic_u32:
				{
					R->i8 = (i8)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->i8 = (i8)V->u64;
				} break;
				case Basic_i8:
				{
					R->i8 = (i8)V->i8;
				} break;
				case Basic_i16:
				{
					R->i8 = (i8)V->i16;
				} break;
				case Basic_i32:
				{
					R->i8 = (i8)V->i32;
				} break;
				case Basic_i64:
				{
					R->i8 = (i8)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->i8 = (i8)V->i64;
				} break;
                case Basic_f32:
                {
					R->i8 = (i8)V->f32;
                } break;
                case Basic_f64:
                {
					R->i8 = (i8)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->i8 = (i8)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void i8_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i8, (i8 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i8, (i8 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i8, (i8 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i8, (i8 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i8, (i8 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i8, (i8 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i8, (i8 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i8, (i8 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i8, (i8 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->i8, (i8 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->i8, (i8 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->i8, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->i8, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->i8, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i8, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->i8, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->i8, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->i8, V->ptr, Size);
        } break;
	}
}
void i16_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->i16 = (i16)V->u8;
				} break;
				case Basic_u16:
				{
					R->i16 = (i16)V->u16;
				} break;
				case Basic_u32:
				{
					R->i16 = (i16)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->i16 = (i16)V->u64;
				} break;
				case Basic_i8:
				{
					R->i16 = (i16)V->i8;
				} break;
				case Basic_i16:
				{
					R->i16 = (i16)V->i16;
				} break;
				case Basic_i32:
				{
					R->i16 = (i16)V->i32;
				} break;
				case Basic_i64:
				{
					R->i16 = (i16)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->i16 = (i16)V->i64;
				} break;
                case Basic_f32:
                {
					R->i16 = (i16)V->f32;
                } break;
                case Basic_f64:
                {
					R->i16 = (i16)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->i16 = (i16)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void i16_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i16, (i16 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i16, (i16 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i16, (i16 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i16, (i16 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i16, (i16 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i16, (i16 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i16, (i16 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i16, (i16 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i16, (i16 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->i16, (i16 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->i16, (i16 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->i16, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->i16, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->i16, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i16, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->i16, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->i16, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->i16, V->ptr, Size);
        } break;
	}
}
void i32_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->i32 = (i32)V->u8;
				} break;
				case Basic_u16:
				{
					R->i32 = (i32)V->u16;
				} break;
				case Basic_u32:
				{
					R->i32 = (i32)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->i32 = (i32)V->u64;
				} break;
				case Basic_i8:
				{
					R->i32 = (i32)V->i8;
				} break;
				case Basic_i16:
				{
					R->i32 = (i32)V->i16;
				} break;
				case Basic_i32:
				{
					R->i32 = (i32)V->i32;
				} break;
				case Basic_i64:
				{
					R->i32 = (i32)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->i32 = (i32)V->i64;
				} break;
                case Basic_f32:
                {
					R->i32 = (i32)V->f32;
                } break;
                case Basic_f64:
                {
					R->i32 = (i32)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->i32 = (i32)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void i32_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i32, (i32 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i32, (i32 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i32, (i32 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i32, (i32 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i32, (i32 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i32, (i32 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i32, (i32 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i32, (i32 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i32, (i32 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->i32, (i32 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->i32, (i32 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->i32, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->i32, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->i32, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i32, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->i32, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->i32, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->i32, V->ptr, Size);
        } break;
	}
}
void i64_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->i64 = (i64)V->u8;
				} break;
				case Basic_u16:
				{
					R->i64 = (i64)V->u16;
				} break;
				case Basic_u32:
				{
					R->i64 = (i64)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->i64 = (i64)V->u64;
				} break;
				case Basic_i8:
				{
					R->i64 = (i64)V->i8;
				} break;
				case Basic_i16:
				{
					R->i64 = (i64)V->i16;
				} break;
				case Basic_i32:
				{
					R->i64 = (i64)V->i32;
				} break;
				case Basic_i64:
				{
					R->i64 = (i64)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->i64 = (i64)V->i64;
				} break;
                case Basic_f32:
                {
					R->i64 = (i64)V->f32;
                } break;
                case Basic_f64:
                {
					R->i64 = (i64)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->i64 = (i64)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void i64_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i64, (i64 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i64, (i64 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i64, (i64 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i64, (i64 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i64, (i64 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i64, (i64 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i64, (i64 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i64, (i64 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i64, (i64 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->i64, (i64 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->i64, (i64 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->i64, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->i64, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->i64, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i64, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->i64, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->i64, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->i64, V->ptr, Size);
        } break;
	}
}
void u8_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->u8 = (u8)V->u8;
				} break;
				case Basic_u16:
				{
					R->u8 = (u8)V->u16;
				} break;
				case Basic_u32:
				{
					R->u8 = (u8)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->u8 = (u8)V->u64;
				} break;
				case Basic_i8:
				{
					R->u8 = (u8)V->i8;
				} break;
				case Basic_i16:
				{
					R->u8 = (u8)V->i16;
				} break;
				case Basic_i32:
				{
					R->u8 = (u8)V->i32;
				} break;
				case Basic_i64:
				{
					R->u8 = (u8)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->u8 = (u8)V->i64;
				} break;
                case Basic_f32:
                {
					R->u8 = (u8)V->f32;
                } break;
                case Basic_f64:
                {
					R->u8 = (u8)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->u8 = (u8)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void u8_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u8, (u8 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u8, (u8 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u8, (u8 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u8, (u8 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u8, (u8 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u8, (u8 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u8, (u8 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u8, (u8 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u8, (u8 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->u8, (u8 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->u8, (u8 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->u8, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->u8, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->u8, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u8, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->u8, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->u8, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->u8, V->ptr, Size);
        } break;
	}
}
void u16_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->u16 = (u16)V->u8;
				} break;
				case Basic_u16:
				{
					R->u16 = (u16)V->u16;
				} break;
				case Basic_u32:
				{
					R->u16 = (u16)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->u16 = (u16)V->u64;
				} break;
				case Basic_i8:
				{
					R->u16 = (u16)V->i8;
				} break;
				case Basic_i16:
				{
					R->u16 = (u16)V->i16;
				} break;
				case Basic_i32:
				{
					R->u16 = (u16)V->i32;
				} break;
				case Basic_i64:
				{
					R->u16 = (u16)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->u16 = (u16)V->i64;
				} break;
                case Basic_f32:
                {
					R->u16 = (u16)V->f32;
                } break;
                case Basic_f64:
                {
					R->u16 = (u16)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->u16 = (u16)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void u16_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u16, (u16 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u16, (u16 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u16, (u16 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u16, (u16 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u16, (u16 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u16, (u16 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u16, (u16 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u16, (u16 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u16, (u16 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->u16, (u16 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->u16, (u16 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->u16, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->u16, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->u16, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u16, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->u16, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->u16, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->u16, V->ptr, Size);
        } break;
	}
}
void u32_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->u32 = (u32)V->u8;
				} break;
				case Basic_u16:
				{
					R->u32 = (u32)V->u16;
				} break;
				case Basic_u32:
				{
					R->u32 = (u32)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->u32 = (u32)V->u64;
				} break;
				case Basic_i8:
				{
					R->u32 = (u32)V->i8;
				} break;
				case Basic_i16:
				{
					R->u32 = (u32)V->i16;
				} break;
				case Basic_i32:
				{
					R->u32 = (u32)V->i32;
				} break;
				case Basic_i64:
				{
					R->u32 = (u32)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->u32 = (u32)V->i64;
				} break;
                case Basic_f32:
                {
					R->u32 = (u32)V->f32;
                } break;
                case Basic_f64:
                {
					R->u32 = (u32)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->u32 = (u32)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void u32_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u32, (u32 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u32, (u32 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u32, (u32 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u32, (u32 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u32, (u32 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u32, (u32 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u32, (u32 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u32, (u32 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u32, (u32 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->u32, (u32 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->u32, (u32 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->u32, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->u32, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->u32, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u32, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->u32, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->u32, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->u32, V->ptr, Size);
        } break;
	}
}
void u64_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->u64 = (u64)V->u8;
				} break;
				case Basic_u16:
				{
					R->u64 = (u64)V->u16;
				} break;
				case Basic_u32:
				{
					R->u64 = (u64)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->u64 = (u64)V->u64;
				} break;
				case Basic_i8:
				{
					R->u64 = (u64)V->i8;
				} break;
				case Basic_i16:
				{
					R->u64 = (u64)V->i16;
				} break;
				case Basic_i32:
				{
					R->u64 = (u64)V->i32;
				} break;
				case Basic_i64:
				{
					R->u64 = (u64)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->u64 = (u64)V->i64;
				} break;
                case Basic_f32:
                {
					R->u64 = (u64)V->f32;
                } break;
                case Basic_f64:
                {
					R->u64 = (u64)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->u64 = (u64)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void u64_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u64, (u64 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u64, (u64 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u64, (u64 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u64, (u64 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u64, (u64 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u64, (u64 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u64, (u64 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u64, (u64 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u64, (u64 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->u64, (u64 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->u64, (u64 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->u64, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->u64, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->u64, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u64, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->u64, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->u64, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->u64, V->ptr, Size);
        } break;
	}
}
void f32_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->f32 = (f32)V->u8;
				} break;
				case Basic_u16:
				{
					R->f32 = (f32)V->u16;
				} break;
				case Basic_u32:
				{
					R->f32 = (f32)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->f32 = (f32)V->u64;
				} break;
				case Basic_i8:
				{
					R->f32 = (f32)V->i8;
				} break;
				case Basic_i16:
				{
					R->f32 = (f32)V->i16;
				} break;
				case Basic_i32:
				{
					R->f32 = (f32)V->i32;
				} break;
				case Basic_i64:
				{
					R->f32 = (f32)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->f32 = (f32)V->i64;
				} break;
                case Basic_f32:
                {
					R->f32 = (f32)V->f32;
                } break;
                case Basic_f64:
                {
					R->f32 = (f32)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->f32 = (f32)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void f32_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->f32, (f32 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->f32, (f32 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->f32, (f32 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->f32, (f32 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->f32, (f32 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->f32, (f32 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->f32, (f32 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->f32, (f32 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->f32, (f32 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->f32, (f32 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->f32, (f32 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->f32, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->f32, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->f32, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->f32, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->f32, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->f32, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->f32, V->ptr, Size);
        } break;
	}
}
void f64_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->f64 = (f64)V->u8;
				} break;
				case Basic_u16:
				{
					R->f64 = (f64)V->u16;
				} break;
				case Basic_u32:
				{
					R->f64 = (f64)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->f64 = (f64)V->u64;
				} break;
				case Basic_i8:
				{
					R->f64 = (f64)V->i8;
				} break;
				case Basic_i16:
				{
					R->f64 = (f64)V->i16;
				} break;
				case Basic_i32:
				{
					R->f64 = (f64)V->i32;
				} break;
				case Basic_i64:
				{
					R->f64 = (f64)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->f64 = (f64)V->i64;
				} break;
                case Basic_f32:
                {
					R->f64 = (f64)V->f32;
                } break;
                case Basic_f64:
                {
					R->f64 = (f64)V->f64;
                } break;
				default: unreachable;
			}
		} break;
        case TypeKind_Pointer:
        {
            R->f64 = (f64)(u64)V->ptr;
        } break;
        default: unreachable;
	}
}
void f64_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->f64, (f64 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->f64, (f64 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->f64, (f64 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->f64, (f64 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->f64, (f64 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->f64, (f64 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->f64, (f64 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->f64, (f64 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->f64, (f64 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->f64, (f64 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->f64, (f64 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->f64, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->f64, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->f64, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->f64, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->f64, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->f64, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->f64, V->ptr, Size);
        } break;
	}
}
void ptr_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->ptr = (void *)V->u8;
				} break;
				case Basic_u16:
				{
					R->ptr = (void *)V->u16;
				} break;
				case Basic_u32:
				{
					R->ptr = (void *)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->ptr = (void *)V->u64;
				} break;
				case Basic_i8:
				{
					R->ptr = (void *)V->i8;
				} break;
				case Basic_i16:
				{
					R->ptr = (void *)V->i16;
				} break;
				case Basic_i32:
				{
					R->ptr = (void *)V->i32;
				} break;
				case Basic_i64:
				{
					R->ptr = (void *)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->ptr = (void *)V->i64;
				} break;
				default: unreachable;
			}
		} break;
        // @NOTE: pointer to pointer casts are handled seperately
        default: unreachable;
	}
}
void ptr_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(R->ptr, (void * *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(R->ptr, (void * *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(R->ptr, (void * *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(R->ptr, (void * *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(R->ptr, (void * *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(R->ptr, (void * *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(R->ptr, (void * *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(R->ptr, (void * *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(R->ptr, (void * *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(R->ptr, (void * *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(R->ptr, (void * *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(R->ptr, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(R->ptr, V->ivec2, Size);
					}
					else
					{
						memcpy(R->ptr, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(R->ptr, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(R->ptr, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(R->ptr, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(R->ptr, V->ptr, Size);
        } break;
	}
}
void ivec2_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->ivec2, (i32 * *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->ivec2, (i32 * *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->ivec2, (i32 * *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->ivec2, (i32 * *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->ivec2, (i32 * *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->ivec2, (i32 * *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->ivec2, (i32 * *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->ivec2, (i32 * *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->ivec2, (i32 * *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->ivec2, (i32 * *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->ivec2, (i32 * *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->ivec2, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->ivec2, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->ivec2, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->ivec2, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->ivec2, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->ivec2, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->ivec2, V->ptr, Size);
        } break;
	}
}
void ivec_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->ivec, (__m128i *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->ivec, (__m128i *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->ivec, (__m128i *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->ivec, (__m128i *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->ivec, (__m128i *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->ivec, (__m128i *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->ivec, (__m128i *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->ivec, (__m128i *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->ivec, (__m128i *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->ivec, (__m128i *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->ivec, (__m128i *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->ivec, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->ivec, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->ivec, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->ivec, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->ivec, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->ivec, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->ivec, V->ptr, Size);
        } break;
	}
}
void fvec2_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->fvec2, (f32 * *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->fvec2, (f32 * *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->fvec2, (f32 * *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->fvec2, (f32 * *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->fvec2, (f32 * *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->fvec2, (f32 * *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->fvec2, (f32 * *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->fvec2, (f32 * *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->fvec2, (f32 * *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->fvec2, (f32 * *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->fvec2, (f32 * *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->fvec2, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->fvec2, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->fvec2, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->fvec2, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->fvec2, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->fvec2, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->fvec2, V->ptr, Size);
        } break;
	}
}
void fvec_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
    if(TYPE->Kind == TypeKind_Enum) TYPE = GetType(TYPE->Enum.Type);
    auto Size = GetTypeSize(TYPE);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->fvec, (__m128 *)&V->u8, Size);
				} break;
				case Basic_u16:
				{
					memcpy(&R->fvec, (__m128 *)&V->u16, Size);
				} break;
				case Basic_u32:
				{
					memcpy(&R->fvec, (__m128 *)&V->u32, Size);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->fvec, (__m128 *)&V->u64, Size);
				} break;
				case Basic_i8:
				{
					memcpy(&R->fvec, (__m128 *)&V->i8, Size);
				} break;
				case Basic_i16:
				{
					memcpy(&R->fvec, (__m128 *)&V->i16, Size);
				} break;
				case Basic_i32:
				{
					memcpy(&R->fvec, (__m128 *)&V->i32, Size);
				} break;
				case Basic_i64:
				{
					memcpy(&R->fvec, (__m128 *)&V->i64, Size);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->fvec, (__m128 *)&V->i64, Size);
				} break;
                case Basic_f32:
                {
					memcpy(&R->fvec, (__m128 *)&V->f32, Size);
                } break;
                case Basic_f64:
                {
					memcpy(&R->fvec, (__m128 *)&V->f64, Size);
                } break;
                case Basic_string:
                {
					memcpy(&R->fvec, V->ptr, Size);
                } break;
				default: unreachable;
			}
		} break;
		case TypeKind_Vector:
		{
			switch(TYPE->Vector.Kind)
            {
                case Vector_Int:
				{
					if(TYPE->Vector.ElementCount == 2)
					{
						memcpy(&R->fvec, V->ivec2, Size);
					}
					else
					{
						memcpy(&R->fvec, &V->ivec, Size);
					}
				} break;
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->fvec, V->fvec2, Size);
                    }
                    else
                    {
                        memcpy(&R->fvec, &V->fvec, Size);
                    }
                } break;
            }
        } break;
        case TypeKind_Pointer:
        {
            memcpy(&R->fvec, &V->ptr, Size);
        } break;
        default:
        {
			memcpy(&R->fvec, V->ptr, Size);
        } break;
	}
}
#pragma clang diagnostic pop
