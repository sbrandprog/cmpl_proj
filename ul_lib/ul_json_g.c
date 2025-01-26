#include "ul_json_g.h"
#include "ul_assert.h"
#include "ul_hs.h"
#include "ul_misc.h"

#define INT_BUF_SIZE 32
#define DBL_BUF_SIZE 64

typedef struct ul_json_g_ctx
{
    ul_json_g_sink_t * sink;
    ul_json_t * json;

    bool err;

    size_t nest_level;
} ctx_t;

typedef struct ul_json_g_file_ctx
{
    FILE * file;
} file_ctx_t;


void ul_json_g_sink_init(ul_json_g_sink_t * sink, void * data, ul_json_g_put_ch_proc_t * put_ch_proc)
{
    *sink = (ul_json_g_sink_t){ .data = data, .put_ch_proc = put_ch_proc, .put_ws = false };
}
void ul_json_g_sink_cleanup(ul_json_g_sink_t * sink)
{
    memset(sink, 0, sizeof(*sink));
}


static bool is_esc_ch(char ch, char * out)
{
    switch (ch)
    {
        case '\"':
            *out = '\"';
            break;
        case '\\':
            *out = '\\';
            break;
        case '/':
            *out = '/';
            break;
        case '\b':
            *out = 'b';
            break;
        case '\f':
            *out = 'f';
            break;
        case '\n':
            *out = 'n';
            break;
        case '\r':
            *out = 't';
            break;
        case '\t':
            *out = 't';
            break;
        default:
            return false;
    }

    return true;
}


static void put_ch(ctx_t * ctx, char ch)
{
    if (!ctx->sink->put_ch_proc(ctx->sink->data, ch))
    {
        ctx->err = true;
    }
}
static void put_str(ctx_t * ctx, const char * ntstr)
{
    for (const char * ch = ntstr; *ch != 0; ++ch)
    {
        put_ch(ctx, *ch);
    }
}

static void put_spc(ctx_t * ctx)
{
    if (ctx->sink->put_ws)
    {
        put_ch(ctx, ' ');
    }
}
static void put_nl(ctx_t * ctx)
{
    if (ctx->sink->put_ws)
    {
        put_ch(ctx, '\n');
    }
}
static void put_tab(ctx_t * ctx)
{
    if (ctx->sink->put_ws)
    {
        for (size_t i = 0; i < ctx->nest_level; ++i)
        {
            put_ch(ctx, '\t');
        }
    }
}

static void generate_str(ctx_t * ctx, const ul_hs_t * str)
{
    put_ch(ctx, UL_JSON_QUOT_MARK);

    if (str != NULL)
    {
        for (char *ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch)
        {
            char code;

            if (is_esc_ch(*ch, &code))
            {
                put_ch(ctx, '\\');
                put_ch(ctx, code);
            }
            else if (*ch < 0x20)
            {
                put_str(ctx, "\\u00");

                for (size_t i = sizeof(code) * CHAR_BIT / 4; i > 0;)
                {
                    --i;

                    put_ch(ctx, (code >> (char)i * 4) & 0xF + '0');
                }
            }
            else
            {
                put_ch(ctx, *ch);
            }
        }
    }
    else
    {
        ctx->err = true;
    }

    put_ch(ctx, UL_JSON_QUOT_MARK);
}

static void generate_val(ctx_t * ctx, ul_json_t * val);

static void generate_val_null(ctx_t * ctx, ul_json_t * val)
{
    put_str(ctx, UL_JSON_NULL);
}
static void generate_val_bool(ctx_t * ctx, ul_json_t * val)
{
    put_str(ctx, val->val_bool ? UL_JSON_TRUE : UL_JSON_FALSE);
}
static void generate_val_int(ctx_t * ctx, ul_json_t * val)
{
    char buf[INT_BUF_SIZE];

    int res = snprintf(buf, _countof(buf), "%" PRIi64, val->val_int);

    ul_assert(res >= 0);

    put_str(ctx, buf);
}
static void generate_val_dbl(ctx_t * ctx, ul_json_t * val)
{
    char buf[DBL_BUF_SIZE];

    int res = snprintf(buf, _countof(buf), "%.17e", val->val_dbl);

    ul_assert(res >= 0);

    put_str(ctx, buf);
}
static void generate_val_str(ctx_t * ctx, ul_json_t * val)
{
    generate_str(ctx, val->val_str);
}
static void generate_val_arr(ctx_t * ctx, ul_json_t * val)
{
    put_ch(ctx, UL_JSON_ARR_BEGIN);

    ul_json_t * json = val->val_json;

    if (json != NULL)
    {
        put_nl(ctx);

        ++ctx->nest_level;

        while (true)
        {
            put_tab(ctx);

            generate_val(ctx, json);

            if (json->next != NULL)
            {
                json = json->next;

                put_ch(ctx, UL_JSON_VAL_SEP);

                put_nl(ctx);
            }
            else
            {
                break;
            }
        }

        put_nl(ctx);

        --ctx->nest_level;

        put_tab(ctx);
    }
    else
    {
        put_spc(ctx);
    }

    put_ch(ctx, UL_JSON_ARR_END);
}
static void generate_val_obj(ctx_t * ctx, ul_json_t * val)
{
    put_ch(ctx, UL_JSON_OBJ_BEGIN);

    ul_json_t * json = val->val_json;

    if (json != NULL)
    {
        put_nl(ctx);

        ++ctx->nest_level;

        while (true)
        {
            put_tab(ctx);

            generate_str(ctx, json->name);

            put_ch(ctx, UL_JSON_NAME_SEP);

            put_spc(ctx);

            generate_val(ctx, json);

            if (json->next != NULL)
            {
                json = json->next;

                put_ch(ctx, UL_JSON_VAL_SEP);

                put_nl(ctx);
            }
            else
            {
                break;
            }
        }

        put_nl(ctx);

        --ctx->nest_level;

        put_tab(ctx);
    }
    else
    {
        put_spc(ctx);
    }

    put_ch(ctx, UL_JSON_OBJ_END);
}
static void generate_val(ctx_t * ctx, ul_json_t * val)
{
    switch (val->type)
    {
        case UlJsonNull:
            generate_val_null(ctx, val);
            break;
        case UlJsonBool:
            generate_val_bool(ctx, val);
            break;
        case UlJsonInt:
            generate_val_int(ctx, val);
            break;
        case UlJsonDbl:
            generate_val_dbl(ctx, val);
            break;
        case UlJsonStr:
            generate_val_str(ctx, val);
            break;
        case UlJsonArr:
            generate_val_arr(ctx, val);
            break;
        case UlJsonObj:
            generate_val_obj(ctx, val);
            break;
        default:
        {
            ctx->err = true;

            ul_json_t null = { .type = UlJsonNull };

            generate_val_null(ctx, &null);

            break;
        }
    }
}

static bool generate_core(ctx_t * ctx)
{
    generate_val(ctx, ctx->json);

    return true;
}
bool ul_json_g_generate(ul_json_g_sink_t * sink, ul_json_t * json)
{
    ctx_t ctx = { .sink = sink, .json = json };

    bool res = generate_core(&ctx);

    return res && !ctx.err;
}

static bool put_file_ch_proc(void * sink_data, char ch)
{
    file_ctx_t * ctx = sink_data;

    return fputc(ch, ctx->file) != EOF;
}
bool ul_json_g_generate_file(const char * file_name, ul_json_t * json)
{
    file_ctx_t ctx = { 0 };

    ctx.file = fopen(file_name, "w");

    bool res = false;

    if (ctx.file != NULL)
    {
        ul_json_g_sink_t sink;

        ul_json_g_sink_init(&sink, &ctx, put_file_ch_proc);
        sink.put_ws = true;

        res = ul_json_g_generate(&sink, json);

        ul_json_g_sink_cleanup(&sink);

        fclose(ctx.file);
    }

    return res;
}
