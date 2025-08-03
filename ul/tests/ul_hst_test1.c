#include <ul/ul_lib.h>

static ul_hst_t hst;

static int main_core()
{
    static const ul_ros_t a = UL_ROS_MAKE("aaa");
    static const ul_ros_t b = UL_ROS_MAKE("bbb");

    ul_hs_t * a_hs = ul_hst_hashadd(&hst, a.size, a.str);

    if (a_hs == NULL)
    {
        printf("added string is NULL\n");
        return -1;
    }

    ul_hs_t * a_hs2 = ul_hst_hashadd(&hst, a.size, a.str);

    if (a_hs2 == NULL)
    {
        printf("same added string is NULL\n");
        return -1;
    }

    if (a_hs != a_hs2)
    {
        printf("nodes of same string are different\n");
        return -1;
    }

    ul_hs_t * b_hs = ul_hst_hashadd(&hst, b.size, b.str);

    if (b_hs == NULL)
    {
        printf("2nd added string is NULL\n");
        return -1;
    }

	if (a_hs == b_hs)
    {
        printf("nodes of different strings are the same\n");
        return -1;
    }

    return 0;
}
int main()
{
    ul_hst_init(&hst);

    int res = main_core();

    ul_hst_cleanup(&hst);

    return res;
}
