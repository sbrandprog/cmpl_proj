#include "pch.h"
#include "ul_arr.h"
#include "ul_es.h"

void ul_es_init_ctx(ul_es_ctx_t * ctx) {
	*ctx = (ul_es_ctx_t){ 0 };

	InitializeSRWLock(&ctx->lock);
}
void ul_es_cleanup_ctx(ul_es_ctx_t * ctx) {
	memset(ctx, 0, sizeof(*ctx));
}


ul_es_node_t * ul_es_create_node(ul_es_ctx_t * ctx, void * user_data) {
	ul_es_node_t * node = malloc(sizeof(*node));

	ul_assert(node != NULL);

	*node = (ul_es_node_t){ .ctx = ctx, .user_data = user_data };

	return node;
}
static void destroy_node_nl(ul_es_node_t * node) {
	while (node->nodes_size > 0) {
		ul_es_unlink_nl(node, node->nodes[0]);
	}

	free(node->nodes);

	free(node);
}
void ul_es_destroy_node(ul_es_node_t * node) {
	if (node == NULL) {
		return;
	}

	ul_es_ctx_t * ctx = node->ctx;
	
	AcquireSRWLockExclusive(&ctx->lock);

	__try {
		destroy_node_nl(node);
	}
	__finally {
		ReleaseSRWLockExclusive(&ctx->lock);
	}
}


static void attach_node_nl(ul_es_node_t * first, ul_es_node_t * second) {
	for (ul_es_node_t ** it = first->nodes, **it_end = it + first->nodes_size; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		if (node == second) {
			return;
		}
	}

	if (first->nodes_size + 1 > first->nodes_cap) {
		ul_arr_grow(&first->nodes_cap, (void **)&first->nodes, sizeof(*first->nodes), 1);
	}

	first->nodes[first->nodes_size++] = second;
}
void ul_es_link_nl(ul_es_node_t * first, ul_es_node_t * second) {
	ul_assert(first->ctx == second->ctx);

	attach_node_nl(first, second);
	attach_node_nl(second, first);
}
static void detach_node_nl(ul_es_node_t * first, ul_es_node_t * second) {
	ul_es_node_t ** it = first->nodes, ** it_end = it + first->nodes_size;

	for (; it != it_end; ++it) {
		ul_es_node_t * node = *it;

		if (node == second) {
			break;
		}
	}

	if (it != it_end) {
		memmove_s(it, (first->nodes + first->nodes_cap - it) * sizeof(*first->nodes), it + 1, (it_end - it - 1) * sizeof(first->nodes));
	}

	--first->nodes_size;
}
void ul_es_unlink_nl(ul_es_node_t * first, ul_es_node_t * second) {
	ul_assert(first->ctx == second->ctx);

	detach_node_nl(first, second);
	detach_node_nl(second, first);
}
