#include "ira_inst.h"
#include "ira_val.h"

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
