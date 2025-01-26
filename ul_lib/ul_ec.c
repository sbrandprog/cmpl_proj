#include "ul_ec.h"
#include "ul_assert.h"

ul_ec_rec_t * ul_ec_rec_create(const char * type)
{
    ul_ec_rec_t * rec = malloc(sizeof(*rec));

    ul_assert(rec != NULL);

    *rec = (ul_ec_rec_t){ .type = type };

    return rec;
}
ul_ec_rec_t * ul_ec_rec_copy(const ul_ec_rec_t * rec)
{
    ul_ec_rec_t * new_rec = ul_ec_rec_create(rec->type);

    new_rec->mod_name = rec->mod_name;

    memcpy(new_rec->strs, rec->strs, sizeof(*new_rec->strs) * _countof(new_rec->strs));
    new_rec->strs_size = rec->strs_size;

    return new_rec;
}
void ul_ec_rec_destroy(ul_ec_rec_t * rec)
{
    if (rec == NULL)
    {
        return;
    }

    free(rec);
}
void ul_ec_rec_destroy_chain(ul_ec_rec_t * rec)
{
    while (rec != NULL)
    {
        ul_ec_rec_t * next = rec->next;

        ul_ec_rec_destroy(rec);

        rec = next;
    }
}

void ul_ec_rec_dump(ul_ec_rec_t * rec)
{
    printf("error record [%s] from [%s]:\n", rec->type, rec->mod_name);

    char *cur = rec->strs, *cur_end = rec->strs + _countof(rec->strs);
    for (size_t str = 0; str < rec->strs_size; ++str)
    {
        printf("%s\n", cur);

        if (str + 1 < rec->strs_size)
        {
            cur = memchr(cur, 0, cur_end - cur);

            if (cur == NULL)
            {
                return;
            }

            ++cur;
        }
    }
}

void ul_ec_init(ul_ec_t * ec, void * user_data, ul_ec_process_actn_proc_t * process_actn_proc)
{
    *ec = (ul_ec_t){ .user_data = user_data, .process_actn_proc = process_actn_proc };
}
void ul_ec_cleanup(ul_ec_t * ec)
{
    memset(ec, 0, sizeof(*ec));
}
