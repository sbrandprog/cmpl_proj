#pragma once
#include "ira_int.h"
#include "u_ros.h"
#include "u_hs.h"

#define IRA_DT_ARR_SIZE_OFF 0
#define IRA_DT_ARR_DATA_OFF 8

struct ira_dt_n {
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
		} ptr;
		struct {
			ira_dt_t * body;
		} arr;
		struct {
			size_t elems_size;
			ira_dt_n_t * elems;
		} tpl;
		struct {
			ira_dt_t * ret;
			size_t args_size;
			ira_dt_n_t * args;
		} func;
	};
};

struct ira_dt_info {
	u_ros_t type_str;
};

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second);

bool ira_dt_get_size(ira_dt_t * dt, size_t * out);
bool ira_dt_get_align(ira_dt_t * dt, size_t * out);

bool ira_dt_get_tpl_elem_off(ira_dt_t * dt, size_t elem_ind, size_t * out);

const ira_dt_info_t ira_dt_infos[IraDt_Count];
