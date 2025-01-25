#include "ul_json_p.h"
#include "ul_arr.h"
#include "ul_hst.h"
#include "ul_misc.h"

typedef struct ul_json_p_ctx
{
    ul_json_p_src_t * src;
    ul_json_t ** out;

    size_t str_size;
    wchar_t * str;
    size_t str_cap;

    wint_t ch;

    bool err;
} ctx_t;

typedef struct ul_json_p_file_ctx
{
    FILE * file;
} file_ctx_t;
typedef struct ul_json_p_str_ctx
{
    const wchar_t * cur;
    const wchar_t * cur_end;
} str_ctx_t;


void ul_json_p_src_init(ul_json_p_src_t * src, ul_hst_t * hst, void * data, ul_json_p_get_ch_proc_t * get_ch_proc)
{
    *src = (ul_json_p_src_t){ .hst = hst, .data = data, .get_ch_proc = get_ch_proc };
}
void ul_json_p_src_cleanup(ul_json_p_src_t * src)
{
    memset(src, 0, sizeof(*src));
}


static bool is_ws(wchar_t ch)
{
    switch (ch)
    {
        case L'\t':
        case L'\n':
        case L'\r':
        case L' ':
            return true;
    }

    return false;
}
static bool is_dig(wchar_t ch)
{
    return L'0' <= ch && ch <= L'9';
}

static bool get_hexdig(wchar_t ch, uint8_t * out)
{
    if (L'0' <= ch && ch <= L'9')
    {
        *out = ch - L'0';
    }
    else if (L'A' <= ch && ch <= L'F')
    {
        *out = ch - L'A' + 10;
    }
    else if (L'a' <= ch && ch <= L'f')
    {
        *out = ch - L'a' + 10;
    }
    else
    {
        return false;
    }

    return true;
}


static bool next_ch(ctx_t * ctx)
{
    ctx->ch = WEOF;

    if (ctx->src == NULL)
    {
        return false;
    }

    wchar_t new_ch;

    if (!ctx->src->get_ch_proc(ctx->src->data, &new_ch))
    {
        return false;
    }

    ctx->ch = (wint_t)new_ch;

    return true;
}
static bool consume_ch_exact(ctx_t * ctx, wchar_t ch)
{
    if (ctx->ch == ch)
    {
        next_ch(ctx);

        return true;
    }

    return false;
}
static void consume_ch_exact_crit(ctx_t * ctx, wchar_t ch)
{
    if (!consume_ch_exact(ctx, ch))
    {
        ctx->err = true;
    }
}
static bool consume_str_exact(ctx_t * ctx, const wchar_t * ntstr)
{
    for (const wchar_t * ch = ntstr; *ch != 0; ++ch)
    {
        if (!consume_ch_exact(ctx, *ch))
        {
            return false;
        }
    }

    return true;
}
static void consume_str_exact_crit(ctx_t * ctx, const wchar_t * ntstr)
{
    if (!consume_str_exact(ctx, ntstr))
    {
        ctx->err = true;
    }
}

static void consume_wss(ctx_t * ctx)
{
    while (is_ws(ctx->ch))
    {
        next_ch(ctx);
    }
}

static void push_str_ch(ctx_t * ctx, wchar_t ch)
{
    ul_arr_grow(ctx->str_size + 1, &ctx->str_cap, &ctx->str, sizeof(*ctx->str));

    ctx->str[ctx->str_size++] = ch;
}
static void push_str_1anydig(ctx_t * ctx)
{
    if (!is_dig(ctx->ch))
    {
        ctx->err = true;
    }

    while (is_dig(ctx->ch))
    {
        push_str_ch(ctx, ctx->ch);

        next_ch(ctx);
    }
}

static void parse_str(ctx_t * ctx, ul_hs_t ** out)
{
    consume_wss(ctx);

    ctx->str_size = 0;

    consume_ch_exact(ctx, UL_JSON_QUOT_MARK);

    while (true)
    {
        if (ctx->ch == WEOF || consume_ch_exact(ctx, UL_JSON_QUOT_MARK))
        {
            break;
        }

        switch (ctx->ch)
        {
            case L'\\':
            {
                next_ch(ctx);

                bool succ = true;
                wchar_t code = 0;

                switch (ctx->ch)
                {
                    case L'\"':
                        code = L'\"';
                        break;
                    case L'\\':
                        code = L'\\';
                        break;
                    case L'/':
                        code = L'/';
                        break;
                    case L'b':
                        code = L'\b';
                        break;
                    case L'f':
                        code = L'\f';
                        break;
                    case L'n':
                        code = L'\n';
                        break;
                    case L'r':
                        code = L'\r';
                        break;
                    case L't':
                        code = L'\t';
                        break;
                    case L'u':
                        for (size_t i = 0; i < 4; ++i)
                        {
                            next_ch(ctx);

                            uint8_t hexdig;

                            if (!get_hexdig(ctx->ch, &hexdig))
                            {
                                succ = false;
                                break;
                            }

                            code = (code << 4) + hexdig;
                        }
                        break;
                    default:
                        succ = false;
                        break;
                }

                if (succ)
                {
                    push_str_ch(ctx, code);
                }
                else
                {
                    ctx->err = true;
                }

                break;
            }
            default:
                if (ul_is_high_surr(ctx->ch))
                {
                    wchar_t high = ctx->ch;

                    next_ch(ctx);

                    if (ul_is_low_surr(ctx->ch))
                    {
                        push_str_ch(ctx, high);
                        push_str_ch(ctx, ctx->ch);
                    }
                    else
                    {
                        ctx->err = true;
                    }
                }
                else
                {
                    if (ctx->ch < 0x20)
                    {
                        ctx->err = true;
                    }
                    else
                    {
                        push_str_ch(ctx, ctx->ch);
                    }
                }
                break;
        }

        next_ch(ctx);
    }

    *out = ul_hst_hashadd(ctx->src->hst, ctx->str_size, ctx->str);
}

static void parse_val(ctx_t * ctx, ul_json_t ** out);

static void parse_val_false(ctx_t * ctx, ul_json_t ** out)
{
    consume_str_exact_crit(ctx, UL_JSON_FALSE);

    *out = ul_json_make_bool(false);
}
static void parse_val_true(ctx_t * ctx, ul_json_t ** out)
{
    consume_str_exact_crit(ctx, UL_JSON_TRUE);

    *out = ul_json_make_bool(true);
}
static void parse_val_null(ctx_t * ctx, ul_json_t ** out)
{
    consume_str_exact_crit(ctx, UL_JSON_NULL);

    *out = ul_json_make_null();
}
static void parse_val_obj(ctx_t * ctx, ul_json_t ** out)
{
    consume_ch_exact_crit(ctx, UL_JSON_OBJ_BEGIN);

    *out = ul_json_make_obj();

    consume_wss(ctx);

    if (!consume_ch_exact(ctx, UL_JSON_OBJ_END))
    {
        ul_json_t ** ins = &(*out)->val_json;

        while (true)
        {
            ul_hs_t * name = NULL;

            parse_str(ctx, &name);

            consume_wss(ctx);

            consume_ch_exact_crit(ctx, UL_JSON_NAME_SEP);

            consume_wss(ctx);

            parse_val(ctx, ins);

            if (*ins != NULL)
            {
                (*ins)->name = name;

                ins = &(*ins)->next;
            }
            else
            {
                ctx->err = true;
            }

            consume_wss(ctx);

            if (!consume_ch_exact(ctx, UL_JSON_VAL_SEP))
            {
                break;
            }

            consume_wss(ctx);
        }

        consume_ch_exact_crit(ctx, UL_JSON_OBJ_END);
    }
}
static void parse_val_arr(ctx_t * ctx, ul_json_t ** out)
{
    consume_ch_exact_crit(ctx, UL_JSON_ARR_BEGIN);

    *out = ul_json_make_arr();

    consume_wss(ctx);

    if (!consume_ch_exact(ctx, UL_JSON_ARR_END))
    {
        ul_json_t ** ins = &(*out)->val_json;

        while (true)
        {
            parse_val(ctx, ins);

            if (*ins != NULL)
            {
                ins = &(*ins)->next;
            }

            consume_wss(ctx);

            if (!consume_ch_exact(ctx, UL_JSON_VAL_SEP))
            {
                break;
            }

            consume_wss(ctx);
        }

        consume_ch_exact_crit(ctx, UL_JSON_ARR_END);
    }
}
static void parse_val_num(ctx_t * ctx, ul_json_t ** out)
{
    ctx->str_size = 0;

    if (ctx->ch == L'-')
    {
        push_str_ch(ctx, ctx->ch);

        next_ch(ctx);
    }

    push_str_1anydig(ctx);

    if (ctx->ch == L'.')
    {
        push_str_ch(ctx, ctx->ch);

        next_ch(ctx);

        push_str_1anydig(ctx);
    }

    if (ctx->ch == L'e' || ctx->ch == L'E')
    {
        push_str_ch(ctx, ctx->ch);

        next_ch(ctx);

        if (ctx->ch == L'+' || ctx->ch == L'-')
        {
            push_str_ch(ctx, ctx->ch);

            next_ch(ctx);
        }

        push_str_1anydig(ctx);
    }

    wchar_t *val_end, *str_end = ctx->str + ctx->str_size;

    int64_t val_int = (int64_t)wcstoll(ctx->str, &val_end, 10);

    if (val_end == str_end)
    {
        *out = ul_json_make_int(val_int);
    }
    else
    {
        double val_dbl = wcstod(ctx->str, &val_end);

        if (val_end != str_end)
        {
            ctx->err = true;

            *out = ul_json_make_int(0);
        }
        else
        {
            *out = ul_json_make_dbl(val_dbl);
        }
    }
}
static void parse_val_str(ctx_t * ctx, ul_json_t ** out)
{
    ul_hs_t * str = NULL;

    parse_str(ctx, &str);

    *out = ul_json_make_str(str);
}

static void parse_val(ctx_t * ctx, ul_json_t ** out)
{
    switch (ctx->ch)
    {
        case L'f':
            parse_val_false(ctx, out);
            break;

        case L'n':
            parse_val_null(ctx, out);
            break;

        case L't':
            parse_val_true(ctx, out);
            break;

        case UL_JSON_OBJ_BEGIN:
            parse_val_obj(ctx, out);
            break;

        case UL_JSON_ARR_BEGIN:
            parse_val_arr(ctx, out);
            break;

        case L'-':
        case L'0':
        case L'1':
        case L'2':
        case L'3':
        case L'4':
        case L'5':
        case L'6':
        case L'7':
        case L'8':
        case L'9':
            parse_val_num(ctx, out);
            break;

        case UL_JSON_QUOT_MARK:
            parse_val_str(ctx, out);
            break;

        default:
            ctx->err = true;
            break;
    }
}

static bool parse_core(ctx_t * ctx)
{
    if (!next_ch(ctx))
    {
        return false;
    }

    consume_wss(ctx);

    parse_val(ctx, ctx->out);

    return true;
}
bool ul_json_p_parse(ul_json_p_src_t * src, ul_json_t ** out)
{
    ctx_t ctx = { .src = src, .out = out };

    bool res = parse_core(&ctx);

    free(ctx.str);

    return res && !ctx.err;
}

static bool get_file_ch_proc(file_ctx_t * ctx, wchar_t * out)
{
    wint_t ch = fgetwc(ctx->file);

    if (ch == WEOF)
    {
        return false;
    }

    *out = ch;

    return true;
}
bool ul_json_p_parse_file(ul_hst_t * hst, const wchar_t * file_name, ul_json_t ** out)
{
    file_ctx_t ctx = { 0 };

    (void)_wfopen_s(&ctx.file, file_name, L"r, ccs=UTF-8");

    bool res = false;

    if (ctx.file != NULL)
    {
        ul_json_p_src_t src;

        ul_json_p_src_init(&src, hst, &ctx, get_file_ch_proc);

        res = ul_json_p_parse(&src, out);

        ul_json_p_src_cleanup(&src);

        fclose(ctx.file);
    }

    return res;
}

static bool get_str_ch_proc(str_ctx_t * ctx, wchar_t * out)
{
    if (ctx->cur == ctx->cur_end)
    {
        return false;
    }

    *out = *ctx->cur++;

    return true;
}
bool ul_json_p_parse_str(ul_hst_t * hst, size_t str_size, const wchar_t * str, ul_json_t ** out)
{
    str_ctx_t ctx = { .cur = str, .cur_end = str + str_size };

    ul_json_p_src_t src;

    ul_json_p_src_init(&src, hst, &ctx, get_str_ch_proc);

    bool res = ul_json_p_parse(&src, out);

    ul_json_p_src_cleanup(&src);

    return res;
}
