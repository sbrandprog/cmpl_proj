#pragma once
#include "asm.h"

enum asm_reg {
	AsmRegNone,
	AsmRegEip, AsmRegRip,
	AsmRegAl, AsmRegAh, AsmRegAx, AsmRegEax, AsmRegRax,
	AsmRegCl, AsmRegCh, AsmRegCx, AsmRegEcx, AsmRegRcx,
	AsmRegDl, AsmRegDh, AsmRegDx, AsmRegEdx, AsmRegRdx,
	AsmRegBl, AsmRegBh, AsmRegBx, AsmRegEbx, AsmRegRbx,
	AsmRegSpl, AsmRegSp, AsmRegEsp, AsmRegRsp,
	AsmRegBpl, AsmRegBp, AsmRegEbp, AsmRegRbp,
	AsmRegSil, AsmRegSi, AsmRegEsi, AsmRegRsi,
	AsmRegDil, AsmRegDi, AsmRegEdi, AsmRegRdi,
	AsmRegR8b, AsmRegR8w, AsmRegR8d, AsmRegR8,
	AsmRegR9b, AsmRegR9w, AsmRegR9d, AsmRegR9,
	AsmRegR10b, AsmRegR10w, AsmRegR10d, AsmRegR10,
	AsmRegR11b, AsmRegR11w, AsmRegR11d, AsmRegR11,
	AsmRegR12b, AsmRegR12w, AsmRegR12d, AsmRegR12,
	AsmRegR13b, AsmRegR13w, AsmRegR13d, AsmRegR13,
	AsmRegR14b, AsmRegR14w, AsmRegR14d, AsmRegR14,
	AsmRegR15b, AsmRegR15w, AsmRegR15d, AsmRegR15,
	AsmReg_Count
};
union asm_reg_grps {
	struct asm_reg_grps_body {
		uint8_t none : 1, ip : 1, gpr : 1;
		uint8_t s_8 : 1, s_16 : 1, s_32 : 1, s_64 : 1;
		uint8_t gpr_ax : 1, gpr_cx : 1, gpr_dx : 1;
		uint8_t no_rex : 1, req_rex : 1;
	};
	uint8_t strg[sizeof(struct asm_reg_grps_body)];
};
struct asm_reg_info {
	uint8_t enc : 3, ext : 1;
	asm_reg_grps_t grps;
};

extern const asm_reg_info_t asm_reg_infos[AsmReg_Count];

asm_size_t asm_reg_get_size(asm_reg_t reg);

bool asm_reg_is_gpr(asm_reg_t reg);

bool asm_reg_check_grps(const asm_reg_grps_t * has, const asm_reg_grps_t * req);
