#include "ul_hs.h"

#define HASH_MULT 31

ul_hs_hash_t ul_hs_hash_ch(ul_hs_hash_t hash, char ch)
{
    return hash * HASH_MULT + (ul_hs_hash_t)ch;
}
ul_hs_hash_t ul_hs_hash_ntstr(ul_hs_hash_t hash, const char * ntstr)
{
    for (const char * ch = ntstr; *ch != 0; ++ch)
    {
        hash = ul_hs_hash_ch(hash, *ch);
    }

    return hash;
}
ul_hs_hash_t ul_hs_hash_str(ul_hs_hash_t hash, size_t str_size, const char * str)
{
    for (const char *ch = str, *ch_end = ch + str_size; ch != ch_end; ++ch)
    {
        hash = ul_hs_hash_ch(hash, *ch);
    }

    return hash;
}
