#include "lnk_sect.h"
#include "lnk_pel_m.h"
#include "lnk_pel_l.h"

#define PAGE_SIZE 4096

#define SECT_NAME_SIZE _countof(((IMAGE_SECTION_HEADER*)NULL)->Name)
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

static const lnk_sect_lp_stype_t dir_start_mark[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {
	[IMAGE_DIRECTORY_ENTRY_IMPORT] = LnkSectLpMarkImpStart,
	[IMAGE_DIRECTORY_ENTRY_EXCEPTION] = LnkSectLpMarkExcptStart,
	[IMAGE_DIRECTORY_ENTRY_BASERELOC] = LnkSectLpMarkRelocStart,
	[IMAGE_DIRECTORY_ENTRY_IAT] = LnkSectLpMarkImpTabStart
};
static const lnk_sect_lp_stype_t dir_end_mark[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {
	[IMAGE_DIRECTORY_ENTRY_IMPORT] = LnkSectLpMarkImpEnd,
	[IMAGE_DIRECTORY_ENTRY_EXCEPTION] = LnkSectLpMarkExcptEnd,
	[IMAGE_DIRECTORY_ENTRY_BASERELOC] = LnkSectLpMarkRelocEnd,
	[IMAGE_DIRECTORY_ENTRY_IAT] = LnkSectLpMarkImpTabEnd
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


static bool prepare_input_sects(ctx_t * ctx) {
	if (ctx->pel->sett.apply_mrgr) {
		if (!lnk_pel_m_merge(ctx->pel, &ctx->mrgd_sect)) {
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
static bool add_lp(ctx_t * ctx, sect_t * sect, const lnk_sect_lp_t * lp) {
	if (lp->off > sect->data_size) {
		return false;
	}

	switch (lp->type) {
		case LnkSectLpNone:
			return true;
		case LnkSectLpMark:
		{
			if (lp->stype == LnkSectLpMarkNone) {
				return true;
			}
			else if (lp->stype >= LnkSectLpMark_Count) {
				return false;
			}

			mark_t * mark = &ctx->marks[lp->stype];

			if (mark->sect != NULL) {
				return false;
			}

			mark->sect = sect;
			mark->off = lp->off;

			return true;
		}
		case LnkSectLpLabel:
		{
			if (lp->stype == LnkSectLpLabelNone) {
				return true;
			}
			else if (lp->stype >= LnkSectLpLabel_Count) {
				return false;
			}

			switch (lp->stype) {
				case LnkSectLpLabelBasic:
				{

					label_t new_label = { .name = lp->label_name, .sect = sect, .off = lp->off };

					size_t ins_pos = ul_bs_lower_bound(sizeof(*ctx->labels), ctx->labels_size, ctx->labels, label_cmp_proc, &new_label);

					if (ins_pos < ctx->labels_size && ctx->labels[ins_pos].name == new_label.name) {
						return false;
					}

					ul_arr_grow(ctx->labels_size + 1, &ctx->labels_cap, &ctx->labels, sizeof(*ctx->labels));

					memmove_s(ctx->labels + ins_pos + 1, sizeof(*ctx->labels) * (ctx->labels_cap - ins_pos), ctx->labels + ins_pos, sizeof(*ctx->labels) * (ctx->labels_size - ins_pos));

					ctx->labels[ins_pos] = new_label;
					++ctx->labels_size;

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
									return false;
								}

								proc->end_sect = sect;
								proc->end_off = lp->off;
								break;
							case LnkSectLpLabelProcUnw:
								if (proc->unw_sect != NULL) {
									return false;
								}

								proc->unw_sect = sect;
								proc->unw_off = lp->off;
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

			return true;
		}
		case LnkSectLpFixup:
		{
			if (lp->stype == LnkSectLpFixupNone) {
				return true;
			}
			else if (lp->stype >= LnkSectLpFixup_Count) {
				return false;
			}

			if (lp->off + lnk_sect_fixups_size[lp->stype] > sect->data_size) {
				return false;
			}

			ul_arr_grow(ctx->fixups_size + 1, &ctx->fixups_cap, &ctx->fixups, sizeof(*ctx->fixups));

			ctx->fixups[ctx->fixups_size++] = (fixup_t){ .stype = lp->stype, .label_name = lp->label_name, .sect = sect, .off = lp->off };

			if (lp->stype == LnkSectLpFixupVa64) {
				++ctx->va64_fixups_size;
			}

			return true;
		}
		default:
			return false;
	}
}
static bool form_main_sects(ctx_t * ctx) {
	for (lnk_sect_t * lnk_sect = ctx->input_sect; lnk_sect != NULL; lnk_sect = lnk_sect->next) {
		sect_t * sect = add_sect(ctx, lnk_sect->name, lnk_sect);

		if (sect == NULL) {
			return false;
		}

		for (lnk_sect_lp_t * lp = lnk_sect->lps, *lp_end = lp + lnk_sect->lps_size; lp != lp_end; ++lp) {
			if (!add_lp(ctx, sect, lp)) {
				return false;
			}
		}
	}

	return true;
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
static bool form_excpt_data_sect(ctx_t * ctx) {
	if (ctx->marks[LnkSectLpMarkExcptStart].sect != NULL || ctx->marks[LnkSectLpMarkExcptEnd].sect != NULL) {
		return false;
	}

	for (proc_t * proc = ctx->procs, *proc_end = proc + ctx->procs_size; proc != proc_end; ++proc) {
		if (proc->end_sect == NULL || proc->unw_sect == NULL) {
			return false;
		}

		proc->label = find_label(ctx, proc->label_name);

		if (proc->label == NULL) {
			return false;
		}
	}

	sect_t * excpt = add_sect(ctx, ctx->pel->sett.excpt_sect_name, NULL);

	if (excpt == NULL) {
		return false;
	}

	size_t excpt_size = sizeof(RUNTIME_FUNCTION) * ctx->procs_size;

	excpt->data = malloc(excpt_size);

	ul_assert(excpt->data != NULL);

	excpt->data_size = excpt_size;
	excpt->data_cap = excpt_size;

	excpt->name = ctx->pel->sett.excpt_sect_name;
	excpt->mem_r = true;

	ctx->excpt_sect = excpt;

	ctx->marks[LnkSectLpMarkExcptStart] = (mark_t){ .sect = excpt, .off = 0 };
	ctx->marks[LnkSectLpMarkExcptEnd] = (mark_t){ .sect = excpt, .off = excpt->data_size };

	return true;
}
static bool form_base_reloc_sect(ctx_t * ctx) {
	if (ctx->marks[LnkSectLpMarkRelocStart].sect != NULL || ctx->marks[LnkSectLpMarkRelocEnd].sect != NULL) {
		return false;
	}

	ctx->va64_fixups = malloc(ctx->va64_fixups_size * sizeof(*ctx->va64_fixups));
	
	ul_assert(ctx->va64_fixups != NULL);

	{
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

	sect_t * reloc = add_sect(ctx, ctx->pel->sett.base_reloc_sect_name, NULL);

	if (reloc == NULL) {
		return false;
	}

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

		size_t block_size = sizeof(IMAGE_BASE_RELOCATION) + (last_ptr - va64_f) * sizeof(WORD);

		if (block_size >= UINT32_MAX) {
			return false;
		}

		ul_arr_grow(block_start + block_size, &reloc->data_cap, &reloc->data, sizeof(*reloc->data));

		memset(reloc->data + reloc->data_size, 0, block_start - reloc->data_size);

		uint8_t * data_cur = reloc->data + block_start;

		{
			IMAGE_BASE_RELOCATION block_hdr = { .VirtualAddress = 0, .SizeOfBlock = (DWORD)block_size };

			*(IMAGE_BASE_RELOCATION *)data_cur = block_hdr;

			data_cur += sizeof(block_hdr);

			ul_arr_grow(ctx->br_fixups_size + 1, &ctx->br_fixups_cap, &ctx->br_fixups, sizeof(*ctx->br_fixups));

			ctx->br_fixups[ctx->br_fixups_size++] = (br_fixup_t){
				.off = block_start + offsetof(IMAGE_BASE_RELOCATION, VirtualAddress),
				.target_sect = first->sect,
				.target_page = cur_page
			};
		}

		for (fixup_t ** cur = va64_f; cur != last_ptr; ++cur) {
			fixup_t * fixup = *cur;

			typedef struct br {
				WORD off : 12;
				WORD type : 4;
			} br_t;

			br_t br = (br_t){ .off = (WORD)(fixup->off - cur_page), .type = IMAGE_REL_BASED_DIR64 };

			*(br_t *)data_cur = br;

			data_cur += sizeof(br);
		}

		reloc->data_size = block_start + block_size;
		va64_f = last_ptr;
	}

	ctx->reloc_sect = reloc;

	ctx->marks[LnkSectLpMarkRelocStart] = (mark_t){ .sect = reloc, .off = 0 };
	ctx->marks[LnkSectLpMarkRelocEnd] = (mark_t){ .sect = reloc, .off = reloc->data_size };

	return true;
}
static bool form_sects(ctx_t * ctx) {
	if (!form_main_sects(ctx)) {
		return false;
	}

	if (ctx->pel->sett.make_excpt_sect && ctx->procs_size > 0) {
		if (!form_excpt_data_sect(ctx)) {
			return false;
		}
	}

	if (ctx->pel->sett.make_base_reloc_sect && ctx->va64_fixups_size > 0) {
		if (!form_base_reloc_sect(ctx)) {
			return false;
		}
	}

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		++ctx->sect_count;
	}

	return true;
}

static bool calculate_offsets(ctx_t * ctx) {
	lnk_pel_t * pel = ctx->pel;
	lnk_pel_sett_t * sett = &pel->sett;

	ctx->raw_hdrs_size = sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS64) + sizeof(IMAGE_SECTION_HEADER) * ctx->sect_count;
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
			return false;
		}
	}

	ctx->image_size = next_virt_addr;

	{
		label_t * ep_label = find_label(ctx, pel->ep_name);

		if (ep_label == NULL) {
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

static bool apply_fixups_excpt(ctx_t * ctx) {
	if (ctx->excpt_sect == NULL) {
		return true;;
	}

	qsort_s(ctx->procs, ctx->procs_size, sizeof(*ctx->procs), excpt_cmp_proc, NULL);

	sect_t * excpt = ctx->excpt_sect;

	RUNTIME_FUNCTION * cur = (RUNTIME_FUNCTION *)excpt->data;

	for (proc_t * proc = ctx->procs, *proc_end = proc + ctx->procs_size; proc != proc_end; ++proc, ++cur) {
		*cur = (RUNTIME_FUNCTION){ .BeginAddress = (DWORD)(proc->label->sect->virt_addr + proc->label->off), .EndAddress = (DWORD)(proc->end_sect->virt_addr + proc->end_off), .UnwindData = (DWORD)(proc->unw_sect->virt_addr + proc->unw_off) };
	}

	ul_assert((uint8_t *)cur == excpt->data + excpt->data_size);

	return true;
}
static bool apply_fixups_base_reloc(ctx_t * ctx) {
	if (ctx->reloc_sect == NULL) {
		return true;
	}

	for (br_fixup_t * fixup = ctx->br_fixups, *fixup_end = fixup + ctx->br_fixups_size; fixup != fixup_end; ++fixup) {
		*(uint32_t *)(ctx->reloc_sect->data + fixup->off) = (uint32_t)(fixup->target_sect->virt_addr + fixup->target_page);
	}

	return true;
}
static bool apply_fixups(ctx_t * ctx) {
	for (fixup_t * fixup = ctx->fixups, *fixup_end = fixup + ctx->fixups_size; fixup != fixup_end; ++fixup) {
		label_t * label = find_label(ctx, fixup->label_name);

		if (label == NULL) {
			return false;
		}

		uint64_t lpos = label->sect->virt_addr + label->off,
			fpos = fixup->sect->virt_addr + fixup->off;

		void * write_pos = fixup->sect->data + fixup->off;

		switch (fixup->stype) {
			case LnkSectLpFixupNone:
				break;
			case LnkSectLpFixupRel8:
			{
				int64_t var = (int64_t)(lpos - fpos - sizeof(uint8_t));

				if (var > INT8_MAX || INT8_MIN > var) {
					return false;
				}

				*(int8_t *)write_pos = (int8_t)var;
				break;
			}
			case LnkSectLpFixupRel32:
			{
				int64_t var = (int64_t)(lpos - fpos - sizeof(uint32_t));

				if (var > INT32_MAX || INT32_MIN > var) {
					return false;
				}

				*(int32_t *)write_pos = (int32_t)var;
				break;
			}
			case LnkSectLpFixupVa64:
				*(uint64_t *)write_pos = (uint64_t)(lpos + ctx->pel->sett.image_base);
				break;
			case LnkSectLpFixupRva32:
				*(uint32_t *)write_pos = (uint32_t)lpos;
				break;
			case LnkSectLpFixupRva31of64:
				if ((lpos & ~0x7FFFFFFFull) != 0) {
					return false;
				}

				*(uint64_t *)write_pos = (uint64_t)((lpos & 0x7FFFFFFFull) | (*(uint64_t *)write_pos & 0xFFFFFFFF80000000ull));
				break;
			default:
				ul_assert_unreachable();
		}
	}

	if (!apply_fixups_excpt(ctx)) {
		return false;
	}

	if (!apply_fixups_base_reloc(ctx)) {
		return false;
	}

	return true;
}

static bool set_dir_info(ctx_t * ctx, IMAGE_NT_HEADERS64 * nt, DWORD dir_ent) {
	lnk_sect_lp_stype_t start = dir_start_mark[dir_ent], end = dir_end_mark[dir_ent];

	ul_assert(start != LnkSectLpNone && end != LnkSectLpNone);

	mark_t * start_mark = &ctx->marks[start], * end_mark = &ctx->marks[end];

	if (start_mark->sect == NULL) {
		return true;
	}

	if (start_mark->sect != end_mark->sect) {
		return false;
	}

	IMAGE_DATA_DIRECTORY * dir = &nt->OptionalHeader.DataDirectory[dir_ent];

	dir->VirtualAddress = (DWORD)(start_mark->sect->virt_addr + start_mark->off);
	dir->Size = (DWORD)(end_mark->off - start_mark->off);

	return true;
}
static void write_zeros(FILE * file, size_t size) {
	static const uint8_t zero_obj[1024] = { 0 };

	size_t div = size / _countof(zero_obj), mod = size % _countof(zero_obj);

	for (size_t i = 0; i < div; ++i) {
		fwrite(zero_obj, sizeof(*zero_obj), _countof(zero_obj), file);
	}

	fwrite(zero_obj, sizeof(*zero_obj), mod, file);
}
static bool write_file_core(ctx_t * ctx, FILE * file) {
	lnk_pel_sett_t * sett = &ctx->pel->sett;

	{
		IMAGE_DOS_HEADER dos = { 0 };

		dos.e_magic = IMAGE_DOS_SIGNATURE;
		dos.e_lfanew = sizeof(dos);

		fwrite(&dos, sizeof(dos), 1, file);
	}

	{
		IMAGE_NT_HEADERS64 nt = { 0 };

		nt.Signature = IMAGE_NT_SIGNATURE;

		nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
		nt.FileHeader.NumberOfSections = (WORD)ctx->sect_count;
		nt.FileHeader.SizeOfOptionalHeader = sizeof(nt.OptionalHeader);
		nt.FileHeader.Characteristics = (WORD)sett->chars;

		nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
		nt.OptionalHeader.ImageBase = (ULONGLONG)sett->image_base;
		nt.OptionalHeader.SectionAlignment = (DWORD)sett->sect_align;
		nt.OptionalHeader.FileAlignment = (DWORD)sett->file_align;
		nt.OptionalHeader.AddressOfEntryPoint = (DWORD)(ctx->ep_virt_addr);
		nt.OptionalHeader.MajorOperatingSystemVersion = (WORD)sett->os_major;
		nt.OptionalHeader.MinorOperatingSystemVersion = (WORD)sett->os_minor;
		nt.OptionalHeader.MajorSubsystemVersion = (WORD)sett->subsys_major;
		nt.OptionalHeader.MinorSubsystemVersion = (WORD)sett->subsys_minor;
		nt.OptionalHeader.SizeOfImage = (DWORD)ctx->image_size;
		nt.OptionalHeader.SizeOfHeaders = (DWORD)ctx->hdrs_size;
		nt.OptionalHeader.Subsystem = (WORD)sett->subsys;
		nt.OptionalHeader.DllCharacteristics = (WORD)sett->dll_chars;
		nt.OptionalHeader.SizeOfStackReserve = (ULONGLONG)sett->stack_res;
		nt.OptionalHeader.SizeOfStackCommit = (ULONGLONG)sett->stack_com;
		nt.OptionalHeader.SizeOfHeapReserve = (ULONGLONG)sett->heap_res;
		nt.OptionalHeader.SizeOfHeapCommit = (ULONGLONG)sett->heap_com;
		nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

		if (!set_dir_info(ctx, &nt, IMAGE_DIRECTORY_ENTRY_IMPORT)) {
			return false;
		}
		if (!set_dir_info(ctx, &nt, IMAGE_DIRECTORY_ENTRY_EXCEPTION)) {
			return false;
		}
		if (!set_dir_info(ctx, &nt, IMAGE_DIRECTORY_ENTRY_BASERELOC)) {
			return false;
		}
		if (!set_dir_info(ctx, &nt, IMAGE_DIRECTORY_ENTRY_IAT)) {
			return false;
		}

		fwrite(&nt, sizeof(nt), 1, file);
	}

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		IMAGE_SECTION_HEADER sect_hdr = { 0 };

		strncpy_s(sect_hdr.Name, sizeof(sect_hdr.Name), sect->name, SECT_NAME_SIZE);
		sect_hdr.Misc.VirtualSize = (DWORD)sect->data_size;
		sect_hdr.VirtualAddress = (DWORD)sect->virt_addr;
		sect_hdr.SizeOfRawData = (DWORD)sect->raw_size;
		sect_hdr.PointerToRawData = (DWORD)sect->raw_addr;

		sect_hdr.Characteristics |= sect->mem_r ? IMAGE_SCN_MEM_READ : 0;
		sect_hdr.Characteristics |= sect->mem_w ? IMAGE_SCN_MEM_WRITE : 0;
		sect_hdr.Characteristics |= sect->mem_e ? IMAGE_SCN_MEM_EXECUTE : 0;
		sect_hdr.Characteristics |= sect->mem_disc ? IMAGE_SCN_MEM_DISCARDABLE : 0;

		fwrite(&sect_hdr, sizeof(sect_hdr), 1, file);
	}

	write_zeros(file, ctx->hdrs_size - ctx->raw_hdrs_size);

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		fwrite(sect->data, sizeof(*sect->data), sect->data_size, file);

		write_zeros(file, sect->raw_size - sect->data_size);
	}

	return true;
}
static bool write_file(ctx_t * ctx) {
	FILE * file;

	if (_wfopen_s(&file, ctx->pel->file_name, L"wb") != 0) {
		return false;
	}

	bool res = write_file_core(ctx, file);

	bool file_no_err = ferror(file) == 0;

	fclose(file);

	return res && file_no_err;
}

static bool export_pd_labels(ctx_t * ctx) {
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

	return true;
}
static bool export_pd_procs(ctx_t * ctx) {
	ul_hs_t * hs_procs = UL_HST_HASHADD_WS(ctx->pel->hst, L"procs"),
		* hs_start = UL_HST_HASHADD_WS(ctx->pel->hst, L"start"),
		* hs_end = UL_HST_HASHADD_WS(ctx->pel->hst, L"end");

	*ctx->pd_json_ins = ul_json_make_arr();
	
	ul_json_t * procs = *ctx->pd_json_ins;

	procs->name = hs_procs;

	ctx->pd_json_ins = &procs->next;

	ul_json_t ** ins = &procs->val_json;

	for (proc_t * proc = ctx->procs, *proc_end = proc + ctx->procs_size; proc != proc_end; ++proc) {
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

	return true;
}
static bool export_pd(ctx_t * ctx) {
	wchar_t file_name_buf[MAX_PATH];

	int res = swprintf_s(file_name_buf, _countof(file_name_buf), L"%s" LNK_PEL_PD_FILE_EXT, ctx->pel->file_name);

	if (res < 0) {
		return false;
	}

	ctx->pd_json = ul_json_make_obj();
	ctx->pd_json_ins = &ctx->pd_json->val_json;

	if (!export_pd_labels(ctx)) {
		return false;
	}

	if (!export_pd_procs(ctx)) {
		return false;
	}

	if (!ul_json_g_generate_file(file_name_buf, ctx->pd_json)) {
		return false;
	}

	return true;
}

static bool link_core(ctx_t * ctx) {
	if (!prepare_input_sects(ctx)) {
		return false;
	}

	if (!form_sects(ctx)) {
		return false;
	}

	if (!calculate_offsets(ctx)) {
		return false;
	}

	if (!apply_fixups(ctx)) {
		return false;
	}

	if (!write_file(ctx)) {
		return false;
	}

	if (ctx->pel->sett.export_pd && !export_pd(ctx)) {
		return false;
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

	return res;
}
