#include "Quote.h"

#include "IR.h"
#include "Interpreter.h"
#include "Semantics.h"
#include "Type.h"

node *MakeQuote(const error_info *ErrI, slice<token> Tokens, slice<string> Names)
{
	node *Quote = AllocateNode(ErrI, AST_QUOTE);
	Quote->Quote.Tokens = Tokens;
	Quote->Quote.Unquoted = Names;
	return Quote;
}

node *ParseQuote(parser *Parser)
{
	ERROR_INFO;
	EatToken(Parser, T_QUOTE);
	dynamic<token> Tokens = {};
	dynamic<string> Names = {};
	EatToken(Parser, T_STARTSCOPE);
	int Depth = 1;
	while (Depth > 0)
	{
		if(Parser->Current->Type == T_EOF)
			break;

		if(Parser->Current->Type == '`')
		{
			Tokens.Push(GetToken(Parser));
			Names.Push(*EatToken(Parser, T_ID).ID);
		}
		else if(Parser->Current->Type == T_STARTSCOPE)
		{
			Depth++;
		}
		else if(Parser->Current->Type == T_ENDSCOPE)
		{
			Depth--;
			if (Depth != 0)
				Tokens.Push(GetToken(Parser));
		}
		else
		{
			Tokens.Push(GetToken(Parser));
		}
	}
	EatToken(Parser, T_ENDSCOPE);
	return MakeQuote(ErrorInfo, SliceFromArray(Tokens), SliceFromArray(Names));
}

u32 BuildToken(block_builder *Builder, token T, u32 TokenT)
{
	u32 Token = PushInstruction(Builder,
			Instruction(OP_ALLOC, -1, TokenT, Builder));

	u32 TypePtr = PushInstruction(Builder, 
			Instruction(OP_INDEX, Token, 0, TokenT, Builder));
	u32 IDPtr = PushInstruction(Builder, 
			Instruction(OP_INDEX, Token, 1, TokenT, Builder));

	u32 Type = PushInstruction(Builder, Instruction(OP_CONSTINT, T.Type, Basic_i16, Builder));
	
	const_value *V = NewType(const_value);
	if(T.ID)
	{
		*V = MakeConstString(T.ID);
	}
	else
	{
		string Empty = STR_LIT("");
		*V = MakeConstString(&Empty);
	}

	u32 Str = PushInstruction(Builder, Instruction(OP_CONST, (u64)V, Basic_string, Builder));

	InstructionStore(TypePtr, Type, Basic_i16);
	InstructionStore(IDPtr, Str, Basic_string);

	return Token;
}

u32 BuildIRFromQuote(block_builder *Builder, node *Node)
{
	Assert(Node->Type == AST_QUOTE);

	u32 TokenT = FindStruct(STR_LIT("base.RVToken"));
	u32 SliceT = GetSliceType(TokenT);

	array<uint> Resolved = {Node->Quote.Unquoted.Count};
	size_t AtResolved = 0;
	for(string Name : Node->Quote.Unquoted)
	{
		b32 IsGlobal = false;
		const symbol *s = GetIRLocal(Builder, &Name, true, &IsGlobal);
		if (IsGlobal)
		{
			instruction GlobalI = Instruction(OP_GLOBAL, (void *)s, s->Type, Builder, 0);
			Resolved[AtResolved++] = PushInstruction(Builder, GlobalI);
		}
		else
		{
			Resolved[AtResolved++] = s->Register;
		}
	}

	ir_quote *Quote = NewType(ir_quote);
	Quote->Tokens = Node->Quote.Tokens;
	Quote->Resolved = SliceFromArray(Resolved);
	
	return PushInstruction(Builder, Instruction(OP_QUOTE, (void*)Quote, SliceT, Builder, 0));
}

struct interp_token
{
	i16 Type;
	interp_string ID;
};

interp_slice *ExecuteQuote(interpreter *VM, ir_quote *Quote)
{
	u32 TokenT = FindStruct(STR_LIT("base.RVToken"));
	size_t TokenSize = GetTypeSize(TokenT);
	int AtResolved = 0;
	size_t SliceAllocSize = 0;
	for(token Token : Quote->Tokens)
	{
		if(Token.Type == '`')
		{
			uint Reg = Quote->Resolved[AtResolved++];
			value *V = VM->Registers.GetValue(Reg);

			u32 Ti = V->Type;
			const type *T = GetType(V->Type);
			if(T->Kind == TypeKind_Pointer)
			{
				Ti = T->Pointer.Pointed;
				T = GetType(Ti);
			}

			if(T->Kind == TypeKind_Slice)
			{
				Assert(T->Slice.Type == TokenT);
				u32 _;
				size_t Count = *(size_t *)IndexVM(VM, V, 0, Ti, &_);
				SliceAllocSize += Count * TokenSize;
			}
			else
			{
				Assert(Ti == TokenT);
				SliceAllocSize += TokenSize;
			}
		}
		else
		{
			SliceAllocSize += TokenSize;
		}
	}

	interp_token *Tokens = (interp_token *)ArenaAllocate(&VM->Arena, SliceAllocSize);
	size_t AtToken = 0;

	AtResolved = 0;
	for(token Token : Quote->Tokens)
	{
		if(Token.Type == '`')
		{
			uint Reg = Quote->Resolved[AtResolved++];
			value *V = VM->Registers.GetValue(Reg);

			u32 Ti = V->Type;
			const type *T = GetType(V->Type);
			if(T->Kind == TypeKind_Pointer)
			{
				Ti = T->Pointer.Pointed;
				T = GetType(Ti);
			}

			if(T->Kind == TypeKind_Slice)
			{
				Assert(T->Slice.Type == TokenT);
				u32 _;
				size_t Count = *(size_t *)IndexVM(VM, V, 0, Ti, &_);
				interp_token *Data = *(interp_token **)IndexVM(VM, V, 1, Ti, &_);
				for(size_t i = 0; i < Count; ++i)
					Tokens[AtToken++] = Data[i];

			}
			else
			{
				Assert(Ti == TokenT);
				Tokens[AtToken++] = *(interp_token *)V->ptr;
			}

		}
		else
		{
			Tokens[AtToken++] = (interp_token){(i16)Token.Type, StringToInterp(Token.ID)};
		}
	}

	auto s = (interp_slice *)VM->Stack.Peek().Allocate(sizeof(interp_slice));
	s->Count = AtToken;
	s->Data = Tokens;

	return s;
}

