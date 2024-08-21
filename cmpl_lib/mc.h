#pragma once
#include "cmpl.h"

#define MC_API CMPL_API

typedef enum mc_defs_sd_type mc_defs_sd_type_t;

typedef uint8_t mc_size_t;
typedef struct mc_size_info mc_size_info_t;

typedef uint8_t mc_reg_t;
typedef union mc_reg_grps mc_reg_grps_t;
typedef struct mc_reg_info mc_reg_info_t;
typedef uint8_t mc_reg_gpr_t;

typedef uint16_t mc_inst_type_t;
typedef uint8_t mc_inst_opd_t;
typedef uint8_t mc_inst_opds_t;
typedef uint8_t mc_inst_imm_type_t;
typedef uint8_t mc_inst_disp_type_t;
typedef struct mc_inst mc_inst_t;
typedef enum mc_inst_off_type mc_inst_off_type_t;
typedef struct mc_inst_offs mc_inst_offs_t;

typedef enum mc_frag_type mc_frag_type_t;
typedef struct mc_frag mc_frag_t;

typedef struct mc_it_sym mc_it_sym_t;
typedef struct mc_it_lib mc_it_lib_t;
typedef struct mc_it mc_it_t;

typedef struct mc_pea mc_pea_t;
