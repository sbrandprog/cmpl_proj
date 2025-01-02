#pragma once
#include "cmpl.h"

#define LNK_API CMPL_API

typedef enum lnk_sect_lp_type lnk_sect_lp_type_t;
typedef enum lnk_sect_lp_stype lnk_sect_lp_stype_t;
typedef struct lnk_sect_lp lnk_sect_lp_t;

typedef struct lnk_sect lnk_sect_t;
typedef struct lnk_sect_desc lnk_sect_desc_t;

typedef struct lnk_pe_dos_hdr lnk_pe_dos_hdr_t;

typedef uint16_t lnk_pe_mach_t;
typedef uint16_t lnk_pe_chars_t;
typedef struct lnk_pe_file_hdr lnk_pe_file_hdr_t;

typedef struct lnk_pe_data_dir lnk_pe_data_dir_t;
typedef enum lnk_pe_data_dir_type lnk_pe_data_dir_type_t;

typedef uint16_t lnk_pe_subsys_t;
typedef uint16_t lnk_pe_dll_chars_t;
typedef struct lnk_pe_opt_hdr lnk_pe_opt_hdr_t;

typedef struct lnk_pe_nt_hdrs lnk_pe_nt_hdrs_t;

typedef uint32_t lnk_pe_sect_chars_t;
typedef struct lnk_pe_sect_hdr lnk_pe_sect_hdr_t;

typedef struct lnk_pe_rt_func lnk_pe_rt_func_t;

typedef struct lnk_pe_base_reloc lnk_pe_base_reloc_t;
typedef struct lnk_pe_base_reloc_entry lnk_pe_base_reloc_entry_t;

typedef struct lnk_pe_impt_desc lnk_pe_impt_desc_t;

typedef struct lnk_pel_sett lnk_pel_sett_t;
typedef struct lnk_pel lnk_pel_t;
