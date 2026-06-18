#pragma once

#include "Parser.h"
#include "Interpreter.h"

struct InterpNode
{
    node_type t;
	interp_file_location location;

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
            interp_string Tag;
            interp_string WasmModule;
            interp_string WasmName;

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

node *InterpToNode(const struct InterpNode *R, dict<const string *> FileContents);


