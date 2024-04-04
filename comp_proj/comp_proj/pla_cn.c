#include "pch.h"
#include "pla_cn.h"

pla_cn_t * pla_cn_create(ul_hs_t * name) {
	pla_cn_t * cn = malloc(sizeof(*cn));

	ul_raise_check_mem_alloc(cn);

	*cn = (pla_cn_t){ .name = name };

	return cn;
}
void pla_cn_destroy(pla_cn_t * cn) {
	if (cn == NULL) {
		return;
	}

	pla_cn_destroy(cn->sub_name);

	free(cn);
}
