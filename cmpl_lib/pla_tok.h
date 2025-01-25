#pragma once
#include "pla_ec.h"

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

inline bool pla_tok_is_emp_ch(wchar_t ch)
{
    return ch == L' ' || ch == '\t' || ch == L'\n';
}

inline bool pla_tok_is_punc_ch(wchar_t ch)
{
    return (L'!' <= ch && ch <= L'/' && ch != L'\"' && ch != '\'') || (L':' <= ch && ch <= L'>') || (L'[' <= ch && ch <= L'^') || (L'{' <= ch && ch <= L'~');
}

inline bool pla_tok_is_first_ident_ch(wchar_t ch)
{
    return (L'A' <= ch && ch <= L'Z') || (L'a' <= ch && ch <= L'z') || ch == L'_';
}
inline bool pla_tok_is_ident_ch(wchar_t ch)
{
    return pla_tok_is_first_ident_ch(ch) || (L'0' <= ch && ch <= L'9');
}

inline bool pla_tok_is_tag_ch(wchar_t ch)
{
    return pla_tok_is_ident_ch(ch);
}

inline bool pla_tok_is_dqoute_ch(wchar_t ch)
{
    return ch == L'\"';
}
inline bool pla_tok_is_ch_str_ch(wchar_t ch)
{
    return !pla_tok_is_dqoute_ch(ch) && ch != L'\n';
}

inline bool pla_tok_is_first_num_str_ch(wchar_t ch)
{
    return (L'0' <= ch && ch <= L'9');
}
inline bool pla_tok_is_num_str_ch(wchar_t ch)
{
    return pla_tok_is_first_num_str_ch(ch) || (L'A' <= ch && ch <= L'Z') || (L'a' <= ch && ch <= L'z') || ch == L'_';
}
inline bool pla_tok_is_num_str_tag_intro_ch(wchar_t ch)
{
    return ch == L'\'';
}
