#pragma once
#include "ira_int.h"

enum ira_val_type
{
    IraValImmVoid,
    IraValImmDt,
    IraValImmBool,
    IraValImmInt,
    IraValImmVec,
    IraValImmPtr,
    IraValLoPtr,
    IraValImmTpl,
    IraValImmStct,
    IraValImmArr,
    IraValImmEnmn,
    IraVal_Count
};
enum ira_val_strg_type
{
    IraValStrgNone,
    IraValStrgDt,
    IraValStrgBool,
    IraValStrgInt,
    IraValStrgLo,
    IraValStrgVal,
    IraValStrgArr,
    IraValStrg_Count
};
struct ira_val_info
{
    ira_val_strg_type_t strg_type;
};
struct ira_val
{
    ira_val_type_t type;
    ira_dt_t * dt;
    union
    {
        ira_dt_t * dt_val;
        bool bool_val;
        ira_int_t int_val;
        ira_lo_t * lo_val;
        ira_val_t * val_val;
        struct
        {
            size_t size;
            ira_val_t ** data;
            size_t cap;
        } arr_val;
    };
};

IRA_API ira_val_t * ira_val_create(ira_val_type_t type, ira_dt_t * dt);
IRA_API void ira_val_destroy(ira_val_t * val);

IRA_API ira_val_t * ira_val_copy(ira_val_t * val);

extern IRA_API const ira_val_info_t ira_val_infos[IraVal_Count];
