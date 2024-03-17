#pragma once
#include "../IR.h"

enum x86_registers {
	Register_A,
	Register_B,
	Register_C,
	Register_D
};


void x86Generate(ir *IR);



