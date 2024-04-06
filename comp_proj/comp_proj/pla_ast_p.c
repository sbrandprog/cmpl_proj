#include "pch.h"
#include "pla_tu_p.h"
#include "pla_ast_p.h"

bool pla_ast_p_parse_file(pla_ast_t * ast, ul_hs_t * file_name) {
	pla_tu_t * tu = NULL;

	bool res = false;

	__try {
		res = pla_tu_p_parse_file(ast->hst, file_name, file_name, &tu);
	}
	__finally {
		if (res) {
			tu->next = ast->tu;
			ast->tu = tu;
		}
		else {
			pla_tu_destroy(tu);
		}
	}

	return res;
}
