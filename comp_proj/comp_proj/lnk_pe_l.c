#include "pch.h"
#include "lnk_pe_l.h"
#include "lnk_sect.h"
#include "u_misc.h"
#include "u_assert.h"

#define PAGE_SIZE 4096

#define SECT_NAME_SIZE _countof(((IMAGE_SECTION_HEADER*)NULL)->Name)
#define SECT_DEFAULT_ALIGN 16

typedef struct sect sect_t;
struct sect {
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

typedef struct label {
	const u_hs_t * name;
	sect_t * sect;
	size_t offset;
} label_t;
typedef struct fixup {
	lnk_sect_lp_stype_t stype;
	const u_hs_t * label_name;
	sect_t * sect;
	size_t offset;
} fixup_t;
typedef struct mark {
	sect_t * sect;
	size_t offset;
} mark_t;

typedef struct br_fixup {
	size_t offset;

	sect_t * target_sect;
	size_t target_page;
} br_fixup_t;

typedef struct lnk_pe_l_ctx {
	lnk_pe_t * pe;

	size_t sect_buf_size;
	lnk_sect_t ** sect_buf0;
	lnk_sect_t ** sect_buf1;


	sect_t * sect;
	sect_t ** sect_ins;
	size_t sect_count;


	size_t labels_size;
	label_t * labels;
	size_t labels_cap;

	size_t fixups_size;
	fixup_t * fixups;
	size_t fixups_cap;

	mark_t marks[LnkSectMark_Count];


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
	[IMAGE_DIRECTORY_ENTRY_IMPORT] = LnkSectMarkImpStart,
	[IMAGE_DIRECTORY_ENTRY_BASERELOC] = LnkSectMarkRelocStart,
	[IMAGE_DIRECTORY_ENTRY_IAT] = LnkSectMarkImpTabStart
};
static const lnk_sect_lp_stype_t dir_end_mark[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = {
	[IMAGE_DIRECTORY_ENTRY_IMPORT] = LnkSectMarkImpEnd,
	[IMAGE_DIRECTORY_ENTRY_BASERELOC] = LnkSectMarkRelocEnd,
	[IMAGE_DIRECTORY_ENTRY_IAT] = LnkSectMarkImpTabEnd
};

static label_t * find_label(ctx_t * ctx, const u_hs_t * name) {
	for (label_t * label = ctx->labels, *label_end = label + ctx->labels_size; label != label_end; ++label) {
		if (name == label->name) {
			return label;
		}
	}

	return NULL;
}

static void prepare_bufs(ctx_t * ctx) {
	lnk_pe_t * pe = ctx->pe;

	for (lnk_sect_t * sect = pe->sect; sect != NULL; sect = sect->next) {
		++ctx->sect_buf_size;
	}

	ctx->sect_buf0 = malloc(ctx->sect_buf_size * sizeof(*ctx->sect_buf0));
	u_assert(ctx->sect_buf0 != NULL);
	ctx->sect_buf1 = malloc(ctx->sect_buf_size * sizeof(*ctx->sect_buf1));
	u_assert(ctx->sect_buf1 != NULL);

	{
		lnk_sect_t * sect = pe->sect;
		lnk_sect_t ** cur = ctx->sect_buf0;

		for (; sect != NULL; sect = sect->next) {
			*cur++ = sect;
		}
	}
}
static lnk_sect_t ** find_common_sects(ctx_t * ctx) {
	lnk_sect_t ** buf0 = ctx->sect_buf0, ** buf0_end = buf0 + ctx->sect_buf_size;

	while (buf0 != buf0_end && *buf0 == NULL) {
		++buf0;
	}

	lnk_sect_t * first = *buf0;
	lnk_sect_t ** cur = ctx->sect_buf1;

	*cur++ = *buf0;
	*buf0 = NULL;

	for (; buf0 != buf0_end; ++buf0) {
		lnk_sect_t * second = *buf0;

		if (second == NULL) {
			continue;
		}

		if (strncmp(first->name, second->name, SECT_NAME_SIZE) == 0) {
			if (first->mem_r != second->mem_r
				|| first->mem_w != second->mem_w
				|| first->mem_e != second->mem_e
				|| first->mem_disc != second->mem_disc) {
				return NULL;
			}

			*cur++ = *buf0;
			*buf0 = NULL;
		}
	}

	return cur;
}
static sect_t * add_sect(ctx_t * ctx) {
	sect_t * new_sect = malloc(sizeof(*new_sect));

	u_assert(new_sect != NULL);

	memset(new_sect, 0, sizeof(*new_sect));

	if (ctx->sect_ins == NULL) {
		ctx->sect_ins = &ctx->sect;

		while (*ctx->sect_ins != NULL) {
			ctx->sect_ins = &(*ctx->sect_ins)->next;
		}
	}

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
			if (lp->stype == LnkSectMarkNone) {
				return true;
			}

			if (lp->stype >= LnkSectMark_Count) {
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
				u_grow_arr(&ctx->labels_cap, &ctx->labels, sizeof(label_t), 1);
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
			if (lp->stype == LnkSectFixupNone) {
				return true;
			}

			if (lp->stype >= LnkSectFixup_Count) {
				return false;
			}

			if (lp->offset + lnk_sect_fixups_size[lp->stype] > sect->data_size) {
				return false;
			}

			if (ctx->fixups_size + 1 > ctx->fixups_cap) {
				u_grow_arr(&ctx->fixups_cap, &ctx->fixups, sizeof(fixup_t), 1);
			}

			fixup_t * fixup = &ctx->fixups[ctx->fixups_size++];

			memset(fixup, 0, sizeof(*fixup));

			fixup->stype = lp->stype;
			fixup->label_name = lp->label_name;
			fixup->sect = sect;
			fixup->offset = lp->offset + offset;

			if (fixup->stype == LnkSectFixupVa64) {
				++ctx->va64_fixups_size;
			}

			return true;
		}
		default:
			u_assert_switch(lp->type);
	}
}
static bool form_sect(ctx_t * ctx, lnk_sect_t ** buf1_end) {
	sect_t * new_sect = add_sect(ctx);

	{
		u_assert(buf1_end != ctx->sect_buf1);

		lnk_sect_t * base_sect = *ctx->sect_buf1;

		new_sect->name = base_sect->name;
		new_sect->mem_r = base_sect->mem_r;
		new_sect->mem_w = base_sect->mem_w;
		new_sect->mem_e = base_sect->mem_e;
		new_sect->mem_disc = base_sect->mem_disc;
	}

	for (lnk_sect_t ** buf1 = ctx->sect_buf1; buf1 != buf1_end; ++buf1) {
		lnk_sect_t * sect = *buf1;

		size_t align = sect->data_align;

		if (align == 0) {
			align = SECT_DEFAULT_ALIGN;
		}

		size_t sect_start = u_align_to(new_sect->data_size, align);

		if (new_sect->data_size + sect_start + sect->data_size > new_sect->data_cap) {
			u_grow_arr(&new_sect->data_cap, &new_sect->data, sizeof(uint8_t), sect_start + sect->data_size);
		}

		memset(new_sect->data + new_sect->data_size, sect->data_align_byte, sect_start - new_sect->data_size);
		memcpy(new_sect->data + sect_start, sect->data, sect->data_size);
		new_sect->data_size = sect_start + sect->data_size;

		for (const lnk_sect_lp_t * lp = sect->lps, *lp_end = lp + sect->lps_size; lp != lp_end; ++lp) {
			if (!add_lp(ctx, new_sect, lp, sect_start)) {
				return false;
			}
		}
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
static int base_reloc_cmp(void * ctx_ptr, const void * first_ptr, const void * second_ptr) {
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
	if (ctx->marks[LnkSectMarkRelocStart].sect != NULL || ctx->marks[LnkSectMarkRelocEnd].sect != NULL) {
		return false;
	}

	ctx->va64_fixups = malloc(ctx->va64_fixups_size * sizeof(*ctx->va64_fixups));
	u_assert(ctx->va64_fixups != NULL);

	{
		fixup_t ** cur = ctx->va64_fixups, ** cur_end = cur + ctx->va64_fixups_size;
		fixup_t * fixup = ctx->fixups, * fixup_end = fixup + ctx->fixups_size;

		for (; fixup != fixup_end; ++fixup) {
			if (fixup->stype == LnkSectFixupVa64) {
				u_assert(cur != cur_end);

				*cur++ = fixup;
			}
		}

		set_sect_indices(ctx);

		qsort_s(ctx->va64_fixups, ctx->va64_fixups_size, sizeof(*ctx->va64_fixups), base_reloc_cmp, ctx);
	}

	sect_t * reloc = add_sect(ctx);

	reloc->name = ctx->pe->sett->base_reloc_name;
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

		size_t block_start = u_align_to(reloc->data_size, sizeof(uint32_t));

		size_t block_size = sizeof(IMAGE_BASE_RELOCATION) + (last_ptr - va64_f) * sizeof(WORD);

		if (block_size >= UINT32_MAX) {
			return false;
		}

		if (block_start + block_size > reloc->data_cap) {
			u_grow_arr(&reloc->data_cap, &reloc->data, sizeof(*reloc->data), block_start - reloc->data_size + block_size);
		}

		memset(reloc->data + reloc->data_size, 0, block_start - reloc->data_size);

		uint8_t * data_cur = reloc->data + block_start;

		{
			IMAGE_BASE_RELOCATION block_hdr = { .VirtualAddress = 0, .SizeOfBlock = (DWORD)block_size };

			*(IMAGE_BASE_RELOCATION *)data_cur = block_hdr;

			data_cur += sizeof(block_hdr);

			if (ctx->br_fixups_size + 1 > ctx->br_fixups_cap) {
				u_grow_arr(&ctx->br_fixups_cap, &ctx->br_fixups, sizeof(*ctx->br_fixups), 1);
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

	ctx->marks[LnkSectMarkRelocStart] = (mark_t){ .sect = reloc, .offset = 0 };
	ctx->marks[LnkSectMarkRelocEnd] = (mark_t){ .sect = reloc, .offset = reloc->data_size };

	return true;
}
static bool form_sects(ctx_t * ctx) {
	prepare_bufs(ctx);

	size_t remn_sects = ctx->sect_buf_size;

	while (remn_sects > 0) {
		lnk_sect_t ** buf1_end = find_common_sects(ctx);

		if (buf1_end == NULL) {
			return false;
		}

		if (!form_sect(ctx, buf1_end)) {
			return false;
		}

		remn_sects -= buf1_end - ctx->sect_buf1;
	}

	if (ctx->pe->sett->make_base_reloc && ctx->va64_fixups_size != 0) {
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
	lnk_pe_t * pe = ctx->pe;

	ctx->raw_hdrs_size = sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS64) + sizeof(IMAGE_SECTION_HEADER) * ctx->sect_count;
	ctx->hdrs_size = u_align_to(ctx->raw_hdrs_size, pe->sett->file_align);
	ctx->first_sect_va = u_align_to(ctx->hdrs_size, pe->sett->sect_align);

	size_t next_raw_addr = ctx->hdrs_size, next_virt_addr = ctx->first_sect_va;

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		sect->raw_size = u_align_to(sect->data_size, pe->sett->file_align);
		sect->raw_addr = next_raw_addr;
		sect->virt_size = u_align_to(sect->raw_size, pe->sett->sect_align);
		sect->virt_addr = next_virt_addr;

		next_raw_addr += sect->raw_size;
		next_virt_addr += sect->virt_size;

		if (next_virt_addr >= LNK_PE_MAX_MODULE_SIZE) {
			return false;
		}
	}

	ctx->image_size = next_virt_addr;

	{
		label_t * ep_label = find_label(ctx, pe->ep_name);

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
			case LnkSectFixupNone:
				break;
			case LnkSectFixupRel8:
			{
				int64_t var = (int64_t)(lpos - fpos - sizeof(uint8_t));

				if (var > INT8_MAX || INT8_MIN > var) {
					return false;
				}

				*(int8_t *)write_pos = (int8_t)var;
				break;
			}
			case LnkSectFixupRel32:
			{
				int64_t var = (int64_t)(lpos - fpos - sizeof(uint32_t));

				if (var > INT32_MAX || INT32_MIN > var) {
					return false;
				}

				*(int32_t *)write_pos = (int32_t)var;
				break;
			}
			case LnkSectFixupVa64:
				*(uint64_t *)write_pos = (uint64_t)(lpos + ctx->pe->sett->image_base);
				break;
			case LnkSectFixupRva32:
				*(uint32_t *)write_pos = (uint32_t)lpos;
				break;
			case LnkSectFixupRva31of64:
				if ((lpos & ~0x7FFFFFFFull) != 0) {
					return false;
				}

				*(uint64_t *)write_pos = (uint64_t)((lpos & 0x7FFFFFFFull) | (*(uint64_t *)write_pos & 0xFFFFFFFF80000000ull));
				break;
			default:
				u_assert_switch(fixup->stype);
		}
	}

	for (br_fixup_t * fixup = ctx->br_fixups, *fixup_end = fixup + ctx->br_fixups_size; fixup != fixup_end; ++fixup) {
		*(uint32_t *)(ctx->reloc_sect->data + fixup->offset) = (uint32_t)(fixup->target_sect->virt_addr + fixup->target_page);
	}

	return true;
}

static bool set_dir_info(ctx_t * ctx, IMAGE_NT_HEADERS64 * nt, DWORD dir_ent) {
	lnk_sect_lp_stype_t start = dir_start_mark[dir_ent], end = dir_end_mark[dir_ent];

	u_assert(start != LnkSectLpNone && end != LnkSectLpNone);

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
		size_t count = fwrite(zero_obj, sizeof(*zero_obj), _countof(zero_obj), file);

		u_assert(count == _countof(zero_obj));
	}

	size_t count = fwrite(zero_obj, sizeof(*zero_obj), mod, file);

	u_assert(count == mod);
}
static bool write_file_core(ctx_t * ctx, FILE * file) {
	lnk_pe_t * pe = ctx->pe;

	{
		IMAGE_DOS_HEADER dos = { 0 };

		dos.e_magic = IMAGE_DOS_SIGNATURE;
		dos.e_lfanew = sizeof(dos);

		size_t count = fwrite(&dos, sizeof(dos), 1, file);

		u_assert(count == 1);
	}

	{
		IMAGE_NT_HEADERS64 nt = { 0 };

		nt.Signature = IMAGE_NT_SIGNATURE;

		nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
		nt.FileHeader.NumberOfSections = (WORD)ctx->sect_count;
		nt.FileHeader.SizeOfOptionalHeader = sizeof(nt.OptionalHeader);
		nt.FileHeader.Characteristics = (WORD)pe->sett->chars;

		nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
		nt.OptionalHeader.ImageBase = (ULONGLONG)pe->sett->image_base;
		nt.OptionalHeader.SectionAlignment = (DWORD)pe->sett->sect_align;
		nt.OptionalHeader.FileAlignment = (DWORD)pe->sett->file_align;
		nt.OptionalHeader.AddressOfEntryPoint = (DWORD)(ctx->ep_virt_addr);
		nt.OptionalHeader.MajorOperatingSystemVersion = (WORD)pe->sett->os_major;
		nt.OptionalHeader.MinorOperatingSystemVersion = (WORD)pe->sett->os_minor;
		nt.OptionalHeader.MajorSubsystemVersion = (WORD)pe->sett->subsys_major;
		nt.OptionalHeader.MinorSubsystemVersion = (WORD)pe->sett->subsys_minor;
		nt.OptionalHeader.SizeOfImage = (DWORD)ctx->image_size;
		nt.OptionalHeader.SizeOfHeaders = (DWORD)ctx->hdrs_size;
		nt.OptionalHeader.Subsystem = (WORD)pe->sett->subsys;
		nt.OptionalHeader.DllCharacteristics = (WORD)pe->sett->dll_chars;
		nt.OptionalHeader.SizeOfStackReserve = (ULONGLONG)pe->sett->stack_res;
		nt.OptionalHeader.SizeOfStackCommit = (ULONGLONG)pe->sett->stack_com;
		nt.OptionalHeader.SizeOfHeapReserve = (ULONGLONG)pe->sett->heap_res;
		nt.OptionalHeader.SizeOfHeapCommit = (ULONGLONG)pe->sett->heap_com;
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

		size_t count = fwrite(&nt, sizeof(nt), 1, file);

		u_assert(count == 1);
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

		size_t count = fwrite(&sect_hdr, sizeof(sect_hdr), 1, file);

		u_assert(count == 1);
	}

	write_zeros(file, ctx->hdrs_size - ctx->raw_hdrs_size);

	for (sect_t * sect = ctx->sect; sect != NULL; sect = sect->next) {
		size_t count = fwrite(sect->data, sizeof(*sect->data), sect->data_size, file);

		u_assert(count == sect->data_size);

		write_zeros(file, sect->raw_size - sect->data_size);
	}

	return true;
}
static bool write_file(ctx_t * ctx) {
	FILE * file;

	if (_wfopen_s(&file, ctx->pe->file_name, L"wb") != 0) {
		return false;
	}

	bool result = write_file_core(ctx, file);

	fclose(file);

	return result;
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
bool lnk_pe_l_link(lnk_pe_t * pe) {
	ctx_t ctx = { .pe = pe };

	bool result = link_core(&ctx);

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

	free(ctx.sect_buf0);
	free(ctx.sect_buf1);

	return result;
}
