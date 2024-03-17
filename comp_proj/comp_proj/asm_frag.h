#pragma once
#include "lnk.h"
#include "asm.h"

enum asm_frag_type {
	AsmFragNone,
	AsmFragProc,
	AsmFragRoData,
	AsmFragWrData,
	AsmFrag_Count
};
struct asm_frag {
	asm_frag_type_t type;
	asm_frag_t * next;

	size_t insts_size;
	asm_inst_t * insts;
	size_t insts_cap;

	size_t align;
};

asm_frag_t * asm_frag_create(asm_frag_type_t type);
void asm_frag_destroy(asm_frag_t * frag);

void asm_frag_destroy_chain(asm_frag_t * frag);

void asm_frag_push_inst(asm_frag_t * frag, asm_inst_t * inst);

bool asm_frag_build(asm_frag_t * frag, lnk_pe_t * out);
