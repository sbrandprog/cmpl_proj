
set_dir_deps_trg(COPY_DLLS cmpl_mc cmpl_lnk cmpl_ul)

add_proj_test_exe(cmpl_mc mc_to_exe1 LIBRARIES cmpl_mc)
add_proj_test_exe(cmpl_mc mc_to_exe2 LIBRARIES cmpl_mc)

add_proj_test_exe(cmpl_mc mc_pea_fail1 LIBRARIES cmpl_mc)

