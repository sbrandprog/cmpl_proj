#pragma once
#include "ul.h"

#ifdef _WIN32
#define ul_assert_debug_break() __debugbreak()
#else
#define ul_assert_debug_break() ((void)0)
#endif

UL_API __declspec(noreturn) void ul_assert_func(const char * expr_str, const char * func_name);

#define ul_assert(expr)                      \
    do                                       \
    {                                        \
        if (!(expr))                         \
        {                                    \
            ul_assert_debug_break();         \
            ul_assert_func(#expr, __func__); \
        }                                    \
    } while (false)
#define ul_assert_unreachable() ul_assert(false && "unreachable")
