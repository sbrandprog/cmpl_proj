#include "ira_val.h"
#include "ira_dt.h"
#include "ira_lo.h"

ira_val_t * ira_val_create(ira_val_type_t type, ira_dt_t * dt)
{
    ira_val_t * val = malloc(sizeof(*val));

    ul_assert(val != NULL);

    memset(val, 0, sizeof(*val));

    *val = (ira_val_t){ .type = type, .dt = dt };

    return val;
}
void ira_val_destroy(ira_val_t * val)
{
    if (val == NULL)
    {
        return;
    }

    const ira_val_info_t * info = &ira_val_infos[val->type];

    switch (info->strg_type)
    {
        case IraValStrgNone:
        case IraValStrgDt:
        case IraValStrgBool:
        case IraValStrgInt:
        case IraValStrgLo:
            break;
        case IraValStrgVal:
            ira_val_destroy(val->val_val);
            break;
        case IraValStrgArr:
            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size; elem != elem_end; ++elem)
            {
                ira_val_destroy(*elem);
            }

            free(val->arr_val.data);

            break;
        default:
            ul_assert_unreachable();
    }

    free(val);
}

ira_val_t * ira_val_copy(ira_val_t * val)
{
    ira_val_t * new_val = ira_val_create(val->type, val->dt);

    const ira_val_info_t * info = &ira_val_infos[val->type];

    switch (info->strg_type)
    {
        case IraValStrgNone:
            break;
        case IraValStrgDt:
            new_val->dt_val = val->dt_val;
            break;
        case IraValStrgBool:
            new_val->bool_val = val->bool_val;
            break;
        case IraValStrgInt:
            new_val->int_val = val->int_val;
            break;
        case IraValStrgLo:
            new_val->lo_val = val->lo_val;
            break;
        case IraValStrgVal:
            new_val->val_val = ira_val_copy(val->val_val);
            break;
        case IraValStrgArr:
            new_val->arr_val.data = malloc(val->arr_val.size * sizeof(*new_val->arr_val.data));

            ul_assert(new_val->arr_val.data != NULL);

			new_val->arr_val.cap = val->arr_val.size;

            for (ira_val_t **elem = val->arr_val.data, **elem_end = elem + val->arr_val.size, **ins = new_val->arr_val.data; elem != elem_end; ++elem, ++ins)
            {
                *ins = ira_val_copy(*elem);

				++new_val->arr_val.size;
            }

            break;
        default:
            ul_assert_unreachable();
    }

    return new_val;
}

const ira_val_info_t ira_val_infos[IraVal_Count] = {
    [IraValImmVoid] = { .strg_type = IraValStrgNone },
    [IraValImmDt] = { .strg_type = IraValStrgDt },
    [IraValImmBool] = { .strg_type = IraValStrgBool },
    [IraValImmInt] = { .strg_type = IraValStrgInt },
    [IraValImmVec] = { .strg_type = IraValStrgArr },
    [IraValImmPtr] = { .strg_type = IraValStrgInt },
    [IraValLoPtr] = { .strg_type = IraValStrgLo },
    [IraValImmTpl] = { .strg_type = IraValStrgArr },
    [IraValImmStct] = { .strg_type = IraValStrgVal },
    [IraValImmArr] = { .strg_type = IraValStrgArr },
    [IraValImmEnmn] = { .strg_type = IraValStrgVal },
};
