#pragma once
#include "ul.h"

struct ul_hs
{
    size_t size;
    char * str;
    ul_hs_hash_t hash;
};

UL_API ul_hs_hash_t ul_hs_hash_ch(ul_hs_hash_t hash, char ch);
UL_API ul_hs_hash_t ul_hs_hash_ntstr(ul_hs_hash_t hash, const char * ntstr);
UL_API ul_hs_hash_t ul_hs_hash_str(ul_hs_hash_t hash, size_t str_size, const char * str);
