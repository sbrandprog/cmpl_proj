#include "pch.h"
#include "pla_stmt.h"
#include "pla_expr.h"
#include "u_assert.h"

pla_stmt_t * pla_stmt_create(pla_stmt_type_t type) {
	pla_stmt_t * stmt = malloc(sizeof(*stmt));

	u_assert(stmt != NULL);

	*stmt = (pla_stmt_t){ .type = type };

	return stmt;
}
void pla_stmt_destroy(pla_stmt_t * stmt) {
	if (stmt == NULL) {
		return;
	}

	switch (stmt->type) {
		case PlaStmtNone:
			break;
		case PlaStmtBlk:
			for (pla_stmt_t * it = stmt->block.body; it != NULL; ) {
				pla_stmt_t * next = it->next;

				pla_stmt_destroy(it);

				it = next;
			}
			break;
		case PlaStmtExpr:
			pla_expr_destroy(stmt->expr.expr);
			break;
		case PlaStmtVar:
			pla_expr_destroy(stmt->var.dt_expr);
			break;
		case PlaStmtRet:
			pla_expr_destroy(stmt->ret.expr);
			break;
		default:
			u_assert_switch(stmt->type);
	}

	free(stmt);
}
