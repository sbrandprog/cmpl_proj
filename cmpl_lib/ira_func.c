#include "ira_func.h"
#include "ira_dt.h"
#include "ira_inst.h"

ira_func_t * ira_func_create(ira_dt_t * dt) {
	ira_func_t * func = malloc(sizeof(*func));

	ul_assert(func != NULL);

	*func = (ira_func_t){ .dt = dt };

	return func;
}
void ira_func_destroy(ira_func_t * func) {
	if (func == NULL) {
		return;
	}

	for (ira_inst_t * inst = func->insts, *inst_end = inst + func->insts_size; inst != inst_end; ++inst) {
		ira_inst_cleanup(inst);
	}

	free(func->insts);
	free(func);
}

ira_inst_t * ira_func_push_inst(ira_func_t * func, ira_inst_t * inst) {
	ul_arr_grow(func->insts_size + 1, &func->insts_cap, &func->insts, sizeof(*func->insts));

	func->insts[func->insts_size++] = *inst;

	return &func->insts[func->insts_size - 1];
}
