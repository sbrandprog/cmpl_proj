#pragma once

#if defined UL_BUILD_DLL
#	define UL_API __declspec(dllexport)
#else
#	define UL_API __declspec(dllimport)
#endif

typedef struct ul_ros ul_ros_t;

typedef struct ul_es_ctx ul_es_ctx_t;
typedef struct ul_es_node ul_es_node_t;

typedef uint64_t ul_hs_hash_t;
typedef struct ul_hs ul_hs_t;

typedef struct ul_hsb ul_hsb_t;

typedef struct ul_hst_node ul_hst_node_t;
typedef struct ul_hst ul_hst_t;
