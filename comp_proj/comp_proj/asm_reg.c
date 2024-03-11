#include "pch.h"
#include "asm_reg.h"
#include "asm_size.h"

asm_size_t asm_reg_get_size(asm_reg_t reg) {
	ul_raise_assert(reg < AsmReg_Count);

	const asm_reg_grps_t * grps = &asm_reg_infos[reg].grps;

	if (grps->s_8) {
		return AsmSize8;
	}
	else if (grps->s_16) {
		return AsmSize16;
	}
	else if (grps->s_32) {
		return AsmSize32;
	}
	else if (grps->s_64) {
		return AsmSize64;
	}
	
	return AsmSizeNone;
}

bool asm_reg_is_gpr(asm_reg_t reg) {
	if (reg >= AsmReg_Count) {
		return false;
	}

	const asm_reg_grps_t * grps = &asm_reg_infos[reg].grps;

	return grps->gpr;
}

bool asm_reg_check_grps(const asm_reg_grps_t * has, const asm_reg_grps_t * req) {
	for (size_t i = 0; i < _countof(has->strg); ++i) {
		uint8_t given_i = has->strg[i], req_i = req->strg[i];

		if ((~given_i & req_i) != 0) {
			return false;
		}
	}

	return true;
}

//base
#define reg(name, enc_, ...) [name] = { .enc = enc_, __VA_ARGS__ }
#define reg_g(...) .grps = { __VA_ARGS__ }
#define reg_ext .ext = 1

//groups
#define g_empty
#define g_none .none = 1
#define g_ip .ip = 1
#define g_gpr .gpr = 1
#define g_s_8 .s_8 = 1
#define g_s_16 .s_16 = 1
#define g_s_32 .s_32 = 1
#define g_s_64 .s_64 = 1
#define g_gpr_ax .gpr_ax = 1
#define g_gpr_cx .gpr_cx = 1
#define g_gpr_dx .gpr_dx = 1
#define g_no_rex .no_rex = 1
#define g_req_rex .req_rex = 1

//quick
#define q_gpr_first4(lttr, cap_lttr, enc, enc_h, sub_grp)\
reg(AsmReg##cap_lttr##l, enc, reg_g(g_gpr, g_s_8, sub_grp)),\
reg(AsmReg##cap_lttr##h, enc_h, reg_g(g_gpr, g_s_8, g_no_rex)),\
reg(AsmReg##cap_lttr##x, enc, reg_g(g_gpr, g_s_16, sub_grp)),\
reg(AsmRegE##lttr##x, enc, reg_g(g_gpr, g_s_32, sub_grp)),\
reg(AsmRegR##lttr##x, enc, reg_g(g_gpr, g_s_64, sub_grp))
#define q_gpr_second4(name, cap_name, enc)\
reg(AsmReg##cap_name##l, enc, reg_g(g_gpr, g_s_8, g_req_rex)),\
reg(AsmReg##cap_name, enc, reg_g(g_gpr, g_s_16)),\
reg(AsmRegE##name, enc, reg_g(g_gpr, g_s_32)),\
reg(AsmRegR##name, enc, reg_g(g_gpr, g_s_64))
#define q_gpr_ext8(name, enc)\
reg(AsmReg##name##b, enc, reg_ext, reg_g(g_gpr, g_s_8)),\
reg(AsmReg##name##w, enc, reg_ext, reg_g(g_gpr, g_s_16)),\
reg(AsmReg##name##d, enc, reg_ext, reg_g(g_gpr, g_s_32)),\
reg(AsmReg##name, enc, reg_ext, reg_g(g_gpr, g_s_64))

const asm_reg_info_t asm_reg_infos[AsmReg_Count] = {
	reg(AsmRegNone, 0, reg_g(g_none)),
	reg(AsmRegEip, 0, reg_g(g_ip, g_s_32)),
	reg(AsmRegRip, 0, reg_g(g_ip, g_s_64)),

	q_gpr_first4(a, A, 0b000, 0b100, g_gpr_ax),
	q_gpr_first4(c, C, 0b001, 0b101, g_gpr_cx),
	q_gpr_first4(d, D, 0b010, 0b110, g_gpr_dx),
	q_gpr_first4(b, B, 0b011, 0b111, g_empty),
	q_gpr_second4(sp, Sp, 0b100),
	q_gpr_second4(bp, Bp, 0b101),
	q_gpr_second4(si, Si, 0b110),
	q_gpr_second4(di, Di, 0b111),
	q_gpr_ext8(R8, 0b000),
	q_gpr_ext8(R9, 0b001),
	q_gpr_ext8(R10, 0b010),
	q_gpr_ext8(R11, 0b011),
	q_gpr_ext8(R12, 0b100),
	q_gpr_ext8(R13, 0b101),
	q_gpr_ext8(R14, 0b110),
	q_gpr_ext8(R15, 0b111)

};
const asm_reg_t asm_reg_gprs[AsmRegGpr_Count][AsmSize_Count] = {
	[AsmRegGprAx] = { [AsmSize8] = AsmRegAl, [AsmSize16] = AsmRegAx, [AsmSize32] = AsmRegEax, [AsmSize64] = AsmRegRax },
	[AsmRegGprCx] = { [AsmSize8] = AsmRegCl, [AsmSize16] = AsmRegCx, [AsmSize32] = AsmRegEcx, [AsmSize64] = AsmRegRcx },
	[AsmRegGprDx] = { [AsmSize8] = AsmRegDl, [AsmSize16] = AsmRegDx, [AsmSize32] = AsmRegEdx, [AsmSize64] = AsmRegRdx },
	[AsmRegGprBx] = { [AsmSize8] = AsmRegBl, [AsmSize16] = AsmRegBx, [AsmSize32] = AsmRegEbx, [AsmSize64] = AsmRegRbx },
	[AsmRegGprSp] = { [AsmSize8] = AsmRegSpl, [AsmSize16] = AsmRegSp, [AsmSize32] = AsmRegEsp, [AsmSize64] = AsmRegRsp },
	[AsmRegGprBp] = { [AsmSize8] = AsmRegBpl, [AsmSize16] = AsmRegBp, [AsmSize32] = AsmRegEbp, [AsmSize64] = AsmRegRbp },
	[AsmRegGprSi] = { [AsmSize8] = AsmRegSil, [AsmSize16] = AsmRegSi, [AsmSize32] = AsmRegEsi, [AsmSize64] = AsmRegRsi },
	[AsmRegGprDi] = { [AsmSize8] = AsmRegDil, [AsmSize16] = AsmRegDi, [AsmSize32] = AsmRegEdi, [AsmSize64] = AsmRegRdi },
	[AsmRegGprR8] = { [AsmSize8] = AsmRegR8b, [AsmSize16] = AsmRegR8w, [AsmSize32] = AsmRegR8d, [AsmSize64] = AsmRegR8 },
	[AsmRegGprR9] = { [AsmSize8] = AsmRegR9b, [AsmSize16] = AsmRegR9w, [AsmSize32] = AsmRegR9d, [AsmSize64] = AsmRegR9 },
	[AsmRegGprR10] = { [AsmSize8] = AsmRegR10b, [AsmSize16] = AsmRegR10w, [AsmSize32] = AsmRegR10d, [AsmSize64] = AsmRegR10 },
	[AsmRegGprR11] = { [AsmSize8] = AsmRegR11b, [AsmSize16] = AsmRegR11w, [AsmSize32] = AsmRegR11d, [AsmSize64] = AsmRegR11 },
	[AsmRegGprR12] = { [AsmSize8] = AsmRegR12b, [AsmSize16] = AsmRegR12w, [AsmSize32] = AsmRegR12d, [AsmSize64] = AsmRegR12 },
	[AsmRegGprR13] = { [AsmSize8] = AsmRegR13b, [AsmSize16] = AsmRegR13w, [AsmSize32] = AsmRegR13d, [AsmSize64] = AsmRegR13 },
	[AsmRegGprR14] = { [AsmSize8] = AsmRegR14b, [AsmSize16] = AsmRegR14w, [AsmSize32] = AsmRegR14d, [AsmSize64] = AsmRegR14 },
	[AsmRegGprR15] = { [AsmSize8] = AsmRegR15b, [AsmSize16] = AsmRegR15w, [AsmSize32] = AsmRegR15d, [AsmSize64] = AsmRegR15 },
};
