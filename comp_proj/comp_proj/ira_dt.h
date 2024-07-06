#pragma once
#include "ira_int.h"

#define IRA_DT_ARR_SIZE_IND 0
#define IRA_DT_ARR_DATA_IND 1

struct ira_dt_qual {
	bool const_q;
};
struct ira_dt_qdt {
	ira_dt_t * dt;
	ira_dt_qual_t qual;
};

struct ira_dt_ndt {
	ira_dt_t * dt;
	ul_hs_t * name;
};

enum ira_dt_type {
	IraDtVoid,
	IraDtDt,
	IraDtBool,
	IraDtInt,
	IraDtVec,
	IraDtPtr,
	IraDtTpl,
	IraDtStct,
	IraDtArr,
	IraDtFunc,
	IraDt_Count
};
struct ira_dt {
	ira_dt_type_t type;
	ira_dt_t * next;
	union {
		ira_int_type_t int_type;
		struct {
			size_t size;
			ira_dt_t * body;
			ira_dt_qual_t qual;
		} vec;
		struct {
			ira_dt_t * body;
			ira_dt_qual_t qual;
		} ptr;
		struct {
			size_t elems_size;
			ira_dt_ndt_t * elems;
			ira_dt_qual_t qual;
		} tpl;
		struct {
			ira_lo_t * lo;
			ira_dt_qual_t qual;
		} stct;
		struct {
			ira_dt_t * body;
			ira_dt_qual_t qual;

			ira_dt_t * assoc_tpl;
		} arr;
		struct {
			ira_dt_t * ret;
			size_t args_size;
			ira_dt_ndt_t * args;
		} func;
	};
};

struct ira_dt_info {
	ul_ros_t type_str;
};

bool ira_dt_is_qual_equal(ira_dt_qual_t first, ira_dt_qual_t second);

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second);

bool ira_dt_is_castable(ira_dt_t * from, ira_dt_t * to);

ira_dt_qual_t ira_dt_get_qual(ira_dt_t * dt);

ira_dt_qual_t ira_dt_apply_qual(ira_dt_qual_t first, ira_dt_qual_t second);

extern const ira_dt_qual_t ira_dt_qual_none;
extern const ira_dt_qual_t ira_dt_qual_const;

extern const ira_dt_info_t ira_dt_infos[IraDt_Count];
