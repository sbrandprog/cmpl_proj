#include "pla_lex.h"
#include "pla_ec.h"
#include "pla_keyw.h"
#include "pla_punc.h"

typedef bool ch_pred_t(char ch);
typedef void ch_proc_t(pla_lex_t * lex, char * out);

void pla_lex_src_init(pla_lex_src_t * src, ul_hs_t * name)
{
    *src = (pla_lex_src_t){ .name = name };
}
void pla_lex_src_cleanup(pla_lex_src_t * src)
{
    memset(src, 0, sizeof(*src));
}

void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr)
{
    *lex = (pla_lex_t){ .hst = hst, .ec_fmtr = ec_fmtr };

    static const ul_ros_t emp_str_raw = UL_ROS_MAKE("");

    lex->emp_hs = ul_hst_hashadd(hst, emp_str_raw.size, emp_str_raw.str);

    pla_tok_init(&lex->tok, PlaTokNone);
}
void pla_lex_cleanup(pla_lex_t * lex)
{
    pla_tok_cleanup(&lex->tok);

    free(lex->str);

    memset(lex, 0, sizeof(*lex));
}

static bool next_ch(pla_lex_t * lex)
{
    if (lex->ch_succ)
    {
        ++lex->line_ch;
    }

    lex->ch_succ = false;

    if (lex->src == NULL || !lex->src->get_ch_proc(lex->src->data, &lex->ch))
    {
        return false;
    }

    lex->ch_succ = true;

    if (lex->line_switch)
    {
        lex->line_switch = false;
        ++lex->line_num;
        lex->line_ch = 0;
    }

    if (lex->ch == '\n')
    {
        lex->line_switch = true;
    }

    return true;
}

static void update_src(pla_lex_t * lex, const pla_lex_src_t * src)
{
    lex->src = src;

    lex->ch = 0;
    lex->ch_succ = false;

    if (!next_ch(lex))
    {
        return;
    }
}
void pla_lex_set_src(pla_lex_t * lex, const pla_lex_src_t * src)
{
    lex->line_switch = false;
    if (src != NULL)
    {
        lex->line_num = src->line_num;
        lex->line_ch = src->line_ch;
    }
    else
    {
        lex->line_num = 0;
        lex->line_ch = 0;
    }

    update_src(lex, src);
}

static pla_ec_pos_t get_ec_pos(pla_lex_t * lex)
{
    return (pla_ec_pos_t){
        .line_num = lex->line_num,
        .line_ch = lex->line_ch
    };
}
static void report_lex(pla_lex_t * lex, const char * fmt, ...)
{
    lex->is_rptd = true;

    va_list args;

    va_start(args, fmt);

    pla_ec_formatpost_va(lex->ec_fmtr, PLA_LEX_MOD_NAME, (lex->src != NULL ? lex->src->name->str : NULL), lex->pos_start, get_ec_pos(lex), fmt, args);

    va_end(args);
}

static void clear_str(pla_lex_t * lex)
{
    lex->str_size = 0;
}
static void push_str_ch(pla_lex_t * lex, char ch)
{
    ul_arr_grow(lex->str_size + 1, &lex->str_cap, (void **)&lex->str, sizeof(*lex->str));

    lex->str[lex->str_size++] = ch;
}
static void fetch_str(pla_lex_t * lex, ch_pred_t * pred, ch_proc_t * proc)
{
    clear_str(lex);

    do
    {
        char str_ch = lex->ch;

        if (proc != NULL)
        {
            proc(lex, &str_ch);
        }

        push_str_ch(lex, str_ch);
    } while (next_ch(lex) && pred(lex->ch));
}
static ul_hs_t * hadd_str(pla_lex_t * lex)
{
    return ul_hst_hashadd(lex->hst, lex->str_size, lex->str);
}
static void fadd_str(pla_lex_t * lex, ch_pred_t * pred, ch_proc_t * proc, ul_hs_t ** out)
{
    fetch_str(lex, pred, proc);

    *out = hadd_str(lex);
}
static void ch_str_ch_proc(pla_lex_t * lex, char * out)
{
    if (lex->ch == '\\')
    {
        if (!next_ch(lex))
        {
            report_lex(lex, "invalid escape sequence: no character", lex->ch);
            return;
        }

        switch (lex->ch)
        {
            case 'a':
                *out = '\a';
                break;
            case 'b':
                *out = '\b';
                break;
            case 'f':
                *out = '\f';
                break;
            case 'n':
                *out = '\n';
                break;
            case 'r':
                *out = '\r';
                break;
            case 't':
                *out = '\t';
                break;
            case 'v':
                *out = '\v';
                break;
            case '\\':
                *out = '\\';
                break;
            case '\'':
                *out = '\'';
                break;
            case '\"':
                *out = '\"';
                break;
            default:
                report_lex(lex, "invalid escape sequence: \\%c", lex->ch);
                break;
        }
    }
}
static void get_tok_core(pla_lex_t * lex)
{
    if (!lex->ch_succ)
    {
        return;
    }

    while (true)
    {
        lex->pos_start = get_ec_pos(lex);

        if (pla_tok_is_emp_ch(lex->ch))
        {
            do
            {
                if (!next_ch(lex))
                {
                    return;
                }
            } while (pla_tok_is_emp_ch(lex->ch));
        }
        else if (pla_tok_is_punc_ch(lex->ch))
        {
            clear_str(lex);

            push_str_ch(lex, lex->ch);

            size_t punc_len;
            pla_punc_t punc = pla_punc_fetch_best(lex->str_size, lex->str, &punc_len);

            if (punc == PlaPuncNone)
            {
                next_ch(lex);
                report_lex(lex, "invalid punctuator: undefined punctuator");
                return;
            }

            size_t punc_len_old = punc_len;

            while (next_ch(lex) && pla_tok_is_punc_ch(lex->ch))
            {
                push_str_ch(lex, lex->ch);

                punc = pla_punc_fetch_best(lex->str_size, lex->str, &punc_len);

                if (punc_len <= punc_len_old)
                {
                    break;
                }

                punc_len_old = punc_len;
            }

            pla_tok_init(&lex->tok, PlaTokPunc);
            lex->tok.punc = punc;

            return;
        }
        else if (pla_tok_is_first_ident_ch(lex->ch))
        {
            fetch_str(lex, pla_tok_is_ident_ch, NULL);

            pla_keyw_t keyw = pla_keyw_fetch_exact(lex->str_size, lex->str);

            if (keyw != PlaKeywNone)
            {
                pla_tok_init(&lex->tok, PlaTokKeyw);
                lex->tok.keyw = keyw;

                return;
            }

            ul_hs_t * str = hadd_str(lex);

            pla_tok_init(&lex->tok, PlaTokIdent);
            lex->tok.ident = str;

            return;
        }
        else if (pla_tok_is_dqoute_ch(lex->ch))
        {
            ul_hs_t *data = lex->emp_hs, *tag = lex->emp_hs;

            do
            {
                if (!next_ch(lex))
                {
                    report_lex(lex, "invalid character string: no string characters");
                    break;
                }

                if (lex->ch_succ && !pla_tok_is_dqoute_ch(lex->ch))
                {
                    fadd_str(lex, pla_tok_is_ch_str_ch, ch_str_ch_proc, &data);
                }

                if (!lex->ch_succ || !pla_tok_is_dqoute_ch(lex->ch))
                {
                    report_lex(lex, "invalid character string: no enclosing double-quote");
                    break;
                }

                if (!next_ch(lex) || !pla_tok_is_tag_ch(lex->ch))
                {
                    report_lex(lex, "invalid character string: no type-tag");
                    break;
                }

                fadd_str(lex, pla_tok_is_tag_ch, NULL, &tag);

            } while (false);

            pla_tok_init(&lex->tok, PlaTokChStr);
            lex->tok.ch_str.data = data;
            lex->tok.ch_str.tag = tag;

            return;
        }
        else if (pla_tok_is_first_num_str_ch(lex->ch))
        {
            ul_hs_t *data = lex->emp_hs, *tag = lex->emp_hs;

            do
            {
                fadd_str(lex, pla_tok_is_num_str_ch, NULL, &data);

                if (!lex->ch_succ || !pla_tok_is_num_str_tag_intro_ch(lex->ch) || !next_ch(lex) || !pla_tok_is_tag_ch(lex->ch))
                {
                    report_lex(lex, "invalid number string: no type-tag");
                    break;
                }

                fadd_str(lex, pla_tok_is_tag_ch, NULL, &tag);

            } while (false);

            pla_tok_init(&lex->tok, PlaTokNumStr);
            lex->tok.num_str.data = data;
            lex->tok.num_str.tag = tag;

            return;
        }
        else
        {
            report_lex(lex, "undefined character: %c", lex->ch);
            next_ch(lex);
            return;
        }
    }
}
bool pla_lex_get_tok(pla_lex_t * lex)
{
    lex->is_rptd = false;

    pla_tok_cleanup(&lex->tok);

    lex->pos_start = get_ec_pos(lex);

    get_tok_core(lex);

    if (lex->tok.type != PlaTokNone)
    {
        lex->tok.pos_start = lex->pos_start;
        lex->tok.pos_end = get_ec_pos(lex);

        return true;
    }

    return false;
}
