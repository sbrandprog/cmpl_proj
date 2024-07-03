#include "pch.h"
#include "ira_val.h"
#include "ira_lo.h"
#include "ira_func.h"

ira_lo_t * ira_lo_create(ira_lo_type_t type, ul_hs_t * name) {
	ira_lo_t * lo = malloc(sizeof(*lo));

	ul_assert(lo != NULL);

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
			ira_lo_destroy_chain(lo->nspc.body);
			break;
		case IraLoFunc:
			ira_func_destroy(lo->func);
			break;
		case IraLoImpt:
			break;
		case IraLoVar:
			ira_val_destroy(lo->var.val);
			break;
		case IraLoDtStct:
			ira_dt_destroy_sd(lo->dt_stct.sd);
			break;
		default:
			ul_assert_unreachable();
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
