#include "ira/ira_lib.h"

static ira_inst_t ira_inst;
static ira_pec_t pec;

int main()
{
    ira_inst_cleanup(&ira_inst);

    ira_pec_cleanup(&pec);

    return 0;
}
