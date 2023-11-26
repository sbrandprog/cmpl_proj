#include "pch.h"
#include "ira_dt.h"
#include "ira_int.h"
#include "u_assert.h"

bool ira_dt_is_ptr_dt_comp(ira_dt_t * dt) {
	if (dt == NULL) {
		return false;
	}

	switch (dt->type) {
		case IraDtNone:
			return false;
		case IraDtVoid:
			return true;
		case IraDtDt:
			return false;
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
		case IraDtFunc:
			return true;
		default:
			u_assert_switch(dt->type);
	}
}
bool ira_dt_is_arr_dt_comp(ira_dt_t * dt) {
	if (dt == NULL) {
		return false;
	}

	switch (dt->type) {
		case IraDtNone:
			return false;
		case IraDtVoid:
			return true;
		case IraDtDt:
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
			return true;
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(dt->type);
	}
}
bool ira_dt_is_func_dt_comp(ira_dt_t * dt) {
	if (dt == NULL) {
		return false;
	}

	switch (dt->type) {
		case IraDtNone:
			return false;
		case IraDtVoid:
		case IraDtDt:
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
			return true;
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(dt->type);
	}
}
bool ira_dt_is_var_dt_comp(ira_dt_t * dt) {
	if (dt == NULL) {
		return false;
	}

	switch (dt->type) {
		case IraDtNone:
			return false;
		case IraDtVoid:
		case IraDtDt:
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
			return true;
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(dt->type);
	}
}
bool ira_dt_is_impt_dt_comp(ira_dt_t * dt) {
	if (dt == NULL) {
		return false;
	}

	switch (dt->type) {
		case IraDtNone:
			return false;
		case IraDtVoid:
			return true;
		case IraDtDt:
			return false;
		case IraDtInt:
		case IraDtPtr:
			return true;
		case IraDtArr:
			return false;
		case IraDtFunc:
			return true;
		default:
			u_assert_switch(dt->type);
	}
}

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second) {
	if (first == second) {
		return true;
	}

	if (first->type != second->type) {
		return false;
	}

	switch (first->type) {
		case IraDtNone:
		case IraDtVoid:
		case IraDtDt:
			break;
		case IraDtInt:
			if (first->int_type != second->int_type) {
				return false;
			}
			break;
		case IraDtPtr:
			if (!ira_dt_is_equivalent(first->ptr.body, second->ptr.body)) {
				return false;
			}
			break;
		case IraDtArr:
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

			for (ira_dt_n_t * f_arg = first->func.args, *f_arg_end = f_arg + first->func.args_size, *s_arg = second->func.args; f_arg != f_arg_end; ++f_arg, ++s_arg) {
				if (!ira_dt_is_equivalent(f_arg->dt, s_arg->dt)) {
					return false;
				}
			}
			break;
		default:
			u_assert_switch(first->type);
	}

	return true;
}

size_t ira_dt_get_size(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtNone:
		case IraDtVoid:
		case IraDtDt:
			return 0;
		case IraDtInt:
			return ira_int_get_size(dt->int_type);
		case IraDtPtr:
			return 8;
		case IraDtArr:
			return 16;
		case IraDtFunc:
			return 0;
		default:
			u_assert_switch(dt->type);
	}
}
size_t ira_dt_get_align(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtNone:
		case IraDtVoid:
		case IraDtDt:
			return 1;
		case IraDtInt:
			return ira_int_get_size(dt->int_type);
		case IraDtPtr:
			return 8;
		case IraDtArr:
			return 8;
		case IraDtFunc:
			return 1;
		default:
			u_assert_switch(dt->type);
	}
}
