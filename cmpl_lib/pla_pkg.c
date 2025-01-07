#include "pla_tus.h"
#include "pla_pkg.h"

#define TUS_FILE_EXT L".pla"

pla_pkg_t * pla_pkg_create(ul_hs_t * name) {
	pla_pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);
	
	*pkg = (pla_pkg_t){ .name = name };

	return pkg;
}
void pla_pkg_destroy(pla_pkg_t * pkg) {
	if (pkg == NULL) {
		return;
	}

	pla_pkg_destroy_chain(pkg->sub_pkg);

	pla_tus_destroy_chain(pkg->tus);

	free(pkg);
}
void pla_pkg_destroy_chain(pla_pkg_t * pkg) {
	while (pkg != NULL) {
		pla_pkg_t * next = pkg->next;

		pla_pkg_destroy(pkg);

		pkg = next;
	}
}

static pla_pkg_t ** find_sub_pkg_ins(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	pla_pkg_t ** sub_pkg_ins = &pkg->sub_pkg;

	while (*sub_pkg_ins != NULL) {
		pla_pkg_t * sub_pkg = *sub_pkg_ins;

		if (sub_pkg->name == sub_pkg_name) {
			break;
		}

		sub_pkg_ins = &sub_pkg->next;
	}

	return sub_pkg_ins;
}
pla_pkg_t * pla_pkg_find_sub_pkg(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	return *find_sub_pkg_ins(pkg, sub_pkg_name);
}
static pla_tus_t ** find_tus_ins(pla_pkg_t * pkg, ul_hs_t * tus_name) {
	pla_tus_t ** tus_ins = &pkg->tus;

	while (*tus_ins != NULL) {
		pla_tus_t * tus = *tus_ins;

		if (tus->name == tus_name) {
			break;
		}

		tus_ins = &tus->next;
	}

	return tus_ins;
}
pla_tus_t * pla_pkg_find_tus(pla_pkg_t * pkg, ul_hs_t * tus_name) {
	return *find_tus_ins(pkg, tus_name);
}

pla_pkg_t * pla_pkg_get_sub_pkg(pla_pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	pla_pkg_t ** sub_pkg_ins = find_sub_pkg_ins(pkg, sub_pkg_name);

	if (*sub_pkg_ins != NULL) {
		ul_assert((*sub_pkg_ins)->name == sub_pkg_name);

		return *sub_pkg_ins;
	}

	*sub_pkg_ins = pla_pkg_create(sub_pkg_name);

	return *sub_pkg_ins;
}
pla_tus_t * pla_pkg_get_tus(pla_pkg_t * pkg, ul_hs_t * tus_name) {
	pla_tus_t ** tus_ins = find_tus_ins(pkg, tus_name);

	if (*tus_ins != NULL) {
		ul_assert((*tus_ins)->name == tus_name);

		return *tus_ins;
	}

	*tus_ins = pla_tus_create(tus_name);

	return *tus_ins;
}

static void get_file_name_and_ext(const wchar_t * path, const wchar_t ** file_name_out, const wchar_t ** file_ext_out) {
	const wchar_t * cur = path;
	const wchar_t * file_name = path, * file_ext = NULL;

	for (; *cur != 0; ++cur) {
		switch (*cur) {
			case '/':
				file_name = cur + 1;
				file_ext = NULL;
				break;
			case '.':
				file_ext = cur;
				break;
		}
	}

	if (file_ext == NULL) {
		file_ext = cur;
	}

	if (file_name_out != NULL) {
		*file_name_out = file_name;
	}
	if (file_ext_out != NULL) {
		*file_ext_out = file_ext;
	}
}
static ul_hs_t * name_from_path(ul_hst_t * hst, const wchar_t * path) {
	const wchar_t * path_file_name, * path_ext;
	get_file_name_and_ext(path, &path_file_name, &path_ext);

	return ul_hst_hashadd(hst, path_ext - path_file_name, path_file_name);
}
static bool process_path_pkg(pla_pkg_t * pkg, ul_hst_t * hst, const wchar_t * path) {
	ul_hs_t * name = name_from_path(hst, path);

	pla_pkg_t * sub_pkg = pla_pkg_get_sub_pkg(pkg, name);

	if (!pla_pkg_fill_from_dir(sub_pkg, hst, path)) {
		return false;
	}

	return true;
}
static bool read_tus_file(pla_tus_t * tus, const wchar_t * path) {
	EnterCriticalSection(&tus->lock);
	
	bool res = pla_tus_read_file_nl(tus, path);

	LeaveCriticalSection(&tus->lock);

	return res;
}
static bool process_path_tus(pla_pkg_t * pkg, ul_hst_t * hst, const wchar_t * path) {
	ul_hs_t * name = name_from_path(hst, path);

	pla_tus_t * tus = pla_pkg_get_tus(pkg, name);

	if (!read_tus_file(tus, path)) {
		return false;
	}

	return true;
}
static bool process_path(pla_pkg_t * pkg, ul_hst_t * hst, const wchar_t * path, ul_fs_ent_type_t path_type) {
	const wchar_t * path_ext;
	get_file_name_and_ext(path, NULL, &path_ext);

	switch (path_type) {
		case UlFsEntFile:
			if (wcscmp(path_ext, TUS_FILE_EXT) == 0
				&& !process_path_tus(pkg, hst, path)) {
				return false;
			}
			break;
		case UlFsEntDir:
			if (!process_path_pkg(pkg, hst, path)) {
				return false;
			}
			break;
		default:
			return false;
	}

	return true;
}
bool pla_pkg_fill_from_dir(pla_pkg_t * pkg, ul_hst_t * hst, const wchar_t * dir_path) {
	size_t dir_path_size = wcslen(dir_path);
	
	ul_fs_dir_t * dir = ul_fs_dir_read(dir_path);

	bool success = true;

	size_t path_buf_size = 0;
	wchar_t * path_buf = NULL;

	for (ul_fs_ent_t * ent = dir->ents, *ent_end = ent + dir->ents_size; ent != ent_end; ++ent) {
		if (wcscmp(ent->name, L".") == 0
			|| wcscmp(ent->name, L"..") == 0) {
			continue;
		}

		size_t name_size = wcslen(ent->name);

		size_t path_size = dir_path_size + 1 + name_size + 1;
		if (path_size > path_buf_size) {
			ul_arr_grow(path_size, &path_buf_size, &path_buf, sizeof(*path_buf));
		}

		{
			int res = swprintf_s(path_buf, path_buf_size, L"%s/%s", dir_path, ent->name);

			ul_assert(res > 0);
		}

		const wchar_t * name_ext = wcsrchr(ent->name, '.');
		if (name_ext == NULL) {
			name_ext = ent->name + name_size;
		}

		if (!process_path(pkg, hst, path_buf, ent->type)) {
			success = false;
			break;
		}
	}

	free(path_buf);
	ul_fs_dir_free(dir);

	return success;
}

bool pla_pkg_fill_from_list(pla_pkg_t * pkg, ul_hst_t * hst, ...) {
	va_list args;

	va_start(args, hst);

	bool success = true;

	for (const wchar_t * elem = va_arg(args, const wchar_t *); elem != NULL; elem = va_arg(args, const wchar_t *)) {
		ul_fs_ent_type_t elem_type = ul_fs_get_ent_type(elem);

		if (!process_path(pkg, hst, elem, elem_type)) {
			success = false;
			break;
		}
	}

	va_end(args);

	return success;
}
