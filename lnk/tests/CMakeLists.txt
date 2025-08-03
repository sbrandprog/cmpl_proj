
set_dir_deps_trg(COPY_DLLS cmpl_lnk cmpl_ul)

add_proj_test_exe(cmpl_lnk lnk_zic_test LIBRARIES cmpl_lnk)

add_proj_test_exe(cmpl_lnk lnk_to_exe1 LIBRARIES cmpl_lnk)
add_proj_test_exe(cmpl_lnk lnk_to_exe2 LIBRARIES cmpl_lnk)
add_proj_test_exe(cmpl_lnk lnk_to_exe3 LIBRARIES cmpl_lnk)

add_proj_test_exe(cmpl_lnk lnk_pel_fail1 LIBRARIES cmpl_lnk)
add_proj_test_exe(cmpl_lnk lnk_pel_fail2 LIBRARIES cmpl_lnk)
