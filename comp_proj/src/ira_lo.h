#pragma once
#include "ira_dt.h"
#include "u_hs.h"

enum ira_lo_type {
	IraLoNone,
	IraLoNspc,
	IraLoFunc,
	IraLoImpt,
	IraLoVar,
	IraLoDtStct,
	IraLoRoVal,
	IraLo_Count
};
struct ira_lo {
	ira_lo_type_t type;

	ira_lo_t * next;

	u_hs_t * name;
	u_hs_t * full_name;

	union {
		struct {
			ira_lo_t * body;
		} nspc;
		ira_func_t * func;
		struct {
			ira_dt_t * dt;
			u_hs_t * lib_name;
			u_hs_t * sym_name;
		} impt;
		struct {
			ira_dt_qdt_t qdt;
			ira_val_t * val;
		} var;
		struct {
			ira_dt_sd_t * sd;
		} dt_stct;
		struct {
			ira_val_t * val;
		} ro_val;
	};
};

ira_lo_t * ira_lo_create(ira_lo_type_t type, u_hs_t * name);
void ira_lo_destroy(ira_lo_t * ira);

void ira_lo_destroy_chain(ira_lo_t * ira);
