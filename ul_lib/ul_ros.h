#pragma once
#include "ul.h"

struct ul_ros
{
    size_t size;
    const char * str;
};

#define UL_ROS_MAKE(str_) { .size = _countof(str_) - 1, .str = str_ }
