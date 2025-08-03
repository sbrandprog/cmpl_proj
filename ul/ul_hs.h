#pragma once
#include "ul.h"

struct ul_hs
{
    size_t size;
    char * str;
    ul_hs_hash_t hash;
};

UL_API ul_hs_hash_t ul_hs_hash_ntstr(const char * ntstr);
UL_API ul_hs_hash_t ul_hs_hash_str(size_t str_size, const char * str);
