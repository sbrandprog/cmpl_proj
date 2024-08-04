#include "lnk_sect.h"
#include "lnk_pel.h"
#include "mc_size.h"
#include "mc_reg.h"
#include "mc_inst.h"
#include "mc_frag.h"

typedef struct sect_info {
	const char * name;

	size_t align;

	uint8_t align_byte;

	bool mem_r, mem_w, mem_e, mem_disc;
} sect_info_t;

static const sect_info_t sect_infos[McFrag_Count] = {
	[McFragProc] = { .name = ".text", .align = 16, .align_byte = 0xCC, .mem_r = true, .mem_e = true },
	[McFragRoData] = { .name = ".rdata", .align = 16, .align_byte = 0x00, .mem_r = true },
	[McFragWrData] = { .name = ".data", .align = 16, .align_byte = 0x00, .mem_r = true, .mem_w = true }
};

mc_frag_t * mc_frag_create(mc_frag_type_t type) {
	mc_frag_t * frag = malloc(sizeof(*frag));

	ul_assert(frag != NULL);

	*frag = (mc_frag_t){ .type = type };

	return frag;
}
void mc_frag_destroy(mc_frag_t * frag) {
	if (frag == NULL) {
		return;
	}

	free(frag->insts);
	free(frag);
}

void mc_frag_destroy_chain(mc_frag_t * frag) {
	while (frag != NULL) {
		mc_frag_t * next = frag->next;

		mc_frag_destroy(frag);

		frag = next;
	}
}

void mc_frag_push_inst(mc_frag_t * frag, mc_inst_t * inst) {
	if (frag->insts_size + 1 > frag->insts_cap) {
		ul_arr_grow(&frag->insts_cap, &frag->insts, sizeof(*frag->insts), 1);
	}

	frag->insts[frag->insts_size++] = *inst;
}

static bool fill_sect_info(mc_frag_t * frag, lnk_sect_t * out) {
	if (frag->type == McFragNone || frag->type >= McFrag_Count) {
		return false;
	}

	const sect_info_t * info = &sect_infos[frag->type];

	out->name = info->name;

	out->data_align = frag->align == 0 ? info->align : frag->align;

	out->data_align_byte = info->align_byte;

	out->mem_r = info->mem_r;
	out->mem_w = info->mem_w;
	out->mem_e = info->mem_e;
	out->mem_disc = info->mem_disc;

	return true;
}

static bool get_imm0_fixup_stype(mc_inst_t * inst, lnk_sect_lp_stype_t * out) {
	*out = LnkSectLpFixupNone;

	switch (inst->opds) {
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

	switch (inst->imm0_type) {
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
		default:
			return false;
	}

	return true;
}
static bool get_disp_fixup_stype(mc_inst_t * inst, lnk_sect_lp_stype_t * out) {
	*out = LnkSectLpFixupNone;

	switch (inst->opds) {
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

	switch (inst->mem_disp_type) {
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

static bool build_special_inst(mc_frag_t * frag, mc_inst_t * inst, lnk_sect_t * out) {
	switch (inst->type) {
		case McInstLabel:
			if (inst->opds != McInstOpds_Label) {
				return false;
			}

			lnk_sect_add_lp(out, LnkSectLpLabel, LnkSectLpLabelNone, inst->label, out->data_size);

			break;
		case McInstAlign:
		{
			if (inst->opds != McInstOpds_Imm || inst->imm0_type != McInstImm64) {
				return false;
			}

			size_t cur_size = out->data_size;

			size_t aligned_size = ul_align_to(cur_size, (size_t)inst->imm0);

			if (aligned_size >= LNK_PEL_MODULE_MAX_SIZE) {
				return false;
			}

			if (aligned_size > out->data_cap) {
				ul_arr_grow(&out->data_cap, &out->data, sizeof(*out->data), aligned_size - out->data_cap);
			}

			memset(out->data + out->data_size, out->data_align_byte, aligned_size - cur_size);

			out->data_size = aligned_size;

			break;
		}
		default:
			return false;
	}

	return true;
}
static bool build_core(mc_frag_t * frag, lnk_sect_t * out) {
	if (!fill_sect_info(frag, out)) {
		return false;
	}

	for (mc_inst_t * inst = frag->insts, *inst_end = inst + frag->insts_size; inst != inst_end; ++inst) {
		if (build_special_inst(frag, inst, out)) {
			continue;
		}

		size_t inst_size;
		mc_inst_bs_t inst_bs;
		mc_inst_offs_t inst_offs;

		if (!mc_inst_build(inst, &inst_size, inst_bs, &inst_offs)) {
			return false;
		}

		if (out->data_size + inst_size > out->data_cap) {
			ul_arr_grow(&out->data_cap, &out->data, sizeof(*out->data), out->data_size + inst_size - out->data_cap);
		}

		size_t inst_start = out->data_size;

		memcpy(out->data + out->data_size, inst_bs, inst_size);
		out->data_size += inst_size;

		lnk_sect_lp_stype_t stype;

		if (!get_imm0_fixup_stype(inst, &stype)) {
			return false;
		}
		else if (stype != LnkSectLpFixupNone) {
			lnk_sect_add_lp(out, LnkSectLpFixup, stype, inst->imm0_label, inst_start + inst_offs.off[McInstOffImm]);
		}

		if (!get_disp_fixup_stype(inst, &stype)) {
			return false;
		}
		else if (stype != LnkSectLpFixupNone) {
			lnk_sect_add_lp(out, LnkSectLpFixup, stype, inst->mem_disp_label, inst_start + inst_offs.off[McInstOffDisp]);
		}
	}

	return true;
}

bool mc_frag_build(mc_frag_t * frag, lnk_sect_t ** out) {
	*out = lnk_sect_create();

	return build_core(frag, *out);
}
