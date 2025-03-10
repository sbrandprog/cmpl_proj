#include "ul_json.h"

struct ul_json_p_src
{
    ul_hst_t * hst;
    void * data;
    ul_json_p_get_ch_proc_t * get_ch_proc;
};

UL_API void ul_json_p_src_init(ul_json_p_src_t * src, ul_hst_t * hst, void * data, ul_json_p_get_ch_proc_t * get_ch_proc);
UL_API void ul_json_p_src_cleanup(ul_json_p_src_t * src);

UL_API bool ul_json_p_parse(ul_json_p_src_t * src, ul_json_t ** out);

UL_API bool ul_json_p_parse_file(ul_hst_t * hst, const char * file_name, ul_json_t ** out);
UL_API bool ul_json_p_parse_str(ul_hst_t * hst, size_t str_size, const char * str, ul_json_t ** out);
