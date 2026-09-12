#include "CommandLine.h"
#include "Lexer.h"
#include "Parser.h"
#include "DumpInfo.h"

bool WalkWriteData(node *Node, void *Arg)
{
	auto bb = (binary_blob *)Arg;
	switch (Node->Type)
	{
		
	}
}

extern dynamic<string> ConfigIDs;
void ParseText(parse_params Params)
{
	error_info ErrorInfo = {};
	file *f = StringToTokens(Params.Text, ErrorInfo);
	parse_result pr = ParseTokens(f, SliceFromArray(ConfigIDs));

	binary_blob bb = StartOutput();

	for (auto node : pr.Nodes)
		WalkASTNode(node, WalkWriteData, &bb);
}



