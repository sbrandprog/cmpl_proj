#pragma once
#include "ira_int.h"
#include "u_ros.h"
#include "u_hs.h"

#define IRA_DT_ARR_SIZE_OFF 0
#define IRA_DT_ARR_DATA_OFF 8

struct ira_dt_qual {
	bool const_q;
};
struct ira_dt_qdt {
	ira_dt_t * dt;
	ira_dt_qual_t qual;
};

struct ira_dt_ndt {
	ira_dt_t * dt;
	u_hs_t * name;
};

enum ira_dt_type {
	IraDtVoid,
	IraDtDt,
	IraDtBool,
	IraDtInt,
	IraDtPtr,
	IraDtArr,
	IraDtTpl,
	IraDtFunc,
	IraDt_Count
};
struct ira_dt {
	ira_dt_type_t type;
	ira_dt_t * next;
	union {
		ira_int_type_t int_type;
		struct {
			ira_dt_t * body;
			ira_dt_qual_t qual;
		} ptr;
		struct {
			ira_dt_t * body;
			ira_dt_qual_t qual;
		} arr;
		struct {
			size_t elems_size;
			ira_dt_ndt_t * elems;
			ira_dt_qual_t qual;
		} tpl;
		struct {
			ira_dt_t * ret;
			size_t args_size;
			ira_dt_ndt_t * args;
		} func;
	};
};

struct ira_dt_info {
	u_ros_t type_str;
};

bool ira_dt_is_qual_equal(ira_dt_qual_t first, ira_dt_qual_t second);

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second);

bool ira_dt_is_castable(ira_dt_t * from, ira_dt_t * to);

bool ira_dt_get_qual(ira_dt_t * dt, ira_dt_qual_t * out);

bool ira_dt_get_size(ira_dt_t * dt, size_t * out);
bool ira_dt_get_align(ira_dt_t * dt, size_t * out);

bool ira_dt_get_tpl_elem_off(ira_dt_t * dt, size_t elem_ind, size_t * out);

extern const ira_dt_qual_t ira_dt_qual_none;
extern const ira_dt_qual_t ira_dt_qual_const;

extern const ira_dt_info_t ira_dt_infos[IraDt_Count];
