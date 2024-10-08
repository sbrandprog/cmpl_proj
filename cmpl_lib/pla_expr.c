#include "ira_optr.h"
#include "pla_cn.h"
#include "pla_expr.h"

pla_expr_t * pla_expr_create(pla_expr_type_t type) {
	pla_expr_t * expr = malloc(sizeof(*expr));

	ul_assert(expr != NULL);

	*expr = (pla_expr_t){ .type = type };

	return expr;
}
void pla_expr_destroy(pla_expr_t * expr) {
	if (expr == NULL) {
		return;
	}

	ul_assert(expr->type < PlaExpr_Count);

	const pla_expr_info_t * info = &pla_expr_infos[expr->type];

	for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case PlaExprOpdNone:
			case PlaExprOpdBoolean:
			case PlaExprOpdIntType:
			case PlaExprOpdHs:
				break;
			case PlaExprOpdCn:
				pla_cn_destroy(expr->opds[opd].cn);
				break;
			case PlaExprOpdExpr:
			case PlaExprOpdExprNoTltn:
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
				ul_assert_unreachable();
		}
	}

	free(expr);
}

const pla_expr_info_t pla_expr_infos[PlaExpr_Count] = {
	[PlaExprNone] = { .type_str = UL_ROS_MAKE(L"ExprNone"), .optr_ctg = IraOptrNone, .opds = { PlaExprOpdNone, PlaExprOpdNone, PlaExprOpdNone } },
#define PLA_EXPR(name, ctg, opd0, opd1, opd2) [PlaExpr##name] = { .type_str = UL_ROS_MAKE(L"Expr" L## #name), .optr_ctg = ctg, .opds = { PlaExprOpd##opd0, PlaExprOpd##opd1, PlaExprOpd##opd2 } },
#include "pla_expr.data"
#undef PLA_EXPR
};
