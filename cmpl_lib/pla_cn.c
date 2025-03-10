#include "pla_cn.h"

pla_cn_t * pla_cn_create()
{
    pla_cn_t * cn = malloc(sizeof(*cn));

    ul_assert(cn != NULL);

    *cn = (pla_cn_t){ 0 };

    return cn;
}
void pla_cn_destroy(pla_cn_t * cn)
{
    if (cn == NULL)
    {
        return;
    }

    pla_cn_destroy(cn->sub_name);

    free(cn);
}
