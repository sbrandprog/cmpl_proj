#include "ul_hsb.h"
#include "ul_arr.h"

void ul_hsb_init(ul_hsb_t * hsb)
{
    *hsb = (ul_hsb_t){ 0 };
}
void ul_hsb_cleanup(ul_hsb_t * hsb)
{
    free(hsb->buf);

    memset(hsb, 0, sizeof(*hsb));
}

size_t ul_hsb_format_va(ul_hsb_t * hsb, const char * fmt, va_list args)
{
    size_t str_size;

    {
        va_list new_args;

        va_copy(new_args, args);

        int res = vsnprintf(NULL, 0, fmt, new_args);

        va_end(new_args);

        ul_assert(res >= 0);

        str_size = (size_t)res;
    }

    size_t str_size_null = str_size + 1;

    ul_arr_grow(str_size_null, &hsb->cap, (void **)&hsb->buf, sizeof(*hsb->buf));

    {
        va_list new_args;

        va_copy(new_args, args);

        int res = vsprintf(hsb->buf, fmt, new_args);

        va_end(new_args);

        ul_assert(res >= 0);

        return (size_t)res;
    }
}
size_t ul_hsb_format(ul_hsb_t * hsb, const char * fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    size_t res = ul_hsb_format_va(hsb, fmt, args);

    va_end(args);

    return res;
}

ul_hs_t * ul_hsb_formatadd_va(ul_hsb_t * hsb, ul_hst_t * hst, const char * fmt, va_list args)
{
    size_t str_size = ul_hsb_format_va(hsb, fmt, args);

    return ul_hst_hashadd(hst, str_size, hsb->buf);
}

extern inline ul_hs_t * ul_hsb_formatadd(ul_hsb_t * hsb, ul_hst_t * hst, const char * fmt, ...);
extern inline ul_hs_t * ul_hsb_cat_hs_delim(ul_hsb_t * hsb, ul_hst_t * hst, ul_hs_t * first, char delim, ul_hs_t * second);
