#pragma once

typedef uint64_t u_hs_hash_t;

typedef struct u_hs {
	size_t size;
	wchar_t * str;
	u_hs_hash_t hash;
} u_hs_t;

u_hs_hash_t u_hs_hash_ch(u_hs_hash_t hash, wchar_t ch);
u_hs_hash_t u_hs_hash_ntstr(u_hs_hash_t hash, const wchar_t * ntstr);
u_hs_hash_t u_hs_hash_str(u_hs_hash_t hash, size_t str_size, const wchar_t * str);
