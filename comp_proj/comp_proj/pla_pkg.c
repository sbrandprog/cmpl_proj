#include "pch.h"
#include "pla_tu.h"
#include "pla_pkg.h"

pla_pkg_t * pla_pkg_create(ul_hs_t * name) {
	pla_pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);

	*pkg = (pla_pkg_t){ .name = name };

	return pkg;
}
void pla_pkg_destroy(pla_pkg_t * pkg) {
	if (pkg == NULL) {
		return;
	}

	pla_tu_destroy_chain(pkg->tu);

	pla_pkg_destroy_chain(pkg->sub_pkg);

	free(pkg);
}
void pla_pkg_destroy_chain(pla_pkg_t * pkg) {
	while (pkg != NULL) {
		pla_pkg_t * next = pkg->next;

		pla_pkg_destroy(pkg);

		pkg = next;
	}
}

pla_pkg_t * pla_pkg_get_sub_pkg(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	pla_pkg_t ** sub_pkg_ins = &pkg->sub_pkg;

	while (*sub_pkg_ins != NULL) {
		pla_pkg_t * sub_pkg = *sub_pkg_ins;

		if (sub_pkg->name == sub_pkg_name) {
			return sub_pkg;
		}

		sub_pkg_ins = &sub_pkg->next;
	}

	*sub_pkg_ins = pla_pkg_create(sub_pkg_name);

	return *sub_pkg_ins;
}
pla_tu_t * pla_pkg_get_tu(pla_pkg_t * pkg, ul_hs_t * tu_name) {
	pla_tu_t ** tu_ins = &pkg->tu;

	while (*tu_ins != NULL) {
		pla_tu_t * tu = *tu_ins;

		if (tu->name == tu_name) {
			return tu;
		}

		tu_ins = &tu->next;
	}

	*tu_ins = pla_tu_create(tu_name);

	return *tu_ins;
}
