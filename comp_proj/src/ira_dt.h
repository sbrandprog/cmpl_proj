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
	IraDtNone,
	IraDtVoid,
	IraDtDt,
	IraDtBool,
	IraDtInt,
	IraDtPtr,
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
			ira_dt_t * body;
		} ptr;
		struct {
			ira_dt_t * body;
		} arr;
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

bool ira_dt_is_ptr_dt_comp(ira_dt_t * dt);
bool ira_dt_is_arr_dt_comp(ira_dt_t * dt);
bool ira_dt_is_func_dt_comp(ira_dt_t * dt);

bool ira_dt_is_var_comp(ira_dt_t * dt);
bool ira_dt_is_impt_comp(ira_dt_t * dt);

bool ira_dt_is_compl_val_comp(ira_dt_t * dt);
bool ira_dt_is_intrp_val_comp(ira_dt_t * dt);

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second);

size_t ira_dt_get_size(ira_dt_t * dt);
size_t ira_dt_get_align(ira_dt_t * dt);

const ira_dt_info_t ira_dt_infos[IraDt_Count];
