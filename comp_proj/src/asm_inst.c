#include "pch.h"
#include "asm_inst.h"
#include "asm_size.h"
#include "asm_reg.h"
#include "u_assert.h"

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
	InstRecOpds_RmReg_RegReg,
	InstRecOpds_RegReg_RmReg,
	InstRecOpds_RmMem_RegReg,
	InstRecOpds_RegReg_RmMem,
	InstRecOpds_RegReg_RmReg_Imm,
	InstRecOpds_RegReg_RmMem_Imm,
	InstRecOpds__Count
};
typedef uint8_t inst_rec_opds_enc_t;

typedef struct inst_rec {
	asm_inst_type_t type;
	uint8_t opc;
	inst_rec_opds_enc_t opds_enc;
	asm_reg_grps_t reg0_grps, reg1_grps;
	asm_size_t imm0_size, mem_size;
	uint8_t modrm_reg_digit : 3;
	uint8_t no_opc : 1;
	uint8_t allow_lock : 1;
	uint8_t use_osize : 1;
	uint8_t use_0f : 1;
	uint8_t ext_w : 1;
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

static const inst_rec_opds_enc_t inst_to_rec_opds[InstRecOpds__Count];

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

		if (!asm_size_is_int(inst->mem_scale)) {
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

		if (!asm_size_is_int(inst->mem_scale)) {
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
			u_assert_switch(inst->opds);
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

static bool is_imm0_match(asm_inst_t * inst, const inst_rec_t * rec) {
	if (asm_inst_imm_type_to_size[inst->imm0_type] != rec->imm0_size) {
		return false;
	}

	return true;
}
static bool is_reg_match(asm_reg_t reg, const asm_reg_grps_t * rec) {
	if (!asm_reg_check_grps(&asm_reg_infos[reg].grps, rec)) {
		return false;
	}

	return true;
}
static bool is_mem_match(asm_inst_t * inst, const inst_rec_t * rec) {
	if (rec->mem_size == AsmSizeNone) {
		return true;
	}

	if (inst->mem_size != rec->mem_size) {
		return false;
	}

	return true;
}
static bool is_opds_match(asm_inst_t * inst, const inst_rec_t * rec) {
	if (inst->opds != inst_to_rec_opds[rec->opds_enc]) {
		return false;
	}

	switch (rec->opds_enc) {
		case InstRecOpds_None:
		case InstRecOpds_Label:
			break;
		case InstRecOpds_Imm:
			if (!is_imm0_match(inst, rec)) {
				return false;
			}
			break;
		case InstRecOpds_RmReg:
			if (!is_reg_match(inst->reg0, &rec->reg0_grps)) {
				return false;
			}
			break;
		case InstRecOpds_RmMem:
			if (!is_mem_match(inst, rec)) {
				return false;
			}
			break;
		case InstRecOpds_ImplReg_Imm:
		case InstRecOpds_OpcReg_Imm:
		case InstRecOpds_RmReg_Imm:
			if (!is_reg_match(inst->reg0, &rec->reg0_grps)
				|| !is_imm0_match(inst, rec)) {
				return false;
			}
			break;
		case InstRecOpds_RmMem_Imm:
			if (!is_mem_match(inst, rec)
				|| !is_imm0_match(inst, rec)) {
				return false;
			}
			break;
		case InstRecOpds_RmReg_RegReg:
		case InstRecOpds_RegReg_RmReg:
			if (!is_reg_match(inst->reg0, &rec->reg0_grps)
				|| !is_reg_match(inst->reg1, &rec->reg1_grps)) {
				return false;
			}
			break;
		case InstRecOpds_RmMem_RegReg:
		case InstRecOpds_RegReg_RmMem:
			if (!is_reg_match(inst->reg0, &rec->reg0_grps)
				|| !is_mem_match(inst, rec)) {
				return false;
			}
			break;
		case InstRecOpds_RegReg_RmReg_Imm:
			if (!is_reg_match(inst->reg0, &rec->reg0_grps)
				|| !is_reg_match(inst->reg1, &rec->reg1_grps)
				|| !is_imm0_match(inst, rec)) {
				return false;
			}
			break;
		case InstRecOpds_RegReg_RmMem_Imm:
			if (!is_reg_match(inst->reg0, &rec->reg0_grps)
				|| !is_mem_match(inst, rec)
				|| !is_imm0_match(inst, rec)) {
				return false;
			}
			break;
		default:
			u_assert_switch(rec->opds_enc);
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

static void make_desc_imm0(inst_desc_t * desc, asm_inst_t * inst, const inst_rec_t * rec) {
	desc->imm0_size = rec->imm0_size;

	if (is_imm_value(inst->imm0_type)) {
		desc->imm0 = inst->imm0;
	}
	else {
		desc->imm0 = 0;
	}
}
static void make_desc_opc_reg(inst_desc_t * desc, asm_reg_t reg) {
	const asm_reg_info_t * info = &asm_reg_infos[reg];

	desc->opc_reg = info->enc;
	desc->ext_b = info->ext;

	desc->req_rex = desc->req_rex || info->grps.req_rex;
	desc->no_rex = desc->no_rex || info->grps.no_rex;
}
static void make_desc_modrm_rm_reg(inst_desc_t * desc, asm_reg_t reg) {
	const asm_reg_info_t * info = &asm_reg_infos[reg];

	desc->use_modrm = true;

	desc->modrm_mod = 0b11;
	desc->modrm_rm = info->enc;
	desc->ext_b = info->ext;

	desc->req_rex = desc->req_rex || info->grps.req_rex;
	desc->no_rex = desc->no_rex || info->grps.no_rex;
}
static void make_desc_modrm_reg_reg(inst_desc_t * desc, asm_reg_t reg) {
	const asm_reg_info_t * info = &asm_reg_infos[reg];

	desc->use_modrm = true;

	desc->modrm_reg = info->enc;
	desc->ext_r = info->ext;

	desc->req_rex = desc->req_rex || info->grps.req_rex;
	desc->no_rex = desc->no_rex || info->grps.no_rex;
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

		u_assert(desc->disp_size != AsmSizeNone || base_info->enc != 0b101);
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
			u_assert_switch(desc->disp_size);
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
			u_assert_switch(scale);
	}
}
static void make_desc_modrm_rm_mem(inst_desc_t * desc, asm_inst_t * inst, const inst_rec_t * rec) {
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
		u_assert(is_disp_32(inst->mem_disp_type));

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
		u_assert(is_disp_32(inst->mem_disp_type));

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
		u_assert_switch(base && index);
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

	switch (rec->opds_enc) {
		case InstRecOpds_None:
		case InstRecOpds_Label:
			break;
		case InstRecOpds_Imm:
			make_desc_imm0(desc, inst, rec);
			break;
		case InstRecOpds_RmReg:
			make_desc_modrm_rm_reg(desc, inst->reg0);
			break;
		case InstRecOpds_RmMem:
			make_desc_modrm_rm_mem(desc, inst, rec);
			break;
		case InstRecOpds_ImplReg_Imm:
			make_desc_imm0(desc, inst, rec);
			break;
		case InstRecOpds_OpcReg_Imm:
			make_desc_opc_reg(desc, inst->reg0);
			make_desc_imm0(desc, inst, rec);
			break;
		case InstRecOpds_RmReg_Imm:
			make_desc_modrm_rm_reg(desc, inst->reg0);
			make_desc_imm0(desc, inst, rec);
			break;
		case InstRecOpds_RmMem_Imm:
			make_desc_modrm_rm_mem(desc, inst, rec);
			make_desc_imm0(desc, inst, rec);
			break;
		case InstRecOpds_RmReg_RegReg:
			make_desc_modrm_rm_reg(desc, inst->reg0);
			make_desc_modrm_reg_reg(desc, inst->reg1);
			break;
		case InstRecOpds_RegReg_RmReg:
			make_desc_modrm_reg_reg(desc, inst->reg0);
			make_desc_modrm_rm_reg(desc, inst->reg1);
			break;
		case InstRecOpds_RmMem_RegReg:
			make_desc_modrm_rm_mem(desc, inst, rec);
			make_desc_modrm_reg_reg(desc, inst->reg0);
			break;
		case InstRecOpds_RegReg_RmMem:
			make_desc_modrm_reg_reg(desc, inst->reg0);
			make_desc_modrm_rm_mem(desc, inst, rec);
			break;
		case InstRecOpds_RegReg_RmReg_Imm:
			make_desc_modrm_reg_reg(desc, inst->reg0);
			make_desc_modrm_rm_reg(desc, inst->reg1);
			make_desc_imm0(desc, inst, rec);
			break;
		case InstRecOpds_RegReg_RmMem_Imm:
			make_desc_modrm_reg_reg(desc, inst->reg0);
			make_desc_modrm_rm_mem(desc, inst, rec);
			make_desc_imm0(desc, inst, rec);
			break;
		default:
			u_assert_switch(inst->opds);
	}

	if (desc->ext_b || desc->ext_x || desc->ext_r || desc->ext_w || desc->req_rex) {
		u_assert(!desc->no_rex);

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

static const inst_rec_opds_enc_t inst_to_rec_opds[InstRecOpds__Count] = {
	[InstRecOpds_None] = AsmInstOpds_None,
	[InstRecOpds_Label] = AsmInstOpds_Label,
	[InstRecOpds_Imm] = AsmInstOpds_Imm,
	[InstRecOpds_RmReg] = AsmInstOpds_Reg,
	[InstRecOpds_RmMem] = AsmInstOpds_Mem,
	[InstRecOpds_ImplReg_Imm] = AsmInstOpds_Reg_Imm,
	[InstRecOpds_OpcReg_Imm] = AsmInstOpds_Reg_Imm,
	[InstRecOpds_RmReg_Imm] = AsmInstOpds_Reg_Imm,
	[InstRecOpds_RmMem_Imm] = AsmInstOpds_Mem_Imm,
	[InstRecOpds_RmReg_RegReg] = AsmInstOpds_Reg_Reg,
	[InstRecOpds_RegReg_RmReg] = AsmInstOpds_Reg_Reg,
	[InstRecOpds_RmMem_RegReg] = AsmInstOpds_Mem_Reg,
	[InstRecOpds_RegReg_RmMem] = AsmInstOpds_Reg_Mem,
	[InstRecOpds_RegReg_RmReg_Imm] = AsmInstOpds_Reg_Reg_Imm,
	[InstRecOpds_RegReg_RmMem_Imm] = AsmInstOpds_Reg_Mem_Imm
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
const asm_size_t asm_inst_disp_type_to_size[AsmInstDisp_Count] = {
	[AsmInstDispAuto] = AsmSizeNone,
	[AsmInstDispNone] = AsmSizeNone,
	[AsmInstDisp8] = AsmSize8,
	[AsmInstDisp32] = AsmSize32,
	[AsmInstDispLabelRel8] = AsmSize8,
	[AsmInstDispLabelRel32] = AsmSize32,
	[AsmInstDispLabelRva32] = AsmSize32
};

enum {
	asm_size_n = AsmSizeNone,
	asm_size_8 = AsmSize8,
	asm_size_16 = AsmSize16,
	asm_size_32 = AsmSize32,
	asm_size_64 = AsmSize64
};

//base
#define inst(type_, opc_, opds_enc_, ...) { .type = AsmInst##type_, .opc = opc_, .opds_enc = InstRecOpds_##opds_enc_, __VA_ARGS__ }

//data
#define d_reg(i, ...) .reg##i##_grps = { __VA_ARGS__ }
#define d_imm(i, size) .imm##i##_size = asm_size_##size
#define d_mem(size) .mem_size = asm_size_##size
#define d_modrm_reg(digit) .modrm_reg_digit = digit
#define d_allow_lock .allow_lock = 1
#define d_use_osize .use_osize = 1
#define d_use_0f .use_0f = 1
#define d_ext_w .ext_w = 1
#define d_no_opc .no_opc = 1

//reg groups
#define rg_gpr(size) .gpr = 1, .s_##size = 1
#define rg_ax(size) rg_gpr(size), .gpr_ax = 1

//quick inst
#define q_none(type_, opc_, ...)						inst(type_, opc_, None, __VA_ARGS__)
#define q_label(type_, opc_, ...)						inst(type_, opc_, Label, __VA_ARGS__)
#define q_imm(type_, opc_, i, ...)						inst(type_, opc_, Imm, d_imm(0, i), __VA_ARGS__)
#define q_rmreg(type_, opc_, r, ...)					inst(type_, opc_, RmReg, d_reg(0, r), __VA_ARGS__)
#define q_rmmem(type_, opc_, m, ...)					inst(type_, opc_, RmMem, d_mem(m), __VA_ARGS__)
#define q_implreg_imm(type_, opc_, r, i, ...)			inst(type_, opc_, ImplReg_Imm, d_reg(0, r), d_imm(0, i), __VA_ARGS__)
#define q_opcreg_imm(type_, opc_, r, i, ...)			inst(type_, opc_, OpcReg_Imm, d_reg(0, r), d_imm(0, i), __VA_ARGS__)
#define q_rmreg_imm(type_, opc_, r, i, ...)				inst(type_, opc_, RmReg_Imm, d_reg(0, r), d_imm(0, i), __VA_ARGS__)
#define q_rmmem_imm(type_, opc_, m, i, ...)				inst(type_, opc_, RmMem_Imm, d_mem(m), d_imm(0, i), __VA_ARGS__)
#define q_rmreg_regreg(type_, opc_, r0, r1, ...)		inst(type_, opc_, RmReg_RegReg, d_reg(0, r0), d_reg(1, r1), __VA_ARGS__)
#define q_rmmem_regreg(type_, opc_, m, r, ...)			inst(type_, opc_, RmMem_RegReg, d_mem(m), d_reg(0, r), __VA_ARGS__)
#define q_regreg_rmreg(type_, opc_, r0, r1, ...)		inst(type_, opc_, RegReg_RmReg, d_reg(0, r0), d_reg(1, r1), __VA_ARGS__)
#define q_regreg_rmmem(type_, opc_, r, m, ...)			inst(type_, opc_, RegReg_RmMem, d_reg(0, r), d_mem(m), __VA_ARGS__)
#define q_regreg_rmreg_imm(type_, opc_, r0, r1, i, ...)	inst(type_, opc_, RegReg_RmReg_Imm, d_reg(0, r0), d_reg(1, r1), d_imm(0, i), __VA_ARGS__)
#define q_regreg_rmmem_imm(type_, opc_, r, m, i, ...)	inst(type_, opc_, RegReg_RmReg_Imm, d_reg(0, r), d_mem(m), d_imm(0, i), __VA_ARGS__)

#define q_gpr_rmreg(type_, opc_, r, ...)						q_rmreg(type_, opc_, rg_gpr(r), __VA_ARGS__)
#define q_gpr_ax_imm(type_, opc_, r, i, ...)					q_implreg_imm(type_, opc_, rg_ax(r), i, __VA_ARGS__)
#define q_gpr_opcreg_imm(type_, opc_, r, i, ...)				q_opcreg_imm(type_, opc_, rg_gpr(r), i, __VA_ARGS__)
#define q_gpr_rmreg_imm(type_, opc_, r, i, ...)					q_rmreg_imm(type_, opc_, rg_gpr(r), i, __VA_ARGS__)
#define q_gpr_rmreg_gpr_regreg(type_, opc_, r0, r1, ...)		q_rmreg_regreg(type_, opc_, rg_gpr(r0), rg_gpr(r1), __VA_ARGS__)
#define q_rmmem_gpr_regreg(type_, opc_, m, r, ...)				q_rmmem_regreg(type_, opc_, m, rg_gpr(r), __VA_ARGS__)
#define q_gpr_regreg_gpr_rmreg(type_, opc_, r0, r1, ...)		q_regreg_rmreg(type_, opc_, rg_gpr(r0), rg_gpr(r1), __VA_ARGS__)
#define q_gpr_regreg_rmmem(type_, opc_, r, m, ...)				q_regreg_rmmem(type_, opc_, rg_gpr(r), m, __VA_ARGS__)
#define q_gpr_regreg_gpr_rmreg_imm(type_, opc_, r0, r1, i, ...)	q_regreg_rmreg_imm(type_, opc_, rg_gpr(r0), rg_gpr(r1), i, __VA_ARGS__)
#define q_gpr_regreg_rmmem_imm(type_, opc_, r, m, i, ...)		q_regreg_rmmem_imm(type_, opc_, rg_gpr(r), m, i, __VA_ARGS__)

#define q_func_wdq(func, type_, opc_, ...)\
	func(type_, opc_, 16, d_use_osize, __VA_ARGS__),\
	func(type_, opc_, 32, __VA_ARGS__),\
	func(type_, opc_, 64, d_ext_w, __VA_ARGS__)
#define q_func_bwdq(func, type_, opc_0, opc_1, ...)\
	func(type_, opc_0, 8, __VA_ARGS__),\
	q_func_wdq(func, type_, opc_1, __VA_ARGS__)
#define q_func_wdq_wdq(func, type_, opc_, ...)\
	func(type_, opc_, 16, 16, d_use_osize, __VA_ARGS__),\
	func(type_, opc_, 32, 32, __VA_ARGS__),\
	func(type_, opc_, 64, 64, d_ext_w, __VA_ARGS__)
#define q_func_bwdq_bwdq(func, type_, opc_0, opc_1, ...)\
	func(type_, opc_0, 8, 8, __VA_ARGS__),\
	q_func_wdq_wdq(func, type_, opc_1, __VA_ARGS__)
#define q_func_wdq_wdd(func, type_, opc_, ...)\
	func(type_, opc_, 16, 16, d_use_osize, __VA_ARGS__),\
	func(type_, opc_, 32, 32, __VA_ARGS__),\
	func(type_, opc_, 64, 32, d_ext_w, __VA_ARGS__)
#define q_func_bwdq_bwdd(func, type_, opc_0, opc_1, ...)\
	func(type_, opc_0, 8, 8, __VA_ARGS__), \
	q_func_wdq_wdd(func, type_, opc_1, __VA_ARGS__)
#define q_func_wdq_bbb(func, type_, opc_, ...)\
	func(type_, opc_, 16, 8, d_use_osize, __VA_ARGS__),\
	func(type_, opc_, 32, 8, __VA_ARGS__),\
	func(type_, opc_, 64, 8, d_ext_w, __VA_ARGS__)
#define q_func_wdq_wdq_bbb(func, type_, opc_, ...)\
	func(type_, opc_, 16, 16, 8, d_use_osize, __VA_ARGS__),\
	func(type_, opc_, 32, 32, 8, __VA_ARGS__),\
	func(type_, opc_, 64, 64, 8, d_ext_w, __VA_ARGS__)
#define q_func_wdq_wdq_wdd(func, type_, opc_, ...)\
	func(type_, opc_, 16, 16, 16, d_use_osize, __VA_ARGS__),\
	func(type_, opc_, 32, 32, 32, __VA_ARGS__),\
	func(type_, opc_, 64, 64, 32, d_ext_w, __VA_ARGS__)

#define q_g0(name, i)\
	q_func_bwdq_bwdq(q_gpr_rmreg_gpr_regreg, name, 0x00 + (0x08 * i), 0x01 + (0x08 * i)),\
	q_func_bwdq_bwdq(q_rmmem_gpr_regreg, name, 0x00 + (0x08 * i), 0x01 + (0x08 * i), d_allow_lock),\
	q_func_bwdq_bwdq(q_gpr_regreg_gpr_rmreg, name, 0x02 + (0x08 * i), 0x03 + (0x08 * i)),\
	q_func_bwdq_bwdq(q_gpr_regreg_rmmem, name, 0x02 + (0x08 * i), 0x03 + (0x08 * i)),\
	q_func_bwdq_bwdd(q_gpr_ax_imm, name, 0x04 + (0x08 * i), 0x05 + (0x08 * i)),\
	q_func_bwdq_bwdd(q_gpr_rmreg_imm, name, 0x80, 0x81, d_modrm_reg(i)),\
	q_func_bwdq_bwdd(q_rmmem_imm, name, 0x80, 0x81, d_modrm_reg(i)),\
	q_func_wdq_bbb(q_gpr_rmreg_imm, name, 0x83, d_modrm_reg(i)),\
	q_func_wdq_bbb(q_rmmem_imm, name, 0x83, d_modrm_reg(i))

#define q_g1(name, i)\
	q_imm(name, 0x70 + i, 8),\
	q_imm(name, 0x80 + i, 32, d_use_0f)

static const inst_rec_t inst_recs[] = {
	q_label(Label, 0x00, d_no_opc),

	q_imm(Data, 0x00, 8, d_no_opc),
	q_imm(Data, 0x00, 16, d_no_opc),
	q_imm(Data, 0x00, 32, d_no_opc),
	q_imm(Data, 0x00, 64, d_no_opc),

	q_g0(Add, 0),
	q_g0(Or, 1),
	q_g0(Adc, 2),
	q_g0(Sbb, 3),
	q_g0(And, 4),
	q_g0(Sub, 5),
	q_g0(Xor, 6),
	q_g0(Cmp, 7),

	q_func_bwdq_bwdq(q_gpr_rmreg_gpr_regreg, Mov, 0x88, 0x89),
	q_func_bwdq_bwdq(q_rmmem_gpr_regreg, Mov, 0x88, 0x89),
	q_func_bwdq_bwdq(q_gpr_regreg_gpr_rmreg, Mov, 0x8A, 0x8B),
	q_func_bwdq_bwdq(q_gpr_regreg_rmmem, Mov, 0x8A, 0x8B),
	q_func_bwdq_bwdq(q_gpr_opcreg_imm, Mov, 0xB0, 0xB8),
	q_func_bwdq_bwdd(q_gpr_rmreg_imm, Mov, 0xC6, 0xC7, d_modrm_reg(0)),
	q_func_bwdq_bwdd(q_rmmem_imm, Mov, 0xC6, 0xC7, d_modrm_reg(0)),

	q_func_wdq_bbb(q_gpr_regreg_gpr_rmreg, Movsx, 0xBE, d_use_0f),
	q_func_wdq_bbb(q_gpr_regreg_rmmem, Movsx, 0xBE, d_use_0f),
	q_gpr_regreg_gpr_rmreg(Movsx, 0xBF, 32, 16, d_use_0f),
	q_gpr_regreg_rmmem(Movsx, 0xBF, 32, 16, d_use_0f),
	q_gpr_regreg_gpr_rmreg(Movsx, 0xBF, 64, 16, d_ext_w, d_use_0f),
	q_gpr_regreg_rmmem(Movsx, 0xBF, 64, 16, d_ext_w, d_use_0f),

	q_gpr_regreg_gpr_rmreg(Movsxd, 0x63, 64, 32, 0x63, d_ext_w),

	q_func_wdq_bbb(q_gpr_regreg_gpr_rmreg, Movzx, 0xB6, d_use_0f),
	q_func_wdq_bbb(q_gpr_regreg_rmmem, Movzx, 0xB6, d_use_0f),
	q_gpr_regreg_gpr_rmreg(Movzx, 0xB7, 32, 16, d_use_0f),
	q_gpr_regreg_rmmem(Movzx, 0xB7, 32, 16, d_use_0f),
	q_gpr_regreg_gpr_rmreg(Movzx, 0xB7, 64, 16, d_ext_w, d_use_0f),
	q_gpr_regreg_rmmem(Movzx, 0xB7, 64, 16, d_ext_w, d_use_0f),

	q_gpr_regreg_rmmem(Lea, 0x8D, 16, n, d_use_osize),
	q_gpr_regreg_rmmem(Lea, 0x8D, 32, n),
	q_gpr_regreg_rmmem(Lea, 0x8D, 64, n, d_ext_w),

	q_imm(Call, 0xE8, 32),
	q_rmmem(Call, 0xFF, n, d_modrm_reg(2)),
	q_gpr_rmreg(Call, 0xFF, 64, d_modrm_reg(2)),

	q_none(Ret, 0xC3),
	q_imm(Ret, 0xC2, 16),

	q_none(Int3, 0xCC),

	q_imm(Jmp, 0xEB, 8),
	q_imm(Jmp, 0xE9, 32),
	q_rmmem(Jmp, 0xFF, n, d_modrm_reg(4)),
	q_gpr_rmreg(Jmp, 0xFF, 64, d_modrm_reg(4)),

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

	q_func_bwdq_bwdd(q_gpr_ax_imm, Test, 0xA8, 0xA9),
	q_func_bwdq_bwdd(q_gpr_rmreg_imm, Test, 0xF6, 0xF7, d_modrm_reg(0)),
	q_func_bwdq_bwdd(q_rmmem_imm, Test, 0xF6, 0xF7, d_modrm_reg(0)),
	q_func_bwdq_bwdq(q_gpr_rmreg_gpr_regreg, Test, 0x84, 0x85),
	q_func_bwdq_bwdq(q_rmmem_gpr_regreg, Test, 0x84, 0x85),

	q_func_bwdq(q_gpr_rmreg, Not, 0xF6, 0xF7, d_modrm_reg(2)),
	q_func_bwdq(q_rmmem, Not, 0xF6, 0xF7, d_modrm_reg(2), d_allow_lock),

	q_func_bwdq(q_gpr_rmreg, Neg, 0xF6, 0xF7, d_modrm_reg(3)),
	q_func_bwdq(q_rmmem, Neg, 0xF6, 0xF7, d_modrm_reg(3), d_allow_lock),

	q_func_bwdq(q_gpr_rmreg, Mul, 0xF6, 0xF7, d_modrm_reg(4)),
	q_func_bwdq(q_rmmem, Mul, 0xF6, 0xF7, d_modrm_reg(4)),

	q_func_bwdq(q_gpr_rmreg, Imul, 0xF6, 0xF7, d_modrm_reg(5)),
	q_func_bwdq(q_rmmem, Imul, 0xF6, 0xF7, d_modrm_reg(5)),
	q_func_wdq_wdq(q_gpr_regreg_gpr_rmreg, Imul, 0xAF, d_use_0f),
	q_func_wdq_wdq(q_gpr_regreg_rmmem, Imul, 0xAF, d_use_0f),
	q_func_wdq_wdq_bbb(q_gpr_regreg_gpr_rmreg_imm, Imul, 0x6B),
	q_func_wdq_wdq_bbb(q_gpr_regreg_rmmem_imm, Imul, 0x6B),
	q_func_wdq_wdq_wdd(q_gpr_regreg_gpr_rmreg_imm, Imul, 0x69),
	q_func_wdq_wdq_wdd(q_gpr_regreg_rmmem_imm, Imul, 0x69),

	q_func_bwdq(q_gpr_rmreg, Div, 0xF6, 0xF7, d_modrm_reg(6)),
	q_func_bwdq(q_rmmem, Div, 0xF6, 0xF7, d_modrm_reg(6)),

	q_func_bwdq(q_gpr_rmreg, Idiv, 0xF6, 0xF7, d_modrm_reg(7)),
	q_func_bwdq(q_rmmem, Idiv, 0xF6, 0xF7, d_modrm_reg(7)),

	q_none(Cbw, 0x98, d_use_osize),
	q_none(Cwde, 0x98),
	q_none(Cdqe, 0x98, d_ext_w),

	q_none(Cwd, 0x99, d_use_osize),
	q_none(Cdq, 0x99),
	q_none(Cqo, 0x99, d_ext_w),
};
static const size_t inst_recs_size = _countof(inst_recs);