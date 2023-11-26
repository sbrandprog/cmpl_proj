#pragma once

typedef uint8_t asm_size_t;

typedef uint8_t asm_reg_t;
typedef union asm_reg_grps asm_reg_grps_t;
typedef struct asm_reg_info asm_reg_info_t;

typedef uint16_t asm_inst_type_t;
typedef uint8_t asm_inst_opds_t;
typedef uint8_t asm_inst_imm_type_t;
typedef uint8_t asm_inst_disp_type_t;
typedef struct asm_inst asm_inst_t;
typedef enum asm_inst_off_type asm_inst_off_type_t;
typedef struct asm_inst_offs asm_inst_offs_t;

typedef enum asm_frag_type asm_frag_type_t;
typedef struct asm_frag asm_frag_t;

typedef struct asm_it_sym asm_it_sym_t;
typedef struct asm_it_lib asm_it_lib_t;
typedef struct asm_it asm_it_t;

typedef struct asm_pea asm_pea_t;
