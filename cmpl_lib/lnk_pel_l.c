#include "lnk_pe.h"
#include "lnk_sect.h"
#include "lnk_pel_m.h"
#include "lnk_pel_l.h"

#define MOD_NAME L"lnk_pel_l"

#define PAGE_SIZE 4096

#define SECT_NAME_SIZE LNK_PE_SECT_NAME_SIZE
#define SECT_DEFAULT_ALIGN 16

typedef struct lnk_pel_l_sect sect_t;
struct lnk_pel_l_sect {
	sect_t * next;
	const char * name;

	size_t data_size;
	uint8_t * data;
	size_t data_cap;

	bool mem_r, mem_w, mem_e, mem_disc;

	size_t ind;

	size_t raw_size, raw_addr;
	size_t virt_size, virt_addr;
};

typedef struct lnk_pel_l_mark {
	sect_t * sect;
	size_t off;
} mark_t;
typedef struct lnk_pel_l_label {
	const ul_hs_t * name;
	sect_t * sect;
	size_t off;
} label_t;
typedef struct lnk_pel_l_fixup {
	lnk_sect_lp_stype_t stype;
	const ul_hs_t * label_name;
	sect_t * sect;
	size_t off;
} fixup_t;
typedef struct lnk_pel_l_proc {
	const ul_hs_t * label_name;

	sect_t * end_sect;
	size_t end_off;

	sect_t * unw_sect;
	size_t unw_off;

	label_t * label;
} proc_t;

typedef struct lnk_pel_l_br_fixup {
	size_t off;

	sect_t * target_sect;
	size_t target_page;
} br_fixup_t;

typedef struct lnk_pel_l_ctx {
	lnk_pel_t * pel;

	bool is_rptd;

	lnk_sect_t * mrgd_sect;
	lnk_sect_t * input_sect;


	sect_t * sect;
	size_t sect_count;


	mark_t marks[LnkSectLpMark_Count];

	size_t labels_size;
	label_t * labels;
	size_t labels_cap;

	size_t fixups_size;
	fixup_t * fixups;
	size_t fixups_cap;

	size_t procs_size;
	proc_t * procs;
	size_t procs_cap;


	sect_t * excpt_sect;


	size_t va64_fixups_size;
	fixup_t ** va64_fixups;

	sect_t * reloc_sect;

	size_t br_fixups_size;
	br_fixup_t * br_fixups;
	size_t br_fixups_cap;


	size_t ep_virt_addr;

	size_t raw_hdrs_size;
	size_t hdrs_size;
	size_t first_sect_va;
	size_t image_size;

	ul_json_t * pd_json;
	ul_json_t ** pd_json_ins;
} ctx_t;

static const lnk_sect_lp_stype_t dir_start_mark[LnkPeDataDir_Count] = {
	[LnkPeDataDirImport] = LnkSectLpMarkImpStart,
	[LnkPeDataDirException] = LnkSectLpMarkExcptStart,
	[LnkPeDataDirBaseReloc] = LnkSectLpMarkRelocStart,
	[LnkPeDataDirIat] = LnkSectLpMarkImpTabStart
};
static const lnk_sect_lp_stype_t dir_end_mark[LnkPeDataDir_Count] = {
	[LnkPeDataDirImport] = LnkSectLpMarkImpEnd,
	[LnkPeDataDirException] = LnkSectLpMarkExcptEnd,
	[LnkPeDataDirBaseReloc] = LnkSectLpMarkRelocEnd,
	[LnkPeDataDirIat] = LnkSectLpMarkImpTabEnd
};


static sect_t * create_sect(lnk_sect_t * src) {
	sect_t * sect = malloc(sizeof(*sect));

	ul_assert(sect != NULL);

	memset(sect, 0, sizeof(*sect));

	if (src != NULL) {
		sect->name = src->name;

		sect->data = malloc(sizeof(*sect->data) * src->data_size);

		ul_assert(sect->data != NULL);

		memcpy_s(sect->data, sizeof(*sect->data) * src->data_size, src->data, sizeof(*src->data) * src->data_size);

		sect->data_size = src->data_size;
		sect->data_cap = src->data_size;

		sect->mem_r = src->mem_r;
		sect->mem_w = src->mem_w;
		sect->mem_e = src->mem_e;
		sect->mem_disc = src->mem_disc;
	}

	return sect;
}
static void destroy_sect(sect_t * sect) {
	if (sect == NULL) {
		return;
	}

	free(sect->data);
	free(sect);
}
static void destroy_sect_chain(sect_t * sect) {
	while (sect != NULL) {
		sect_t * next = sect->next;

		destroy_sect(sect);

		sect = next;
	}
}


static void report(ctx_t * ctx, const wchar_t * fmt, ...) {
	ctx->is_rptd = true;

	if (ctx->pel->ec_fmtr == NULL) {
		return;
	}

	{
		va_list args;

		va_start(args, fmt);

		ul_ec_fmtr_format_va(ctx->pel->ec_fmtr, fmt, args);

		va_end(args);
	}

	ul_ec_fmtr_post(ctx->pel->ec_fmtr, UL_EC_REC_TYPE_DFLT, MOD_NAME);
}

static bool prepare_input_sects(ctx_t * ctx) {
	if (ctx->pel->sett.apply_mrgr) {
		if (!lnk_pel_m_merge(ctx->pel, &ctx->mrgd_sect)) {
			report(ctx, L"failed to merge sections");
			return false;
		}

		ctx->input_sect = ctx->mrgd_sect;
	}
	else {
		ctx->input_sect = ctx->pel->sect;
	}

	return true;
}

static bool label_cmp_proc(const label_t * first, const label_t * second) {
	return (uint8_t *)first->name < (uint8_t *)second->name;
}
static label_t * find_label(ctx_t * ctx, const ul_hs_t * name) {
	label_t pred = { .name = name };

	size_t label_ind = ul_bs_lower_bound(sizeof(*ctx->labels), ctx->labels_size, ctx->labels, label_cmp_proc, &pred);

	if (label_ind < ctx->labels_size && ctx->labels[label_ind].name == name) {
		return &ctx->labels[label_ind];
	}

	return NULL;
}

static bool proc_cmp_proc(const proc_t * first, const proc_t * second) {
	return (uint8_t *)first->label_name < (uint8_t *)second->label_name;
}

static sect_t * add_sect(ctx_t * ctx, const char * srch_name, lnk_sect_t * src) {
	sect_t ** ins = &ctx->sect;

	while (*ins != NULL) {
		sect_t * sect = *ins;

		if (strcmp(sect->name, srch_name) == 0) {
			return NULL;
		}

		ins = &sect->next;
	}

	*ins = create_sect(src);

	return *ins;
}
static void add_lp(ctx_t * ctx, sect_t * sect, const lnk_sect_lp_t * lp) {
	if (lp->off > sect->data_size) {
		report(ctx, L"invalid link point offset");
		return;
	}

	switch (lp->type) {
		case LnkSectLpNone:
			break;
		case LnkSectLpMark:
			if (lp->stype == LnkSectLpMarkNone) {
				(void)0;
			}
			else if (lp->stype >= LnkSectLpMark_Count) {
				report(ctx, L"invalid mark subtype");
			}
			else {
				mark_t * mark = &ctx->marks[lp->stype];

				if (mark->sect != NULL) {
					report(ctx, L"invalid mark: [%s] already set", lnk_sect_lp_mark_strs[lp->stype].str);
				}
				else {
					mark->sect = sect;
					mark->off = lp->off;
				}
			}
			break;
		case LnkSectLpLabel:
			if (lp->stype == LnkSectLpLabelNone) {
				(void)0;
			}
			else if (lp->stype >= LnkSectLpLabel_Count) {
				report(ctx, L"invalid label subtype");
			}
			else {
				switch (lp->stype) {
					case LnkSectLpLabelBasic:
					{
						label_t new_label = { .name = lp->label_name, .sect = sect, .off = lp->off };

						size_t ins_pos = ul_bs_lower_bound(sizeof(*ctx->labels), ctx->labels_size, ctx->labels, label_cmp_proc, &new_label);

						if (ins_pos < ctx->labels_size && ctx->labels[ins_pos].name == new_label.name) {
							report(ctx, L"invalid label: non-unique name [%s]", new_label.name->str);
						}
						else {
							ul_arr_grow(ctx->labels_size + 1, &ctx->labels_cap, &ctx->labels, sizeof(*ctx->labels));

							memmove_s(ctx->labels + ins_pos + 1, sizeof(*ctx->labels) * (ctx->labels_cap - ins_pos), ctx->labels + ins_pos, sizeof(*ctx->labels) * (ctx->labels_size - ins_pos));

							ctx->labels[ins_pos] = new_label;
							++ctx->labels_size;
						}

						break;
					}
					case LnkSectLpLabelProcEnd:
					case LnkSectLpLabelProcUnw:
					{
						proc_t new_proc = { .label_name = lp->label_name };

						size_t ins_pos = ul_bs_lower_bound(sizeof(*ctx->procs), ctx->procs_size, ctx->procs, proc_cmp_proc, &new_proc);

						if (ins_pos < ctx->procs_size && ctx->procs[ins_pos].label_name == new_proc.label_name) {
							proc_t * proc = &ctx->procs[ins_pos];

							switch (lp->stype) {
								case LnkSectLpLabelProcEnd:
									if (proc->end_sect != NULL) {
										report(ctx, L"invalid label, procedure end: end point already set");
									}
									else {
										proc->end_sect = sect;
										proc->end_off = lp->off;
									}

									break;
								case LnkSectLpLabelProcUnw:
									if (proc->unw_sect != NULL) {
										report(ctx, L"invalid label, unwind: unwind data already set");
									}
									else {
										proc->unw_sect = sect;
										proc->unw_off = lp->off;
									}

									break;
								default:
									ul_assert_unreachable();
							}
						}
						else {
							switch (lp->stype) {
								case LnkSectLpLabelProcEnd:
									new_proc.end_sect = sect;
									new_proc.end_off = lp->off;
									break;
								case LnkSectLpLabelProcUnw:
									new_proc.unw_sect = sect;
									new_proc.unw_off = lp->off;
									break;
								default:
									ul_assert_unreachable();
							}

							ul_arr_grow(ctx->procs_size + 1, &ctx->procs_cap, &ctx->procs, sizeof(*ctx->procs));

							memmove_s(ctx->procs + ins_pos + 1, sizeof(*ctx->procs) * (ctx->procs_cap - ins_pos), ctx->procs + ins_pos, sizeof(*ctx->procs) * (ctx->procs_size - ins_pos));

							ctx->procs[ins_pos] = new_proc;
							++ctx->procs_size;
						}

						break;
					}
					default:
						ul_assert_unreachable();
				}
			}
			break;
		case LnkSectLpFixup:
			if (lp->stype == LnkSectLpFixupNone) {
				(void)0;
			}
			else if (lp->stype >= LnkSectLpFixup_Count) {
				report(ctx, L"invalid fixup subtype");
			}
			else {
				if (lp->off + lnk_sect_fixups_size[lp->stype] > sect->data_size) {
					report(ctx, L"invalid fixup offset: (offset + fixup data) exceeds section boundary");
				}
				else {
					ul_arr_grow(ctx->fixups_size + 1, &ctx->fixups_cap, &ctx->fixups, sizeof(*ctx->fixups));

					ctx->fixups[ctx->fixups_size++] = (fixup_t){ .stype = lp->stype, .label_name = lp->label_name, .sect = sect, .off = lp->off };

					if (lp->stype == LnkSectLpFixupVa64) {
						++ctx->va64_fixups_size;
					}
				}
			}
			break;
		default:
			report(ctx, L"invalid link point type");
			break;
	}
}
static void form_main_sects(ctx_t * ctx) {
	for (lnk_sect_t * lnk_sect = ctx->input_sect; lnk_sect != NULL; lnk_sect = lnk_sect->next) {
		sect_t * sect = add_sect(ctx, lnk_sect->name, lnk_sect);

		if (sect == NULL) {
			report(ctx, L"invalid section: non-unique section name");
			continue;
		}

		for (lnk_sect_lp_t * lp = lnk_sect->lps, *lp_end = lp + lnk_sect->lps_size; lp != lp_end; ++lp) {
			add_lp(ctx, sect, lp);
		}
	}
}
static bool process_excpt_data(ctx_t * ctx) {
	bool err_flag = false;

	for (proc_t * proc = ctx->procs, *proc_end = proc + ctx->procs_size; proc != proc_end; ++proc) {
		proc->label = find_label(ctx, proc->label_name);

		if (proc->label == NULL || proc->end_sect == NULL || proc->unw_sect == NULL) {
			report(ctx, L"invalid procedure data: data for procedure [%s] is incomplete", proc->label_name->str);
			err_flag = true;
		}
	}

	return !err_flag;
}
static void form_excpt_data_sect(ctx_t * ctx) {
	if (ctx->marks[LnkSectLpMarkExcptStart].sect != NULL || ctx->marks[LnkSectLpMarkExcptEnd].sect != NULL) {
		report(ctx, L"invalid input: exception data marks already set");
		return;
	}

	if (process_excpt_data(ctx)) {
		sect_t * excpt = add_sect(ctx, ctx->pel->sett.excpt_sect_name, NULL);

		if (excpt == NULL) {
			report(ctx, L"failed to add exception data section");
		}
		else {
			size_t excpt_size = sizeof(lnk_pe_rt_func_t) * ctx->procs_size;

			excpt->data = malloc(excpt_size);

			ul_assert(excpt->data != NULL);

			excpt->data_size = excpt_size;
			excpt->data_cap = excpt_size;

			excpt->name = ctx->pel->sett.excpt_sect_name;
			excpt->mem_r = true;

			ctx->excpt_sect = excpt;

			ctx->marks[LnkSectLpMarkExcptStart] = (mark_t){ .sect = excpt, .off = 0 };
			ctx->marks[LnkSectLpMarkExcptEnd] = (mark_t){ .sect = excpt, .off = excpt->data_size };
		}
	}
}
static void set_sect_inds(ctx_t * ctx) {
	sect_t * sect = ctx->sect;
	size_t ind = 0;

	for (; sect != NULL; sect = sect->next) {
		sect->ind = ind++;
	}
}
static int base_reloc_cmp_proc(void * ctx_ptr, const fixup_t * const * first_ptr, const fixup_t * const * second_ptr) {
	const fixup_t * first = *first_ptr, * second = *second_ptr;

	if (first->sect->ind < second->sect->ind) {
		return -1;
	}
	else if (first->sect->ind > second->sect->ind) {
		return 1;
	}

	if (first->off < second->off) {
		return -1;
	}
	else if (first->off > second->off) {
		return 1;
	}

	return 0;
}
static void form_base_reloc_arr(ctx_t * ctx) {
	ctx->va64_fixups = malloc(ctx->va64_fixups_size * sizeof(*ctx->va64_fixups));

	ul_assert(ctx->va64_fixups != NULL);

	fixup_t ** cur = ctx->va64_fixups, ** cur_end = cur + ctx->va64_fixups_size;
	fixup_t * fixup = ctx->fixups, * fixup_end = fixup + ctx->fixups_size;

	for (; fixup != fixup_end; ++fixup) {
		if (fixup->stype == LnkSectLpFixupVa64) {
			ul_assert(cur != cur_end);

			*cur++ = fixup;
		}
	}

	set_sect_inds(ctx);

	qsort_s(ctx->va64_fixups, ctx->va64_fixups_size, sizeof(*ctx->va64_fixups), base_reloc_cmp_proc, ctx);
}
static void form_base_reloc_sect(ctx_t * ctx) {
	if (ctx->marks[LnkSectLpMarkRelocStart].sect != NULL || ctx->marks[LnkSectLpMarkRelocEnd].sect != NULL) {
		report(ctx, L"invalid input: base relocations marks already set");
		return;
	}

	form_base_reloc_arr(ctx);

	sect_t * reloc = add_sect(ctx, ctx->pel->sett.base_reloc_sect_name, NULL);

	if (reloc == NULL) {
		report(ctx, L"failed to add base relocations section");
	}
	else {
		reloc->name = ctx->pel->sett.base_reloc_sect_name;
		reloc->mem_r = true;
		reloc->mem_disc = true;

		for (fixup_t ** va64_f = ctx->va64_fixups, **va64_f_end = va64_f + ctx->va64_fixups_size; va64_f != va64_f_end;) {
			fixup_t * first = *va64_f;

			size_t cur_page = first->off & ~(PAGE_SIZE - 1);

			fixup_t ** cur = va64_f;

			while (cur != va64_f_end && (*cur)->sect == first->sect && ((*cur)->off & ~(PAGE_SIZE - 1)) == cur_page) {
				++cur;
			}

			fixup_t ** last_ptr = cur;

			size_t block_start = ul_align_to(reloc->data_size, sizeof(uint32_t));

			size_t block_size = sizeof(lnk_pe_base_reloc_t) + (last_ptr - va64_f) * sizeof(lnk_pe_base_reloc_entry_t);

			ul_arr_grow(block_start + block_size, &reloc->data_cap, &reloc->data, sizeof(*reloc->data));

			memset(reloc->data + reloc->data_size, 0, block_start - reloc->data_size);

			uint8_t * data_cur = reloc->data + block_start;

			{
				lnk_pe_base_reloc_t block_hdr = { .virtual_address = 0, .size_of_block = (uint32_t)block_size };

				*(lnk_pe_base_reloc_t *)data_cur = block_hdr;

				data_cur += sizeof(block_hdr);

				ul_arr_grow(ctx->br_fixups_size + 1, &ctx->br_fixups_cap, &ctx->br_fixups, sizeof(*ctx->br_fixups));

				ctx->br_fixups[ctx->br_fixups_size++] = (br_fixup_t){
					.off = block_start + offsetof(lnk_pe_base_reloc_t, virtual_address),
					.target_sect = first->sect,
					.target_page = cur_page
				};
			}

			for (fixup_t ** cur = va64_f; cur != last_ptr; ++cur) {
				fixup_t * fixup = *cur;

				lnk_pe_base_reloc_entry_t br = (lnk_pe_base_reloc_entry_t){ .offset = (uint16_t)(fixup->off - cur_page), .type = LnkPeBaseRelocDir64 };

				*(lnk_pe_base_reloc_entry_t *)data_cur = br;

				data_cur += sizeof(br);
			}

			reloc->data_size = block_start + block_size;
			va64_f = last_ptr;
		}

		ctx->reloc_sect = reloc;

		ctx->marks[LnkSectLpMarkRelocStart] = (mark_t){ .sect = reloc, .off = 0 };
		ctx->marks[LnkSectLpMarkRelocEnd] = (mark_t){ .sect = reloc, .off = reloc->data_size };
	}
}
static void form_sects(ctx_t * ctx) {
	form_main_sects(ctx);

	if (ctx->pel->sett.make_excpt_sect && ctx->procs_size > 0) {
		form_excpt_data_sect(ctx);
	}

	if (ctx->pel->sett.make_base_reloc_sect && ctx->va64_fixups_size > 0) {
		form_base_reloc_sect(ctx);
	}

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		++ctx->sect_count;
	}
}

static bool calculate_offsets(ctx_t * ctx) {
	lnk_pel_t * pel = ctx->pel;
	lnk_pel_sett_t * sett = &pel->sett;

	ctx->raw_hdrs_size = sizeof(lnk_pe_dos_hdr_t) + sizeof(lnk_pe_nt_hdrs_t) + sizeof(lnk_pe_sect_hdr_t) * ctx->sect_count;
	ctx->hdrs_size = ul_align_to(ctx->raw_hdrs_size, sett->file_align);
	ctx->first_sect_va = ul_align_to(ctx->hdrs_size, sett->sect_align);

	size_t next_raw_addr = ctx->hdrs_size, next_virt_addr = ctx->first_sect_va;

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		sect->raw_size = ul_align_to(sect->data_size, sett->file_align);
		sect->raw_addr = next_raw_addr;
		sect->virt_size = ul_align_to(sect->raw_size, sett->sect_align);
		sect->virt_addr = next_virt_addr;

		next_raw_addr += sect->raw_size;
		next_virt_addr += sect->virt_size;

		if (next_virt_addr >= LNK_PEL_MODULE_MAX_SIZE) {
			report(ctx, L"invalid sections: size of final executable exceeds 2GiB");
			return false;
		}
	}

	ctx->image_size = next_virt_addr;

	{
		label_t * ep_label = find_label(ctx, pel->ep_name);

		if (ep_label == NULL) {
			report(ctx, L"invalid entry point: label [%s] not found", ctx->pel->ep_name->str);
			return false;
		}

		ctx->ep_virt_addr = ep_label->sect->virt_addr + ep_label->off;
	}

	return true;
}

static int excpt_cmp_proc(void * ctx_ptr, const proc_t * first, const proc_t * second) {
	size_t first_off = first->label->sect->virt_addr + first->label->off,
		second_off = second->label->sect->virt_addr + second->label->off;

	if (first_off < second_off) {
		return -1;
	}
	else if (first_off > second_off) {
		return 1;
	}

	return 0;
}

static void apply_fixups_excpt(ctx_t * ctx) {
	if (ctx->excpt_sect == NULL) {
		return;
	}

	qsort_s(ctx->procs, ctx->procs_size, sizeof(*ctx->procs), excpt_cmp_proc, NULL);

	sect_t * excpt = ctx->excpt_sect;

	lnk_pe_rt_func_t * cur = (lnk_pe_rt_func_t *)excpt->data;

	for (proc_t * proc = ctx->procs, *proc_end = proc + ctx->procs_size; proc != proc_end; ++proc, ++cur) {
		*cur = (lnk_pe_rt_func_t){ .begin_address = (uint32_t)(proc->label->sect->virt_addr + proc->label->off), .end_address = (uint32_t)(proc->end_sect->virt_addr + proc->end_off), .unwind_data = (uint32_t)(proc->unw_sect->virt_addr + proc->unw_off) };
	}

	ul_assert((uint8_t *)cur == excpt->data + excpt->data_size);
}
static void apply_fixups_base_reloc(ctx_t * ctx) {
	if (ctx->reloc_sect == NULL) {
		return;
	}

	for (br_fixup_t * fixup = ctx->br_fixups, *fixup_end = fixup + ctx->br_fixups_size; fixup != fixup_end; ++fixup) {
		*(uint32_t *)(ctx->reloc_sect->data + fixup->off) = (uint32_t)(fixup->target_sect->virt_addr + fixup->target_page);
	}
}
static void apply_fixups(ctx_t * ctx) {
	for (fixup_t * fixup = ctx->fixups, *fixup_end = fixup + ctx->fixups_size; fixup != fixup_end; ++fixup) {
		label_t * label = find_label(ctx, fixup->label_name);

		if (label == NULL) {
			report(ctx, L"invalid fixup: label [%s] not found", fixup->label_name->str);
			continue;
		}

		uint64_t lpos = label->sect->virt_addr + label->off,
			fpos = fixup->sect->virt_addr + fixup->off;

		void * write_pos = fixup->sect->data + fixup->off;

		bool prec_limit_exc = false;

		switch (fixup->stype) {
			case LnkSectLpFixupNone:
				break;
			case LnkSectLpFixupRel8:
			{
				int64_t val = (int64_t)(lpos - fpos - sizeof(uint8_t)) + *(int8_t *)write_pos;

				if (val > INT8_MAX || INT8_MIN > val) {
					prec_limit_exc = true;
				}
				else {
					*(int8_t *)write_pos = (int8_t)val;
				}

				break;
			}
			case LnkSectLpFixupRel32:
			{
				int64_t val = (int64_t)(lpos - fpos - sizeof(uint32_t)) + *(int32_t *)write_pos;

				if (val > INT32_MAX || INT32_MIN > val) {
					prec_limit_exc = true;
				}
				else {
					*(int32_t *)write_pos = (int32_t)val;
				}

				break;
			}
			case LnkSectLpFixupVa64:
				*(uint64_t *)write_pos += (uint64_t)(lpos + ctx->pel->sett.image_base);
				break;
			case LnkSectLpFixupRva32:
			{
				uint64_t val = lpos + *(uint32_t *)write_pos;

				if (val > UINT32_MAX) {
					prec_limit_exc = true;
				}
				else {
					*(uint32_t *)write_pos = (uint32_t)val;
				}

				break;
			}
			case LnkSectLpFixupRva31of64:
				if ((lpos & ~0x7FFFFFFFull) != 0) {
					prec_limit_exc = true;
				}
				else {
					*(uint64_t *)write_pos = (uint64_t)((lpos & 0x7FFFFFFFull) | (*(uint64_t *)write_pos & 0xFFFFFFFF80000000ull));
				}

				break;
			default:
				ul_assert_unreachable();
		}

		if (prec_limit_exc) {
			report(ctx, L"invalid fixup: fixup data exceeded precision limit, name: [%s]", label->name->str);
		}
	}

	apply_fixups_excpt(ctx);

	apply_fixups_base_reloc(ctx);
}

static void set_dir_info(ctx_t * ctx, lnk_pe_nt_hdrs_t * nt, size_t dir_ent) {
	lnk_sect_lp_stype_t start = dir_start_mark[dir_ent], end = dir_end_mark[dir_ent];

	ul_assert(start != LnkSectLpNone && end != LnkSectLpNone);

	mark_t * start_mark = &ctx->marks[start], * end_mark = &ctx->marks[end];

	if (start_mark->sect == NULL && end_mark->sect == NULL) {
		return;
	}
	else if (start_mark->sect != end_mark->sect) {
		report(ctx, L"invalid marks: start & end marks set in different sections (or one of them is not set)");
	}
	else {
		lnk_pe_data_dir_t * dir = &nt->optional_header.data_directory[dir_ent];

		dir->virtual_address = (uint32_t)(start_mark->sect->virt_addr + start_mark->off);
		dir->size = (uint32_t)(end_mark->off - start_mark->off);
	}
}
static void write_zeros(FILE * file, size_t size) {
	static const uint8_t zero_obj[1024] = { 0 };

	size_t div = size / _countof(zero_obj), mod = size % _countof(zero_obj);

	for (size_t i = 0; i < div; ++i) {
		fwrite(zero_obj, sizeof(*zero_obj), _countof(zero_obj), file);
	}

	fwrite(zero_obj, sizeof(*zero_obj), mod, file);
}
static void write_file_core(ctx_t * ctx, FILE * file) {
	lnk_pel_sett_t * sett = &ctx->pel->sett;

	{
		lnk_pe_dos_hdr_t dos = { 0 };

		dos.magic = LNK_PE_DOS_MAGIC;
		dos.lfanew = sizeof(dos);

		fwrite(&dos, sizeof(dos), 1, file);
	}

	{
		lnk_pe_nt_hdrs_t nt = { 0 };

		nt.signature = LNK_PE_NT_SIGNATURE;

		nt.file_header.machine = LnkPeMachAmd64;
		nt.file_header.number_of_sections = (uint16_t)ctx->sect_count;
		nt.file_header.size_of_optional_header = sizeof(nt.optional_header);
		nt.file_header.characteristics = (uint16_t)sett->chars;

		nt.optional_header.magic = LNK_PE_OPT_MAGIC;
		nt.optional_header.image_base = sett->image_base;
		nt.optional_header.section_alignment = (uint32_t)sett->sect_align;
		nt.optional_header.file_alignment = (uint32_t)sett->file_align;
		nt.optional_header.address_of_entry_point = (uint32_t)(ctx->ep_virt_addr);
		nt.optional_header.major_operating_system_version = sett->os_major;
		nt.optional_header.minor_operating_system_version = sett->os_minor;
		nt.optional_header.major_subsystem_version = sett->subsys_major;
		nt.optional_header.minor_subsystem_version = sett->subsys_minor;
		nt.optional_header.size_of_image = (uint32_t)ctx->image_size;
		nt.optional_header.size_of_headers = (uint32_t)ctx->hdrs_size;
		nt.optional_header.subsystem = sett->subsys;
		nt.optional_header.dll_characteristics = sett->dll_chars;
		nt.optional_header.size_of_stack_reserve = sett->stack_res;
		nt.optional_header.size_of_stack_commit = sett->stack_com;
		nt.optional_header.size_of_heap_reserve = sett->heap_res;
		nt.optional_header.size_of_heap_commit = sett->heap_com;
		nt.optional_header.number_of_rva_and_sizes = LnkPeDataDir_Count;

		static const size_t dirs_to_set[] = {
			LnkPeDataDirImport,
			LnkPeDataDirException,
			LnkPeDataDirBaseReloc,
			LnkPeDataDirIat
		};

		for (const size_t * dir = dirs_to_set, *dir_end = dir + _countof(dirs_to_set); dir != dir_end; ++dir) {
			set_dir_info(ctx, &nt, *dir);
		}

		fwrite(&nt, sizeof(nt), 1, file);
	}

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		lnk_pe_sect_hdr_t sect_hdr = { 0 };

		strncpy_s(sect_hdr.name, sizeof(sect_hdr.name), sect->name, SECT_NAME_SIZE);
		sect_hdr.misc.virtual_size = (uint32_t)sect->data_size;
		sect_hdr.virtual_address = (uint32_t)sect->virt_addr;
		sect_hdr.size_of_raw_data = (uint32_t)sect->raw_size;
		sect_hdr.pointer_to_raw_data = (uint32_t)sect->raw_addr;

		sect_hdr.characteristics |= sect->mem_r ? LnkPeSectCharsMemRead : 0;
		sect_hdr.characteristics |= sect->mem_w ? LnkPeSectCharsMemWrite : 0;
		sect_hdr.characteristics |= sect->mem_e ? LnkPeSectCharsMemExecute : 0;
		sect_hdr.characteristics |= sect->mem_disc ? LnkPeSectCharsMemDiscardable : 0;

		fwrite(&sect_hdr, sizeof(sect_hdr), 1, file);
	}

	write_zeros(file, ctx->hdrs_size - ctx->raw_hdrs_size);

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		fwrite(sect->data, sizeof(*sect->data), sect->data_size, file);

		write_zeros(file, sect->raw_size - sect->data_size);
	}
}
static void write_file(ctx_t * ctx) {
	FILE * file = NULL;

	if (_wfopen_s(&file, ctx->pel->sett.file_name, L"wb") != 0) {
		report(ctx, L"failed to open file [%s]", ctx->pel->sett.file_name);
	}
	else {
		write_file_core(ctx, file);

		if (ferror(file) != 0) {
			report(ctx, L"error flag on file [%s] is set", ctx->pel->sett.file_name);
		}

		fclose(file);
	}
}

static void export_pd_labels(ctx_t * ctx) {
	ul_hs_t * hs_labels = UL_HST_HASHADD_WS(ctx->pel->hst, L"labels"),
		* hs_addr = UL_HST_HASHADD_WS(ctx->pel->hst, L"addr"),
		* hs_name = UL_HST_HASHADD_WS(ctx->pel->hst, L"name");

	*ctx->pd_json_ins = ul_json_make_arr();

	ul_json_t * labels = *ctx->pd_json_ins;

	labels->name = hs_labels;

	ctx->pd_json_ins = &labels->next;

	ul_json_t ** ins = &labels->val_json;

	for (label_t * label = ctx->labels, *label_end = label + ctx->labels_size; label != label_end; ++label) {
		*ins = ul_json_make_obj();

		ul_json_t ** elem_ins = &(*ins)->val_json;

		*elem_ins = ul_json_make_int((int64_t)(label->sect->virt_addr + label->off));
		(*elem_ins)->name = hs_addr;
		elem_ins = &(*elem_ins)->next;

		*elem_ins = ul_json_make_str(label->name);
		(*elem_ins)->name = hs_name;
		elem_ins = &(*elem_ins)->next;

		ins = &(*ins)->next;
	}
}
static void export_pd_procs(ctx_t * ctx) {
	ul_hs_t * hs_procs = UL_HST_HASHADD_WS(ctx->pel->hst, L"procs"),
		* hs_start = UL_HST_HASHADD_WS(ctx->pel->hst, L"start"),
		* hs_end = UL_HST_HASHADD_WS(ctx->pel->hst, L"end");

	*ctx->pd_json_ins = ul_json_make_arr();

	ul_json_t * procs = *ctx->pd_json_ins;

	procs->name = hs_procs;

	ctx->pd_json_ins = &procs->next;

	ul_json_t ** ins = &procs->val_json;

	for (proc_t * proc = ctx->procs, *proc_end = proc + ctx->procs_size; proc != proc_end; ++proc) {
		if (proc->label == NULL || proc->end_sect == NULL) {
			continue;
		}

		*ins = ul_json_make_obj();

		ul_json_t ** elem_ins = &(*ins)->val_json;

		*elem_ins = ul_json_make_int((int64_t)(proc->label->sect->virt_addr + proc->label->off));
		(*elem_ins)->name = hs_start;
		elem_ins = &(*elem_ins)->next;

		*elem_ins = ul_json_make_int((int64_t)(proc->end_sect->virt_addr + proc->end_off));
		(*elem_ins)->name = hs_end;
		elem_ins = &(*elem_ins)->next;

		ins = &(*ins)->next;
	}
}
static void export_pd(ctx_t * ctx) {
	size_t file_name_buf_size = (wcslen(ctx->pel->sett.file_name) + wcslen(LNK_PEL_PD_FILE_EXT) + 1);
	wchar_t * file_name_buf = malloc(sizeof(*file_name_buf) * file_name_buf_size);

	ul_assert(file_name_buf != NULL);

	int res = swprintf_s(file_name_buf, file_name_buf_size, L"%s" LNK_PEL_PD_FILE_EXT, ctx->pel->sett.file_name);

	if (res < 0) {
		report(ctx, L"failed to format file name of program database");
	}
	else {
		ctx->pd_json = ul_json_make_obj();
		ctx->pd_json_ins = &ctx->pd_json->val_json;

		export_pd_labels(ctx);

		export_pd_procs(ctx);

		if (!ul_json_g_generate_file(file_name_buf, ctx->pd_json)) {
			report(ctx, L"failed to write program database");
		}
	}

	free(file_name_buf);
}

static bool link_core(ctx_t * ctx) {
	if (!prepare_input_sects(ctx)) {
		return false;
	}

	form_sects(ctx);

	if (!calculate_offsets(ctx)) {
		return false;
	}

	apply_fixups(ctx);

	write_file(ctx);

	if (ctx->pel->sett.export_pd) {
		export_pd(ctx);
	}

	return true;
}
bool lnk_pel_l_link(lnk_pel_t * pel) {
	ctx_t ctx = { .pel = pel };

	bool res = link_core(&ctx);

	ul_json_destroy(ctx.pd_json);

	free(ctx.br_fixups);
	free(ctx.va64_fixups);

	free(ctx.procs);

	free(ctx.fixups);
	free(ctx.labels);

	destroy_sect_chain(ctx.sect);

	lnk_sect_destroy_chain(ctx.mrgd_sect);

	return res && !ctx.is_rptd;
}
