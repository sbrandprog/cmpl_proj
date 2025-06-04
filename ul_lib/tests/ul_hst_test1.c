#include <ul_lib/ul_lib.h>

static ul_hst_t hst;

static int main_core()
{
	int res = 0;

    static const ul_ros_t a = UL_ROS_MAKE("aaa");
    static const ul_ros_t b = UL_ROS_MAKE("bbb");

    ul_hst_node_ref_t a_hs = ul_hst_hashadd(&hst, a.size, a.str);

    if (a_hs.node == NULL)
    {
        printf("added string is NULL\n");
        res = -1;
    }

    ul_hst_node_ref_t a_hs2 = ul_hst_hashadd(&hst, a.size, a.str);

    if (a_hs2.node == NULL)
    {
        printf("same added string is NULL\n");
        res = -1;
    }

    if (a_hs.node != a_hs2.node)
    {
        printf("nodes of same string are different\n");
        res = -1;
    }

    ul_hst_node_ref_t b_hs = ul_hst_hashadd(&hst, b.size, b.str);

    if (b_hs.node == NULL)
    {
        printf("2nd added string is NULL\n");
        res = -1;
    }

	if (a_hs.node == b_hs.node)
    {
        printf("nodes of different strings are the same\n");
        res = -1;
    }

	ul_hst_free_ref(&a_hs);
	ul_hst_free_ref(&a_hs2);
	ul_hst_free_ref(&b_hs);

    return res;
}
int main()
{
    ul_hst_init(&hst);

    int res = main_core();

    ul_hst_cleanup(&hst);

    return res;
}
