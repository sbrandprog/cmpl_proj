#include "ira_dt.h"
#include "ira_int.h"

bool ira_dt_is_qual_equal(ira_dt_qual_t first, ira_dt_qual_t second)
{
    if (first.const_q != second.const_q)
    {
        return false;
    }

    return true;
}

bool ira_dt_is_func_vas_equivalent(ira_dt_func_vas_t * first, ira_dt_func_vas_t * second)
{
    if (first == second)
    {
        return true;
    }

    if (first->type != second->type)
    {
        return false;
    }

    return true;
}

bool ira_dt_is_equivalent(ira_dt_t * first, ira_dt_t * second)
{
    if (first == second)
    {
        return true;
    }

    if (first->type != second->type)
    {
        return false;
    }

    switch (first->type)
    {
        case IraDtVoid:
        case IraDtDt:
        case IraDtBool:
            break;
        case IraDtInt:
            if (first->int_type != second->int_type)
            {
                return false;
            }
            break;
        case IraDtVec:
            if (first->vec.size != second->vec.size)
            {
                return false;
            }

            if (!ira_dt_is_qual_equal(first->vec.qual, second->vec.qual))
            {
                return false;
            }

            if (!ira_dt_is_equivalent(first->vec.body, second->vec.body))
            {
                return false;
            }
            break;
        case IraDtPtr:
            if (!ira_dt_is_qual_equal(first->ptr.qual, second->ptr.qual))
            {
                return false;
            }

            if (!ira_dt_is_equivalent(first->ptr.body, second->ptr.body))
            {
                return false;
            }

            break;
        case IraDtTpl:
            if (!ira_dt_is_qual_equal(first->tpl.qual, second->tpl.qual))
            {
                return false;
            }

            if (first->tpl.elems_size != second->tpl.elems_size)
            {
                return false;
            }

            for (ira_dt_ndt_t *f_elem = first->tpl.elems, *f_elem_end = f_elem + first->tpl.elems_size, *s_elem = second->tpl.elems; f_elem != f_elem_end; ++f_elem, ++s_elem)
            {
                if (!ira_dt_is_equivalent(f_elem->dt, s_elem->dt))
                {
                    return false;
                }
            }
            break;
        case IraDtStct:
            if (!ira_dt_is_qual_equal(first->stct.qual, second->stct.qual))
            {
                return false;
            }

            if (first->stct.tag != second->stct.tag)
            {
                ira_dt_t *first_body = first->stct.tag->body, *second_body = second->stct.tag->body;

                if (first_body == NULL || second_body == NULL)
                {
                    return false;
                }

                if (!ira_dt_is_equivalent(first_body, second_body))
                {
                    return false;
                }
            }
            break;
        case IraDtArr:
            if (!ira_dt_is_qual_equal(first->arr.qual, second->arr.qual))
            {
                return false;
            }

            if (!ira_dt_is_equivalent(first->arr.body, second->arr.body))
            {
                return false;
            }
            break;
        case IraDtFunc:
            if (first->func.args_size != second->func.args_size)
            {
                return false;
            }

            if (!ira_dt_is_equivalent(first->func.ret, second->func.ret))
            {
                return false;
            }

            if (!ira_dt_is_func_vas_equivalent(first->func.vas, second->func.vas))
            {
                return false;
            }

            for (ira_dt_ndt_t *f_arg = first->func.args, *f_arg_end = f_arg + first->func.args_size, *s_arg = second->func.args; f_arg != f_arg_end; ++f_arg, ++s_arg)
            {
                if (!ira_dt_is_equivalent(f_arg->dt, s_arg->dt))
                {
                    return false;
                }
            }
            break;
        case IraDtEnmn:
            if (first->enmn.tag != second->enmn.tag)
            {
                if (!ira_dt_is_equivalent(first->enmn.tag->body, second->enmn.tag->body))
                {
                    return false;
                }
            }

            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}

static bool is_castable_to_void(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
        case IraDtEnmn:
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_dt(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
        case IraDtDt:
            break;
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
        case IraDtEnmn:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_bool(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
            return false;
        case IraDtBool:
            break;
        case IraDtInt:
            if (implct)
            {
                return false;
            }
            break;
        case IraDtVec:
            return false;
        case IraDtPtr:
            if (implct)
            {
                return false;
            }
            break;
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
            return false;
        case IraDtEnmn:
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_int(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
            return false;
        case IraDtBool:
            break;
        case IraDtInt:
            if (implct)
            {
                const ira_int_info_t *from_info = &ira_int_infos[from->int_type], *to_info = &ira_int_infos[to->int_type];

                if (from_info->sign != to_info->sign || from_info->size > to_info->size)
                {
                    return false;
                }
            }
            break;
        case IraDtVec:
            return false;
        case IraDtPtr:
            if (implct)
            {
                return false;
            }
            break;
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
            return false;
        case IraDtEnmn:
            if (!is_castable_to_int(from->enmn.tag->body, to, implct))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_vec(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
            return false;
        case IraDtVec:
            if (!ira_dt_is_equivalent(from->vec.body, to->vec.body))
            {
                return false;
            }

            if (implct)
            {
                if (from->vec.size != to->vec.size)
                {
                    return false;
                }
            }
            break;
        case IraDtPtr:
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
        case IraDtEnmn:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_ptr(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
            return false;
        case IraDtBool:
        case IraDtInt:
            if (implct)
            {
                return false;
            }
            break;
        case IraDtVec:
            return false;
        case IraDtPtr:
            if (implct)
            {
                if (!ira_dt_is_castable(from->ptr.body, to->ptr.body, implct))
                {
                    return false;
                }

                if (!ira_dt_is_qual_equal(ira_dt_apply_qual(from->ptr.qual, to->ptr.qual), to->ptr.qual))
                {
                    return false;
                }
            }
            break;
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
            return false;
        case IraDtEnmn:
            if (!is_castable_to_ptr(from->enmn.tag->body, to, implct))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_tpl(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
            return false;
        case IraDtTpl:
            if (!ira_dt_is_equivalent(from, to))
            {
                return false;
            }
            break;
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
        case IraDtEnmn:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_stct(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
        case IraDtTpl:
            return false;
        case IraDtStct:
            if (!ira_dt_is_equivalent(from, to))
            {
                return false;
            }
            break;
        case IraDtArr:
        case IraDtFunc:
        case IraDtEnmn:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_arr(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
        case IraDtTpl:
        case IraDtStct:
            return false;
        case IraDtArr:
            if (!ira_dt_is_equivalent(from, to))
            {
                return false;
            }
            break;
        case IraDtFunc:
        case IraDtEnmn:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_func(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
        case IraDtVec:
        case IraDtPtr:
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
            return false;
        case IraDtFunc:
            if (!ira_dt_is_equivalent(from, to))
            {
                return false;
            }
            break;
        case IraDtEnmn:
            return false;
        default:
            ul_assert_unreachable();
    }

    return true;
}
static bool is_castable_to_enmn(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    switch (from->type)
    {
        case IraDtVoid:
            break;
        case IraDtDt:
        case IraDtBool:
            return false;
        case IraDtInt:
            if (implct)
            {
                return false;
            }
            break;
        case IraDtVec:
        case IraDtPtr:
        case IraDtTpl:
        case IraDtStct:
        case IraDtArr:
        case IraDtFunc:
            return false;
        case IraDtEnmn:
            if (implct)
            {
                return false;
            }

            if (!ira_dt_is_equivalent(from, to))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}
bool ira_dt_is_castable(ira_dt_t * from, ira_dt_t * to, bool implct)
{
    if (from->type >= IraDt_Count || to->type >= IraDt_Count)
    {
        ul_assert_unreachable();
        return false;
    }

    if (from->type == IraDtStct && from->stct.tag->body == NULL
        || to->type == IraDtStct && to->stct.tag->body == NULL)
    {
        return false;
    }

    switch (to->type)
    {
        case IraDtVoid:
            if (!is_castable_to_void(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtDt:
            if (!is_castable_to_dt(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtBool:
            if (!is_castable_to_bool(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtInt:
            if (!is_castable_to_int(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtVec:
            if (!is_castable_to_vec(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtPtr:
            if (!is_castable_to_ptr(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtTpl:
            if (!is_castable_to_tpl(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtStct:
            if (!is_castable_to_stct(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtArr:
            if (!is_castable_to_arr(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtFunc:
            if (!is_castable_to_func(from, to, implct))
            {
                return false;
            }
            break;
        case IraDtEnmn:
            if (!is_castable_to_enmn(from, to, implct))
            {
                return false;
            }
            break;
        default:
            ul_assert_unreachable();
    }

    return true;
}

ira_dt_qual_t ira_dt_get_qual(ira_dt_t * dt)
{
    switch (dt->type)
    {
        case IraDtVoid:
        case IraDtDt:
        case IraDtBool:
        case IraDtInt:
            return ira_dt_qual_none;
        case IraDtVec:
            return dt->vec.qual;
        case IraDtPtr:
            return dt->ptr.qual;
        case IraDtTpl:
            return dt->tpl.qual;
        case IraDtStct:
            return dt->stct.qual;
        case IraDtArr:
            return dt->arr.qual;
        case IraDtFunc:
            return ira_dt_qual_none;
        default:
            ul_assert_unreachable();
    }
}

ira_dt_qual_t ira_dt_apply_qual(ira_dt_qual_t to, ira_dt_qual_t from)
{
    to.const_q = to.const_q || from.const_q;
    return to;
}

const ira_dt_qual_t ira_dt_qual_none = { 0 };
const ira_dt_qual_t ira_dt_qual_const = { .const_q = true };

const ira_dt_info_t ira_dt_infos[IraDt_Count] = {
    [IraDtVoid] = { .type_str = UL_ROS_MAKE("DtVoid") },
    [IraDtDt] = { .type_str = UL_ROS_MAKE("DtDt") },
    [IraDtBool] = { .type_str = UL_ROS_MAKE("DtBool") },
    [IraDtInt] = { .type_str = UL_ROS_MAKE("DtInt") },
    [IraDtVec] = { .type_str = UL_ROS_MAKE("DtVec") },
    [IraDtPtr] = { .type_str = UL_ROS_MAKE("DtPtr") },
    [IraDtTpl] = { .type_str = UL_ROS_MAKE("DtTpl") },
    [IraDtStct] = { .type_str = UL_ROS_MAKE("DtStct") },
    [IraDtArr] = { .type_str = UL_ROS_MAKE("DtArr") },
    [IraDtFunc] = { .type_str = UL_ROS_MAKE("DtFunc") },
    [IraDtEnmn] = { .type_str = UL_ROS_MAKE("DtEnmn") }
};
