#pragma once
#include "ul_arr.h"
#include "ul_hs.h"

typedef struct ul_hst_node ul_hst_node_t;
struct ul_hst_node
{
    ul_hst_node_t * next;
    ul_hs_t hstr;
};

struct ul_hst
{
	size_t ents_size;
    ul_hst_node_t ** ents;

	size_t strs_count;
};

UL_API void ul_hst_init(ul_hst_t * hst);
UL_API void ul_hst_cleanup(ul_hst_t * hst);

UL_API ul_hs_t * ul_hst_add(ul_hst_t * hst, size_t str_size, const char * str, ul_hs_hash_t str_hash);
inline ul_hs_t * ul_hst_hashadd(ul_hst_t * hst, size_t str_size, const char * str)
{
    return ul_hst_add(hst, str_size, str, ul_hs_hash_str(str_size, str));
}

#define UL_HST_HASHADD_WS(hst, ws) ul_hst_hashadd(hst, ul_arr_count(ws) - 1, ws)
