#include "pla_ec.h"

static bool scan_pos(const wchar_t * buf, pla_ec_pos_t * out)
{
    pla_ec_pos_t pos = { 0 };

    int res = swscanf_s(buf, L"%zi %zi", &pos.line_num, &pos.line_ch);

    if (res != 2)
    {
        return false;
    }

    *out = pos;

    return true;
}

bool pla_ec_scan(ul_ec_rec_t * rec, pla_ec_err_t * out)
{
    if (wcscmp(rec->type, PLA_EC_REC_TYPE) != 0)
    {
        return false;
    }

    pla_ec_err_t err = { .mod_name = rec->mod_name };

    const wchar_t *cur = rec->strs, *cur_end = cur + _countof(rec->strs);
    for (size_t i = 0, i_end = min(4, rec->strs_size); i < i_end; ++i)
    {
        switch (i)
        {
            case 0:
                err.src_name = cur;
                break;
            case 1:
                if (!scan_pos(cur, &err.pos_start))
                {
                    return false;
                }
                break;
            case 2:
                if (!scan_pos(cur, &err.pos_end))
                {
                    return false;
                }
                break;
            case 3:
                err.msg = cur;
                break;
        }

        if (i + 1 < i_end)
        {
            cur = wmemchr(cur, 0, cur_end - cur);

            if (cur == NULL)
            {
                return false;
            }

            ++cur;
        }
    }

    *out = err;

    return true;
}

void pla_ec_formatpost_va(ul_ec_fmtr_t * fmtr, const wchar_t * mod_name, const wchar_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const wchar_t * fmt, va_list args)
{
    ul_ec_fmtr_format(fmtr, L"%s", src_name);
    ul_ec_fmtr_format(fmtr, L"%zi %zi", pos_start.line_num, pos_start.line_ch);
    ul_ec_fmtr_format(fmtr, L"%zi %zi", pos_end.line_num, pos_end.line_ch);
    ul_ec_fmtr_format_va(fmtr, fmt, args);

    ul_ec_fmtr_post(fmtr, PLA_EC_REC_TYPE, mod_name);
}
void pla_ec_formatpost(ul_ec_fmtr_t * fmtr, const wchar_t * mod_name, const wchar_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const wchar_t * fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    pla_ec_formatpost_va(fmtr, mod_name, src_name, pos_start, pos_end, fmt, args);

    va_end(args);
}
