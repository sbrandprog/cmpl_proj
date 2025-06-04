#include "pla_tok.h"

extern inline bool pla_tok_is_emp_ch(char ch);

extern inline bool pla_tok_is_punc_ch(char ch);

extern inline bool pla_tok_is_first_ident_ch(char ch);
extern inline bool pla_tok_is_ident_ch(char ch);

extern inline bool pla_tok_is_tag_ch(char ch);

extern inline bool pla_tok_is_dqoute_ch(char ch);
extern inline bool pla_tok_is_ch_str_ch(char ch);
extern inline bool pla_tok_is_first_num_str_ch(char ch);
extern inline bool pla_tok_is_num_str_ch(char ch);
extern inline bool pla_tok_is_num_str_tag_intro_ch(char ch);

void pla_tok_init(pla_tok_t * tok, pla_tok_type_t type)
{
    *tok = (pla_tok_t){ .type = type };
}
void pla_tok_copy(pla_tok_t * dst, const pla_tok_t * src)
{
    *dst = *src;
}
void pla_tok_cleanup(pla_tok_t * tok)
{
    memset(tok, 0, sizeof(*tok));
}
