#pragma once
#include "ira.h"
#include "pla_pds.h"

#define PLA_TLTR_MOD_NAME L"pla_tltr"

struct pla_tltr_src {
	ul_hs_t * name;
	pla_tu_t * tu;
};
enum pla_tltr_tse_type {
	PlaTltrTseDclr,
	PlaTltrTseStmt,
	PlaTltrTseExpr,
	PlaTltrTse_Count
};
struct pla_tltr_tse {
	pla_tltr_tse_type_t type;

	pla_tltr_tse_t * prev;

	union {
		pla_dclr_t * dclr;
		pla_stmt_t * stmt;
		pla_expr_t * expr;
	};
};
enum pla_tltr_vse_type {
	PlaTltrVseNspc,
	PlaTltrVse_Count
};
struct pla_tltr_vse {
	pla_tltr_vse_type_t type;

	ul_hs_t * name;
	pla_tltr_vse_t * prev;

	union {
		struct {
			ira_lo_t * lo;
			ul_hs_t * name;
		} nspc;
	};
};
struct pla_tltr {
	ul_hst_t * hst;
	ul_ec_fmtr_t * ec_fmtr;
	ira_pec_t * out;

	ul_hs_t * pds[PlaPds_Count];

	ul_hsb_t hsb;

	const pla_tltr_src_t * src;

	pla_tltr_tse_t * tse;
	pla_tltr_vse_t * vse;
};

PLA_API void pla_tltr_init(pla_tltr_t * tltr, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr, ira_pec_t * out);
PLA_API void pla_tltr_cleanup(pla_tltr_t * tltr);


PLA_API void pla_tltr_report(pla_tltr_t * tltr, const wchar_t * fmt, ...);
PLA_API void pla_tltr_report_pec_err(pla_tltr_t * tltr);


PLA_API bool pla_tltr_calculate_expr(pla_tltr_t * tltr, pla_expr_t * expr, ira_val_t ** out);
PLA_API bool pla_tltr_calculate_expr_dt(pla_tltr_t * tltr, pla_expr_t * expr, ira_dt_t ** out);


PLA_API void pla_tltr_push_tse(pla_tltr_t * tltr, pla_tltr_tse_t * tse);
PLA_API void pla_tltr_pop_tse(pla_tltr_t * tltr, pla_tltr_tse_t * tse);


PLA_API void pla_tltr_push_vse(pla_tltr_t * tltr, pla_tltr_vse_t * vse);
PLA_API void pla_tltr_pop_vse(pla_tltr_t * tltr, pla_tltr_vse_t * vse);


PLA_API bool pla_tltr_translate(pla_tltr_t * tltr, const pla_tltr_src_t * src);
