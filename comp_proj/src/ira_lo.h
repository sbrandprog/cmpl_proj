#pragma once
#include "ira.h"
#include "u_hs.h"

enum ira_lo_type {
	IraLoNone,
	IraLoNspc,
	IraLoFunc,
	IraLoImpt,
	IraLo_Count
};
struct ira_lo {
	ira_lo_type_t type;

	ira_lo_t * next;
	ira_lo_t * body;

	u_hs_t * name;
	u_hs_t * full_name;

	union {
		ira_func_t * func;
		struct {
			ira_dt_t * dt;
			u_hs_t * lib_name;
			u_hs_t * sym_name;
		} impt;
	};
};

ira_lo_t * ira_lo_create(ira_lo_type_t type, u_hs_t * name);
void ira_lo_destroy(ira_lo_t * ira);

void ira_lo_destroy_chain(ira_lo_t * ira);
