#pragma once
#include "pla_ast_t.h"

bool pla_ast_t_s_translate(pla_ast_t_ctx_t * ctx, ira_dt_t * func_dt, pla_stmt_t * stmt, ira_func_t ** out);
