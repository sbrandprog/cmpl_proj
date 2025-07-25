#pragma once
#include "pla.h"

struct pla_pkg
{
    ul_hs_t * name;

    pla_pkg_t * next;

    pla_pkg_t * sub_pkg;

    pla_tus_t * tus;
};

PLA_API pla_pkg_t * pla_pkg_create(ul_hs_t * name);
PLA_API void pla_pkg_destroy(pla_pkg_t * pkg);
PLA_API void pla_pkg_destroy_chain(pla_pkg_t * pkg);

PLA_API pla_pkg_t * pla_pkg_find_sub_pkg(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name);
PLA_API pla_tus_t * pla_pkg_find_tus(pla_pkg_t * pkg, ul_hs_t * tus_name);

PLA_API pla_pkg_t * pla_pkg_get_sub_pkg(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name);
PLA_API pla_tus_t * pla_pkg_get_tus(pla_pkg_t * pkg, ul_hs_t * tus_name);

PLA_API ul_hs_t * pla_pkg_get_tus_name_from_path(ul_hst_t * hst, const char * path);

PLA_API bool pla_pkg_fill_from_dir(pla_pkg_t * pkg, ul_hst_t * hst, const char * dir_path);
PLA_API bool pla_pkg_fill_from_list(pla_pkg_t * pkg, ul_hst_t * hst, ...);
