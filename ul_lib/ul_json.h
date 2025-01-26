#pragma once
#include "ul.h"

#define UL_JSON_NULL "null"
#define UL_JSON_FALSE "false"
#define UL_JSON_TRUE "true"
#define UL_JSON_NAME_SEP ':'
#define UL_JSON_VAL_SEP ','
#define UL_JSON_QUOT_MARK '\"'
#define UL_JSON_ARR_BEGIN '['
#define UL_JSON_ARR_END ']'
#define UL_JSON_OBJ_BEGIN '{'
#define UL_JSON_OBJ_END '}'

enum ul_json_type
{
    UlJsonNull,
    UlJsonBool,
    UlJsonInt,
    UlJsonDbl,
    UlJsonStr,
    UlJsonArr,
    UlJsonObj,
    UlJson_Count
};
struct ul_json
{
    ul_json_type_t type;
    ul_json_t * next;
    const ul_hs_t * name;
    union
    {
        bool val_bool;
        int64_t val_int;
        double val_dbl;
        const ul_hs_t * val_str;
        ul_json_t * val_json;
    };
};

UL_API ul_json_t * ul_json_create(ul_json_type_t type);
UL_API void ul_json_destroy(ul_json_t * json);

UL_API void ul_json_destroy_chain(ul_json_t * json);

inline ul_json_t * ul_json_make_null()
{
    return ul_json_create(UlJsonNull);
}
inline ul_json_t * ul_json_make_bool(bool val)
{
    ul_json_t * json = ul_json_create(UlJsonBool);

    json->val_bool = val;

    return json;
}
inline ul_json_t * ul_json_make_int(int64_t val)
{
    ul_json_t * json = ul_json_create(UlJsonInt);

    json->val_int = val;

    return json;
}
inline ul_json_t * ul_json_make_dbl(double val)
{
    ul_json_t * json = ul_json_create(UlJsonDbl);

    json->val_dbl = val;

    return json;
}
inline ul_json_t * ul_json_make_str(const ul_hs_t * val)
{
    ul_json_t * json = ul_json_create(UlJsonStr);

    json->val_str = val;

    return json;
}
inline ul_json_t * ul_json_make_arr()
{
    return ul_json_create(UlJsonArr);
}
inline ul_json_t * ul_json_make_obj()
{
    return ul_json_create(UlJsonObj);
}
