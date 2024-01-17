#include "pch.h"
#include "ira_val.h"
#include "ira_lo.h"
#include "ira_func.h"
#include "u_assert.h"
#include "u_misc.h"

ira_lo_t * ira_lo_create(ira_lo_type_t type, u_hs_t * name) {
	ira_lo_t * lo = malloc(sizeof(*lo));

	u_assert(lo != NULL);

	*lo = (ira_lo_t){ .type = type, .name = name };

	return lo;
}
void ira_lo_destroy(ira_lo_t * lo) {
	if (lo == NULL) {
		return;
	}

	switch (lo->type) {
		case IraLoNone:
			break;
		case IraLoNspc:
			for (ira_lo_t * it = lo->nspc.body; it != NULL; ) {
				ira_lo_t * next = it->next;

				ira_lo_destroy(it);

				it = next;
			}
			break;
		case IraLoFunc:
			ira_func_destroy(lo->func);
			break;
		case IraLoImpt:
			break;
		case IraLoVar:
			ira_val_destroy(lo->var.val);
			break;
		default:
			u_assert_switch(lo->type);
	}

	free(lo);
}

void ira_lo_destroy_chain(ira_lo_t * lo) {
	while (lo != NULL) {
		ira_lo_t * next = lo->next;
		ira_lo_destroy(lo);
		lo = next;
	}
}

bool ira_lo_get_dt(ira_lo_t * lo, ira_dt_t ** out) {
	switch (lo->type) {
		case IraLoNone:
		case IraLoNspc:
			return false;
		case IraLoFunc:
			*out = lo->func->dt;
			break;
		case IraLoImpt:
			*out = lo->impt.dt;
			break;
		case IraLoVar:
			*out = lo->var.dt;
			break;
		default:
			u_assert_switch(lo->type);
	}

	return true;
}
