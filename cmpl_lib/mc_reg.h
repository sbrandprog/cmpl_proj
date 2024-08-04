#pragma once
#include "mc_size.h"

enum mc_reg {
	McRegNone,
	McRegEip, McRegRip,
	McRegAl, McRegAh, McRegAx, McRegEax, McRegRax,
	McRegCl, McRegCh, McRegCx, McRegEcx, McRegRcx,
	McRegDl, McRegDh, McRegDx, McRegEdx, McRegRdx,
	McRegBl, McRegBh, McRegBx, McRegEbx, McRegRbx,
	McRegSpl, McRegSp, McRegEsp, McRegRsp,
	McRegBpl, McRegBp, McRegEbp, McRegRbp,
	McRegSil, McRegSi, McRegEsi, McRegRsi,
	McRegDil, McRegDi, McRegEdi, McRegRdi,
	McRegR8b, McRegR8w, McRegR8d, McRegR8,
	McRegR9b, McRegR9w, McRegR9d, McRegR9,
	McRegR10b, McRegR10w, McRegR10d, McRegR10,
	McRegR11b, McRegR11w, McRegR11d, McRegR11,
	McRegR12b, McRegR12w, McRegR12d, McRegR12,
	McRegR13b, McRegR13w, McRegR13d, McRegR13,
	McRegR14b, McRegR14w, McRegR14d, McRegR14,
	McRegR15b, McRegR15w, McRegR15d, McRegR15,
	McReg_Count
};
union mc_reg_grps {
	struct mc_reg_grps_body {
		uint8_t none : 1, ip : 1, gpr : 1;
		uint8_t s_8 : 1, s_16 : 1, s_32 : 1, s_64 : 1;
		uint8_t gpr_ax : 1, gpr_cx : 1, gpr_dx : 1;
		uint8_t no_rex : 1, req_rex : 1;
	};
	uint8_t strg[sizeof(struct mc_reg_grps_body)];
};
struct mc_reg_info {
	uint8_t enc : 3, ext : 1;
	mc_reg_grps_t grps;
	mc_reg_gpr_t gpr;
};
enum mc_reg_gpr {
	McRegGprAx,
	McRegGprCx,
	McRegGprDx,
	McRegGprBx,
	McRegGprSp,
	McRegGprBp,
	McRegGprSi,
	McRegGprDi,
	McRegGprR8,
	McRegGprR9,
	McRegGprR10,
	McRegGprR11,
	McRegGprR12,
	McRegGprR13,
	McRegGprR14,
	McRegGprR15,
	McRegGpr_Count,
};

extern MC_API const mc_reg_info_t mc_reg_infos[McReg_Count];
extern MC_API const mc_reg_t mc_reg_gprs[McRegGpr_Count][McSize_Count];

MC_API mc_size_t mc_reg_get_size(mc_reg_t reg);

MC_API bool mc_reg_check_grps(const mc_reg_grps_t * has, const mc_reg_grps_t * req);
