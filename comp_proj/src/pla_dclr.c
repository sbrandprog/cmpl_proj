#include "pch.h"
#include "pla_dclr.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "u_assert.h"

pla_dclr_t * pla_dclr_create(pla_dclr_type_t type) {
	pla_dclr_t * dclr = malloc(sizeof(*dclr));

	u_assert(dclr != NULL);

	*dclr = (pla_dclr_t){ .type = type };

	return dclr;
}
void pla_dclr_destroy(pla_dclr_t * dclr) {
	if (dclr == NULL) {
		return;
	}

	switch (dclr->type) {
		case PlaDclrNone:
			break;
		case PlaDclrNspc:
			for (pla_dclr_t * it = dclr->nspc.body; it != NULL; ) {
				pla_dclr_t * next = it->next;

				pla_dclr_destroy(it);

				it = next;
			}
			break;
		case PlaDclrFunc:
			pla_expr_destroy(dclr->func.dt_expr);
			pla_stmt_destroy(dclr->func.block);
			break;
		case PlaDclrImpt:
			pla_expr_destroy(dclr->impt.dt_expr);
			break;
		default:
			u_assert_switch(dclr->type);
	}

	free(dclr);
}

