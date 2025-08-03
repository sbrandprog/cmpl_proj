#include <ul/ul_lib.h>

static ul_ec_t ec;
static ul_ec_buf_t ec_buf;
static ul_ec_fmtr_t ec_fmtr;
static ul_hsb_t hsb;
static ul_hst_t hst;
static ul_json_g_sink_t json_g_sink;
static ul_json_p_src_t json_p_src;

int main()
{
    ul_ec_cleanup(&ec);

    ul_ec_buf_cleanup(&ec_buf);

    ul_ec_fmtr_cleanup(&ec_fmtr);

    ul_hsb_cleanup(&hsb);

    ul_hst_cleanup(&hst);

    ul_json_g_sink_cleanup(&json_g_sink);

    ul_json_p_src_cleanup(&json_p_src);

    return 0;
}
