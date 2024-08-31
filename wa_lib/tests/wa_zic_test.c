#include "wa_lib/wa_lib.h"

static wa_ctx_t ctx;
static wa_style_col_t style_col;
static wa_style_font_t style_font;
static wa_style_t style;

int main() {
	wa_ctx_cleanup(&ctx);

	wa_style_col_cleanup(&style_col);

	wa_style_font_cleanup(&style_font);

	wa_style_cleanup(&style);

	return 0;
}
