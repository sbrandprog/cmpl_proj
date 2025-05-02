#include "cmpl_lib/cmpl_lib.h"

#define FILE_NAME_BUF_SIZE 64

#define EXE_NAME "test.exe"
#define EXE_CMD EXE_NAME

static const char * file_name = NULL;

static ul_hst_t hst;
static pla_repo_t repo;

static int main_core(int argc, char * argv[])
{
    if (argc != 2)
    {
        printf("expected 1 file name argument\n");
        return -1;
    }

    ul_hst_init(&hst);

    ul_hs_t * file_name = ul_hst_hashadd(&hst, strlen(argv[1]), argv[1]);

    if (file_name == NULL)
    {
        printf("failed to convert file name to hashed string\n");
        return -1;
    }

    pla_repo_init(&repo, &hst);

    repo.root = pla_pkg_create(NULL);

    if (!pla_pkg_fill_from_list(repo.root, &hst, "pla_lib", file_name->str, NULL))
    {
        printf("failed to fill repo\n");
        return -1;
    }

    {
        ul_hs_t * first_tus_name = pla_pkg_get_tus_name_from_path(&hst, file_name->str);
        pla_bs_src_t bs_src;

        pla_bs_src_init(&bs_src, &repo, first_tus_name);
        bs_src.lnk_sett.file_name = EXE_NAME;

        bool res = pla_bs_build_nl(&bs_src);

        pla_bs_src_cleanup(&bs_src);

        if (!res)
        {
            printf("failed to build executable\n");
			
            return -1;
        }
    }

    {
        const char * const args[] = { EXE_NAME, NULL };

        int res = ul_spawn_wait(EXE_NAME, args);

        if (res != 0)
        {
            printf("unexpected executable result (codes: %d 0x%X)\n", res, res);
            return -1;
        }
    }

    return 0;
}
int main(int argc, char * argv[])
{
    int res = main_core(argc, argv);

    pla_repo_cleanup(&repo);

    ul_hst_cleanup(&hst);

    return res;
}
