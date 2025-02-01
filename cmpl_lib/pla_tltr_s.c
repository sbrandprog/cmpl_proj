#include "pla_tltr_s.h"
#include "ira_func.h"
#include "ira_inst.h"
#include "ira_lo.h"
#include "ira_pec.h"
#include "ira_val.h"
#include "pla_cn.h"
#include "pla_expr.h"
#include "pla_stmt.h"

#define UNQ_VAR_NAME_SUFFIX "#"

typedef pla_tltr_tse_t tse_t;
typedef pla_tltr_vse_t vse_t;

typedef struct pla_ast_t_s_var var_t;
struct pla_ast_t_s_var
{
    var_t * next;
    ul_hs_t * orig_name;
    ul_hs_t * inst_name;
    ira_dt_qdt_t qdt;
};
typedef struct pla_ast_t_s_vvb vvb_t;
struct pla_ast_t_s_vvb
{
    vvb_t * prev;
    var_t * var;
};
typedef struct pla_ast_t_s_cfb cfb_t;
struct pla_ast_t_s_cfb
{
    cfb_t * prev;
    ul_hs_t * name;
    ul_hs_t * exit_label;
    ul_hs_t * cnt_label;
};

typedef struct pla_ast_t_s_expr expr_t;
typedef union pla_ast_t_s_expr_opd
{
    bool boolean;
    ira_int_type_t int_type;
    ul_hs_t * hs;
    pla_cn_t * cn;
    expr_t * expr;
} expr_opd_t;
typedef enum pla_ast_t_s_expr_val_type
{
    ExprValImmVar,
    ExprValImmDeref,
    ExprValVar,
    ExprValDeref,
    ExprVal_Count
} expr_val_type_t;
typedef union pla_ast_t_s_expr_val
{
    var_t * var;
} expr_val_t;
typedef struct pla_ast_t_s_expr_call_arg
{
    expr_t * val_expr;
    ira_dt_t * implct_cast_to;
} expr_call_arg_t;
struct pla_ast_t_s_expr
{
    pla_expr_t * base;
    union
    {
        expr_opd_t opds[PLA_EXPR_OPDS_SIZE];
        struct
        {
            expr_opd_t opd0, opd1, opd2;
        };
    };

    ira_dt_qdt_t val_qdt;

    expr_val_type_t val_type;
    expr_val_t val;

    union
    {
        struct
        {
            ira_val_t * val;
        } load_val;
        struct
        {
            size_t elems_size;
        } dt_tpl;
        struct
        {
            size_t args_size;
            ira_dt_func_vas_t * vas;
        } dt_func;
        struct
        {
            enum expr_ident_type
            {
                ExprIdentVar,
                ExprIdentLo,
                ExprIdent_Count
            } type;
            union
            {
                ira_lo_t * lo;
                var_t * var;
            };
        } ident;
        struct
        {
            size_t args_size;
            expr_call_arg_t * args;
        } call;
        struct
        {
            ira_dt_t * res_dt;
        } mmbr_acc;
        struct
        {
            ira_pec_optr_res_t res;
        } optr;
        struct
        {
            ira_dt_t * implct_cast_to;
        } asgn;
    };
};

typedef struct pla_ast_t_s_ctx
{
    pla_tltr_t * tltr;
    ira_dt_t * func_dt;
    pla_stmt_t * stmt;
    ira_func_t ** out;

    ul_hsb_t * hsb;
    ul_hst_t * hst;
    ira_pec_t * pec;

    ira_func_t * func;
    ira_dt_t * func_ret;

    vvb_t * vvb;
    size_t unq_var_index;
    cfb_t * cfb;
    size_t unq_label_index;
} ctx_t;

typedef bool val_proc_t(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out);


static const bool expr_val_is_left_val[ExprVal_Count] = {
    [ExprValImmVar] = false,
    [ExprValImmDeref] = false,
    [ExprValVar] = true,
    [ExprValDeref] = true
};

static const ul_ros_t label_base_logic_and = UL_ROS_MAKE("logic_and");
static const ul_ros_t label_base_logic_or = UL_ROS_MAKE("logic_or");
static const ul_ros_t label_base_cond = UL_ROS_MAKE("cond");
static const ul_ros_t label_base_pre_loop = UL_ROS_MAKE("pre_loop");
static const ul_ros_t label_base_post_loop = UL_ROS_MAKE("post_loop");

static const ul_ros_t label_suffix_end = UL_ROS_MAKE("end");
static const ul_ros_t label_suffix_tc_end = UL_ROS_MAKE("tc_end");
static const ul_ros_t label_suffix_fc_end = UL_ROS_MAKE("fc_end");
static const ul_ros_t label_suffix_body = UL_ROS_MAKE("body");
static const ul_ros_t label_suffix_cnt = UL_ROS_MAKE("cnt");
static const ul_ros_t label_suffix_exit = UL_ROS_MAKE("exit");

static ul_hs_t * get_unq_var_name(ctx_t * ctx, ul_hs_t * name)
{
    return ul_hsb_formatadd(ctx->hsb, ctx->hst, "%s%s%zi", name->str, UNQ_VAR_NAME_SUFFIX, ctx->unq_var_index++);
}
static ul_hs_t * get_unq_label_name(ctx_t * ctx, const ul_ros_t * base, size_t index, const ul_ros_t * suffix)
{
    return ul_hsb_formatadd(ctx->hsb, ctx->hst, "%s%zi%s", base->str, index, suffix->str);
}

static void push_vvb(ctx_t * ctx, vvb_t * vvb)
{
    vvb->prev = ctx->vvb;

    ctx->vvb = vvb;
}
static void pop_vvb(ctx_t * ctx)
{
    vvb_t * vvb = ctx->vvb;

    for (var_t * var = vvb->var; var != NULL;)
    {
        var_t * next = var->next;

        free(var);

        var = next;
    }

    ctx->vvb = vvb->prev;
}
static void push_cfb(ctx_t * ctx, cfb_t * cfb)
{
    cfb->prev = ctx->cfb;

    ctx->cfb = cfb;
}
static void pop_cfb(ctx_t * ctx)
{
    cfb_t * cfb = ctx->cfb;

    ctx->cfb = cfb->prev;
}

static expr_t * create_expr(ctx_t * ctx, pla_expr_t * base)
{
    expr_t * expr = malloc(sizeof(*expr));

    ul_assert(expr != NULL);

    *expr = (expr_t){ .base = base };

    return expr;
}
static void destroy_expr(ctx_t * ctx, expr_t * expr)
{
    if (expr == NULL)
    {
        return;
    }

    pla_expr_t * base = expr->base;

    const pla_expr_info_t * info = &pla_expr_infos[base->type];

    for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd)
    {
        switch (info->opds[opd])
        {
            case PlaExprOpdNone:
            case PlaExprOpdBoolean:
            case PlaExprOpdIntType:
            case PlaExprOpdHs:
            case PlaExprOpdCn:
                break;
            case PlaExprOpdExpr:
                destroy_expr(ctx, expr->opds[opd].expr);
                break;
            case PlaExprOpdExprNoTltn:
                break;
            case PlaExprOpdExprList1:
                for (expr_t * it = expr->opds[opd].expr; it != NULL;)
                {
                    expr_t * next = it->opds[1].expr;

                    destroy_expr(ctx, it);

                    it = next;
                }
                break;
            case PlaExprOpdExprListLink:
                break;
            default:
                ul_assert_unreachable();
        }
    }

    switch (base->type)
    {
        case PlaExprDtVoid:
        case PlaExprDtBool:
        case PlaExprDtInt:
        case PlaExprValVoid:
        case PlaExprValBool:
        case PlaExprChStr:
        case PlaExprNumStr:
            ira_val_destroy(expr->load_val.val);
            expr->load_val.val = NULL;
            break;
        case PlaExprCall:
            free(expr->call.args);
            break;
    }

    free(expr);
}

static var_t * create_var(ctx_t * ctx, ul_hs_t * name, ira_dt_t * dt, ira_dt_qual_t dt_qual, bool unq_name)
{
    var_t * new_var = malloc(sizeof(*new_var));

    ul_assert(new_var != NULL);

    *new_var = (var_t){
        .orig_name = name,
        .inst_name = name,
        .qdt = { .dt = dt, .qual = dt_qual }
    };

    if (unq_name)
    {
        new_var->inst_name = get_unq_var_name(ctx, name);
    }

    return new_var;
}
static var_t * define_var(ctx_t * ctx, ul_hs_t * name, ira_dt_t * dt, ira_dt_qual_t dt_qual, bool unq_name)
{
    var_t ** ins = &ctx->vvb->var;

    while (*ins != NULL)
    {
        var_t * var = *ins;

        if (name == var->orig_name)
        {
            return NULL;
        }

        ins = &(*ins)->next;
    }

    var_t * new_var = create_var(ctx, name, dt, dt_qual, unq_name);

    *ins = new_var;

    return new_var;
}
static var_t * get_imm_var(ctx_t * ctx, ira_dt_t * dt)
{
    var_t ** ins = &ctx->vvb->var;

    while (*ins != NULL)
    {
        ins = &(*ins)->next;
    }

    var_t * new_var = create_var(ctx, ctx->tltr->pds[PlaPdsImmVarName], dt, ira_dt_qual_none, true);

    *ins = new_var;

    return new_var;
}

static void push_def_var_inst(ctx_t * ctx, var_t * var)
{
    ira_inst_t def_var = { .type = IraInstDefVar, .opd0.hs = var->inst_name, .opd1.dt = var->qdt.dt, .opd2.dt_qual = var->qdt.qual };

    ira_func_push_inst(ctx->func, &def_var);
}
static void push_def_label_inst(ctx_t * ctx, ul_hs_t * name)
{
    ira_inst_t def_label = { .type = IraInstDefLabel, .opd0.hs = name };

    ira_func_push_inst(ctx->func, &def_label);
}
static void push_inst_imm_var0(ctx_t * ctx, ira_inst_t * inst, ira_dt_t * dt, var_t ** out)
{
    var_t * out_var = get_imm_var(ctx, dt);

    push_def_var_inst(ctx, out_var);

    inst->opd0.hs = out_var->inst_name;

    ira_func_push_inst(ctx->func, inst);

    *out = out_var;
}
static void push_inst_imm_var0_expr(ctx_t * ctx, expr_t * expr, ira_inst_t * inst)
{
    expr->val_type = ExprValImmVar;
    push_inst_imm_var0(ctx, inst, expr->val_qdt.dt, &expr->val.var);
}

static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out);

static bool get_cs_ascii(ctx_t * ctx, ul_hs_t * str, ira_val_t ** out)
{
    *out = ira_val_create(IraValImmArr, ctx->pec->dt_spcl.ascii_str);

    ira_int_type_t elem_type = (*out)->dt->arr.body->int_type;

    ul_assert(elem_type == IraIntU8);

    size_t expected_size = str->size + 1;

    (*out)->arr_val.data = malloc(expected_size * sizeof(*(*out)->arr_val.data));

    ul_assert((*out)->arr_val.data != NULL);

    ira_val_t ** ins = (*out)->arr_val.data;
    for (char *ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch)
    {
        if ((*ch & ~0x7F) != 0)
        {
            pla_tltr_report(ctx->tltr, "non-ascii character [0x%" PRIx8 "] in ascii string:\n%s", (uint8_t)*ch, str->str);
            return false;
        }

        if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui8 = (uint8_t)*ch }, ins++))
        {
            pla_tltr_report_pec_err(ctx->tltr);
            return false;
        }

        ++(*out)->arr_val.size;
    }

    if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui8 = 0 }, ins++))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    ++(*out)->arr_val.size;

    ul_assert((ins - (*out)->arr_val.data) == expected_size);

    return true;
}
static bool get_cs_utf8(ctx_t * ctx, ul_hs_t * str, ira_val_t ** out)
{
    *out = ira_val_create(IraValImmArr, ctx->pec->dt_spcl.utf8_str);

    ira_int_type_t elem_type = (*out)->dt->arr.body->int_type;

    ul_assert(elem_type == IraIntU8);

    size_t expected_size = str->size + 1;

    (*out)->arr_val.data = malloc(expected_size * sizeof(*(*out)->arr_val.data));

    ul_assert((*out)->arr_val.data != NULL);

    ira_val_t ** ins = (*out)->arr_val.data;
    for (char *ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch)
    {
        if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui8 = (uint8_t)*ch }, ins++))
        {
            pla_tltr_report_pec_err(ctx->tltr);
            return false;
        }

        ++(*out)->arr_val.size;
    }

    if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui8 = 0 }, ins++))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    ++(*out)->arr_val.size;

    ul_assert((ins - (*out)->arr_val.data) == expected_size);

    return true;
}
static bool get_cs_utf16(ctx_t * ctx, ul_hs_t * str, ira_val_t ** out)
{
    *out = ira_val_create(IraValImmArr, ctx->pec->dt_spcl.utf16_str);

    ira_int_type_t elem_type = (*out)->dt->arr.body->int_type;

    ul_assert(elem_type == IraIntU16);

    for (char *ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch)
    {
        uint32_t code = (uint8_t)*ch;

        if ((code & 0x80) != 0)
        {
            uint8_t clear_mask;
            uint8_t extra_codes;
            uint32_t code_min;

            if ((code & 0x60) == 0x40)
            {
                clear_mask = 0x1F;
                extra_codes = 1;
                code_min = 0x100;
            }
            else if ((code & 0x70) == 0x60)
            {
                clear_mask = 0x0F;
                extra_codes = 2;
                code_min = 0x800;
            }
            else if ((code & 0x78) == 0x70)
            {
                clear_mask = 0x07;
                extra_codes = 3;
                code_min = 0x10000;
            }
            else
            {
                pla_tltr_report(ctx->tltr, "invalid utf-8 multibyte introducer [0x%" PRIx8 "] in string:\n%s", (uint8_t)code, str->str);
                return false;
            }

            code &= clear_mask;

            while (extra_codes > 0)
            {
                --extra_codes;

                if (++ch == ch_end)
                {
                    pla_tltr_report(ctx->tltr, "unexpected end of utf-8 multibyte sequence in string:\n%s", str->str);
                    return false;
                }

                uint8_t extra = *ch;

                if ((extra & 0xC0) != 0x80)
                {
                    pla_tltr_report(ctx->tltr, "invalid utf-8 multibyte continuator [0x%" PRIx8 "] in string:\n%s", extra, str->str);
                    return false;
                }

                code = (code << 6) | extra;
            }

            if (code < code_min)
            {
                pla_tltr_report(ctx->tltr, "redundant utf-8 encoding, code point [0x%" PRIx32 "] uses [" PRIu8 "] bytes in string:\n%s", code, extra_codes + 1, str->str);
                return false;
            }

            if (0xD800 <= code && code <= 0xDFFF || code > 0x10FFFF)
            {
                pla_tltr_report(ctx->tltr, "multibyte utf8 resulted in invalid unicode code point [0x%" PRIx32 "] in string:\n%s", code, extra_codes + 1, str->str);
                return false;
            }
        }

        ul_arr_grow((*out)->arr_val.size + (code > 0xFFFF ? 2 : 1), &(*out)->arr_val.cap, (void **)&(*out)->arr_val.data, sizeof((*out)->arr_val.data));

        if (code > 0xFFFF)
        {
            code -= 0x10000;

            if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui16 = (uint16_t)(0xD800 + ((code >> 10) & 0x3FF)) }, &(*out)->arr_val.data[(*out)->arr_val.size++]))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }

            if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui16 = (uint16_t)(0xDC00 + (code & 0x3FF)) }, &(*out)->arr_val.data[(*out)->arr_val.size++]))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }
        }
        else
        {
            if (!ira_pec_make_val_imm_int(ctx->pec, elem_type, (ira_int_t){ .ui16 = (uint16_t)code }, &(*out)->arr_val.data[(*out)->arr_val.size++]))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }
        }
    }

    return true;
}

static bool is_int_ns(ctx_t * ctx, ul_hs_t * tag, ira_int_type_t * out)
{
    if (tag == ctx->tltr->pds[PlaPdsU8])
    {
        *out = IraIntU8;
    }
    else if (tag == ctx->tltr->pds[PlaPdsS8])
    {
        *out = IraIntS8;
    }
    else if (tag == ctx->tltr->pds[PlaPdsU16])
    {
        *out = IraIntU16;
    }
    else if (tag == ctx->tltr->pds[PlaPdsS16])
    {
        *out = IraIntS16;
    }
    else if (tag == ctx->tltr->pds[PlaPdsU32])
    {
        *out = IraIntU32;
    }
    else if (tag == ctx->tltr->pds[PlaPdsS32])
    {
        *out = IraIntS32;
    }
    else if (tag == ctx->tltr->pds[PlaPdsU64])
    {
        *out = IraIntU64;
    }
    else if (tag == ctx->tltr->pds[PlaPdsS64])
    {
        *out = IraIntS64;
    }
    else
    {
        return false;
    }

    return true;
}
static bool get_ns_radix(wchar_t ch, uint64_t * out)
{
    if ('A' <= ch && ch <= 'Z')
    {
        ch += ('a' - 'A');
    }

    switch (ch)
    {
        case 'b':
            *out = 2;
            return true;
        case 'o':
            *out = 8;
            return true;
        case 'd':
            *out = 10;
            return true;
        case 'x':
            *out = 16;
            return true;
    }

    return false;
}
static bool get_ns_digit(wchar_t ch, uint64_t * out)
{
    if ('0' <= ch && ch <= '9')
    {
        *out = (uint64_t)(ch - '0');
        return true;
    }
    else if ('A' <= ch && ch <= 'Z')
    {
        *out = (uint64_t)(ch - 'A') + 10ull;
        return true;
    }
    else if ('a' <= ch && ch <= 'z')
    {
        *out = (uint64_t)(ch - 'a') + 10ull;
        return true;
    }

    return false;
}
static bool get_ns_int(ctx_t * ctx, ul_hs_t * str, ira_int_type_t int_type, ira_val_t ** out)
{
    const char *cur = str->str, *cur_end = cur + str->size;

    uint64_t var = 0, radix = 10;

    if (*cur == '0' && cur + 1 != cur_end && get_ns_radix(*(cur + 1), &radix))
    {
        if (cur + 2 == cur_end)
        {
            pla_tltr_report(ctx->tltr, "invalid integer string (radix form):\n%s", str->str);
            return false;
        }

        cur += 2;
    }

    const uint64_t pre_ovf_val = UINT64_MAX / radix, pre_ovf_digit = UINT64_MAX % radix;

    uint64_t digit;
    while (cur != cur_end)
    {
        if (*cur == '_')
        {
            ++cur;
            continue;
        }

        if (!get_ns_digit(*cur, &digit) || digit > radix)
        {
            pla_tltr_report(ctx->tltr, "digit [%llu] is greater than radix [%llu] in integer string:\n%s", digit, radix, str->str);
            return false;
        }

        if (var > pre_ovf_val || var == pre_ovf_val && digit > pre_ovf_digit)
        {
            pla_tltr_report(ctx->tltr, "integer string is exceeding precision limit of parsing unit:\n%s", str->str);
            return false;
        }

        var = var * radix + digit;

        ++cur;
    }

    uint64_t lim = ira_int_infos[int_type].max;

    if (var > lim)
    {
        pla_tltr_report(ctx->tltr, "integer string is exceeding precision limit of data type [%i]:\n%s", int_type, str->str);
        return false;
    }

    if (!ira_pec_make_val_imm_int(ctx->pec, int_type, (ira_int_t){ .ui64 = var }, out))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}

static bool get_dt_void_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    if (!ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_void, out))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}
static bool get_dt_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    if (!ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_bool, out))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}
static bool get_dt_int_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    ira_int_type_t int_type = expr->opd0.int_type;

    if (int_type >= IraInt_Count)
    {
        pla_tltr_report(ctx->tltr, "invalid int_type for int dt:%i", int_type);
        return false;
    }

    if (!ira_pec_make_val_imm_dt(ctx->pec, &ctx->pec->dt_ints[expr->opd0.int_type], out))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}
static bool get_void_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    if (!ira_pec_make_val_imm_void(ctx->pec, out))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}
static bool get_bool_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    if (!ira_pec_make_val_imm_bool(ctx->pec, expr->opd0.boolean, out))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}
static bool get_cs_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    ul_hs_t * data = expr->opd0.hs;
    ul_hs_t * tag = expr->opd1.hs;

    if (tag == ctx->tltr->pds[PlaPdsAsciiStrTag])
    {
        if (!get_cs_ascii(ctx, data, out))
        {
            return false;
        }
    }
    else if (tag == ctx->tltr->pds[PlaPdsUtf8StrTag])
    {
        if (!get_cs_utf8(ctx, data, out))
        {
            return false;
        }
    }
    else if (tag == ctx->tltr->pds[PlaPdsUtf16StrTag])
    {
        if (!get_cs_utf16(ctx, data, out))
        {
            return false;
        }
    }
    else
    {
        pla_tltr_report(ctx->tltr, "unknown type-tag [%s] for character string:\n%s", tag->str, data->str);
        return false;
    }

    return true;
}
static bool get_ns_val_proc(ctx_t * ctx, pla_expr_t * expr, ira_val_t ** out)
{
    ul_hs_t * data = expr->opd0.hs;
    ul_hs_t * tag = expr->opd1.hs;

    ira_int_type_t int_type;

    if (is_int_ns(ctx, tag, &int_type))
    {
        if (!get_ns_int(ctx, data, int_type, out))
        {
            return false;
        }
    }
    else
    {
        pla_tltr_report(ctx->tltr, "unknown type-tag [%s] for number string:\n%s", tag->str, data->str);
        return false;
    }

    return true;
}

static ira_lo_t * find_cn_lo(ctx_t * ctx, ira_lo_t * nspc, pla_cn_t * cn)
{
    for (ira_lo_nspc_node_t * node = nspc->nspc.body; node != NULL; node = node->next)
    {
        if (node->name != cn->name)
        {
            continue;
        }

        ira_lo_t * lo = node->lo;

        switch (node->lo->type)
        {
            case IraLoNone:
                break;
            case IraLoNspc:
                if (cn->sub_name != NULL)
                {
                    return find_cn_lo(ctx, lo, cn->sub_name);
                }
                break;
            case IraLoFunc:
            case IraLoImpt:
            case IraLoVar:
                if (cn->sub_name == NULL)
                {
                    return lo;
                }
                break;
            default:
                ul_assert_unreachable();
        }

        break;
    }

    return NULL;
}
static bool process_ident_lo(ctx_t * ctx, expr_t * expr, ira_lo_t * lo)
{
    switch (lo->type)
    {
        case IraLoNspc:
            return false;
        case IraLoFunc:
            if (!ira_pec_get_dt_ptr(ctx->pec, lo->func->dt, ira_dt_qual_none, &expr->val_qdt.dt))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }
            break;
        case IraLoImpt:
            if (!ira_pec_get_dt_ptr(ctx->pec, lo->impt.dt, ira_dt_qual_none, &expr->val_qdt.dt))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }
            break;
        case IraLoVar:
            expr->val_qdt = lo->var.qdt;
            break;
        default:
            ul_assert_unreachable();
    }

    expr->ident.type = ExprIdentLo;
    expr->ident.lo = lo;

    return true;
}
static bool get_tpl_mmbr_dt(ctx_t * ctx, ira_dt_t * dt, ul_hs_t * mmbr, ira_dt_t ** out)
{
    for (ira_dt_ndt_t *elem = dt->tpl.elems, *elem_end = elem + dt->tpl.elems_size; elem != elem_end; ++elem)
    {
        if (mmbr == elem->name)
        {
            if (!ira_pec_apply_qual(ctx->pec, elem->dt, dt->tpl.qual, out))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }

            return true;
        }
    }

    return false;
}
static bool get_mmbr_acc_dt(ctx_t * ctx, ira_dt_t * opd_dt, ul_hs_t * mmbr, ira_dt_t ** out)
{
    switch (opd_dt->type)
    {
        case IraDtVoid:
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
            return false;
        case IraDtTpl:
            if (!get_tpl_mmbr_dt(ctx, opd_dt, mmbr, out))
            {
                return false;
            }
            break;
        case IraDtStct:
        {
            ira_dt_t * body = opd_dt->stct.tag->body;

            if (body == NULL || !get_tpl_mmbr_dt(ctx, body, mmbr, out))
            {
                return false;
            }

            break;
        }
        case IraDtArr:
            if (!get_tpl_mmbr_dt(ctx, opd_dt->arr.assoc_tpl, mmbr, out))
            {
                return false;
            }
            break;
        case IraDtFunc:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool check_for_va_support(ctx_t * ctx)
{
    ira_dt_func_vas_t * vas = ctx->func_dt->func.vas;

    switch (vas->type)
    {
        case IraDtFuncVasNone:
            return false;
        case IraDtFuncVasCstyle:
            return true;
        default:
            ul_assert_unreachable();
    }
}

static bool translate_expr0_opds(ctx_t * ctx, expr_t * expr)
{
    pla_expr_t * base = expr->base;

    const pla_expr_info_t * info = &pla_expr_infos[base->type];

    for (size_t opd = 0; opd < PLA_EXPR_OPDS_SIZE; ++opd)
    {
        switch (info->opds[opd])
        {
            case PlaExprOpdNone:
                break;
            case PlaExprOpdBoolean:
                expr->opds[opd].boolean = base->opds[opd].boolean;
                break;
            case PlaExprOpdIntType:
                expr->opds[opd].int_type = base->opds[opd].int_type;
                break;
            case PlaExprOpdHs:
                expr->opds[opd].hs = base->opds[opd].hs;
                break;
            case PlaExprOpdCn:
                expr->opds[opd].cn = base->opds[opd].cn;
                break;
            case PlaExprOpdExpr:
                if (!translate_expr0(ctx, base->opds[opd].expr, &expr->opds[opd].expr))
                {
                    return false;
                }
                break;
            case PlaExprOpdExprNoTltn:
                break;
            case PlaExprOpdExprList1:
            {
                expr_t ** it_ins = &expr->opds[opd].expr;

                for (pla_expr_t * it = base->opds[opd].expr; it != NULL; it = it->opds[1].expr, it_ins = &(*it_ins)->opds[1].expr)
                {
                    if (!translate_expr0(ctx, it, it_ins))
                    {
                        return false;
                    }
                }
                break;
            }
            case PlaExprOpdExprListLink:
                break;
            default:
                ul_assert_unreachable();
        }
    }

    return true;
}
static bool translate_expr0_blank(ctx_t * ctx, expr_t * expr)
{
    return true;
}
static bool translate_expr0_invalid(ctx_t * ctx, expr_t * expr)
{
    pla_tltr_report(ctx->tltr, "this expression type is untranslatable");
    return false;
}
static bool translate_expr0_load_val(ctx_t * ctx, expr_t * expr)
{
    pla_expr_t * base = expr->base;

    val_proc_t * proc;

    switch (base->type)
    {
        case PlaExprDtVoid:
            proc = get_dt_void_val_proc;
            break;
        case PlaExprDtBool:
            proc = get_dt_bool_val_proc;
            break;
        case PlaExprDtInt:
            proc = get_dt_int_val_proc;
            break;
        case PlaExprValVoid:
            proc = get_void_val_proc;
            break;
        case PlaExprValBool:
            proc = get_bool_val_proc;
            break;
        case PlaExprChStr:
            proc = get_cs_val_proc;
            break;
        case PlaExprNumStr:
            proc = get_ns_val_proc;
            break;
        default:
            ul_assert_unreachable();
    }

    if (!proc(ctx, base, &expr->load_val.val))
    {
        return false;
    }

    expr->val_qdt.dt = expr->load_val.val->dt;

    return true;
}
static bool translate_expr0_dt_ptr(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * dt_dt = &ctx->pec->dt_dt;

    if (expr->opd0.expr->val_qdt.dt != dt_dt)
    {
        pla_tltr_report(ctx->tltr, "dt_ptr requires 'dt' data type for opd[0]");
        return false;
    }

    expr->val_qdt.dt = dt_dt;

    return true;
}
static bool translate_expr0_dt_tpl(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * dt_dt = &ctx->pec->dt_dt;

    size_t elems_size = 0;

    for (expr_t * elem = expr->opd1.expr; elem != NULL; elem = elem->opd1.expr, ++elems_size)
    {
        if (elem->base->type != PlaExprDtTplElem)
        {
            pla_tltr_report(ctx->tltr, "dt_tpl: unexpected expression [%s]", pla_expr_infos[elem->base->type].type_str.str);
            return false;
        }

        expr_t * elem_expr = elem->opd0.expr;

        if (elem_expr->val_qdt.dt != dt_dt)
        {
            pla_tltr_report(ctx->tltr, "dt_tpl requires 'dt' data type for [%zi]th argument", (size_t)(elems_size));
            return false;
        }
    }

    expr->dt_tpl.elems_size = elems_size;

    expr->val_qdt.dt = dt_dt;

    return true;
}
static bool translate_expr0_dt_arr(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * dt_dt = &ctx->pec->dt_dt;

    if (expr->opd0.expr->val_qdt.dt != dt_dt)
    {
        pla_tltr_report(ctx->tltr, "dt_arr requires 'dt' data type for opd[0]");
        return false;
    }

    ira_dt_t * dim_opd_dt = expr->opd1.expr->val_qdt.dt;

    if (dim_opd_dt->type != IraDtVoid && dim_opd_dt->type != IraDtInt)
    {
        pla_tltr_report(ctx->tltr, "dt_arr requires 'void' or 'int' data type for opd[1]");
        return false;
    }

    expr->val_qdt.dt = dt_dt;

    return true;
}
static bool translate_expr0_dt_func(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * dt_dt = &ctx->pec->dt_dt;

    size_t args_size = 0;
    ira_dt_func_vas_t * vas = ctx->pec->dt_func_vass.none;

    for (expr_t * arg = expr->opd1.expr; arg != NULL; arg = arg->opd1.expr, ++args_size)
    {
        switch (arg->base->type)
        {
            case PlaExprDtFuncArg:
            {
                expr_t * arg_expr = arg->opd0.expr;

                if (arg_expr->val_qdt.dt != dt_dt)
                {
                    pla_tltr_report(ctx->tltr, "dt_func requires 'dt' data type for [%zi]th argument", (size_t)(args_size));
                    return false;
                }
                break;
            }
            case PlaExprDtFuncVa:
                if (arg->opd0.hs == ctx->tltr->pds[PlaPdsCstyle])
                {
                    vas = ctx->pec->dt_func_vass.cstyle;
                }
                else
                {
                    pla_tltr_report(ctx->tltr, "dt_func: invalid variable arguments scheme [%s]", arg->opd0.hs->str);
                    return false;
                }

                if (arg->opd1.expr != NULL)
                {
                    pla_tltr_report(ctx->tltr, "dt_func: variable arguments scheme must be last in arguments list");
                    return false;
                }

                --args_size;
                break;
            default:
                pla_tltr_report(ctx->tltr, "dt_func: unexpected expression [%s]", pla_expr_infos[arg->base->type].type_str.str);
                return false;
        }
    }

    expr->dt_func.args_size = args_size;
    expr->dt_func.vas = vas;

    if (expr->opd0.expr->val_qdt.dt != dt_dt)
    {
        pla_tltr_report(ctx->tltr, "dt_func requires 'dt' data type for return value");
        return false;
    }

    expr->val_qdt.dt = dt_dt;

    return true;
}
static bool translate_expr0_dt_const(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * dt_dt = &ctx->pec->dt_dt;

    if (expr->opd0.expr->val_qdt.dt != dt_dt)
    {
        pla_tltr_report(ctx->tltr, "make_dt_const requires 'dt' data type for first operand");
        return false;
    }

    expr->val_qdt.dt = dt_dt;

    return true;
}
static bool translate_expr0_ident(ctx_t * ctx, expr_t * expr)
{
    pla_cn_t * cn = expr->opd0.cn;

    if (cn->sub_name == NULL)
    {
        for (vvb_t * vvb = ctx->vvb; vvb != NULL; vvb = vvb->prev)
        {
            for (var_t * var = vvb->var; var != NULL; var = var->next)
            {
                if (var->orig_name == cn->name)
                {
                    expr->val_qdt = var->qdt;

                    expr->ident.type = ExprIdentVar;
                    expr->ident.var = var;
                    return true;
                }
            }
        }
    }

    for (vse_t * vse = ctx->tltr->vse; vse != NULL; vse = vse->prev)
    {
        switch (vse->type)
        {
            case PlaTltrVseNspc:
            {
                ira_lo_t * lo = find_cn_lo(ctx, vse->nspc.lo, cn);

                if (lo != NULL && process_ident_lo(ctx, expr, lo))
                {
                    return true;
                }

                break;
            }
            default:
                ul_assert_unreachable();
        }
    }

    pla_tltr_report(ctx->tltr, "failed to find an object for identifier [%s]", cn->name->str);
    return false;
}
static bool translate_expr0_call_func_ptr(ctx_t * ctx, expr_t * expr, ira_dt_t * callee_dt)
{
    ira_dt_t * func_dt = callee_dt->ptr.body;

    {
        size_t args_size = 0;

        for (expr_t * arg = expr->opd1.expr; arg != NULL; arg = arg->opd1.expr)
        {
            ++args_size;
        }

        switch (func_dt->func.vas->type)
        {
            case IraDtFuncVasNone:
                if (args_size != func_dt->func.args_size)
                {
                    pla_tltr_report(ctx->tltr, "count of call arguments [%zi] must be equal to count of function arguments [%zi]", args_size, func_dt->func.args_size);
                    return false;
                }
                break;
            case IraDtFuncVasCstyle:
                if (args_size < func_dt->func.args_size)
                {
                    pla_tltr_report(ctx->tltr, "count of call arguments [%zi] must be equal or greater than count of function arguments [%zi]", args_size, func_dt->func.args_size);
                    return false;
                }
                break;
            default:
                ul_assert_unreachable();
        }

        expr->call.args_size = args_size;

        expr->call.args = malloc(sizeof(*expr->call.args) * args_size);

        ul_assert(expr->call.args != NULL);

        memset(expr->call.args, 0, sizeof(*expr->call.args) * args_size);
    }

    ira_dt_ndt_t *arg_func_dt = func_dt->func.args, *arg_func_dt_end = arg_func_dt + func_dt->func.args_size;
    expr_call_arg_t * arg = expr->call.args;
    for (expr_t * arg_expr = expr->opd1.expr; arg_expr != NULL; arg_expr = arg_expr->opd1.expr, ++arg)
    {
        if (arg_expr->base->type != PlaExprCallArg)
        {
            pla_tltr_report(ctx->tltr, "unexpected expression [%s] in function call", pla_expr_infos[arg_expr->base->type].type_str.str);
            return false;
        }

        expr_t * arg_expr_val = arg_expr->opd0.expr;

        *arg = (expr_call_arg_t){ .val_expr = arg_expr_val, .implct_cast_to = NULL };

        if (arg_func_dt != arg_func_dt_end)
        {
            if (!ira_dt_is_equivalent(arg_expr_val->val_qdt.dt, arg_func_dt->dt))
            {
                if (!ira_dt_is_castable(arg_expr_val->val_qdt.dt, arg_func_dt->dt, true))
                {
                    pla_tltr_report(ctx->tltr, "data type of [%zi]th call argument does not match and cannot be implicitly converted to a data type of function argument", (size_t)(arg_func_dt - func_dt->func.args + 1));
                    return false;
                }

                arg->implct_cast_to = arg_func_dt->dt;
            }

            ++arg_func_dt;
        }
    }

    expr->val_qdt.dt = func_dt->func.ret;

    return true;
}
static bool translate_expr0_call(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * callee_dt = expr->opd0.expr->val_qdt.dt;

    if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc)
    {
        if (!translate_expr0_call_func_ptr(ctx, expr, callee_dt))
        {
            return false;
        }
    }
    else
    {
        pla_tltr_report(ctx->tltr, "unknown callee type");
        return false;
    }

    return true;
}
static bool translate_expr0_subscr(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd1 = expr->opd1.expr;

    if (opd1->val_qdt.dt->type != IraDtInt)
    {
        pla_tltr_report(ctx->tltr, "opd[1] must have an integer data type");
        return false;
    }

    expr_t * opd0 = expr->opd0.expr;
    ira_dt_t * opd0_dt = opd0->val_qdt.dt;

    ira_dt_t * res_dt;
    ira_dt_qual_t res_dt_qual;

    switch (opd0_dt->type)
    {
        case IraDtVec:
            res_dt = opd0_dt->vec.body;
            res_dt_qual = opd0_dt->vec.qual;
            break;
        case IraDtPtr:
            res_dt = opd0_dt->ptr.body;
            res_dt_qual = opd0_dt->ptr.qual;
            break;
        case IraDtArr:
            res_dt = opd0_dt->arr.body;
            res_dt_qual = opd0_dt->arr.qual;
            break;
        default:
            pla_tltr_report(ctx->tltr, "opd[0] must have a (vector | pointer | array) data type");
            return false;
    }

    if (!ira_pec_apply_qual(ctx->pec, res_dt, res_dt_qual, &res_dt))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    expr->val_qdt.dt = res_dt;
    expr->val_qdt.qual = opd0->val_qdt.qual;

    return true;
}
static bool translate_expr0_mmbr_acc(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd = expr->opd0.expr;

    ira_dt_t * opd_dt = opd->val_qdt.dt;
    ul_hs_t * mmbr = expr->opd1.hs;

    if (!get_mmbr_acc_dt(ctx, opd_dt, mmbr, &expr->mmbr_acc.res_dt))
    {
        pla_tltr_report(ctx->tltr, "operand of [%s] data type does not support member access or [%s] member", ira_dt_infos[opd_dt->type].type_str.str, mmbr->str);
        return false;
    }

    expr->val_qdt.dt = expr->mmbr_acc.res_dt;
    expr->val_qdt.qual = opd->val_qdt.qual;

    return true;
}
static bool translate_expr0_deref(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * ptr_dt = expr->opd0.expr->val_qdt.dt;

    if (ptr_dt->type != IraDtPtr)
    {
        pla_tltr_report(ctx->tltr, "deref: invalid operand [%s]", ira_dt_infos[ptr_dt->type].type_str.str);
        return false;
    }

    expr->val_qdt.dt = ptr_dt->ptr.body;
    expr->val_qdt.qual = ptr_dt->ptr.qual;

    return true;
}
static bool translate_expr0_va_start(ctx_t * ctx, expr_t * expr)
{
    if (!check_for_va_support(ctx))
    {
        pla_tltr_report(ctx->tltr, "va_start: used in function with no variable arguments");
        return false;
    }

    expr->val_qdt.dt = ctx->pec->dt_spcl.va_elem;

    return true;
}
static bool translate_expr0_va_arg(ctx_t * ctx, expr_t * expr)
{
    if (!check_for_va_support(ctx))
    {
        pla_tltr_report(ctx->tltr, "va_arg: used in function with no variable arguments");
        return false;
    }

    ira_dt_t * arg_dt;

    if (!pla_tltr_calculate_expr_dt(ctx->tltr, expr->base->opd0.expr, &arg_dt))
    {
        return false;
    }

    if (expr->opd1.expr->val_qdt.dt != ctx->pec->dt_spcl.va_elem)
    {
        pla_tltr_report(ctx->tltr, "va_arg: opd[1] must be a va_elem data type (result of va_start)");
        return false;
    }

    expr->val_qdt.dt = arg_dt;

    return true;
}
static bool translate_expr0_addr_of(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd = expr->opd0.expr;

    if (!ira_pec_get_dt_ptr(ctx->pec, opd->val_qdt.dt, opd->val_qdt.qual, &expr->val_qdt.dt))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    return true;
}
static bool translate_expr0_cast(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * from = expr->opd0.expr->val_qdt.dt;
    ira_dt_t * to;

    if (!pla_tltr_calculate_expr_dt(ctx->tltr, expr->base->opd1.expr, &to))
    {
        return false;
    }

    if (!ira_dt_is_castable(from, to, false))
    {
        pla_tltr_report(ctx->tltr, "cannot cast operand of [%s] data type to a [%s] data type", ira_dt_infos[from->type].type_str.str, ira_dt_infos[to->type].type_str.str);
        return false;
    }

    expr->val_qdt.dt = to;

    return true;
}
static bool translate_expr0_unr(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * left = expr->opd0.expr->val_qdt.dt;
    const pla_expr_info_t * info = &pla_expr_infos[expr->base->type];

    if (!ira_pec_get_best_optr(ctx->pec, info->optr_ctg, left, NULL, &expr->optr.res))
    {
        pla_tltr_report(ctx->tltr, "could not find an operator for [%s] expression with [%s] operand", pla_expr_infos[expr->base->type].type_str.str, ira_dt_infos[left->type].type_str.str);
        return false;
    }

    expr->val_qdt.dt = expr->optr.res.res_dt;

    return true;
}
static bool translate_expr0_bin(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * left = expr->opd0.expr->val_qdt.dt;
    ira_dt_t * right = expr->opd1.expr->val_qdt.dt;
    const pla_expr_info_t * info = &pla_expr_infos[expr->base->type];

    if (!ira_pec_get_best_optr(ctx->pec, info->optr_ctg, left, right, &expr->optr.res))
    {
        pla_tltr_report(ctx->tltr, "could not find an operator for [%s] expression with [%s], [%s] operands", pla_expr_infos[expr->base->type].type_str.str, ira_dt_infos[left->type].type_str.str, ira_dt_infos[right->type].type_str.str);
        return false;
    }

    expr->val_qdt.dt = expr->optr.res.res_dt;

    return true;
}
static bool translate_expr0_logic_optr(ctx_t * ctx, expr_t * expr)
{
    ira_dt_t * dt_bool = &ctx->pec->dt_bool;

    if (expr->opd0.expr->val_qdt.dt != dt_bool)
    {
        pla_tltr_report(ctx->tltr, "logic_and: opd[0] should have a boolean data type");
        return false;
    }

    if (expr->opd1.expr->val_qdt.dt != dt_bool)
    {
        pla_tltr_report(ctx->tltr, "logic_and: opd[1] should have a boolean data type");
        return false;
    }

    expr->val_qdt.dt = dt_bool;

    return true;
}
static bool translate_expr0_asgn(ctx_t * ctx, expr_t * expr)
{
    ira_dt_qdt_t * opd0_qdt = &expr->opd0.expr->val_qdt;
    ira_dt_t * opd1_dt = expr->opd1.expr->val_qdt.dt;

    if (opd0_qdt->qual.const_q == true)
    {
        pla_tltr_report(ctx->tltr, "assignment operator requires left-side operand to have a non-constant data type");
        return false;
    }

    if (!ira_dt_is_equivalent(opd0_qdt->dt, opd1_dt))
    {
        if (!ira_dt_is_castable(opd1_dt, opd0_qdt->dt, true))
        {
            pla_tltr_report(ctx->tltr, "assignment operator requires operands to be of equivalent data types or implicitly convertible");
            return false;
        }

        expr->asgn.implct_cast_to = opd0_qdt->dt;
    }

    expr->val_qdt = *opd0_qdt;

    return true;
}
static bool translate_expr0_tse(ctx_t * ctx, pla_expr_t * base, expr_t ** out)
{
    *out = create_expr(ctx, base);

    expr_t * expr = *out;

    if (!translate_expr0_opds(ctx, expr))
    {
        return false;
    }

    typedef bool translate_expr0_proc_t(ctx_t * ctx, expr_t * expr);

    static translate_expr0_proc_t * const translate_expr0_procs[PlaExpr_Count] = {
        [PlaExprNone] = translate_expr0_invalid,
        [PlaExprDtVoid] = translate_expr0_load_val,
        [PlaExprDtBool] = translate_expr0_load_val,
        [PlaExprDtInt] = translate_expr0_load_val,
        [PlaExprDtPtr] = translate_expr0_dt_ptr,
        [PlaExprDtTpl] = translate_expr0_dt_tpl,
        [PlaExprDtTplElem] = translate_expr0_blank,
        [PlaExprDtArr] = translate_expr0_dt_arr,
        [PlaExprDtFunc] = translate_expr0_dt_func,
        [PlaExprDtFuncArg] = translate_expr0_blank,
        [PlaExprDtFuncVa] = translate_expr0_blank,
        [PlaExprDtConst] = translate_expr0_dt_const,
        [PlaExprValVoid] = translate_expr0_load_val,
        [PlaExprValBool] = translate_expr0_load_val,
        [PlaExprChStr] = translate_expr0_load_val,
        [PlaExprNumStr] = translate_expr0_load_val,
        [PlaExprIdent] = translate_expr0_ident,
        [PlaExprCall] = translate_expr0_call,
        [PlaExprCallArg] = translate_expr0_blank,
        [PlaExprSubscr] = translate_expr0_subscr,
        [PlaExprMmbrAcc] = translate_expr0_mmbr_acc,
        [PlaExprDeref] = translate_expr0_deref,
        [PlaExprVaStart] = translate_expr0_va_start,
        [PlaExprVaArg] = translate_expr0_va_arg,
        [PlaExprAddrOf] = translate_expr0_addr_of,
        [PlaExprCast] = translate_expr0_cast,
        [PlaExprLogicNeg] = translate_expr0_unr,
        [PlaExprBitNeg] = translate_expr0_unr,
        [PlaExprArithNeg] = translate_expr0_unr,
        [PlaExprMul] = translate_expr0_bin,
        [PlaExprDiv] = translate_expr0_bin,
        [PlaExprMod] = translate_expr0_bin,
        [PlaExprAdd] = translate_expr0_bin,
        [PlaExprSub] = translate_expr0_bin,
        [PlaExprLeShift] = translate_expr0_bin,
        [PlaExprRiShift] = translate_expr0_bin,
        [PlaExprLess] = translate_expr0_bin,
        [PlaExprLessEq] = translate_expr0_bin,
        [PlaExprGrtr] = translate_expr0_bin,
        [PlaExprGrtrEq] = translate_expr0_bin,
        [PlaExprEq] = translate_expr0_bin,
        [PlaExprNeq] = translate_expr0_bin,
        [PlaExprBitAnd] = translate_expr0_bin,
        [PlaExprBitXor] = translate_expr0_bin,
        [PlaExprBitOr] = translate_expr0_bin,
        [PlaExprLogicAnd] = translate_expr0_logic_optr,
        [PlaExprLogicOr] = translate_expr0_logic_optr,
        [PlaExprAsgn] = translate_expr0_asgn,
    };

    translate_expr0_proc_t * proc = translate_expr0_procs[base->type];

    ul_assert(proc != NULL);

    if (!proc(ctx, expr))
    {
        return false;
    }

    return true;
}
static bool translate_expr0(ctx_t * ctx, pla_expr_t * base, expr_t ** out)
{
    tse_t tse = { .type = PlaTltrTseExpr, .expr = base };

    pla_tltr_push_tse(ctx->tltr, &tse);

    bool res = translate_expr0_tse(ctx, base, out);

    pla_tltr_pop_tse(ctx->tltr, &tse);

    return res;
}


static bool translate_expr1(ctx_t * ctx, expr_t * expr);

static bool translate_expr1_imm_var(ctx_t * ctx, expr_t * expr, var_t ** out)
{
    if (!translate_expr1(ctx, expr))
    {
        return false;
    }

    switch (expr->val_type)
    {
        case ExprValImmVar:
        case ExprValVar:
            *out = expr->val.var;
            break;
        case ExprValImmDeref:
        case ExprValDeref:
        {
            ira_inst_t read_ptr = { .type = IraInstReadPtr, .opd1.hs = expr->val.var->inst_name };

            push_inst_imm_var0(ctx, &read_ptr, expr->val_qdt.dt, out);
            break;
        }
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool translate_expr1_var_ptr(ctx_t * ctx, expr_t * expr, var_t ** out)
{
    if (!translate_expr1(ctx, expr))
    {
        return false;
    }

    switch (expr->val_type)
    {
        case ExprValImmVar:
        case ExprValVar:
        {
            ira_dt_t * ptr_dt;

            if (!ira_pec_get_dt_ptr(ctx->pec, expr->val.var->qdt.dt, expr->val.var->qdt.qual, &ptr_dt))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }

            ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = expr->val.var->inst_name };

            push_inst_imm_var0(ctx, &addr_of, ptr_dt, out);

            break;
        }
        case ExprValImmDeref:
        case ExprValDeref:
            *out = expr->val.var;
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}

static void set_expr_ptr_val(ctx_t * ctx, expr_t * expr, expr_val_type_t val_type, var_t * ptr_var)
{
    switch (val_type)
    {
        case ExprValImmVar:
        case ExprValImmDeref:
            expr->val_type = ExprValImmDeref;
            expr->val.var = ptr_var;
            break;
        case ExprValVar:
        case ExprValDeref:
            expr->val_type = ExprValDeref;
            expr->val.var = ptr_var;
            break;
        default:
            ul_assert_unreachable();
    }
}

static bool translate_expr1_load_val(ctx_t * ctx, expr_t * expr)
{
    ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = expr->load_val.val };

    push_inst_imm_var0_expr(ctx, expr, &load_val);

    expr->load_val.val = NULL;

    return true;
}
static bool translate_expr1_dt_ptr(ctx_t * ctx, expr_t * expr)
{
    var_t * opd_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var))
    {
        return false;
    }

    ira_inst_t make_dt_ptr = { .type = IraInstMakeDtPtr, .opd1.hs = opd_var->inst_name, .opd2.dt_qual = ira_dt_qual_none };

    push_inst_imm_var0_expr(ctx, expr, &make_dt_ptr);

    return true;
}
static bool translate_expr1_dt_tpl(ctx_t * ctx, expr_t * expr)
{
    size_t elems_size = expr->dt_tpl.elems_size;

    ul_hs_t ** elems = malloc(elems_size * sizeof(*elems));

    ul_assert(elems != NULL);

    ul_hs_t ** ids = malloc(elems_size * sizeof(*ids));

    ul_assert(ids != NULL);

    ul_hs_t ** elem_var_ins = elems;
    ul_hs_t ** id_ins = ids;
    for (expr_t * elem = expr->opd1.expr; elem != NULL; elem = elem->opd1.expr, ++elem_var_ins, ++id_ins)
    {
        var_t * elem_var;

        if (!translate_expr1_imm_var(ctx, elem->opd0.expr, &elem_var))
        {
            free(elems);
            free(ids);
            return false;
        }

        *elem_var_ins = elem_var->inst_name;
        *id_ins = elem->opd2.hs;
    }

    ira_inst_t make_dt_stct = { .type = IraInstMakeDtTpl, .opd1.dt_qual = ira_dt_qual_none, .opd2.size = elems_size, .opd3.hss = elems, .opd4.hss = ids };

    push_inst_imm_var0_expr(ctx, expr, &make_dt_stct);

    return true;
}
static bool translate_expr1_dt_arr(ctx_t * ctx, expr_t * expr)
{
    var_t * opd_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var))
    {
        return false;
    }

    ira_dt_t * dim_opd_dt = expr->opd1.expr->val_qdt.dt;

    if (dim_opd_dt->type == IraDtVoid)
    {
        ira_inst_t make_dt_arr = { .type = IraInstMakeDtArr, .opd1.hs = opd_var->inst_name, .opd2.dt_qual = ira_dt_qual_none };

        push_inst_imm_var0_expr(ctx, expr, &make_dt_arr);
    }
    else
    {
        var_t * dim_var;

        if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &dim_var))
        {
            return false;
        }

        ira_inst_t make_dt_vec = { .type = IraInstMakeDtVec, .opd1.hs = opd_var->inst_name, .opd2.hs = dim_var->inst_name, .opd3.dt_qual = ira_dt_qual_none };

        push_inst_imm_var0_expr(ctx, expr, &make_dt_vec);
    }

    return true;
}
static bool translate_expr1_dt_func(ctx_t * ctx, expr_t * expr)
{
    size_t args_size = expr->dt_func.args_size;

    ul_hs_t ** args = malloc(args_size * sizeof(*args));

    ul_assert(args != NULL);

    ul_hs_t ** ids = malloc(args_size * sizeof(*ids));

    ul_assert(ids != NULL);

    ira_dt_func_vas_t * vas = ctx->pec->dt_func_vass.none;

    ul_hs_t ** arg_var_ins = args;
    ul_hs_t ** id_ins = ids;
    for (expr_t * arg = expr->opd1.expr; arg != NULL; arg = arg->opd1.expr, ++arg_var_ins, ++id_ins)
    {
        switch (arg->base->type)
        {
            case PlaExprDtFuncArg:
            {
                var_t * arg_var;

                if (!translate_expr1_imm_var(ctx, arg->opd0.expr, &arg_var))
                {
                    free(args);
                    free(ids);
                    return false;
                }

                *arg_var_ins = arg_var->inst_name;
                *id_ins = arg->opd2.hs;

                break;
            }
            case PlaExprDtFuncVa:
                break;
            default:
                ul_assert_unreachable();
        }
    }

    var_t * ret_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &ret_var))
    {
        free(args);
        free(ids);
        return false;
    }

    ira_inst_t make_dt_func = { .type = IraInstMakeDtFunc, .opd1.hs = ret_var->inst_name, .opd2.size = args_size, .opd3.hss = args, .opd4.hss = ids, .opd5.dt_func_vas = expr->dt_func.vas };

    push_inst_imm_var0_expr(ctx, expr, &make_dt_func);

    return true;
}
static bool translate_expr1_dt_const(ctx_t * ctx, expr_t * expr)
{
    var_t * opd_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var))
    {
        return false;
    }

    ira_inst_t make_dt_const = { .type = IraInstMakeDtConst, .opd1.hs = opd_var->inst_name };

    push_inst_imm_var0_expr(ctx, expr, &make_dt_const);

    return true;
}
static bool translate_expr1_ident(ctx_t * ctx, expr_t * expr)
{
    switch (expr->ident.type)
    {
        case ExprIdentVar:
            expr->val_type = ExprValVar;
            expr->val.var = expr->ident.var;
            break;
        case ExprIdentLo:
            switch (expr->ident.lo->type)
            {
                case IraLoFunc:
                case IraLoImpt:
                {
                    ira_val_t * lo_val;

                    if (!ira_pec_make_val_lo_ptr(ctx->pec, expr->ident.lo, &lo_val))
                    {
                        pla_tltr_report_pec_err(ctx->tltr);
                        return false;
                    }

                    ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = lo_val };

                    push_inst_imm_var0_expr(ctx, expr, &load_val);

                    break;
                }
                case IraLoVar:
                {
                    ira_val_t * lo_val;

                    if (!ira_pec_make_val_lo_ptr(ctx->pec, expr->ident.lo, &lo_val))
                    {
                        pla_tltr_report_pec_err(ctx->tltr);
                        return false;
                    }

                    ira_inst_t load_val = { .type = IraInstLoadVal, .opd1.val = lo_val };

                    var_t * lo_ptr_var;

                    push_inst_imm_var0(ctx, &load_val, load_val.opd1.val->dt, &lo_ptr_var);

                    expr->val_type = ExprValDeref;
                    expr->val.var = lo_ptr_var;

                    break;
                }
                default:
                    ul_assert_unreachable();
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool translate_expr1_call_func_ptr(ctx_t * ctx, expr_t * expr, var_t * callee)
{
    ira_dt_t * func_dt = callee->qdt.dt->ptr.body;

    ul_hs_t ** args = malloc(sizeof(*args) * expr->call.args_size);

    ul_assert(args != NULL);

    memset(args, 0, sizeof(*args) * expr->call.args_size);

    ul_hs_t ** arg_var_ins = args;
    for (expr_call_arg_t *arg = expr->call.args, *arg_end = arg + expr->call.args_size; arg != arg_end; ++arg, ++arg_var_ins)
    {
        var_t * arg_var;

        if (!translate_expr1_imm_var(ctx, arg->val_expr, &arg_var))
        {
            free(args);
            return false;
        }

        if (arg->implct_cast_to != NULL)
        {
            ira_inst_t cast = { .type = IraInstCast, .opd1.hs = arg_var->inst_name, .opd2.dt = arg->implct_cast_to };

            push_inst_imm_var0(ctx, &cast, arg->implct_cast_to, &arg_var);
        }

        *arg_var_ins = arg_var->inst_name;
    }

    ira_inst_t call = { .type = IraInstCallFuncPtr, .opd1.hs = callee->inst_name, .opd2.size = expr->call.args_size, .opd3.hss = args };

    push_inst_imm_var0_expr(ctx, expr, &call);

    return true;
}
static bool translate_expr1_call(ctx_t * ctx, expr_t * expr)
{
    var_t * callee_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &callee_var))
    {
        return false;
    }

    ira_dt_t * callee_dt = callee_var->qdt.dt;

    if (callee_dt->type == IraDtPtr && callee_dt->ptr.body->type == IraDtFunc)
    {
        if (!translate_expr1_call_func_ptr(ctx, expr, callee_var))
        {
            return false;
        }
    }
    else
    {
        ul_assert_unreachable();
    }

    return true;
}
static bool translate_expr1_subscr(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd0 = expr->opd0.expr;
    ira_dt_t * opd0_dt = opd0->val_qdt.dt;

    var_t * ptr_var;

    switch (opd0_dt->type)
    {
        case IraDtVec:
        {
            var_t * opd0_ptr;

            if (!translate_expr1_var_ptr(ctx, opd0, &opd0_ptr))
            {
                return false;
            }

            ira_dt_t * elem_dt;

            if (!ira_pec_apply_qual(ctx->pec, opd0_dt->vec.body, opd0_dt->vec.qual, &elem_dt))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }

            ira_dt_t * ptr_var_dt;

            if (!ira_pec_get_dt_ptr(ctx->pec, elem_dt, opd0_ptr->qdt.dt->ptr.qual, &ptr_var_dt))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }

            ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd0_ptr->inst_name, .opd2.dt = ptr_var_dt };

            push_inst_imm_var0(ctx, &cast, ptr_var_dt, &ptr_var);

            break;
        }
        case IraDtPtr:
            if (!translate_expr1_imm_var(ctx, opd0, &ptr_var))
            {
                return false;
            }
            break;
        case IraDtArr:
        {
            var_t * opd0_ptr;

            if (!translate_expr1_var_ptr(ctx, opd0, &opd0_ptr))
            {
                return false;
            }

            ul_hs_t * mmbr = ctx->pec->pds[IraPdsDataMmbr];
            ira_dt_t * mmbr_dt;

            if (!get_mmbr_acc_dt(ctx, opd0->val_qdt.dt, mmbr, &mmbr_dt))
            {
                ul_assert_unreachable();
            }

            ira_dt_t * ptr_var_dt;

            if (!ira_pec_get_dt_ptr(ctx->pec, mmbr_dt, opd0_ptr->qdt.dt->ptr.qual, &ptr_var_dt))
            {
                pla_tltr_report_pec_err(ctx->tltr);
                return false;
            }

            var_t * mmbr_ptr;

            ira_inst_t mmbr_acc_ptr = { .type = IraInstMmbrAccPtr, .opd1.hs = opd0_ptr->inst_name, .opd2.hs = mmbr };

            push_inst_imm_var0(ctx, &mmbr_acc_ptr, ptr_var_dt, &mmbr_ptr);

            ira_inst_t read_ptr = { .type = IraInstReadPtr, .opd1.hs = mmbr_ptr->inst_name };

            push_inst_imm_var0(ctx, &read_ptr, mmbr_ptr->qdt.dt->ptr.body, &ptr_var);

            break;
        }
        default:
            ul_assert_unreachable();
    }

    var_t * opd1;

    if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1))
    {
        return false;
    }

    var_t * res_var;

    ira_inst_t shift_ptr = { .type = IraInstShiftPtr, .opd1.hs = ptr_var->inst_name, .opd2.hs = opd1->inst_name };

    push_inst_imm_var0(ctx, &shift_ptr, ptr_var->qdt.dt, &res_var);

    expr->val_type = ExprValDeref;
    expr->val.var = res_var;

    return true;
}
static bool translate_expr1_mmbr_acc(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd = expr->opd0.expr;

    var_t * ptr_var;

    if (!translate_expr1_var_ptr(ctx, opd, &ptr_var))
    {
        return false;
    }

    ira_dt_t * ptr_mmbr_dt;

    if (!ira_pec_get_dt_ptr(ctx->pec, expr->mmbr_acc.res_dt, ptr_var->qdt.dt->ptr.qual, &ptr_mmbr_dt))
    {
        pla_tltr_report_pec_err(ctx->tltr);
        return false;
    }

    var_t * ptr_mmbr_var;

    ira_inst_t mmbr_acc_ptr = { .type = IraInstMmbrAccPtr, .opd1.hs = ptr_var->inst_name, .opd2.hs = expr->opd1.hs };

    push_inst_imm_var0(ctx, &mmbr_acc_ptr, ptr_mmbr_dt, &ptr_mmbr_var);

    set_expr_ptr_val(ctx, expr, opd->val_type, ptr_mmbr_var);

    return true;
}
static bool translate_expr1_deref(ctx_t * ctx, expr_t * expr)
{
    var_t * opd_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var))
    {
        return false;
    }

    expr->val_type = ExprValDeref;
    expr->val.var = opd_var;

    return true;
}
static bool translate_expr1_va_start(ctx_t * ctx, expr_t * expr)
{
    ira_inst_t inst = { .type = IraInstVaStart };

    push_inst_imm_var0_expr(ctx, expr, &inst);

    return true;
}
static bool translate_expr1_va_arg(ctx_t * ctx, expr_t * expr)
{
    var_t * va_elem_var;

    if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &va_elem_var))
    {
        return false;
    }

    ira_inst_t inst = { .type = IraInstVaArg, .opd1.hs = va_elem_var->inst_name };

    push_inst_imm_var0_expr(ctx, expr, &inst);

    return true;
}
static bool translate_expr1_addr_of(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd = expr->opd0.expr;

    if (!translate_expr1(ctx, opd))
    {
        return false;
    }

    if (!expr_val_is_left_val[opd->val_type])
    {
        pla_tltr_report(ctx->tltr, "address-of's operand is required to be an lvalue");
        return false;
    }

    switch (opd->val_type)
    {
        case ExprValVar:
        {
            var_t * opd_var = opd->val.var;

            ira_inst_t addr_of = { .type = IraInstAddrOf, .opd1.hs = opd_var->inst_name };

            push_inst_imm_var0_expr(ctx, expr, &addr_of);

            break;
        }
        case ExprValDeref:
        {
            expr->val_type = ExprValImmVar;
            expr->val.var = opd->val.var;
            break;
        }
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool translate_expr1_cast(ctx_t * ctx, expr_t * expr)
{
    var_t * opd_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var))
    {
        return false;
    }

    ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd_var->inst_name, .opd2.dt = expr->val_qdt.dt };

    push_inst_imm_var0_expr(ctx, expr, &cast);

    return true;
}
static bool translate_expr1_unr(ctx_t * ctx, expr_t * expr)
{
    var_t * opd_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd_var))
    {
        return false;
    }

    ira_inst_t optr = { .type = IraInstUnrOptr, .opd1.optr = expr->optr.res.optr, .opd2.hs = opd_var->inst_name };

    push_inst_imm_var0_expr(ctx, expr, &optr);

    return true;
}
static bool translate_expr1_bin(ctx_t * ctx, expr_t * expr)
{
    var_t *opd0_var, *opd1_var;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd0_var))
    {
        return false;
    }

    if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1_var))
    {
        return false;
    }

    if (expr->optr.res.right_implct_cast_to != NULL)
    {
        ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd1_var->inst_name, .opd2.dt = expr->optr.res.right_implct_cast_to };

        push_inst_imm_var0(ctx, &cast, expr->optr.res.right_implct_cast_to, &opd1_var);
    }

    ira_inst_t optr = { .type = IraInstBinOptr, .opd1.optr = expr->optr.res.optr, .opd2.hs = opd0_var->inst_name, .opd3.hs = opd1_var->inst_name };

    push_inst_imm_var0_expr(ctx, expr, &optr);

    return true;
}
static bool translate_expr1_logic_optr(ctx_t * ctx, expr_t * expr)
{
    var_t * opd0;

    if (!translate_expr1_imm_var(ctx, expr->opd0.expr, &opd0))
    {
        return false;
    }

    const ul_ros_t *end_label_base, *end_label_suffix;
    ira_inst_type_t br_type;

    switch (expr->base->type)
    {
        case PlaExprLogicAnd:
            end_label_base = &label_base_logic_and;
            end_label_suffix = &label_suffix_end;
            br_type = IraInstBrf;
            break;
        case PlaExprLogicOr:
            end_label_base = &label_base_logic_or;
            end_label_suffix = &label_suffix_end;
            br_type = IraInstBrt;
            break;
        default:
            ul_assert_unreachable();
    }

    ul_hs_t * end_label = get_unq_label_name(ctx, end_label_base, ctx->unq_label_index++, end_label_suffix);

    ira_inst_t br = { .type = br_type, .opd0.hs = end_label, .opd1.hs = opd0->inst_name };

    ira_func_push_inst(ctx->func, &br);

    var_t * opd1;

    if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1))
    {
        return false;
    }

    ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = opd0->inst_name, .opd1.hs = opd1->inst_name };

    ira_func_push_inst(ctx->func, &copy);

    push_def_label_inst(ctx, end_label);

    expr->val_type = ExprValImmVar;
    expr->val.var = opd0;

    return true;
}
static bool translate_expr1_asgn(ctx_t * ctx, expr_t * expr)
{
    expr_t * opd0 = expr->opd0.expr;
    var_t * opd1_var;

    if (!translate_expr1_imm_var(ctx, expr->opd1.expr, &opd1_var))
    {
        return false;
    }

    if (expr->asgn.implct_cast_to != NULL)
    {
        ira_inst_t cast = { .type = IraInstCast, .opd1.hs = opd1_var->inst_name, .opd2.dt = expr->asgn.implct_cast_to };

        push_inst_imm_var0(ctx, &cast, expr->asgn.implct_cast_to, &opd1_var);
    }

    if (!translate_expr1(ctx, opd0))
    {
        return false;
    }

    if (!expr_val_is_left_val[opd0->val_type])
    {
        pla_tltr_report(ctx->tltr, "assignment's left operand is required to be an lvalue");
        return false;
    }

    switch (opd0->val_type)
    {
        case ExprValVar:
        {
            var_t * opd0_var = opd0->val.var;

            ira_inst_t copy = { .type = IraInstCopy, .opd0.hs = opd0_var->inst_name, .opd1.hs = opd1_var->inst_name };

            ira_func_push_inst(ctx->func, &copy);

            break;
        }
        case ExprValDeref:
        {
            ira_inst_t write_ptr = { .type = IraInstWritePtr, .opd0.hs = opd0->val.var->inst_name, .opd1.hs = opd1_var->inst_name };

            ira_func_push_inst(ctx->func, &write_ptr);

            break;
        }
        default:
            ul_assert_unreachable();
    }

    expr->val_type = opd0->val_type;
    expr->val = opd0->val;

    return true;
}
static bool translate_expr1_tse(ctx_t * ctx, expr_t * expr)
{
    typedef bool translate_expr1_proc_t(ctx_t * ctx, expr_t * expr);

    static translate_expr1_proc_t * const translate_expr1_procs[PlaExpr_Count] = {
        [PlaExprDtVoid] = translate_expr1_load_val,
        [PlaExprDtBool] = translate_expr1_load_val,
        [PlaExprDtInt] = translate_expr1_load_val,
        [PlaExprDtPtr] = translate_expr1_dt_ptr,
        [PlaExprDtTpl] = translate_expr1_dt_tpl,
        [PlaExprDtArr] = translate_expr1_dt_arr,
        [PlaExprDtFunc] = translate_expr1_dt_func,
        [PlaExprDtConst] = translate_expr1_dt_const,
        [PlaExprValVoid] = translate_expr1_load_val,
        [PlaExprValBool] = translate_expr1_load_val,
        [PlaExprChStr] = translate_expr1_load_val,
        [PlaExprNumStr] = translate_expr1_load_val,
        [PlaExprIdent] = translate_expr1_ident,
        [PlaExprCall] = translate_expr1_call,
        [PlaExprSubscr] = translate_expr1_subscr,
        [PlaExprMmbrAcc] = translate_expr1_mmbr_acc,
        [PlaExprDeref] = translate_expr1_deref,
        [PlaExprVaStart] = translate_expr1_va_start,
        [PlaExprVaArg] = translate_expr1_va_arg,
        [PlaExprAddrOf] = translate_expr1_addr_of,
        [PlaExprCast] = translate_expr1_cast,
        [PlaExprLogicNeg] = translate_expr1_unr,
        [PlaExprBitNeg] = translate_expr1_unr,
        [PlaExprArithNeg] = translate_expr1_unr,
        [PlaExprMul] = translate_expr1_bin,
        [PlaExprDiv] = translate_expr1_bin,
        [PlaExprMod] = translate_expr1_bin,
        [PlaExprAdd] = translate_expr1_bin,
        [PlaExprSub] = translate_expr1_bin,
        [PlaExprLeShift] = translate_expr1_bin,
        [PlaExprRiShift] = translate_expr1_bin,
        [PlaExprLess] = translate_expr1_bin,
        [PlaExprLessEq] = translate_expr1_bin,
        [PlaExprGrtr] = translate_expr1_bin,
        [PlaExprGrtrEq] = translate_expr1_bin,
        [PlaExprEq] = translate_expr1_bin,
        [PlaExprNeq] = translate_expr1_bin,
        [PlaExprBitAnd] = translate_expr1_bin,
        [PlaExprBitXor] = translate_expr1_bin,
        [PlaExprBitOr] = translate_expr1_bin,
        [PlaExprLogicAnd] = translate_expr1_logic_optr,
        [PlaExprLogicOr] = translate_expr1_logic_optr,
        [PlaExprAsgn] = translate_expr1_asgn,
    };

    translate_expr1_proc_t * proc = translate_expr1_procs[expr->base->type];

    ul_assert(proc != NULL);

    if (!proc(ctx, expr))
    {
        return false;
    }

    return true;
}
static bool translate_expr1(ctx_t * ctx, expr_t * expr)
{
    tse_t tse = { .type = PlaTltrTseExpr, .expr = expr->base };

    pla_tltr_push_tse(ctx->tltr, &tse);

    bool res = translate_expr1_tse(ctx, expr);

    pla_tltr_pop_tse(ctx->tltr, &tse);

    return res;
}


static bool translate_expr_guard(ctx_t * ctx, pla_expr_t * pla_expr, expr_t ** expr, var_t ** out)
{
    if (!translate_expr0(ctx, pla_expr, expr))
    {
        return false;
    }

    if (out != NULL)
    {
        if (!translate_expr1_imm_var(ctx, *expr, out))
        {
            return false;
        }
    }
    else
    {
        if (!translate_expr1(ctx, *expr))
        {
            return false;
        }
    }

    return true;
}
static bool translate_expr(ctx_t * ctx, pla_expr_t * pla_expr, var_t ** out)
{
    expr_t * expr = NULL;

    bool res = translate_expr_guard(ctx, pla_expr, &expr, out);

    destroy_expr(ctx, expr);

    return res;
}

static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt);

static bool translate_stmt_blk_vvb(ctx_t * ctx, pla_stmt_t * stmt)
{
    for (pla_stmt_t * it = stmt->blk.body; it != NULL; it = it->next)
    {
        if (!translate_stmt(ctx, it))
        {
            return false;
        }
    }

    return true;
}
static bool translate_stmt_blk(ctx_t * ctx, pla_stmt_t * stmt)
{
    vvb_t vvb = { 0 };

    push_vvb(ctx, &vvb);

    bool res = translate_stmt_blk_vvb(ctx, stmt);

    pop_vvb(ctx);

    return res;
}
static bool translate_stmt_expr(ctx_t * ctx, pla_stmt_t * stmt)
{
    if (!translate_expr(ctx, stmt->expr.expr, NULL))
    {
        return false;
    }

    return true;
}
static bool translate_stmt_var_dt(ctx_t * ctx, pla_stmt_t * stmt)
{
    ira_dt_t * dt;

    if (!pla_tltr_calculate_expr_dt(ctx->tltr, stmt->var_dt.dt_expr, &dt))
    {
        return false;
    }

    if (!ira_pec_is_dt_complete(dt))
    {
        pla_tltr_report(ctx->tltr, "variables must have only complete data type");
        return false;
    }

    var_t * var = define_var(ctx, stmt->var_dt.name, dt, stmt->var_dt.dt_qual, true);

    if (var == NULL)
    {
        pla_tltr_report(ctx->tltr, "there is already variable of same name [%s] in this block", stmt->var_dt.name->str);
        return false;
    }

    push_def_var_inst(ctx, var);

    return true;
}
static bool translate_stmt_var_val(ctx_t * ctx, pla_stmt_t * stmt)
{
    var_t * val_var;

    if (!translate_expr(ctx, stmt->var_val.val_expr, &val_var))
    {
        return false;
    }

    var_t * var = define_var(ctx, stmt->var_val.name, val_var->qdt.dt, stmt->var_val.dt_qual, true);

    if (var == NULL)
    {
        pla_tltr_report(ctx->tltr, "there is already variable of same name [%s] in this block", stmt->var_val.name->str);
        return false;
    }

    ira_inst_t def_var_copy = { .type = IraInstDefVarCopy, .opd0.hs = var->inst_name, .opd1.hs = val_var->inst_name, .opd2.dt_qual = var->qdt.qual };

    ira_func_push_inst(ctx->func, &def_var_copy);

    return true;
}
static bool translate_stmt_cond(ctx_t * ctx, pla_stmt_t * stmt)
{
    var_t * cond_var;

    if (!translate_expr(ctx, stmt->cond.cond_expr, &cond_var))
    {
        return false;
    }

    if (cond_var->qdt.dt != &ctx->pec->dt_bool)
    {
        pla_tltr_report(ctx->tltr, "condition expression mush have a boolean data type");
        return false;
    }

    size_t label_index = ctx->unq_label_index++;

    ul_hs_t * exit_label;

    {
        ul_hs_t * tc_end = get_unq_label_name(ctx, &label_base_cond, label_index, &label_suffix_tc_end);

        ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = tc_end, .opd1.hs = cond_var->inst_name };

        ira_func_push_inst(ctx->func, &brf);

        if (!translate_stmt(ctx, stmt->cond.true_br))
        {
            return false;
        }

        exit_label = tc_end;
    }

    if (stmt->cond.false_br != false)
    {
        ul_hs_t * fc_end = get_unq_label_name(ctx, &label_base_cond, label_index, &label_suffix_fc_end);

        ira_inst_t bru = { .type = IraInstBru, .opd0.hs = fc_end };

        ira_func_push_inst(ctx->func, &bru);

        push_def_label_inst(ctx, exit_label);

        exit_label = fc_end;

        if (!translate_stmt(ctx, stmt->cond.false_br))
        {
            return false;
        }
    }

    push_def_label_inst(ctx, exit_label);

    return true;
}
static bool translate_stmt_pre_loop_cfb(ctx_t * ctx, pla_stmt_t * stmt, cfb_t * cfb)
{
    size_t label_index = ctx->unq_label_index++;

    cfb->exit_label = get_unq_label_name(ctx, &label_base_pre_loop, label_index, &label_suffix_exit);
    cfb->cnt_label = get_unq_label_name(ctx, &label_base_pre_loop, label_index, &label_suffix_cnt);

    push_def_label_inst(ctx, cfb->cnt_label);

    var_t * cond_var;

    if (!translate_expr(ctx, stmt->pre_loop.cond_expr, &cond_var))
    {
        return false;
    }

    {
        ira_inst_t brf = { .type = IraInstBrf, .opd0.hs = cfb->exit_label, .opd1.hs = cond_var->inst_name };

        ira_func_push_inst(ctx->func, &brf);
    }

    if (!translate_stmt(ctx, stmt->pre_loop.body))
    {
        return false;
    }

    {
        ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cfb->cnt_label };

        ira_func_push_inst(ctx->func, &bru);
    }

    push_def_label_inst(ctx, cfb->exit_label);

    return true;
}
static bool translate_stmt_pre_loop(ctx_t * ctx, pla_stmt_t * stmt)
{
    cfb_t cfb = { .name = stmt->pre_loop.name };

    push_cfb(ctx, &cfb);

    bool res = translate_stmt_pre_loop_cfb(ctx, stmt, &cfb);

    pop_cfb(ctx);

    return res;
}
static bool translate_stmt_post_loop_cfb(ctx_t * ctx, pla_stmt_t * stmt, cfb_t * cfb)
{
    size_t label_index = ctx->unq_label_index++;

    cfb->exit_label = get_unq_label_name(ctx, &label_base_post_loop, label_index, &label_suffix_exit);
    cfb->cnt_label = get_unq_label_name(ctx, &label_base_post_loop, label_index, &label_suffix_cnt);
    ul_hs_t * body_label = get_unq_label_name(ctx, &label_base_post_loop, label_index, &label_suffix_body);

    push_def_label_inst(ctx, body_label);

    if (!translate_stmt(ctx, stmt->post_loop.body))
    {
        return false;
    }

    push_def_label_inst(ctx, cfb->cnt_label);

    var_t * cond_var;

    if (!translate_expr(ctx, stmt->pre_loop.cond_expr, &cond_var))
    {
        return false;
    }

    {
        ira_inst_t brt = { .type = IraInstBrt, .opd0.hs = body_label, .opd1.hs = cond_var->inst_name };

        ira_func_push_inst(ctx->func, &brt);
    }

    push_def_label_inst(ctx, cfb->exit_label);

    return true;
}
static bool translate_stmt_post_loop(ctx_t * ctx, pla_stmt_t * stmt)
{
    cfb_t cfb = { .name = stmt->post_loop.name };

    push_cfb(ctx, &cfb);

    bool res = translate_stmt_post_loop_cfb(ctx, stmt, &cfb);

    pop_cfb(ctx);

    return res;
}
static bool translate_stmt_brk(ctx_t * ctx, pla_stmt_t * stmt)
{
    ul_hs_t * cfb_name = stmt->brk.name;
    cfb_t * cfb = ctx->cfb;

    for (; cfb != NULL; cfb = cfb->prev)
    {
        if (cfb_name != NULL && cfb->name != cfb_name)
        {
            continue;
        }

        if (cfb->exit_label != NULL)
        {
            break;
        }
    }

    if (cfb == NULL)
    {
        pla_tltr_report(ctx->tltr, "failed to find target for break statement");
        return false;
    }

    {
        ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cfb->exit_label };

        ira_func_push_inst(ctx->func, &bru);
    }

    return true;
}
static bool translate_stmt_cnt(ctx_t * ctx, pla_stmt_t * stmt)
{
    ul_hs_t * cfb_name = stmt->brk.name;
    cfb_t * cfb = ctx->cfb;

    for (; cfb != NULL; cfb = cfb->prev)
    {
        if (cfb_name != NULL && cfb->name != cfb_name)
        {
            continue;
        }

        if (cfb->cnt_label != NULL)
        {
            break;
        }
    }

    if (cfb == NULL)
    {
        pla_tltr_report(ctx->tltr, "failed to find target for continue statement");
        return false;
    }

    {
        ira_inst_t bru = { .type = IraInstBru, .opd0.hs = cfb->cnt_label };

        ira_func_push_inst(ctx->func, &bru);
    }

    return true;
}
static bool translate_stmt_ret(ctx_t * ctx, pla_stmt_t * stmt)
{
    var_t * ret_var;

    if (!translate_expr(ctx, stmt->expr.expr, &ret_var))
    {
        return false;
    }

    if (ctx->func_ret != NULL)
    {
        if (!ira_dt_is_equivalent(ctx->func_ret, ret_var->qdt.dt))
        {
            pla_tltr_report(ctx->tltr, "return data type does not match to function return data type");
            return false;
        }
    }
    else
    {
        ctx->func_ret = ret_var->qdt.dt;
    }

    ira_inst_t ret = { .type = IraInstRet, .opd0.hs = ret_var->inst_name };

    ira_func_push_inst(ctx->func, &ret);

    return true;
}
static bool translate_stmt_tse(ctx_t * ctx, pla_stmt_t * stmt)
{
    switch (stmt->type)
    {
        case PlaStmtNone:
            break;
        case PlaStmtBlk:
            if (!translate_stmt_blk(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtExpr:
            if (!translate_stmt_expr(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtVarDt:
            if (!translate_stmt_var_dt(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtVarVal:
            if (!translate_stmt_var_val(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtCond:
            if (!translate_stmt_cond(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtPreLoop:
            if (!translate_stmt_pre_loop(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtPostLoop:
            if (!translate_stmt_post_loop(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtBrk:
            if (!translate_stmt_brk(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtCnt:
            if (!translate_stmt_cnt(ctx, stmt))
            {
                return false;
            }
            break;
        case PlaStmtRet:
            if (!translate_stmt_ret(ctx, stmt))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool translate_stmt(ctx_t * ctx, pla_stmt_t * stmt)
{
    tse_t tse = { .type = PlaTltrTseStmt, .stmt = stmt };

    pla_tltr_push_tse(ctx->tltr, &tse);

    bool res = translate_stmt_tse(ctx, stmt);

    pla_tltr_pop_tse(ctx->tltr, &tse);

    return res;
}


static bool translate_args_vvb(ctx_t * ctx)
{
    if (ctx->func_dt != NULL)
    {
        ctx->func_ret = ctx->func_dt->func.ret;

        for (ira_dt_ndt_t *arg = ctx->func_dt->func.args, *arg_end = arg + ctx->func_dt->func.args_size; arg != arg_end; ++arg)
        {
            var_t * var = define_var(ctx, arg->name, arg->dt, ira_dt_qual_none, false);

            ul_assert(var != NULL);
        }
    }

    if (!translate_stmt(ctx, ctx->stmt))
    {
        return false;
    }

    if (ctx->func->dt == NULL)
    {
        if (ctx->func_ret == NULL)
        {
            return false;
        }

        if (!ira_pec_get_dt_func(ctx->pec, ctx->func_ret, 0, NULL, ctx->pec->dt_func_vass.none, &ctx->func->dt))
        {
            return false;
        }
    }

    return true;
}
static bool translate_args(ctx_t * ctx)
{
    vvb_t vvb = { 0 };

    push_vvb(ctx, &vvb);

    bool res = translate_args_vvb(ctx);

    pop_vvb(ctx);

    return res;
}

static bool translate_core(ctx_t * ctx)
{
    if (ctx->func_dt != NULL && ctx->func_dt->type != IraDtFunc)
    {
        return false;
    }

    if (ctx->stmt->type != PlaStmtBlk)
    {
        return false;
    }

    ctx->hsb = &ctx->tltr->hsb;
    ctx->hst = ctx->tltr->hst;
    ctx->pec = ctx->tltr->out;

    *ctx->out = ira_func_create(ctx->func_dt);

    ctx->func = *ctx->out;

    if (!translate_args(ctx))
    {
        return false;
    }

    return true;
}

bool pla_tltr_s_translate(pla_tltr_t * tltr, ira_dt_t * func_dt, pla_stmt_t * stmt, ira_func_t ** out)
{
    ctx_t ctx = { .tltr = tltr, .func_dt = func_dt, .stmt = stmt, .out = out };

    bool res = translate_core(&ctx);

    return res;
}
