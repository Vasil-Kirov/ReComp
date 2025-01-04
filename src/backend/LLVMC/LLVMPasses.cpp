#include "LLVMPasses.h"
#include "llvm-c/Transforms/PassBuilder.h"

void RunOptimizationPasses(generator *g, LLVMTargetMachineRef T, int Level, u32 Flags)
{
	if(Level == 0)
		return;

	LLVMPassBuilderOptionsRef pb = LLVMCreatePassBuilderOptions();

	// LLVMPassBuilderOptionsSetLoopInterleaving(
	// 		pb, true);

	// LLVMPassBuilderOptionsSetLoopVectorization(
	// 		pb, true);

	// LLVMPassBuilderOptionsSetSLPVectorization(
	// 		pb, true);

	// LLVMPassBuilderOptionsSetLoopUnrolling(
	// 		pb, true);

	// LLVMPassBuilderOptionsSetForgetAllSCEVInLoopUnroll(
	// 		pb, true);

	// //LLVMPassBuilderOptionsSetLicmMssaOptCap(
	// 		//pb, );

	// //LLVMPassBuilderOptionsSetLicmMssaNoAccForPromotionCap(
	// 		//pb, unsigned LicmMssaNoAccForPromotionCap);

	// LLVMPassBuilderOptionsSetCallGraphProfile(
	// 		pb, true);

	// LLVMPassBuilderOptionsSetMergeFunctions(
			//pb, true);

	//LLVMPassBuilderOptionsSetInlinerThreshold(
	//		pb, 999);

	dynamic<const char *> Passes = {};
	switch(Level)
	{
		// @NOTE: currently not doing optimizations for level 0
		case 0:
		{
			Passes.Push("always-inline");
			Passes.Push("function(annotation-remarks)");
		} break;

		// Os
		case 1:
		{
			Passes.Push(u8R"(
annotation2metadata,
forceattrs,
inferattrs,
function<eager-inv>(
	lower-expect,
	simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
	sroa<modify-cfg>,
	early-cse<>
),
ipsccp,
called-value-propagation,
globalopt,
function<eager-inv>(
	mem2reg,
	instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
	simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>
),
always-inline,
require<globals-aa>,
function(
	invalidate<aa>
),
require<profile-summary>,
cgscc(
	devirt<4>(
		inline,
		function-attrs<skip-non-recursive-function-attrs>,
		function<eager-inv;no-rerun>(
			sroa<modify-cfg>,
			early-cse<memssa>,
			speculative-execution<only-if-divergent-target>,
			jump-threading,
			correlated-propagation,
			simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
			instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
			aggressive-instcombine,
			tailcallelim,
			simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
			reassociate,
			constraint-elimination,
			loop-mssa(
				loop-instsimplify,
				loop-simplifycfg,
				licm<no-allowspeculation>,
				loop-rotate<header-duplication;no-prepare-for-lto>,
				licm<allowspeculation>,
				simple-loop-unswitch<no-nontrivial;trivial>
			),
			simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
			instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
			loop(
				loop-idiom,
				indvars,
				loop-deletion,
				loop-unroll-full
			),
			sroa<modify-cfg>,
			vector-combine,
			mldst-motion<no-split-footer-bb>,
			gvn<>,
			sccp,
			bdce,
			instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
			jump-threading,
			correlated-propagation,
			adce,
			memcpyopt,
			dse,
			move-auto-init,
			loop-mssa(
			licm<allowspeculation>
			),
				simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts;speculate-blocks;simplify-cond-branch>,
				instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>
					),
				function-attrs,
				function(
						require<should-not-run-function-passes>
						)
					)
					),
				deadargelim,
				globalopt,
				globaldce,
				elim-avail-extern,
				rpo-function-attrs,
				recompute-globalsaa,
				function<eager-inv>(
						float2int,
						lower-constant-intrinsics,
						loop(
							loop-rotate<header-duplication;no-prepare-for-lto>,
							loop-deletion
							),
						loop-distribute,
						inject-tli-mappings,
						loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>,
						infer-alignment,
						loop-load-elim,
						instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
						simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts;speculate-blocks;simplify-cond-branch>,
						slp-vectorizer,
						vector-combine,
						instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
						loop-unroll<O2>,
						transform-warning,
						sroa<preserve-cfg>,
						infer-alignment,
						instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
						loop-mssa(
								licm<allowspeculation>
								),
						alignment-from-assumptions,
						loop-sink,
						instsimplify,
						div-rem-pairs,
						tailcallelim,
						simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>
							),
						globaldce,
						constmerge,
						cg-profile,
						rel-lookup-table-converter,
						function(
								annotation-remarks
								),
						verify
							)");
			} break;
			// Max optimizations
			case 2:
			case 3:
			case 4:
			{
				Passes.Push(u8R"(
annotation2metadata,
forceattrs,
inferattrs,
function<eager-inv>(
	lower-expect,
	simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;no-switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
	sroa<modify-cfg>,
	early-cse<>,
	callsite-splitting
),
ipsccp,
called-value-propagation,
globalopt,
function<eager-inv>(
	mem2reg,
	instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
	simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>
),
always-inline,
require<globals-aa>,
function(invalidate<aa>),
require<profile-summary>,
cgscc(
	devirt<4>(
		inline,
		function-attrs<skip-non-recursive-function-attrs>,
		argpromotion,
		function<eager-inv;no-rerun>(
			sroa<modify-cfg>,
			early-cse<memssa>,
			speculative-execution<only-if-divergent-target>,
			jump-threading,
			correlated-propagation,
			simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
			instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
			aggressive-instcombine,
			libcalls-shrinkwrap,
			tailcallelim,
			simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
			reassociate,
			constraint-elimination,
			loop-mssa(
				loop-instsimplify,
				loop-simplifycfg,
				licm<no-allowspeculation>,
				loop-rotate<header-duplication;no-prepare-for-lto>,
				licm<allowspeculation>,
				simple-loop-unswitch<nontrivial;trivial>
			),
			simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>,
			instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
			loop(
				loop-idiom,
				indvars,
				loop-deletion,
				loop-unroll-full
			),
			sroa<modify-cfg>,
			vector-combine,
			mldst-motion<no-split-footer-bb>,
			gvn<>,
			sccp,
			bdce,
			instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
			jump-threading,
			correlated-propagation,
			adce,
			memcpyopt,
			dse,
			move-auto-init,
				loop-mssa(licm<allowspeculation>),
					simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;hoist-common-insts;sink-common-insts;speculate-blocks;simplify-cond-branch>,
					instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>
						),
					function-attrs,
					function(
							require<should-not-run-function-passes>
							)
						)
						),
					deadargelim,
					globalopt,
					globaldce,
					elim-avail-extern,
					rpo-function-attrs,
					recompute-globalsaa,
					function<eager-inv>(
							float2int,
							lower-constant-intrinsics,
							chr,
							loop(
								loop-rotate<header-duplication;no-prepare-for-lto>,
								loop-deletion
								),
							loop-distribute,
							inject-tli-mappings,
							loop-vectorize<no-interleave-forced-only;no-vectorize-forced-only;>,
							infer-alignment,
							loop-load-elim,
							instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
							simplifycfg<bonus-inst-threshold=1;forward-switch-cond;switch-range-to-icmp;switch-to-lookup;no-keep-loops;hoist-common-insts;sink-common-insts;speculate-blocks;simplify-cond-branch>,
							slp-vectorizer,
							vector-combine,
							instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
							loop-unroll<O3>,
							transform-warning,
							sroa<preserve-cfg>,
							infer-alignment,
							instcombine<max-iterations=1;no-use-loop-info;no-verify-fixpoint>,
							loop-mssa(licm<allowspeculation>),
							alignment-from-assumptions,
							loop-sink,
							instsimplify,
							div-rem-pairs,
							tailcallelim,
							simplifycfg<bonus-inst-threshold=1;no-forward-switch-cond;switch-range-to-icmp;no-switch-to-lookup;keep-loops;no-hoist-common-insts;no-sink-common-insts;speculate-blocks;simplify-cond-branch>
								),
							globaldce,
							constmerge,
							cg-profile,
							rel-lookup-table-converter,
							function(
									annotation-remarks
									),
							verify
								)");
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
	}

	B.Data.Free();
	LLVMConsumeError(err);
	LLVMDisposePassBuilderOptions(pb);
}
