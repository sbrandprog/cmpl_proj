#include "lnk_sect.h"
#include "lnk_pel.h"
#include "mc_defs.h"
#include "mc_size.h"
#include "mc_reg.h"
#include "mc_inst.h"
#include "mc_frag.h"

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

void mc_frag_push_inst(mc_frag_t * frag, const mc_inst_t * inst) {
	ul_arr_grow(frag->insts_size + 1, &frag->insts_cap, &frag->insts, sizeof(*frag->insts));

	frag->insts[frag->insts_size++] = *inst;
}

static bool create_sect(mc_frag_t * frag, lnk_sect_t ** out) {
	if (frag->type >= McFrag_Count) {
		return false;
	}

	const lnk_sect_desc_t * desc = &mc_defs_sds[mc_frag_type_to_sd[frag->type]];

	*out = lnk_sect_create_desc(desc);

	(*out)->data_align = frag->align == 0 ? desc->data_align : frag->align;

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
		case McInstImmLabelRva31of64:
			*out = LnkSectLpFixupRva31of64;
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

static bool build_core(mc_frag_t * frag, lnk_sect_t ** out) {
	if (!create_sect(frag, out)) {
		return false;
	}

	lnk_sect_t * sect = *out;

	for (mc_inst_t * inst = frag->insts, *inst_end = inst + frag->insts_size; inst != inst_end; ++inst) {
		switch (inst->type) {
			case McInstLabel:
				if (inst->opds != McInstOpds_Label) {
					return false;
				}

				lnk_sect_add_lp(sect, LnkSectLpLabel, inst->label_stype, inst->label, sect->data_size);

				break;
			case McInstAlign:
			{
				if (inst->opds != McInstOpds_Imm || inst->imm0_type != McInstImm64) {
					return false;
				}

				size_t cur_size = sect->data_size;

				size_t aligned_size = ul_align_to(cur_size, (size_t)inst->imm0);

				if (aligned_size >= LNK_PEL_MODULE_MAX_SIZE) {
					return false;
				}

				ul_arr_grow(aligned_size, &sect->data_cap, &sect->data, sizeof(*sect->data));

				memset(sect->data + sect->data_size, sect->data_align_byte, aligned_size - cur_size);

				sect->data_size = aligned_size;

				break;
			}
			default:
			{

				size_t inst_size;
				mc_inst_bs_t inst_bs;
				mc_inst_offs_t inst_offs;

				if (!mc_inst_build(inst, &inst_size, inst_bs, &inst_offs)) {
					return false;
				}

				ul_arr_grow(sect->data_size + inst_size, &sect->data_cap, &sect->data, sizeof(*sect->data));

				size_t inst_start = sect->data_size;

				memcpy(sect->data + sect->data_size, inst_bs, inst_size);
				sect->data_size += inst_size;

				lnk_sect_lp_stype_t stype;

				if (!get_imm0_fixup_stype(inst, &stype)) {
					return false;
				}
				else if (stype != LnkSectLpFixupNone) {
					lnk_sect_add_lp(sect, LnkSectLpFixup, stype, inst->imm0_label, inst_start + inst_offs.off[McInstOffImm]);
				}

				if (!get_disp_fixup_stype(inst, &stype)) {
					return false;
				}
				else if (stype != LnkSectLpFixupNone) {
					lnk_sect_add_lp(sect, LnkSectLpFixup, stype, inst->mem_disp_label, inst_start + inst_offs.off[McInstOffDisp]);
				}

				break;
			}
		}
	}

	return true;
}

bool mc_frag_build(mc_frag_t * frag, lnk_sect_t ** out) {
	return build_core(frag, out);
}

const mc_defs_sd_type_t mc_frag_type_to_sd[McFrag_Count] = {
	[McFragCode] = McDefsSdText_Code,
	[McFragRoData] = McDefsSdRdata_Data,
	[McFragRwData] = McDefsSdData_Data,
	[McFragUnw] = McDefsSdRdata_Unw
};
