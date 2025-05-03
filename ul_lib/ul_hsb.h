#pragma once
#include "ul_hst.h"

struct ul_hsb
{
    char * buf;
    size_t cap;
};

UL_API void ul_hsb_init(ul_hsb_t * hsb);
UL_API void ul_hsb_cleanup(ul_hsb_t * hsb);

UL_API size_t ul_hsb_format_va(ul_hsb_t * hsb, const char * fmt, va_list args);
UL_API size_t ul_hsb_format(ul_hsb_t * hsb, const char * fmt, ...);

UL_API ul_hst_node_ref_t ul_hsb_formatadd_va(ul_hsb_t * hsb, ul_hst_t * hst, const char * fmt, va_list args);
inline ul_hst_node_ref_t ul_hsb_formatadd(ul_hsb_t * hsb, ul_hst_t * hst, const char * fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    ul_hst_node_ref_t res = ul_hsb_formatadd_va(hsb, hst, fmt, args);

    va_end(args);

    return res;
}

inline ul_hst_node_ref_t ul_hsb_cat_hs_delim(ul_hsb_t * hsb, ul_hst_t * hst, ul_hst_node_ref_t * first, char delim, ul_hst_node_ref_t * second)
{
    return ul_hsb_formatadd(hsb, hst, "%s%c%s", first->node->str, delim, second->node->str);
}
