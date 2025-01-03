#pragma once

#include <Parser.h>

// Linearized Node
struct LNode
{
	node *Node;
	int Left;
	int Right;
	int Idx3;
	int Idx4;
	LNode(node *Node_) : Node(Node_), Left(-1), Right(-1), Idx3(-1), Idx4(-1)
	{}
};

struct Linearizer
{
	dynamic<LNode> Array;
	slice<node *> Nodes;
	Linearizer(slice<node *> Nodes);

	int Add(LNode);
	int LinearizeNode(node *Node);
	int LinearizeBody(slice<node *> Body);
	void Linearize();
	void PrintBody(LNode Body);
	void Print();
};


