#include "ul_ec_prntr.h"
#include "ul_arr.h"

void ul_ec_prntr_init(ul_ec_prntr_t * prntr, void * user_data, ul_ec_prntr_print_proc_t * print_proc)
{
    *prntr = (ul_ec_prntr_t){ .user_data = user_data, .print_proc = print_proc };
}
void ul_ec_prntr_cleanup(ul_ec_prntr_t * prntr)
{
    memset(prntr, 0, sizeof(*prntr));
}

static bool print_dflt_proc(void * user_data, ul_ec_rec_t * rec)
{
    if (strcmp(rec->type, UL_EC_REC_TYPE_DFLT) != 0)
    {
        return false;
    }

    ul_ec_rec_dump(rec);

    return true;
}
void ul_ec_prntr_init_dflt(ul_ec_prntr_t * prntr)
{
    ul_ec_prntr_init(prntr, NULL, print_dflt_proc);
}

#if defined __linux__
extern inline bool ul_ec_prntr_print(ul_ec_prntr_t * prntr, ul_ec_rec_t * rec);
#endif
