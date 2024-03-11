#pragma once
#include "asm.h"

#define ASM_INST_OPDS_SIZE 3
#define ASM_INST_BS_SIZE 15

enum asm_inst_type {
	AsmInstNone,
	AsmInstLabel,
	AsmInstData,
	AsmInstAlign,
	AsmInstAdd,
	AsmInstOr,
	AsmInstAdc,
	AsmInstSbb,
	AsmInstAnd,
	AsmInstSub,
	AsmInstXor,
	AsmInstCmp,
	AsmInstMov,
	AsmInstMovsx,
	AsmInstMovsxd,
	AsmInstMovzx,
	AsmInstLea,
	AsmInstCall,
	AsmInstRet,
	AsmInstInt3,
	AsmInstJmp,
	AsmInstJo,
	AsmInstJno,
	AsmInstJc, AsmInstJb = AsmInstJc, AsmInstJnae = AsmInstJc,
	AsmInstJnc, AsmInstJnb = AsmInstJnc, AsmInstJae = AsmInstJnc,
	AsmInstJz, AsmInstJe = AsmInstJz,
	AsmInstJnz, AsmInstJne = AsmInstJnz,
	AsmInstJbe, AsmInstJna = AsmInstJbe,
	AsmInstJnbe, AsmInstJa = AsmInstJnbe,
	AsmInstJs,
	AsmInstJns,
	AsmInstJp, AsmInstJpe = AsmInstJp,
	AsmInstJnp, AsmInstJpo = AsmInstJnp,
	AsmInstJl, AsmInstJnge = AsmInstJl,
	AsmInstJnl, AsmInstJge = AsmInstJnl,
	AsmInstJle, AsmInstJng = AsmInstJle,
	AsmInstJnle, AsmInstJg = AsmInstJnle,
	AsmInstTest,
	AsmInstNot,
	AsmInstNeg,
	AsmInstMul,
	AsmInstImul,
	AsmInstDiv,
	AsmInstIdiv,
	AsmInstCbw, AsmInstCwde, AsmInstCdqe,
	AsmInstCwd, AsmInstCdq, AsmInstCqo,
	AsmInstSeto,
	AsmInstSetno,
	AsmInstSetb, AsmInstSetc = AsmInstSetb, AsmInstSetnae = AsmInstSetb,
	AsmInstSetnb, AsmInstSetnc = AsmInstSetnb, AsmInstSetae = AsmInstSetnb,
	AsmInstSetz, AsmInstSete = AsmInstSetz,
	AsmInstSetnz, AsmInstSetne = AsmInstSetnz,
	AsmInstSetbe, AsmInstSetna = AsmInstSetbe,
	AsmInstSetnbe, AsmInstSeta = AsmInstSetnbe,
	AsmInstSets,
	AsmInstSetns,
	AsmInstSetp, AsmInstSetpe = AsmInstSetp,
	AsmInstSetnp, AsmInstSetpo = AsmInstSetnp,
	AsmInstSetl, AsmInstSetnge = AsmInstSetl,
	AsmInstSetnl, AsmInstSetge = AsmInstSetnl,
	AsmInstSetle, AsmInstSetng = AsmInstSetle,
	AsmInstSetnle, AsmInstSetg = AsmInstSetnle,
	AsmInstRol,
	AsmInstRor,
	AsmInstRcl,
	AsmInstRcr,
	AsmInstShl, AsmInstSal = AsmInstShl,
	AsmInstShr,
	AsmInstSar,
	AsmInst_Count
};
enum asm_inst_opd {
	AsmInstOpdNone,
	AsmInstOpdLabel,
	AsmInstOpdImm,
	AsmInstOpdReg,
	AsmInstOpdMem,
	AsmInstOpd_Count
};
enum asm_inst_opds {
	AsmInstOpds_None,
	AsmInstOpds_Label,
	AsmInstOpds_Imm,
	AsmInstOpds_Reg,
	AsmInstOpds_Mem,
	AsmInstOpds_Reg_Imm,
	AsmInstOpds_Mem_Imm,
	AsmInstOpds_Reg_Reg,
	AsmInstOpds_Reg_Mem,
	AsmInstOpds_Mem_Reg,
	AsmInstOpds_Reg_Reg_Imm,
	AsmInstOpds_Reg_Mem_Imm,
	AsmInstOpds__Count
};
enum asm_inst_imm_type {
	AsmInstImmNone,
	AsmInstImm8,
	AsmInstImm16,
	AsmInstImm32,
	AsmInstImm64,
	AsmInstImmLabelRel8,
	AsmInstImmLabelRel32,
	AsmInstImmLabelVa64,
	AsmInstImmLabelRva32,
	AsmInstImm_Count
};
enum asm_inst_disp_type {
	AsmInstDispAuto,
	AsmInstDispNone,
	AsmInstDisp8,
	AsmInstDisp32,
	AsmInstDispLabelRel8,
	AsmInstDispLabelRel32,
	AsmInstDispLabelRva32,
	AsmInstDisp_Count
};
struct asm_inst {
	asm_inst_type_t type;
	asm_inst_opds_t opds;
	asm_reg_t reg0;
	asm_reg_t reg1;
	asm_reg_t mem_base;
	asm_reg_t mem_index;
	asm_size_t mem_scale;
	asm_size_t mem_size;
	asm_inst_disp_type_t mem_disp_type;
	asm_inst_imm_type_t imm0_type;
	union {
		int32_t mem_disp;
		ul_hs_t * mem_disp_label;
	};
	union {
		int64_t imm0;
		ul_hs_t * imm0_label;
		ul_hs_t * label;
	};
};
enum asm_inst_off_type {
	AsmInstOffPrLock,
	AsmInstOffPrAsize,
	AsmInstOffPrOsize,
	AsmInstOffPrRex,
	AsmInstOffPr0f,
	AsmInstOffOpc,
	AsmInstOffModrm,
	AsmInstOffSib,
	AsmInstOffDisp,
	AsmInstOffImm,
	AsmInstOffEnd,
	AsmInstOff_Count
};
struct asm_inst_offs {
	uint8_t off[AsmInstOff_Count];
};

typedef uint8_t asm_inst_bs_t[ASM_INST_BS_SIZE];

bool asm_inst_build(asm_inst_t * inst, size_t * inst_size, asm_inst_bs_t out, asm_inst_offs_t * offs_out);

extern const asm_inst_opd_t asm_inst_opds_to_opd[AsmInstOpds__Count][ASM_INST_OPDS_SIZE];
extern const asm_size_t asm_inst_imm_type_to_size[AsmInstImm_Count];
extern const asm_size_t asm_inst_disp_type_to_size[AsmInstDisp_Count];
