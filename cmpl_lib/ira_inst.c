#include "ira_inst.h"
#include "ira_val.h"

void ira_inst_copy(ira_inst_t * dst, const ira_inst_t * src)
{
    ul_assert(src->type < IraInst_Count);

    const ira_inst_info_t * info = &ira_inst_infos[src->type];

    for (size_t opd = 0; opd < IRA_INST_OPDS_SIZE; ++opd)
    {
        switch (info->opds[opd])
        {
            case IraInstOpdNone:
                break;
            case IraInstOpdIntCmp:
                dst->opds[opd].int_cmp = src->opds[opd].int_cmp;
                break;
            case IraInstOpdDtQual:
                dst->opds[opd].dt_qual = src->opds[opd].dt_qual;
                break;
            case IraInstOpdDtFuncVas:
                dst->opds[opd].dt_func_vas = src->opds[opd].dt_func_vas;
                break;
            case IraInstOpdDt:
                dst->opds[opd].dt = src->opds[opd].dt;
                break;
            case IraInstOpdLabel:
            case IraInstOpdVarDef:
            case IraInstOpdVar:
            case IraInstOpdMmbr:
                dst->opds[opd].hs = src->opds[opd].hs;
                break;
            case IraInstOpdVal:
                dst->opds[opd].val = ira_val_copy(src->opds[opd].val);
                break;
            case IraInstOpdOptr:
                dst->opds[opd].optr = src->opds[opd].optr;
                break;
            case IraInstOpdVarsSize:
                dst->opds[opd].size = src->opds[opd].size;
                break;
            case IraInstOpdVars2:
            case IraInstOpdIds2:
                dst->opds[opd].hss = malloc(src->opds[2].size * sizeof(*dst->opds[opd].hss));

                ul_assert(dst->opds[opd].hss != NULL);

                for (ul_hs_t **src_hs = src->opds[opd].hss, **src_hs_end = src_hs + src->opds[2].size, **dst_hs = dst->opds[opd].hss; src_hs != src_hs_end; ++src_hs, ++dst_hs)
                {
                    *dst_hs = *src_hs;
                }

                break;
            default:
                ul_assert_unreachable();
        }
    }
}
void ira_inst_cleanup(ira_inst_t * inst)
{
    ul_assert(inst->type < IraInst_Count);

    const ira_inst_info_t * info = &ira_inst_infos[inst->type];

    for (size_t opd = 0; opd < IRA_INST_OPDS_SIZE; ++opd)
    {
        switch (info->opds[opd])
        {
            case IraInstOpdNone:
            case IraInstOpdIntCmp:
            case IraInstOpdDtQual:
            case IraInstOpdDtFuncVas:
            case IraInstOpdDt:
            case IraInstOpdLabel:
            case IraInstOpdVarDef:
            case IraInstOpdVar:
            case IraInstOpdOptr:
            case IraInstOpdMmbr:
            case IraInstOpdVarsSize:
                break;
            case IraInstOpdVal:
                ira_val_destroy(inst->opds[opd].val);
                break;
            case IraInstOpdVars2:
            case IraInstOpdIds2:
                free(inst->opds[opd].hss);
                break;
            default:
                ul_assert_unreachable();
        }
    }

    memset(inst, 0, sizeof(*inst));
}

const ira_inst_info_t ira_inst_infos[IraInst_Count] = {
    [IraInstNone] = { .type_str = UL_ROS_MAKE("InstNone"), .intrp_comp = true, .compl_comp = true, .opds = { IraInstOpdNone, IraInstOpdNone, IraInstOpdNone, IraInstOpdNone, IraInstOpdNone }, .mods_ctl_flow = false },
#define IRA_INST(name, compl_, intrp_, opd0, opd1, opd2, opd3, opd4, opd5, mods_ctl_flow_) [IraInst##name] = { .type_str = UL_ROS_MAKE("Inst" #name), .intrp_comp = intrp_, .compl_comp = compl_, .opds = { IraInstOpd##opd0, IraInstOpd##opd1, IraInstOpd##opd2, IraInstOpd##opd3, IraInstOpd##opd4, IraInstOpd##opd5 }, .mods_ctl_flow = mods_ctl_flow_ },
#include "ira_inst.data"
#undef IRA_INST
};
