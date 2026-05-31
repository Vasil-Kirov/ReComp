#include "Interpreter.h"

struct InterpFileLocation
{
    interp_string file;
    ssize_t line;
    ssize_t chr;
};

struct InterpNode
{
    node_type t;
	InterpFileLocation location;

    union
    {
        struct
        {
            interp_string name;
            //type T;
        } id;

        struct
        {
            InterpNode * operand;
            i16 op;
            //type T;
        } post_op;

        struct
        {
            InterpNode * expr;
            InterpNode * true_;
            InterpNode * false_;
            //type T;
        } if_x;

        struct
        {
            interp_slice body;
            //type T;
            bool is_expr_run;
        } run;

        struct
        {
            InterpNode * expr;
            //type T;
        } typed_expr;

        struct
        {
            InterpNode * expr;
        } assert_;

        struct
        {
            interp_string name;
            InterpNode * type_node;
            InterpNode * default_;
            //type T;
        } var;

        struct
        {
            interp_slice nodes;
            //interp_slice Ts;
            //type T;
        } list;

        struct
        {
            InterpNode * left;
            InterpNode * right;
            //type T;
        } ptr_diff;

        struct
        {
        } continue_;

        struct
        {
            bool is_up;
        } scope_delimiter;

        struct
        {
            interp_slice body;
        } defer_;

        struct
        {
            reserved id;
            //type T;
        } reserved;

        struct
        {
            bool is_string;
            interp_string file_name;
            interp_string content;
        } embed;

        struct
        {
            interp_string name;
        } generic;

        struct
        {
            interp_string name;
            interp_slice items;
            InterpNode * type_node;
        } enum_;

        struct
        {
            interp_string name;
            InterpNode * expression;
        } item;

        struct
        {
            InterpNode * expression;
            interp_slice cases;
            //type switch_type;
            //type return_type;
        } switch_;

        struct
        {
            InterpNode * value;
            interp_slice body;
        } case_;

        struct
        {
            InterpNode * type_node;
            interp_slice items;
            //type T;
        } type_list;

        struct
        {
            InterpNode * expression;
            //type T;
        } size_;

        struct
        {
            InterpNode * expression;
            //type T;
        } type_of_;

        struct
        {
            interp_string name;
            interp_slice members;
            interp_slice type_params;
            bool is_union;
        } struct_decl;

        struct
        {
            InterpNode * expression;
            //type T;
        } type_info_lookup;

        struct
        {
            InterpNode * operand;
            interp_string member;
            u32 index;
            u32 sub_index;
            //type T;
        } selector;

        struct
        {
            InterpNode * operand;
            i16 op;
            //type T;
        } unary;

        struct
        {
            InterpNode * left;
            InterpNode * right;
            //type ExprT;
            i16 op;
        } binary;

        struct
        {
            InterpNode * expression;
            interp_slice body;
            interp_slice else_;
        } if_;

        struct
        {
            InterpNode * expr1;
            InterpNode * expr2;
            InterpNode * expr3;
            interp_slice body;

            for_type kind;

            //type array_T;
            //type it_T;
            bool it_by_ref;
        } for_;

        struct
        {
            InterpNode * operand;
            InterpNode * expression;

            //type operand_t;
            //type indexed_t;
            //type index_expr_t;
        } index;

        struct
        {
            InterpNode * operand;
            InterpNode * from;
            InterpNode * to;

            //type operand_t;
            //type expr_t;
        } slice;

        struct
        {
            InterpNode * expression;
            InterpNode * type_node;

            bool is_bit_cast;

            //type from_type;
            //type to_type;
        } cast_;

        struct
        {
            u32 C;
        } char_literal;

        struct
        {
            const_value value;
            // type T;
        } constant;

        struct
        {
            interp_string link_name;

            InterpNode * lhs;
            InterpNode * expression;
            InterpNode * t;

            //type type_index;
            u32 flags;
        } decl;

        struct
        {
            interp_string Name;
            interp_string LinkName;

            interp_slice args;
            interp_slice return_types;
            interp_slice Body;

            u32 flags;
        } fn_;

        struct
        {
            InterpNode * fn_;

            //Slice<Node*> args;
			interp_slice args;

            interp_string sym_name;

            //type T;
            //Slice<type> arg_types;
        } call;

        struct
        {
            InterpNode *t_node;
            InterpNode *expression;
            //type T;
        } array_type;

        struct
        {
            InterpNode * id;
			interp_slice args;
            //type T;
        } generic_struct_type;

        struct
        {
            InterpNode * pointed;
            u32 flags;
            //type T;
        } pointer_type;

        struct
        {
            InterpNode * expr;
            //type T;
        } return_;
    };
};

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

