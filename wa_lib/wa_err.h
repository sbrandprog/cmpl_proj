#pragma once
#include "wa.h"

WA_API void wa_err_check_fatal_func(const wchar_t * func_name);
WA_API void wa_err_check_func(const wchar_t * func_name);

#define wa_err_check_fatal() wa_err_check_fatal_func(__FUNCTIONW__)
#define wa_err_check() wa_err_check_func(__FUNCTIONW__)
