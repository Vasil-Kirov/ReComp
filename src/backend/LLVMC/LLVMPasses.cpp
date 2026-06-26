#include "LLVMPasses.h"
#include "llvm-c/Transforms/PassBuilder.h"

void RunOptimizationPasses(generator *g, LLVMTargetMachineRef T, int Level, u32 Flags)
{
	//if(Level == 0)
		//return;

	LLVMPassBuilderOptionsRef pb = LLVMCreatePassBuilderOptions();

	if(Level >= 2)
	{
		LLVMPassBuilderOptionsSetLoopInterleaving(
				pb, true);

		LLVMPassBuilderOptionsSetLoopVectorization(
				pb, true);

		LLVMPassBuilderOptionsSetSLPVectorization(
				pb, true);

		LLVMPassBuilderOptionsSetLoopUnrolling(
				pb, true);

		LLVMPassBuilderOptionsSetForgetAllSCEVInLoopUnroll(
				pb, true);

		//LLVMPassBuilderOptionsSetLicmMssaOptCap(
		//pb, );

		//LLVMPassBuilderOptionsSetLicmMssaNoAccForPromotionCap(
		//pb, unsigned LicmMssaNoAccForPromotionCap);

		LLVMPassBuilderOptionsSetCallGraphProfile(
				pb, true);

		if((Flags & CF_DebugInfo) == 0)
		{
			LLVMPassBuilderOptionsSetMergeFunctions(
					pb, true);
		}

		//LLVMPassBuilderOptionsSetInlinerThreshold(
				//pb, 999);
	}

	dynamic<const char *> Passes = {};
	switch(Level)
	{
		case 0:
		{
			Passes.Push("default<O0>");
			//Passes.Push("always-inline");
			//Passes.Push("function(annotation-remarks)");
		} break;

		// Os
		case 1:
		{
			Passes.Push("default<Os>");
		} break;
		// O2
		case 2:
		{
			Passes.Push("default<O2>");
		} break;
		// O3
		default:
		{
			if(Level < 3)
				break;
			Passes.Push("default<O3>");
		} break;
	}

	if(Flags & CF_SanAdress)
	{
		Passes.Push("asan");
	}

	if(Flags & CF_SanMemory)
	{
		Passes.Push("msan");
	}

	if(Flags & CF_SanThread)
	{
		Passes.Push("tsan");
	}

	if(Flags & CF_SanUndefined)
	{
		Passes.Push("ubsan");
	}

	string_builder B = MakeBuilder();
	ForArray(Idx, Passes)
	{
		if(Idx != 0) {
			PushBuilder(&B, ',');
		}
		PushBuilder(&B, Passes[Idx]);
	}

	for (size_t i = 0; i < B.Size; /**/) {
		switch (B.Data[i]) {
			case ' ':
			case '\n':
			case '\t':
			memmove(&B.Data.Data[i], &B.Data.Data[i+1], B.Size-i);
			B.Size--;
			continue;
			default:
			i += 1;
			break;
		}
	}

	LLVMErrorRef err = LLVMRunPasses(g->mod, B.Data.Data, T, pb);
	if(err)
	{
		char *msg = LLVMGetErrorMessage(err);
		LFATAL("--- Running Passes Failed ---\n\t%s", msg);
		LLVMDisposeErrorMessage(msg);
	}
	else
	{
		LLVMConsumeError(err);
	}

	B.Data.Free();
	LLVMDisposePassBuilderOptions(pb);
}

