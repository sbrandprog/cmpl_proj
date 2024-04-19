#include "pch.h"
#include "ira_dt.h"
#include "ira_int.h"
#include "ira_lo.h"

bool ira_dt_is_complete(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			break;
		case IraDtVec:
			if (!ira_dt_is_complete(dt->vec.body)) {
				return false;
			}
			break;
		case IraDtPtr:
			break;
		case IraDtStct:
			if (dt->stct.lo->dt_stct.sd == NULL) {
				return false;
			}
			break;
		case IraDtArr:
			if (!ira_dt_is_complete(dt->arr.body)) {
				return false;
			}
			break;
		case IraDtFunc:
			if (!ira_dt_is_complete(dt->func.ret)) {
				return false;
			}

			for (ira_dt_ndt_t * arg = dt->func.args, *arg_end = arg + dt->func.args_size; arg != arg_end; ++arg) {
				if (!ira_dt_is_complete(arg->dt)) {
					return false;
				}
			}
			break;
		default:
			ul_assert_unreachable();
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
		case IraDtStct:
			if (!ira_dt_is_qual_equal(first->stct.qual, second->stct.qual)) {
				return false;
			}

			if (first->stct.lo != second->stct.lo) {
				ira_dt_sd_t * first_sd = first->stct.lo->dt_stct.sd, * second_sd = second->stct.lo->dt_stct.sd;

				if (first_sd == NULL || second_sd == NULL) {
					return false;
				}

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
		case IraDtStct:
			if (to->stct.lo->dt_stct.sd == NULL) {
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

bool ira_dt_get_qual(ira_dt_t * dt, ira_dt_qual_t * out) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			*out = ira_dt_qual_none;
			break;
		case IraDtVec:
			*out = dt->vec.qual;
			break;
		case IraDtPtr:
			*out = dt->ptr.qual;
			break;
		case IraDtStct:
			*out = dt->stct.qual;
			break;
		case IraDtArr:
			*out = dt->arr.qual;
			break;
		case IraDtFunc:
			*out = ira_dt_qual_none;
			break;
		default:
			ul_assert_unreachable();
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
			*out = (size_t)ira_int_infos[dt->int_type].size;
			break;
		case IraDtVec:
			if (!ira_dt_get_size(dt->vec.body, out)) {
				return false;
			}

			*out *= dt->vec.size;

			break;
		case IraDtPtr:
			*out = 8;
			break;
		case IraDtStct:
		{
			ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

			if (sd == NULL) {
				return false;
			}

			*out = sd->size;
			break;
		}
		case IraDtArr:
			if (!ira_dt_get_size(dt->arr.assoc_stct, out)) {
				return false;
			}
			break;
		case IraDtFunc:
			*out = 0;
			break;
		default:
			ul_assert_unreachable();
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
			*out = (size_t)ira_int_infos[dt->int_type].size;
			break;
		case IraDtVec:
			if (!ira_dt_get_align(dt->vec.body, out)) {
				return false;
			}
			break;
		case IraDtPtr:
			*out = 8;
			break;
		case IraDtStct:
		{
			ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

			if (sd == NULL) {
				return false;
			}

			*out = sd->align;
			break;
		}
		case IraDtArr:
			if (!ira_dt_get_align(dt->arr.assoc_stct, out)) {
				return false;
			}
			break;
		case IraDtFunc:
			*out = 1;
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}

ira_dt_qual_t ira_dt_apply_qual(ira_dt_qual_t first, ira_dt_qual_t second) {
	first.const_q = first.const_q || second.const_q;
	return first;
}

static void calc_sd_props(ira_dt_sd_t * sd) {
	size_t size = 0, align = 1;

	for (ira_dt_ndt_t * elem = sd->elems, *elem_end = elem + sd->elems_size; elem != elem_end; ++elem) {
		size_t elem_align;

		if (!ira_dt_get_align(elem->dt, &elem_align)) {
			ul_assert_unreachable();
		}

		size = ul_align_to(size, elem_align);

		size_t elem_size;

		if (!ira_dt_get_size(elem->dt, &elem_size)) {
			ul_assert_unreachable();
		}

		size += elem_size;

		align = max(align, elem_align);
	}

	size = ul_align_to(size, align);

	sd->size = size;
	sd->align = align;
}
bool ira_dt_create_sd(size_t elems_size, ira_dt_ndt_t * elems, ira_dt_sd_t ** out) {
	for (ira_dt_ndt_t * elem = elems, *elem_end = elem + elems_size; elem != elem_end; ++elem) {
		if (elem->name == NULL || !ira_dt_is_complete(elem->dt)) {
			return false;
		}
	}

	*out = malloc(sizeof(**out));

	ul_assert(*out != NULL);

	**out = (ira_dt_sd_t){ .elems_size = elems_size };

	ira_dt_ndt_t * new_elems = malloc(elems_size * sizeof(*new_elems));

	ul_assert(*out != NULL);

	memcpy(new_elems, elems, elems_size * sizeof(*new_elems));

	(*out)->elems = new_elems;

	calc_sd_props(*out);

	return true;
}
void ira_dt_destroy_sd(ira_dt_sd_t * sd) {
	if (sd == NULL) {
		return;
	}

	free(sd->elems);

	free(sd);
}

size_t ira_dt_get_sd_elem_off(ira_dt_sd_t * sd, size_t elem_ind) {
	ul_assert(elem_ind < sd->elems_size);

	size_t off = 0;

	for (ira_dt_ndt_t * elem = sd->elems, *elem_end = elem + elem_ind; elem != elem_end; ++elem) {
		size_t elem_align;

		if (!ira_dt_get_align(elem->dt, &elem_align)) {
			ul_assert_unreachable();
		}

		off = ul_align_to(off, elem_align);

		size_t elem_size;

		if (!ira_dt_get_size(elem->dt, &elem_size)) {
			ul_assert_unreachable();
		}

		off += elem_size;
	}

	return off;
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
	[IraDtArr] = { .type_str = UL_ROS_MAKE(L"DtArr") },
	[IraDtStct] = { .type_str = UL_ROS_MAKE(L"DtStct") },
	[IraDtFunc] = { .type_str = UL_ROS_MAKE(L"DtFunc") }
};
