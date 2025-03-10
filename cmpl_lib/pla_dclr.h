#pragma once
#include "ira_dt.h"
#include "pla_ec.h"

enum pla_dclr_type
{
    PlaDclrNone,
    PlaDclrNspc,
    PlaDclrFunc,
    PlaDclrImpt,
    PlaDclrVarDt,
    PlaDclrVarVal,
    PlaDclrDtStct,
    PlaDclrDtStctDecl,
    PlaDclrEnmn,
    PlaDclrEnmnElem,
    PlaDclr_Count
};
struct pla_dclr
{
    pla_dclr_type_t type;

    pla_dclr_t * next;

    pla_ec_pos_t pos_start;
    pla_ec_pos_t pos_end;

    ul_hs_t * name;

    union
    {
        struct
        {
            pla_dclr_t * body;
        } nspc;
        struct
        {
            pla_expr_t * dt_expr;
            pla_stmt_t * block;
        } func;
        struct
        {
            pla_expr_t * dt_expr;
            ul_hs_t * lib_name;
            ul_hs_t * sym_name;
        } impt;
        struct
        {
            pla_expr_t * dt_expr;
            ira_dt_qual_t dt_qual;
        } var_dt;
        struct
        {
            pla_expr_t * val_expr;
            ira_dt_qual_t dt_qual;
        } var_val;
        struct
        {
            pla_expr_t * dt_stct_expr;
        } dt_stct;
        struct
        {
            pla_expr_t * dt_expr;
            pla_dclr_t * elem;
        } enmn;
        struct
        {
            pla_expr_t * val;
        } enmn_elem;
    };
};

struct pla_dclr_info
{
    ul_ros_t type_str;
};

PLA_API pla_dclr_t * pla_dclr_create(pla_dclr_type_t type);
PLA_API void pla_dclr_destroy(pla_dclr_t * dclr);
PLA_API void pla_dclr_destroy_chain(pla_dclr_t * dclr);

extern PLA_API const pla_dclr_info_t pla_dclr_infos[PlaDclr_Count];
