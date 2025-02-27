#include "lnk_pel_m.h"
#include "lnk_sect.h"

#define MOD_NAME "lnk_pel_m"

typedef struct lnk_pel_m_sect sect_t;
struct lnk_pel_m_sect
{
    lnk_sect_t * base;

    size_t fxd_sect_ord_ind;

    size_t input_ind;

    size_t ref_ord_ind;
};

typedef struct lnk_pel_m_label
{
    const ul_hs_t * name;
    sect_t * sect;
} label_t;

typedef struct lnk_pel_m_sect_ord_data
{
    sect_t * sect;
    size_t new_ord_ind;
} sect_ord_data_t;

typedef struct lnk_pel_m_ctx
{
    lnk_pel_t * pel;
    lnk_sect_t ** out;

    bool is_rptd;

    size_t sects_size;
    sect_t ** sects;
    size_t sects_cap;

    size_t ord_buf_size;
    sect_ord_data_t * ord_buf;
    size_t ord_buf_cap;

    size_t labels_size;
    label_t * labels;
    size_t labels_cap;

    lnk_sect_t * mrgd_sect;
    lnk_sect_t ** mrgd_sect_ins;
} ctx_t;


static const char * const fxd_sect_ord[] = {
    ".text",
    ".rdata",
    ".data",
    ".reloc"
};
static const size_t fxd_sect_ord_size = ul_arr_count(fxd_sect_ord);


static size_t get_fxd_sect_ord_ind(const char * name)
{
    for (size_t i = 0; i < fxd_sect_ord_size; ++i)
    {
        if (strcmp(fxd_sect_ord[i], name) == 0)
        {
            return i;
        }
    }

    return fxd_sect_ord_size;
}


static sect_t * create_sect(lnk_sect_t * base, size_t input_ind, size_t ref_ord_ind)
{
    sect_t * sect = malloc(sizeof(*sect));

    ul_assert(sect != NULL);

    *sect = (sect_t){ .base = base, .fxd_sect_ord_ind = get_fxd_sect_ord_ind(base->name), .input_ind = input_ind, .ref_ord_ind = ref_ord_ind };

    return sect;
}
static void destroy_sect(sect_t * sect)
{
    if (sect == NULL)
    {
        return;
    }

    free(sect);
}


static bool label_cmp_proc(const void * first_ptr, const void * second_ptr)
{
    const label_t * first = first_ptr;
    const label_t * second = second_ptr;

    return (uint8_t *)first->name < (uint8_t *)second->name;
}
static label_t * find_label(ctx_t * ctx, ul_hs_t * name)
{
    label_t pred = { .name = name };

    size_t label_ind = ul_bs_lower_bound(sizeof(*ctx->labels), ctx->labels_size, ctx->labels, label_cmp_proc, &pred);

    if (label_ind < ctx->labels_size && ctx->labels[label_ind].name == name)
    {
        return &ctx->labels[label_ind];
    }

    return NULL;
}


static void report(ctx_t * ctx, const char * fmt, ...)
{
    ctx->is_rptd = true;

    if (ctx->pel->ec_fmtr == NULL)
    {
        return;
    }

    {
        va_list args;

        va_start(args, fmt);

        ul_ec_fmtr_format_va(ctx->pel->ec_fmtr, fmt, args);

        va_end(args);
    }

    ul_ec_fmtr_post(ctx->pel->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
}

static void process_sect_lp(ctx_t * ctx, sect_t * sect, lnk_sect_lp_t * lp)
{
    switch (lp->type)
    {
        case LnkSectLpNone:
        case LnkSectLpMark:
            break;
        case LnkSectLpLabel:
            switch (lp->stype)
            {
                case LnkSectLpLabelNone:
                    break;
                case LnkSectLpLabelBasic:
                {
                    ul_arr_grow(ctx->labels_size + 1, &ctx->labels_cap, (void **)&ctx->labels, sizeof(*ctx->labels));

                    label_t new_label = (label_t){ .name = lp->label_name, .sect = sect };

                    size_t ins_pos = ul_bs_lower_bound(sizeof(*ctx->labels), ctx->labels_size, ctx->labels, label_cmp_proc, &new_label);

                    if (ins_pos < ctx->labels_size && ctx->labels[ins_pos].name == new_label.name)
                    {
                        report(ctx, "invalid label: non-unique name [%s]", new_label.name->str);
                    }
                    else
                    {
                        memmove(ctx->labels + ins_pos + 1, ctx->labels + ins_pos, sizeof(*ctx->labels) * (ctx->labels_size - ins_pos));

                        ctx->labels[ins_pos] = new_label;
                        ++ctx->labels_size;
                    }
                    break;
                }
                case LnkSectLpLabelProcEnd:
                case LnkSectLpLabelProcUnw:
                    break;
                default:
                    report(ctx, "invalid label subtype");
                    break;
            }

            break;
        case LnkSectLpFixup:
            break;
        default:
            report(ctx, "invalid link point type");
            break;
    }
}
static void form_sects_arr(ctx_t * ctx)
{
    size_t input_ind = 0;

    for (lnk_sect_t * lnk_sect = ctx->pel->sect; lnk_sect != NULL; lnk_sect = lnk_sect->next)
    {
        ul_arr_grow(ctx->sects_size + 1, &ctx->sects_cap, (void **)&ctx->sects, sizeof(*ctx->sects));

        sect_t * sect = create_sect(lnk_sect, input_ind++, SIZE_MAX);

        ctx->sects[ctx->sects_size++] = sect;

        for (lnk_sect_lp_t *lp = lnk_sect->lps, *lp_end = lp + lnk_sect->lps_size; lp != lp_end; ++lp)
        {
            process_sect_lp(ctx, sect, lp);
        }
    }
}

static void push_ord_buf_sect(ctx_t * ctx, sect_t * sect, size_t ref_ord_ind)
{
    if (sect->ref_ord_ind <= ref_ord_ind)
    {
        return;
    }

    for (sect_ord_data_t *data = ctx->ord_buf, *data_end = data + ctx->ord_buf_size; data != data_end; ++data)
    {
        if (data->sect == sect)
        {
            data->new_ord_ind = ul_min(data->new_ord_ind, ref_ord_ind);

            return;
        }
    }

    ul_arr_grow(ctx->ord_buf_size + 1, &ctx->ord_buf_cap, (void **)&ctx->ord_buf, sizeof(*ctx->ord_buf));

    ctx->ord_buf[ctx->ord_buf_size++] = (sect_ord_data_t){ .sect = sect, .new_ord_ind = ref_ord_ind };
}
static void push_ord_buf_refs(ctx_t * ctx, sect_t * sect)
{
    size_t trg_ord_ind = sect->ref_ord_ind + 1;

    lnk_sect_t * base = sect->base;

    for (lnk_sect_lp_t *lp = base->lps, *lp_end = lp + base->lps_size; lp != lp_end; ++lp)
    {
        switch (lp->type)
        {
            case LnkSectLpNone:
            case LnkSectLpMark:
            case LnkSectLpLabel:
                break;
            case LnkSectLpFixup:
                switch (lp->stype)
                {
                    case LnkSectLpFixupNone:
                    case LnkSectLpFixupRel8:
                    case LnkSectLpFixupRel32:
                    case LnkSectLpFixupVa64:
                    case LnkSectLpFixupRva32:
                    case LnkSectLpFixupRva31of64:
                    {
                        label_t * label = find_label(ctx, lp->label_name);

                        if (label == NULL)
                        {
                            report(ctx, "invalid fixup: label [%s] not found", lp->label_name->str);
                        }
                        else
                        {
                            if (label->sect != sect)
                            {
                                push_ord_buf_sect(ctx, label->sect, trg_ord_ind);
                            }
                        }
                        break;
                    }
                    default:
                        report(ctx, "invalid fixup subtype");
                        break;
                }
                break;
        }
    }
}
static bool set_ord_inds(ctx_t * ctx)
{
    {
        label_t * ep_label = find_label(ctx, ctx->pel->ep_name);

        if (ep_label == NULL)
        {
            report(ctx, "invalid entry point: label [%s] not found", ctx->pel->ep_name->str);
            return false;
        }

        push_ord_buf_sect(ctx, ep_label->sect, 0);
    }

    while (ctx->ord_buf_size > 0)
    {
        sect_ord_data_t data = ctx->ord_buf[0];

        memmove(ctx->ord_buf, ctx->ord_buf + 1, sizeof(*ctx->ord_buf) * (ctx->ord_buf_size - 1));

        --ctx->ord_buf_size;

        if (data.sect->ref_ord_ind <= data.new_ord_ind)
        {
            continue;
        }

        data.sect->ref_ord_ind = data.new_ord_ind;

        push_ord_buf_refs(ctx, data.sect);
    }

    return true;
}

static int sect_cmp_proc(const void * first_ptr, const void * second_ptr)
{
    const sect_t * first = *(const sect_t * const *)first_ptr;
    const sect_t * second = *(const sect_t * const *)second_ptr;

    if (first->fxd_sect_ord_ind < second->fxd_sect_ord_ind)
    {
        return -1;
    }
    else if (first->fxd_sect_ord_ind > second->fxd_sect_ord_ind)
    {
        return 1;
    }

    if (first->fxd_sect_ord_ind == fxd_sect_ord_size)
    {
        int name_cmp = strcmp(first->base->name, second->base->name);

        if (name_cmp != 0)
        {
            return name_cmp;
        }
    }

    if (first->base->ord_ind < second->base->ord_ind)
    {
        return -1;
    }
    else if (first->base->ord_ind > second->base->ord_ind)
    {
        return 1;
    }

    if (first->ref_ord_ind < second->ref_ord_ind)
    {
        return -1;
    }
    else if (first->ref_ord_ind > second->ref_ord_ind)
    {
        return 1;
    }

    if (first->input_ind < second->input_ind)
    {
        return -1;
    }
    else if (first->input_ind > second->input_ind)
    {
        return 1;
    }

    return 0;
}
static void sort_sects(ctx_t * ctx)
{
    qsort(ctx->sects, ctx->sects_size, sizeof(*ctx->sects), sect_cmp_proc);
}

static void merge_sects(ctx_t * ctx)
{
    ctx->mrgd_sect_ins = &ctx->mrgd_sect;

    for (sect_t **sect = ctx->sects, **sect_end = sect + ctx->sects_size; sect != sect_end;)
    {
        lnk_sect_t * sect_base = (*sect)->base;
        sect_t ** batch = sect + 1;

        while (batch != sect_end
               && strcmp(sect_base->name, (*batch)->base->name) == 0)
        {
            lnk_sect_t * batch_base = (*batch)->base;

            if (!(ul_is_pwr_of_2(batch_base->data_align) || batch_base->data_align == 0)
                || batch_base->mem_r != sect_base->mem_r
                || batch_base->mem_w != sect_base->mem_w
                || batch_base->mem_e != sect_base->mem_e
                || batch_base->mem_disc != sect_base->mem_disc)
            {
                report(ctx, "invalid sections: [%s] sections have different description data", sect_base->name);
            }

            ++batch;
        }

        lnk_sect_t * mrgd_sect = lnk_sect_create(sect_base->name);

        *ctx->mrgd_sect_ins = mrgd_sect;
        ctx->mrgd_sect_ins = &mrgd_sect->next;

        size_t data_align = 0;

        for (sect_t ** cur = sect; cur != batch; ++cur)
        {
            lnk_sect_t * cur_base = (*cur)->base;

            data_align = ul_max(data_align, cur_base->data_align);

            size_t cur_start = ul_align_to(mrgd_sect->data_size, cur_base->data_align);

            ul_arr_grow(cur_start + cur_base->data_size, &mrgd_sect->data_cap, (void **)&mrgd_sect->data, sizeof(*mrgd_sect->data));

            memset(mrgd_sect->data + mrgd_sect->data_size, cur_base->data_align_byte, cur_start - mrgd_sect->data_size);
            memcpy(mrgd_sect->data + cur_start, cur_base->data, sizeof(*cur_base->data) * cur_base->data_size);

            for (lnk_sect_lp_t *lp = cur_base->lps, *lp_end = lp + cur_base->lps_size; lp != lp_end; ++lp)
            {
                lnk_sect_add_lp(mrgd_sect, lp->type, lp->stype, lp->label_name, lp->off + cur_start);
            }

            mrgd_sect->data_size = cur_start + cur_base->data_size;
        }

        mrgd_sect->data_align = data_align;

        mrgd_sect->data_align_byte = sect_base->data_align_byte;

        mrgd_sect->mem_r = sect_base->mem_r;
        mrgd_sect->mem_w = sect_base->mem_w;
        mrgd_sect->mem_e = sect_base->mem_e;
        mrgd_sect->mem_disc = sect_base->mem_disc;

        sect = batch;
    }
}

static void assign_sects(ctx_t * ctx)
{
    *ctx->out = ctx->mrgd_sect;

    ctx->mrgd_sect = NULL;
}


static bool merge_core(ctx_t * ctx)
{
    form_sects_arr(ctx);

    if (!set_ord_inds(ctx))
    {
        return false;
    }

    sort_sects(ctx);

    merge_sects(ctx);

    assign_sects(ctx);

    return true;
}
bool lnk_pel_m_merge(lnk_pel_t * pel, lnk_sect_t ** out)
{
    ctx_t ctx = { .pel = pel, .out = out };

    bool res = merge_core(&ctx);

    for (sect_t **sect = ctx.sects, **sect_end = sect + ctx.sects_size; sect != sect_end; ++sect)
    {
        destroy_sect(*sect);
    }

    lnk_sect_destroy_chain(ctx.mrgd_sect);

    free(ctx.ord_buf);

    free(ctx.labels);

    free(ctx.sects);

    return res && !ctx.is_rptd;
}
