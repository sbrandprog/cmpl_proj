#include "pla_tus.h"

#define READ_BUF_SIZE 256

pla_tus_t * pla_tus_create(ul_hs_t * name)
{
    pla_tus_t * tus = malloc(sizeof(*tus));

    ul_assert(tus != NULL);

    *tus = (pla_tus_t){ .name = name };

    return tus;
}
void pla_tus_destroy(pla_tus_t * tus)
{
    if (tus == NULL)
    {
        return;
    }

    free(tus->src);

    free(tus);
}
void pla_tus_destroy_chain(pla_tus_t * tus)
{
    while (tus != NULL)
    {
        pla_tus_t * next = tus->next;

        pla_tus_destroy(tus);

        tus = next;
    }
}

void pla_tus_insert_str(pla_tus_t * tus, size_t ins_pos, size_t str_size, char * str)
{
    ul_assert(ins_pos <= tus->src_size);

    ul_arr_grow(tus->src_size + str_size, &tus->src_cap, (void **)&tus->src, sizeof(*tus->src));

    memmove(tus->src + ins_pos + str_size, tus->src + ins_pos, tus->src_size - ins_pos);
    memcpy(tus->src + ins_pos, str, str_size);
    tus->src_size += str_size;
}

bool pla_tus_read_file(pla_tus_t * tus, const char * file_name)
{
    FILE * file = fopen(file_name, "r");

    if (file == NULL)
    {
        return false;
    }

    tus->src_size = 0;

    char buf[READ_BUF_SIZE];

    while (fgets(buf, ul_arr_count(buf), file) != NULL)
    {
        pla_tus_insert_str(tus, tus->src_size, strlen(buf), buf);
    }

    fclose(file);

    return true;
}
