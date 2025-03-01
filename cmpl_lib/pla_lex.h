#pragma once
#include "pla_tok.h"

#define PLA_LEX_MOD_NAME "pla_lex"

struct pla_lex_src
{
    ul_hs_t * name;
    void * data;
    pla_lex_src_get_ch_proc_t * get_ch_proc;
    size_t line_num;
    size_t line_ch;
};
struct pla_lex
{
    ul_hst_t * hst;
    ul_ec_fmtr_t * ec_fmtr;

    ul_hs_t * emp_hs;

    const pla_lex_src_t * src;

    char ch;
    bool ch_succ;

    bool line_switch;
    size_t line_num;
    size_t line_ch;

    size_t str_size;
    char * str;
    size_t str_cap;

    pla_tok_t tok;

    bool is_rptd;
};

PLA_API void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr);
PLA_API void pla_lex_cleanup(pla_lex_t * lex);

PLA_API void pla_lex_set_src(pla_lex_t * lex, const pla_lex_src_t * src);

PLA_API bool pla_lex_get_tok(pla_lex_t * lex);
