#pragma once
#include "Basic.h"
#include "VString.h"

typedef struct {
	u8 *data;
	size_t at;
	size_t size;
} binary_reader;

binary_reader VLibMakeReader(u8 *data, size_t size);

u8  VLibReadUInt8 (binary_reader *);
u16 VLibReadUInt16(binary_reader *);
u32 VLibReadUInt32(binary_reader *);
u64 VLibReadUInt64(binary_reader *);

i8  VLibReadInt8 (binary_reader *);
i16 VLibReadInt16(binary_reader *);
i32 VLibReadInt32(binary_reader *);
i64 VLibReadInt64(binary_reader *);

u8 *VLibReadBytes(binary_reader *reader, size_t size);
string VLibReadStringNullTerminated(binary_reader *reader); // @NOTE: Null terminater included in string
string VLibReadString(binary_reader *reader, size_t size);

#if !defined(VLIB_NO_SHORT_NAMES)

#define MakeReader VLibMakeReader
#define ReadUInt8  VLibReadUInt8 
#define ReadUInt16 VLibReadUInt16
#define ReadUInt32 VLibReadUInt32
#define ReadUInt64 VLibReadUInt64
#define ReadInt8  VLibReadInt8 
#define ReadInt16 VLibReadInt16
#define ReadInt32 VLibReadInt32
#define ReadInt64 VLibReadInt64
#define ReadBytes VLibReadBytes
#define ReadStringNullTerminated VLibReadStringNullTerminated
#define ReadString VLibReadString

#endif

#if defined(VLIB_IMPLEMENTATION)

binary_reader VLibMakeReader(u8 *data, size_t size)
{
	binary_reader reader;
	reader.data = data;
	reader.size = size;
	reader.at = 0;
	return reader;
}

string VLibReadStringNullTerminated(binary_reader *reader)
{
	u8 *start = &reader->data[reader->at];
	u8 *scan = start;
	while(*scan) scan++;
	return VLibReadString(reader, scan - start + 1);
}

string VLibReadString(binary_reader *reader, size_t size)
{
	string result = string {.Data=(const char *)&reader->data[reader->at], .Size=size};
	reader->at += size;
	return result;
}

u8 *VLibReadBytes(binary_reader *reader, size_t size)
{
	u8 *result = &reader->data[reader->at];
	reader->at += size;
	return result;
}

u8 VLibReadUInt8 (binary_reader *reader)
{
	u8 result = *VLibReadBytes(reader, 1);
	return result;
}

u16 VLibReadUInt16(binary_reader *reader)
{
	u16 result = *(u16 *)VLibReadBytes(reader, 2);
	return result;
}

u32 VLibReadUInt32(binary_reader *reader)
{
	u32 result = *(u32 *)VLibReadBytes(reader, 4);
	return result;
}

u64 VLibReadUInt64(binary_reader *reader)
{
	u64 result = *(u64 *)VLibReadBytes(reader, 8);
	return result;
}

i8 VLibReadInt8 (binary_reader *reader)
{
	i8 result = *VLibReadBytes(reader, 1);
	return result;
}

i16 VLibReadInt16(binary_reader *reader)
{
	i16 result = *(i16 *)VLibReadBytes(reader, 2);
	return result;
}

i32 VLibReadInt32(binary_reader *reader)
{
	i32 result = *(i32 *)VLibReadBytes(reader, 4);
	return result;
}

i64 VLibReadInt64(binary_reader *reader)
{
	i64 result = *(i64 *)VLibReadBytes(reader, 8);
	return result;
}


#endif

