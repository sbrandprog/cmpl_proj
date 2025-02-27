#include "ira_pec_c.h"
#include "ira_lo.h"
#include "ira_pec_ip.h"
#include "ira_val.h"
#include "mc_frag.h"
#include "mc_inst.h"
#include "mc_pea.h"

#define MOD_NAME "ira_pec_c"

#define LO_NAME_DELIM '.'

#define VAL_FRAG_UNQ_SUFFIX "#val_frag"

typedef struct ira_pec_c_ctx_cl_elem
{
    ira_lo_t * lo;
} cl_elem_t;

typedef struct ira_pec_c_ctx
{
    ira_pec_t * pec;
    mc_pea_t * out;

    ul_hsb_t hsb;
    ul_hst_t * hst;

    size_t val_frag_index;

    size_t cl_elems_size;
    cl_elem_t * cl_elems;
    size_t cl_elems_cap;
    size_t cl_elems_cur;

    mc_frag_t * cl_frag;
    mc_frag_t ** cl_frag_ins;
} ctx_t;

static void report(ctx_t * ctx, const char * fmt, ...)
{
    if (ctx->pec->ec_fmtr == NULL)
    {
        return;
    }

    {
        va_list args;

        va_start(args, fmt);

        ul_ec_fmtr_format_va(ctx->pec->ec_fmtr, fmt, args);

        va_end(args);
    }

    ul_ec_fmtr_post(ctx->pec->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
}

mc_frag_t * ira_pec_c_get_frag(ira_pec_c_ctx_t * ctx, mc_frag_type_t frag_type, ul_hs_t * frag_label)
{
    *ctx->cl_frag_ins = mc_frag_create(frag_type);

    mc_frag_t * frag = *ctx->cl_frag_ins;

    ctx->cl_frag_ins = &frag->next;

    if (frag_label != NULL)
    {
        mc_inst_t label = { .type = McInstLabel, .opds = McInstOpds_Label, .label_stype = LnkSectLpLabelBasic, .label = frag_label };

        mc_frag_push_inst(frag, &label);
    }

    return frag;
}

static ul_hs_t * get_val_frag_label(ctx_t * ctx, ul_hs_t * hint_name)
{
    return ul_hsb_formatadd(&ctx->hsb, ctx->hst, "%s%s%zi", hint_name->str, VAL_FRAG_UNQ_SUFFIX, ctx->val_frag_index++);
}

static bool is_lo_compilable(ira_lo_t * lo)
{
    switch (lo->type)
    {
        case IraLoNone:
        case IraLoNspc:
            return false;
        case IraLoFunc:
        case IraLoImpt:
        case IraLoVar:
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
bool ira_pec_c_process_val_compl(ira_pec_c_ctx_t * ctx, ira_val_t * val)
{
    switch (val->type)
    {
        case IraValImmVoid:
            break;
        case IraValImmDt:
            return false;
        case IraValImmBool:
        case IraValImmInt:
        case IraValImmPtr:
            break;
        case IraValLoPtr:
            if (!is_lo_compilable(val->lo_val))
            {
                report(ctx, "reference to non-compilable language object [%s]", val->lo_val->name->str);
                return false;
            }

            ira_pec_c_push_cl_elem(ctx, val->lo_val);
            break;
        case IraValImmTpl:
            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size; elem != elem_end; ++elem)
            {
                if (!ira_pec_c_process_val_compl(ctx, *elem))
                {
                    return false;
                }
            }
            break;
        case IraValImmStct:
            if (!ira_pec_c_process_val_compl(ctx, val->val_val))
            {
                return false;
            }
            break;
        case IraValImmArr:
            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size; elem != elem_end; ++elem)
            {
                if (!ira_pec_c_process_val_compl(ctx, *elem))
                {
                    return false;
                }
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool compile_val(ctx_t * ctx, mc_frag_t * frag, ul_hs_t * hint_name, ira_val_t * val)
{
    {
        size_t dt_align;

        if (!ira_pec_get_dt_align(val->dt, &dt_align))
        {
            return false;
        }

        mc_inst_t align = { .type = McInstAlign, .opds = McInstOpds_Imm, .imm0_type = McInstImm64, .imm0 = (int64_t)dt_align };

        mc_frag_push_inst(frag, &align);
    }

    mc_inst_t data = { .type = McInstData, .opds = McInstOpds_Imm };

    switch (val->type)
    {
        case IraValImmVoid:
            break;
        case IraValImmDt:
            return false;
        case IraValImmBool:
            data.imm0_type = McInstImm8;
            data.imm0 = val->bool_val ? 1 : 0;

            mc_frag_push_inst(frag, &data);
            break;
        case IraValImmInt:
            data.imm0_type = ira_int_infos[val->dt->int_type].mc_imm_type;
            data.imm0 = val->int_val.si64;

            mc_frag_push_inst(frag, &data);
            break;
        case IraValImmVec:
            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size; elem != elem_end; ++elem)
            {
                if (!compile_val(ctx, frag, hint_name, *elem))
                {
                    return false;
                }
            }
            break;
        case IraValImmPtr:
            data.imm0_type = McInstImm64;
            data.imm0 = (int64_t)val->int_val.ui64;

            mc_frag_push_inst(frag, &data);
            break;
        case IraValLoPtr:
            if (!is_lo_compilable(val->lo_val))
            {
                report(ctx, "reference to non-compilable language object [%s]", val->lo_val->name->str);
                return false;
            }

            data.imm0_type = McInstImmLabelVa64;
            data.imm0_label = val->lo_val->name;

            mc_frag_push_inst(frag, &data);
            break;
        case IraValImmTpl:
            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size; elem != elem_end; ++elem)
            {
                if (!compile_val(ctx, frag, hint_name, *elem))
                {
                    return false;
                }
            }
            break;
        case IraValImmStct:
            if (!compile_val(ctx, frag, hint_name, val->val_val))
            {
                return false;
            }
            break;
        case IraValImmArr:
        {
            ul_hs_t * arr_label = get_val_frag_label(ctx, hint_name);

            mc_frag_t * arr_frag = ira_pec_c_get_frag(ctx, frag->type, arr_label);

            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size; elem != elem_end; ++elem)
            {
                if (!compile_val(ctx, arr_frag, hint_name, *elem))
                {
                    return false;
                }
            }

            {
                mc_inst_t data = { .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImm64, .imm0 = (int64_t)val->arr_val.size };

                mc_frag_push_inst(frag, &data);
            }

            {
                mc_inst_t data = { .type = McInstData, .type = McInstData, .opds = McInstOpds_Imm, .imm0_type = McInstImmLabelVa64, .imm0_label = arr_label };

                mc_frag_push_inst(frag, &data);
            }

            break;
        }
        case IraValImmEnmn:
            if (!compile_val(ctx, frag, hint_name, val->val_val))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }


    return true;
}
bool ira_pec_c_compile_val_frag(ira_pec_c_ctx_t * ctx, ira_val_t * val, ul_hs_t * hint_name, ul_hs_t ** out_label)
{
    ul_hs_t * frag_label = get_val_frag_label(ctx, hint_name);

    mc_frag_t * frag = ira_pec_c_get_frag(ctx, McFragRoData, frag_label);

    if (!compile_val(ctx, frag, hint_name, val))
    {
        return false;
    }

    *out_label = frag_label;

    return true;
}

ul_hsb_t * ira_pec_c_get_hsb(ira_pec_c_ctx_t * ctx)
{
    return &ctx->hsb;
}
ul_hst_t * ira_pec_c_get_hst(ira_pec_c_ctx_t * ctx)
{
    return ctx->hst;
}
ira_pec_t * ira_pec_c_get_pec(ira_pec_c_ctx_t * ctx)
{
    return ctx->pec;
}

void ira_pec_c_push_cl_elem(ira_pec_c_ctx_t * ctx, ira_lo_t * lo)
{
    for (cl_elem_t *elem = ctx->cl_elems, *elem_end = elem + ctx->cl_elems_size; elem != elem_end; ++elem)
    {
        if (elem->lo == lo)
        {
            return;
        }
    }

    ul_arr_grow(ctx->cl_elems_size + 1, &ctx->cl_elems_cap, (void **)&ctx->cl_elems, sizeof(*ctx->cl_elems));

    ctx->cl_elems[ctx->cl_elems_size++] = (cl_elem_t){ .lo = lo };
}

static bool compile_lo_impt(ctx_t * ctx, ira_lo_t * lo)
{
    mc_pea_it_add_sym(&ctx->out->it, lo->impt.lib_name, lo->impt.sym_name, lo->name);

    return true;
}
static bool compile_lo_var(ctx_t * ctx, ira_lo_t * lo)
{
    if (!ira_pec_is_dt_complete(lo->var.qdt.dt))
    {
        report(ctx, "variable [%s] has incomplete data type", lo->name->str);
        return false;
    }

    mc_frag_type_t frag_type = lo->var.qdt.qual.const_q ? McFragRoData : McFragRwData;

    mc_frag_t * frag = ira_pec_c_get_frag(ctx, frag_type, lo->name);

    if (!compile_val(ctx, frag, lo->name, lo->var.val))
    {
        report(ctx, "failed to compile value for variable [%s]", lo->name->str);
        return false;
    }

    return true;
}
static bool compile_lo(ctx_t * ctx, ira_lo_t * lo)
{
    switch (lo->type)
    {
        case IraLoFunc:
            if (!ira_pec_ip_compile(ctx, lo))
            {
                return false;
            }
            break;
        case IraLoImpt:
            if (!compile_lo_impt(ctx, lo))
            {
                return false;
            }
            break;
        case IraLoVar:
            if (!compile_lo_var(ctx, lo))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}

static ira_lo_t * find_lo(ira_lo_t * nspc, ul_hs_t * name)
{
    for (ira_lo_nspc_node_t * node = nspc->nspc.body; node != NULL; node = node->next)
    {
        if (node->name == name)
        {
            return node->lo;
        }
    }

    return NULL;
}

static bool prepare_data(ctx_t * ctx)
{
    ira_lo_t * ep_lo = find_lo(ctx->pec->root, ctx->pec->ep_name);

    if (ep_lo == NULL)
    {
        report(ctx, "failed to find entry point [%s]", ctx->pec->ep_name->str);
        return false;
    }

    ira_pec_c_push_cl_elem(ctx, ep_lo);

    ctx->hst = ctx->pec->hst;

    ul_hsb_init(&ctx->hsb);

    ctx->out->ep_name = ep_lo->name;

    ctx->cl_frag_ins = &ctx->cl_frag;

    return true;
}
static bool process_data(ctx_t * ctx)
{
    bool res = true;

    while (ctx->cl_elems_cur < ctx->cl_elems_size)
    {
        cl_elem_t elem = ctx->cl_elems[ctx->cl_elems_cur++];

        if (!compile_lo(ctx, elem.lo))
        {
            res = false;
        }
    }

    return res;
}
static void move_frags(ctx_t * ctx)
{
    ctx->out->frag = ctx->cl_frag;

    ctx->cl_frag = NULL;
}
static bool compile_core(ctx_t * ctx)
{
    if (ctx->pec->root == NULL)
    {
        return false;
    }

    if (!prepare_data(ctx))
    {
        return false;
    }

    if (!process_data(ctx))
    {
        return false;
    }

    move_frags(ctx);

    return true;
}
bool ira_pec_c_compile(ira_pec_t * pec, mc_pea_t * out)
{
    ctx_t ctx = { .pec = pec, .out = out };

    bool res = compile_core(&ctx);

    mc_frag_destroy_chain(ctx.cl_frag);

    free(ctx.cl_elems);

    ul_hsb_cleanup(&ctx.hsb);

    return res;
}
