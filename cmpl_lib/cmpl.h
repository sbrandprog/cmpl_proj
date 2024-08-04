#pragma once

#if defined CMPL_BUILD_DLL
#define CMPL_API __declspec(dllexport)
#else
#define CMPL_API __declspec(dllimport)
#endif
