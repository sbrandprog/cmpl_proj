#include "ul_assert.h"

void ul_assert_func(const char * expr_str, const char * func_name)
{
    fprintf(stderr, "assertion failure of [%s] in [%s]\n", expr_str, func_name);

    abort();
}
