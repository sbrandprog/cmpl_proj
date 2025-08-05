#pragma once
#include "mc_size.h"

#define MC_INST_OPDS_SIZE 3
#define MC_INST_BS_SIZE 15

enum mc_inst_type
{
    McInstNone,
    McInstLabel,
    McInstData,
    McInstAlign,
    McInstAdd,
    McInstOr,
    McInstAdc,
    McInstSbb,
    McInstAnd,
    McInstSub,
    McInstXor,
    McInstCmp,
    McInstMov,
    McInstMovsx,
    McInstMovsxd,
    McInstMovzx,
    McInstLea,
    McInstCall,
    McInstRet,
    McInstInt3,
    McInstJmp,
    McInstJo,
    McInstJno,
    McInstJc,
    McInstJb = McInstJc,
    McInstJnae = McInstJc,
    McInstJnc,
    McInstJnb = McInstJnc,
    McInstJae = McInstJnc,
    McInstJz,
    McInstJe = McInstJz,
    McInstJnz,
    McInstJne = McInstJnz,
    McInstJbe,
    McInstJna = McInstJbe,
    McInstJnbe,
    McInstJa = McInstJnbe,
    McInstJs,
    McInstJns,
    McInstJp,
    McInstJpe = McInstJp,
    McInstJnp,
    McInstJpo = McInstJnp,
    McInstJl,
    McInstJnge = McInstJl,
    McInstJnl,
    McInstJge = McInstJnl,
    McInstJle,
    McInstJng = McInstJle,
    McInstJnle,
    McInstJg = McInstJnle,
    McInstTest,
    McInstNot,
    McInstNeg,
    McInstMul,
    McInstImul,
    McInstDiv,
    McInstIdiv,
    McInstCbw,
    McInstCwde,
    McInstCdqe,
    McInstCwd,
    McInstCdq,
    McInstCqo,
    McInstSeto,
    McInstSetno,
    McInstSetb,
    McInstSetc = McInstSetb,
    McInstSetnae = McInstSetb,
    McInstSetnb,
    McInstSetnc = McInstSetnb,
    McInstSetae = McInstSetnb,
    McInstSetz,
    McInstSete = McInstSetz,
    McInstSetnz,
    McInstSetne = McInstSetnz,
    McInstSetbe,
    McInstSetna = McInstSetbe,
    McInstSetnbe,
    McInstSeta = McInstSetnbe,
    McInstSets,
    McInstSetns,
    McInstSetp,
    McInstSetpe = McInstSetp,
    McInstSetnp,
    McInstSetpo = McInstSetnp,
    McInstSetl,
    McInstSetnge = McInstSetl,
    McInstSetnl,
    McInstSetge = McInstSetnl,
    McInstSetle,
    McInstSetng = McInstSetle,
    McInstSetnle,
    McInstSetg = McInstSetnle,
    McInstRol,
    McInstRor,
    McInstRcl,
    McInstRcr,
    McInstShl,
    McInstSal = McInstShl,
    McInstShr,
    McInstSar,
    McInst_Count
};
enum mc_inst_opd
{
    McInstOpdNone,
    McInstOpdLabel,
    McInstOpdImm,
    McInstOpdReg,
    McInstOpdMem,
    McInstOpd_Count
};
enum mc_inst_opds
{
    McInstOpds_None,
    McInstOpds_Label,
    McInstOpds_Imm,
    McInstOpds_Reg,
    McInstOpds_Mem,
    McInstOpds_Reg_Imm,
    McInstOpds_Mem_Imm,
    McInstOpds_Reg_Reg,
    McInstOpds_Reg_Mem,
    McInstOpds_Mem_Reg,
    McInstOpds_Reg_Reg_Imm,
    McInstOpds_Reg_Mem_Imm,
    McInstOpds__Count
};
enum mc_inst_imm_type
{
    McInstImmNone,
    McInstImm8,
    McInstImm16,
    McInstImm32,
    McInstImm64,
    McInstImmLabelRel8,
    McInstImmLabelRel32,
    McInstImmLabelVa64,
    McInstImmLabelRva32,
    McInstImmLabelRva31of64,
    McInstImm_Count
};
enum mc_inst_disp_type
{
    McInstDispAuto,
    McInstDispNone,
    McInstDisp8,
    McInstDisp32,
    McInstDispLabelRel8,
    McInstDispLabelRel32,
    McInstDispLabelRva32,
    McInstDisp_Count
};
struct mc_inst
{
    mc_inst_type_t type;
    mc_inst_opds_t opds;
    mc_reg_t reg0;
    mc_reg_t reg1;
    mc_reg_t mem_base;
    mc_reg_t mem_index;
    mc_size_t mem_scale;
    mc_size_t mem_size;
    mc_inst_disp_type_t mem_disp_type;
    mc_inst_imm_type_t imm0_type;
    int32_t mem_disp;
    union
    {
        ul_hs_t * mem_disp_label;
        lnk_sect_lp_stype_t label_stype;
    };
    union
    {
        int64_t imm0;
        ul_hs_t * imm0_label;
        ul_hs_t * label;
    };
};
enum mc_inst_off_type
{
    McInstOffPrLock,
    McInstOffPrAsize,
    McInstOffPrOsize,
    McInstOffPrRex,
    McInstOffPr0f,
    McInstOffOpc,
    McInstOffModrm,
    McInstOffSib,
    McInstOffDisp,
    McInstOffImm,
    McInstOffEnd,
    McInstOff_Count
};
struct mc_inst_offs
{
    uint8_t off[McInstOff_Count];
};
struct mc_inst_bs
{
    uint8_t mem[MC_INST_BS_SIZE];
};

MC_API bool mc_inst_build(mc_inst_t * inst, ul_ec_fmtr_t * ec_fmtr, size_t * inst_size_out, mc_inst_bs_t * bs_out, mc_inst_offs_t * offs_out);

inline bool mc_inst_get_size(mc_inst_t * inst, size_t * out)
{
    return mc_inst_build(inst, NULL, out, NULL, NULL);
}

extern MC_API const mc_inst_opd_t mc_inst_opds_to_opd[McInstOpds__Count][MC_INST_OPDS_SIZE];
extern MC_API const mc_size_t mc_inst_imm_type_to_size[McInstImm_Count];
extern MC_API const mc_inst_imm_type_t mc_inst_imm_size_to_type[McSize_Count];
extern MC_API const mc_size_t mc_inst_disp_type_to_size[McInstDisp_Count];
