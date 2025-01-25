#include "pla_dclr.h"
#include "pla_expr.h"
#include "pla_stmt.h"

pla_dclr_t * pla_dclr_create(pla_dclr_type_t type)
{
    pla_dclr_t * dclr = malloc(sizeof(*dclr));

    ul_assert(dclr != NULL);

    *dclr = (pla_dclr_t){ .type = type };

    return dclr;
}
void pla_dclr_destroy(pla_dclr_t * dclr)
{
    if (dclr == NULL)
    {
        return;
    }

    switch (dclr->type)
    {
        case PlaDclrNone:
            break;
        case PlaDclrNspc:
            pla_dclr_destroy_chain(dclr->nspc.body);
            break;
        case PlaDclrFunc:
            pla_expr_destroy(dclr->func.dt_expr);
            pla_stmt_destroy(dclr->func.block);
            break;
        case PlaDclrImpt:
            pla_expr_destroy(dclr->impt.dt_expr);
            break;
        case PlaDclrVarDt:
            pla_expr_destroy(dclr->var_dt.dt_expr);
            break;
        case PlaDclrVarVal:
            pla_expr_destroy(dclr->var_val.val_expr);
            break;
        case PlaDclrDtStct:
            pla_expr_destroy(dclr->dt_stct.dt_stct_expr);
            break;
        case PlaDclrDtStctDecl:
            break;
        case PlaDclrEnmn:
            pla_expr_destroy(dclr->enmn.dt_expr);
            pla_dclr_destroy_chain(dclr->enmn.elem);
            break;
        case PlaDclrEnmnElem:
            pla_expr_destroy(dclr->enmn_elem.val);
            break;
        default:
            ul_assert_unreachable();
    }

    free(dclr);
}
void pla_dclr_destroy_chain(pla_dclr_t * dclr)
{
    while (dclr != NULL)
    {
        pla_dclr_t * next = dclr->next;

        pla_dclr_destroy(dclr);

        dclr = next;
    }
}

const pla_dclr_info_t pla_dclr_infos[PlaDclr_Count] = {
    [PlaDclrNone] = { .type_str = UL_ROS_MAKE(L"DclrNone") },
    [PlaDclrNspc] = { .type_str = UL_ROS_MAKE(L"DclrNspc") },
    [PlaDclrFunc] = { .type_str = UL_ROS_MAKE(L"DclrFunc") },
    [PlaDclrImpt] = { .type_str = UL_ROS_MAKE(L"DclrImpt") },
    [PlaDclrVarDt] = { .type_str = UL_ROS_MAKE(L"DclrVarDt") },
    [PlaDclrVarVal] = { .type_str = UL_ROS_MAKE(L"DclrVarVal") },
    [PlaDclrDtStct] = { .type_str = UL_ROS_MAKE(L"DclrDtStct") },
    [PlaDclrDtStctDecl] = { .type_str = UL_ROS_MAKE(L"DclrDtStctDecl") },
    [PlaDclrEnmn] = { .type_str = UL_ROS_MAKE(L"DclrEnmn") },
    [PlaDclrEnmnElem] = { .type_str = UL_ROS_MAKE(L"DclrEnmnElem") }
};
