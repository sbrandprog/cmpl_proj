#pragma once
#include "ul.h"

struct ul_hs
{
    size_t size;
    wchar_t * str;
    ul_hs_hash_t hash;
};

UL_API ul_hs_hash_t ul_hs_hash_ch(ul_hs_hash_t hash, wchar_t ch);
UL_API ul_hs_hash_t ul_hs_hash_ntstr(ul_hs_hash_t hash, const wchar_t * ntstr);
UL_API ul_hs_hash_t ul_hs_hash_str(ul_hs_hash_t hash, size_t str_size, const wchar_t * str);
