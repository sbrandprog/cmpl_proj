#pragma once

#if defined _WIN32
#if defined CMPL_BUILD_DLL
#define CMPL_API __declspec(dllexport)
#else
#define CMPL_API __declspec(dllimport)
#endif
#else
#define CMPL_API
#endif
