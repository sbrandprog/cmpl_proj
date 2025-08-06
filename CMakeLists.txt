cmake_minimum_required(VERSION 3.24)

enable_testing()

project(cmpl_proj)

include(cmake/proj_helper.cmake)

add_subdirectory(ul)
add_subdirectory(lnk)
add_subdirectory(mc)
add_subdirectory(ira)
