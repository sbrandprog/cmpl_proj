#include "pch.h"
#include "asm_it.h"
#include "u_misc.h"

#define SECT_NAME ".rdata"
#define LABEL_PREFIX L"#it:"
#define NAME_ALIGN 2

typedef IMAGE_IMPORT_DESCRIPTOR impt_desc_t;

typedef struct asm_it_ctx {
	asm_it_t * it;

	u_hst_t * hst;

	lnk_sect_t ** out;

	lnk_sect_t * dir_sect;
	lnk_sect_t * addr_sect;
	lnk_sect_t * hnt_sect;

	size_t label_index;
	wchar_t * label_buf;
} ctx_t;

void asm_it_cleanup(asm_it_t * it) {
	for (asm_it_lib_t * lib = it->lib; lib != NULL; ) {
		asm_it_lib_t * next = lib->next;

		for (asm_it_sym_t * sym = lib->sym; sym != NULL; ) {
			asm_it_sym_t * next = sym->next;

			free(sym);

			sym = next;
		}

		free(lib);

		lib = next;
	}

	memset(it, 0, sizeof(*it));
}

asm_it_lib_t * asm_it_get_lib(asm_it_t * it, u_hs_t * name) {
	asm_it_lib_t ** ins = &it->lib;

	while (*ins != NULL) {
		asm_it_lib_t * lib = *ins;

		if (lib->name == name) {
			return lib;
		}

		ins = &lib->next;
	}

	asm_it_lib_t * new_lib = malloc(sizeof(*new_lib));

	u_assert(new_lib != NULL);

	*new_lib = (asm_it_lib_t){ .name = name };

	*ins = new_lib;

	return new_lib;
}
asm_it_sym_t * asm_it_add_sym(asm_it_t * it, u_hs_t * lib_name, u_hs_t * sym_name, u_hs_t * sym_link_name) {
	asm_it_lib_t * lib = asm_it_get_lib(it, lib_name);

	return asm_it_lib_add_sym(lib, sym_name, sym_link_name);
}

asm_it_sym_t * asm_it_lib_add_sym(asm_it_lib_t * lib, u_hs_t * name, u_hs_t * link_name) {
	asm_it_sym_t ** ins = &lib->sym;

	while (*ins != NULL) {
		asm_it_sym_t * sym = *ins;

		if (sym->name == name) {
			return sym;
		}

		ins = &sym->next;
	}

	asm_it_sym_t * new_sym = malloc(sizeof(*new_sym));

	u_assert(new_sym != NULL);

	*new_sym = (asm_it_sym_t){ .name = name, .link_name = link_name };

	*ins = new_sym;

	return new_sym;
}

static lnk_sect_t * create_sects_sect(ctx_t * ctx) {
	lnk_sect_t * sect = lnk_sect_create(ctx->out);

	sect->name = SECT_NAME;

	sect->data_align = 8;
	sect->data_align_byte = 0x00;

	sect->mem_r = true;

	return sect;
}
static void create_sects(ctx_t * ctx) {
	ctx->dir_sect = create_sects_sect(ctx);
	ctx->addr_sect = create_sects_sect(ctx);
	ctx->hnt_sect = create_sects_sect(ctx);
}

static u_hs_t * get_unq_label_name(ctx_t * ctx) {
	size_t str_size = _countof(LABEL_PREFIX) - 1 + U_SIZE_T_NUM_SIZE;
	size_t str_size_full = str_size + 1;

	if (ctx->label_buf == NULL) {
		ctx->label_buf = malloc(sizeof(*ctx->label_buf) * str_size_full);

		u_assert(ctx->label_buf != NULL);
	}

	int result = swprintf_s(ctx->label_buf, str_size_full, L"%s%zi", LABEL_PREFIX, ctx->label_index++);

	u_assert(result >= 0 && result < (int)str_size);

	return u_hst_hashadd(ctx->hst, (size_t)result, ctx->label_buf);
}
static void write_data(lnk_sect_t * sect, size_t data_size, void * data) {
	if (sect->data_size + data_size > sect->data_cap) {
		u_grow_arr(&sect->data_cap, &sect->data, sizeof(*sect->data), data_size);
	}

	memcpy(sect->data + sect->data_size, data, data_size);
	sect->data_size += data_size;
}
static bool write_ascii_str(lnk_sect_t * sect, u_hs_t * str) {
	size_t str_size = u_align_to(str->size + 1, NAME_ALIGN);

	if (sect->data_size + str_size > sect->data_cap) {
		u_grow_arr(&sect->data_cap, &sect->data, sizeof(*sect->data), sect->data_cap + str_size - sect->data_cap);
	}

	uint8_t * cur = sect->data + sect->data_size;

	for (wchar_t * ch = str->str, *ch_end = ch + str->size; ch != ch_end; ++ch) {
		if ((*ch & ~0x7F) != 0) {
			return false;
		}

		*cur++ = (uint8_t)*ch;
	}

	for (size_t i = str->size; i < str_size; ++i) {
		*cur++ = 0;
	}

	sect->data_size += str_size;

	return true;
}

static bool process_lib(ctx_t * ctx, asm_it_lib_t * lib) {
	u_hs_t * lib_name_label = get_unq_label_name(ctx);
	u_hs_t * lib_at_label = get_unq_label_name(ctx);

	lnk_sect_t * hnt_sect = ctx->hnt_sect;

	{
		lnk_sect_t * dir_sect = ctx->dir_sect;

		impt_desc_t desc = { 0 };

		lnk_sect_add_lp(dir_sect, LnkSectLpFixup, LnkSectFixupRva32, lib_name_label, dir_sect->data_size + offsetof(impt_desc_t, Name));
		lnk_sect_add_lp(dir_sect, LnkSectLpFixup, LnkSectFixupRva32, lib_at_label, dir_sect->data_size + offsetof(impt_desc_t, FirstThunk));

		write_data(dir_sect, sizeof(desc), &desc);


		lnk_sect_add_lp(hnt_sect, LnkSectLpLabel, LnkSectLabelNone, lib_name_label, hnt_sect->data_size);

		if (!write_ascii_str(hnt_sect, lib->name)) {
			return false;
		}
	}

	{
		lnk_sect_t * addr_sect = ctx->addr_sect;

		lnk_sect_add_lp(addr_sect, LnkSectLpLabel, LnkSectLabelNone, lib_at_label, addr_sect->data_size);

		uint16_t sym_hint = 0;
		uint64_t sym_rec = 0;

		for (asm_it_sym_t * sym = lib->sym; sym != NULL; sym = sym->next) {
			u_hs_t * sym_label = get_unq_label_name(ctx);

			lnk_sect_add_lp(addr_sect, LnkSectLpLabel, LnkSectLabelNone, sym->link_name, addr_sect->data_size);
			lnk_sect_add_lp(addr_sect, LnkSectLpFixup, LnkSectFixupRva31of64, sym_label, addr_sect->data_size);

			write_data(addr_sect, sizeof(sym_rec), &sym_rec);


			lnk_sect_add_lp(hnt_sect, LnkSectLpLabel, LnkSectLabelNone, sym_label, hnt_sect->data_size);

			write_data(hnt_sect, sizeof(sym_hint), &sym_hint);

			write_ascii_str(hnt_sect, sym->name);
		}

		write_data(addr_sect, sizeof(sym_rec), &sym_rec);
	}

	return true;
}

static bool build_core(ctx_t * ctx) {
	asm_it_t * it = ctx->it;

	if (it->lib == NULL) {
		return true;
	}

	create_sects(ctx);

	lnk_sect_add_lp(ctx->dir_sect, LnkSectLpMark, LnkSectMarkImpStart, NULL, ctx->dir_sect->data_size);

	for (asm_it_lib_t * lib = it->lib; lib != NULL; lib = lib->next) {
		if (!process_lib(ctx, lib)) {
			return false;
		}
	}

	{
		impt_desc_t desc = { 0 };

		write_data(ctx->dir_sect, sizeof(desc), &desc);
	}

	lnk_sect_add_lp(ctx->dir_sect, LnkSectLpMark, LnkSectMarkImpEnd, NULL, ctx->dir_sect->data_size);

	return true;
}

bool asm_it_build(asm_it_t * it, u_hst_t * hst, lnk_sect_t ** out) {
	ctx_t ctx = { .it = it, .hst = hst, .out = out };

	bool result = build_core(&ctx);

	free(ctx.label_buf);

	return result;
}
