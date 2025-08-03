#pragma once
#include "ul_arr.h"

struct ul_ros
{
    size_t size;
    const char * str;
};

#define UL_ROS_MAKE(str_) { .size = ul_arr_count(str_) - 1, .str = str_ }
