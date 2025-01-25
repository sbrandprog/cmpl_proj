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

ul_fs_ent_type_t ul_fs_get_ent_type(const wchar_t * entry_path)
{
    return get_ent_type_from_attrs(GetFileAttributesW(entry_path));
}

ul_fs_dir_t * ul_fs_dir_read(const wchar_t * dir_path)
{
    wchar_t buf[MAX_PATH];

    int res = swprintf_s(buf, _countof(buf), L"%s/*", dir_path);

    if (res <= 0)
    {
        return NULL;
    }

    WIN32_FIND_DATAW file_data;

    HANDLE srch_h = FindFirstFileExW(buf, FindExInfoBasic, &file_data, FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);

    if (srch_h == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    ul_fs_dir_t * dir = create_dir();

    do
    {
        if (dir->ents_size + 1 > dir->ents_cap)
        {
            ul_arr_grow(dir->ents_size + 1, &dir->ents_cap, &dir->ents, sizeof(*dir->ents));
        }

        ul_fs_ent_t * ent = &dir->ents[dir->ents_size++];

        *ent = (ul_fs_ent_t){ .type = get_ent_type_from_attrs(file_data.dwFileAttributes) };

        size_t name_size = wcslen(file_data.cFileName);

        ent->name = malloc(sizeof(*ent->name) * (name_size + 1));

        ul_assert(ent->name != NULL);

        wcscpy_s(ent->name, name_size + 1, file_data.cFileName);
    } while (FindNextFileW(srch_h, &file_data) != 0);

    FindClose(srch_h);

    return dir;
}
void ul_fs_dir_free(ul_fs_dir_t * dir)
{
    destroy_dir(dir);
}

#endif
