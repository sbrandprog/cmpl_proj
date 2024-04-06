#include "pch.h"
#include "pla_ast.h"
#include "pla_tu.h"

void pla_ast_init(pla_ast_t * ast, ul_hst_t * hst) {
	*ast = (pla_ast_t){ .hst = hst };

	for (pla_pds_t pds = 0; pds < PlaPds_Count; ++pds) {
		const ul_ros_t * pds_str = &pla_pds_strs[pds];

		ast->pds[pds] = ul_hst_hashadd(ast->hst, pds_str->size, pds_str->str);
	}
}
void pla_ast_cleanup(pla_ast_t * ast) {
	pla_tu_destroy_chain(ast->tu);

	memset(ast, 0, sizeof(*ast));
}
