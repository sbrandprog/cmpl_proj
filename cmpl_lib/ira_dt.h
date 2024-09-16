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

struct ira_dt_stct_tag {
	ira_dt_stct_tag_t * next;
	ira_dt_t * tpl;
};

enum ira_dt_func_vas_type {
	IraDtFuncVasNone,
	IraDtFuncVasCstyle,
	IraDtFuncVas_Count
};
struct ira_dt_func_vas {
	ira_dt_func_vas_type_t type;
	ira_dt_func_vas_t * next;
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
			ira_dt_stct_tag_t * tag;
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
			ira_dt_func_vas_t * vas;
		} func;
	};
};

struct ira_dt_info {
	ul_ros_t type_str;
};

IRA_API bool ira_dt_is_qual_equal(ira_dt_qual_t first, ira_dt_qual_t second);

IRA_API bool ira_dt_is_func_vas_equivalent(ira_dt_func_vas_t * first, ira_dt_func_vas_t * second);

IRA_API bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second);

IRA_API bool ira_dt_is_castable(ira_dt_t * from, ira_dt_t * to, bool implct);

IRA_API ira_dt_qual_t ira_dt_get_qual(ira_dt_t * dt);

IRA_API ira_dt_qual_t ira_dt_apply_qual(ira_dt_qual_t to, ira_dt_qual_t from);

extern IRA_API const ira_dt_qual_t ira_dt_qual_none;
extern IRA_API const ira_dt_qual_t ira_dt_qual_const;

extern IRA_API const ira_dt_info_t ira_dt_infos[IraDt_Count];
