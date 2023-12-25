#pragma once
#include "u_hst.h"

typedef struct u_hsb {
	wchar_t * buf;
	size_t cap;
} u_hsb_t;

void u_hsb_cleanup(u_hsb_t * hsb);

size_t u_hsb_format(u_hsb_t * hsb, const wchar_t * fmt, ...);
u_hs_t * u_hsb_formatadd(u_hsb_t * hsb, u_hst_t * hst, const wchar_t * fmt, ...);
