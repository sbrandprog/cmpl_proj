#pragma once
#include "pla.h"
#include "u_hs.h"

enum pla_stmt_type {
	PlaStmtNone,
	PlaStmtBlk,
	PlaStmtExpr,
	PlaStmtVar,
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
		} var;
		struct {
			pla_expr_t * expr;
		} ret;
	};
};

pla_stmt_t * pla_stmt_create(pla_stmt_type_t type);
void pla_stmt_destroy(pla_stmt_t * stmt);
