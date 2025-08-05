#include "mc_frag.h"
#include "mc_defs.h"
#include "mc_inst.h"

#define MOD_NAME "mc_frag"

typedef struct mc_frag_ctx
{
    mc_frag_t * frag;
    ul_ec_fmtr_t * ec_fmtr;
    lnk_sect_t ** out;

    bool is_rptd;
} ctx_t;

static void report(ctx_t * ctx, const char * fmt, ...)
{
    ctx->is_rptd = true;

    if (ctx->ec_fmtr == NULL)
    {
        return;
    }

    {
        va_list args;

        va_start(args, fmt);

        ul_ec_fmtr_format_va(ctx->ec_fmtr, fmt, args);

        va_end(args);
    }

    ul_ec_fmtr_post(ctx->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
}

mc_frag_t * mc_frag_create(mc_frag_type_t type)
{
    mc_frag_t * frag = malloc(sizeof(*frag));

    ul_assert(frag != NULL);

    *frag = (mc_frag_t){ .type = type };

    return frag;
}
void mc_frag_destroy(mc_frag_t * frag)
{
    if (frag == NULL)
    {
        return;
    }

    free(frag->insts);
    free(frag);
}

void mc_frag_destroy_chain(mc_frag_t * frag)
{
    while (frag != NULL)
    {
        mc_frag_t * next = frag->next;

        mc_frag_destroy(frag);

        frag = next;
    }
}

void mc_frag_push_inst(mc_frag_t * frag, const mc_inst_t * inst)
{
    ul_arr_grow(frag->insts_size + 1, &frag->insts_cap, (void **)&frag->insts, sizeof(*frag->insts));

    frag->insts[frag->insts_size++] = *inst;
}

static void create_sect(ctx_t * ctx)
{
    mc_frag_t * frag = ctx->frag;
    lnk_sect_t ** out = ctx->out;

    mc_defs_sd_type_t sd_type = McDefsSdInv_Inv;

    if (frag->type >= McFrag_Count)
    {
        report(ctx, "invalid fragment type");
    }
    else
    {
        sd_type = mc_frag_type_to_sd[frag->type];
    }

    const lnk_sect_desc_t * desc = &mc_defs_sds[sd_type];

    *out = lnk_sect_create_desc(desc);

    (*out)->data_align = frag->align == 0 ? desc->data_align : frag->align;
}

static bool get_imm0_fixup_stype(mc_inst_t * inst, lnk_sect_lp_stype_t * out)
{
    *out = LnkSectLpFixupNone;

    switch (inst->opds)
    {
        case McInstOpds_None:
        case McInstOpds_Label:
        case McInstOpds_Reg:
        case McInstOpds_Mem:
        case McInstOpds_Reg_Reg:
        case McInstOpds_Reg_Mem:
        case McInstOpds_Mem_Reg:
            return true;
        case McInstOpds_Imm:
        case McInstOpds_Reg_Imm:
        case McInstOpds_Mem_Imm:
            break;
        default:
            return false;
    }

    switch (inst->imm0_type)
    {
        case McInstImmNone:
        case McInstImm8:
        case McInstImm16:
        case McInstImm32:
        case McInstImm64:
            return true;
        case McInstImmLabelRel8:
            *out = LnkSectLpFixupRel8;
            break;
        case McInstImmLabelRel32:
            *out = LnkSectLpFixupRel32;
            break;
        case McInstImmLabelVa64:
            *out = LnkSectLpFixupVa64;
            break;
        case McInstImmLabelRva32:
            *out = LnkSectLpFixupRva32;
            break;
        case McInstImmLabelRva31of64:
            *out = LnkSectLpFixupRva31of64;
            break;
        default:
            return false;
    }

    return true;
}
static bool get_disp_fixup_stype(mc_inst_t * inst, lnk_sect_lp_stype_t * out)
{
    *out = LnkSectLpFixupNone;

    switch (inst->opds)
    {
        case McInstOpds_None:
        case McInstOpds_Label:
        case McInstOpds_Imm:
        case McInstOpds_Reg:
        case McInstOpds_Reg_Imm:
        case McInstOpds_Reg_Reg:
            return true;
        case McInstOpds_Mem:
        case McInstOpds_Mem_Imm:
        case McInstOpds_Reg_Mem:
        case McInstOpds_Mem_Reg:
            break;
        default:
            return false;
    }

    switch (inst->mem_disp_type)
    {
        case McInstDispAuto:
        case McInstDispNone:
        case McInstDisp8:
        case McInstDisp32:
            return true;
        case McInstDispLabelRel8:
            *out = LnkSectLpFixupRel8;
            break;
        case McInstDispLabelRel32:
            *out = LnkSectLpFixupRel32;
            break;
        case McInstDispLabelRva32:
            *out = LnkSectLpFixupRva32;
            break;
        default:
            return false;
    }

    return true;
}
static void build_insts(ctx_t * ctx)
{
    mc_frag_t * frag = ctx->frag;
    lnk_sect_t * sect = *ctx->out;

    for (mc_inst_t *inst = frag->insts, *inst_end = inst + frag->insts_size; inst != inst_end; ++inst)
    {
        switch (inst->type)
        {
            case McInstLabel:
                if (inst->opds != McInstOpds_Label)
                {
                    report(ctx, "invalid label-instruction operands");
                }

                lnk_sect_add_lp(sect, LnkSectLpLabel, inst->label_stype, inst->label, sect->data_size);

                break;
            case McInstAlign:
                if (inst->opds != McInstOpds_Imm || inst->imm0_type != McInstImm64)
                {
                    report(ctx, "invalid align-instruction operands");
                }
                else
                {
                    size_t cur_size = sect->data_size;

                    size_t aligned_size = ul_align_to(cur_size, (size_t)inst->imm0);

                    ul_arr_grow(aligned_size, &sect->data_cap, (void **)&sect->data, sizeof(*sect->data));

                    memset(sect->data + sect->data_size, sect->data_align_byte, aligned_size - cur_size);

                    sect->data_size = aligned_size;
                }

                break;
            default:
            {
                size_t inst_size;
                mc_inst_bs_t inst_bs;
                mc_inst_offs_t inst_offs;

                if (mc_inst_build(inst, ctx->ec_fmtr, &inst_size, &inst_bs, &inst_offs))
                {
                    ul_arr_grow(sect->data_size + inst_size, &sect->data_cap, (void **)&sect->data, sizeof(*sect->data));

                    size_t inst_start = sect->data_size;

                    memcpy(sect->data + sect->data_size, inst_bs.mem, inst_size);
                    sect->data_size += inst_size;

                    lnk_sect_lp_stype_t stype;

                    if (!get_imm0_fixup_stype(inst, &stype))
                    {
                        report(ctx, "invalid 0th immediate's type");
                    }
                    else if (stype != LnkSectLpFixupNone)
                    {
                        lnk_sect_add_lp(sect, LnkSectLpFixup, stype, inst->imm0_label, inst_start + inst_offs.off[McInstOffImm]);
                    }

                    if (!get_disp_fixup_stype(inst, &stype))
                    {
                        report(ctx, "invalid displacement's type");
                    }
                    else if (stype != LnkSectLpFixupNone)
                    {
                        lnk_sect_add_lp(sect, LnkSectLpFixup, stype, inst->mem_disp_label, inst_start + inst_offs.off[McInstOffDisp]);
                    }
                }

                break;
            }
        }
    }
}

static void build_core(ctx_t * ctx)
{
    create_sect(ctx);

    build_insts(ctx);
}
bool mc_frag_build(mc_frag_t * frag, ul_ec_fmtr_t * ec_fmtr, lnk_sect_t ** out)
{
    ctx_t ctx = { .frag = frag, .ec_fmtr = ec_fmtr, .out = out };

    build_core(&ctx);

    return !ctx.is_rptd;
}

const mc_defs_sd_type_t mc_frag_type_to_sd[McFrag_Count] = {
    [McFragCode] = McDefsSdText_Code,
    [McFragRoData] = McDefsSdRdata_Data,
    [McFragRwData] = McDefsSdData_Data,
    [McFragUnw] = McDefsSdRdata_Unw
};
