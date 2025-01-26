#pragma once
#include "pla.h"

struct pla_tus
{
    ul_hs_t * name;

    pla_tus_t * next;

    size_t src_size;
    char * src;
    size_t src_cap;
};

PLA_API pla_tus_t * pla_tus_create(ul_hs_t * name);
PLA_API void pla_tus_destroy(pla_tus_t * tus);
PLA_API void pla_tus_destroy_chain(pla_tus_t * tus);

PLA_API void pla_tus_insert_str(pla_tus_t * tus, size_t ins_pos, size_t str_size, char * str);

PLA_API bool pla_tus_read_file(pla_tus_t * tus, const char * file_name);
