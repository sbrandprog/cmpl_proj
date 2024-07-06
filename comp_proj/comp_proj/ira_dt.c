#include "pch.h"
#include "ira_dt.h"
#include "ira_int.h"
#include "ira_lo.h"

bool ira_dt_is_qual_equal(ira_dt_qual_t first, ira_dt_qual_t second) {
	if (first.const_q != second.const_q) {
		return false;
	}

	return true;
}

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second) {
	if (first == second) {
		return true;
	}

	if (first->type != second->type) {
		return false;
	}

	switch (first->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
			break;
		case IraDtInt:
			if (first->int_type != second->int_type) {
				return false;
			}
			break;
		case IraDtVec:
			if (first->vec.size != second->vec.size) {
				return false;
			}

			if (!ira_dt_is_qual_equal(first->vec.qual, second->vec.qual)) {
				return false;
			}

			if (!ira_dt_is_equivalent(first->vec.body, second->vec.body)) {
				return false;
			}
			break;
		case IraDtPtr:
			if (!ira_dt_is_qual_equal(first->ptr.qual, second->ptr.qual)) {
				return false;
			}

			if (!ira_dt_is_equivalent(first->ptr.body, second->ptr.body)) {
				return false;
			}

			break;
		case IraDtTpl:
			if (!ira_dt_is_qual_equal(first->tpl.qual, second->tpl.qual)) {
				return false;
			}

			if (first->tpl.elems_size != second->tpl.elems_size) {
				return false;
			}

			for (ira_dt_ndt_t * f_elem = first->tpl.elems, *f_elem_end = f_elem + first->tpl.elems_size, *s_elem = second->tpl.elems; f_elem != f_elem_end; ++f_elem, ++s_elem) {
				if (!ira_dt_is_equivalent(f_elem->dt, s_elem->dt)) {
					return false;
				}
			}
			break;
		case IraDtStct:
			if (!ira_dt_is_qual_equal(first->stct.qual, second->stct.qual)) {
				return false;
			}

			if (first->stct.lo != second->stct.lo) {
				ira_dt_t * first_tpl = first->stct.lo->dt_stct.tpl, * second_tpl = second->stct.lo->dt_stct.tpl;

				if (first_tpl == NULL || second_tpl == NULL) {
					return false;
				}

				if (!ira_dt_is_equivalent(first_tpl, second_tpl)) {
					return false;
				}
			}
			break;
		case IraDtArr:
			if (!ira_dt_is_qual_equal(first->arr.qual, second->arr.qual)) {
				return false;
			}

			if (!ira_dt_is_equivalent(first->arr.body, second->arr.body)) {
				return false;
			}
			break;
		case IraDtFunc:
			if (first->func.args_size != second->func.args_size) {
				return false;
			}

			if (!ira_dt_is_equivalent(first->func.ret, second->func.ret)) {
				return false;
			}

			for (ira_dt_ndt_t * f_arg = first->func.args, *f_arg_end = f_arg + first->func.args_size, *s_arg = second->func.args; f_arg != f_arg_end; ++f_arg, ++s_arg) {
				if (!ira_dt_is_equivalent(f_arg->dt, s_arg->dt)) {
					return false;
				}
			}
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}

static bool is_castable_to_void(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_dt(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
		case IraDtDt:
			break;
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_bool(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
			return false;
		case IraDtBool:
		case IraDtInt:
			break;
		case IraDtVec:
			return false;
		case IraDtPtr:
			break;
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_int(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
			return false;
		case IraDtBool:
		case IraDtInt:
			break;
		case IraDtVec:
			return false;
		case IraDtPtr:
			break;
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_vec(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			return false;
		case IraDtVec:
			__debugbreak();
			return false;
		case IraDtPtr:
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_ptr(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
			return false;
		case IraDtBool:
		case IraDtInt:
			break;
		case IraDtVec:
			return false;
		case IraDtPtr:
			break;
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_tpl(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
		case IraDtTpl:
			__debugbreak();
			return false;
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_stct(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
			return false;
		case IraDtStct:
			__debugbreak();
			return false;
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_arr(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
		case IraDtTpl:
		case IraDtStct:
			return false;
		case IraDtArr:
			__debugbreak();
			return false;
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool is_castable_to_func(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
		case IraDtTpl:
		case IraDtStct:
		case IraDtArr:
		case IraDtFunc:
			return false;
		default:
			ul_assert_unreachable();
	}

	return true;
}
bool ira_dt_is_castable(ira_dt_t * from, ira_dt_t * to) {
	switch (to->type) {
		case IraDtVoid:
			if (!is_castable_to_void(from, to)) {
				return false;
			}
			break;
		case IraDtDt:
			if (!is_castable_to_dt(from, to)) {
				return false;
			}
			break;
		case IraDtBool:
			if (!is_castable_to_bool(from, to)) {
				return false;
			}
			break;
		case IraDtInt:
			if (!is_castable_to_int(from, to)) {
				return false;
			}
			break;
		case IraDtVec:
			if (!is_castable_to_vec(from, to)) {
				return false;
			}
			break;
		case IraDtPtr:
			if (!is_castable_to_ptr(from, to)) {
				return false;
			}
			break;
		case IraDtTpl:
			if (!is_castable_to_tpl(from, to)) {
				return false;
			}
			break;
		case IraDtStct:
			if (to->stct.lo->dt_stct.tpl == NULL) {
				return false;
			}

			if (!is_castable_to_stct(from, to)) {
				return false;
			}
			break;
		case IraDtArr:
			if (!is_castable_to_arr(from, to)) {
				return false;
			}
			break;
		case IraDtFunc:
			if (!is_castable_to_func(from, to)) {
				return false;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}

ira_dt_qual_t ira_dt_get_qual(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			return ira_dt_qual_none;
		case IraDtVec:
			return dt->vec.qual;
		case IraDtPtr:
			return dt->ptr.qual;
		case IraDtTpl:
			return dt->tpl.qual;
		case IraDtStct:
			return dt->stct.qual;
		case IraDtArr:
			return dt->arr.qual;
		case IraDtFunc:
			return ira_dt_qual_none;
		default:
			ul_assert_unreachable();
	}
}

ira_dt_qual_t ira_dt_apply_qual(ira_dt_qual_t first, ira_dt_qual_t second) {
	first.const_q = first.const_q || second.const_q;
	return first;
}

const ira_dt_qual_t ira_dt_qual_none;
const ira_dt_qual_t ira_dt_qual_const = { .const_q = true };

const ira_dt_info_t ira_dt_infos[IraDt_Count] = {
	[IraDtVoid] = { .type_str = UL_ROS_MAKE(L"DtVoid") },
	[IraDtDt] = { .type_str = UL_ROS_MAKE(L"DtDt") },
	[IraDtBool] = { .type_str = UL_ROS_MAKE(L"DtBool") },
	[IraDtInt] = { .type_str = UL_ROS_MAKE(L"DtInt") },
	[IraDtVec] = { .type_str = UL_ROS_MAKE(L"DtVec") },
	[IraDtPtr] = { .type_str = UL_ROS_MAKE(L"DtPtr") },
	[IraDtTpl] = { .type_str = UL_ROS_MAKE(L"DtTpl") },
	[IraDtStct] = { .type_str = UL_ROS_MAKE(L"DtStct") },
	[IraDtArr] = { .type_str = UL_ROS_MAKE(L"DtArr") },
	[IraDtFunc] = { .type_str = UL_ROS_MAKE(L"DtFunc") }
};
