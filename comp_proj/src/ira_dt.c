#include "pch.h"
#include "ira_dt.h"
#include "ira_int.h"
#include "u_misc.h"

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
		case IraDtTpl:
			if (first->tpl.elems_size != second->tpl.elems_size) {
				return false;
			}

			for (ira_dt_n_t * f_elem = first->tpl.elems, *f_elem_end = f_elem + first->tpl.elems_size, *s_elem = second->tpl.elems; f_elem != f_elem_end; ++f_elem, ++s_elem) {
				if (!ira_dt_is_equivalent(f_elem->dt, s_elem->dt)) {
					return false;
				}
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
		case IraDtVoid:
		case IraDtDt:
			return 0;
		case IraDtBool:
			return 1;
		case IraDtInt:
			return ira_int_get_size(dt->int_type);
		case IraDtPtr:
			return 8;
		case IraDtArr:
			return 16;
		case IraDtTpl:
		{
			size_t size = 0, align = 1;

			for (ira_dt_n_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
				size_t elem_align = ira_dt_get_align(elem->dt);

				size = u_align_to(size, elem_align);
				size += ira_dt_get_size(elem->dt);

				align = max(align, elem_align);
			}

			size = u_align_to(size, align);

			return size;
		}
		case IraDtFunc:
			return 0;
		default:
			u_assert_switch(dt->type);
	}
}
size_t ira_dt_get_align(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
			return 1;
		case IraDtInt:
			return ira_int_get_size(dt->int_type);
		case IraDtPtr:
			return 8;
		case IraDtArr:
			return 8;
		case IraDtTpl:
		{
			size_t align = 1;

			for (ira_dt_n_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
				align = max(align, ira_dt_get_align(elem->dt));
			}

			return align;
		}
		case IraDtFunc:
			return 1;
		default:
			u_assert_switch(dt->type);
	}
}

size_t ira_dt_get_tpl_elem_off(ira_dt_t * dt, size_t elem_ind) {
	u_assert(dt != NULL);
	u_assert(dt->type == IraDtTpl);
	u_assert(elem_ind < dt->tpl.elems_size);

	size_t off = 0;

	for (ira_dt_n_t * elem = dt->tpl.elems, *elem_end = elem + elem_ind; elem != elem_end; ++elem) {
		size_t elem_align = ira_dt_get_align(elem->dt);

		off = u_align_to(off, elem_align);
		off += ira_dt_get_size(elem->dt);
	}

	return off;
}

const ira_dt_info_t ira_dt_infos[IraDt_Count] = {
	[IraDtVoid] = { .type_str = U_MAKE_ROS(L"DtVoid"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = true },
	[IraDtDt] = { .type_str = U_MAKE_ROS(L"DtDt"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = false },
	[IraDtBool] = { .type_str = U_MAKE_ROS(L"DtBool"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = true },
	[IraDtInt] = { .type_str = U_MAKE_ROS(L"DtInt"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = true },
	[IraDtPtr] = { .type_str = U_MAKE_ROS(L"DtPtr"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = true },
	[IraDtArr] = { .type_str = U_MAKE_ROS(L"DtArr"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = true },
	[IraDtTpl] = { .type_str = U_MAKE_ROS(L"DtSmv"), .ptr_dt_comp = true, .arr_dt_comp = true, .tpl_dt_comp = true, .func_dt_comp = true, .var_comp = true, .impt_comp = true },
	[IraDtFunc] = { .type_str = U_MAKE_ROS(L"DtFunc"), .ptr_dt_comp = true, .arr_dt_comp = false, .tpl_dt_comp = false, .func_dt_comp = false, .var_comp = true, .impt_comp = true }
};
