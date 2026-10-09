#pragma once

#include "VString.h"
enum TestCheckResult {
	TestCheck_Ok,
	TestCheck_NotOk,
	TestCheck_NotComparable,
};

struct test
{
	string Name;
	int TestCounter;
	int TestOk;
	int TestErr;
	void PrintNotOkMessage(string Error);
	void PrintOkMessage();
	void StartTest(string TestName);
	void EndTest();
	TestCheckResult CheckEq(struct value *A, value *B);
};

