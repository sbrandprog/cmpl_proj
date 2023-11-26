#pragma once
#include "ira_pds.h"
#include "ira_dt.h"
#include "u_hst.h"

struct ira_pec {
	u_hst_t * hst;

	u_hs_t * pds[IraPds_Count];

	ira_dt_t dt_void;
	ira_dt_t dt_dt;
	ira_dt_t dt_ints[IraInt_Count];
	ira_dt_t * dt_ptr;
	ira_dt_t * dt_arr;
	ira_dt_t * dt_func;

	struct {
		ira_dt_t * size;
		ira_dt_t * arr_size;
		ira_dt_t * ascii_str;
	} dt_spcl;

	ira_lo_t * root;

	u_hs_t * ep_name;
};

void ira_pec_init(ira_pec_t * pec, u_hst_t * hst);
void ira_pec_cleanup(ira_pec_t * pec);

ira_dt_t * ira_pec_get_dt_ptr(ira_pec_t * pec, ira_dt_t * body);
ira_dt_t * ira_pec_get_dt_arr(ira_pec_t * pec, ira_dt_t * body);
ira_dt_t * ira_pec_get_dt_func(ira_pec_t * pec, ira_dt_t * ret, size_t args_size, ira_dt_n_t * args);

ira_val_t * ira_pec_make_val_imm_dt(ira_pec_t * pec, ira_dt_t * dt);
ira_val_t * ira_pec_make_val_imm_int(ira_pec_t * pec, ira_int_type_t int_type, ira_int_t int_val);
ira_val_t * ira_pec_make_val_lo_ptr(ira_pec_t * pec, ira_lo_t * lo);
