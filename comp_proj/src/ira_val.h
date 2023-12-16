#pragma once
#include "ira_int.h"

enum ira_val_type {
	IraValNone,
	IraValImmDt,
	IraValImmBool,
	IraValImmInt,
	IraValImmArr,
	IraValLoPtr,
	IraValNullPtr,
	IraVal_Count
};
struct ira_val {
	ira_val_type_t type;
	ira_dt_t * dt;
	union {
		ira_dt_t * dt_val;
		bool bool_val;
		ira_int_t int_val;
		ira_lo_t * lo_val;
		struct {
			size_t size;
			ira_val_t ** data;
		} arr;
	};
};

ira_val_t * ira_val_create(ira_val_type_t type, ira_dt_t * dt);
void ira_val_destroy(ira_val_t * val);

ira_val_t * ira_val_copy(ira_val_t * val);
