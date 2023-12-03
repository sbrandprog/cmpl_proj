#pragma once
#include "pla.h"
#include "u_ros.h"
#include "u_hs.h"

enum pla_dclr_type {
	PlaDclrNone,
	PlaDclrNspc,
	PlaDclrFunc,
	PlaDclrImpt,
	PlaDclr_Count
};
struct pla_dclr {
	pla_dclr_type_t type;
	pla_dclr_t * next;
	u_hs_t * name;
	union {
		struct {
			pla_dclr_t * body;
		} nspc;
		struct {
			pla_expr_t * dt_expr;
			pla_stmt_t * block;
		} func;
		struct {
			pla_expr_t * dt_expr;
			u_hs_t * lib_name;
			u_hs_t * sym_name;
		} impt;
	};
};

struct pla_dclr_info {
	u_ros_t type_str;
};

pla_dclr_t * pla_dclr_create(pla_dclr_type_t type);
void pla_dclr_destroy(pla_dclr_t * dclr);

const pla_dclr_info_t pla_dclr_infos[PlaDclr_Count];
