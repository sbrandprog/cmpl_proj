#pragma once

_Noreturn void u_assert_func(const wchar_t * func_name, const wchar_t * expr_str);

#define u_assert(expr) (void)((expr) || (u_assert_func(__FUNCTIONW__, L ## #expr), 1))
#define u_assert_switch(switch_expr) u_assert(((switch_expr), 0))