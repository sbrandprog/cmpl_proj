#pragma once
#include "ira_int.h"

enum ira_val_type {
	IraValImmVoid,
	IraValImmDt,
	IraValImmBool,
	IraValImmInt,
	IraValImmVec,
	IraValNullPtr,
	IraValLoPtr,
	IraValImmStct,
	IraValImmArr,
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
			ira_val_t ** data;
		} vec;
		struct {
			size_t size;
			ira_val_t ** data;
		} arr;
		struct {
			size_t size;
			ira_val_t ** elems;
		} stct;
	};
};

ira_val_t * ira_val_create(ira_val_type_t type, ira_dt_t * dt);
void ira_val_destroy(ira_val_t * val);

ira_val_t * ira_val_copy(ira_val_t * val);
