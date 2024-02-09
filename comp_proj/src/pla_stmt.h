#pragma once
#include "pla.h"
#include "ira_dt.h"
#include "u_ros.h"
#include "u_hs.h"

enum pla_stmt_type {
	PlaStmtNone,
	PlaStmtBlk,
	PlaStmtExpr,
	PlaStmtVarDt,
	PlaStmtVarVal,
	PlaStmtCond,
	PlaStmtPreLoop,
	PlaStmtRet,
	PlaStmt_Count
};
struct pla_stmt {
	pla_stmt_type_t type;
	pla_stmt_t * next;
	union {
		struct {
			pla_stmt_t * body;
		} block;
		struct {
			pla_expr_t * expr;
		} expr;
		struct {
			u_hs_t * name;
			pla_expr_t * dt_expr;
			ira_dt_qual_t dt_qual;
		} var_dt;
		struct {
			u_hs_t * name;
			pla_expr_t * val_expr;
			ira_dt_qual_t dt_qual;
		} var_val;
		struct {
			pla_expr_t * cond_expr;
			pla_stmt_t * true_br;
			pla_stmt_t * false_br;
		} cond;
		struct {
			pla_expr_t * cond_expr;
			pla_stmt_t * body;
		} pre_loop;
		struct {
			pla_expr_t * expr;
		} ret;
	};
};

struct pla_stmt_info {
	u_ros_t type_str;
};

pla_stmt_t * pla_stmt_create(pla_stmt_type_t type);
void pla_stmt_destroy(pla_stmt_t * stmt);

const pla_stmt_info_t pla_stmt_infos[PlaStmt_Count];
