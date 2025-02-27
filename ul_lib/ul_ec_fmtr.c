#include "ul_ec_fmtr.h"
#include "ul_arr.h"
#include "ul_assert.h"

void ul_ec_fmtr_init(ul_ec_fmtr_t * fmtr, ul_ec_t * ec)
{
    *fmtr = (ul_ec_fmtr_t){ .ec = ec };
}
void ul_ec_fmtr_cleanup(ul_ec_fmtr_t * fmtr)
{
    memset(fmtr, 0, sizeof(*fmtr));
}

void ul_ec_fmtr_format_va(ul_ec_fmtr_t * fmtr, const char * fmt, va_list args)
{
    if (fmtr->strs_pos < ul_arr_count(fmtr->rec.strs) - 1)
    {
        int res = vsnprintf(fmtr->rec.strs + fmtr->strs_pos, ul_arr_count(fmtr->rec.strs) - fmtr->strs_pos, fmt, args);

        if (res >= 0)
        {
            size_t new_strs_pos = fmtr->strs_pos + (size_t)res + 1;

            if (new_strs_pos > ul_arr_count(fmtr->rec.strs))
            {
                new_strs_pos = ul_arr_count(fmtr->rec.strs);
            }

            fmtr->strs_pos = new_strs_pos;

            ++fmtr->rec.strs_size;
        }
        else
        {
            fmtr->strs_pos = ul_arr_count(fmtr->rec.strs);
        }
    }
    else
    {
        fmtr->strs_pos = ul_arr_count(fmtr->rec.strs);
    }
}

#if defined __linux__
extern inline void ul_ec_fmtr_format(ul_ec_fmtr_t * fmtr, const char * fmt, ...);
#endif

void ul_ec_fmtr_post(ul_ec_fmtr_t * fmtr, const char * type, const char * mod_name)
{
    fmtr->rec.type = type;
    fmtr->rec.mod_name = mod_name;

    ul_ec_post(fmtr->ec, &fmtr->rec);

    fmtr->rec.strs_size = 0;
    fmtr->strs_pos = 0;
}
