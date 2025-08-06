
set_dir_deps_trg(COPY_DLLS cmpl_ira cmpl_mc cmpl_lnk cmpl_ul)

add_proj_test_exe(cmpl_ira ira_zic_test LIBRARIES cmpl_ira)

add_proj_test_exe(cmpl_ira ira_to_exe1 LIBRARIES cmpl_ira)

