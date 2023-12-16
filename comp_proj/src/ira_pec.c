#include "pch.h"
#include "ira_pec.h"
#include "ira_val.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "u_assert.h"

typedef struct get_listed_dt_pred {
	ira_dt_type_t type;
	union {
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
} get_listed_dt_pred_t;

static void destroy_dt_chain(ira_pec_t * pec, ira_dt_t * dt) {
	while (dt != NULL) {
		ira_dt_t * next = dt->next;

		switch (dt->type) {
			case IraDtPtr:
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

void ira_pec_init(ira_pec_t * pec, u_hst_t * hst) {
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
		pec->dt_spcl.arr_size = pec->dt_spcl.size;
		pec->dt_spcl.ascii_str = ira_pec_get_dt_arr(pec, &pec->dt_ints[IraIntU8]);
	}

	pec->ep_name = pec->pds[IraPdsEpName];
}
void ira_pec_cleanup(ira_pec_t * pec) {
	ira_lo_destroy(pec->root);

	destroy_dt_chain(pec, pec->dt_ptr);
	destroy_dt_chain(pec, pec->dt_arr);
	destroy_dt_chain(pec, pec->dt_func);

	memset(pec, 0, sizeof(*pec));
}

static bool get_listed_dt_assert(get_listed_dt_pred_t * pred) {
	switch (pred->type) {
		case IraDtPtr:
			if (!ira_dt_is_ptr_dt_comp(pred->ptr.body)) {
				return false;
			}
			break;
		case IraDtArr:
			if (!ira_dt_is_arr_dt_comp(pred->ptr.body)) {
				return false;
			}
			break;
		case IraDtFunc:
			if (!ira_dt_is_func_dt_comp(pred->func.ret)) {
				return false;
			}

			for (ira_dt_n_t * arg = pred->func.args, *arg_end = arg + pred->func.args_size; arg != arg_end; ++arg) {
				if (!ira_dt_is_func_dt_comp(arg->dt)) {
					return false;
				}

				for (ira_dt_n_t * arg2 = arg + 1; arg2 != arg_end; ++arg2) {
					if (arg->name == arg2->name) {
						return false;
					}
				}
			}
			break;
		default:
			u_assert_switch(pred->type);
	}

	return true;
}
static bool get_listed_dt_cmp(get_listed_dt_pred_t * pred, ira_dt_t * dt) {
	switch (pred->type) {
		case IraDtPtr:
			if (dt->ptr.body != pred->ptr.body) {
				return false;
			}
			break;
		case IraDtArr:
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

			for (ira_dt_n_t * arg = dt->func.args, *arg_end = arg + dt->func.args_size, *pred_arg = pred->func.args; arg != arg_end; ++arg, ++arg_end) {
				if (arg->dt != pred_arg->dt || arg->name != pred_arg->name) {
					return false;
				}
			}
			break;
		default:
			u_assert_switch(type);
	}

	return true;
}
static void get_listed_dt_copy(get_listed_dt_pred_t * pred, ira_dt_t * out) {
	switch (pred->type) {
		case IraDtPtr:
			*out = (ira_dt_t){ .type = IraDtPtr, .ptr.body = pred->ptr.body };
			break;
		case IraDtArr:
			*out = (ira_dt_t){ .type = IraDtArr, .arr.body = pred->arr.body };
			break;
		case IraDtFunc:
		{
			ira_dt_n_t * new_dt_args = malloc(pred->func.args_size * sizeof(*new_dt_args));

			u_assert(new_dt_args != NULL);

			memcpy(new_dt_args, pred->func.args, pred->func.args_size * sizeof(*new_dt_args));

			*out = (ira_dt_t){ .type = IraDtFunc, .func = { .args_size = pred->func.args_size, .args = new_dt_args, .ret = pred->func.ret } };
			break;
		}
		default:
			u_assert_switch(type);
	}
}
static ira_dt_t * get_listed_dt(ira_pec_t * pec, get_listed_dt_pred_t * pred, ira_dt_t ** ins) {
	u_assert(get_listed_dt_assert(pred));

	while (*ins != NULL) {
		ira_dt_t * dt = *ins;

		if (get_listed_dt_cmp(pred, dt)) {
			return dt;
		}

		ins = &dt->next;
	}

	ira_dt_t * new_dt = malloc(sizeof(*new_dt));

	u_assert(new_dt != NULL);

	get_listed_dt_copy(pred, new_dt);

	*ins = new_dt;

	return new_dt;
}

ira_dt_t * ira_pec_get_dt_ptr(ira_pec_t * pec, ira_dt_t * body) {
	get_listed_dt_pred_t pred = { .type = IraDtPtr, .ptr.body = body };

	return get_listed_dt(pec, &pred, &pec->dt_ptr);
}
ira_dt_t * ira_pec_get_dt_arr(ira_pec_t * pec, ira_dt_t * body) {
	get_listed_dt_pred_t pred = { .type = IraDtArr, .arr.body = body };

	return get_listed_dt(pec, &pred, &pec->dt_arr);
}
ira_dt_t * ira_pec_get_dt_func(ira_pec_t * pec, ira_dt_t * ret, size_t args_size, ira_dt_n_t * args) {
	get_listed_dt_pred_t pred = { .type = IraDtFunc, .func = { .ret = ret, .args_size = args_size, .args = args } };

	return get_listed_dt(pec, &pred, &pec->dt_func);
}

ira_val_t * ira_pec_make_val_imm_dt(ira_pec_t * pec, ira_dt_t * dt) {
	ira_val_t * val = ira_val_create(IraValImmDt, &pec->dt_dt);

	val->dt_val = dt;

	return val;
}
ira_val_t * ira_pec_make_val_imm_bool(ira_pec_t * pec, bool bool_val) {
	ira_val_t * val = ira_val_create(IraValImmBool, &pec->dt_bool);

	val->bool_val = bool_val;

	return val;
}
ira_val_t * ira_pec_make_val_imm_int(ira_pec_t * pec, ira_int_type_t int_type, ira_int_t int_val) {
	u_assert(int_type < IraInt_Count);

	ira_val_t * val = ira_val_create(IraValImmInt, &pec->dt_ints[int_type]);

	val->int_val = int_val;

	return val;
}
ira_val_t * ira_pec_make_val_lo_ptr(ira_pec_t * pec, ira_lo_t * lo) {
	ira_dt_t * val_dt;

	switch (lo->type) {
		case IraLoFunc:
			val_dt = ira_pec_get_dt_ptr(pec, lo->func->dt);
			break;
		case IraLoImpt:
			val_dt = ira_pec_get_dt_ptr(pec, lo->impt.dt);
			break;
		default:
			u_assert_switch(lo->type);
	}

	ira_val_t * val = ira_val_create(IraValLoPtr, val_dt);

	val->lo_val = lo;

	return val;
}
