#include "pch.h"
#include "lnk_sect.h"
#include "lnk_pe.h"
#include "asm_it.h"

#define SECT_NAME ".rdata"
#define LABEL_PREFIX L"#it:"
#define NAME_ALIGN 2

typedef IMAGE_IMPORT_DESCRIPTOR impt_desc_t;

typedef struct asm_it_ctx {
	asm_it_t * it;
	ul_hst_t * hst;
	lnk_pe_t * out;

	ul_hsb_t hsb;

	lnk_sect_t * dir_sect;
	lnk_sect_t * addr_sect;
	lnk_sect_t * hnt_sect;

	size_t label_index;
} ctx_t;

void asm_it_init(asm_it_t * it) {
	*it = (asm_it_t){ 0 };

	InitializeCriticalSection(&it->lock);
}
void asm_it_cleanup(asm_it_t * it) {
	DeleteCriticalSection(&it->lock);

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

static asm_it_lib_t * get_it_lib_nl(asm_it_t * it, ul_hs_t * name) {
	asm_it_lib_t ** ins = &it->lib;

	while (*ins != NULL) {
		asm_it_lib_t * lib = *ins;

		if (lib->name == name) {
			return lib;
		}

		ins = &lib->next;
	}

	asm_it_lib_t * new_lib = malloc(sizeof(*new_lib));

	ul_raise_check_mem_alloc(new_lib);

	*new_lib = (asm_it_lib_t){ .name = name };

	*ins = new_lib;

	return new_lib;
}
asm_it_lib_t * asm_it_get_lib(asm_it_t * it, ul_hs_t * name) {
	EnterCriticalSection(&it->lock);

	asm_it_lib_t * res;

	__try {
		res = get_it_lib_nl(it, name);
	}
	__finally {
		LeaveCriticalSection(&it->lock);
	}

	return res;
}
static asm_it_sym_t * add_lib_sym_nl(asm_it_lib_t * lib, ul_hs_t * name, ul_hs_t * link_name) {
	asm_it_sym_t ** ins = &lib->sym;

	while (*ins != NULL) {
		asm_it_sym_t * sym = *ins;

		if (sym->name == name) {
			return sym;
		}

		ins = &sym->next;
	}

	asm_it_sym_t * new_sym = malloc(sizeof(*new_sym));

	ul_raise_check_mem_alloc(new_sym);

	*new_sym = (asm_it_sym_t){ .name = name, .link_name = link_name };

	*ins = new_sym;

	return new_sym;
}
asm_it_sym_t * asm_it_add_sym(asm_it_t * it, ul_hs_t * lib_name, ul_hs_t * sym_name, ul_hs_t * sym_link_name) {
	EnterCriticalSection(&it->lock);

	asm_it_sym_t * res;

	__try {
		asm_it_lib_t * lib = get_it_lib_nl(it, lib_name);

		res = add_lib_sym_nl(lib, sym_name, sym_link_name);
	}
	__finally {
		LeaveCriticalSection(&it->lock);
	}

	return res;
}

static lnk_sect_t * create_sects_sect(ctx_t * ctx) {
	lnk_sect_t * sect = lnk_pe_push_new_sect(ctx->out);

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

static ul_hs_t * get_unq_label_name(ctx_t * ctx) {
	return ul_hsb_formatadd(&ctx->hsb, ctx->hst, L"%s%zi", LABEL_PREFIX, ctx->label_index++);
}
static void write_data(lnk_sect_t * sect, size_t data_size, void * data) {
	if (sect->data_size + data_size > sect->data_cap) {
		ul_arr_grow(&sect->data_cap, &sect->data, sizeof(*sect->data), data_size);
	}

	memcpy(sect->data + sect->data_size, data, data_size);
	sect->data_size += data_size;
}
static bool write_ascii_str(lnk_sect_t * sect, ul_hs_t * str) {
	size_t str_size = ul_align_to(str->size + 1, NAME_ALIGN);

	if (sect->data_size + str_size > sect->data_cap) {
		ul_arr_grow(&sect->data_cap, &sect->data, sizeof(*sect->data), sect->data_cap + str_size - sect->data_cap);
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
	ul_hs_t * lib_name_label = get_unq_label_name(ctx);
	ul_hs_t * lib_at_label = get_unq_label_name(ctx);

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
			ul_hs_t * sym_label = get_unq_label_name(ctx);

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

	ul_hsb_init(&ctx->hsb);

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

bool asm_it_build(asm_it_t * it, ul_hst_t * hst, lnk_pe_t * out) {
	ctx_t ctx = { .it = it, .hst = hst, .out = out };

	bool res;
	
	__try {
		res = build_core(&ctx);
	}
	__finally {
		ul_hsb_cleanup(&ctx.hsb);
	}

	return res;
}
