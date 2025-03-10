#pragma once

#if defined _WIN32
#if defined UL_BUILD_DLL
#define UL_API __declspec(dllexport)
#else
#define UL_API __declspec(dllimport)
#endif
#else
#define UL_API
#endif

typedef struct ul_ros ul_ros_t;

typedef bool ul_bs_cmp_proc_t(const void * first_ptr, const void * second_ptr);

typedef enum ul_fs_ent_type ul_fs_ent_type_t;
typedef struct ul_fs_ent ul_fs_ent_t;
typedef struct ul_fs_dir ul_fs_dir_t;

typedef uint64_t ul_hs_hash_t;
typedef struct ul_hs ul_hs_t;

typedef struct ul_hsb ul_hsb_t;

typedef struct ul_hst_node ul_hst_node_t;
typedef struct ul_hst ul_hst_t;

typedef struct ul_ec_rec ul_ec_rec_t;
typedef enum ul_ec_actn_type ul_ec_actn_type_t;
typedef struct ul_ec_actn ul_ec_actn_t;
typedef void(ul_ec_process_actn_proc_t)(void * user_data, const ul_ec_actn_t * actn);
typedef struct ul_ec ul_ec_t;

typedef bool ul_ec_prntr_print_proc_t(void * user_data, ul_ec_rec_t * rec);
typedef struct ul_ec_prntr ul_ec_prntr_t;

typedef struct ul_ec_buf ul_ec_buf_t;

typedef struct ul_ec_fmtr ul_ec_fmtr_t;

typedef enum ul_json_type ul_json_type_t;
typedef struct ul_json ul_json_t;

typedef bool ul_json_p_get_ch_proc_t(void * src_data, char * out);
typedef struct ul_json_p_src ul_json_p_src_t;

typedef bool ul_json_g_put_ch_proc_t(void * sink_data, char ch);
typedef struct ul_json_g_sink ul_json_g_sink_t;
