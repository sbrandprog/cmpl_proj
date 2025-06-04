#pragma once
#include "ul_arr.h"
#include "ul_hs.h"

struct ul_hst_node
{
    ul_hst_node_t * next;
    size_t size;
    char * str;
    ul_hs_hash_t hash;
};

struct ul_hst_node_ref
{
    ul_hst_node_t * node;
};

struct ul_hst
{
    size_t ents_size;
    ul_hst_node_t ** ents;

    size_t strs_count;
};

UL_API void ul_hst_init(ul_hst_t * hst);
UL_API void ul_hst_cleanup(ul_hst_t * hst);

UL_API ul_hst_node_ref_t ul_hst_add(ul_hst_t * hst, size_t str_size, const char * str, ul_hs_hash_t str_hash);
inline ul_hst_node_ref_t ul_hst_hashadd(ul_hst_t * hst, size_t str_size, const char * str)
{
    return ul_hst_add(hst, str_size, str, ul_hs_hash_str(str_size, str));
}

UL_API void ul_hst_copy_ref(ul_hst_node_ref_t * dst, const ul_hst_node_ref_t * src);
UL_API void ul_hst_free_ref(ul_hst_node_ref_t * ref);

#define UL_HST_NULL_REF ((ul_hst_node_ref_t){ .node = NULL })

#define UL_HST_HASHADD_WS(hst, ws) ul_hst_hashadd(hst, ul_arr_count(ws) - 1, ws)
