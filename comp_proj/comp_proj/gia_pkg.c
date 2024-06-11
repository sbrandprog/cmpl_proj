#include "pch.h"
#include "gia_tus.h"
#include "gia_pkg.h"

#define TUS_FILE_EXT L".pla"

gia_pkg_t * gia_pkg_create(ul_hs_t * name) {
	gia_pkg_t * pkg = malloc(sizeof(*pkg));

	ul_assert(pkg != NULL);
	
	*pkg = (gia_pkg_t){ .name = name };

	return pkg;
}
void gia_pkg_destroy(gia_pkg_t * pkg) {
	if (pkg == NULL) {
		return;
	}

	gia_pkg_destroy_chain(pkg->sub_pkg);

	gia_tus_destroy_chain(pkg->tus);

	free(pkg);
}
void gia_pkg_destroy_chain(gia_pkg_t * pkg) {
	while (pkg != NULL) {
		gia_pkg_t * next = pkg->next;

		gia_pkg_destroy(pkg);

		pkg = next;
	}
}

gia_pkg_t * gia_pkg_get_sub_pkg(gia_pkg_t * pkg, ul_hs_t * sub_pkg_name) {
	gia_pkg_t ** sub_pkg_ins = &pkg->sub_pkg;

	while (*sub_pkg_ins != NULL) {
		gia_pkg_t * sub_pkg = *sub_pkg_ins;

		if (sub_pkg->name == sub_pkg_name) {
			return sub_pkg;
		}

		sub_pkg_ins = &sub_pkg->next;
	}

	*sub_pkg_ins = gia_pkg_create(sub_pkg_name);

	return *sub_pkg_ins;
}
gia_tus_t * gia_pkg_get_tus(gia_pkg_t * pkg, ul_hs_t * tus_name) {
	gia_tus_t ** tus_ins = &pkg->tus;

	while (*tus_ins != NULL) {
		gia_tus_t * tus = *tus_ins;

		if (tus->name == tus_name) {
			return tus;
		}

		tus_ins = &tus->next;
	}

	*tus_ins = gia_tus_create(tus_name);

	return *tus_ins;
}

static ul_hs_t * name_from_path(ul_hst_t * hst, const wchar_t * path) {
	const wchar_t * path_file_name = PathFindFileNameW(path);
	const wchar_t * path_ext = PathFindExtensionW(path);

	return ul_hst_hashadd(hst, path_ext - path_file_name, path_file_name);
}
static bool process_path_pkg(gia_pkg_t * pkg, ul_hst_t * hst, const wchar_t * path) {
	ul_hs_t * name = name_from_path(hst, path);

	gia_pkg_t * sub_pkg = gia_pkg_get_sub_pkg(pkg, name);

	if (!gia_pkg_fill_from_dir(sub_pkg, hst, path)) {
		return false;
	}

	return true;
}
static bool read_tus_file(gia_tus_t * tus, const wchar_t * path) {
	EnterCriticalSection(&tus->lock);
	
	bool res = false;
	
	__try {
		res = gia_tus_read_file_nl(tus, path);
	}
	__finally {
		LeaveCriticalSection(&tus->lock);
	}

	return res;
}
static bool process_path_tus(gia_pkg_t * pkg, ul_hst_t * hst, const wchar_t * path) {
	ul_hs_t * name = name_from_path(hst, path);

	gia_tus_t * tus = gia_pkg_get_tus(pkg, name);

	if (!read_tus_file(tus, path)) {
		return false;
	}

	return true;
}
bool gia_pkg_fill_from_dir(gia_pkg_t * pkg, ul_hst_t * hst, const wchar_t * dir_path) {
	wchar_t buf[MAX_PATH];

	swprintf_s(buf, _countof(buf), L"%s/*", dir_path);

	WIN32_FIND_DATAW file_data;

	HANDLE srch_h = FindFirstFileExW(buf, FindExInfoBasic, &file_data, FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);

	if (srch_h == INVALID_HANDLE_VALUE) {
		return false;
	}

	bool success = true;

	do {
		if (wcscmp(file_data.cFileName, L".") == 0
			|| wcscmp(file_data.cFileName, L"..") == 0) {
			continue;
		}

		const wchar_t * path_ext = PathFindExtensionW(file_data.cFileName);

		swprintf_s(buf, _countof(buf), L"%s/%s", dir_path, file_data.cFileName);

		if (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!process_path_pkg(pkg, hst, buf)) {
				success = false;
				break;
			}
		}
		else if (wcscmp(path_ext, TUS_FILE_EXT) == 0) {
			if (!process_path_tus(pkg, hst, buf)) {
				success = false;
				break;
			}
		}
	} while (FindNextFileW(srch_h, &file_data) != 0);

	FindClose(srch_h);

	return success;
}

bool gia_pkg_fill_from_list(gia_pkg_t * pkg, ul_hst_t * hst, ...) {
	va_list args;

	va_start(args, hst);

	bool success = true;

	for (const wchar_t * elem = va_arg(args, const wchar_t *); elem != NULL; elem = va_arg(args, const wchar_t *)) {
		if (!PathFileExistsW(elem)) {
			success = false;
			break;
		}
		
		ul_hs_t * name = name_from_path(hst, elem);
		
		if (PathIsDirectoryW(elem)) {
			if (!process_path_pkg(pkg, hst, elem)) {
				success = false;
				break;
			}
		}
		else {
			if (!process_path_tus(pkg, hst, elem)) {
				success = false;
				break;
			}
		}
	}

	va_end(args);

	return success;
}
