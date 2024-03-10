#pragma once
#include "u_hs.h"

#define U_HST_ENTS_COUNT 263

typedef struct u_hst_node u_hst_node_t;
struct u_hst_node {
	u_hst_node_t * next;
	u_hs_t hstr;
};

typedef struct u_hst {
	u_hst_node_t * ents[U_HST_ENTS_COUNT];
} u_hst_t;


void u_hst_cleanup(u_hst_t * hst);

u_hs_t * u_hst_add(u_hst_t * hst, size_t str_size, const wchar_t * str, u_hs_hash_t str_hash);
inline u_hs_t * u_hst_hashadd(u_hst_t * hst, size_t str_size, const wchar_t * str) {
	return u_hst_add(hst, str_size, str, u_hs_hash_str(0, str_size, str));
}

#define u_hst_get_rostr(hst, str) u_hst_hashadd(hst, _countof(str) - 1, str)