static const char * exe_name = "test.exe";
static const char * main_tus_file_name = "test.pla";
static ul_ros_t main_tus_name = UL_ROS_MAKE("test");

static ul_hst_t main_hst;
static pla_repo_t main_repo;

static bool main_fill_repo()
{
    pla_repo_init(&main_repo, &main_hst);

    main_repo.root = pla_pkg_create(NULL);

    if (!pla_pkg_fill_from_list(main_repo.root, main_repo.hst, "pla_lib", main_tus_file_name, NULL))
    {
        return false;
    }

    return true;
}

static int main_core()
{
    ul_hst_init(&main_hst);

    if (!main_fill_repo())
    {
        return -1;
    }

    pla_bs_src_t bs_src = { .repo = &main_repo, .first_tus_name = ul_hst_hashadd(&main_hst, main_tus_name.size, main_tus_name.str), .lnk_sett = lnk_pel_dflt_sett };

    bs_src.lnk_sett.export_pd = true;
    bs_src.lnk_sett.file_name = exe_name;

    if (!pla_bs_build_nl(&bs_src))
    {
        return -1;
    }

    printf("success\n");

    return 0;
}

int main()
{
    int res = main_core();

    pla_repo_cleanup(&main_repo);

    ul_hst_cleanup(&main_hst);

    return res;
}
