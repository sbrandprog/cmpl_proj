#include "pch.h"
#include "ul_raise.h"
#include "ul_hst.h"

void ul_hst_init(ul_hst_t * hst) {
	memset(hst, 0, sizeof(*hst));

	InitializeCriticalSection(&hst->lock);
}
static void cleanup_nodes_nl(ul_hst_t * hst) {
	for (ul_hst_node_t ** ent = hst->ents, **ent_end = ent + _countof(hst->ents); ent != ent_end; ++ent) {
		for (ul_hst_node_t * node = *ent; node != NULL; ) {
			ul_hst_node_t * temp = node->next;

			free(node->hstr.str);
			free(node);

			node = temp;
		}

		*ent = NULL;
	}
}
void ul_hst_cleanup(ul_hst_t * hst) {
	EnterCriticalSection(&hst->lock);

	__try {
		cleanup_nodes_nl(hst);
	}
	__finally {
		LeaveCriticalSection(&hst->lock);
	}

	DeleteCriticalSection(&hst->lock);

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

	ul_raise_check_mem_alloc(new_node_str);

	wmemcpy(new_node_str, str, str_size);
	new_node_str[str_size] = 0;

	ul_hst_node_t * new_node = malloc(sizeof(*new_node));

	if (new_node == NULL) {
		free(new_node_str);
		ul_raise_no_mem();
	}

	*new_node = (ul_hst_node_t){ .hstr = { .size = str_size, .str = new_node_str, .hash = str_hash } };

	*node_place = new_node;

	return &new_node->hstr;
}
ul_hs_t * ul_hst_add(ul_hst_t * hst, size_t str_size, const wchar_t * str, ul_hs_hash_t str_hash) {
	ul_hs_t * res = NULL;

	EnterCriticalSection(&hst->lock);

	__try {
		res = ul_hst_add_nolock(hst, str_size, str, str_hash);
	}
	__finally {
		LeaveCriticalSection(&hst->lock);
	}

	return res;
}
