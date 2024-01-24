#include "pch.h"
#include "pla_expr.h"
#include "pla_irid.h"
#include "u_assert.h"

pla_expr_t * pla_expr_create(pla_expr_type_t type) {
	pla_expr_t * expr = malloc(sizeof(*expr));

	u_assert(expr != NULL);

	*expr = (pla_expr_t){ .type = type };

	return expr;
}
void pla_expr_destroy(pla_expr_t * expr) {
	if (expr == NULL) {
		return;
	}

	u_assert(expr->type < PlaExpr_Count);

	const pla_expr_info_t * info = &pla_expr_infos[expr->type];

	for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case PlaExprOpdNone:
			case PlaExprOpdBoolean:
			case PlaExprOpdIntType:
			case PlaExprOpdHs:
				break;
			case PlaExprOpdIrid:
				pla_irid_destroy(expr->opds[opd].irid);
				break;
			case PlaExprOpdExpr:
				pla_expr_destroy(expr->opds[opd].expr);
				break;
			case PlaExprOpdExprList1:
				for (pla_expr_t * it = expr->opds[opd].expr; it != NULL;) {
					pla_expr_t * next = it->opd1.expr;

					pla_expr_destroy(it);

					it = next;
				}
				break;
			case PlaExprOpdExprListLink:
				break;
			default:
				u_assert_switch(info->opds[opd]);
		}
	}

	free(expr);
}

const pla_expr_info_t pla_expr_infos[PlaExpr_Count] = {
	[PlaExprNone] = { .type_str = U_MAKE_ROS(L"ExprNone"), .opds = { PlaExprOpdNone, PlaExprOpdNone, PlaExprOpdNone } },
#define PLA_EXPR(name, opd0, opd1, opd2) [PlaExpr##name] = { .type_str = U_MAKE_ROS(L"Expr" L## #name), .opds = { PlaExprOpd##opd0, PlaExprOpd##opd1, PlaExprOpd##opd2 } },
#include "pla_expr.def"
#undef PLA_EXPR
};
