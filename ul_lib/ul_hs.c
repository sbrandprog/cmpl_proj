#include "ul_hs.h"

#define HASH_MULT 31

ul_hs_hash_t ul_hs_hash_ch(ul_hs_hash_t hash, wchar_t ch) {
	return hash * HASH_MULT + (ul_hs_hash_t)ch;
}
ul_hs_hash_t ul_hs_hash_ntstr(ul_hs_hash_t hash, const wchar_t * ntstr) {
	for (const wchar_t * ch = ntstr; *ch != 0; ++ch) {
		hash = ul_hs_hash_ch(hash, *ch);
	}

	return hash;
}
ul_hs_hash_t ul_hs_hash_str(ul_hs_hash_t hash, size_t str_size, const wchar_t * str) {
	for (const wchar_t * ch = str, *ch_end = ch + str_size; ch != ch_end; ++ch) {
		hash = ul_hs_hash_ch(hash, *ch);
	}

	return hash;
}
