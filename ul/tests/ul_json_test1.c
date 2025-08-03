#include <ul/ul_lib.h>

#define FILE_NAME "test1.json"

typedef struct test_case
{
    ul_ros_t str;
    bool res;
} test_case_t;

static const test_case_t test_cases[] = {
    { .str = UL_ROS_MAKE("\tfalse"), .res = true },
    { .str = UL_ROS_MAKE("\nnull"), .res = true },
    { .str = UL_ROS_MAKE(" true"), .res = true },
    { .str = UL_ROS_MAKE("{ }"), .res = true },
    { .str = UL_ROS_MAKE("[\t]"), .res = true },
    { .str = UL_ROS_MAKE("{ }"), .res = true },
    { .str = UL_ROS_MAKE("\"\""), .res = true },
    { .str = UL_ROS_MAKE("\"string\""), .res = true },
    { .str = UL_ROS_MAKE("\"\\\"Hello world!\\n\\\"\""), .res = true },
    { .str = UL_ROS_MAKE("\r\n100"), .res = true },
    { .str = UL_ROS_MAKE("9223372036854775807"), .res = true },
    { .str = UL_ROS_MAKE("-9223372036854775807"), .res = true },
    { .str = UL_ROS_MAKE("-9223372036854775808"), .res = true },
    { .str = UL_ROS_MAKE("3.14159265358979323846"), .res = true },
    { .str = UL_ROS_MAKE("2.71828182845904523536"), .res = true },
    { .str = UL_ROS_MAKE("{ \"first\": 20, \"second\":\t30 }"), .res = true },
};
static const size_t test_cases_size = ul_arr_count(test_cases);

static ul_hst_t hst;

static bool is_json_equal(ul_json_t * first, ul_json_t * second)
{
    if (first == second)
    {
        return true;
    }

    if (first->type != second->type)
    {
        return false;
    }

    switch (first->type)
    {
        case UlJsonNull:
            break;
        case UlJsonBool:
            if (first->val_bool != second->val_bool)
            {
                return false;
            }

            break;
        case UlJsonInt:
            if (first->val_int != second->val_int)
            {
                return false;
            }

            break;
        case UlJsonDbl:
            if (first->val_dbl != second->val_dbl)
            {
                return false;
            }

            break;
        case UlJsonStr:
            if (first->val_str != second->val_str)
            {
                return false;
            }

            break;
        case UlJsonArr:
        case UlJsonObj:
        {
            ul_json_t *fj = first->val_json, *sj = second->val_json;

            for (; fj != NULL && sj != NULL; fj = fj->next, sj = sj->next)
            {
                if (first->type == UlJsonObj && sj->name != fj->name)
                {
                    return false;
                }

                if (!is_json_equal(fj, sj))
                {
                    return false;
                }
            }

            if (fj != sj)
            {
                return false;
            }

            break;
        }
        default:
            return false;
    }

    return true;
}

static int main_core()
{
    ul_hst_init(&hst);

    size_t tc_ind = 0;
    for (const test_case_t *tc = test_cases, *tc_end = tc + test_cases_size; tc != tc_end; ++tc, ++tc_ind)
    {
        bool res0 = false, res1 = true, res2 = true, res3 = true;
        ul_json_t *json0 = NULL, *json1 = NULL;

        res0 = ul_json_p_parse_str(&hst, tc->str.size, tc->str.str, &json0);

        if (res0)
        {
            res1 = ul_json_g_generate_file(FILE_NAME, json0);

            res2 = ul_json_p_parse_file(&hst, FILE_NAME, &json1);
        }

        if (json0 != NULL && json1 != NULL)
        {
            res3 = is_json_equal(json0, json1);
        }

        ul_json_destroy(json0);
        ul_json_destroy(json1);

        if (res0 != tc->res || res0 && (!res1 || !res2) || !res3)
        {
            printf("failed #%zi test case\n", tc_ind);
            printf("test case:\n%s\n", tc->str.str);

            const char * desc_str = NULL;

            if (res0 != tc->res)
            {
                desc_str = "does not match expected result";
            }
            else if (!res1)
            {
                desc_str = "failed to write file";
            }
            else if (!res2)
            {
                desc_str = "failed to read written file";
            }
            else if (!res3)
            {
                desc_str = "[test case] json is not equal to [file written] json";
            }

            printf("descrption: %s\n", desc_str);

            return -1;
        }
    }

    printf("success\n");

    return 0;
}
int main()
{
    int res = main_core();

    ul_hst_cleanup(&hst);

    remove(FILE_NAME);

    return res;
}
