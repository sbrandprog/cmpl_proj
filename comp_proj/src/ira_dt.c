#include "pch.h"
#include "ira_dt.h"
#include "ira_int.h"
#include "ira_lo.h"
#include "u_misc.h"

bool ira_dt_is_complete(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtPtr:
		case IraDtArr:
			break;
		case IraDtStct:
			if (dt->stct.lo->dt_stct.sd == NULL) {
				return false;
			}
			break;
		case IraDtFunc:
			break;
		default:
			u_assert_switch(dt->type);
	}

	return true;
}

bool ira_dt_is_qual_equal(ira_dt_qual_t first, ira_dt_qual_t second) {
	if (first.const_q != second.const_q) {
		return false;
	}

	return true;
}

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second) {
	if (!ira_dt_is_complete(first) || !ira_dt_is_complete(second)) {
		return false;
	}

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
		{
			if (!ira_dt_is_qual_equal(first->ptr.qual, second->ptr.qual)) {
				return false;
			}

			size_t first_size, second_size;

			if (!ira_dt_get_size(first, &first_size) || !ira_dt_get_size(second, &second_size) || first_size != second_size) {
				return false;
			}

			size_t first_align, second_align;

			if (!ira_dt_get_align(first, &first_align) || !ira_dt_get_align(second, &second_align) || first_align != second_align) {
				return false;
			}

			break;
		}
		case IraDtArr:
			if (!ira_dt_is_qual_equal(first->arr.qual, second->arr.qual)) {
				return false;
			}

			if (!ira_dt_is_equivalent(first->arr.body, second->arr.body)) {
				return false;
			}
			break;
		case IraDtStct:
			if (!ira_dt_is_qual_equal(first->stct.qual, second->stct.qual)) {
				return false;
			}

			if (first->stct.lo != second->stct.lo) {
				ira_dt_sd_t * first_sd = first->stct.lo->dt_stct.sd, * second_sd = second->stct.lo->dt_stct.sd;

				if (first_sd->elems_size != second_sd->elems_size) {
					return false;
				}

				for (ira_dt_ndt_t * f_elem = first_sd->elems, *f_elem_end = f_elem + first_sd->elems_size, *s_elem = second_sd->elems; f_elem != f_elem_end; ++f_elem, ++s_elem) {
					if (!ira_dt_is_equivalent(f_elem->dt, s_elem->dt)) {
						return false;
					}
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
		case IraDtStct:
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
		case IraDtStct:
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
		case IraDtStct:
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
		case IraDtStct:
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
		case IraDtStct:
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
		case IraDtStct:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
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
		case IraDtPtr:
		case IraDtArr:
			return false;
		case IraDtStct:
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
		case IraDtStct:
		case IraDtFunc:
			return false;
		default:
			u_assert_switch(from->type);
	}

	return true;
}
bool ira_dt_is_castable(ira_dt_t * from, ira_dt_t * to) {
	if (!ira_dt_is_complete(from) || !ira_dt_is_complete(to)) {
		return false;
	}

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
		case IraDtStct:
			if (!is_castable_to_stct(from, to)) {
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
		case IraDtStct:
			*out = dt->stct.qual;
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
		case IraDtStct:
		{
			ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

			if (sd == NULL) {
				return false;
			}

			size_t size = 0, align = 1;

			for (ira_dt_ndt_t * elem = sd->elems, *elem_end = elem + sd->elems_size; elem != elem_end; ++elem) {
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
		case IraDtStct:
		{
			ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

			if (sd == NULL) {
				return false;
			}

			size_t align = 1;

			for (ira_dt_ndt_t * elem = sd->elems, *elem_end = elem + sd->elems_size; elem != elem_end; ++elem) {
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

bool ira_dt_get_stct_elem_off(ira_dt_t * dt, size_t elem_ind, size_t * out) {
	u_assert(dt->type == IraDtStct);

	ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;
	
	if (sd == NULL) {
		return false;
	}

	u_assert(elem_ind < sd->elems_size);

	size_t off = 0;

	for (ira_dt_ndt_t * elem = sd->elems, *elem_end = elem + elem_ind; elem != elem_end; ++elem) {
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

bool ira_dt_create_sd(size_t elems_size, ira_dt_ndt_t * elems, ira_dt_sd_t ** out) {
	for (ira_dt_ndt_t * elem = elems, *elem_end = elem + elems_size; elem != elem_end; ++elem) {
		if (!ira_dt_is_complete(elem->dt)) {
			return false;
		}
	}

	*out = malloc(sizeof(**out));

	u_assert(*out != NULL);

	**out = (ira_dt_sd_t){ .elems_size = elems_size };

	ira_dt_ndt_t * new_elems = malloc(elems_size * sizeof(*new_elems));

	u_assert(new_elems != NULL);

	memcpy(new_elems, elems, elems_size * sizeof(*new_elems));

	(*out)->elems = new_elems;

	return true;
}
void ira_dt_destroy_sd(ira_dt_sd_t * sd) {
	if (sd == NULL) {
		return;
	}

	free(sd->elems);

	free(sd);
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
	[IraDtStct] = { .type_str = U_MAKE_ROS(L"DtStct") },
	[IraDtFunc] = { .type_str = U_MAKE_ROS(L"DtFunc") }
};
