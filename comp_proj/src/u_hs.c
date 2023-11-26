#include "pch.h"
#include "u_hs.h"

#define HASH_MULT 31

u_hs_hash_t u_hs_hash_ch(u_hs_hash_t hash, wchar_t ch) {
	return hash * HASH_MULT + (u_hs_hash_t)ch;
}
u_hs_hash_t u_hs_hash_ntstr(u_hs_hash_t hash, const wchar_t * ntstr) {
	for (const wchar_t * ch = ntstr; *ch != 0; ++ch) {
		hash = u_hs_hash_ch(hash, *ch);
	}

	return hash;
}
u_hs_hash_t u_hs_hash_str(u_hs_hash_t hash, size_t str_size, const wchar_t * str) {
	for (const wchar_t * ch = str, *ch_end = ch + str_size; ch != ch_end; ++ch) {
		hash = u_hs_hash_ch(hash, *ch);
	}

	return hash;
}
