#pragma once
#include "asm.h"
#include "lnk_sect.h"

struct asm_it_sym {
	asm_it_sym_t * next;
	ul_hs_t * name;
	ul_hs_t * link_name;
};
struct asm_it_lib {
	asm_it_lib_t * next;
	ul_hs_t * name;
	asm_it_sym_t * sym;
};
struct asm_it {
	asm_it_lib_t * lib;
};

void asm_it_cleanup(asm_it_t * it);

asm_it_lib_t * asm_it_get_lib(asm_it_t * it, ul_hs_t * name);
asm_it_sym_t * asm_it_add_sym(asm_it_t * it, ul_hs_t * lib_name, ul_hs_t * sym_name, ul_hs_t * sym_link_name);

asm_it_sym_t * asm_it_lib_add_sym(asm_it_lib_t * lib, ul_hs_t * name, ul_hs_t * link_name);

bool asm_it_build(asm_it_t * it, ul_hst_t * hst, lnk_sect_t ** out);
