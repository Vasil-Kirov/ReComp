#pragma once
#include "IR.h"
#include "Interpreter.h"
#include "Parser.h"


node *ParseQuote(parser *Parser);
u32 BuildIRFromQuote(block_builder *Builder, node *Node);
interp_slice *ExecuteQuote(interpreter *VM, ir_quote *Quote);

