#include "Linearize.h"

Linearizer::Linearizer(slice<node *> Nodes_)
{
	this->Nodes = Nodes_;
	this->Array = {};
}

int Linearizer::LinearizeBody(slice<node *> Body)
{
	int Idx = Add(LNode(NULL));
	int Start = Array.Count;
	For(Body)
	{
		LinearizeNode(*it);
	}
	Array.Data[Idx].Left = (int)(Array.Count - Start); // Body size
	return Idx;
}

int Linearizer::Add(LNode Node)
{
	int Result = Array.Count;
	Array.Push(Node);
	return Result;
}

int Linearizer::LinearizeNode(node *Node)
{
	if(Node == NULL)
		return -1;

	int Idx = -1;
	bool SkipAdd = false;
	LNode Result = LNode(Node);
	switch(Node->Type)
	{
		case AST_YIELD:
		{
			Result.Left = LinearizeNode(Node->Yield.Expr);
		} break;
		case AST_USING:
		{
			Result.Left = LinearizeNode(Node->Using.Expr);
		} break;
		case AST_ASSERT:
		{
			Result.Left = LinearizeNode(Node->Assert.Expr);
		} break;
		case AST_VAR:
		{
			Result.Left = LinearizeNode(Node->Var.TypeNode);
		} break;
		case AST_LIST:
		{
			Result.Left = LinearizeBody(Node->List.Nodes);
		} break;
		case AST_PTRDIFF:
		{
			Result.Left = LinearizeNode(Node->PtrDiff.Left);
			Result.Right = LinearizeNode(Node->PtrDiff.Right);
		} break;
		case AST_DEFER:
		{
			Result.Left = LinearizeBody(Node->Defer.Body);
		} break;
		case AST_CASE:
		{
			Result.Left = LinearizeNode(Node->Case.Value);
			Result.Right = LinearizeBody(Node->Case.Body);
		} break;
		case AST_SWITCH:
		{
			Result.Left = LinearizeNode(Node->Match.Expression);
			Result.Right = LinearizeBody(Node->Match.Cases);
		} break;
		case AST_LISTITEM:
		{
			Result.Left = LinearizeNode(Node->Item.Expression);
		} break;
		case AST_TYPEOF:
		{
			Result.Left = LinearizeNode(Node->TypeOf.Expression);
		} break;
		case AST_SIZE:
		{
			Result.Left = LinearizeNode(Node->Size.Expression);
		} break;
		case AST_SELECTOR:
		{
			Result.Left = LinearizeNode(Node->Selector.Operand);
		} break;
		case AST_ENUM:
		{
			Result.Left  = LinearizeNode(Node->Enum.Type);
			Result.Right = LinearizeBody(Node->Enum.Items);
		} break;
		case AST_STRUCTDECL:
		{
			Result.Left = LinearizeBody(Node->StructDecl.Members);
		} break;
		case AST_TYPEINFO:
		{
			Result.Left = LinearizeNode(Node->TypeInfoLookup.Expression);
		} break;
		case AST_INDEX:
		{
			Result.Left  = LinearizeNode(Node->Index.Operand);
			Result.Right = LinearizeNode(Node->Index.Expression);
		} break;
		case AST_TYPELIST:
		{
			Result.Left  = LinearizeNode(Node->TypeList.TypeNode);
			Result.Right = LinearizeBody(Node->TypeList.Items);
		} break;
		case AST_CAST:
		{
			Result.Left  = LinearizeNode(Node->Cast.TypeNode);
			Result.Right = LinearizeNode(Node->Cast.Expression);
		} break;
		case AST_RETURN:
		{
			Result.Left = LinearizeNode(Node->Return.Expression);
		} break;
		case AST_CALL:
		{
			Result.Left  = LinearizeNode(Node->Call.Fn);
			Result.Right = LinearizeBody(Node->Call.Args);
		} break;
		case AST_FN:
		{
			Result.Left  = LinearizeNode(Node->Fn.ProfileCallback);
			Result.Right = LinearizeBody(Node->Fn.Args);
			Result.Idx3  = LinearizeBody(Node->Fn.ReturnTypes);
			Idx = Add(Result);
			Result.Idx4  = LinearizeBody(SliceFromArray(Node->Fn.Body));
			Array.Data[Idx].Idx4 = Result.Idx4;
			SkipAdd = true;
		} break;
		case AST_ARRAYTYPE:
		{
			Result.Left  = LinearizeNode(Node->ArrayType.Expression);
			Result.Right = LinearizeNode(Node->ArrayType.Type);
		} break;
		case AST_PTRTYPE:
		{
			Result.Left = LinearizeNode(Node->PointerType.Pointed);
		} break;
		case AST_DECL:
		{
			Result.Left  = LinearizeNode(Node->Decl.LHS);
			Result.Right = LinearizeNode(Node->Decl.Type);
			Result.Idx3  = LinearizeNode(Node->Decl.Expression);
		} break;
		case AST_FOR:
		{
			Result.Left  = LinearizeNode(Node->For.Expr1);
			Result.Right = LinearizeNode(Node->For.Expr2);
			Result.Idx3  = LinearizeNode(Node->For.Expr3);
			Result.Idx4  = LinearizeBody(SliceFromArray(Node->For.Body));
		} break;
		case AST_IF:
		{
			Result.Left  = LinearizeNode(Node->If.Expression);
			Result.Right = LinearizeBody(SliceFromArray(Node->If.Body));
			Result.Idx3  = LinearizeBody(SliceFromArray(Node->If.Else));
			Idx = Add(Result);
		} break;
		case AST_BINARY:
		{
			Result.Left  = LinearizeNode(Node->Binary.Left);
			Result.Right = LinearizeNode(Node->Binary.Right);
		} break;
		case AST_UNARY:
		{
			Result.Left = LinearizeNode(Node->Unary.Operand);
		} break;
		case AST_CONSTANT:
		case AST_CHARLIT:
		case AST_NOP:
		case AST_ID:
		case AST_BREAK:
		case AST_EMBED:
		case AST_CONTINUE:
		case AST_SCOPE:
		case AST_RESERVED:
		case AST_GENERIC:
		{
		} break;
		case AST_INVALID:
		{
			unreachable;
		} break;
	}
	if(!SkipAdd)
		Idx = Add(Result);
	return Idx;
}

void Linearizer::PrintBody(LNode Body)
{
	LDEBUG(" { ");

	LDEBUG(" } ");
}

void Linearizer::Print()
{
//	For(Array)
//	{
//		if(it->Node == NULL)
//		{
//			LDEBUG("NOP");
//			continue;
//		}
//		const char *Name = GetASTName(it->Node->Type);
//		LDEBUG("Node: %s", Name);
//	}
}

void Linearizer::Linearize()
{
	Array = {};
	For(Nodes)
	{
		LinearizeNode(*it);
	}
}

