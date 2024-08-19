#pragma once
#include "ul.h"

UL_API __declspec(noreturn) void ul_assert_func(const wchar_t * expr_str, const wchar_t * func_name);

#define ul_assert(expr) do { if (!(expr)) { __debugbreak(); ul_assert_func(L ## #expr, __FUNCTIONW__); } } while(false)
#define ul_assert_unreachable() ul_assert(false && L"unreachable")
