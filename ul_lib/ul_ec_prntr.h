#pragma once
#include "ul_ec.h"

struct ul_ec_prntr {
	void * user_data;
	ul_ec_prntr_print_proc_t * print_proc;
};

UL_API void ul_ec_prntr_init(ul_ec_prntr_t * prntr, void * user_data, ul_ec_prntr_print_proc_t * print_proc);
UL_API void ul_ec_prntr_cleanup(ul_ec_prntr_t * prntr);

UL_API void ul_ec_prntr_init_dflt(ul_ec_prntr_t * prntr);

inline bool ul_ec_prntr_print(ul_ec_prntr_t * prntr, ul_ec_rec_t * rec) {
	return prntr->print_proc(prntr->user_data, rec);
}
