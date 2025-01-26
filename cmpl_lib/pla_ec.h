#pragma once
#include "pla.h"

#define PLA_EC_REC_TYPE "pla_err"

struct pla_ec_pos
{
    size_t line_num;
    size_t line_ch;
};

struct pla_ec_err
{
    const char * mod_name;
    const char * src_name;
    pla_ec_pos_t pos_start;
    pla_ec_pos_t pos_end;
    const char * msg;
};

PLA_API bool pla_ec_scan(ul_ec_rec_t * rec, pla_ec_err_t * out);

PLA_API void pla_ec_formatpost_va(ul_ec_fmtr_t * fmtr, const char * mod_name, const char * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const char * fmt, va_list args);
PLA_API void pla_ec_formatpost(ul_ec_fmtr_t * fmtr, const char * mod_name, const char * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const char * fmt, ...);
