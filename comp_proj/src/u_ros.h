#pragma once

#define U_MAKE_ROS(str_) { .size = _countof(str_) - 1, .str = str_ }

typedef struct u_ros {
	size_t size;
	const wchar_t * str;
} u_ros_t;
