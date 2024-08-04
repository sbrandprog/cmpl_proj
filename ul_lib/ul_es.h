#pragma once
#include "ul.h"

struct ul_es_ctx {
	SRWLOCK lock;
};

struct ul_es_node {
	ul_es_ctx_t * ctx;
	void * user_data;

	size_t nodes_size;
	ul_es_node_t ** nodes;
	size_t nodes_cap;
};

UL_SYMBOL void ul_es_init_ctx(ul_es_ctx_t * ctx);
UL_SYMBOL void ul_es_cleanup_ctx(ul_es_ctx_t * ctx);


UL_SYMBOL ul_es_node_t * ul_es_create_node(ul_es_ctx_t * ctx, void * user_data);
UL_SYMBOL void ul_es_destroy_node(ul_es_node_t * node);


UL_SYMBOL void ul_es_link_nl(ul_es_node_t * first, ul_es_node_t * second);
inline void ul_es_link(ul_es_node_t * first, ul_es_node_t * second) {
	ul_assert(first->ctx == second->ctx);

	AcquireSRWLockExclusive(&first->ctx->lock);

	ul_es_link_nl(first, second);
	
	ReleaseSRWLockExclusive(&first->ctx->lock);
}
UL_SYMBOL void ul_es_unlink_nl(ul_es_node_t * first, ul_es_node_t * second);
inline void ul_es_unlink(ul_es_node_t * first, ul_es_node_t * second) {
	ul_assert(first->ctx == second->ctx);

	AcquireSRWLockExclusive(&first->ctx->lock);

	ul_es_unlink_nl(first, second);
	
	ReleaseSRWLockExclusive(&first->ctx->lock);
}
