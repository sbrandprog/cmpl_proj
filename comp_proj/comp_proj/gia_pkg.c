#include "pch.h"
#include "gia_tus.h"
#include "gia_pkg.h"

gia_pkg_t * gia_pkg_create(ul_hs_t * name) {
	gia_pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);
	
	*pkg = (gia_pkg_t){ .name = name };

	return pkg;
}
void gia_pkg_destroy(gia_pkg_t * pkg) {
	if (pkg == NULL) {
		return;
	}

	gia_pkg_destroy_chain(pkg->sub_pkg);

	gia_tus_destroy_chain(pkg->tus);

	free(pkg);
}
void gia_pkg_destroy_chain(gia_pkg_t * pkg) {
	while (pkg != NULL) {
		gia_pkg_t * next = pkg->next;

		gia_pkg_destroy(pkg);

		pkg = next;
	}
}

gia_pkg_t * gia_pkg_get_sub_pkg(gia_pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	gia_pkg_t ** sub_pkg_ins = &pkg->sub_pkg;

	while (*sub_pkg_ins != NULL) {
		gia_pkg_t * sub_pkg = *sub_pkg_ins;

		if (sub_pkg->name == sub_pkg_name) {
			return sub_pkg;
		}

		sub_pkg_ins = &sub_pkg->next;
	}

	*sub_pkg_ins = gia_pkg_create(sub_pkg_name);

	return *sub_pkg_ins;
}
gia_tus_t * gia_pkg_get_tus(gia_pkg_t * pkg, ul_hs_t * tus_name) {
	gia_tus_t ** tus_ins = &pkg->tus;

	while (*tus_ins != NULL) {
		gia_tus_t * tus = *tus_ins;

		if (tus->name == tus_name) {
			return tus;
		}

		tus_ins = &tus->next;
	}

	*tus_ins = gia_tus_create(tus_name);

	return *tus_ins;
}
