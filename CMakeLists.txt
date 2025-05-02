cmake_minimum_required(VERSION 3.24)

enable_testing()

project(comp_proj)

include(cmake/proj_helper.cmake)

add_subdirectory(cmpl_lib)
add_subdirectory(main_dev)
add_subdirectory(ul_lib)
