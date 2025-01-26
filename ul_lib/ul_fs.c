#include "ul_fs.h"
#include "ul_arr.h"

static ul_fs_dir_t * create_dir()
{
    ul_fs_dir_t * dir = malloc(sizeof(*dir));

    ul_assert(dir != NULL);

    *dir = (ul_fs_dir_t){ 0 };

    return dir;
}
static void destroy_dir(ul_fs_dir_t * dir)
{
    if (dir == NULL)
    {
        return;
    }

    for (ul_fs_ent_t *ent = dir->ents, *ent_end = ent + dir->ents_size; ent != ent_end; ++ent)
    {
        free(ent->name);
    }

    free(dir->ents);

    free(dir);
}

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#define PATH_EXTENDER L"\\\\?\\"
#define PATH_EXTENDER_SIZE (_countof(PATH_EXTENDER) - 1)
#define SEARCH_SUFFIX L"\\*"
#define SEARCH_SUFFIX_SIZE (_countof(SEARCH_SUFFIX) - 1)

static ul_fs_ent_type_t get_ent_type_from_attrs(DWORD attrs)
{
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return UlFsEntNone;
    }
    else if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    {
        return UlFsEntDir;
    }
    else
    {
        return UlFsEntFile;
    }
}

static wchar_t * convert_to_wc(size_t ch_str_size, const char * ch_str)
{
    wchar_t * wc_str = NULL;

    int res0 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ch_str, (int)ch_str_size, NULL, 0);

    if (res0 != 0)
    {
        size_t wc_str_size = (size_t)res0 + 1;

        wc_str = malloc(sizeof(*wc_str) * wc_str_size);

        ul_assert(wc_str != NULL);

        int res1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ch_str, (int)ch_str_size, wc_str, (int)wc_str_size);

        if (res1 == 0 || res1 > res0)
        {
            free(wc_str);
            wc_str = NULL;
        }
        else
        {
            wc_str[res1] = 0;
        }
    }

    return wc_str;
}
static char * convert_to_ch(size_t wc_str_size, const wchar_t * wc_str)
{
    char * ch_str = NULL;

    int res0 = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wc_str, (int)wc_str_size, NULL, 0, NULL, NULL);

    if (res0 != 0)
    {
        size_t ch_str_size = (size_t)res0 + 1;

        ch_str = malloc(sizeof(*ch_str) * ch_str_size);

        ul_assert(ch_str != NULL);

        int res1 = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wc_str, (int)wc_str_size, ch_str, (int)ch_str_size, NULL, NULL);

        if (res1 == 0 || res1 > res0)
        {
            free(ch_str);
            ch_str = NULL;
        }
        else
        {
            ch_str[res1] = 0;
        }
    }

    return ch_str;
}

static wchar_t * get_full_path_wc(const wchar_t * path)
{
    wchar_t * full_path = NULL;

    DWORD res0 = GetFullPathNameW(path, 0, NULL, NULL);

    if (res0 != 0)
    {
        size_t full_path_size = PATH_EXTENDER_SIZE + (size_t)res0 + 1;

        full_path = malloc(sizeof(*full_path) * full_path_size);

        ul_assert(full_path != NULL);

        DWORD res1 = GetFullPathNameW(path, (DWORD)(full_path_size - PATH_EXTENDER_SIZE), full_path + PATH_EXTENDER_SIZE, NULL);
	
        if (res1 == 0 || res1 > res0)
        {
            free(full_path);
            full_path = NULL;
        }
        else
        {
            wmemcpy(full_path, PATH_EXTENDER, PATH_EXTENDER_SIZE);
            full_path[PATH_EXTENDER_SIZE + (size_t)res1] = 0;
        }
    }

    return full_path;
}
static wchar_t * get_full_path_ch(const char * path)
{
    wchar_t * full_path = NULL;
    wchar_t * path_wc = convert_to_wc(strlen(path), path);

    if (path_wc != NULL)
    {
        full_path = get_full_path_wc(path_wc);

        free(path_wc);
        path_wc = NULL;
    }

    return full_path;
}

static wchar_t * get_search_path(const char * path)
{
    wchar_t * search_path = NULL;
    wchar_t * full_path = get_full_path_ch(path);

    if (full_path != NULL)
    {
        size_t full_path_size = wcslen(full_path);
        size_t search_path_size = full_path_size + SEARCH_SUFFIX_SIZE + 1;

        search_path = malloc(sizeof(*search_path) * search_path_size);

        ul_assert(search_path != NULL);

        wmemcpy(search_path, full_path, full_path_size);
        wmemcpy(search_path + full_path_size, SEARCH_SUFFIX, SEARCH_SUFFIX_SIZE);
        search_path[full_path_size + SEARCH_SUFFIX_SIZE] = 0;

        free(full_path);
        full_path = NULL;
    }

    return search_path;
}

ul_fs_ent_type_t ul_fs_get_ent_type(const char * entry_path)
{
    wchar_t * entry_path_full = get_full_path_ch(entry_path);

    DWORD attrs = GetFileAttributesW(entry_path_full);

    free(entry_path_full);

    return get_ent_type_from_attrs(attrs);
}

ul_fs_dir_t * ul_fs_dir_read(const char * dir_path)
{
    WIN32_FIND_DATAW file_data;
    ul_fs_dir_t * dir = NULL;
    HANDLE srch_h;

    {
        wchar_t * dir_path_search = get_search_path(dir_path);

        srch_h = FindFirstFileExW(dir_path_search, FindExInfoBasic, &file_data, FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);

        free(dir_path_search);
    }

    if (srch_h != INVALID_HANDLE_VALUE)
    {
        dir = create_dir();

        bool success = true;

        do
        {
            char * ent_name = convert_to_ch(wcsnlen(file_data.cFileName, _countof(file_data.cFileName)), file_data.cFileName);

            if (ent_name == NULL)
            {
                success = false;
                break;
            }

            if (dir->ents_size + 1 > dir->ents_cap)
            {
                ul_arr_grow(dir->ents_size + 1, &dir->ents_cap, &dir->ents, sizeof(*dir->ents));
            }

            ul_fs_ent_t * ent = &dir->ents[dir->ents_size++];

            *ent = (ul_fs_ent_t){ .type = get_ent_type_from_attrs(file_data.dwFileAttributes), .name = ent_name };
        } while (FindNextFileW(srch_h, &file_data) != 0);

        FindClose(srch_h);
        srch_h = INVALID_HANDLE_VALUE;

        if (!success)
        {
            destroy_dir(dir);
            dir = NULL;
        }
    }

    return dir;
}
void ul_fs_dir_free(ul_fs_dir_t * dir)
{
    destroy_dir(dir);
}

#endif
