#pragma once
#include "ul.h"

#ifdef _WIN32
#define ul_assert_debug_break() __debugbreak()
#define ul_assert_noreturn __declspec(noreturn)
#else
#define ul_assert_debug_break() ((void)0)
#define ul_assert_noreturn _Noreturn
#endif

UL_API ul_assert_noreturn void ul_assert_func(const char * expr_str, const char * func_name);

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
