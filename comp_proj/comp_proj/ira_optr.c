#include "pch.h"
#include "ira_dt.h"
#include "ira_optr.h"

ira_optr_t * ira_optr_create(ira_optr_impl_type_t impl_type) {
	ira_optr_t * optr = malloc(sizeof(*optr));

	ul_assert(optr != NULL);

	*optr = (ira_optr_t){ .impl_type = impl_type };

	return optr;
}
void ira_optr_destroy(ira_optr_t * optr) {
	if (optr == NULL) {
		return;
	}

	switch (optr->impl_type) {
		case IraOptrImplNone:
		case IraOptrImplInstUnrBool:
		case IraOptrImplInstUnrInt:
		case IraOptrImplInstBinInt:
		case IraOptrImplInstBinIntBool:
		case IraOptrImplInstBinPtrBool:
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

	switch (first->impl_type) {
		case IraOptrImplNone:
			break;
		case IraOptrImplInstUnrBool:
			switch (second->impl_type) {
				case IraOptrImplInstUnrBool:
					return true;
			}
			break;
		case IraOptrImplInstUnrInt:
			switch (second->impl_type) {
				case IraOptrImplInstUnrInt:
					return true;
			}
			break;
		case IraOptrImplInstBinInt:
		case IraOptrImplInstBinIntBool:
			switch (second->impl_type) {
				case IraOptrImplInstBinInt:
				case IraOptrImplInstBinIntBool:
					return true;
			}
			break;
		case IraOptrImplInstBinPtrBool:
			switch (second->impl_type) {
				case IraOptrImplInstBinPtrBool:
					return true;
			}
			break;
		default:
			ul_assert_unreachable();
	}

	return false;
}

ira_optr_t * ira_optr_copy(ira_optr_t * optr) {
	ira_optr_t * new_optr = ira_optr_create(optr->impl_type);

	switch (optr->impl_type) {
		case IraOptrImplNone:
			break;
		case IraOptrImplInstUnrBool:
		case IraOptrImplInstUnrInt:
		case IraOptrImplInstBinInt:
		case IraOptrImplInstBinIntBool:
		case IraOptrImplInstBinPtrBool:
			new_optr->inst = optr->inst;
			break;
		default:
			ul_assert_unreachable();
	}

	return new_optr;
}
