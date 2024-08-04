#pragma once
#include "ira.h"

enum ira_optr_type {
	IraOptrNone,
	IraOptrLogicNeg,
	IraOptrBitNeg,
	IraOptrArithNeg,
	IraOptrMul,
	IraOptrDiv,
	IraOptrMod,
	IraOptrAdd,
	IraOptrSub,
	IraOptrLeShift,
	IraOptrRiShift,
	IraOptrLess,
	IraOptrLessEq,
	IraOptrGrtr,
	IraOptrGrtrEq,
	IraOptrEq,
	IraOptrNeq,
	IraOptrBitAnd,
	IraOptrBitOr,
	IraOptrBitXor,
	IraOptrLogicAnd,
	IraOptrLogicOr,
	IraOptr_Count
};
enum ira_optr_impl_type {
	IraOptrImplNone,
	IraOptrImplInstUnrBool,
	IraOptrImplInstUnrInt,
	IraOptrImplInstBinInt,
	IraOptrImplInstBinIntBool,
	IraOptrImplInstBinPtrBool,
	IraOptrImpl_Count
};
struct ira_optr {
	ira_optr_impl_type_t impl_type;

	ira_optr_t * next;

	union {
		struct {
			ira_inst_type_t type;
			ira_int_cmp_t int_cmp;
		} inst;
	};
};

IRA_API ira_optr_t * ira_optr_create(ira_optr_impl_type_t impl_type);
IRA_API void ira_optr_destroy(ira_optr_t * optr);
IRA_API void ira_optr_destroy_chain(ira_optr_t * optr);

IRA_API ira_optr_t * ira_optr_copy(ira_optr_t * optr);

IRA_API bool ira_optr_is_equivalent(ira_optr_t * first, ira_optr_t * second);
