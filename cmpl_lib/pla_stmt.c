#include "pla_stmt.h"
#include "pla_expr.h"

pla_stmt_t * pla_stmt_create(pla_stmt_type_t type)
{
    pla_stmt_t * stmt = malloc(sizeof(*stmt));

    ul_assert(stmt != NULL);

    *stmt = (pla_stmt_t){ .type = type };

    return stmt;
}
void pla_stmt_destroy(pla_stmt_t * stmt)
{
    if (stmt == NULL)
    {
        return;
    }

    switch (stmt->type)
    {
        case PlaStmtNone:
            break;
        case PlaStmtBlk:
            for (pla_stmt_t * it = stmt->blk.body; it != NULL;)
            {
                pla_stmt_t * next = it->next;

                pla_stmt_destroy(it);

                it = next;
            }
            break;
        case PlaStmtExpr:
            pla_expr_destroy(stmt->expr.expr);
            break;
        case PlaStmtVarDt:
            pla_expr_destroy(stmt->var_dt.dt_expr);
            break;
        case PlaStmtVarVal:
            pla_expr_destroy(stmt->var_val.val_expr);
            break;
        case PlaStmtCond:
            pla_expr_destroy(stmt->cond.cond_expr);
            pla_stmt_destroy(stmt->cond.true_br);
            pla_stmt_destroy(stmt->cond.false_br);
            break;
        case PlaStmtPreLoop:
            pla_expr_destroy(stmt->pre_loop.cond_expr);
            pla_stmt_destroy(stmt->pre_loop.body);
            break;
        case PlaStmtPostLoop:
            pla_expr_destroy(stmt->post_loop.cond_expr);
            pla_stmt_destroy(stmt->post_loop.body);
            break;
        case PlaStmtBrk:
        case PlaStmtCnt:
            break;
        case PlaStmtRet:
            pla_expr_destroy(stmt->ret.expr);
            break;
        default:
            ul_assert_unreachable();
    }

    free(stmt);
}

const pla_stmt_info_t pla_stmt_infos[PlaStmt_Count] = {
    [PlaStmtNone] = { .type_str = UL_ROS_MAKE("StmtNone") },
    [PlaStmtBlk] = { .type_str = UL_ROS_MAKE("StmtBlk") },
    [PlaStmtExpr] = { .type_str = UL_ROS_MAKE("StmtExpr") },
    [PlaStmtVarDt] = { .type_str = UL_ROS_MAKE("StmtVarDt") },
    [PlaStmtVarVal] = { .type_str = UL_ROS_MAKE("StmtVarVal") },
    [PlaStmtCond] = { .type_str = UL_ROS_MAKE("StmtCond") },
    [PlaStmtPreLoop] = { .type_str = UL_ROS_MAKE("StmtPreLoop") },
    [PlaStmtPostLoop] = { .type_str = UL_ROS_MAKE("StmtPostLoop") },
    [PlaStmtBrk] = { .type_str = UL_ROS_MAKE("StmtBrk") },
    [PlaStmtCnt] = { .type_str = UL_ROS_MAKE("StmtCnt") },
    [PlaStmtRet] = { .type_str = UL_ROS_MAKE("StmtRet") },
};
