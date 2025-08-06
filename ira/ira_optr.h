#pragma once
#include "ira.h"
#include "ira_dt.h"

enum ira_optr_type
{
    IraOptrNone,
    IraOptrBltnLogicNegBool,
    IraOptrBltnArithNegInt,
    IraOptrBltnMulInt,
    IraOptrBltnDivInt,
    IraOptrBltnModInt,
    IraOptrBltnAddInt,
    IraOptrBltnSubInt,
    IraOptrBltnLeShiftInt,
    IraOptrBltnRiShiftInt,
    IraOptrBltnLessInt,
    IraOptrBltnLessEqInt,
    IraOptrBltnGrtrInt,
    IraOptrBltnGrtrEqInt,
    IraOptrBltnEqInt,
    IraOptrBltnNeqInt,
    IraOptrBltnAndInt,
    IraOptrBltnXorInt,
    IraOptrBltnOrInt,
    IraOptrBltnLessPtr,
    IraOptrBltnLessEqPtr,
    IraOptrBltnGrtrPtr,
    IraOptrBltnGrtrEqPtr,
    IraOptrBltnEqPtr,
    IraOptrBltnNeqPtr,
    IraOptrBltnArithNegEnmn,
    IraOptrBltnMulEnmn,
    IraOptrBltnDivEnmn,
    IraOptrBltnModEnmn,
    IraOptrBltnAddEnmn,
    IraOptrBltnSubEnmn,
    IraOptrBltnLeShiftEnmn,
    IraOptrBltnRiShiftEnmn,
    IraOptrBltnLessEnmn,
    IraOptrBltnLessEqEnmn,
    IraOptrBltnGrtrEnmn,
    IraOptrBltnGrtrEqEnmn,
    IraOptrBltnEqEnmn,
    IraOptrBltnNeqEnmn,
    IraOptrBltnAndEnmn,
    IraOptrBltnXorEnmn,
    IraOptrBltnOrEnmn,
    IraOptr_Count,
};
struct ira_optr
{
    ira_optr_type_t type;

    ira_optr_t * next;
};

enum ira_optr_ctg
{
    IraOptrCtgNone,
    IraOptrCtgLogicNeg,
    IraOptrCtgBitNeg,
    IraOptrCtgArithNeg,
    IraOptrCtgMul,
    IraOptrCtgDiv,
    IraOptrCtgMod,
    IraOptrCtgAdd,
    IraOptrCtgSub,
    IraOptrCtgLeShift,
    IraOptrCtgRiShift,
    IraOptrCtgLess,
    IraOptrCtgLessEq,
    IraOptrCtgGrtr,
    IraOptrCtgGrtrEq,
    IraOptrCtgEq,
    IraOptrCtgNeq,
    IraOptrCtgBitAnd,
    IraOptrCtgBitOr,
    IraOptrCtgBitXor,
    IraOptrCtgLogicAnd,
    IraOptrCtgLogicOr,
    IraOptrCtg_Count
};

struct ira_optr_info
{
    bool is_bltn;
    union
    {
        struct
        {
            bool is_unr;
            ira_optr_ctg_t ctg;
            ira_dt_type_t opd_dt_type;
        } bltn;
    };
};

IRA_API ira_optr_t * ira_optr_create(ira_optr_type_t type);
IRA_API void ira_optr_destroy(ira_optr_t * optr);
IRA_API void ira_optr_destroy_chain(ira_optr_t * optr);

IRA_API ira_optr_t * ira_optr_copy(ira_optr_t * optr);

IRA_API bool ira_optr_is_equivalent(ira_optr_t * first, ira_optr_t * second);

extern IRA_API const ira_optr_info_t ira_optr_infos[IraOptr_Count];
