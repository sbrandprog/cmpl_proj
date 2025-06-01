#include "ira_optr.h"
#include "ira_dt.h"

ira_optr_t * ira_optr_create(ira_optr_type_t type)
{
    ira_optr_t * optr = malloc(sizeof(*optr));

    ul_assert(optr != NULL);

    *optr = (ira_optr_t){ .type = type };

    return optr;
}
void ira_optr_destroy(ira_optr_t * optr)
{
    if (optr == NULL)
    {
        return;
    }

    switch (optr->type)
    {
        case IraOptrNone:
        case IraOptrBltnLogicNegBool:
        case IraOptrBltnArithNegInt:
        case IraOptrBltnMulInt:
        case IraOptrBltnDivInt:
        case IraOptrBltnModInt:
        case IraOptrBltnAddInt:
        case IraOptrBltnSubInt:
        case IraOptrBltnLeShiftInt:
        case IraOptrBltnRiShiftInt:
        case IraOptrBltnLessInt:
        case IraOptrBltnLessEqInt:
        case IraOptrBltnGrtrInt:
        case IraOptrBltnGrtrEqInt:
        case IraOptrBltnEqInt:
        case IraOptrBltnNeqInt:
        case IraOptrBltnAndInt:
        case IraOptrBltnXorInt:
        case IraOptrBltnOrInt:
        case IraOptrBltnLessPtr:
        case IraOptrBltnLessEqPtr:
        case IraOptrBltnGrtrPtr:
        case IraOptrBltnGrtrEqPtr:
        case IraOptrBltnEqPtr:
        case IraOptrBltnNeqPtr:
        case IraOptrBltnArithNegEnmn:
        case IraOptrBltnMulEnmn:
        case IraOptrBltnDivEnmn:
        case IraOptrBltnModEnmn:
        case IraOptrBltnAddEnmn:
        case IraOptrBltnSubEnmn:
        case IraOptrBltnLeShiftEnmn:
        case IraOptrBltnRiShiftEnmn:
        case IraOptrBltnLessEnmn:
        case IraOptrBltnLessEqEnmn:
        case IraOptrBltnGrtrEnmn:
        case IraOptrBltnGrtrEqEnmn:
        case IraOptrBltnEqEnmn:
        case IraOptrBltnNeqEnmn:
        case IraOptrBltnAndEnmn:
        case IraOptrBltnXorEnmn:
        case IraOptrBltnOrEnmn:
            break;
        default:
            ul_assert_unreachable();
    }

    free(optr);
}
void ira_optr_destroy_chain(ira_optr_t * optr)
{
    while (optr != NULL)
    {
        ira_optr_t * next = optr->next;

        ira_optr_destroy(optr);

        optr = next;
    }
}

bool ira_optr_is_equivalent(ira_optr_t * first, ira_optr_t * second)
{
    if (first == second)
    {
        return true;
    }

    switch (first->type)
    {
        case IraOptrNone:
            break;
        case IraOptrBltnLogicNegBool:
            switch (second->type)
            {
                case IraOptrBltnLogicNegBool:
                    return true;
                default:
                    break;
            }
            break;
        case IraOptrBltnArithNegInt:
            switch (second->type)
            {
                case IraOptrBltnArithNegInt:
                    return true;
                default:
                    break;
            }
            break;
        case IraOptrBltnMulInt:
        case IraOptrBltnDivInt:
        case IraOptrBltnModInt:
        case IraOptrBltnAddInt:
        case IraOptrBltnSubInt:
        case IraOptrBltnLeShiftInt:
        case IraOptrBltnRiShiftInt:
        case IraOptrBltnLessInt:
        case IraOptrBltnLessEqInt:
        case IraOptrBltnGrtrInt:
        case IraOptrBltnGrtrEqInt:
        case IraOptrBltnEqInt:
        case IraOptrBltnNeqInt:
        case IraOptrBltnAndInt:
        case IraOptrBltnXorInt:
        case IraOptrBltnOrInt:
            switch (second->type)
            {
                case IraOptrBltnMulInt:
                case IraOptrBltnDivInt:
                case IraOptrBltnModInt:
                case IraOptrBltnAddInt:
                case IraOptrBltnSubInt:
                case IraOptrBltnLeShiftInt:
                case IraOptrBltnRiShiftInt:
                case IraOptrBltnLessInt:
                case IraOptrBltnLessEqInt:
                case IraOptrBltnGrtrInt:
                case IraOptrBltnGrtrEqInt:
                case IraOptrBltnEqInt:
                case IraOptrBltnNeqInt:
                case IraOptrBltnAndInt:
                case IraOptrBltnXorInt:
                case IraOptrBltnOrInt:
                    return true;
                default:
                    break;
            }
            break;
        case IraOptrBltnLessPtr:
        case IraOptrBltnLessEqPtr:
        case IraOptrBltnGrtrPtr:
        case IraOptrBltnGrtrEqPtr:
        case IraOptrBltnEqPtr:
        case IraOptrBltnNeqPtr:
            switch (second->type)
            {
                case IraOptrBltnLessPtr:
                case IraOptrBltnLessEqPtr:
                case IraOptrBltnGrtrPtr:
                case IraOptrBltnGrtrEqPtr:
                case IraOptrBltnEqPtr:
                case IraOptrBltnNeqPtr:
                    return true;
                default:
                    break;
            }
            break;
        case IraOptrBltnArithNegEnmn:
            switch (second->type)
            {
                case IraOptrBltnArithNegEnmn:
                    return true;
                default:
                    break;
            }
            break;
        case IraOptrBltnMulEnmn:
        case IraOptrBltnDivEnmn:
        case IraOptrBltnModEnmn:
        case IraOptrBltnAddEnmn:
        case IraOptrBltnSubEnmn:
        case IraOptrBltnLeShiftEnmn:
        case IraOptrBltnRiShiftEnmn:
        case IraOptrBltnLessEnmn:
        case IraOptrBltnLessEqEnmn:
        case IraOptrBltnGrtrEnmn:
        case IraOptrBltnGrtrEqEnmn:
        case IraOptrBltnEqEnmn:
        case IraOptrBltnNeqEnmn:
        case IraOptrBltnAndEnmn:
        case IraOptrBltnXorEnmn:
        case IraOptrBltnOrEnmn:
            switch (second->type)
            {
                case IraOptrBltnMulEnmn:
                case IraOptrBltnDivEnmn:
                case IraOptrBltnModEnmn:
                case IraOptrBltnAddEnmn:
                case IraOptrBltnSubEnmn:
                case IraOptrBltnLeShiftEnmn:
                case IraOptrBltnRiShiftEnmn:
                case IraOptrBltnLessEnmn:
                case IraOptrBltnLessEqEnmn:
                case IraOptrBltnGrtrEnmn:
                case IraOptrBltnGrtrEqEnmn:
                case IraOptrBltnEqEnmn:
                case IraOptrBltnNeqEnmn:
                case IraOptrBltnAndEnmn:
                case IraOptrBltnXorEnmn:
                case IraOptrBltnOrEnmn:
                    return true;
                default:
                    break;
            }
        default:
            ul_assert_unreachable();
    }

    return false;
}

ira_optr_t * ira_optr_copy(ira_optr_t * optr)
{
    ira_optr_t * new_optr = ira_optr_create(optr->type);

    switch (optr->type)
    {
        case IraOptrNone:
        case IraOptrBltnLogicNegBool:
        case IraOptrBltnArithNegInt:
        case IraOptrBltnMulInt:
        case IraOptrBltnDivInt:
        case IraOptrBltnModInt:
        case IraOptrBltnAddInt:
        case IraOptrBltnSubInt:
        case IraOptrBltnLeShiftInt:
        case IraOptrBltnRiShiftInt:
        case IraOptrBltnLessInt:
        case IraOptrBltnLessEqInt:
        case IraOptrBltnGrtrInt:
        case IraOptrBltnGrtrEqInt:
        case IraOptrBltnEqInt:
        case IraOptrBltnNeqInt:
        case IraOptrBltnAndInt:
        case IraOptrBltnXorInt:
        case IraOptrBltnOrInt:
        case IraOptrBltnLessPtr:
        case IraOptrBltnLessEqPtr:
        case IraOptrBltnGrtrPtr:
        case IraOptrBltnGrtrEqPtr:
        case IraOptrBltnEqPtr:
        case IraOptrBltnNeqPtr:
        case IraOptrBltnArithNegEnmn:
        case IraOptrBltnMulEnmn:
        case IraOptrBltnDivEnmn:
        case IraOptrBltnModEnmn:
        case IraOptrBltnAddEnmn:
        case IraOptrBltnSubEnmn:
        case IraOptrBltnLeShiftEnmn:
        case IraOptrBltnRiShiftEnmn:
        case IraOptrBltnLessEnmn:
        case IraOptrBltnLessEqEnmn:
        case IraOptrBltnGrtrEnmn:
        case IraOptrBltnGrtrEqEnmn:
        case IraOptrBltnEqEnmn:
        case IraOptrBltnNeqEnmn:
        case IraOptrBltnAndEnmn:
        case IraOptrBltnXorEnmn:
        case IraOptrBltnOrEnmn:
            break;
        default:
            ul_assert_unreachable();
    }

    return new_optr;
}

const ira_optr_info_t ira_optr_infos[IraOptr_Count] = {
    [IraOptrNone] = { .is_bltn = false, .bltn = { .is_unr = false, .ctg = IraOptrCtgNone, .opd_dt_type = IraDtVoid } },
    [IraOptrBltnLogicNegBool] = { .is_bltn = true, .bltn = { .is_unr = true, .ctg = IraOptrCtgLogicNeg, .opd_dt_type = IraDtBool } },
    [IraOptrBltnArithNegInt] = { .is_bltn = true, .bltn = { .is_unr = true, .ctg = IraOptrCtgArithNeg, .opd_dt_type = IraDtInt } },
    [IraOptrBltnMulInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgMul, .opd_dt_type = IraDtInt } },
    [IraOptrBltnDivInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgDiv, .opd_dt_type = IraDtInt } },
    [IraOptrBltnModInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgMod, .opd_dt_type = IraDtInt } },
    [IraOptrBltnAddInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgAdd, .opd_dt_type = IraDtInt } },
    [IraOptrBltnSubInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgSub, .opd_dt_type = IraDtInt } },
    [IraOptrBltnLeShiftInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLeShift, .opd_dt_type = IraDtInt } },
    [IraOptrBltnRiShiftInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgRiShift, .opd_dt_type = IraDtInt } },
    [IraOptrBltnLessInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLess, .opd_dt_type = IraDtInt } },
    [IraOptrBltnLessEqInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLessEq, .opd_dt_type = IraDtInt } },
    [IraOptrBltnGrtrInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgGrtr, .opd_dt_type = IraDtInt } },
    [IraOptrBltnGrtrEqInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgGrtrEq, .opd_dt_type = IraDtInt } },
    [IraOptrBltnEqInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgEq, .opd_dt_type = IraDtInt } },
    [IraOptrBltnNeqInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgNeq, .opd_dt_type = IraDtInt } },
    [IraOptrBltnAndInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgBitAnd, .opd_dt_type = IraDtInt } },
    [IraOptrBltnXorInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgBitXor, .opd_dt_type = IraDtInt } },
    [IraOptrBltnOrInt] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgBitOr, .opd_dt_type = IraDtInt } },
    [IraOptrBltnLessPtr] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLess, .opd_dt_type = IraDtPtr } },
    [IraOptrBltnLessEqPtr] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLessEq, .opd_dt_type = IraDtPtr } },
    [IraOptrBltnGrtrPtr] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgGrtr, .opd_dt_type = IraDtPtr } },
    [IraOptrBltnGrtrEqPtr] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgGrtrEq, .opd_dt_type = IraDtPtr } },
    [IraOptrBltnEqPtr] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgEq, .opd_dt_type = IraDtPtr } },
    [IraOptrBltnNeqPtr] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgNeq, .opd_dt_type = IraDtPtr } },
    [IraOptrBltnArithNegEnmn] = { .is_bltn = true, .bltn = { .is_unr = true, .ctg = IraOptrCtgArithNeg, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnMulEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgMul, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnDivEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgDiv, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnModEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgMod, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnAddEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgAdd, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnSubEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgSub, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnLeShiftEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLeShift, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnRiShiftEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgRiShift, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnLessEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLess, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnLessEqEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgLessEq, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnGrtrEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgGrtr, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnGrtrEqEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgGrtrEq, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnEqEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgEq, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnNeqEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgNeq, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnAndEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgBitAnd, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnXorEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgBitXor, .opd_dt_type = IraDtEnmn } },
    [IraOptrBltnOrEnmn] = { .is_bltn = true, .bltn = { .is_unr = false, .ctg = IraOptrCtgBitOr, .opd_dt_type = IraDtEnmn } }
};
