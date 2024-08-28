#include "ul_json.h"

struct ul_json_p_src {
	void * data;
	ul_json_p_get_ch_proc_t * get_ch_proc;
	ul_hst_t * hst;
};

UL_API bool ul_json_p_parse(ul_json_p_src_t * src, ul_json_t ** out);

UL_API bool ul_json_p_parse_file(ul_hst_t * hst, const wchar_t * file_name, ul_json_t ** out);
UL_API bool ul_json_p_parse_str(ul_hst_t * hst, size_t str_size, const wchar_t * str, ul_json_t ** out);
