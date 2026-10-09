#include "Test.h"
#include "Platform.h"
#include "Type.h"
#include "Interpreter.h"

void test::StartTest(string Name)
{
	this->Name = Name;
	this->TestCounter = 0;
	this->TestOk = 0;
	this->TestErr = 0;

	auto b = MakeBuilder();
	b.printf("Test \"%.*s\"\n", (int)Name.Size, Name.Data);
	array<u8> Mem = {b.Data.Count+1};
	string Print = MakeString(b, Mem.Data);

	LogLockMutex();
	PlatformOutputString(Print, LOG_CLEAN);
	LogUnlockMutex();

	Mem.Free();
}

void test::PrintOkMessage()
{
	TestCounter++;
	TestOk++;

	auto b = MakeBuilder();
	b.printf("    Test #%-3d %-10s\n", TestCounter, "[  OK  ]");

	array<u8> Mem = {b.Data.Count+1};
	string Print = MakeString(b, Mem.Data);

	LogLockMutex();
	string Start = SliceString(Print, 0, 13);
	string End = SliceString(Print, 13, 0);
	PlatformOutputString(Start, LOG_CLEAN);
	PlatformOutputString(End,   LOG_DEBUG /* Green */);
	LogUnlockMutex();

	Mem.Free();
}

void test::PrintNotOkMessage(string Error)
{
	TestCounter++;
	TestErr++;

	auto b = MakeBuilder();
	b.printf("    Test #%-3d %-10s%.*s\n", TestCounter, "[ FAIL ] -> ", (int)Error.Size, Error.Data);

	array<u8> Mem = {b.Data.Count+1};
	string Print = MakeString(b, Mem.Data);

	LogLockMutex();
	string Start = SliceString(Print, 0, 13);
	string End = SliceString(Print, 13, 0);
	PlatformOutputString(Start, LOG_CLEAN);
	PlatformOutputString(End,   LOG_ERROR /* Red */);
	LogUnlockMutex();

	Mem.Free();
}

void test::EndTest()
{

	auto b = MakeBuilder();
	b.printf("Results for %.*s -> %d OK, %d FAILED\n", (int)Name.Size, Name.Data, TestOk, TestErr);

	array<u8> Mem = {b.Data.Count+1};
	string Print = MakeString(b, Mem.Data);

	LogLockMutex();
	PlatformOutputString(Print, LOG_CLEAN);
	LogUnlockMutex();

	Mem.Free();
}

TestCheckResult test::CheckEq(value *A, value *B)
{
	if (A->Type != B->Type) {
		auto b = MakeBuilder();
		b.printf("Cannot compare types %s and %s", GetTypeName(A->Type), GetTypeName(B->Type));
		PrintNotOkMessage(MakeString(b));
		return TestCheck_NotComparable;
	}

	auto b = MakeBuilder();

	value Result = {};
	Result.Type = Basic_bool;
	const type *Type = GetType(A->Type);
	if (Type->Kind == TypeKind_Enum)
	{
		Type = GetType(Type->Enum.Type);
	}
	if (Type->Kind == TypeKind_Basic)
	{
		switch (Type->Basic.Kind)
		{
			case Basic_bool:
			case Basic_u8:
			{
				Result.u8 = A->u8 == B->u8;
				b.printf("%d == %d", A->u8, B->u8);
			} break;
			case Basic_u16:
			{
				Result.u8 = A->u16 == B->u16;
				b.printf("%d == %d", A->u16, B->u16);
			} break;
			case Basic_u32:
			{
				Result.u8 = A->u32 == B->u32;
				b.printf("%d == %d", A->u32, B->u32);
			} break;
			case Basic_u64:
			case Basic_uint:
			{
				Result.u8 = A->u64 == B->u64;
				b.printf("%llu == %llu", A->u64, B->u64);
			} break;
			case Basic_i8:
			{
				Result.u8 = A->i8 == B->i8;
				b.printf("%d == %d", A->i8, B->i8);
			} break;
			case Basic_i16:
			{
				Result.u8 = A->i16 == B->i16;
				b.printf("%d == %d", A->i16, B->i16);
			} break;
			case Basic_i32:
			{
				Result.u8 = A->i32 == B->i32;
				b.printf("%d == %d", A->i32, B->i32);
			} break;
			case Basic_i64:
			case Basic_type:
			case Basic_int:
			{
				Result.u8 = A->i64 == B->i64;
				b.printf("%lld == %lld", A->i64, B->i64);
			} break;
			case Basic_f32:
			{
				Result.u8 = A->f32 == B->f32;
				b.printf("%f == %f", A->f32, B->f32);
			} break;
			case Basic_f64:
			{
				Result.u8 = A->f64 == B->f64;
				b.printf("%f == %f", A->f64, B->f64);
			} break;
			default:
			{
				auto b = MakeBuilder();
				b.printf("Cannot compare types %s and %s", GetTypeName(A->Type), GetTypeName(B->Type));
				PrintNotOkMessage(MakeString(b));
				return TestCheck_NotComparable;
			} break;
		}
	}
	else if (Type->Kind == TypeKind_Pointer)
	{
		Result.u8 = A->ptr == B->ptr;
		b.printf("%p == %p", A->ptr, B->ptr);
	}
	else
	{
		auto b = MakeBuilder();
		b.printf("Cannot compare types %s and %s", GetTypeName(A->Type), GetTypeName(B->Type));
		PrintNotOkMessage(MakeString(b));
		return TestCheck_NotComparable;
	}

	if (Result.u8) {
		b.Data.Free();
		PrintOkMessage();
		return TestCheck_Ok;
	}

	PrintNotOkMessage(MakeString(b));
	return TestCheck_NotOk;
}

