#pragma once
#include "cmpl.h"

#define PLA_API CMPL_API

typedef enum pla_pds pla_pds_t;

typedef struct pla_ec_pos pla_ec_pos_t;

typedef struct pla_ec_err pla_ec_err_t;

typedef enum pla_punc pla_punc_t;

typedef enum pla_keyw pla_keyw_t;

typedef enum pla_tok_type pla_tok_type_t;
typedef struct pla_tok pla_tok_t;

typedef bool pla_lex_src_get_ch_proc_t(void * src_data, wchar_t * out);
typedef struct pla_lex_src pla_lex_src_t;
typedef struct pla_lex pla_lex_t;

typedef struct pla_cn pla_cn_t;

typedef enum pla_expr_opd_type pla_expr_opd_type_t;
typedef union pla_expr_opd pla_expr_opd_t;
typedef enum pla_expr_type pla_expr_type_t;
typedef struct pla_expr pla_expr_t;
typedef struct pla_expr_info pla_expr_info_t;

typedef enum pla_stmt_type pla_stmt_type_t;
typedef struct pla_stmt pla_stmt_t;
typedef struct pla_stmt_info pla_stmt_info_t;

typedef enum pla_dclr_type pla_dclr_type_t;
typedef struct pla_dclr pla_dclr_t;
typedef struct pla_dclr_info pla_dclr_info_t;

typedef struct pla_tu_ref pla_tu_ref_t;
typedef struct pla_tu pla_tu_t;

typedef bool pla_prsr_src_get_tok_proc_t(void * src_data, pla_tok_t * out);
typedef struct pla_prsr_src pla_prsr_src_t;
typedef struct pla_prsr_rse pla_prsr_rse_t;
typedef struct pla_prsr pla_prsr_t;

typedef struct pla_tltr_src pla_tltr_src_t;
typedef enum pla_tltr_tse_type pla_tltr_tse_type_t;
typedef struct pla_tltr_tse pla_tltr_tse_t;
typedef enum pla_tltr_vse_type pla_tltr_vse_type_t;
typedef struct pla_tltr_vse pla_tltr_vse_t;
typedef struct pla_tltr pla_tltr_t;

typedef struct pla_tus pla_tus_t;

typedef struct pla_pkg pla_pkg_t;

typedef struct pla_repo pla_repo_t;

typedef struct pla_repo_lsnr pla_repo_lsnr_t;

typedef struct pla_bs_sett pla_bs_sett_t;
