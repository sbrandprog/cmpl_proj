#include "ul_json.h"

struct ul_json_g_sink {
	void * data;
	ul_json_g_put_ch_proc_t * put_ch_proc;
	bool put_ws;
};

UL_API bool ul_json_g_generate(ul_json_g_sink_t * sink, ul_json_t * json);

UL_API bool ul_json_g_generate_file(const wchar_t * file_name, ul_json_t * json);
