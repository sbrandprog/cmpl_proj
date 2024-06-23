#include "pch.h"
#include "ul_assert.h"
#include "ul_hst.h"

void ul_hst_init(ul_hst_t * hst) {
	memset(hst, 0, sizeof(*hst));

	InitializeCriticalSection(&hst->lock);
}
void ul_hst_cleanup(ul_hst_t * hst) {
	DeleteCriticalSection(&hst->lock);

	for (ul_hst_node_t ** ent = hst->ents, **ent_end = ent + _countof(hst->ents); ent != ent_end; ++ent) {
		for (ul_hst_node_t * node = *ent; node != NULL; ) {
			ul_hst_node_t * temp = node->next;

			free(node->hstr.str);
			free(node);

			node = temp;
		}

		*ent = NULL;
	}

	memset(hst, 0, sizeof(*hst));
}

static ul_hs_t * ul_hst_add_nolock(ul_hst_t * hst, size_t str_size, const wchar_t * str, ul_hs_hash_t str_hash) {
	ul_hst_node_t ** node_place = &hst->ents[str_hash % _countof(hst->ents)];

	while (*node_place != NULL) {
		ul_hst_node_t * node = *node_place;

		if (node->hstr.hash == str_hash
			&& node->hstr.size == str_size
			&& wcsncmp(node->hstr.str, str, str_size) == 0) {
			return &node->hstr;
		}

		node_place = &node->next;
	}

	wchar_t * new_node_str = malloc((str_size + 1) * sizeof(*new_node_str));

	ul_assert(new_node_str != NULL);

	wmemcpy(new_node_str, str, str_size);
	new_node_str[str_size] = 0;

	ul_hst_node_t * new_node = malloc(sizeof(*new_node));

	ul_assert(new_node != NULL);

	*new_node = (ul_hst_node_t){ .hstr = { .size = str_size, .str = new_node_str, .hash = str_hash } };

	*node_place = new_node;

	return &new_node->hstr;
}
ul_hs_t * ul_hst_add(ul_hst_t * hst, size_t str_size, const wchar_t * str, ul_hs_hash_t str_hash) {
	EnterCriticalSection(&hst->lock);

	ul_hs_t * res = ul_hst_add_nolock(hst, str_size, str, str_hash);

	LeaveCriticalSection(&hst->lock);

	return res;
}
