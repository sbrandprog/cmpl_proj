#pragma once
#include "ira_pds.h"
#include "ira_dt.h"
#include "ira_optr.h"

struct ira_pec {
	ul_hst_t * hst;
	ul_ec_fmtr_t * ec_fmtr;

	ul_hsb_t hsb;

	ul_hs_t * pds[IraPds_Count];

	ira_dt_t dt_void;
	ira_dt_t dt_dt;
	ira_dt_t dt_bool;
	ira_dt_t dt_ints[IraInt_Count];
	ira_dt_t * dt_vec;
	ira_dt_t * dt_ptr;
	ira_dt_t * dt_tpl;
	ira_dt_t * dt_stct;
	ira_dt_t * dt_arr;
	ira_dt_t * dt_func;

	ira_dt_stct_tag_t * dt_stct_tag;
	ira_dt_func_vas_t * dt_func_vas;

	struct {
		ira_dt_func_vas_t * none;
		ira_dt_func_vas_t * cstyle;
	} dt_func_vass;
	struct {
		ira_dt_t * size;
		ira_dt_t * va_elem;
		ira_dt_t * ascii_str;
		ira_dt_t * wide_str;
	} dt_spcl;

	ira_optr_t * optrs[IraOptrCtg_Count];

	ira_lo_t * root;

	ira_lo_t * lo;
	size_t lo_index;

	ul_hs_t * ep_name;
};

struct ira_pec_optr_res {
	ira_optr_t * optr;
	ira_dt_t * right_implct_cast_to;
	ira_dt_t * res_dt;
};

IRA_API bool ira_pec_init(ira_pec_t * pec, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr);
IRA_API void ira_pec_cleanup(ira_pec_t * pec);


IRA_API bool ira_pec_is_dt_complete(ira_dt_t * dt);

IRA_API bool ira_pec_get_dt_size(ira_dt_t * dt, size_t * out);
IRA_API bool ira_pec_get_dt_align(ira_dt_t * dt, size_t * out);

IRA_API size_t ira_pec_get_tpl_elem_off(ira_dt_t * dt, size_t elem_i);


IRA_API ira_dt_stct_tag_t * ira_pec_get_dt_stct_tag(ira_pec_t * pec);

IRA_API bool ira_pec_get_dt_vec(ira_pec_t * pec, size_t size, ira_dt_t * body, ira_dt_qual_t qual, ira_dt_t ** out);
IRA_API bool ira_pec_get_dt_ptr(ira_pec_t * pec, ira_dt_t * body, ira_dt_qual_t qual, ira_dt_t ** out);
IRA_API bool ira_pec_get_dt_tpl(ira_pec_t * pec, size_t elems_size, ira_dt_ndt_t * elems, ira_dt_qual_t qual, ira_dt_t ** out);
IRA_API bool ira_pec_get_dt_stct(ira_pec_t * pec, ira_dt_stct_tag_t * tag, ira_dt_qual_t qual, ira_dt_t ** out);
IRA_API bool ira_pec_get_dt_arr(ira_pec_t * pec, ira_dt_t * body, ira_dt_qual_t qual, ira_dt_t ** out);
IRA_API bool ira_pec_get_dt_func(ira_pec_t * pec, ira_dt_t * ret, size_t args_size, ira_dt_ndt_t * args, ira_dt_func_vas_t * vas, ira_dt_t ** out);

IRA_API bool ira_pec_apply_qual(ira_pec_t * pec, ira_dt_t * dt, ira_dt_qual_t qual, ira_dt_t ** out);


IRA_API bool ira_pec_make_val_imm_void(ira_pec_t * pec, ira_val_t ** out);
IRA_API bool ira_pec_make_val_imm_dt(ira_pec_t * pec, ira_dt_t * dt, ira_val_t ** out);
IRA_API bool ira_pec_make_val_imm_bool(ira_pec_t * pec, bool bool_val, ira_val_t ** out);
IRA_API bool ira_pec_make_val_imm_int(ira_pec_t * pec, ira_int_type_t int_type, ira_int_t int_val, ira_val_t ** out);
IRA_API bool ira_pec_make_val_lo_ptr(ira_pec_t * pec, ira_lo_t * lo, ira_val_t ** out);

IRA_API bool ira_pec_make_val_null(ira_pec_t * pec, ira_dt_t * dt, ira_val_t ** out);


IRA_API bool ira_pec_get_optr_dt(ira_pec_t * pec, ira_optr_t * optr, ira_dt_t * left, ira_dt_t * right, ira_pec_optr_res_t * out);
IRA_API bool ira_pec_get_best_optr(ira_pec_t * pec, ira_optr_ctg_t optr_ctg, ira_dt_t * left, ira_dt_t * right, ira_pec_optr_res_t * out);

IRA_API ira_lo_t * ira_pec_push_unq_lo(ira_pec_t * pec, ira_lo_type_t type, ul_hs_t * hint_name);
