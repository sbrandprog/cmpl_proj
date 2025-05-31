#pragma once
#include "pla_ec.h"
#include "pla_keyw.h"
#include "pla_punc.h"

enum pla_tok_type
{
    PlaTokNone,
    PlaTokPunc,
    PlaTokKeyw,
    PlaTokIdent,
    PlaTokChStr,
    PlaTokNumStr,
    PlaTok_Count
};
struct pla_tok
{
    pla_tok_type_t type;

    pla_ec_pos_t pos_start;
    pla_ec_pos_t pos_end;

    union
    {
        pla_punc_t punc;
        pla_keyw_t keyw;
        ul_hs_t * ident;
        struct
        {
            ul_hs_t * data;
            ul_hs_t * tag;
        } ch_str, num_str;
    };
};

inline bool pla_tok_is_emp_ch(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n';
}

inline bool pla_tok_is_punc_ch(char ch)
{
    return ('!' <= ch && ch <= '/' && ch != '\"' && ch != '\'') || (':' <= ch && ch <= '>') || ('[' <= ch && ch <= '^') || ('{' <= ch && ch <= '~');
}

inline bool pla_tok_is_first_ident_ch(char ch)
{
    return ('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') || ch == '_';
}
inline bool pla_tok_is_ident_ch(char ch)
{
    return pla_tok_is_first_ident_ch(ch) || ('0' <= ch && ch <= '9');
}

inline bool pla_tok_is_tag_ch(char ch)
{
    return pla_tok_is_ident_ch(ch);
}

inline bool pla_tok_is_dqoute_ch(char ch)
{
    return ch == '\"';
}
inline bool pla_tok_is_ch_str_ch(char ch)
{
    return !pla_tok_is_dqoute_ch(ch) && ch != '\n';
}

inline bool pla_tok_is_first_num_str_ch(char ch)
{
    return ('0' <= ch && ch <= '9');
}
inline bool pla_tok_is_num_str_ch(char ch)
{
    return pla_tok_is_first_num_str_ch(ch) || ('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') || ch == '_';
}
inline bool pla_tok_is_num_str_tag_intro_ch(char ch)
{
    return ch == '\'';
}

PLA_API void pla_tok_init(pla_tok_t * tok, pla_tok_type_t type);
PLA_API void pla_tok_copy(pla_tok_t * dst, const pla_tok_t * src);
PLA_API void pla_tok_cleanup(pla_tok_t * tok);
