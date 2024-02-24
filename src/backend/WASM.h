#pragma once
#include "../IR.h"

enum wasm_op
{
	WASM_UNREACHABLE = 0x0,
	WASM_NOP         = 0x01,
	WASM_i32And      = 0x71,
	WASM_i32Extend8  = 0xC0,
	WASM_i32Extend16 = 0xC1,
	WASM_i32Const    = 0x41,
	WASM_i64Const    = 0x42,
	WASM_f32Const    = 0x43,
	WASM_f64Const    = 0x44,

	WASM_i32Add = 0x6A,
	WASM_i64Add = 0x7C,
	WASM_f32Add = 0x92,
	WASM_f64Add = 0xA0,

	WASM_i32Sub = 0x6B,
	WASM_i64Sub = 0x7D,
	WASM_f32Sub = 0x93,
	WASM_f64Sub = 0xA1,

	WASM_i32Mul = 0x6C,
	WASM_i64Mul = 0x7E,
	WASM_f32Mul = 0x94,
	WASM_f64Mul = 0xA2,

	WASM_i32Div = 0x6D,
	WASM_i64Div = 0x7F,
	WASM_f32Div = 0x95,
	WASM_f64Div = 0xA3,

	WASM_i32DivU = 0x6E,
	WASM_i64DivU = 0x80,
	WASM_f32DivU = WASM_UNREACHABLE,
	WASM_f64DivU = WASM_UNREACHABLE,

	WASM_i32Mod = 0x6F,
	WASM_i64Mod = 0x81,
	WASM_f32Mod = WASM_UNREACHABLE,
	WASM_f64Mod = WASM_UNREACHABLE,

	WASM_i32ModU = 0x70,
	WASM_i64ModU = 0x82,
	WASM_f32ModU = WASM_UNREACHABLE,
	WASM_f64ModU = WASM_UNREACHABLE,

	WASM_i32 = 0x7F,
	WASM_i64 = 0x7E,
	WASM_f32 = 0x7D,
	WASM_f64 = 0x7C,

	WASM_LocalGet = 0x20,
	WASM_LocalSet = 0x21,

};

#define WRITE_INSTRUCTION_TYPE(CODE, TYPE, NAME) WriteInstructionBasedOnType((CODE), WASM_i32##NAME, WASM_i64##NAME, WASM_f32##NAME, WASM_f64##NAME, (TYPE))

struct wasm_output
{
	u8 *Code;
	u32 Size;
};

struct wasm_builder
{
	u32 *Locals;
	u32 LastLocal;
};

void OutputWasm(ir IR);

