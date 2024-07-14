#pragma once
#include "pla.h"
#include "ira_int.h"

#define PLA_EXPR_OPDS_SIZE 3

enum pla_expr_opd_type {
	PlaExprOpdNone,
	PlaExprOpdBoolean,
	PlaExprOpdIntType,
	PlaExprOpdHs,
	PlaExprOpdCn,
	PlaExprOpdExpr,
	PlaExprOpdExprList1,
	PlaExprOpdExprListLink,
	PlaExprOpd_Count
};
union pla_expr_opd {
	bool boolean;
	ira_int_type_t int_type;
	ul_hs_t * hs;
	pla_cn_t * cn;
	pla_expr_t * expr;
};

enum pla_expr_type {
	PlaExprNone,
#define PLA_EXPR(name, ...) PlaExpr##name,
#include "pla_expr.def"
#undef PLA_EXPR
	PlaExpr_Count
};
struct pla_expr {
	pla_expr_type_t type;
	union {
		pla_expr_opd_t opds[PLA_EXPR_OPDS_SIZE];
		struct {
			pla_expr_opd_t opd0, opd1, opd2;
		};
	};
};

struct pla_expr_info {
	ul_ros_t type_str;
	ira_optr_type_t optr_type;
	union {
		pla_expr_opd_type_t opds[PLA_EXPR_OPDS_SIZE];
		struct {
			pla_expr_opd_type_t opd0, opd1, opd2;
		};
	};
};

pla_expr_t * pla_expr_create(pla_expr_type_t type);
void pla_expr_destroy(pla_expr_t * expr);

const pla_expr_info_t pla_expr_infos[PlaExpr_Count];
