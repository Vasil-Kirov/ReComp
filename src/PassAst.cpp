#include "PassAst.h"
#include "Pipeline.h"

InterpNode *NodeToInterp(node *N);
interp_string StringToInterp(const string *S);

interp_slice StringSliceToInterp(slice<string> strs)
{
	array<interp_string> Array(strs.Count);
	size_t At = 0;
	for(string s : strs)
	{
		Array[At++] = StringToInterp(&s);
	}
	return interp_slice {Array.Count, Array.Data};
}

interp_string StringToInterp(const string *S)
{
	if(!S)
		return interp_string{};
	return interp_string {S->Size, S->Data};
}

const string *StringFromInterpPtr(interp_string S)
{
	string r = StringFromInterp(S);
	return DupeType(r, string);
}

interp_slice NodeToInterpSlice(slice<node *> Nodes)
{
	array<InterpNode*> Array(Nodes.Count);
	size_t At = 0;
	for(node *Node : Nodes)
	{
		Array[At++] = NodeToInterp(Node);
	}
	return interp_slice {Array.Count, Array.Data};
}

InterpNode *NodeToInterp(node *N)
{
	if(!N || N == (node *)0x1)
		return nullptr;
	InterpNode *R = NewType(InterpNode);
	string FileName = string { N->ErrorInfo->FileName, strlen(N->ErrorInfo->FileName)};
	R->location.file = StringToInterp(&FileName);
	R->location.line = N->ErrorInfo->Range.StartLine;
	R->location.chr = N->ErrorInfo->Range.StartChar;

	R->t = N->Type;
	switch (N->Type)
	{
		case AST_INVALID: 
			unreachable; 
			break;

		case AST_SLICE:
		{
			R->slice.operand = NodeToInterp(N->Slice.Operand);
			R->slice.from = NodeToInterp(N->Slice.From);
			R->slice.to = NodeToInterp(N->Slice.To);
		} break;

		case AST_RUN:
		{
			R->run.body = NodeToInterpSlice(N->Run.Body);
			//R->run.TypeIdx = N->Run.TypeIdx;
			R->run.is_expr_run = N->Run.IsExprRun;
		} break;

		case AST_YIELD:
		{
			R->typed_expr.expr = NodeToInterp(N->TypedExpr.Expr);
			//R->typed_expr.T = N->TypedExpr.TypeIdx;
		} break;

		case AST_USING:
		{
			R->typed_expr.expr = NodeToInterp(N->TypedExpr.Expr);
		} break;

		case AST_ASSERT:
		{
			R->assert_.expr = NodeToInterp(N->Assert.Expr);
		} break;

		case AST_VAR:
		{
			R->var.name = StringToInterp(N->Var.Name);
			//R->var.Type = N->Var.Type;
			R->var.type_node = NodeToInterp(N->Var.TypeNode);
			R->var.default_ = NodeToInterp(N->Var.Default);
		} break;

		case AST_LIST:
		{
			R->list.nodes = NodeToInterpSlice(N->List.Nodes);
		} break;

		case AST_EMBED:
		{
			R->embed.is_string = N->Embed.IsString;
			R->embed.content   = StringToInterp(&N->Embed.Content);
			R->embed.file_name = StringToInterp(N->Embed.FileName);
		} break;

		case AST_CHARLIT:
		{
			R->char_literal.C = N->CharLiteral.C;
		} break;

		case AST_CONSTANT:
		{
			R->constant.value = N->Constant.Value;
			//R->constant.type = N->Constant.Type;
		} break;

		case AST_BINARY:
		{
			R->binary.left = NodeToInterp(N->Binary.Left);
			R->binary.right = NodeToInterp(N->Binary.Right);
			R->binary.op = N->Binary.Op;
			//R->binary.expressionType = N->Binary.ExpressionType;
		} break;

		case AST_UNARY:
		{
			R->unary.operand = NodeToInterp(N->Unary.Operand);
			R->unary.op = N->Unary.Op;
			//R->Unary.Type = N->Unary.Type;
		} break;

		case AST_IFX:
		{
			R->if_x.expr = NodeToInterp(N->IfX.Expr);
			R->if_x.true_ = NodeToInterp(N->IfX.True);
			R->if_x.false_ = NodeToInterp(N->IfX.False);
			//R->if_x.TypeIdx = N->IfX.TypeIdx;
		} break;

		case AST_IF:
		{
			R->if_.expression = NodeToInterp(N->If.Expression);
			R->if_.body = NodeToInterpSlice(SliceFromArray(N->If.Body));
			R->if_.else_ = NodeToInterpSlice(SliceFromArray(N->If.Else));
		} break;

		case AST_FOR:
		{
			R->for_.expr1 = NodeToInterp(N->For.Expr1);
			R->for_.expr2 = NodeToInterp(N->For.Expr2);
			R->for_.expr3 = NodeToInterp(N->For.Expr3);
			R->for_.body = NodeToInterpSlice(SliceFromArray(N->For.Body));
			R->for_.kind = N->For.Kind;
			//R->for_.ArrayType = N->For.ArrayType;
			//R->for_.ItType = N->For.ItType;
			R->for_.it_by_ref = N->For.ItByRef;
		} break;

		case AST_ID:
		{
			R->id.name = StringToInterp(N->ID.Name);
			//R->id.Type = N->ID.Type;
		} break;

		case AST_DECL:
		{
			R->decl.lhs = NodeToInterp(N->Decl.LHS);
			R->decl.expression = NodeToInterp(N->Decl.Expression);
			R->decl.t = NodeToInterp(N->Decl.Type);
			//R->decl.TypeIndex = N->Decl.TypeIndex;
			R->decl.flags = N->Decl.Flags;
			R->decl.link_name = StringToInterp(N->Decl.LinkName);
		} break;

		case AST_CALL:
		{
			R->call.fn_ = NodeToInterp(N->Call.Fn);
			R->call.args = NodeToInterpSlice(N->Call.Args);
			R->call.sym_name = StringToInterp(&N->Call.SymName);
			//R->Call.Type = N->Call.Type;
			//R->Call.ArgTypes = N->Call.ArgTypes; // Shallow-copied
		} break;

		case AST_RETURN:
		{
			R->return_.expr = NodeToInterp(N->Return.Expression);
			//R->Return.TypeIdx = N->Return.TypeIdx;
		} break;

		case AST_PTRTYPE:
		{
			R->pointer_type.pointed = NodeToInterp(N->PointerType.Pointed);
			R->pointer_type.flags = N->PointerType.Flags;
		} break;

		case AST_ARRAYTYPE:
		{
			R->array_type.t_node = NodeToInterp(N->ArrayType.Type);
			R->array_type.expression = NodeToInterp(N->ArrayType.Expression);
		} break;

		case AST_FN:
		{
			R->fn_.Name     = StringToInterp(N->Fn.Name);
			R->fn_.LinkName = StringToInterp(N->Fn.LinkName);
			R->fn_.Tag = StringToInterp(N->Fn.Tag);
			R->fn_.WasmModule = StringToInterp(N->Fn.WasmModule);
			R->fn_.WasmName = StringToInterp(N->Fn.WasmName);
			R->fn_.args = NodeToInterpSlice(N->Fn.Args);
			R->fn_.return_types = NodeToInterpSlice(N->Fn.ReturnTypes);
			R->fn_.Body = NodeToInterpSlice(SliceFromArray(N->Fn.Body));
			//R->fn_.TypeIdx = N->Fn.TypeIdx;
			R->fn_.flags = N->Fn.Flags;
			//R->fn_.FnModule = N->Fn.FnModule;
			//R->fn_.ProfileCallback = NodeToInterp(N->Fn.ProfileCallback);
			//R->fn_.CallbackType = N->Fn.CallbackType;
		} break;

		case AST_CAST:
		{
			R->cast_.expression = NodeToInterp(N->Cast.Expression);
			R->cast_.type_node = NodeToInterp(N->Cast.TypeNode);
			//R->cast_.FromType = N->Cast.FromType;
			//R->cast_.ToType = N->Cast.ToType;
			R->cast_.is_bit_cast = N->Cast.IsBitCast;
		} break;

		case AST_TYPELIST:
		{
			R->type_list.type_node = NodeToInterp(N->TypeList.TypeNode);
			R->type_list.items = NodeToInterpSlice(N->TypeList.Items);
			//R->type_list.Type = N->TypeList.Type;
		} break;

		case AST_INDEX:
		{
			R->index.operand = NodeToInterp(N->Index.Operand);
			R->index.expression = NodeToInterp(N->Index.Expression);
			//R->index.OperandType = N->Index.OperandType;
			//R->index.IndexedType = N->Index.IndexedType;
			//R->index.ForceNotLoad = N->Index.ForceNotLoad;
		} break;

		case AST_STRUCTDECL:
		{
			R->struct_decl.name = StringToInterp(N->StructDecl.Name);
			R->struct_decl.members = NodeToInterpSlice(N->StructDecl.Members);
			R->struct_decl.is_union = N->StructDecl.IsUnion;
			R->struct_decl.type_params = StringSliceToInterp(N->StructDecl.TypeParams);
			//R->struct_decl.IsError = N->StructDecl.IsError;
		} break;

		case AST_ENUM:
		{
			R->enum_.name = StringToInterp(N->Enum.Name);
			R->enum_.items = NodeToInterpSlice(N->Enum.Items);
			R->enum_.type_node = NodeToInterp(N->Enum.Type);
		} break;

		case AST_SELECTOR:
		{
			R->selector.operand = NodeToInterp(N->Selector.Operand);
			R->selector.member = StringToInterp(N->Selector.Member);
			R->selector.index = N->Selector.Index;
			R->selector.sub_index = N->Selector.SubIndex;
			//R->selector.type = N->Selector.Type;
		} break;

		case AST_SIZE:
		{
			R->size_.expression = NodeToInterp(N->Size.Expression);
			//R->size_.type = N->Size.Type;
		} break;

		case AST_TYPEOF:
		{
			R->type_of_.expression = NodeToInterp(N->TypeOf.Expression);
			//R->type_of_.Type = N->TypeOf.Type;
		} break;

		case AST_GENERIC:
		{
			R->generic.name = StringToInterp(N->Generic.Name);
		} break;

		case AST_RESERVED:
		{
			R->reserved.id = N->Reserved.ID;
			//R->reserved.Type = N->Reserved.Type;
		} break;

		case AST_NOP:
		case AST_BREAK:
		case AST_CONTINUE:
			// No additional data to copy
			break;

		case AST_GENSTRUCTTYPE:
		{
			R->generic_struct_type.args = NodeToInterpSlice(N->GenericStructType.Args);
			R->generic_struct_type.id = NodeToInterp(N->GenericStructType.ID);
			//R->generic_struct_type.Analyzed = N->GenericStructType.Analyzed;
		} break;

		case AST_LISTITEM:
		{
			R->item.name = StringToInterp(N->Item.Name);
			R->item.expression = NodeToInterp(N->Item.Expression);
		} break;

		case AST_SWITCH:
		{
			R->switch_.expression = NodeToInterp(N->Switch.Expression);
			R->switch_.cases = NodeToInterpSlice(N->Switch.Cases);
			//R->switch_.SwitchType = N->Switch.SwitchType;
			//R->switch_.ReturnType = N->Switch.ReturnType;
		} break;

		case AST_CASE:
		{
			R->case_.value = NodeToInterp(N->Case.Value);
			R->case_.body = NodeToInterpSlice(N->Case.Body);
		} break;

		case AST_POSTOP:
		{
			R->post_op.operand = NodeToInterp(N->PostOp.Operand);
			R->post_op.op = N->PostOp.Type;
			//R->post_op.typeIdx = N->PostOp.TypeIdx;
		} break;

		case AST_DEFER:
		{
			R->defer_.body = NodeToInterpSlice(N->Defer.Body);
		} break;

		case AST_SCOPE:
		{
			R->scope_delimiter.is_up = N->ScopeDelimiter.IsUp;
		} break;

		case AST_TYPEINFO:
		{
			//R->type_info_lookup.Type = N->TypeInfoLookup.Type;
			R->type_info_lookup.expression = NodeToInterp(N->TypeInfoLookup.Expression);
		} break;

		case AST_PTRDIFF:
		{
			R->ptr_diff.left = NodeToInterp(N->PtrDiff.Left);
			R->ptr_diff.right = NodeToInterp(N->PtrDiff.Right);
			//R->ptr_diff.Type = N->PtrDiff.Type;
		} break;

		case AST_FILE_LOCATION:
		{
		} break;
	}
	return R;
}

node *InterpToNode(const InterpNode *R, dict<const string *> FileContents);
slice<node *> InterpSliceToNode(interp_slice Nodes, dict<const string *> FileContents)
{
	array<node*> Array(Nodes.Count);
	size_t At = 0;
	for(int i = 0; i < Nodes.Count; ++i)
	{
		Array[At++] = InterpToNode(((InterpNode **)Nodes.Data)[i], FileContents);
	}
	return SliceFromArray(Array);
}

dynamic<node *> InterpSliceToNodeDynamic(interp_slice Nodes, dict<const string *> FileContents)
{
	dynamic<node *> r = {};
	for(int i = 0; i < Nodes.Count; ++i)
	{
		node *N = InterpToNode(((InterpNode **)Nodes.Data)[i], FileContents);
		if(N)
			r.Push(N);
	}
	return r;
}

node *InterpToNode(const InterpNode *R, dict<const string *> FileContents)
{
	if(!R)
		return nullptr;
	
	// @TODO: FIX THIS, it can lead to a lot of duplicate reads and saving the same file's
	// contents into memory over and over again
	string FileName = StringFromInterp(R->location.file);
	const string **Content = FileContents.GetUnstablePtr(FileName);
	const string *f = NULL;
	if(!Content)
	{
		string Found = FindFile(FileName);
		if(Found == "")
		{
			LogCompilerError("Source file in custom module node is not a valid path to a file: %.*s", R->location.file.Count, R->location.file.Data);
			CountError();
			return nullptr;
		}
		string ReadContent = ReadEntireFile(Found);
		if(ReadContent.Data == NULL)
		{
			LogCompilerError("Source file in custom module node is not readable: %.*s", R->location.file.Count, R->location.file.Data);
			CountError();
			return nullptr;
		}
		string *NewContent = DupeType(ReadContent, string);
		FileContents.Add(FileName, NewContent);
		f = NewContent;
	}
	else
		f = *Content;
	node *N = NewType(node);
	N->ErrorInfo = CreateErrorInfoFromInterpLocation(R->location, StringFromInterp(R->location.file), f);

	N->Type= R->t;
	switch (R->t)
	{
		case AST_INVALID: 
			unreachable; 
			break;

		case AST_SLICE:
		{
			N->Slice.Operand = InterpToNode(R->slice.operand, FileContents);
			N->Slice.From = InterpToNode(R->slice.from, FileContents);
			N->Slice.To = InterpToNode(R->slice.to, FileContents);
		} break;

		case AST_RUN:
		{
			N->Run.Body = InterpSliceToNode(R->run.body, FileContents);
			//R->run.TypeIdx = N->Run.TypeIdx;
			N->Run.IsExprRun = R->run.is_expr_run;
		} break;

		case AST_YIELD:
		{
			N->TypedExpr.Expr = InterpToNode(R->typed_expr.expr, FileContents);
			//R->typed_expr.T = N->TypedExpr.TypeIdx;
		} break;

		case AST_USING:
		{
			N->TypedExpr.Expr = InterpToNode(R->typed_expr.expr, FileContents);
		} break;

		case AST_ASSERT:
		{
			N->Assert.Expr = InterpToNode(R->assert_.expr, FileContents);
		} break;

		case AST_VAR:
		{
			N->Var.Name = StringFromInterpPtr(R->var.name);
			//R->var.Type = N->Var.Type;
			N->Var.TypeNode = InterpToNode(R->var.type_node, FileContents);
			N->Var.Default = InterpToNode(R->var.default_, FileContents);
		} break;

		case AST_LIST:
		{
			N->List.Nodes = InterpSliceToNode(R->list.nodes, FileContents);
		} break;

		case AST_EMBED:
		{
			N->Embed.IsString = R->embed.is_string;
			N->Embed.Content  = StringFromInterp(R->embed.content  );
			N->Embed.FileName = StringFromInterpPtr(R->embed.file_name);
		} break;

		case AST_CHARLIT:
		{
			N->CharLiteral.C = R->char_literal.C;
		} break;

		case AST_CONSTANT:
		{
			N->Constant.Value = R->constant.value;
			//R->constant.type = N->Constant.Type;
		} break;

		case AST_BINARY:
		{
			N->Binary.Left = InterpToNode(R->binary.left, FileContents);
			N->Binary.Right = InterpToNode(R->binary.right, FileContents);
			N->Binary.Op = (token_type)R->binary.op;
			//R->binary.expressionType = N->Binary.ExpressionType;
		} break;

		case AST_UNARY:
		{
			N->Unary.Operand = InterpToNode(R->unary.operand, FileContents);
			N->Unary.Op = (token_type)R->unary.op;
			//R->Unary.Type = N->Unary.Type;
		} break;

		case AST_IFX:
		{
			N->IfX.Expr = InterpToNode(R->if_x.expr, FileContents);
			N->IfX.True = InterpToNode(R->if_x.true_, FileContents);
			N->IfX.False = InterpToNode(R->if_x.false_, FileContents);
			//R->if_x.TypeIdx = N->IfX.TypeIdx;
		} break;

		case AST_IF:
		{
			N->If.Expression = InterpToNode(R->if_.expression, FileContents);
			N->If.Body = InterpSliceToNodeDynamic(R->if_.body, FileContents);
			N->If.Else = InterpSliceToNodeDynamic(R->if_.else_, FileContents);
		} break;

		case AST_FOR:
		{
			N->For.Expr1 = InterpToNode(R->for_.expr1, FileContents);
			N->For.Expr2 = InterpToNode(R->for_.expr2, FileContents);
			N->For.Expr3 = InterpToNode(R->for_.expr3, FileContents);
			N->For.Body  = InterpSliceToNodeDynamic(R->for_.body, FileContents);
			N->For.Kind = R->for_.kind;
			//R->for_.ArrayType = N->For.ArrayType;
			//R->for_.ItType = N->For.ItType;
			N->For.ItByRef = R->for_.it_by_ref;
		} break;

		case AST_ID:
		{
			N->ID.Name = StringFromInterpPtr(R->id.name);
			//R->id.Type = N->ID.Type;
		} break;

		case AST_DECL:
		{
			N->Decl.LHS = InterpToNode(R->decl.lhs, FileContents);
			N->Decl.Expression = InterpToNode(R->decl.expression, FileContents);
			N->Decl.Type = InterpToNode(R->decl.t, FileContents);
			//R->decl.TypeIndex = N->Decl.TypeIndex;
			N->Decl.Flags = R->decl.flags;
			N->Decl.LinkName = StringFromInterpPtr(R->decl.link_name);
		} break;

		case AST_CALL:
		{
			N->Call.Fn = InterpToNode(R->call.fn_, FileContents);
			N->Call.Args = InterpSliceToNode(R->call.args, FileContents);
			N->Call.SymName = StringFromInterp(R->call.sym_name);
			//R->Call.Type = N->Call.Type;
			//R->Call.ArgTypes = N->Call.ArgTypes; // Shallow-copied
		} break;

		case AST_RETURN:
		{
			N->Return.Expression = InterpToNode(R->return_.expr, FileContents);
			//R->Return.TypeIdx = N->Return.TypeIdx;
		} break;

		case AST_PTRTYPE:
		{
			N->PointerType.Pointed = InterpToNode(R->pointer_type.pointed, FileContents);
			N->PointerType.Flags = R->pointer_type.flags;
		} break;

		case AST_ARRAYTYPE:
		{
			N->ArrayType.Type = InterpToNode(R->array_type.t_node, FileContents);
			N->ArrayType.Expression = InterpToNode(R->array_type.expression, FileContents);
		} break;

		case AST_FN:
		{
			N->Fn.Name = StringFromInterpPtr(R->fn_.Name    );
			N->Fn.LinkName = StringFromInterpPtr(R->fn_.LinkName);
			N->Fn.Tag = StringFromInterpPtr(R->fn_.Tag);
			N->Fn.WasmModule = StringFromInterpPtr(R->fn_.WasmModule);
			N->Fn.WasmName = StringFromInterpPtr(R->fn_.WasmName);
			N->Fn.Args = InterpSliceToNode(R->fn_.args, FileContents);
			N->Fn.ReturnTypes = InterpSliceToNode(R->fn_.return_types, FileContents);
			N->Fn.Body = InterpSliceToNodeDynamic(R->fn_.Body, FileContents);
			//R->fn_.TypeIdx = N->Fn.TypeIdx;
			N->Fn.Flags = R->fn_.flags;
			//R->fn_.FnModule = N->Fn.FnModule;
			//R->fn_.ProfileCallback = NodeToInterp(N->Fn.ProfileCallback);
			//R->fn_.CallbackType = N->Fn.CallbackType;
		} break;

		case AST_CAST:
		{
			N->Cast.Expression = InterpToNode(R->cast_.expression, FileContents);
			N->Cast.TypeNode = InterpToNode(R->cast_.type_node, FileContents);
			//R->cast_.FromType = N->Cast.FromType;
			//R->cast_.ToType = N->Cast.ToType;
			N->Cast.IsBitCast = R->cast_.is_bit_cast;
		} break;

		case AST_TYPELIST:
		{
			N->TypeList.TypeNode = InterpToNode(R->type_list.type_node, FileContents);
			N->TypeList.Items = InterpSliceToNode(R->type_list.items, FileContents);
			//R->type_list.Type = N->TypeList.Type;
		} break;

		case AST_INDEX:
		{
			N->Index.Operand = InterpToNode(R->index.operand, FileContents);
			N->Index.Expression = InterpToNode(R->index.expression, FileContents);
			//R->index.OperandType = N->Index.OperandType;
			//R->index.IndexedType = N->Index.IndexedType;
			//R->index.ForceNotLoad = N->Index.ForceNotLoad;
		} break;

		case AST_STRUCTDECL:
		{
			N->StructDecl.Name = StringFromInterpPtr(R->struct_decl.name);
			N->StructDecl.Members = InterpSliceToNode(R->struct_decl.members, FileContents);
			N->StructDecl.IsUnion = R->struct_decl.is_union;

			array<string> TypeParams(R->struct_decl.type_params.Count);
			for(int i = 0; i < R->struct_decl.type_params.Count; ++i)
			{
				TypeParams[i] = StringFromInterp(((interp_string *)R->struct_decl.type_params.Data)[i]);
			}
			N->StructDecl.TypeParams = SliceFromArray(TypeParams);
			//R->struct_decl.IsError = N->StructDecl.IsError;
		} break;

		case AST_ENUM:
		{
			N->Enum.Name = StringFromInterpPtr(R->enum_.name);
			N->Enum.Items = InterpSliceToNode(R->enum_.items, FileContents);
			N->Enum.Type = InterpToNode(R->enum_.type_node, FileContents);
		} break;

		case AST_SELECTOR:
		{
			N->Selector.Operand = InterpToNode(R->selector.operand, FileContents);
			N->Selector.Member = StringFromInterpPtr(R->selector.member);
			N->Selector.Index = R->selector.index;
			N->Selector.SubIndex = R->selector.sub_index;
			//R->selector.type = N->Selector.Type;
		} break;

		case AST_SIZE:
		{
			N->Size.Expression = InterpToNode(R->size_.expression, FileContents);
			//R->size_.type = N->Size.Type;
		} break;

		case AST_TYPEOF:
		{
			N->TypeOf.Expression = InterpToNode(R->type_of_.expression, FileContents);
			//R->type_of_.Type = N->TypeOf.Type;
		} break;

		case AST_GENERIC:
		{
			N->Generic.Name = StringFromInterpPtr(R->generic.name);
		} break;

		case AST_RESERVED:
		{
			N->Reserved.ID = R->reserved.id;
			//R->reserved.Type = N->Reserved.Type;
		} break;

		case AST_NOP:
		case AST_BREAK:
		case AST_CONTINUE:
			// No additional data to copy
			break;

		case AST_GENSTRUCTTYPE:
		{
			N->GenericStructType.Args = InterpSliceToNode(R->generic_struct_type.args, FileContents);
			N->GenericStructType.ID = InterpToNode(R->generic_struct_type.id, FileContents);
			//R->generic_struct_type.Analyzed = N->GenericStructType.Analyzed;
		} break;

		case AST_LISTITEM:
		{
			N->Item.Name = StringFromInterpPtr(R->item.name);
			N->Item.Expression = InterpToNode(R->item.expression, FileContents);
		} break;

		case AST_SWITCH:
		{
			N->Switch.Expression = InterpToNode(R->switch_.expression, FileContents);
			N->Switch.Cases = InterpSliceToNode(R->switch_.cases, FileContents);
			//R->switch_.SwitchType = N->Switch.SwitchType;
			//R->switch_.ReturnType = N->Switch.ReturnType;
		} break;

		case AST_CASE:
		{
			N->Case.Value = InterpToNode(R->case_.value, FileContents);
			N->Case.Body = InterpSliceToNode(R->case_.body, FileContents);
		} break;

		case AST_POSTOP:
		{
			N->PostOp.Operand = InterpToNode(R->post_op.operand, FileContents);
			N->PostOp.Type = (token_type)R->post_op.op;
			//R->post_op.typeIdx = N->PostOp.TypeIdx;
		} break;

		case AST_DEFER:
		{
			N->Defer.Body = InterpSliceToNode(R->defer_.body, FileContents);
		} break;

		case AST_SCOPE:
		{
			N->ScopeDelimiter.IsUp = R->scope_delimiter.is_up;
		} break;

		case AST_TYPEINFO:
		{
			//R->type_info_lookup.Type = N->TypeInfoLookup.Type;
			N->TypeInfoLookup.Expression = InterpToNode(R->type_info_lookup.expression, FileContents);
		} break;

		case AST_PTRDIFF:
		{
			N->PtrDiff.Left = InterpToNode(R->ptr_diff.left, FileContents);
			N->PtrDiff.Right = InterpToNode(R->ptr_diff.right, FileContents);
			//R->ptr_diff.Type = N->PtrDiff.Type;
		} break;

		case AST_FILE_LOCATION:
		{
		} break;
	}
	return N;
}

