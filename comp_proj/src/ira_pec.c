#include "pch.h"
#include "ira_pec.h"
#include "ira_val.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "u_assert.h"

static void destroy_dt_chain(ira_pec_t * pec, ira_dt_t * dt) {
	while (dt != NULL) {
		ira_dt_t * next = dt->next;

		switch (dt->type) {
			case IraDtVec:
			case IraDtPtr:
			case IraDtStct:
			case IraDtArr:
				break;
			case IraDtFunc:
				free(dt->func.args);
				break;
			default:
				u_assert_switch(dt->type);
		}

		free(dt);

		dt = next;
	}
}

bool ira_pec_init(ira_pec_t * pec, u_hst_t * hst) {
	*pec = (ira_pec_t){ .hst = hst };

	for (ira_pds_t pds = 0; pds < IraPds_Count; ++pds) {
		const u_ros_t * pds_str = &ira_pds_strs[pds];

		pec->pds[pds] = u_hst_hashadd(pec->hst, pds_str->size, pds_str->str);
	}

	{
		pec->dt_void.type = IraDtVoid;

		pec->dt_dt.type = IraDtDt;

		pec->dt_bool.type = IraDtBool;

		for (ira_int_type_t itype = IraInt_First; itype < IraInt_Count; ++itype) {
			ira_dt_t * dt_int = &pec->dt_ints[itype];

			dt_int->type = IraDtInt;
			dt_int->int_type = itype;
		}

		pec->dt_spcl.size = &pec->dt_ints[IraIntU64];
		
		if (!ira_pec_get_dt_arr(pec, &pec->dt_ints[IraIntU8], ira_dt_qual_const, &pec->dt_spcl.ascii_str)) {
			return false;
		}

		if (!ira_pec_get_dt_arr(pec, &pec->dt_ints[IraIntU16], ira_dt_qual_const, &pec->dt_spcl.wide_str)) {
			return false;
		}
	}

	pec->ep_name = pec->pds[IraPdsEpName];

	return true;
}
void ira_pec_cleanup(ira_pec_t * pec) {
	ira_lo_destroy(pec->root);

	ira_lo_destroy_chain(pec->dt_stct_lo);

	destroy_dt_chain(pec, pec->dt_vec);
	destroy_dt_chain(pec, pec->dt_ptr);
	destroy_dt_chain(pec, pec->dt_stct);
	destroy_dt_chain(pec, pec->dt_arr);
	destroy_dt_chain(pec, pec->dt_func);

	memset(pec, 0, sizeof(*pec));
}

static bool get_listed_dt_cmp(ira_pec_t * pec, ira_dt_t * pred, ira_dt_t * dt) {
	u_assert(pred->type == dt->type);

	switch (pred->type) {
		case IraDtVec:
			if (dt->vec.size != pred->vec.size) {
				return false;
			}

			if (!ira_dt_is_qual_equal(dt->vec.qual, pred->vec.qual)) {
				return false;
			}

			if (dt->vec.body != pred->vec.body) {
				return false;
			}
			break;
		case IraDtPtr:
			if (!ira_dt_is_qual_equal(dt->ptr.qual, pred->ptr.qual)) {
				return false;
			}

			if (dt->ptr.body != pred->ptr.body) {
				return false;
			}
			break;
		case IraDtStct:
			if (!ira_dt_is_qual_equal(dt->stct.qual, pred->stct.qual)) {
				return false;
			}

			if (dt->stct.lo != pred->stct.lo) {
				if (dt->stct.lo->name == pec->pds[IraPdsStctLoName] && dt->stct.lo->name == pred->stct.lo->name) {
					ira_dt_sd_t * dt_sd = dt->stct.lo->dt_stct.sd, * pred_sd = pred->stct.lo->dt_stct.sd;

					u_assert(dt_sd != NULL && pred_sd != NULL);

					if (dt_sd->elems_size != pred_sd->elems_size) {
						return false;
					}

					for (ira_dt_ndt_t * elem = dt_sd->elems, *elem_end = elem + dt_sd->elems_size, *pred_elem = pred_sd->elems; elem != elem_end; ++elem, ++pred_elem) {
						if (elem->dt != pred_elem->dt || elem->name != pred_elem->name) {
							return false;
						}
					}
				}
				else {
					return false;
				}
			}
			break;
		case IraDtArr:
			if (!ira_dt_is_qual_equal(dt->arr.qual, pred->arr.qual)) {
				return false;
			}

			if (dt->arr.body != pred->arr.body) {
				return false;
			}
			break;

		case IraDtFunc:
			if (dt->func.args_size != pred->func.args_size) {
				return false;
			}

			if (dt->func.ret != pred->func.ret) {
				return false;
			}

			for (ira_dt_ndt_t * arg = dt->func.args, *arg_end = arg + dt->func.args_size, *pred_arg = pred->func.args; arg != arg_end; ++arg, ++pred_arg) {
				if (arg->dt != pred_arg->dt || arg->name != pred_arg->name) {
					return false;
				}
			}
			break;
		default:
			u_assert_switch(pred->type);
	}

	return true;
}
static bool get_listed_dt_copy(ira_pec_t * pec, ira_dt_t * pred, ira_dt_t * out) {
	switch (pred->type) {
		case IraDtVec:
			*out = (ira_dt_t){ .type = pred->type, .vec = { .size = pred->vec.size, .body = pred->vec.body, .qual = pred->vec.qual } };
			break;
		case IraDtPtr:
			*out = (ira_dt_t){ .type = pred->type, .ptr = { .body = pred->ptr.body, .qual = pred->ptr.qual } };
			break;
		case IraDtStct:
			if (pred->stct.lo->name != pec->pds[IraPdsStctLoName]) {
				*out = (ira_dt_t){ .type = pred->type, .stct = { .lo = pred->stct.lo, .qual = pred->stct.qual } };
			}
			else {
				ira_dt_sd_t * pred_sd = pred->stct.lo->dt_stct.sd;

				ira_lo_t * lo = ira_lo_create(IraLoDtStct, pec->pds[IraPdsStctLoName]);

				lo->next = pec->dt_stct_lo;
				pec->dt_stct_lo = lo;

				if (!ira_dt_create_sd(pred_sd->elems_size, pred_sd->elems, &lo->dt_stct.sd)) {
					return false;
				}
				
				*out = (ira_dt_t){ .type = pred->type, .stct = { .lo = lo, .qual = pred->stct.qual } };
			}
			break;
		case IraDtArr:
		{
			*out = (ira_dt_t){ .type = pred->type, .arr = { .body = pred->arr.body, .qual = pred->arr.qual } };

			ira_dt_ndt_t elems[2] = {
				[IRA_DT_ARR_SIZE_IND] = { .dt = pec->dt_spcl.size, .name = pec->pds[IraPdsSizeMmbr] },
				[IRA_DT_ARR_DATA_IND] = { .dt = NULL, .name = pec->pds[IraPdsDataMmbr] }
			};

			if (!ira_pec_get_dt_ptr(pec, pred->arr.body, ira_dt_qual_none, &elems[IRA_DT_ARR_DATA_IND].dt)) {
				return false;
			}

			if (!ira_pec_get_dt_stct(pec, _countof(elems), elems, pred->arr.qual, &out->arr.assoc_stct)) {
				return false;
			}
			break;
		}
		case IraDtFunc:
		{
			for (ira_dt_ndt_t * arg = pred->func.args, *arg_end = arg + pred->func.args_size; arg != arg_end; ++arg) {
				if (arg->name == NULL) {
					return false;
				}
			}

			ira_dt_ndt_t * new_args = malloc(pred->func.args_size * sizeof(*new_args));

			u_assert(new_args != NULL);

			memcpy(new_args, pred->func.args, pred->func.args_size * sizeof(*new_args));

			*out = (ira_dt_t){ .type = pred->type, .func = { .args_size = pred->func.args_size, .args = new_args, .ret = pred->func.ret } };
			break;
		}
		default:
			u_assert_switch(pred->type);
	}

	return true;
}
static bool get_listed_dt(ira_pec_t * pec, ira_dt_t * pred, ira_dt_t ** ins, ira_dt_t ** out) {
	while (*ins != NULL) {
		ira_dt_t * dt = *ins;

		if (get_listed_dt_cmp(pec, pred, dt)) {
			*out = dt;
			return true;
		}

		ins = &dt->next;
	}

	ira_dt_t * new_dt = malloc(sizeof(*new_dt));

	u_assert(new_dt != NULL);

	memset(new_dt, 0, sizeof(*new_dt));

	*ins = new_dt;
	
	if (!get_listed_dt_copy(pec, pred, new_dt)) {
		return false;
	}

	*out = new_dt;

	return true;
}

bool ira_pec_get_dt_vec(ira_pec_t * pec, size_t size, ira_dt_t * body, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtVec, .vec = { .size = size, .body = body, .qual = qual } };

	return get_listed_dt(pec, &pred, &pec->dt_vec, out);
}
bool ira_pec_get_dt_ptr(ira_pec_t * pec, ira_dt_t * body, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtPtr, .ptr = { .body = body, .qual = qual } };

	return get_listed_dt(pec, &pred, &pec->dt_ptr, out);
}
bool ira_pec_get_dt_stct(ira_pec_t * pec, size_t elems_size, ira_dt_ndt_t * elems, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_sd_t sd = { .elems_size = elems_size, .elems = elems };

	ira_lo_t lo = { .type = IraLoDtStct, .name = pec->pds[IraPdsStctLoName], .dt_stct.sd = &sd };

	ira_dt_t pred = { .type = IraDtStct, .stct = { .lo = &lo, .qual = qual } };

	return get_listed_dt(pec, &pred, &pec->dt_stct, out);
}
bool ira_pec_get_dt_stct_lo(ira_pec_t * pec, ira_lo_t * lo, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtStct, .stct = { .lo = lo, .qual = qual } };

	return get_listed_dt(pec, &pred, &pec->dt_stct, out);
}
bool ira_pec_get_dt_arr(ira_pec_t * pec, ira_dt_t * body, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtArr, .arr = { .body = body, .qual = qual } };

	return get_listed_dt(pec, &pred, &pec->dt_arr, out);
}
bool ira_pec_get_dt_func(ira_pec_t * pec, ira_dt_t * ret, size_t args_size, ira_dt_ndt_t * args, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtFunc, .func = { .ret = ret, .args_size = args_size, .args = args } };

	return get_listed_dt(pec, &pred, &pec->dt_func, out);
}

bool ira_pec_apply_qual(ira_pec_t * pec, ira_dt_t * dt, ira_dt_qual_t qual, ira_dt_t ** out) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			*out = dt;
			break;
		case IraDtVec:
			qual = ira_dt_apply_qual(dt->vec.qual, qual);

			if (!ira_pec_get_dt_vec(pec, dt->vec.size, dt->vec.body, qual, out)) {
				return false;
			}
			break;
		case IraDtPtr:
			qual = ira_dt_apply_qual(dt->ptr.qual, qual);

			if (!ira_pec_get_dt_ptr(pec, dt->ptr.body, qual, out)) {
				return false;
			}
			break;
		case IraDtStct:
			qual = ira_dt_apply_qual(dt->ptr.qual, qual);

			if (!ira_pec_get_dt_stct_lo(pec, dt->stct.lo, qual, out)) {
				return false;
			}
			break;
		case IraDtArr:
			qual = ira_dt_apply_qual(dt->ptr.qual, qual);

			if (!ira_pec_get_dt_arr(pec, dt->arr.body, qual, out)) {
				return false;
			}
			break;
		case IraDtFunc:
			*out = dt;
			break;
		default:
			u_assert_switch(dt->type);
	}

	return true;
}

bool ira_pec_make_val_imm_void(ira_pec_t * pec, ira_val_t ** out) {
	*out = ira_val_create(IraValImmVoid, &pec->dt_void);

	return true;
}
bool ira_pec_make_val_imm_dt(ira_pec_t * pec, ira_dt_t * dt, ira_val_t ** out) {
	*out = ira_val_create(IraValImmDt, &pec->dt_dt);

	(*out)->dt_val = dt;

	return true;
}
bool ira_pec_make_val_imm_bool(ira_pec_t * pec, bool bool_val, ira_val_t ** out) {
	*out = ira_val_create(IraValImmBool, &pec->dt_bool);

	(*out)->bool_val = bool_val;

	return true;
}
bool ira_pec_make_val_imm_int(ira_pec_t * pec, ira_int_type_t int_type, ira_int_t int_val, ira_val_t ** out) {
	u_assert(int_type < IraInt_Count);

	*out = ira_val_create(IraValImmInt, &pec->dt_ints[int_type]);

	(*out)->int_val = int_val;

	return true;
}
bool ira_pec_make_val_lo_ptr(ira_pec_t * pec, ira_lo_t * lo, ira_val_t ** out) {
	ira_dt_t * val_dt;

	switch (lo->type) {
		case IraLoFunc:
			if (!ira_pec_get_dt_ptr(pec, lo->func->dt, ira_dt_qual_none, &val_dt)) {
				return false;
			}
			break;
		case IraLoImpt:
			if (!ira_pec_get_dt_ptr(pec, lo->impt.dt, ira_dt_qual_none, &val_dt)) {
				return false;
			}
			break;
		case IraLoVar:
			if (!ira_pec_get_dt_ptr(pec, lo->var.qdt.dt, lo->var.qdt.qual, &val_dt)) {
				return false;
			}
			break;
		default:
			u_assert_switch(lo->type);
	}

	*out = ira_val_create(IraValLoPtr, val_dt);

	(*out)->lo_val = lo;

	return true;
}

bool ira_pec_make_val_null(ira_pec_t * pec, ira_dt_t * dt, ira_val_t ** out) {
	ira_val_type_t val_type;
	
	switch (dt->type) {
		case IraDtVoid:
			val_type = IraValImmVoid;
			break;
		case IraDtDt:
			val_type = IraValImmDt;
			break;
		case IraDtBool:
			val_type = IraValImmBool;
			break;
		case IraDtInt:
			val_type = IraValImmInt;
			break;
		case IraDtVec:
			val_type = IraValImmVec;
			break;
		case IraDtPtr:
			val_type = IraValNullPtr;
			break;
		case IraDtStct:
			val_type = IraValImmStct;
			break;
		case IraDtArr:
			val_type = IraValImmArr;
			break;
		default:
			u_assert_switch(dt->type);
	}

	*out = ira_val_create(val_type, dt);

	switch (dt->type) {
		case IraDtVoid:
			break;
		case IraDtDt:
			(*out)->dt_val = &pec->dt_void;
			break;
		case IraDtBool:
			(*out)->bool_val = false;
			break;
		case IraDtInt:
			(*out)->int_val.ui64 = 0;
			break;
		case IraDtVec:
			(*out)->vec.data = malloc(dt->vec.size * sizeof(*(*out)->vec.data));

			u_assert((*out)->vec.data != NULL);

			memset((*out)->vec.data, 0, dt->vec.size * sizeof(*(*out)->vec.data));

			if (dt->vec.size > 0) {
				if (!ira_pec_make_val_null(pec, dt->vec.body, &(*out)->vec.data[0])) {
					return false;
				}

				ira_val_t * ref_val = (*out)->vec.data[0];

				ira_val_t ** elem = (*out)->vec.data, ** elem_end = elem + dt->vec.size;

				++elem;

				while (elem != elem_end) {
					*elem++ = ira_val_copy(ref_val);
				}
			}

			break;
		case IraDtPtr:
			break;
		case IraDtStct:
		{
			ira_dt_sd_t * sd = dt->stct.lo->dt_stct.sd;

			if (sd == NULL) {
				return false;
			}

			(*out)->stct.size = sd->elems_size;

			(*out)->stct.elems = malloc((*out)->stct.size * sizeof((*out)->stct.elems));

			u_assert((*out)->stct.elems != NULL);

			memset((*out)->stct.elems, 0, (*out)->stct.size * sizeof((*out)->stct.elems));

			ira_val_t ** elem = (*out)->stct.elems, ** elem_end = elem + (*out)->stct.size;
			ira_dt_ndt_t * elem_dt = sd->elems;

			for (; elem != elem_end; ++elem, ++elem_dt) {
				if (!ira_pec_make_val_null(pec, elem_dt->dt, elem)) {
					return false;
				}
			}
			break;
		}
		case IraDtArr:
		{
			(*out)->arr.size = 0;

			size_t arr_size_bytes = (*out)->arr.size * sizeof((*out)->arr.data);

			(*out)->arr.data = malloc(arr_size_bytes);

			u_assert((*out)->arr.data != NULL);

			memset((*out)->arr.data, 0, arr_size_bytes);

			break;
		}
		default:
			u_assert_switch(dt->type);
	}

	return true;
}
