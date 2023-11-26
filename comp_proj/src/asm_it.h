#pragma once
#include "asm.h"
#include "lnk_sect.h"
#include "u_hst.h"

struct asm_it_sym {
	asm_it_sym_t * next;
	u_hs_t * name;
	u_hs_t * link_name;
};
struct asm_it_lib {
	asm_it_lib_t * next;
	u_hs_t * name;
	asm_it_sym_t * sym;
};
struct asm_it {
	asm_it_lib_t * lib;
};

void asm_it_cleanup(asm_it_t * it);

asm_it_lib_t * asm_it_get_lib(asm_it_t * it, u_hs_t * name);
asm_it_sym_t * asm_it_add_sym(asm_it_t * it, u_hs_t * lib_name, u_hs_t * sym_name, u_hs_t * sym_link_name);

asm_it_sym_t * asm_it_lib_add_sym(asm_it_lib_t * lib, u_hs_t * name, u_hs_t * link_name);

bool asm_it_build(asm_it_t * it, u_hst_t * hst, lnk_sect_t ** out);
