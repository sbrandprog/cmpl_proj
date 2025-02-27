#include "pla_tok.h"

#if defined __linux__
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
#endif
