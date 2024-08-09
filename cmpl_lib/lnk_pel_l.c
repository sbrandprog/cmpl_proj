#include "lnk_pel_l.h"
#include "lnk_sect.h"

#define PAGE_SIZE 4096

#define SECT_NAME_SIZE _countof(((IMAGE_SECTION_HEADER*)NULL)->Name)
#define SECT_DEFAULT_ALIGN 16

typedef struct lnk_pel_l_is {
	lnk_sect_t * base;
	size_t index;
} is_t;

typedef struct lnk_pel_l_sect sect_t;
struct lnk_pel_l_sect {
	sect_t * next;
	const char * name;

	size_t data_size;
	uint8_t * data;
	size_t data_cap;

	bool mem_r, mem_w, mem_e, mem_disc;

	size_t index;

	size_t raw_size, raw_addr;
	size_t virt_size, virt_addr;
};

typedef struct lnk_pel_l_label {
	const ul_hs_t * name;
	sect_t * sect;
	size_t offset;
} label_t;
typedef struct lnk_pel_l_fixup {
	lnk_sect_lp_stype_t stype;
	const ul_hs_t * label_name;
	sect_t * sect;
	size_t offset;
} fixup_t;
typedef struct lnk_pel_l_mark {
	sect_t * sect;
	size_t offset;
} mark_t;

typedef struct lnk_pel_l_br_fixup {
	size_t offset;

	sect_t * target_sect;
	size_t target_page;
} br_fixup_t;

typedef struct lnk_pel_l_ctx {
	lnk_pel_t * pel;

	size_t iss_size;
	is_t * iss;
	size_t iss_cap;


	sect_t * sect;
	sect_t ** sect_ins;
	size_t sect_count;


	size_t labels_size;
	label_t * labels;
	size_t labels_cap;

	size_t fixups_size;
	fixup_t * fixups;
	size_t fixups_cap;

	mark_t marks[LnkSectLpMark_Count];


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
} ctx_t;

static const lnk_sect_lp_stype_t dir_start_mark[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {
	[IMAGE_DIRECTORY_ENTRY_IMPORT] = LnkSectLpMarkImpStart,
	[IMAGE_DIRECTORY_ENTRY_BASERELOC] = LnkSectLpMarkRelocStart,
	[IMAGE_DIRECTORY_ENTRY_IAT] = LnkSectLpMarkImpTabStart
};
static const lnk_sect_lp_stype_t dir_end_mark[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {
	[IMAGE_DIRECTORY_ENTRY_IMPORT] = LnkSectLpMarkImpEnd,
	[IMAGE_DIRECTORY_ENTRY_BASERELOC] = LnkSectLpMarkRelocEnd,
	[IMAGE_DIRECTORY_ENTRY_IAT] = LnkSectLpMarkImpTabEnd
};

static label_t * find_label(ctx_t * ctx, const ul_hs_t * name) {
	for (label_t * label = ctx->labels, *label_end = label + ctx->labels_size; label != label_end; ++label) {
		if (name == label->name) {
			return label;
		}
	}

	return NULL;
}

static int input_sect_cmp_proc(void * ctx_ptr, const void * first_ptr, const void * second_ptr) {
	const is_t * first = first_ptr, * second = second_ptr;

	int res = strcmp(first->base->name, second->base->name);

	if (res != 0) {
		return res;
	}

	if (first->index < second->index) {
		return -1;
	}
	else if (first->index > second->index) {
		return 1;
	}

	return 0;
}
static sect_t * add_sect(ctx_t * ctx) {
	sect_t * new_sect = malloc(sizeof(*new_sect));

	ul_assert(new_sect != NULL);

	memset(new_sect, 0, sizeof(*new_sect));

	*ctx->sect_ins = new_sect;
	ctx->sect_ins = &new_sect->next;

	return new_sect;
}
static bool add_lp(ctx_t * ctx, sect_t * sect, const lnk_sect_lp_t * lp, size_t offset) {
	if (lp->offset + offset > sect->data_size) {
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

			if (lp->stype >= LnkSectLpMark_Count) {
				return false;
			}

			mark_t * mark = &ctx->marks[lp->stype];

			if (mark->sect != NULL) {
				return false;
			}

			mark->sect = sect;
			mark->offset = lp->offset + offset;

			return true;
		}
		case LnkSectLpLabel:
		{
			if (ctx->labels_size + 1 > ctx->labels_cap) {
				ul_arr_grow(&ctx->labels_cap, &ctx->labels, sizeof(label_t), 1);
			}

			label_t * label = &ctx->labels[ctx->labels_size++];

			memset(label, 0, sizeof(*label));

			label->name = lp->label_name;
			label->offset = lp->offset + offset;
			label->sect = sect;

			return true;
		}
		case LnkSectLpFixup:
		{
			if (lp->stype == LnkSectLpFixupNone) {
				return true;
			}

			if (lp->stype >= LnkSectLpFixup_Count) {
				return false;
			}

			if (lp->offset + lnk_sect_fixups_size[lp->stype] > sect->data_size) {
				return false;
			}

			if (ctx->fixups_size + 1 > ctx->fixups_cap) {
				ul_arr_grow(&ctx->fixups_cap, &ctx->fixups, sizeof(fixup_t), 1);
			}

			fixup_t * fixup = &ctx->fixups[ctx->fixups_size++];

			memset(fixup, 0, sizeof(*fixup));

			fixup->stype = lp->stype;
			fixup->label_name = lp->label_name;
			fixup->sect = sect;
			fixup->offset = lp->offset + offset;

			if (fixup->stype == LnkSectLpFixupVa64) {
				++ctx->va64_fixups_size;
			}

			return true;
		}
		default:
			return false;
	}
}
static bool form_input_sects(ctx_t * ctx) {
	{
		size_t index = 0;

		for (lnk_sect_t * sect = ctx->pel->sect; sect != NULL; sect = sect->next) {
			if (ctx->iss_size + 1 > ctx->iss_cap) {
				ul_arr_grow(&ctx->iss_cap, &ctx->iss, sizeof(*ctx->iss), 1);
			}

			ctx->iss[ctx->iss_size++] = (is_t){ .base = sect, .index = index++ };
		}

		qsort_s(ctx->iss, ctx->iss_size, sizeof(*ctx->iss), input_sect_cmp_proc, NULL);
	}

	for (is_t * is = ctx->iss, *is_end = is + ctx->iss_size; is != is_end; ) {
		lnk_sect_t * is_base = is->base;
		is_t * cur = is;

		while (cur != is_end && strcmp(cur->base->name, is->base->name) == 0) {
			lnk_sect_t * cur_base = cur->base;

			if (cur_base->mem_r != is_base->mem_r
				|| cur_base->mem_w != is_base->mem_w
				|| cur_base->mem_e != is_base->mem_e
				|| cur_base->mem_disc != is_base->mem_disc) {
				return false;
			}

			++cur;
		}

		is_t * batch_end = cur;

		sect_t * new_sect = add_sect(ctx);

		new_sect->name = is_base->name;
		new_sect->mem_r = is_base->mem_r;
		new_sect->mem_w = is_base->mem_w;
		new_sect->mem_e = is_base->mem_e;
		new_sect->mem_disc = is_base->mem_disc;

		for (is_t * cur = is; cur != batch_end; ++cur) {
			lnk_sect_t * cur_base = cur->base;

			size_t align = cur_base->data_align;

			if (align == 0) {
				align = SECT_DEFAULT_ALIGN;
			}

			size_t sect_start = ul_align_to(new_sect->data_size, align);

			if (sect_start + cur_base->data_size > new_sect->data_cap) {
				ul_arr_grow(&new_sect->data_cap, &new_sect->data, sizeof(uint8_t), sect_start + cur_base->data_size - new_sect->data_cap);
			}

			memset(new_sect->data + new_sect->data_size, cur_base->data_align_byte, sect_start - new_sect->data_size);
			memcpy(new_sect->data + sect_start, cur_base->data, cur_base->data_size);
			new_sect->data_size = sect_start + cur_base->data_size;

			for (const lnk_sect_lp_t * lp = cur_base->lps, *lp_end = lp + cur_base->lps_size; lp != lp_end; ++lp) {
				if (!add_lp(ctx, new_sect, lp, sect_start)) {
					return false;
				}
			}
		}

		is = batch_end;
	}

	return true;
}
static void set_sect_indices(ctx_t * ctx) {
	sect_t * sect = ctx->sect;
	size_t index = 0;

	for (; sect != NULL; sect = sect->next) {
		sect->index = index++;
	}
}
static int base_reloc_cmp_proc(void * ctx_ptr, const void * first_ptr, const void * second_ptr) {
	const fixup_t * first = *(const fixup_t **)first_ptr, * second = *(const fixup_t **)second_ptr;

	if (first->sect->index < second->sect->index) {
		return -1;
	}
	else if (first->sect->index > second->sect->index) {
		return 1;
	}

	if (first->offset < second->offset) {
		return -1;
	}
	else if (first->offset > second->offset) {
		return 1;
	}

	return 0;
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

		set_sect_indices(ctx);

		qsort_s(ctx->va64_fixups, ctx->va64_fixups_size, sizeof(*ctx->va64_fixups), base_reloc_cmp_proc, ctx);
	}

	sect_t * reloc = add_sect(ctx);

	reloc->name = ctx->pel->sett.base_reloc_name;
	reloc->mem_r = true;
	reloc->mem_disc = true;

	for (fixup_t ** va64_f = ctx->va64_fixups, **va64_f_end = va64_f + ctx->va64_fixups_size; va64_f != va64_f_end;) {
		fixup_t * first = *va64_f;

		size_t cur_page = first->offset & ~(PAGE_SIZE - 1);

		fixup_t ** cur = va64_f;

		while (cur != va64_f_end && (*cur)->sect == first->sect && ((*cur)->offset & ~(PAGE_SIZE - 1)) == cur_page) {
			++cur;
		}

		fixup_t ** last_ptr = cur;

		size_t block_start = ul_align_to(reloc->data_size, sizeof(uint32_t));

		size_t block_size = sizeof(IMAGE_BASE_RELOCATION) + (last_ptr - va64_f) * sizeof(WORD);

		if (block_size >= UINT32_MAX) {
			return false;
		}

		if (block_start + block_size > reloc->data_cap) {
			ul_arr_grow(&reloc->data_cap, &reloc->data, sizeof(*reloc->data), block_start + block_size - reloc->data_cap);
		}

		memset(reloc->data + reloc->data_size, 0, block_start - reloc->data_size);

		uint8_t * data_cur = reloc->data + block_start;

		{
			IMAGE_BASE_RELOCATION block_hdr = { .VirtualAddress = 0, .SizeOfBlock = (DWORD)block_size };

			*(IMAGE_BASE_RELOCATION *)data_cur = block_hdr;

			data_cur += sizeof(block_hdr);

			if (ctx->br_fixups_size + 1 > ctx->br_fixups_cap) {
				ul_arr_grow(&ctx->br_fixups_cap, &ctx->br_fixups, sizeof(*ctx->br_fixups), 1);
			}

			ctx->br_fixups[ctx->br_fixups_size++] = (br_fixup_t){
				.offset = block_start + offsetof(IMAGE_BASE_RELOCATION, VirtualAddress),
				.target_sect = first->sect,
				.target_page = cur_page
			};
		}

		for (fixup_t ** cur = va64_f; cur != last_ptr; ++cur) {
			fixup_t * fixup = *cur;

			typedef struct br {
				WORD offset : 12;
				WORD type : 4;
			} br_t;

			br_t br = (br_t){ .offset = (WORD)(fixup->offset - cur_page), .type = IMAGE_REL_BASED_DIR64 };

			*(br_t *)data_cur = br;

			data_cur += sizeof(br);
		}

		reloc->data_size = block_start + block_size;
		va64_f = last_ptr;
	}

	ctx->reloc_sect = reloc;

	ctx->marks[LnkSectLpMarkRelocStart] = (mark_t){ .sect = reloc, .offset = 0 };
	ctx->marks[LnkSectLpMarkRelocEnd] = (mark_t){ .sect = reloc, .offset = reloc->data_size };

	return true;
}
static bool form_sects(ctx_t * ctx) {
	ctx->sect_ins = &ctx->sect;

	if (!form_input_sects(ctx)) {
		return false;
	}

	if (ctx->pel->sett.make_base_reloc && ctx->va64_fixups_size != 0) {
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

		ctx->ep_virt_addr = ep_label->sect->virt_addr + ep_label->offset;
	}

	return true;
}

static bool apply_fixups(ctx_t * ctx) {
	for (fixup_t * fixup = ctx->fixups, *fixup_end = fixup + ctx->fixups_size; fixup != fixup_end; ++fixup) {
		label_t * label = find_label(ctx, fixup->label_name);

		if (label == NULL) {
			return false;
		}

		uint64_t lpos = label->sect->virt_addr + label->offset,
			fpos = fixup->sect->virt_addr + fixup->offset;

		void * write_pos = fixup->sect->data + fixup->offset;

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

	for (br_fixup_t * fixup = ctx->br_fixups, *fixup_end = fixup + ctx->br_fixups_size; fixup != fixup_end; ++fixup) {
		*(uint32_t *)(ctx->reloc_sect->data + fixup->offset) = (uint32_t)(fixup->target_sect->virt_addr + fixup->target_page);
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

	dir->VirtualAddress = (DWORD)(start_mark->sect->virt_addr + start_mark->offset);
	dir->Size = (DWORD)(end_mark->offset - start_mark->offset);

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

static bool link_core(ctx_t * ctx) {
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

	return true;
}
bool lnk_pel_l_link(lnk_pel_t * pel) {
	ctx_t ctx = { .pel = pel };

	bool res = link_core(&ctx);

	free(ctx.br_fixups);
	free(ctx.va64_fixups);

	free(ctx.labels);
	free(ctx.fixups);

	for (sect_t * sect = ctx.sect; sect != NULL;) {
		sect_t * next = sect->next;

		free(sect->data);
		free(sect);

		sect = next;
	}

	free(ctx.iss);

	return res;
}
