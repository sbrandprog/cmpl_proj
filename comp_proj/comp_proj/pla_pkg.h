#pragma once
#include "pla.h"

struct pla_pkg {
	ul_hs_t * name;

	pla_pkg_t * next;

	pla_pkg_t * sub_pkg;

	pla_tu_t * tu;
};

pla_pkg_t * pla_pkg_create(ul_hs_t * name);
void pla_pkg_destroy(pla_pkg_t * pkg);
void pla_pkg_destroy_chain(pla_pkg_t * pkg);

pla_pkg_t * pla_pkg_get_sub_pkg(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name);
pla_tu_t * pla_pkg_get_tu(pla_pkg_t * pkg, ul_hs_t * tu_name);
