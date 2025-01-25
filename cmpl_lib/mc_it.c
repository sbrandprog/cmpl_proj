#include "mc_it.h"
#include "lnk_pe.h"
#include "lnk_pel.h"
#include "lnk_sect.h"
#include "mc_defs.h"

#define MOD_NAME L"mc_it"

#define LABEL_SUFFIX L"#it:"
#define LABEL_LIB_NAME_EXT L'n'
#define LABEL_LIB_AT_EXT L'a'
#define LABEL_SYM_NAME_EXT L's'
#define NAME_ALIGN 2

typedef struct mc_it_ctx
{
    mc_it_t * it;
    lnk_pel_t * out;

    bool is_rptd;

    ul_hsb_t hsb;

    lnk_sect_t * dir_sect;
    lnk_sect_t * addr_sect;
    lnk_sect_t * hnt_sect;
} ctx_t;

void mc_it_init(mc_it_t * it, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr)
{
    *it = (mc_it_t){ .hst = hst, .ec_fmtr = ec_fmtr };
}
void mc_it_cleanup(mc_it_t * it)
{
    for (mc_it_lib_t * lib = it->lib; lib != NULL;)
    {
        mc_it_lib_t * next = lib->next;

        for (mc_it_sym_t * sym = lib->sym; sym != NULL;)
        {
            mc_it_sym_t * next = sym->next;

            free(sym);

            sym = next;
        }

        free(lib);

        lib = next;
    }

    memset(it, 0, sizeof(*it));
}

mc_it_lib_t * mc_it_get_lib(mc_it_t * it, ul_hs_t * name)
{
    mc_it_lib_t ** ins = &it->lib;

    while (*ins != NULL)
    {
        mc_it_lib_t * lib = *ins;

        if (lib->name == name)
        {
            return lib;
        }

        ins = &lib->next;
    }

    mc_it_lib_t * new_lib = malloc(sizeof(*new_lib));

    ul_assert(new_lib != NULL);

    *new_lib = (mc_it_lib_t){ .name = name };

    *ins = new_lib;

    return new_lib;
}
mc_it_sym_t * mc_it_add_sym(mc_it_t * it, ul_hs_t * lib_name, ul_hs_t * sym_name, ul_hs_t * sym_link_name)
{
    mc_it_lib_t * lib = mc_it_get_lib(it, lib_name);

    mc_it_sym_t ** ins = &lib->sym;

    while (*ins != NULL)
    {
        mc_it_sym_t * sym = *ins;

        if (sym->name == sym_name)
        {
            return sym;
        }

        ins = &sym->next;
    }

    mc_it_sym_t * new_sym = malloc(sizeof(*new_sym));

    ul_assert(new_sym != NULL);

    *new_sym = (mc_it_sym_t){ .name = sym_name, .link_name = sym_link_name };

    *ins = new_sym;

    return new_sym;
}

static void report(ctx_t * ctx, const wchar_t * fmt, ...)
{
    ctx->is_rptd = true;

    if (ctx->it->ec_fmtr == NULL)
    {
        return;
    }

    {
        va_list args;

        va_start(args, fmt);

        ul_ec_fmtr_format_va(ctx->it->ec_fmtr, fmt, args);

        va_end(args);
    }

    ul_ec_fmtr_post(ctx->it->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
}

static bool is_empty(ctx_t * ctx)
{
    for (mc_it_lib_t * lib = ctx->it->lib; lib != NULL; lib = lib->next)
    {
        if (lib->sym != NULL)
        {
            return false;
        }
    }

    return true;
}

static void create_sects(ctx_t * ctx)
{
    ctx->dir_sect = lnk_sect_create_desc(&mc_defs_sds[McDefsSdRdata_ItDir]);
    ctx->addr_sect = lnk_sect_create_desc(&mc_defs_sds[McDefsSdRdata_ItAddr]);
    ctx->hnt_sect = lnk_sect_create_desc(&mc_defs_sds[McDefsSdRdata_ItHnt]);
}

static ul_hs_t * get_it_label_name(ctx_t * ctx, ul_hs_t * base_name, wchar_t ext_char)
{
    return ul_hsb_formatadd(&ctx->hsb, ctx->it->hst, L"%s%s%c", base_name->str, LABEL_SUFFIX, ext_char);
}
static void write_data(lnk_sect_t * sect, size_t data_size, void * data)
{
    ul_arr_grow(sect->data_size + data_size, &sect->data_cap, &sect->data, sizeof(*sect->data));

    memcpy(sect->data + sect->data_size, data, data_size);
    sect->data_size += data_size;
}
static bool write_ascii_str(lnk_sect_t * sect, ul_hs_t * str)
{
    size_t str_size = ul_align_to(str->size + 1, NAME_ALIGN);

    ul_arr_grow(sect->data_size + str_size, &sect->data_cap, &sect->data, sizeof(*sect->data));

    uint8_t * cur = sect->data + sect->data_size;

    bool err = false;

    for (wchar_t *ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch)
    {
        if ((*ch & ~0x7F) != 0)
        {
            err = true;
            continue;
        }

        *cur++ = (uint8_t)*ch;
    }

    for (size_t i = str->size; i < str_size; ++i)
    {
        *cur++ = 0;
    }

    sect->data_size += str_size;

    return !err;
}

static void process_lib(ctx_t * ctx, mc_it_lib_t * lib)
{
    ul_hs_t * lib_name_label = get_it_label_name(ctx, lib->name, LABEL_LIB_NAME_EXT);
    ul_hs_t * lib_at_label = get_it_label_name(ctx, lib->name, LABEL_LIB_AT_EXT);

    lnk_sect_t * hnt_sect = ctx->hnt_sect;

    {
        lnk_sect_t * dir_sect = ctx->dir_sect;

        lnk_pe_impt_desc_t desc = { 0 };

        lnk_sect_add_lp(dir_sect, LnkSectLpFixup, LnkSectLpFixupRva32, lib_name_label, dir_sect->data_size + offsetof(lnk_pe_impt_desc_t, name));
        lnk_sect_add_lp(dir_sect, LnkSectLpFixup, LnkSectLpFixupRva32, lib_at_label, dir_sect->data_size + offsetof(lnk_pe_impt_desc_t, first_thunk));

        write_data(dir_sect, sizeof(desc), &desc);


        lnk_sect_add_lp(hnt_sect, LnkSectLpLabel, LnkSectLpLabelBasic, lib_name_label, hnt_sect->data_size);

        if (!write_ascii_str(hnt_sect, lib->name))
        {
            report(ctx, L"invalid library name [%s]", lib->name->str);
        }
    }

    {
        lnk_sect_t * addr_sect = ctx->addr_sect;

        lnk_sect_add_lp(addr_sect, LnkSectLpLabel, LnkSectLpLabelBasic, lib_at_label, addr_sect->data_size);

        uint16_t sym_hint = 0;
        uint64_t sym_rec = 0;

        for (mc_it_sym_t * sym = lib->sym; sym != NULL; sym = sym->next)
        {
            ul_hs_t * sym_label = get_it_label_name(ctx, sym->link_name, LABEL_SYM_NAME_EXT);

            lnk_sect_add_lp(addr_sect, LnkSectLpLabel, LnkSectLpLabelBasic, sym->link_name, addr_sect->data_size);
            lnk_sect_add_lp(addr_sect, LnkSectLpFixup, LnkSectLpFixupRva31of64, sym_label, addr_sect->data_size);

            write_data(addr_sect, sizeof(sym_rec), &sym_rec);


            lnk_sect_add_lp(hnt_sect, LnkSectLpLabel, LnkSectLpLabelBasic, sym_label, hnt_sect->data_size);

            write_data(hnt_sect, sizeof(sym_hint), &sym_hint);

            if (!write_ascii_str(hnt_sect, sym->name))
            {
                report(ctx, L"invalid symbol name [%s] in library [%s]", sym->name->str, lib->name->str);
            }
        }

        write_data(addr_sect, sizeof(sym_rec), &sym_rec);
    }
}

static lnk_sect_t ** push_sect(ctx_t * ctx, lnk_sect_t ** ins, lnk_sect_t ** sect_loc)
{
    *ins = *sect_loc;

    *sect_loc = NULL;

    ins = &(*ins)->next;

    return ins;
}
static void push_sects(ctx_t * ctx)
{
    lnk_sect_t ** ins = &ctx->out->sect;

    while (*ins != NULL)
    {
        ins = &(*ins)->next;
    }

    ins = push_sect(ctx, ins, &ctx->dir_sect);
    ins = push_sect(ctx, ins, &ctx->addr_sect);
    ins = push_sect(ctx, ins, &ctx->hnt_sect);
}

static void build_core(ctx_t * ctx)
{
    mc_it_t * it = ctx->it;

    if (is_empty(ctx))
    {
        return;
    }

    ul_hsb_init(&ctx->hsb);

    create_sects(ctx);

    lnk_sect_add_lp(ctx->dir_sect, LnkSectLpMark, LnkSectLpMarkImpStart, NULL, ctx->dir_sect->data_size);

    for (mc_it_lib_t * lib = it->lib; lib != NULL; lib = lib->next)
    {
        process_lib(ctx, lib);
    }

    {
        lnk_pe_impt_desc_t desc = { 0 };

        write_data(ctx->dir_sect, sizeof(desc), &desc);
    }

    lnk_sect_add_lp(ctx->dir_sect, LnkSectLpMark, LnkSectLpMarkImpEnd, NULL, ctx->dir_sect->data_size);

    push_sects(ctx);
}

bool mc_it_build(mc_it_t * it, lnk_pel_t * out)
{
    ctx_t ctx = { .it = it, .out = out };

    build_core(&ctx);

    ul_hsb_cleanup(&ctx.hsb);

    lnk_sect_destroy(ctx.dir_sect);
    lnk_sect_destroy(ctx.addr_sect);
    lnk_sect_destroy(ctx.hnt_sect);

    return !ctx.is_rptd;
}
