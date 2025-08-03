#include "ul_hs.h"

#define FNV_64_OFFSET ((ul_hs_hash_t)14695981039346656037ull)
#define FNV_64_PRIME ((ul_hs_hash_t)1099511628211ull)

static ul_hs_hash_t do_fnv1a_init() {
	return FNV_64_OFFSET;
}
static ul_hs_hash_t do_fnv1a_step(ul_hs_hash_t hash, char ch)
{
	hash ^= (uint8_t)ch;
	hash *= FNV_64_PRIME;

	return hash;
}

ul_hs_hash_t ul_hs_hash_ntstr(const char * ntstr)
{
    ul_hs_hash_t hash = do_fnv1a_init();

    for (const char * ch = ntstr; *ch != 0; ++ch)
    {
        hash = do_fnv1a_step(hash, *ch);
    }

    return hash;
}
ul_hs_hash_t ul_hs_hash_str(size_t str_size, const char * str)
{
    ul_hs_hash_t hash = do_fnv1a_init();

    for (const char *ch = str, *ch_end = ch + str_size; ch != ch_end; ++ch)
    {
        hash = do_fnv1a_step(hash, *ch);
    }

    return hash;
}
