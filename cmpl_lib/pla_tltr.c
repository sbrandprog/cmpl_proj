#include "pla_tltr.h"
#include "ira_func.h"
#include "ira_lo.h"
#include "ira_pec_ip.h"
#include "ira_val.h"
#include "pla_dclr.h"
#include "pla_ec.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "pla_tltr_s.h"
#include "pla_tu.h"

typedef pla_tltr_tse_t tse_t;
typedef pla_tltr_vse_t vse_t;


void pla_tltr_init(pla_tltr_t * tltr, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr, ira_pec_t * out)
{
    *tltr = (pla_tltr_t){ .hst = hst, .ec_fmtr = ec_fmtr, .out = out };

    for (pla_pds_t pds = 0; pds < PlaPds_Count; ++pds)
    {
        const ul_ros_t * pds_str = &pla_pds_strs[pds];

        tltr->pds[pds] = ul_hst_hashadd(tltr->hst, pds_str->size, pds_str->str);
    }

    ul_hsb_init(&tltr->hsb);
}
void pla_tltr_cleanup(pla_tltr_t * tltr)
{
    ul_hsb_cleanup(&tltr->hsb);

    memset(tltr, 0, sizeof(*tltr));
}


static void get_report_pos(pla_tltr_t * tltr, pla_ec_pos_t * pos_start_out, pla_ec_pos_t * pos_end_out)
{
    tse_t * tse = tltr->tse;

    if (tse == NULL)
    {
        *pos_start_out = (pla_ec_pos_t){ 0 };
        *pos_end_out = (pla_ec_pos_t){ 0 };

        return;
    }

    switch (tse->type)
    {
        case PlaTltrTseDclr:
            *pos_start_out = tse->dclr->pos_start;
            *pos_end_out = tse->dclr->pos_end;
            break;
        case PlaTltrTseStmt:
            *pos_start_out = tse->stmt->pos_start;
            *pos_end_out = tse->stmt->pos_end;
            break;
        case PlaTltrTseExpr:
            *pos_start_out = tse->expr->pos_start;
            *pos_end_out = tse->expr->pos_end;
            break;
        default:
            ul_assert_unreachable();
    }
}
void pla_tltr_report(pla_tltr_t * tltr, const char * fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    const char * src_name = tltr->src != NULL ? tltr->src->name->str : NULL;

    pla_ec_pos_t pos_start, pos_end;

    get_report_pos(tltr, &pos_start, &pos_end);

    pla_ec_formatpost_va(tltr->ec_fmtr, PLA_TLTR_MOD_NAME, src_name, pos_start, pos_end, fmt, args);

    va_end(args);
}
void pla_tltr_report_pec_err(pla_tltr_t * tltr)
{
    pla_tltr_report(tltr, "pec function error");
}


static bool calculate_expr_core(pla_tltr_t * tltr, pla_stmt_t * blk, ira_func_t ** func, ira_val_t ** out)
{
    if (!pla_tltr_s_translate(tltr, NULL, blk, func))
    {
        pla_tltr_report(tltr, "failed to translate an expression\n");
        return false;
    }

    if (!ira_pec_ip_interpret(tltr->out, *func, out))
    {
        pla_tltr_report(tltr, "failed to calculate an expression\n");
        return false;
    }

    return true;
}
bool pla_tltr_calculate_expr(pla_tltr_t * tltr, pla_expr_t * expr, ira_val_t ** out)
{
    tse_t tse = { .type = PlaTltrTseExpr, .expr = expr };

    pla_tltr_push_tse(tltr, &tse);

    pla_stmt_t ret = { .type = PlaStmtRet, .pos_start = expr->pos_start, .pos_end = expr->pos_end, .ret.expr = expr };
    pla_stmt_t blk = { .type = PlaStmtBlk, .pos_start = expr->pos_start, .pos_end = expr->pos_end, .blk.body = &ret };

    ira_func_t * func = NULL;

    bool res = calculate_expr_core(tltr, &blk, &func, out);

    ira_func_destroy(func);

    pla_tltr_pop_tse(tltr, &tse);

    return res;
}
bool pla_tltr_calculate_expr_dt(pla_tltr_t * tltr, pla_expr_t * expr, ira_dt_t ** out)
{
    bool result = false;
    ira_val_t * val = NULL;

    if (pla_tltr_calculate_expr(tltr, expr, &val))
    {
        switch (val->type)
        {
            case IraValImmDt:
                *out = val->dt_val;
                result = true;
                break;
            default:
            {
                tse_t tse = { .type = PlaTltrTseExpr, .expr = expr };

                pla_tltr_push_tse(tltr, &tse);

                pla_tltr_report(tltr, "expected an interpretable data type expression");

                pla_tltr_pop_tse(tltr, &tse);

                break;
            }
        }
    }

    ira_val_destroy(val);

    return result;
}


void pla_tltr_push_tse(pla_tltr_t * tltr, pla_tltr_tse_t * tse)
{
    tse->prev = tltr->tse;

    tltr->tse = tse;
}
void pla_tltr_pop_tse(pla_tltr_t * tltr, pla_tltr_tse_t * tse)
{
    ul_assert(tse == tltr->tse);

    tltr->tse = tse->prev;
}


static ul_hs_t * get_vse_raw_name(vse_t * vse)
{
    switch (vse->type)
    {
        case PlaTltrVseNspc:
            return vse->nspc.name;
        default:
            ul_assert_unreachable();
    }

    return NULL;
}
void pla_tltr_push_vse(pla_tltr_t * tltr, pla_tltr_vse_t * vse)
{
    vse->prev = tltr->vse;

    ul_hs_t * vse_name = get_vse_raw_name(vse);

    ul_hs_t * prefix = vse->prev != NULL ? get_vse_raw_name(vse->prev) : NULL;

    if (prefix != NULL)
    {
        vse_name = ul_hsb_cat_hs_delim(&tltr->hsb, tltr->hst, prefix, PLA_TU_NAME_DELIM, vse_name);
    }

    vse->name = vse_name;

    tltr->vse = vse;
}
void pla_tltr_pop_vse(pla_tltr_t * tltr, pla_tltr_vse_t * vse)
{
    ul_assert(vse == tltr->vse);

    switch (vse->type)
    {
        case PlaTltrVseNspc:
            break;
        default:
            ul_assert_unreachable();
    }

    tltr->vse = vse->prev;
}


static bool translate_dclr(pla_tltr_t * tltr, pla_dclr_t * dclr);

static ira_lo_t ** get_vse_lo_ins(pla_tltr_t * tltr, ul_hs_t * name)
{
    vse_t * vse = tltr->vse;

    if (vse == NULL)
    {
        ul_assert(name == NULL);

        return &tltr->out->root;
    }

    switch (vse->type)
    {
        case PlaTltrVseNspc:
        {
            ira_lo_nspc_node_t ** ins = &vse->nspc.lo->nspc.body;

            for (; *ins != NULL; ins = &(*ins)->next)
            {
                ira_lo_nspc_node_t * node = *ins;

                if (node->name == name)
                {
                    break;
                }
            }

            if (*ins == NULL)
            {
                *ins = ira_lo_create_nspc_node(name);
            }

            return &(*ins)->lo;
        }
        default:
            ul_assert_unreachable();
    }
}

static ul_hs_t * make_vse_lo_hint_name(pla_tltr_t * tltr, ul_hs_t * name)
{
    ul_hs_t * hint_name = name;

    vse_t * vse = tltr->vse;

    ul_hs_t * vse_name = vse != NULL ? vse->name : NULL;

    if (vse_name != NULL)
    {
        hint_name = ul_hsb_cat_hs_delim(&tltr->hsb, tltr->hst, vse_name, PLA_TU_NAME_DELIM, hint_name);
    }

    return hint_name;
}
static ira_lo_t * push_lo(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_type_t lo_type)
{
    ul_hs_t * hint_name = make_vse_lo_hint_name(tltr, dclr->name);

    return ira_pec_push_unq_lo(tltr->out, lo_type, hint_name);
}

static bool init_lo_stct_var(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** lo_out, ira_dt_stct_tag_t ** tag_out)
{
    *lo_out = push_lo(tltr, dclr, IraLoVar);

    (*lo_out)->var.qdt.dt = &tltr->out->dt_dt;
    (*lo_out)->var.qdt.qual = ira_dt_qual_const;

    ira_dt_stct_tag_t * tag;

    if (!ira_pec_get_dt_stct_tag(tltr->out, &tag))
    {
        pla_tltr_report_pec_err(tltr);
        return false;
    }

    ira_dt_t * stct;

    if (!ira_pec_get_dt_stct(tltr->out, tag, ira_dt_qual_none, &stct))
    {
        pla_tltr_report_pec_err(tltr);
        return false;
    }

    if (!ira_pec_make_val_imm_dt(tltr->out, stct, &(*lo_out)->var.val))
    {
        pla_tltr_report_pec_err(tltr);
        return false;
    }

    if (tag_out != NULL)
    {
        *tag_out = tag;
    }

    return true;
}

static bool translate_dclr_nspc_vse(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    for (pla_dclr_t * it = dclr->nspc.body; it != NULL; it = it->next)
    {
        if (!translate_dclr(tltr, it))
        {
            return false;
        }
    }

    return true;
}
static bool translate_dclr_nspc(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out == NULL)
    {
        *out = push_lo(tltr, dclr, IraLoNspc);
    }
    else if ((*out)->type != IraLoNspc)
    {
        pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
        return false;
    }

    vse_t vse = {
        .type = PlaTltrVseNspc,
        .nspc = { .lo = *out, .name = dclr->name }
    };

    pla_tltr_push_vse(tltr, &vse);

    bool res = translate_dclr_nspc_vse(tltr, dclr, out);

    pla_tltr_pop_vse(tltr, &vse);

    return res;
}
static bool translate_dclr_func(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out != NULL)
    {
        pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
        return false;
    }

    *out = push_lo(tltr, dclr, IraLoFunc);

    ira_dt_t * func_dt = NULL;

    if (!pla_tltr_calculate_expr_dt(tltr, dclr->func.dt_expr, &func_dt))
    {
        return false;
    }

    if (func_dt->type != IraDtFunc)
    {
        pla_tltr_report(tltr, "function language object requires function data type");
        return false;
    }

    if (!pla_tltr_s_translate(tltr, func_dt, dclr->func.block, &(*out)->func))
    {
        return false;
    }

    return true;
}
static bool translate_dclr_impt(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out != NULL)
    {
        pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
        return false;
    }

    *out = push_lo(tltr, dclr, IraLoImpt);

    if (!pla_tltr_calculate_expr_dt(tltr, dclr->impt.dt_expr, &(*out)->impt.dt))
    {
        return false;
    }

    (*out)->impt.lib_name = dclr->impt.lib_name;
    (*out)->impt.sym_name = dclr->impt.sym_name;

    return true;
}
static bool translate_dclr_var_dt(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out != NULL)
    {
        pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
        return false;
    }

    *out = push_lo(tltr, dclr, IraLoVar);

    if (!pla_tltr_calculate_expr_dt(tltr, dclr->var_dt.dt_expr, &(*out)->var.qdt.dt))
    {
        return false;
    }

    (*out)->var.qdt.qual = dclr->var_dt.dt_qual;

    if (!ira_pec_is_dt_complete((*out)->var.qdt.dt))
    {
        pla_tltr_report(tltr, "global variables must have only complete data types");
        return false;
    }

    if (!ira_pec_make_val_null(tltr->out, (*out)->var.qdt.dt, &(*out)->var.val))
    {
        pla_tltr_report_pec_err(tltr);
        return false;
    }

    return true;
}
static bool translate_dclr_var_val(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out != NULL)
    {
        pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
        return false;
    }

    *out = push_lo(tltr, dclr, IraLoVar);

    if (!pla_tltr_calculate_expr(tltr, dclr->var_val.val_expr, &(*out)->var.val))
    {
        return false;
    }

    (*out)->var.qdt.dt = (*out)->var.val->dt;
    (*out)->var.qdt.qual = dclr->var_val.dt_qual;

    return true;
}
static bool translate_dclr_stct(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    ira_dt_stct_tag_t * tag = NULL;

    if (*out != NULL)
    {
        if ((*out)->type != IraLoVar)
        {
            pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
            return false;
        }

        if ((*out)->var.qdt.dt != &tltr->out->dt_dt
            || (*out)->var.val == NULL
            || (*out)->var.val->type != IraValImmDt
            || (*out)->var.val->dt_val->type != IraDtStct)
        {
            pla_tltr_report(tltr, "invalid language object for struct [%s]", dclr->name->str);
            return false;
        }

        if ((*out)->var.val->dt_val->stct.tag->body != NULL)
        {
            pla_tltr_report(tltr, "struct [%s] already defined", dclr->name->str);
            return false;
        }

        tag = (*out)->var.val->dt_val->stct.tag;
    }
    else
    {
        if (!init_lo_stct_var(tltr, dclr, out, &tag))
        {
            return false;
        }
    }

    ira_dt_t * dt;

    if (!pla_tltr_calculate_expr_dt(tltr, dclr->dt_stct.dt_stct_expr, &dt))
    {
        return false;
    }

    if (!ira_pec_set_dt_stct_tag_body(tltr->out, tag, dt))
    {
        pla_tltr_report_pec_err(tltr);
        return false;
    }

    return true;
}
static bool translate_dclr_stct_decl(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out != NULL)
    {
        if ((*out)->type != IraLoVar)
        {
            pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
            return false;
        }
    }
    else
    {
        if (!init_lo_stct_var(tltr, dclr, out, NULL))
        {
            return false;
        }
    }

    return true;
}
static bool translate_dclr_enmn(pla_tltr_t * tltr, pla_dclr_t * dclr, ira_lo_t ** out)
{
    if (*out != NULL)
    {
        pla_tltr_report(tltr, "language object with [%s] name already exists", dclr->name->str);
        return false;
    }

    *out = push_lo(tltr, dclr, IraLoVar);

    (*out)->var.qdt.dt = &tltr->out->dt_dt;
    (*out)->var.qdt.qual = ira_dt_qual_const;

    ira_dt_t * enmn_body_dt;

    if (!pla_tltr_calculate_expr_dt(tltr, dclr->enmn.dt_expr, &enmn_body_dt))
    {
        return false;
    }

    ira_dt_t * enmn_dt;

    {
        ira_dt_enmn_tag_t * tag;

        if (!ira_pec_get_dt_enmn_tag(tltr->out, enmn_body_dt, &tag))
        {
            pla_tltr_report_pec_err(tltr);
            return false;
        }

        if (!ira_pec_get_dt_enmn(tltr->out, tag, &enmn_dt))
        {
            pla_tltr_report_pec_err(tltr);
            return false;
        }

        if (!ira_pec_make_val_imm_dt(tltr->out, enmn_dt, &(*out)->var.val))
        {
            pla_tltr_report_pec_err(tltr);
            return false;
        }
    }

    for (pla_dclr_t * elem = dclr->enmn.elem; elem != NULL; elem = elem->next)
    {
        if (elem->type != PlaDclrEnmnElem)
        {
            pla_tltr_report(tltr, "invalid enumeration element [%s] in [%s] enumeration", elem->name->str, dclr->name->str);
            return false;
        }

        ira_lo_t ** elem_ins = get_vse_lo_ins(tltr, elem->name);

        if (*elem_ins != NULL)
        {
            pla_tltr_report(tltr, "language object with [%s] name already exists", elem->name->str);
            return false;
        }

        *elem_ins = push_lo(tltr, elem, IraLoVar);

        ira_lo_t * elem_lo = *elem_ins;

        elem_lo->var.qdt.dt = enmn_dt;
        elem_lo->var.qdt.qual = ira_dt_qual_const;
        elem_lo->var.val = ira_val_create(IraValImmEnmn, enmn_dt);

        if (!pla_tltr_calculate_expr(tltr, elem->enmn_elem.val, &elem_lo->var.val->val_val))
        {
            pla_tltr_report(tltr, "invalid enumeration element [%s] value", elem->name->str);
            return false;
        }

        if (elem_lo->var.val->val_val->dt != enmn_body_dt)
        {
            pla_tltr_report(tltr, "enumeration element's [%s] data type does not match data type of enumeration [%s]", elem->name->str, dclr->name->str);
            return false;
        }
    }

    return true;
}
static bool translate_dclr_tse(pla_tltr_t * tltr, pla_dclr_t * dclr)
{
    ira_lo_t ** ins = get_vse_lo_ins(tltr, dclr->name);

    switch (dclr->type)
    {
        case PlaDclrNone:
            break;
        case PlaDclrNspc:
            if (!translate_dclr_nspc(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrFunc:
            if (!translate_dclr_func(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrImpt:
            if (!translate_dclr_impt(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrVarDt:
            if (!translate_dclr_var_dt(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrVarVal:
            if (!translate_dclr_var_val(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrDtStct:
            if (!translate_dclr_stct(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrDtStctDecl:
            if (!translate_dclr_stct_decl(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrEnmn:
            if (!translate_dclr_enmn(tltr, dclr, ins))
            {
                return false;
            }
            break;
        case PlaDclrEnmnElem:
            pla_tltr_report(tltr, "unexpected enumeration element in declarator scope");
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool translate_dclr(pla_tltr_t * tltr, pla_dclr_t * dclr)
{
    tse_t tse = { .type = PlaTltrTseDclr, .dclr = dclr };

    pla_tltr_push_tse(tltr, &tse);

    bool res = translate_dclr_tse(tltr, dclr);

    pla_tltr_pop_tse(tltr, &tse);

    return res;
}


static bool translate_core(pla_tltr_t * tltr)
{
    pla_tu_t * tu = tltr->src->tu;

    if (tu == NULL || tu->root->type != PlaDclrNspc || tu->root->name != NULL)
    {
        return false;
    }

    if (!translate_dclr(tltr, tu->root))
    {
        return false;
    }

    return true;
}
bool pla_tltr_translate(pla_tltr_t * tltr, const pla_tltr_src_t * src)
{
    if (tltr->tse != NULL || tltr->vse != NULL)
    {
        return false;
    }

    tltr->src = src;

    bool res = translate_core(tltr);

    tltr->src = NULL;

    ul_assert(tltr->tse == NULL && tltr->vse == NULL);

    return res;
}
