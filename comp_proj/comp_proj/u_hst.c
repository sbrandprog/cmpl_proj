#include "pch.h"
#include "u_hst.h"
#include "u_assert.h"

void u_hst_cleanup(u_hst_t * hst) {
	for (u_hst_node_t ** ent = hst->ents, **ent_end = ent + _countof(hst->ents); ent != ent_end; ++ent) {
		for (u_hst_node_t * node = *ent; node != NULL; ) {
			u_hst_node_t * temp = node->next;

			free(node->hstr.str);
			free(node);

			node = temp;
		}
	}
}

u_hs_t * u_hst_add(u_hst_t * hst, size_t str_size, const wchar_t * str, u_hs_hash_t str_hash) {
	u_hst_node_t ** node_place = &hst->ents[str_hash % _countof(hst->ents)];

	while (*node_place != NULL) {
		u_hst_node_t * node = *node_place;

		if (node->hstr.hash == str_hash
			&& node->hstr.size == str_size
			&& wcsncmp(node->hstr.str, str, str_size) == 0) {
			return &node->hstr;
		}

		node_place = &node->next;
	}

	wchar_t * new_node_str = malloc((str_size + 1) * sizeof(*new_node_str));

	u_assert(new_node_str != NULL);

	wmemcpy(new_node_str, str, str_size);
	new_node_str[str_size] = 0;

	u_hst_node_t * new_node = malloc(sizeof(*new_node));

	u_assert(new_node != NULL);

	*new_node = (u_hst_node_t){ .hstr = { .size = str_size, .str = new_node_str, .hash = str_hash } };

	*node_place = new_node;

	return &new_node->hstr;
}