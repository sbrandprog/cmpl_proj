#pragma once
#include "ul.h"

enum ul_fs_ent_type {
	UlFsEntNone,
	UlFsEntFile,
	UlFsEntDir,
	UlFsEnt_Count
};
struct ul_fs_ent {
	ul_fs_ent_type_t type;
	wchar_t * name;
};
struct ul_fs_dir {
	size_t ents_size;
	ul_fs_ent_t * ents;
	size_t ents_cap;
};

UL_API ul_fs_ent_type_t ul_fs_get_ent_type(const wchar_t * entry_path);

UL_API ul_fs_dir_t * ul_fs_dir_read(const wchar_t * dir_path);
UL_API void ul_fs_dir_free(ul_fs_dir_t * dir);
