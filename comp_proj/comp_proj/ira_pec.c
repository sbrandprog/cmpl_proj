#include "pch.h"
#include "ira_val.h"
#include "ira_inst.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec.h"


static ira_dt_stct_tag_t * create_dt_stct_tag() {
	ira_dt_stct_tag_t * tag = malloc(sizeof(*tag));

	ul_assert(tag != NULL);

	*tag = (ira_dt_stct_tag_t){ 0 };

	return tag;
}
static void destroy_dt_stct_tag(ira_dt_stct_tag_t * tag) {
	if (tag == NULL) {
		return;
	}

	free(tag);
}
static void destroy_dt_stct_tag_chain(ira_dt_stct_tag_t * tag) {
	while (tag != NULL) {
		ira_dt_stct_tag_t * next = tag->next;

		destroy_dt_stct_tag(tag);

		tag = next;
	}
}


static void destroy_dt_chain(ira_pec_t * pec, ira_dt_t * dt) {
	while (dt != NULL) {
		ira_dt_t * next = dt->next;

		switch (dt->type) {
			case IraDtVec:
			case IraDtPtr:
				break;
			case IraDtTpl:
				free(dt->tpl.elems);
				break;
			case IraDtStct:
			case IraDtArr:
				break;
			case IraDtFunc:
				free(dt->func.args);
				break;
			default:
				ul_assert_unreachable();
		}

		free(dt);

		dt = next;
	}
}


static ira_optr_t * create_optr_inst(ira_optr_impl_type_t impl_type, ira_inst_type_t inst_type, ira_int_cmp_t int_cmp) {
	ira_optr_t * optr = ira_optr_create(impl_type);

	optr->inst.type = inst_type;
	optr->inst.int_cmp = int_cmp;

	return optr;
}


static ira_lo_t * create_lo(ira_lo_type_t type, ul_hs_t * name) {
	ira_lo_t * lo = malloc(sizeof(*lo));

	ul_assert(lo != NULL);

	*lo = (ira_lo_t){ .type = type, .name = name };

	return lo;
}
static void destroy_lo(ira_lo_t * lo) {
	if (lo == NULL) {
		return;
	}

	switch (lo->type) {
		case IraLoNone:
			break;
		case IraLoNspc:
			ira_lo_destroy_nspc_node_chain(lo->nspc.body);
			break;
		case IraLoFunc:
			ira_func_destroy(lo->func);
			break;
		case IraLoImpt:
			break;
		case IraLoVar:
			ira_val_destroy(lo->var.val);
			break;
		default:
			ul_assert_unreachable();
	}

	free(lo);
}
static void destroy_lo_chain(ira_lo_t * lo) {
	while (lo != NULL) {
		ira_lo_t * next = lo->next;

		destroy_lo(lo);

		lo = next;
	}
}


static ira_optr_t ** get_optr_ins(ira_pec_t * pec, ira_optr_type_t optr_type, ira_optr_t * pred) {
	ira_optr_t ** ins = &pec->optrs[optr_type];

	for (; *ins != NULL; ins = &(*ins)->next) {
		if (ira_optr_is_equivalent(*ins, pred)) {
			return ins;
		}
	}

	return ins;
}
static void register_bltn_optr_inst(ira_pec_t * pec, ira_optr_type_t optr_type, ira_optr_impl_type_t impl_type, ira_inst_type_t inst_type, ira_int_cmp_t int_cmp) {
	switch (impl_type) {
		case IraOptrImplInstUnrBool:
		case IraOptrImplInstUnrInt:
		case IraOptrImplInstBinInt:
		case IraOptrImplInstBinIntBool:
		case IraOptrImplInstBinPtrBool:
			break;
		default:
			ul_assert_unreachable();
	}

	ira_optr_t pred = { .impl_type = impl_type, .inst = { .type = inst_type, .int_cmp = int_cmp } };

	ira_optr_t ** ins = get_optr_ins(pec, optr_type, &pred);

	ul_assert(*ins == NULL);

	*ins = ira_optr_copy(&pred);
}
static void register_bltn_optrs(ira_pec_t * pec) {
	register_bltn_optr_inst(pec, IraOptrLogicNeg, IraOptrImplInstUnrBool, IraInstNegBool, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrArithNeg, IraOptrImplInstUnrInt, IraInstNegInt, IraIntCmp_Count);

	register_bltn_optr_inst(pec, IraOptrAdd, IraOptrImplInstBinInt, IraInstAddInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrSub, IraOptrImplInstBinInt, IraInstSubInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrMul, IraOptrImplInstBinInt, IraInstMulInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrDiv, IraOptrImplInstBinInt, IraInstDivInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrMod, IraOptrImplInstBinInt, IraInstModInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrLeShift, IraOptrImplInstBinInt, IraInstLeShiftInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrRiShift, IraOptrImplInstBinInt, IraInstRiShiftInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrBitAnd, IraOptrImplInstBinInt, IraInstAndInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrBitXor, IraOptrImplInstBinInt, IraInstXorInt, IraIntCmp_Count);
	register_bltn_optr_inst(pec, IraOptrBitOr, IraOptrImplInstBinInt, IraInstOrInt, IraIntCmp_Count);

	register_bltn_optr_inst(pec, IraOptrLess, IraOptrImplInstBinIntBool, IraInstCmpInt, IraIntCmpLess);
	register_bltn_optr_inst(pec, IraOptrLessEq, IraOptrImplInstBinIntBool, IraInstCmpInt, IraIntCmpLessEq);
	register_bltn_optr_inst(pec, IraOptrEq, IraOptrImplInstBinIntBool, IraInstCmpInt, IraIntCmpEq);
	register_bltn_optr_inst(pec, IraOptrGrtr, IraOptrImplInstBinIntBool, IraInstCmpInt, IraIntCmpGrtr);
	register_bltn_optr_inst(pec, IraOptrGrtrEq, IraOptrImplInstBinIntBool, IraInstCmpInt, IraIntCmpGrtrEq);
	register_bltn_optr_inst(pec, IraOptrNeq, IraOptrImplInstBinIntBool, IraInstCmpInt, IraIntCmpNeq);

	register_bltn_optr_inst(pec, IraOptrLess, IraOptrImplInstBinPtrBool, IraInstCmpPtr, IraIntCmpLess);
	register_bltn_optr_inst(pec, IraOptrLessEq, IraOptrImplInstBinPtrBool, IraInstCmpPtr, IraIntCmpLessEq);
	register_bltn_optr_inst(pec, IraOptrEq, IraOptrImplInstBinPtrBool, IraInstCmpPtr, IraIntCmpEq);
	register_bltn_optr_inst(pec, IraOptrGrtr, IraOptrImplInstBinPtrBool, IraInstCmpPtr, IraIntCmpGrtr);
	register_bltn_optr_inst(pec, IraOptrGrtrEq, IraOptrImplInstBinPtrBool, IraInstCmpPtr, IraIntCmpGrtrEq);
	register_bltn_optr_inst(pec, IraOptrNeq, IraOptrImplInstBinPtrBool, IraInstCmpPtr, IraIntCmpNeq);
}

bool ira_pec_init(ira_pec_t * pec, ul_hst_t * hst) {
	*pec = (ira_pec_t){ .hst = hst };

	ul_hsb_init(&pec->hsb);

	for (ira_pds_t pds = 0; pds < IraPds_Count; ++pds) {
		const ul_ros_t * pds_str = &ira_pds_strs[pds];

		pec->pds[pds] = ul_hst_hashadd(pec->hst, pds_str->size, pds_str->str);
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

	register_bltn_optrs(pec);

	pec->ep_name = pec->pds[IraPdsEpName];

	return true;
}
void ira_pec_cleanup(ira_pec_t * pec) {
	destroy_lo_chain(pec->lo);

	for (ira_optr_type_t optr = 0; optr < IraOptr_Count; ++optr) {
		ira_optr_destroy_chain(pec->optrs[optr]);
	}

	destroy_dt_chain(pec, pec->dt_vec);
	destroy_dt_chain(pec, pec->dt_ptr);
	destroy_dt_chain(pec, pec->dt_tpl);
	destroy_dt_chain(pec, pec->dt_stct);
	destroy_dt_chain(pec, pec->dt_arr);
	destroy_dt_chain(pec, pec->dt_func);

	destroy_dt_stct_tag_chain(pec->dt_stct_tag);

	ul_hsb_cleanup(&pec->hsb);

	memset(pec, 0, sizeof(*pec));
}


bool ira_pec_is_dt_complete(ira_dt_t * dt) {
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
		case IraDtVec:
		case IraDtPtr:
		case IraDtTpl:
			break;
		case IraDtStct:
			if (dt->stct.tag->tpl == NULL) {
				return false;
			}
			break;
		case IraDtArr:
		case IraDtFunc:
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}

bool ira_pec_get_dt_size(ira_dt_t * dt, size_t * out) {
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
			if (!ira_pec_get_dt_size(dt->vec.body, out)) {
				return false;
			}

			*out *= dt->vec.size;

			break;
		case IraDtPtr:
			*out = 8;
			break;
		case IraDtTpl:
		{
			size_t size = 0, align = 1;

			for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
				size_t elem_align, elem_size;

				if (!ira_pec_get_dt_align(elem->dt, &elem_align) || !ira_pec_get_dt_size(elem->dt, &elem_size)) {
					return false;
				}

				size = ul_align_to(size, elem_align);
				size += elem_size;

				align = max(align, elem_align);
			}

			size = ul_align_to(size, align);

			*out = size;

			break;
		}
		case IraDtStct:
		{
			ira_dt_t * tpl = dt->stct.tag->tpl;

			if (tpl == NULL || !ira_pec_get_dt_size(tpl, out)) {
				return false;
			}

			break;
		}
		case IraDtArr:
			if (!ira_pec_get_dt_size(dt->arr.assoc_tpl, out)) {
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
bool ira_pec_get_dt_align(ira_dt_t * dt, size_t * out) {
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
			if (!ira_pec_get_dt_align(dt->vec.body, out)) {
				return false;
			}
			break;
		case IraDtPtr:
			*out = 8;
			break;
		case IraDtTpl:
		{
			size_t align = 1;

			for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem) {
				size_t elem_align;

				if (!ira_pec_get_dt_align(elem->dt, &elem_align)) {
					return false;
				}

				align = max(align, elem_align);
			}

			*out = align;

			break;
		}
		case IraDtStct:
		{
			ira_dt_t * tpl = dt->stct.tag->tpl;

			if (tpl == NULL || !ira_pec_get_dt_align(tpl, out)) {
				return false;
			}

			break;
		}
		case IraDtArr:
			if (!ira_pec_get_dt_align(dt->arr.assoc_tpl, out)) {
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

size_t ira_pec_get_tpl_elem_off(ira_dt_t * dt, size_t elem_i) {
	ul_assert(dt->type == IraDtTpl && elem_i < dt->tpl.elems_size);

	size_t off = 0;

	for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + elem_i; elem != elem_end; ++elem) {
		size_t elem_align, elem_size;

		if (!ira_pec_get_dt_align(elem->dt, &elem_align) || !ira_pec_get_dt_size(elem->dt, &elem_size)) {
			ul_assert_unreachable();
		}

		off = ul_align_to(off, elem_align);
		off += elem_size;
	}

	return off;
}


ira_dt_stct_tag_t * ira_pec_get_dt_stct_tag(ira_pec_t * pec) {
	ira_dt_stct_tag_t * tag = create_dt_stct_tag();

	tag->next = pec->dt_stct_tag;
	pec->dt_stct_tag = tag;

	return tag;
}

static bool get_listed_dt_check(ira_pec_t * pec, ira_dt_t * pred) {
	switch (pred->type) {
		case IraDtVec:
			if (!ira_pec_is_dt_complete(pred->vec.body)) {
				return false;
			}
			break;
		case IraDtPtr:
			break;
		case IraDtTpl:
			for (ira_dt_ndt_t * elem = pred->tpl.elems, *elem_end = elem + pred->tpl.elems_size; elem != elem_end; ++elem) {
				if (!ira_pec_is_dt_complete(elem->dt)) {
					return false;
				}
			}
			break;
		case IraDtStct:
			break;
		case IraDtArr:
			if (!ira_pec_is_dt_complete(pred->arr.body)) {
				return false;
			}
			break;
		case IraDtFunc:
			if (!ira_pec_is_dt_complete(pred->func.ret)) {
				return false;
			}

			for (ira_dt_ndt_t * arg = pred->func.args, *arg_end = arg + pred->func.args_size; arg != arg_end; ++arg) {
				if (!ira_pec_is_dt_complete(arg->dt) || arg->name == NULL) {
					return false;
				}
			}
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool get_listed_dt_cmp(ira_pec_t * pec, ira_dt_t * pred, ira_dt_t * dt) {
	ul_assert(pred->type == dt->type);

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
		case IraDtTpl:
			if (!ira_dt_is_qual_equal(dt->tpl.qual, pred->tpl.qual)) {
				return false;
			}

			if (dt->tpl.elems_size != pred->tpl.elems_size) {
				return false;
			}

			for (ira_dt_ndt_t * elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size, *pred_elem = pred->tpl.elems; elem != elem_end; ++elem, ++pred_elem) {
				if (elem->dt != pred_elem->dt || elem->name != pred_elem->name) {
					return false;
				}
			}
			break;
		case IraDtStct:
			if (!ira_dt_is_qual_equal(dt->stct.qual, pred->stct.qual)) {
				return false;
			}

			if (dt->stct.tag != pred->stct.tag) {
				return false;
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
			ul_assert_unreachable();
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
		case IraDtTpl:
		{
			*out = (ira_dt_t){ .type = pred->type, .tpl = { .qual = pred->tpl.qual } };

			out->tpl.elems = malloc(sizeof(*out->tpl.elems) * pred->tpl.elems_size);

			ul_assert(out->tpl.elems != NULL);

			memcpy(out->tpl.elems, pred->tpl.elems, sizeof(*out->tpl.elems) * pred->tpl.elems_size);

			out->tpl.elems_size = pred->tpl.elems_size;

			break;
		}
		case IraDtStct:
			*out = (ira_dt_t){ .type = pred->type, .stct = { .tag = pred->stct.tag, .qual = pred->stct.qual } };
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

			if (!ira_pec_get_dt_tpl(pec, _countof(elems), elems, pred->arr.qual, &out->arr.assoc_tpl)) {
				return false;
			}
			break;
		}
		case IraDtFunc:
		{
			ira_dt_ndt_t * new_args = malloc(pred->func.args_size * sizeof(*new_args));

			ul_assert(new_args != NULL);

			memcpy(new_args, pred->func.args, pred->func.args_size * sizeof(*new_args));

			*out = (ira_dt_t){ .type = pred->type, .func = { .args_size = pred->func.args_size, .args = new_args, .ret = pred->func.ret } };
			break;
		}
		default:
			ul_assert_unreachable();
	}

	return true;
}
static bool get_listed_dt(ira_pec_t * pec, ira_dt_t * pred, ira_dt_t ** ins, ira_dt_t ** out) {
	if (!get_listed_dt_check(pec, pred)) {
		return false;
	}

	while (*ins != NULL) {
		ira_dt_t * dt = *ins;

		if (get_listed_dt_cmp(pec, pred, dt)) {
			*out = dt;
			return true;
		}

		ins = &dt->next;
	}

	ira_dt_t * new_dt = malloc(sizeof(*new_dt));

	ul_assert(new_dt != NULL);

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
bool ira_pec_get_dt_tpl(ira_pec_t * pec, size_t elems_size, ira_dt_ndt_t * elems, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtTpl, .tpl = { .elems_size = elems_size, .elems = elems, .qual = qual } };

	return get_listed_dt(pec, &pred, &pec->dt_tpl, out);
}
bool ira_pec_get_dt_stct(ira_pec_t * pec, ira_dt_stct_tag_t * tag, ira_dt_qual_t qual, ira_dt_t ** out) {
	ira_dt_t pred = { .type = IraDtStct, .stct = { .tag = tag, .qual = qual } };

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
	ira_dt_qual_t dt_qual = ira_dt_get_qual(dt);

	qual = ira_dt_apply_qual(dt_qual, qual);

	if (ira_dt_is_qual_equal(dt_qual, qual)) {
		*out = dt;
		return true;
	}
	
	switch (dt->type) {
		case IraDtVoid:
		case IraDtDt:
		case IraDtBool:
		case IraDtInt:
			*out = dt;
			break;
		case IraDtVec:
			if (!ira_pec_get_dt_vec(pec, dt->vec.size, dt->vec.body, qual, out)) {
				return false;
			}
			break;
		case IraDtPtr:
			if (!ira_pec_get_dt_ptr(pec, dt->ptr.body, qual, out)) {
				return false;
			}
			break;
		case IraDtTpl:
			if (!ira_pec_get_dt_tpl(pec, dt->tpl.elems_size, dt->tpl.elems, qual, out)) {
				return false;
			}
			break;
		case IraDtStct:
			if (!ira_pec_get_dt_stct(pec, dt->stct.tag, qual, out)) {
				return false;
			}
			break;
		case IraDtArr:
			if (!ira_pec_get_dt_arr(pec, dt->arr.body, qual, out)) {
				return false;
			}
			break;
		case IraDtFunc:
			*out = dt;
			break;
		default:
			ul_assert_unreachable();
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
	ul_assert(int_type < IraInt_Count);

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
			ul_assert_unreachable();
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
			val_type = IraValImmPtr;
			break;
		case IraDtTpl:
			val_type = IraValImmTpl;
			break;
		case IraDtStct:
			if (dt->stct.tag->tpl == NULL) {
				return false;
			}

			val_type = IraValImmStct;
			break;
		case IraDtArr:
			val_type = IraValImmArr;
			break;
		default:
			ul_assert_unreachable();
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
			(*out)->arr_val.data = malloc(dt->vec.size * sizeof(*(*out)->arr_val.data));

			ul_assert((*out)->arr_val.data != NULL);

			memset((*out)->arr_val.data, 0, dt->vec.size * sizeof(*(*out)->arr_val.data));

			if (dt->vec.size > 0) {
				if (!ira_pec_make_val_null(pec, dt->vec.body, &(*out)->arr_val.data[0])) {
					return false;
				}

				ira_val_t * ref_val = (*out)->arr_val.data[0];

				ira_val_t ** elem = (*out)->arr_val.data, ** elem_end = elem + dt->vec.size;

				++elem;

				while (elem != elem_end) {
					*elem++ = ira_val_copy(ref_val);
				}
			}

			break;
		case IraDtPtr:
			(*out)->int_val.ui64 = 0;
			break;
		case IraDtTpl:
		{
			(*out)->arr_val.data = malloc(dt->tpl.elems_size * sizeof((*out)->arr_val.data));

			ul_assert((*out)->arr_val.data != NULL);

			memset((*out)->arr_val.data, 0, dt->tpl.elems_size * sizeof((*out)->arr_val.data));

			ira_val_t ** elem = (*out)->arr_val.data, ** elem_end = elem + dt->tpl.elems_size;
			ira_dt_ndt_t * elem_dt = dt->tpl.elems;

			for (; elem != elem_end; ++elem, ++elem_dt) {
				if (!ira_pec_make_val_null(pec, elem_dt->dt, elem)) {
					return false;
				}
			}
			break;
		}
		case IraDtStct:
			if (!ira_pec_make_val_null(pec, dt->stct.tag->tpl, &(*out)->val_val)) {
				return false;
			}
			break;
		case IraDtArr:
		{
			(*out)->arr_val.size = 0;

			size_t arr_size_bytes = (*out)->arr_val.size * sizeof((*out)->arr_val.data);

			(*out)->arr_val.data = malloc(arr_size_bytes);

			ul_assert((*out)->arr_val.data != NULL);

			memset((*out)->arr_val.data, 0, arr_size_bytes);

			break;
		}
		default:
			ul_assert_unreachable();
	}

	return true;
}


bool ira_pec_get_optr_dt(ira_pec_t * pec, ira_optr_t * optr, ira_dt_t * first, ira_dt_t * second, ira_dt_t ** out) {
	switch (optr->impl_type) {
		case IraOptrImplInstUnrBool:
			if (first->type != IraDtBool) {
				return false;
			}

			*out = first;
			break;
		case IraOptrImplInstUnrInt:
			if (first->type != IraDtInt) {
				return false;
			}

			*out = first;
			break;
		case IraOptrImplInstBinInt:
			if (first->type != IraDtInt || !ira_dt_is_equivalent(first, second)) {
				return false;
			}

			*out = first;
			break;
		case IraOptrImplInstBinIntBool:
			if (first->type != IraDtInt || !ira_dt_is_equivalent(first, second)) {
				return false;
			}

			*out = &pec->dt_bool;
			break;
		case IraOptrImplInstBinPtrBool:
			if (first->type != IraDtPtr || !ira_dt_is_equivalent(first, second)) {
				return false;
			}

			*out = &pec->dt_bool;
			break;
		default:
			ul_assert_unreachable();
	}

	return true;
}
bool ira_pec_get_best_optr(ira_pec_t * pec, ira_optr_type_t optr_type, ira_dt_t * first, ira_dt_t * second, ira_optr_t ** optr_out, ira_dt_t ** res_dt_out) {
	for (ira_optr_t * optr = pec->optrs[optr_type]; optr != NULL; optr = optr->next) {
		ira_dt_t * res_dt;

		if (ira_pec_get_optr_dt(pec, optr, first, second, &res_dt)) {
			*optr_out = optr;
			*res_dt_out = res_dt;

			return true;
		}
	}

	return false;
}


ira_lo_t * ira_pec_push_unq_lo(ira_pec_t * pec, ira_lo_type_t type, ul_hs_t * hint_name) {
	ul_assert(type < IraLo_Count);

	static const wchar_t * const lo_type_names[IraLo_Count] = {
		[IraLoNone] = L"none",
		[IraLoNspc] = L"nspc",
		[IraLoFunc] = L"func",
		[IraLoImpt] = L"impt",
		[IraLoVar] = L"var"
	};

	const wchar_t * type_name = lo_type_names[type];

	ul_assert(type_name != NULL);

	ul_hs_t * unq_name;
	
	if (hint_name != NULL) {
		unq_name = ul_hsb_formatadd(&pec->hsb, pec->hst, L"%s:%s%zi", hint_name->str, type_name, pec->lo_index++);
	}
	else {
		unq_name = ul_hsb_formatadd(&pec->hsb, pec->hst, L"%s%zi", type_name, pec->lo_index++);
	}

	ira_lo_t * lo = create_lo(type, unq_name);

	lo->next = pec->lo;
	pec->lo = lo;

	return lo;
}
