#include "pch.h"
#include "ira_dt.h"
#include "ira_int.h"
#include "u_misc.h"

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
		case IraDtPtr:
			if (!ira_dt_is_qual_equal(first->ptr.qual, second->ptr.qual)) {
				return false;
			}

			if (!ira_dt_is_equivalent(first->ptr.body, second->ptr.body)) {
				return false;
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
		case IraDtTpl:
			if (first->tpl.elems_size != second->tpl.elems_size) {
				return false;
			}

			if (!ira_dt_is_qual_equal(first->tpl.qual, second->tpl.qual)) {
				return false;
			}

			for (ira_dt_ndt_t * f_elem = first->tpl.elems, *f_elem_end = f_elem + first->tpl.elems_size, *s_elem = second->tpl.elems; f_elem != f_elem_end; ++f_elem, ++s_elem) {
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

			for (ira_dt_ndt_t * f_arg = first->func.args, *f_arg_end = f_arg + first->func.args_size, *s_arg = second->func.args; f_arg != f_arg_end; ++f_arg, ++s_arg) {
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

static bool is_castable_to_void(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
		case IraDtTpl:
		case IraDtFunc:
			break;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
		case IraDtArr:
		case IraDtTpl:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
			break;
		case IraDtArr:
		case IraDtTpl:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
			break;
		case IraDtArr:
		case IraDtTpl:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
			break;
		case IraDtArr:
		case IraDtTpl:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
			return false;
		case IraDtArr:
			__debugbreak();
			return false;
		case IraDtTpl:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
		case IraDtArr:
			return false;
		case IraDtTpl:
			__debugbreak();
			return false;
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
	}

	return true;
}
static bool is_castable_to_func(ira_dt_t * from, ira_dt_t * to) {
	switch (from->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
		case IraDtTpl:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
			if (!is_castable_to_ptr(from, to)) {
				return false;
			}
			break;
		case IraDtArr:
			if (!is_castable_to_arr(from, to)) {
				return false;
			}
			break;
		case IraDtTpl:
			if (!is_castable_to_tpl(from, to)) {
				return false;
			}
			break;
		case IraDtFunc:
			if (!is_castable_to_func(from, to)) {
				return false;
			}
			break;
		default:
			u_assert_switch(to->type);
	}

	return true;
}

bool ira_dt_get_qual(ira_dt_t * dt, ira_dt_qual_t * out) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			*out = ira_dt_qual_none;
			break;
		case IraDtPtr:
			*out = dt->ptr.qual;
			break;
		case IraDtArr:
			*out = dt->arr.qual;
			break;
		case IraDtTpl:
			*out = dt->tpl.qual;
			break;
		case IraDtFunc:
			*out = ira_dt_qual_none;
			break;
		default:
			u_assert_switch(dt->type);
	}

	return true;
}

bool ira_dt_get_size(ira_dt_t * dt, size_t * out) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
			*out = 0;
			break;
		case IraDtBool:
			*out = 1;
			break;
		case IraDtInt:
			*out = ira_int_get_size(dt->int_type);
			break;
		case IraDtPtr:
			*out = 8;
			break;
		case IraDtArr:
			*out = 16;
			break;
		case IraDtTpl:
		{
			size_t size = 0, align = 1;

			for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
				size_t elem_align;

				if (!ira_dt_get_align(elem->dt, &elem_align)) {
					return false;
				}

				size = u_align_to(size, elem_align);
				
				size_t elem_size;
				
				if (!ira_dt_get_size(elem->dt, &elem_size)) {
					return false;
				}
				
				size += elem_size;

				align = max(align, elem_align);
			}

			size = u_align_to(size, align);

			*out = size;
			break;
		}
		case IraDtFunc:
			*out = 0;
			break;
		default:
			u_assert_switch(dt->type);
	}

	return true;
}
bool ira_dt_get_align(ira_dt_t * dt, size_t * out) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
			*out = 1;
			break;
		case IraDtInt:
			*out = ira_int_get_size(dt->int_type);
			break;
		case IraDtPtr:
		case IraDtArr:
			*out = 8;
			break;
		case IraDtTpl:
		{
			size_t align = 1;

			for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
				size_t elem_align;
				
				if (!ira_dt_get_align(elem->dt, &elem_align)) {
					return false;
				}

				align = max(align, elem_align);
			}

			*out = align;
			break;
		}
		case IraDtFunc:
			*out = 1;
			break;
		default:
			u_assert_switch(dt->type);
	}

	return true;
}

bool ira_dt_get_tpl_elem_off(ira_dt_t * dt, size_t elem_ind, size_t * out) {
	u_assert(dt != NULL);
	u_assert(dt->type == IraDtTpl);
	u_assert(elem_ind < dt->tpl.elems_size);

	size_t off = 0;

	for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + elem_ind; elem != elem_end; ++elem) {
		size_t elem_align;
		
		if (!ira_dt_get_align(elem->dt, &elem_align)) {
			return false;
		}

		off = u_align_to(off, elem_align);

		size_t elem_size;

		if (!ira_dt_get_size(elem->dt, &elem_size)) {
			return false;
		}

		off += elem_size;
	}

	*out = off;
	return true;
}

const ira_dt_qual_t ira_dt_qual_none;
const ira_dt_qual_t ira_dt_qual_const = { .const_q = true };

const ira_dt_info_t ira_dt_infos[IraDt_Count] = {
	[IraDtVoid] = { .type_str = U_MAKE_ROS(L"DtVoid") },
	[IraDtDt] = { .type_str = U_MAKE_ROS(L"DtDt") },
	[IraDtBool] = { .type_str = U_MAKE_ROS(L"DtBool") },
	[IraDtInt] = { .type_str = U_MAKE_ROS(L"DtInt") },
	[IraDtPtr] = { .type_str = U_MAKE_ROS(L"DtPtr") },
	[IraDtArr] = { .type_str = U_MAKE_ROS(L"DtArr") },
	[IraDtTpl] = { .type_str = U_MAKE_ROS(L"DtTpl") },
	[IraDtFunc] = { .type_str = U_MAKE_ROS(L"DtFunc") }
};
