#include "Interpreter.h"
#include "Type.h"

void i8_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void i8_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i8, (i8 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i8, (i8 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i8, (i8 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i8, (i8 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i8, (i8 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i8, (i8 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i8, (i8 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i8, (i8 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i8, (i8 *)&V->i64, 8);
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
						memcpy(&R->i8, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->i8, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i8, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->i8, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->i8, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void i16_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void i16_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i16, (i16 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i16, (i16 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i16, (i16 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i16, (i16 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i16, (i16 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i16, (i16 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i16, (i16 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i16, (i16 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i16, (i16 *)&V->i64, 8);
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
						memcpy(&R->i16, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->i16, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i16, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->i16, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->i16, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void i32_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void i32_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i32, (i32 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i32, (i32 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i32, (i32 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i32, (i32 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i32, (i32 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i32, (i32 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i32, (i32 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i32, (i32 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i32, (i32 *)&V->i64, 8);
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
						memcpy(&R->i32, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->i32, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i32, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->i32, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->i32, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void i64_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void i64_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->i64, (i64 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->i64, (i64 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->i64, (i64 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->i64, (i64 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->i64, (i64 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->i64, (i64 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->i64, (i64 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->i64, (i64 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->i64, (i64 *)&V->i64, 8);
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
						memcpy(&R->i64, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->i64, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->i64, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->i64, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->i64, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void u8_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void u8_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u8, (u8 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u8, (u8 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u8, (u8 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u8, (u8 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u8, (u8 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u8, (u8 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u8, (u8 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u8, (u8 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u8, (u8 *)&V->i64, 8);
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
						memcpy(&R->u8, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->u8, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u8, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->u8, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->u8, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void u16_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void u16_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u16, (u16 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u16, (u16 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u16, (u16 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u16, (u16 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u16, (u16 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u16, (u16 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u16, (u16 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u16, (u16 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u16, (u16 *)&V->i64, 8);
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
						memcpy(&R->u16, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->u16, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u16, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->u16, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->u16, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void u32_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void u32_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u32, (u32 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u32, (u32 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u32, (u32 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u32, (u32 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u32, (u32 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u32, (u32 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u32, (u32 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u32, (u32 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u32, (u32 *)&V->i64, 8);
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
						memcpy(&R->u32, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->u32, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u32, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->u32, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->u32, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void u64_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void u64_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->u64, (u64 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->u64, (u64 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->u64, (u64 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->u64, (u64 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->u64, (u64 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->u64, (u64 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->u64, (u64 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->u64, (u64 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->u64, (u64 *)&V->i64, 8);
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
						memcpy(&R->u64, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->u64, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->u64, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->u64, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->u64, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void f32_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void f32_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->f32, (f32 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->f32, (f32 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->f32, (f32 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->f32, (f32 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->f32, (f32 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->f32, (f32 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->f32, (f32 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->f32, (f32 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->f32, (f32 *)&V->i64, 8);
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
						memcpy(&R->f32, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->f32, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->f32, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->f32, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->f32, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void f64_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
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
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void f64_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->f64, (f64 *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->f64, (f64 *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->f64, (f64 *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->f64, (f64 *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->f64, (f64 *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->f64, (f64 *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->f64, (f64 *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->f64, (f64 *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->f64, (f64 *)&V->i64, 8);
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
						memcpy(&R->f64, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->f64, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->f64, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->f64, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->f64, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void type_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->type = (type)V->u8;
				} break;
				case Basic_u16:
				{
					R->type = (type)V->u16;
				} break;
				case Basic_u32:
				{
					R->type = (type)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->type = (type)V->u64;
				} break;
				case Basic_i8:
				{
					R->type = (type)V->i8;
				} break;
				case Basic_i16:
				{
					R->type = (type)V->i16;
				} break;
				case Basic_i32:
				{
					R->type = (type)V->i32;
				} break;
				case Basic_i64:
				{
					R->type = (type)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->type = (type)V->i64;
				} break;
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void type_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->type, (type *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->type, (type *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->type, (type *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->type, (type *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->type, (type *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->type, (type *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->type, (type *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->type, (type *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->type, (type *)&V->i64, 8);
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
						memcpy(&R->type, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->type, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->type, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->type, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->type, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void int_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->int = (int)V->u8;
				} break;
				case Basic_u16:
				{
					R->int = (int)V->u16;
				} break;
				case Basic_u32:
				{
					R->int = (int)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->int = (int)V->u64;
				} break;
				case Basic_i8:
				{
					R->int = (int)V->i8;
				} break;
				case Basic_i16:
				{
					R->int = (int)V->i16;
				} break;
				case Basic_i32:
				{
					R->int = (int)V->i32;
				} break;
				case Basic_i64:
				{
					R->int = (int)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->int = (int)V->i64;
				} break;
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void int_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->int, (int *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->int, (int *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->int, (int *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->int, (int *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->int, (int *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->int, (int *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->int, (int *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->int, (int *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->int, (int *)&V->i64, 8);
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
						memcpy(&R->int, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->int, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->int, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->int, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->int, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void uint_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->uint = (uint)V->u8;
				} break;
				case Basic_u16:
				{
					R->uint = (uint)V->u16;
				} break;
				case Basic_u32:
				{
					R->uint = (uint)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->uint = (uint)V->u64;
				} break;
				case Basic_i8:
				{
					R->uint = (uint)V->i8;
				} break;
				case Basic_i16:
				{
					R->uint = (uint)V->i16;
				} break;
				case Basic_i32:
				{
					R->uint = (uint)V->i32;
				} break;
				case Basic_i64:
				{
					R->uint = (uint)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->uint = (uint)V->i64;
				} break;
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void uint_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(&R->uint, (uint *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(&R->uint, (uint *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(&R->uint, (uint *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(&R->uint, (uint *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(&R->uint, (uint *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(&R->uint, (uint *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(&R->uint, (uint *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(&R->uint, (uint *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(&R->uint, (uint *)&V->i64, 8);
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
						memcpy(&R->uint, V->ivec2, 8);
					}
					else
					{
						memcpy(&R->uint, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(&R->uint, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(&R->uint, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(&R->uint, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
void ptr_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					R->ptr = (ptr)V->u8;
				} break;
				case Basic_u16:
				{
					R->ptr = (ptr)V->u16;
				} break;
				case Basic_u32:
				{
					R->ptr = (ptr)V->u32;
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					R->ptr = (ptr)V->u64;
				} break;
				case Basic_i8:
				{
					R->ptr = (ptr)V->i8;
				} break;
				case Basic_i16:
				{
					R->ptr = (ptr)V->i16;
				} break;
				case Basic_i32:
				{
					R->ptr = (ptr)V->i32;
				} break;
				case Basic_i64:
				{
					R->ptr = (ptr)V->i64;
				} break;
				case Basic_type:
				case Basic_int:
				{
					R->ptr = (ptr)V->i64;
				} break;
				default: unreachable;
			}
		} break;
        default: unreachable;
	}
}
void ptr_bit_cast_fn(value *R, value *V) {
	const type *TYPE = GetType(V->Type);
	switch (TYPE->Kind)
	{
        case TypeKind_Basic:
		{
			switch(TYPE->Basic.Kind)
			{
				case Basic_u8:
				{
					memcpy(R->ptr, (ptr *)&V->u8, 1);
				} break;
				case Basic_u16:
				{
					memcpy(R->ptr, (ptr *)&V->u16, 2);
				} break;
				case Basic_u32:
				{
					memcpy(R->ptr, (ptr *)&V->u32, 4);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					memcpy(R->ptr, (ptr *)&V->u64, 8);
				} break;
				case Basic_i8:
				{
					memcpy(R->ptr, (ptr *)&V->i8, 1);
				} break;
				case Basic_i16:
				{
					memcpy(R->ptr, (ptr *)&V->i16, 2);
				} break;
				case Basic_i32:
				{
					memcpy(R->ptr, (ptr *)&V->i32, 4);
				} break;
				case Basic_i64:
				{
					memcpy(R->ptr, (ptr *)&V->i64, 8);
				} break;
				case Basic_type:
				case Basic_int:
				{
					memcpy(R->ptr, (ptr *)&V->i64, 8);
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
						memcpy(R->ptr, V->ivec2, 8);
					}
					else
					{
						memcpy(R->ptr, &V->ivec, 8);
					}
				} break; \
                case Vector_Float:
                {
                    if(TYPE->Vector.ElementCount == 2)
                    {
                        memcpy(R->ptr, V->fvec2, 8);
                    }
                    else
                    {
                        memcpy(R->ptr, &V->fvec, 8);
                    }
                } break;
            }
        } break;
        default:
        {
			memcpy(R->ptr, V->ptr, GetTypeSize(TYPE));
        } break;
	}
}
