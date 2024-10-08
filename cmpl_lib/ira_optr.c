#include "ira_dt.h"
#include "ira_optr.h"

ira_optr_t * ira_optr_create(ira_optr_type_t type) {
	ira_optr_t * optr = malloc(sizeof(*optr));

	ul_assert(optr != NULL);

	*optr = (ira_optr_t){ .type = type };

	return optr;
}
void ira_optr_destroy(ira_optr_t * optr) {
	if (optr == NULL) {
		return;
	}

	switch (optr->type) {
		case IraOptrNone:
		case IraOptrBltnNegBool:
		case IraOptrBltnNegInt:
		case IraOptrBltnMulInt:
		case IraOptrBltnDivInt:
		case IraOptrBltnModInt:
		case IraOptrBltnAddInt:
		case IraOptrBltnSubInt:
		case IraOptrBltnLeShiftInt:
		case IraOptrBltnRiShiftInt:
		case IraOptrBltnLessInt:
		case IraOptrBltnLessEqInt:
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnEqInt:
		case IraOptrBltnNeqInt:
		case IraOptrBltnAndInt:
		case IraOptrBltnXorInt:
		case IraOptrBltnOrInt:
		case IraOptrBltnLessPtr:
		case IraOptrBltnLessEqPtr:
		case IraOptrBltnGrtrPtr:
		case IraOptrBltnGrtrEqPtr:
		case IraOptrBltnEqPtr:
		case IraOptrBltnNeqPtr:
			break;
		default:
			ul_assert_unreachable();
	}

	free(optr);
}
void ira_optr_destroy_chain(ira_optr_t * optr) {
	while (optr != NULL) {
		ira_optr_t * next = optr->next;

		ira_optr_destroy(optr);

		optr = next;
	}
}

bool ira_optr_is_equivalent(ira_optr_t * first, ira_optr_t * second) {
	if (first == second) {
		return true;
	}

	switch (first->type) {
		case IraOptrNone:
			break;
		case IraOptrBltnNegBool:
			switch (second->type) {
				case IraOptrBltnNegBool:
					return true;
			}
			break;
		case IraOptrBltnNegInt:
			switch (second->type) {
				case IraOptrBltnNegInt:
					return true;
			}
			break;
		case IraOptrBltnMulInt:
		case IraOptrBltnDivInt:
		case IraOptrBltnModInt:
		case IraOptrBltnAddInt:
		case IraOptrBltnSubInt:
		case IraOptrBltnLeShiftInt:
		case IraOptrBltnRiShiftInt:
		case IraOptrBltnLessInt:
		case IraOptrBltnLessEqInt:
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnEqInt:
		case IraOptrBltnNeqInt:
		case IraOptrBltnAndInt:
		case IraOptrBltnXorInt:
		case IraOptrBltnOrInt:
			switch (second->type) {
				case IraOptrBltnMulInt:
				case IraOptrBltnDivInt:
				case IraOptrBltnModInt:
				case IraOptrBltnAddInt:
				case IraOptrBltnSubInt:
				case IraOptrBltnLeShiftInt:
				case IraOptrBltnRiShiftInt:
				case IraOptrBltnLessInt:
				case IraOptrBltnLessEqInt:
				case IraOptrBltnGrtrInt:
				case IraOptrBltnGrtrEqInt:
				case IraOptrBltnEqInt:
				case IraOptrBltnNeqInt:
				case IraOptrBltnAndInt:
				case IraOptrBltnXorInt:
				case IraOptrBltnOrInt:
					return true;
			}
			break;
		case IraOptrBltnLessPtr:
		case IraOptrBltnLessEqPtr:
		case IraOptrBltnGrtrPtr:
		case IraOptrBltnGrtrEqPtr:
		case IraOptrBltnEqPtr:
		case IraOptrBltnNeqPtr:
			switch (second->type) {
				case IraOptrBltnLessPtr:
				case IraOptrBltnLessEqPtr:
				case IraOptrBltnGrtrPtr:
				case IraOptrBltnGrtrEqPtr:
				case IraOptrBltnEqPtr:
				case IraOptrBltnNeqPtr:
					return true;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	return false;
}

ira_optr_t * ira_optr_copy(ira_optr_t * optr) {
	ira_optr_t * new_optr = ira_optr_create(optr->type);

	switch (optr->type) {
		case IraOptrNone:
		case IraOptrBltnNegBool:
		case IraOptrBltnNegInt:
		case IraOptrBltnMulInt:
		case IraOptrBltnDivInt:
		case IraOptrBltnModInt:
		case IraOptrBltnAddInt:
		case IraOptrBltnSubInt:
		case IraOptrBltnLeShiftInt:
		case IraOptrBltnRiShiftInt:
		case IraOptrBltnLessInt:
		case IraOptrBltnLessEqInt:
		case IraOptrBltnGrtrInt:
		case IraOptrBltnGrtrEqInt:
		case IraOptrBltnEqInt:
		case IraOptrBltnNeqInt:
		case IraOptrBltnAndInt:
		case IraOptrBltnXorInt:
		case IraOptrBltnOrInt:
		case IraOptrBltnLessPtr:
		case IraOptrBltnLessEqPtr:
		case IraOptrBltnGrtrPtr:
		case IraOptrBltnGrtrEqPtr:
		case IraOptrBltnEqPtr:
		case IraOptrBltnNeqPtr:
			break;
		default:
			ul_assert_unreachable();
	}

	return new_optr;
}

const ira_optr_info_t ira_optr_infos[IraOptr_Count] = {
	[IraOptrNone] = { .is_bltn = false, .bltn = { .ctg = IraOptrCtgNone, .is_unr = false } },
	[IraOptrBltnNegBool] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgLogicNeg, .is_unr = true } },
	[IraOptrBltnNegInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgArithNeg, .is_unr = true } },
	[IraOptrBltnMulInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgMul, .is_unr = false } },
	[IraOptrBltnDivInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgDiv, .is_unr = false } },
	[IraOptrBltnModInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgMod, .is_unr = false } },
	[IraOptrBltnAddInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgAdd, .is_unr = false } },
	[IraOptrBltnSubInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgSub, .is_unr = false } },
	[IraOptrBltnLeShiftInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgLeShift, .is_unr = false } },
	[IraOptrBltnRiShiftInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgRiShift, .is_unr = false } },
	[IraOptrBltnLessInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgLess, .is_unr = false } },
	[IraOptrBltnLessEqInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgLessEq, .is_unr = false } },
	[IraOptrBltnGrtrInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgGrtr, .is_unr = false } },
	[IraOptrBltnGrtrEqInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgGrtrEq, .is_unr = false } },
	[IraOptrBltnEqInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgEq, .is_unr = false } },
	[IraOptrBltnNeqInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgNeq, .is_unr = false } },
	[IraOptrBltnAndInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgBitAnd, .is_unr = false } },
	[IraOptrBltnXorInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgBitXor, .is_unr = false } },
	[IraOptrBltnOrInt] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgBitOr, .is_unr = false } },
	[IraOptrBltnLessPtr] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgLess, .is_unr = false } },
	[IraOptrBltnLessEqPtr] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgLessEq, .is_unr = false } },
	[IraOptrBltnGrtrPtr] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgGrtr, .is_unr = false } },
	[IraOptrBltnGrtrEqPtr] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgGrtrEq, .is_unr = false } },
	[IraOptrBltnEqPtr] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgEq, .is_unr = false } },
	[IraOptrBltnNeqPtr] = { .is_bltn = true, .bltn = { .ctg = IraOptrCtgNeq, .is_unr = false } }
};
