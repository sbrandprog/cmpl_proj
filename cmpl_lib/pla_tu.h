#pragma once
#include "pla_ec.h"

#define PLA_TU_NAME_DELIM '.'

struct pla_tu_ref
{
    pla_ec_pos_t pos_start;
    pla_ec_pos_t pos_end;

    pla_cn_t * cn;
    bool is_rel;
};
struct pla_tu
{
    ul_hs_t * name;

    pla_tu_t * next;

    pla_dclr_t * root;

    size_t refs_size;
    pla_tu_ref_t * refs;
    size_t refs_cap;
};

PLA_API pla_tu_t * pla_tu_create(ul_hs_t * name);
PLA_API void pla_tu_destroy(pla_tu_t * tu);
PLA_API void pla_tu_destroy_chain(pla_tu_t * tu);

PLA_API pla_tu_ref_t * pla_tu_push_ref(pla_tu_t * tu);
