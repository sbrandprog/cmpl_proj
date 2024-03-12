#include "pch.h"
#include "asm_inst.h"
#include "asm_size.h"
#include "asm_reg.h"

#define BS_BUF_SIZE 64

#define OSIZE 0x66
#define ASIZE 0x67
#define LOCK 0xF0
#define REPZ 0xF3
#define REPNZ 0xF2
#define REX_BASE 0x40
#define REX_B 0x01
#define REX_X 0x02
#define REX_R 0x04
#define REX_W 0x08


enum inst_rec_opds_enc {
	InstRecOpds_None,
	InstRecOpds_Label,
	InstRecOpds_Imm,
	InstRecOpds_RmReg,
	InstRecOpds_RmMem,
	InstRecOpds_ImplReg_Imm,
	InstRecOpds_OpcReg_Imm,
	InstRecOpds_RmReg_Imm,
	InstRecOpds_RmMem_Imm,
	InstRecOpds_RmReg_Imm1,
	InstRecOpds_RmMem_Imm1,
	InstRecOpds_RmReg_ImplReg,
	InstRecOpds_RmMem_ImplReg,
	InstRecOpds_RmReg_RegReg,
	InstRecOpds_RegReg_RmReg,
	InstRecOpds_RmMem_RegReg,
	InstRecOpds_RegReg_RmMem,
	InstRecOpds_RegReg_RmReg_Imm,
	InstRecOpds_RegReg_RmMem_Imm,
	InstRecOpds__Count
};
typedef uint8_t inst_rec_opds_enc_t;

enum inst_rec_imm_type {
	InstRecImm8,
	InstRecImm16,
	InstRecImm32,
	InstRecImm64,
	InstRecImm8_1,
	InstRecImm_Count
};
typedef uint8_t inst_rec_imm_type_t;
enum inst_rec_reg_type {
	InstRecRegGprOpc8,
	InstRecRegGprOpc16,
	InstRecRegGprOpc32,
	InstRecRegGprOpc64,
	InstRecRegGprReg8,
	InstRecRegGprReg16,
	InstRecRegGprReg32,
	InstRecRegGprReg64,
	InstRecRegGprRm8,
	InstRecRegGprRm16,
	InstRecRegGprRm32,
	InstRecRegGprRm64,
	InstRecRegAx8,
	InstRecRegAx16,
	InstRecRegAx32,
	InstRecRegAx64,
	InstRecRegCx8,
	InstRecRegCx16,
	InstRecRegCx32,
	InstRecRegCx64,
	InstRecReg_Count
};
typedef uint8_t inst_rec_reg_type_t;

typedef struct inst_rec {
	asm_inst_type_t type;
	uint8_t opc;
	asm_inst_opds_t opds;
	inst_rec_reg_type_t reg0_type, reg1_type;
	inst_rec_imm_type_t imm0_type;
	asm_size_t mem0_size;
	uint8_t modrm_reg_digit : 3;
	uint8_t allow_lock : 1;
	uint8_t use_osize : 1;
	uint8_t use_0f : 1;
	uint8_t ext_w : 1;
	uint8_t no_opc : 1;
} inst_rec_t;

typedef struct inst_desc {
	uint8_t opc;

	bool req_rex, no_rex;

	bool use_lock, use_asize, use_osize, use_rex, use_0f, use_opc, use_modrm, use_sib;

	bool ext_b, ext_x, ext_r, ext_w;

	asm_size_t imm0_size, disp_size;

	uint8_t opc_reg;

	uint8_t modrm_rm, modrm_reg, modrm_mod;

	uint8_t sib_base, sib_index, sib_scale;

	int32_t disp;

	int64_t imm0;
} inst_desc_t;

const asm_size_t imm_type_to_size[InstRecImm_Count] = {
	[InstRecImm8] = AsmSize8,
	[InstRecImm16] = AsmSize16,
	[InstRecImm32] = AsmSize32,
	[InstRecImm64] = AsmSize64,
	[InstRecImm8_1] = AsmSizeNone,
};
const asm_reg_grps_t reg_type_to_grps[InstRecReg_Count] = {
	[InstRecRegGprOpc8] = { .s_8 = 1, .gpr = 1 },
	[InstRecRegGprOpc16] = { .s_16 = 1, .gpr = 1 },
	[InstRecRegGprOpc32] = { .s_32 = 1, .gpr = 1 },
	[InstRecRegGprOpc64] = { .s_64 = 1, .gpr = 1 },
	[InstRecRegGprReg8] = { .s_8 = 1, .gpr = 1 },
	[InstRecRegGprReg16] = { .s_16 = 1, .gpr = 1 },
	[InstRecRegGprReg32] = { .s_32 = 1, .gpr = 1 },
	[InstRecRegGprReg64] = { .s_64 = 1, .gpr = 1 },
	[InstRecRegGprRm8] = { .s_8 = 1, .gpr = 1 },
	[InstRecRegGprRm16] = { .s_16 = 1, .gpr = 1 },
	[InstRecRegGprRm32] = { .s_32 = 1, .gpr = 1 },
	[InstRecRegGprRm64] = { .s_64 = 1, .gpr = 1 },
	[InstRecRegAx8] = { .s_8 = 1, .gpr = 1, .gpr_ax = 1 },
	[InstRecRegAx16] = { .s_16 = 1, .gpr = 1, .gpr_ax = 1 },
	[InstRecRegAx32] = { .s_32 = 1, .gpr = 1, .gpr_ax = 1 },
	[InstRecRegAx64] = { .s_64 = 1, .gpr = 1, .gpr_ax = 1 },
	[InstRecRegCx8] = { .s_8 = 1, .gpr = 1, .gpr_cx = 1 },
	[InstRecRegCx16] = { .s_16 = 1, .gpr = 1, .gpr_cx = 1 },
	[InstRecRegCx32] = { .s_32 = 1, .gpr = 1, .gpr_cx = 1 },
	[InstRecRegCx64] = { .s_64 = 1, .gpr = 1, .gpr_cx = 1 },
};

const inst_rec_t inst_recs[];
static const size_t inst_recs_size;

static bool is_imm_value(asm_inst_imm_type_t imm_type) {
	switch (imm_type) {
		case AsmInstImm8:
		case AsmInstImm16:
		case AsmInstImm32:
		case AsmInstImm64:
			return true;
	}

	return false;
}
static bool is_disp_32(asm_inst_disp_type_t disp_type) {
	switch (disp_type) {
		case AsmInstDispAuto:
		case AsmInstDisp32:
		case AsmInstDispLabelRel32:
		case AsmInstDispLabelRva32:
			return true;
	}

	return false;
}
static bool is_disp_value(asm_inst_disp_type_t disp_type) {
	switch (disp_type) {
		case AsmInstDispAuto:
		case AsmInstDisp8:
		case AsmInstDisp32:
			return true;
	}

	return false;
}

static bool validate_imm(asm_inst_imm_type_t imm_type) {
	if (imm_type >= AsmInstImm_Count) {
		return false;
	}

	return true;
}
static bool validate_reg(asm_reg_t reg, bool * req_rex, bool * no_rex, bool * use_ext) {
	if (reg >= AsmReg_Count) {
		return false;
	}

	const asm_reg_info_t * info = &asm_reg_infos[reg];

	*req_rex = *req_rex || info->grps.req_rex;
	*no_rex = *no_rex || info->grps.no_rex;
	*use_ext = *use_ext || info->ext;

	return true;
}
static bool validate_mem(asm_inst_t * inst, bool * use_ext) {
	if (inst->mem_base >= AsmReg_Count || inst->mem_index >= AsmReg_Count) {
		return false;
	}

	if (inst->mem_disp_type >= AsmInstDisp_Count) {
		return false;
	}

	const asm_reg_info_t * base = &asm_reg_infos[inst->mem_base],
		* index = &asm_reg_infos[inst->mem_index];

	if (base->grps.ip && index->grps.none) {
		if (!base->grps.s_32 && !base->grps.s_64) {
			return false;
		}

		if (!is_disp_32(inst->mem_disp_type)) {
			return false;
		}
	}
	else if (base->grps.gpr && index->grps.none) {
		if (!base->grps.s_32 && !base->grps.s_64) {
			return false;
		}

		if (inst->mem_disp_type != AsmInstDispAuto) {
			if (asm_inst_disp_type_to_size[inst->mem_disp_type] == AsmSizeNone && base->enc == 0b101) {
				return false;
			}
		}

		*use_ext = *use_ext || base->ext;
	}
	else if (base->grps.none && index->grps.gpr) {
		if (!index->grps.s_32 && !index->grps.s_64) {
			return false;
		}

		if (index->enc == 0b100 && !index->ext) {
			return false;
		}

		if (!asm_size_infos[inst->mem_scale].is_int) {
			return false;
		}

		if (!is_disp_32(inst->mem_disp_type)) {
			return false;
		}

		*use_ext = *use_ext || index->ext;
	}
	else if (base->grps.gpr && index->grps.gpr) {
		if ((!base->grps.s_32 || !index->grps.s_32)
			&& (!base->grps.s_64 || !index->grps.s_64)) {
			return false;
		}

		if (index->enc == 0b100 && !index->ext) {
			return false;
		}

		if (!asm_size_infos[inst->mem_scale].is_int) {
			return false;
		}

		if (inst->mem_disp_type != AsmInstDispAuto) {
			if (asm_inst_disp_type_to_size[inst->mem_disp_type] == AsmSizeNone && base->enc == 0b101) {
				return false;
			}
		}

		*use_ext = *use_ext || base->ext || index->ext;
	}
	else {
		return false;
	}

	return true;
}
static bool validate_opds(asm_inst_t * inst) {
	bool req_rex = false, no_rex = false, use_ext = false;

	switch (inst->opds) {
		case InstRecOpds_None:
		case InstRecOpds_Label:
			break;
		case AsmInstOpds_Imm:
			if (!validate_imm(inst->imm0_type)) {
				return false;
			}
			break;
		case AsmInstOpds_Reg:
			if (!validate_reg(inst->reg0, &req_rex, &no_rex, &use_ext)) {
				return false;
			}
			break;
		case AsmInstOpds_Mem:
			if (!validate_mem(inst, &use_ext)) {
				return false;
			}
			break;
		case AsmInstOpds_Reg_Imm:
			if (!validate_reg(inst->reg0, &req_rex, &no_rex, &use_ext)
				|| !validate_imm(inst->imm0_type)) {
				return false;
			}
			break;
		case AsmInstOpds_Mem_Imm:
			if (!validate_mem(inst, &use_ext)
				|| !validate_imm(inst->imm0_type)) {
				return false;
			}
			break;
		case AsmInstOpds_Reg_Reg:
			if (!validate_reg(inst->reg0, &req_rex, &no_rex, &use_ext)
				|| !validate_reg(inst->reg1, &req_rex, &no_rex, &use_ext)) {
				return false;
			}
			break;
		case AsmInstOpds_Reg_Mem:
		case AsmInstOpds_Mem_Reg:
			if (!validate_reg(inst->reg0, &req_rex, &no_rex, &use_ext)
				|| !validate_mem(inst, &use_ext)) {
				return false;
			}
			break;
		case AsmInstOpds_Reg_Reg_Imm:
			if (!validate_reg(inst->reg0, &req_rex, &no_rex, &use_ext)
				|| !validate_reg(inst->reg1, &req_rex, &no_rex, &use_ext)
				|| !validate_imm(inst->imm0_type)) {
				return false;
			}
			break;
		case AsmInstOpds_Reg_Mem_Imm:
			if (!validate_reg(inst->reg0, &req_rex, &no_rex, &use_ext)
				|| !validate_mem(inst, &use_ext)
				|| !validate_imm(inst->imm0_type)) {
				return false;
			}
			break;
		default:
			ul_raise_unreachable();
	}

	if (no_rex && (req_rex || use_ext)) {
		return false;
	}

	return true;
}
static bool validate_inst(asm_inst_t * inst) {
	if (inst->type >= AsmInst_Count) {
		return false;
	}

	if (!validate_opds(inst)) {
		return false;
	}

	return true;
}

static bool is_imm_match(asm_inst_t * inst, const inst_rec_t * rec, size_t imm_i) {
	ul_raise_assert(imm_i == 0);

	switch (rec->imm0_type) {
		case InstRecImm8:
		case InstRecImm16:
		case InstRecImm32:
		case InstRecImm64:
			if (asm_inst_imm_type_to_size[inst->imm0_type] != imm_type_to_size[rec->imm0_type]) {
				return false;
			}
			break;
		case InstRecImm8_1:
			if (inst->imm0_type != AsmInstImm8 || inst->imm0 != 1) {
				return false;
			}
			break;
		default:
			ul_raise_unreachable();
	}

	return true;
}
static bool is_reg_match(asm_inst_t * inst, const inst_rec_t * rec, size_t reg_i) {
	asm_reg_t reg;
	inst_rec_reg_type_t reg_type;

	switch (reg_i) {
		case 0:
			reg = inst->reg0;
			reg_type = rec->reg0_type;
			break;
		case 1:
			reg = inst->reg1;
			reg_type = rec->reg1_type;
			break;
		default:
			ul_raise_unreachable();
	}

	const asm_reg_grps_t * reg_grps = &reg_type_to_grps[reg_type];

	if (!asm_reg_check_grps(&asm_reg_infos[reg].grps, reg_grps)) {
		return false;
	}

	return true;
}
static bool is_mem_match(asm_inst_t * inst, const inst_rec_t * rec, size_t mem_i) {
	ul_raise_assert(mem_i == 0);

	if (rec->mem0_size == AsmSizeNone) {
		return true;
	}

	if (inst->mem_size != rec->mem0_size) {
		return false;
	}

	return true;
}
static bool is_opds_match(asm_inst_t * inst, const inst_rec_t * rec) {
	if (inst->opds != rec->opds) {
		return false;
	}

	size_t imm_i = 0, reg_i = 0, mem_i = 0;

	for (size_t opd = 0; opd < ASM_INST_OPDS_SIZE; ++opd) {
		asm_inst_opd_t opd_type = asm_inst_opds_to_opd[rec->opds][opd];

		switch (opd_type) {
			case AsmInstOpdNone:
			case AsmInstOpdLabel:
				break;
			case AsmInstOpdImm:
				if (!is_imm_match(inst, rec, imm_i++)) {
					return false;
				}
				break;
			case AsmInstOpdReg:
				if (!is_reg_match(inst, rec, reg_i++)) {
					return false;
				}
				break;
			case AsmInstOpdMem:
				if (!is_mem_match(inst, rec, mem_i++)) {
					return false;
				}
				break;
			default:
				ul_raise_unreachable();
		}
	}

	return true;
}
static const inst_rec_t * find_rec(asm_inst_t * inst) {
	size_t i = 0;

	while (i < inst_recs_size && inst->type != inst_recs[i].type) {
		++i;
	}

	for (; i < inst_recs_size && inst->type == inst_recs[i].type; ++i) {
		if (is_opds_match(inst, &inst_recs[i])) {
			return &inst_recs[i];
		}
	}

	return NULL;
}

static void make_desc_imm(inst_desc_t * desc, asm_inst_t * inst, const inst_rec_t * rec, size_t imm_i) {
	ul_raise_assert(imm_i == 0);

	desc->imm0_size = imm_type_to_size[rec->imm0_type];

	switch (rec->imm0_type) {
		case InstRecImm8:
		case InstRecImm16:
		case InstRecImm32:
		case InstRecImm64:
			if (is_imm_value(inst->imm0_type)) {
				desc->imm0 = inst->imm0;
			}
			else {
				desc->imm0 = 0;
			}
			break;
		case InstRecImm8_1:
			break;
		default:
			ul_raise_unreachable()
				;
	}
}
static void make_desc_reg(inst_desc_t * desc, asm_inst_t * inst, const inst_rec_t * rec, size_t reg_i) {
	asm_reg_t reg;
	inst_rec_reg_type_t reg_type;

	switch (reg_i) {
		case 0:
			reg = inst->reg0;
			reg_type = rec->reg0_type;
			break;
		case 1:
			reg = inst->reg1;
			reg_type = rec->reg1_type;
			break;
		default:
			ul_raise_unreachable();
	}

	const asm_reg_info_t * info = &asm_reg_infos[reg];

	switch (reg_type) {
		case InstRecRegGprOpc8:
		case InstRecRegGprOpc16:
		case InstRecRegGprOpc32:
		case InstRecRegGprOpc64:
			desc->opc_reg = info->enc;
			desc->ext_b = info->ext;

			desc->req_rex = desc->req_rex || info->grps.req_rex;
			desc->no_rex = desc->no_rex || info->grps.no_rex;
			break;
		case InstRecRegGprReg8:
		case InstRecRegGprReg16:
		case InstRecRegGprReg32:
		case InstRecRegGprReg64:
			desc->use_modrm = true;

			desc->modrm_reg = info->enc;
			desc->ext_r = info->ext;

			desc->req_rex = desc->req_rex || info->grps.req_rex;
			desc->no_rex = desc->no_rex || info->grps.no_rex;
			break;
		case InstRecRegGprRm8:
		case InstRecRegGprRm16:
		case InstRecRegGprRm32:
		case InstRecRegGprRm64:
			desc->use_modrm = true;

			desc->modrm_mod = 0b11;
			desc->modrm_rm = info->enc;
			desc->ext_b = info->ext;

			desc->req_rex = desc->req_rex || info->grps.req_rex;
			desc->no_rex = desc->no_rex || info->grps.no_rex;
			break;
		case InstRecRegAx8:
		case InstRecRegAx16:
		case InstRecRegAx32:
		case InstRecRegAx64:
		case InstRecRegCx8:
		case InstRecRegCx16:
		case InstRecRegCx32:
		case InstRecRegCx64:
			break;
		default:
			ul_raise_unreachable();
	}
}
static void make_desc_disp_size(inst_desc_t * desc, asm_inst_t * inst, const asm_reg_info_t * base_info) {
	if (inst->mem_disp_type == AsmInstDispAuto) {
		if (inst->mem_disp == 0 && base_info->enc != 0b101) {
			desc->disp_size = AsmSizeNone;
		}
		else if (INT8_MIN <= inst->mem_disp && inst->mem_disp <= INT8_MAX) {
			desc->disp_size = AsmSize8;
		}
		else {
			desc->disp_size = AsmSize32;
		}
	}
	else {
		desc->disp_size = asm_inst_disp_type_to_size[inst->mem_disp_type];

		ul_raise_assert(desc->disp_size != AsmSizeNone || base_info->enc != 0b101);
	}

	switch (desc->disp_size) {
		case AsmSizeNone:
			desc->modrm_mod = 0b00;
			break;
		case AsmSize8:
			desc->modrm_mod = 0b01;
			break;
		case AsmSize32:
			desc->modrm_mod = 0b10;
			break;
		default:
			ul_raise_unreachable();
	}
}
static void make_desc_sib_scale(inst_desc_t * desc, asm_size_t scale) {
	switch (scale) {
		case AsmSize8:
			desc->sib_scale = 0b00;
			break;
		case AsmSize16:
			desc->sib_scale = 0b01;
			break;
		case AsmSize32:
			desc->sib_scale = 0b10;
			break;
		case AsmSize64:
			desc->sib_scale = 0b11;
			break;
		default:
			ul_raise_unreachable();
	}
}
static void make_desc_mem(inst_desc_t * desc, asm_inst_t * inst, const inst_rec_t * rec, size_t mem_i) {
	ul_raise_assert(mem_i == 0);

	const asm_reg_info_t * base = &asm_reg_infos[inst->mem_base],
		* index = &asm_reg_infos[inst->mem_index];

	desc->use_modrm = true;

	if (is_disp_value(inst->mem_disp_type)) {
		desc->disp = inst->mem_disp;
	}
	else {
		desc->disp = 0;
	}

	if (base->grps.ip && index->grps.none) {
		desc->modrm_mod = 0b00;
		desc->modrm_rm = 0b101;

		desc->disp_size = AsmSize32;
		ul_raise_assert(is_disp_32(inst->mem_disp_type));

		if (base->grps.s_32) {
			desc->use_asize = true;
		}
	}
	else if (base->grps.gpr && index->grps.none) {
		desc->modrm_rm = base->enc;
		desc->ext_b = base->ext;

		if (desc->modrm_rm == 0b100) {
			desc->use_sib = true;

			desc->sib_base = 0b100;
			desc->sib_index = 0b100;
			desc->sib_scale = 0b00;
		}

		make_desc_disp_size(desc, inst, base);

		if (base->grps.s_32) {
			desc->use_asize = true;
		}
	}
	else if (base->grps.none && index->grps.gpr) {
		desc->modrm_mod = 0b00;
		desc->modrm_rm = 0b100;

		desc->use_sib = true;

		desc->sib_base = 0b101;
		desc->sib_index = index->enc;
		desc->ext_x = index->ext;
		make_desc_sib_scale(desc, inst->mem_scale);

		desc->disp_size = AsmSize32;
		ul_raise_assert(is_disp_32(inst->mem_disp_type));

		if (index->grps.s_32) {
			desc->use_asize = true;
		}
	}
	else if (base->grps.gpr && index->grps.gpr) {
		desc->modrm_mod = 0b00;
		desc->modrm_rm = 0b100;

		desc->use_sib = true;

		desc->sib_base = base->enc;
		desc->ext_b = base->ext;
		desc->sib_index = index->enc;
		desc->ext_x = index->ext;
		make_desc_sib_scale(desc, inst->mem_scale);

		make_desc_disp_size(desc, inst, base);

		if (base->grps.s_32) {
			desc->use_asize = true;
		}
	}
	else {
		ul_raise_unreachable();
	}
}
static void make_desc(inst_desc_t * desc, asm_inst_t * inst, const inst_rec_t * rec) {
	memset(desc, 0, sizeof(*desc));

	desc->ext_w = rec->ext_w;

	desc->use_osize = rec->use_osize;
	desc->use_0f = rec->use_0f;

	desc->opc = rec->opc;
	desc->use_opc = !rec->no_opc;

	desc->modrm_reg = rec->modrm_reg_digit;

	size_t imm_i = 0, reg_i = 0, mem_i = 0;

	for (size_t opd = 0; opd < ASM_INST_OPDS_SIZE; ++opd) {
		asm_inst_opd_t opd_type = asm_inst_opds_to_opd[rec->opds][opd];

		switch (opd_type) {
			case AsmInstOpdNone:
			case AsmInstOpdLabel:
				break;
			case AsmInstOpdImm:
				make_desc_imm(desc, inst, rec, imm_i++);
				break;
			case AsmInstOpdReg:
				make_desc_reg(desc, inst, rec, reg_i++);
				break;
			case AsmInstOpdMem:
				make_desc_mem(desc, inst, rec, mem_i++);
				break;
			default:
				ul_raise_unreachable();
		}
	}

	if (desc->ext_b || desc->ext_x || desc->ext_r || desc->ext_w || desc->req_rex) {
		ul_raise_assert(!desc->no_rex);

		desc->use_rex = true;
	}
}

static bool build_desc(const inst_desc_t * desc, size_t * inst_size, asm_inst_bs_t bs_out, asm_inst_offs_t * offs_out) {
	uint8_t buf[BS_BUF_SIZE];
	uint8_t * data = buf;

	if (desc->use_lock) {
		offs_out->off[AsmInstOffPrLock] = (uint8_t)(data - buf);
		*data++ = LOCK;
	}
	if (desc->use_asize) {
		offs_out->off[AsmInstOffPrAsize] = (uint8_t)(data - buf);
		*data++ = ASIZE;
	}
	if (desc->use_osize) {
		offs_out->off[AsmInstOffPrOsize] = (uint8_t)(data - buf);
		*data++ = OSIZE;
	}

	if (desc->use_rex) {
		offs_out->off[AsmInstOffPrRex] = (uint8_t)(data - buf);

		uint8_t rex = REX_BASE;

		if (desc->ext_b) {
			rex |= REX_B;
		}
		if (desc->ext_x) {
			rex |= REX_X;
		}
		if (desc->ext_r) {
			rex |= REX_R;
		}
		if (desc->ext_w) {
			rex |= REX_W;
		}

		*data++ = rex;
	}

	if (desc->use_0f) {
		offs_out->off[AsmInstOffPr0f] = (uint8_t)(data - buf);

		*data++ = 0x0F;
	}

	if (desc->use_opc) {
		offs_out->off[AsmInstOffOpc] = (uint8_t)(data - buf);

		*data++ = desc->opc | (desc->opc_reg & 0b111);
	}

	if (desc->use_modrm) {
		offs_out->off[AsmInstOffModrm] = (uint8_t)(data - buf);

		*data++ = (desc->modrm_rm & 0b111) | (desc->modrm_reg & 0b111) << 3 | (desc->modrm_mod & 0b11) << 6;
	}

	if (desc->use_sib) {
		offs_out->off[AsmInstOffSib] = (uint8_t)(data - buf);

		*data++ = (desc->sib_base & 0b111) | (desc->sib_index & 0b111) << 3 | (desc->sib_scale & 0b11) << 6;
	}

	if (desc->disp_size != AsmSizeNone) {
		offs_out->off[AsmInstOffDisp] = (uint8_t)(data - buf);

		uint8_t * var = (uint8_t *)&desc->disp;
		switch (desc->disp_size) {
			case AsmSize32:
				*data++ = *var++;
				*data++ = *var++;
			case AsmSize16:
				*data++ = *var++;
			case AsmSize8:
				*data++ = *var++;
				break;
			default:
				__debugbreak();
				break;
		}
	}

	if (desc->imm0_size != AsmSizeNone) {
		offs_out->off[AsmInstOffImm] = (uint8_t)(data - buf);

		uint8_t * var = (uint8_t *)&desc->imm0;
		switch (desc->imm0_size) {
			case AsmSize64:
				*data++ = *var++;
				*data++ = *var++;
				*data++ = *var++;
				*data++ = *var++;
			case AsmSize32:
				*data++ = *var++;
				*data++ = *var++;
			case AsmSize16:
				*data++ = *var++;
			case AsmSize8:
				*data++ = *var++;
				break;
			default:
				__debugbreak();
				break;
		}
	}

	size_t size = (uint8_t)(data - buf);

	if (size >= BS_BUF_SIZE) {
		exit(EXIT_FAILURE);
	}

	if (size >= ASM_INST_BS_SIZE) {
		return false;
	}

	memcpy(bs_out, buf, size);
	*inst_size = size;

	return true;
}

bool asm_inst_build(asm_inst_t * inst, size_t * inst_size, asm_inst_bs_t bs_out, asm_inst_offs_t * offs_out) {
	if (!validate_inst(inst)) {
		return false;
	}

	const inst_rec_t * rec = find_rec(inst);

	if (rec == NULL) {
		return false;
	}

	inst_desc_t desc;

	make_desc(&desc, inst, rec);

	asm_inst_offs_t offs;

	if (!build_desc(&desc, inst_size, bs_out, &offs)) {
		return false;
	}

	if (offs_out != NULL) {
		*offs_out = offs;
	}

	return true;
}

const asm_inst_opd_t asm_inst_opds_to_opd[AsmInstOpds__Count][ASM_INST_OPDS_SIZE] = {
	[AsmInstOpds_None] = { AsmInstOpdNone, AsmInstOpdNone, AsmInstOpdNone },
	[AsmInstOpds_Label] = { AsmInstOpdLabel, AsmInstOpdNone, AsmInstOpdNone },
	[AsmInstOpds_Imm] = { AsmInstOpdImm, AsmInstOpdNone, AsmInstOpdNone },
	[AsmInstOpds_Reg] = { AsmInstOpdReg, AsmInstOpdNone, AsmInstOpdNone },
	[AsmInstOpds_Mem] = { AsmInstOpdMem, AsmInstOpdNone, AsmInstOpdNone },
	[AsmInstOpds_Reg_Imm] = { AsmInstOpdReg, AsmInstOpdImm, AsmInstOpdNone },
	[AsmInstOpds_Mem_Imm] = { AsmInstOpdMem, AsmInstOpdImm, AsmInstOpdNone },
	[AsmInstOpds_Reg_Reg] = { AsmInstOpdReg, AsmInstOpdReg, AsmInstOpdNone },
	[AsmInstOpds_Reg_Mem] = { AsmInstOpdReg, AsmInstOpdMem, AsmInstOpdNone },
	[AsmInstOpds_Mem_Reg] = { AsmInstOpdMem, AsmInstOpdReg, AsmInstOpdNone },
	[AsmInstOpds_Reg_Reg_Imm] = { AsmInstOpdReg, AsmInstOpdReg, AsmInstOpdImm },
	[AsmInstOpds_Reg_Mem_Imm] = { AsmInstOpdReg, AsmInstOpdMem, AsmInstOpdImm },
};
const asm_size_t asm_inst_imm_type_to_size[AsmInstImm_Count] = {
	[AsmInstImmNone] = AsmSizeNone,
	[AsmInstImm8] = AsmSize8,
	[AsmInstImm16] = AsmSize16,
	[AsmInstImm32] = AsmSize32,
	[AsmInstImm64] = AsmSize64,
	[AsmInstImmLabelRel8] = AsmSize8,
	[AsmInstImmLabelRel32] = AsmSize32,
	[AsmInstImmLabelVa64] = AsmSize64,
	[AsmInstImmLabelRva32] = AsmSize32
};
const asm_inst_imm_type_t asm_inst_imm_size_to_type[AsmSize_Count] = {
	[AsmSize8] = AsmInstImm8,
	[AsmSize16] = AsmInstImm16,
	[AsmSize32] = AsmInstImm32,
	[AsmSize64] = AsmInstImm64,
};
const asm_size_t asm_inst_disp_type_to_size[AsmInstDisp_Count] = {
	[AsmInstDispAuto] = AsmSizeNone,
	[AsmInstDispNone] = AsmSizeNone,
	[AsmInstDisp8] = AsmSize8,
	[AsmInstDisp32] = AsmSize32,
	[AsmInstDispLabelRel8] = AsmSize8,
	[AsmInstDispLabelRel32] = AsmSize32,
	[AsmInstDispLabelRva32] = AsmSize32
};

//base
#define inst(type_, opc_, opds_, ...) { .type = AsmInst##type_, .opc = opc_, .opds = AsmInstOpds_##opds_, __VA_ARGS__ }

//data
#define d_reg(i, rt) .reg##i##_type = InstRecReg##rt
#define d_imm(i, it) .imm##i##_type = InstRec##it
#define d_mem(i, ms) .mem##i##_size = Asm##ms
#define d_modrm_reg(digit) .modrm_reg_digit = digit
#define d_allow_lock .allow_lock = 1
#define d_use_osize .use_osize = 1
#define d_use_0f .use_0f = 1
#define d_ext_w .ext_w = 1
#define d_no_opc .no_opc = 1

//quick inst
#define q_none(type_, opc_, ...)							inst(type_, opc_, None, __VA_ARGS__)
#define q_label(type_, opc_, ...)							inst(type_, opc_, Label, __VA_ARGS__)
#define q_imm(type_, opc_, it0, ...)						inst(type_, opc_, Imm, d_imm(0, it0), __VA_ARGS__)
#define q_reg(type_, opc_, rt0, ...)						inst(type_, opc_, Reg, d_reg(0, rt0), __VA_ARGS__)
#define q_mem(type_, opc_, ms0, ...)						inst(type_, opc_, Mem, d_mem(0, ms0), __VA_ARGS__)
#define q_reg_imm(type_, opc_, rt0, it0, ...)				inst(type_, opc_, Reg_Imm, d_reg(0, rt0), d_imm(0, it0), __VA_ARGS__)
#define q_mem_imm(type_, opc_, ms0, it0, ...)				inst(type_, opc_, Mem_Imm, d_mem(0, ms0), d_imm(0, it0), __VA_ARGS__)
#define q_reg_reg(type_, opc_, rt0, rt1, ...)				inst(type_, opc_, Reg_Reg, d_reg(0, rt0), d_reg(1, rt1), __VA_ARGS__)
#define q_reg_mem(type_, opc_, rt0, ms0, ...)				inst(type_, opc_, Reg_Mem, d_reg(0, rt0), d_mem(0, ms0), __VA_ARGS__)
#define q_mem_reg(type_, opc_, ms0, rt0, ...)				inst(type_, opc_, Mem_Reg, d_mem(0, ms0), d_reg(0, rt0), __VA_ARGS__)
#define q_reg_reg_imm(type_, opc_, rt0, rt1, it0, ...)		inst(type_, opc_, Reg_Reg, d_reg(0, rt0), d_reg(1, rt1), d_imm(0, it0), __VA_ARGS__)
#define q_reg_mem_imm(type_, opc_, rt0, ms0, it0, ...)		inst(type_, opc_, Reg_Mem_Imm, d_reg(0, rt0), d_mem(0, ms0), d_imm(0, it0), __VA_ARGS__)

#define q_func_wdq(func, type_, opc0, o0, ...)\
	func(type_, opc0, o0##16, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, __VA_ARGS__),\
	func(type_, opc0, o0##64, d_ext_w, __VA_ARGS__)
#define q_func_bwdq(func, type_, opc0, opc1, o0, ...)\
	func(type_, opc0, o0##8, __VA_ARGS__),\
	q_func_wdq(func, type_, opc1, o0, __VA_ARGS__)
#define q_func_wdq_ccc(func, type_, opc0, o0, o1, ...)\
	func(type_, opc0, o0##16, o1, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, o1, __VA_ARGS__),\
	func(type_, opc0, o0##64, o1, d_ext_w, __VA_ARGS__)
#define q_func_bwdq_cccc(func, type_, opc0, opc1, o0, o1, ...)\
	func(type_, opc0, o0##8, o1, __VA_ARGS__),\
	q_func_wdq_ccc(func, type_, opc1, o0, o1, __VA_ARGS__)
#define q_func_wdq_wdq(func, type_, opc0, o0, o1, ...)\
	func(type_, opc0, o0##16, o1##16, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, o1##32, __VA_ARGS__),\
	func(type_, opc0, o0##64, o1##64, d_ext_w, __VA_ARGS__)
#define q_func_bwdq_bwdq(func, type_, opc0, opc1, o0, o1, ...)\
	func(type_, opc0, o0##8, o1##8, __VA_ARGS__),\
	q_func_wdq_wdq(func, type_, opc1, o0, o1, __VA_ARGS__)
#define q_func_wdq_wdd(func, type_, opc0, o0, o1, ...)\
	func(type_, opc0, o0##16, o1##16, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, o1##32, __VA_ARGS__),\
	func(type_, opc0, o0##64, o1##32, d_ext_w, __VA_ARGS__)
#define q_func_bwdq_bwdd(func, type_, opc0, opc1, o0, o1, ...)\
	func(type_, opc0, o0##8, o1##8, __VA_ARGS__), \
	q_func_wdq_wdd(func, type_, opc1, o0, o1, __VA_ARGS__)
#define q_func_wdq_bbb(func, type_, opc0, o0, o1, ...)\
	func(type_, opc0, o0##16, o1##8, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, o1##8, __VA_ARGS__),\
	func(type_, opc0, o0##64, o1##8, d_ext_w, __VA_ARGS__)
#define q_func_bwdq_bbbb(func, type_, opc0, opc1, o0, o1, ...)\
	func(type_, opc0, o0##8, o1##8, __VA_ARGS__), \
	q_func_wdq_bbb(func, type_, opc1, o0, o1, __VA_ARGS__)
#define q_func_wdq_wdq_bbb(func, type_, opc0, o0, o1, o2, ...)\
	func(type_, opc0, o0##16, o1##16, o2##8, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, o1##32, o2##8, __VA_ARGS__),\
	func(type_, opc0, o0##64, o1##64, o2##8, d_ext_w, __VA_ARGS__)
#define q_func_wdq_wdq_wdd(func, type_, opc0, o0, o1, o2, ...)\
	func(type_, opc0, o0##16, o1##16, o2##16, d_use_osize, __VA_ARGS__),\
	func(type_, opc0, o0##32, o1##32, o2##32, __VA_ARGS__),\
	func(type_, opc0, o0##64, o1##64, o2##32, d_ext_w, __VA_ARGS__)

#define q_g0(name, i)\
	q_func_bwdq_bwdq(q_reg_reg, name, 0x00 + (0x08 * i), 0x01 + (0x08 * i), GprRm, GprReg),\
	q_func_bwdq_bwdq(q_mem_reg, name, 0x00 + (0x08 * i), 0x01 + (0x08 * i), Size, GprReg, d_allow_lock),\
	q_func_bwdq_bwdq(q_reg_reg, name, 0x02 + (0x08 * i), 0x03 + (0x08 * i), GprReg, GprRm),\
	q_func_bwdq_bwdq(q_reg_mem, name, 0x02 + (0x08 * i), 0x03 + (0x08 * i), GprReg, Size),\
	q_func_bwdq_bwdd(q_reg_imm, name, 0x04 + (0x08 * i), 0x05 + (0x08 * i), Ax, Imm),\
	q_func_bwdq_bwdd(q_reg_imm, name, 0x80, 0x81, GprRm, Imm, d_modrm_reg(i)),\
	q_func_bwdq_bwdd(q_mem_imm, name, 0x80, 0x81, Size, Imm, d_modrm_reg(i)),\
	q_func_wdq_bbb(q_reg_imm, name, 0x83, GprRm, Imm, d_modrm_reg(i)),\
	q_func_wdq_bbb(q_mem_imm, name, 0x83, Size, Imm, d_modrm_reg(i))
#define q_g1(name, i)\
	q_imm(name, 0x70 + i, Imm8),\
	q_imm(name, 0x80 + i, Imm32, d_use_0f)
#define q_g2(name, i)\
	q_mem(name, 0x90 + i, Size8, d_modrm_reg(0), d_use_0f),\
	q_reg(name, 0x90 + i, GprRm8, d_modrm_reg(0), d_use_0f)
#define q_g3(name, i)\
	q_func_bwdq_cccc(q_reg_imm, name, 0xD0, 0xD1, GprRm, Imm8_1, d_modrm_reg(i)),\
	q_func_bwdq_cccc(q_mem_imm, name, 0xD0, 0xD1, Size, Imm8_1, d_modrm_reg(i)),\
	q_func_bwdq_cccc(q_reg_reg, name, 0xD2, 0xD3, GprRm, Cx8, d_modrm_reg(i)),\
	q_func_bwdq_cccc(q_mem_reg, name, 0xD2, 0xD3, Size, Cx8, d_modrm_reg(i)),\
	q_func_bwdq_bbbb(q_reg_imm, name, 0xC0, 0xC1, GprRm, Imm, d_modrm_reg(i)),\
	q_func_bwdq_bbbb(q_mem_imm, name, 0xC0, 0xC1, Size, Imm, d_modrm_reg(i))

static const inst_rec_t inst_recs[] = {
	q_imm(Data, 0x00, Imm8, d_no_opc),
	q_imm(Data, 0x00, Imm16, d_no_opc),
	q_imm(Data, 0x00, Imm32, d_no_opc),
	q_imm(Data, 0x00, Imm64, d_no_opc),

	q_g0(Add, 0),
	q_g0(Or, 1),
	q_g0(Adc, 2),
	q_g0(Sbb, 3),
	q_g0(And, 4),
	q_g0(Sub, 5),
	q_g0(Xor, 6),
	q_g0(Cmp, 7),

	q_func_bwdq_bwdq(q_reg_reg, Mov, 0x88, 0x89, GprRm, GprReg),
	q_func_bwdq_bwdq(q_mem_reg, Mov, 0x88, 0x89, Size, GprReg),
	q_func_bwdq_bwdq(q_reg_reg, Mov, 0x8A, 0x8B, GprReg, GprRm),
	q_func_bwdq_bwdq(q_reg_mem, Mov, 0x8A, 0x8B, GprReg, Size),
	q_func_bwdq_bwdq(q_reg_imm, Mov, 0xB0, 0xB8, GprOpc, Imm),
	q_func_bwdq_bwdd(q_reg_imm, Mov, 0xC6, 0xC7, GprRm, Imm, d_modrm_reg(0)),
	q_func_bwdq_bwdd(q_mem_imm, Mov, 0xC6, 0xC7, Size, Imm, d_modrm_reg(0)),

	q_func_wdq_bbb(q_reg_reg, Movsx, 0xBE, GprReg, GprRm, d_use_0f),
	q_func_wdq_bbb(q_reg_mem, Movsx, 0xBE, GprReg, Size, d_use_0f),
	q_reg_reg(Movsx, 0xBF, GprReg32, GprRm16, d_use_0f),
	q_reg_mem(Movsx, 0xBF, GprReg32, Size16, d_use_0f),
	q_reg_reg(Movsx, 0xBF, GprReg64, GprRm16, d_ext_w, d_use_0f),
	q_reg_mem(Movsx, 0xBF, GprReg64, Size16, d_ext_w, d_use_0f),

	q_reg_reg(Movsxd, 0x63, GprReg64, GprRm32, d_ext_w),
	q_reg_mem(Movsxd, 0x63, GprReg64, Size32, d_ext_w),

	q_func_wdq_bbb(q_reg_reg, Movzx, 0xB6, GprReg, GprRm, d_use_0f),
	q_func_wdq_bbb(q_reg_mem, Movzx, 0xB6, GprReg, Size, d_use_0f),
	q_reg_reg(Movzx, 0xB7, GprReg32, GprRm16, d_use_0f),
	q_reg_mem(Movzx, 0xB7, GprReg32, Size16, d_use_0f),
	q_reg_reg(Movzx, 0xB7, GprReg64, GprRm16, d_ext_w, d_use_0f),
	q_reg_mem(Movzx, 0xB7, GprReg64, Size16, d_ext_w, d_use_0f),

	q_reg_mem(Lea, 0x8D, GprReg16, SizeNone, d_use_osize),
	q_reg_mem(Lea, 0x8D, GprReg32, SizeNone),
	q_reg_mem(Lea, 0x8D, GprReg64, SizeNone, d_ext_w),

	q_imm(Call, 0xE8, Imm32),
	q_mem(Call, 0xFF, SizeNone, d_modrm_reg(2)),
	q_reg(Call, 0xFF, GprRm64, d_modrm_reg(2)),

	q_none(Ret, 0xC3),
	q_imm(Ret, 0xC2, Imm16),

	q_none(Int3, 0xCC),

	q_imm(Jmp, 0xEB, Imm8),
	q_imm(Jmp, 0xE9, Imm32),
	q_mem(Jmp, 0xFF, SizeNone, d_modrm_reg(4)),
	q_reg(Jmp, 0xFF, GprRm64, d_modrm_reg(4)),

	q_g1(Jo, 0),
	q_g1(Jno, 1),
	q_g1(Jc, 2),
	q_g1(Jnc, 3),
	q_g1(Jz, 4),
	q_g1(Jnz, 5),
	q_g1(Jbe, 6),
	q_g1(Jnbe, 7),
	q_g1(Js, 8),
	q_g1(Jns, 9),
	q_g1(Jp, 10),
	q_g1(Jnp, 11),
	q_g1(Jl, 12),
	q_g1(Jnl, 13),
	q_g1(Jle, 14),
	q_g1(Jnle, 15),

	q_func_bwdq_bwdd(q_reg_imm, Test, 0xA8, 0xA9, Ax, Imm),
	q_func_bwdq_bwdd(q_reg_imm, Test, 0xF6, 0xF7, GprRm, Imm, d_modrm_reg(0)),
	q_func_bwdq_bwdd(q_mem_imm, Test, 0xF6, 0xF7, Size, Imm, d_modrm_reg(0)),
	q_func_bwdq_bwdq(q_reg_reg, Test, 0x84, 0x85, GprRm, GprReg),
	q_func_bwdq_bwdq(q_mem_reg, Test, 0x84, 0x85, Size, GprReg),

	q_func_bwdq(q_reg, Not, 0xF6, 0xF7, GprRm, d_modrm_reg(2)),
	q_func_bwdq(q_mem, Not, 0xF6, 0xF7, Size, d_modrm_reg(2), d_allow_lock),

	q_func_bwdq(q_reg, Neg, 0xF6, 0xF7, GprRm, d_modrm_reg(3)),
	q_func_bwdq(q_mem, Neg, 0xF6, 0xF7, Size, d_modrm_reg(3), d_allow_lock),

	q_func_bwdq(q_reg, Mul, 0xF6, 0xF7, GprRm, d_modrm_reg(4)),
	q_func_bwdq(q_mem, Mul, 0xF6, 0xF7, Size, d_modrm_reg(4)),

	q_func_bwdq(q_reg, Imul, 0xF6, 0xF7, GprRm, d_modrm_reg(5)),
	q_func_bwdq(q_mem, Imul, 0xF6, 0xF7, Size, d_modrm_reg(5)),
	q_func_wdq_wdq(q_reg_reg, Imul, 0xAF, GprReg, GprRm, d_use_0f),
	q_func_wdq_wdq(q_reg_mem, Imul, 0xAF, GprReg, Size, d_use_0f),
	q_func_wdq_wdq_bbb(q_reg_reg_imm, Imul, 0x6B, GprReg, GprRm, Imm),
	q_func_wdq_wdq_bbb(q_reg_mem_imm, Imul, 0x6B, GprReg, Size, Imm),
	q_func_wdq_wdq_wdd(q_reg_reg_imm, Imul, 0x69, GprReg, GprRm, Imm),
	q_func_wdq_wdq_wdd(q_reg_mem_imm, Imul, 0x69, GprReg, Size, Imm),

	q_func_bwdq(q_reg, Div, 0xF6, 0xF7, GprRm, d_modrm_reg(6)),
	q_func_bwdq(q_mem, Div, 0xF6, 0xF7, Size, d_modrm_reg(6)),

	q_func_bwdq(q_reg, Idiv, 0xF6, 0xF7, GprRm, d_modrm_reg(7)),
	q_func_bwdq(q_mem, Idiv, 0xF6, 0xF7, Size, d_modrm_reg(7)),

	q_none(Cbw, 0x98, d_use_osize),
	q_none(Cwde, 0x98),
	q_none(Cdqe, 0x98, d_ext_w),

	q_none(Cwd, 0x99, d_use_osize),
	q_none(Cdq, 0x99),
	q_none(Cqo, 0x99, d_ext_w),

	q_g2(Seto, 0),
	q_g2(Setno, 1),
	q_g2(Setb, 2),
	q_g2(Setnb, 3),
	q_g2(Setz, 4),
	q_g2(Setnz, 5),
	q_g2(Setbe, 6),
	q_g2(Setnbe, 7),
	q_g2(Sets, 8),
	q_g2(Setns, 9),
	q_g2(Setp, 10),
	q_g2(Setnp, 11),
	q_g2(Setl, 12),
	q_g2(Setnl, 13),
	q_g2(Setle, 14),
	q_g2(Setnle, 15),

	q_g3(Rol, 0),
	q_g3(Ror, 1),
	q_g3(Rcl, 2),
	q_g3(Rcr, 3),
	q_g3(Shl, 4),
	q_g3(Shr, 5),
	q_g3(Sar, 7),
};
static const size_t inst_recs_size = _countof(inst_recs);
