#pragma once
#include "ul.h"

#define UL_EC_REC_STRS_SIZE_MAX (256 - (sizeof(const wchar_t *) * 2 + sizeof(void *) + sizeof(size_t)) / sizeof(wchar_t))

#define UL_EC_REC_TYPE_DFLT L"ul_ec_rec"

struct ul_ec_rec {
	const wchar_t * type;
	ul_ec_rec_t * next;
	const wchar_t * mod_name;
	size_t strs_size;
	wchar_t strs[UL_EC_REC_STRS_SIZE_MAX];
};

enum ul_ec_actn_type {
	UlEcActnPost,
	UlEcActnClear,
	UlEcActn_Count
};
struct ul_ec_actn {
	ul_ec_actn_type_t type;
	union {
		struct {
			const ul_ec_rec_t * rec;
		} post;
		struct {
			const wchar_t * type;
			const wchar_t * mod_name;
		} clear;
	};
};

struct ul_ec {
	void * user_data;
	ul_ec_process_actn_proc_t * process_actn_proc;
};

UL_API ul_ec_rec_t * ul_ec_rec_create(const wchar_t * type);
UL_API ul_ec_rec_t * ul_ec_rec_copy(const ul_ec_rec_t * rec);
UL_API void ul_ec_rec_destroy(ul_ec_rec_t * rec);
UL_API void ul_ec_rec_destroy_chain(ul_ec_rec_t * rec);

UL_API void ul_ec_rec_dump(ul_ec_rec_t * rec);

UL_API void ul_ec_init(ul_ec_t * ec, void * user_data, ul_ec_process_actn_proc_t * process_actn_proc);
UL_API void ul_ec_cleanup(ul_ec_t * ec);

inline void ul_ec_process_actn(ul_ec_t * ec, const ul_ec_actn_t * actn) {
	ec->process_actn_proc(ec->user_data, actn);
}
inline void ul_ec_post(ul_ec_t * ec, ul_ec_rec_t * rec) {
	ul_ec_actn_t actn = { .type = UlEcActnPost, .post.rec = rec };

	ec->process_actn_proc(ec->user_data, &actn);
}
inline void ul_ec_clear(ul_ec_t * ec, const wchar_t * type, const wchar_t * mod_name) {
	ul_ec_actn_t actn = { .type = UlEcActnClear, .clear = { .type = type, .mod_name = mod_name } };

	ec->process_actn_proc(ec->user_data, &actn);
}
inline void ul_ec_clear_all(ul_ec_t * ec) {
	ul_ec_clear(ec, NULL, NULL);
}
