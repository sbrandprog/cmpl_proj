#include "ira_val.h"
#include "ira_lo.h"
#include "ira_func.h"

ira_lo_nspc_node_t * ira_lo_create_nspc_node(ul_hs_t * name) {
	ira_lo_nspc_node_t * node = malloc(sizeof(*node));

	ul_assert(node != NULL);

	*node = (ira_lo_nspc_node_t){ .name = name };

	return node;
}
void ira_lo_destroy_nspc_node(ira_lo_nspc_node_t * node) {
	if (node == NULL) {
		return;
	}

	free(node);
}
void ira_lo_destroy_nspc_node_chain(ira_lo_nspc_node_t * node) {
	while (node != NULL) {
		ira_lo_nspc_node_t * next = node->next;

		ira_lo_destroy_nspc_node(node);

		node = next;
	}
}
