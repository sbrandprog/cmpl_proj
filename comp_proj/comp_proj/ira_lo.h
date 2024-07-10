#pragma once
#include "ira_dt.h"

struct ira_lo_nspc_node {
	ul_hs_t * name;
	ira_lo_nspc_node_t * next;
	ira_lo_t * lo;
};

enum ira_lo_type {
	IraLoNone,
	IraLoNspc,
	IraLoFunc,
	IraLoImpt,
	IraLoVar,
	IraLo_Count
};
struct ira_lo {
	ira_lo_type_t type;

	ira_lo_t * next;

	ul_hs_t * name;

	union {
		struct {
			ira_lo_nspc_node_t * body;
		} nspc;
		ira_func_t * func;
		struct {
			ira_dt_t * dt;
			ul_hs_t * lib_name;
			ul_hs_t * sym_name;
		} impt;
		struct {
			ira_dt_qdt_t qdt;
			ira_val_t * val;
		} var;
	};
};


ira_lo_nspc_node_t * ira_lo_create_nspc_node(ul_hs_t * name);
void ira_lo_destroy_nspc_node(ira_lo_nspc_node_t * node);
void ira_lo_destroy_nspc_node_chain(ira_lo_nspc_node_t * node);
