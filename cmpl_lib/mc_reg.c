#include "mc_reg.h"
#include "mc_size.h"

mc_size_t mc_reg_get_size(mc_reg_t reg)
{
    if (reg >= McReg_Count)
    {
        return McSizeNone;
    }

    const mc_reg_grps_t * grps = &mc_reg_infos[reg].grps;

    if (grps->s_8)
    {
        return McSize8;
    }
    else if (grps->s_16)
    {
        return McSize16;
    }
    else if (grps->s_32)
    {
        return McSize32;
    }
    else if (grps->s_64)
    {
        return McSize64;
    }

    return McSizeNone;
}

bool mc_reg_check_grps(const mc_reg_grps_t * has, const mc_reg_grps_t * req)
{
    for (size_t i = 0; i < _countof(has->strg); ++i)
    {
        uint8_t given_i = has->strg[i], req_i = req->strg[i];

        if ((~given_i & req_i) != 0)
        {
            return false;
        }
    }

    return true;
}

// base
#define reg(name, enc_, ...) [name] = { .enc = enc_, __VA_ARGS__ }
#define reg_g(...) .grps = { __VA_ARGS__ }
#define reg_gpr(name) .gpr = McRegGpr##name
#define reg_ext .ext = 1

// groups
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

// quick
#define q_gpr_first4(lttr, cap_lttr, enc, enc_h, sub_grp)                                    \
    reg(McReg##cap_lttr##l, enc, reg_g(g_gpr, g_s_8, sub_grp), reg_gpr(cap_lttr##x)),        \
        reg(McReg##cap_lttr##h, enc_h, reg_g(g_gpr, g_s_8, g_no_rex), reg_gpr(cap_lttr##x)), \
        reg(McReg##cap_lttr##x, enc, reg_g(g_gpr, g_s_16, sub_grp), reg_gpr(cap_lttr##x)),   \
        reg(McRegE##lttr##x, enc, reg_g(g_gpr, g_s_32, sub_grp), reg_gpr(cap_lttr##x)),      \
        reg(McRegR##lttr##x, enc, reg_g(g_gpr, g_s_64, sub_grp), reg_gpr(cap_lttr##x))
#define q_gpr_second4(name, cap_name, enc)                                           \
    reg(McReg##cap_name##l, enc, reg_g(g_gpr, g_s_8, g_req_rex), reg_gpr(cap_name)), \
        reg(McReg##cap_name, enc, reg_g(g_gpr, g_s_16), reg_gpr(cap_name)),          \
        reg(McRegE##name, enc, reg_g(g_gpr, g_s_32), reg_gpr(cap_name)),             \
        reg(McRegR##name, enc, reg_g(g_gpr, g_s_64), reg_gpr(cap_name))
#define q_gpr_ext8(name, enc)                                                   \
    reg(McReg##name##b, enc, reg_ext, reg_g(g_gpr, g_s_8), reg_gpr(name)),      \
        reg(McReg##name##w, enc, reg_ext, reg_g(g_gpr, g_s_16), reg_gpr(name)), \
        reg(McReg##name##d, enc, reg_ext, reg_g(g_gpr, g_s_32), reg_gpr(name)), \
        reg(McReg##name, enc, reg_ext, reg_g(g_gpr, g_s_64), reg_gpr(name))

const mc_reg_info_t mc_reg_infos[McReg_Count] = {
    reg(McRegNone, 0, reg_g(g_none)),
    reg(McRegEip, 0, reg_g(g_ip, g_s_32)),
    reg(McRegRip, 0, reg_g(g_ip, g_s_64)),

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
const mc_reg_t mc_reg_gprs[McRegGpr_Count][McSize_Count] = {
    [McRegGprAx] = { [McSize8] = McRegAl, [McSize16] = McRegAx, [McSize32] = McRegEax, [McSize64] = McRegRax },
    [McRegGprCx] = { [McSize8] = McRegCl, [McSize16] = McRegCx, [McSize32] = McRegEcx, [McSize64] = McRegRcx },
    [McRegGprDx] = { [McSize8] = McRegDl, [McSize16] = McRegDx, [McSize32] = McRegEdx, [McSize64] = McRegRdx },
    [McRegGprBx] = { [McSize8] = McRegBl, [McSize16] = McRegBx, [McSize32] = McRegEbx, [McSize64] = McRegRbx },
    [McRegGprSp] = { [McSize8] = McRegSpl, [McSize16] = McRegSp, [McSize32] = McRegEsp, [McSize64] = McRegRsp },
    [McRegGprBp] = { [McSize8] = McRegBpl, [McSize16] = McRegBp, [McSize32] = McRegEbp, [McSize64] = McRegRbp },
    [McRegGprSi] = { [McSize8] = McRegSil, [McSize16] = McRegSi, [McSize32] = McRegEsi, [McSize64] = McRegRsi },
    [McRegGprDi] = { [McSize8] = McRegDil, [McSize16] = McRegDi, [McSize32] = McRegEdi, [McSize64] = McRegRdi },
    [McRegGprR8] = { [McSize8] = McRegR8b, [McSize16] = McRegR8w, [McSize32] = McRegR8d, [McSize64] = McRegR8 },
    [McRegGprR9] = { [McSize8] = McRegR9b, [McSize16] = McRegR9w, [McSize32] = McRegR9d, [McSize64] = McRegR9 },
    [McRegGprR10] = { [McSize8] = McRegR10b, [McSize16] = McRegR10w, [McSize32] = McRegR10d, [McSize64] = McRegR10 },
    [McRegGprR11] = { [McSize8] = McRegR11b, [McSize16] = McRegR11w, [McSize32] = McRegR11d, [McSize64] = McRegR11 },
    [McRegGprR12] = { [McSize8] = McRegR12b, [McSize16] = McRegR12w, [McSize32] = McRegR12d, [McSize64] = McRegR12 },
    [McRegGprR13] = { [McSize8] = McRegR13b, [McSize16] = McRegR13w, [McSize32] = McRegR13d, [McSize64] = McRegR13 },
    [McRegGprR14] = { [McSize8] = McRegR14b, [McSize16] = McRegR14w, [McSize32] = McRegR14d, [McSize64] = McRegR14 },
    [McRegGprR15] = { [McSize8] = McRegR15b, [McSize16] = McRegR15w, [McSize32] = McRegR15d, [McSize64] = McRegR15 },
};
