#include "pch.h"
#include "pla_ast_p.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_irid.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "pla_dclr.h"
#include "pla_ast.h"
#include "ira_int.h"
#include "u_assert.h"
#include "u_misc.h"

typedef bool ch_pred_t(wchar_t ch);

typedef struct file_pos {
	long long file_cur;
	size_t line_num;
	size_t line_ch;
} file_pos_t;

typedef enum tok_type {
	TokNone,
	TokPunc,
	TokKeyw,
	TokIdent,
	TokChStr,
	TokNumStr,
	Tok_Count
} tok_type_t;
typedef struct tok {
	tok_type_t type;
	union {
		pla_punc_t punc;
		pla_keyw_t keyw;
		u_hs_t * ident;
		struct {
			u_hs_t * data;
			u_hs_t * tag;
		} ch_str, num_str;
	};
} tok_t;

typedef size_t bin_oper_preced_t;
typedef struct post_oper_info {
	pla_punc_t punc;
	pla_expr_type_t expr_type;
} post_oper_info_t;
typedef struct unr_oper_info {
	pla_punc_t punc;
	pla_expr_type_t expr_type;
} unr_oper_info_t;
typedef struct bin_oper_info {
	pla_punc_t punc;
	pla_expr_type_t expr_type;
	bin_oper_preced_t preced;
} bin_oper_info_t;

typedef struct asm_ast_p_ctx {
	u_hst_t * hst;

	const wchar_t * file_name;

	pla_ast_t * out;

	FILE * file;

	wchar_t ch;
	bool ch_succ;
	long long ch_pos;

	bool line_switch;
	size_t line_num;
	size_t line_ch;
	long long line_start;

	size_t str_size;
	wchar_t * str;
	size_t str_cap;
	u_hs_hash_t str_hash;

	size_t punc_start;

	file_pos_t tok_start;
	tok_t tok;
} ctx_t;


static const post_oper_info_t opr_post_infos[] = {
	{ .punc = PlaPuncLeBrack, .expr_type = PlaExprDtArr },
	{ .punc = PlaPuncDot, .expr_type = PlaExprMmbrAcc },
	{ .punc = PlaPuncLeParen, .expr_type = PlaExprCall },
	{ .punc = PlaPuncLeBrack, .expr_type = PlaExprSubscr },
	{ .punc = PlaPuncDplus, .expr_type = PlaExprPostInc },
	{ .punc = PlaPuncDminus, .expr_type = PlaExprPostDec },
	{ .punc = PlaPuncExclMark, .expr_type = PlaExprDeref },
	{ .punc = PlaPuncTilde, .expr_type = PlaExprAddrOf },
};
static const size_t opr_post_infos_count = _countof(opr_post_infos);
static const unr_oper_info_t opr_unr_infos[] = {
	{ .punc = PlaPuncDplus, .expr_type = PlaExprPostInc },
	{ .punc = PlaPuncDminus, .expr_type = PlaExprPostDec },
	{ .punc = PlaPuncExclMark, .expr_type = PlaExprLogicNeg },
	{ .punc = PlaPuncPerc, .expr_type = PlaExprCast },
	{ .punc = PlaPuncTilde, .expr_type = PlaExprBitNeg },
	{ .punc = PlaPuncMinus, .expr_type = PlaExprArithNeg }
};
static const size_t opr_unr_infos_count = _countof(opr_unr_infos);
static const bin_oper_info_t opr_bin_infos[] = {
	{ .punc = PlaPuncAster, .expr_type = PlaExprMul, .preced = 11 },
	{ .punc = PlaPuncSlash, .expr_type = PlaExprDiv, .preced = 11 },
	{ .punc = PlaPuncPerc, .expr_type = PlaExprMod, .preced = 11 },
	{ .punc = PlaPuncPlus, .expr_type = PlaExprAdd, .preced = 10 },
	{ .punc = PlaPuncMinus, .expr_type = PlaExprSub, .preced = 10 },
	{ .punc = PlaPuncLeShift, .expr_type = PlaExprLeShift, .preced = 9 },
	{ .punc = PlaPuncRiShift, .expr_type = PlaExprRiShift, .preced = 9 },
	{ .punc = PlaPuncLess, .expr_type = PlaExprLess, .preced = 8 },
	{ .punc = PlaPuncLessEq, .expr_type = PlaExprLessEq, .preced = 8 },
	{ .punc = PlaPuncGrtr, .expr_type = PlaExprGrtr, .preced = 8 },
	{ .punc = PlaPuncGrtrEq, .expr_type = PlaExprGrtrEq, .preced = 8 },
	{ .punc = PlaPuncEq, .expr_type = PlaExprEq, .preced = 7 },
	{ .punc = PlaPuncNeq, .expr_type = PlaExprNeq, .preced = 7 },
	{ .punc = PlaPuncAmper, .expr_type = PlaExprBitAnd, .preced = 6 },
	{ .punc = PlaPuncCaret, .expr_type = PlaExprBitXor, .preced = 5 },
	{ .punc = PlaPuncPipe, .expr_type = PlaExprBitOr, .preced = 4 },
	{ .punc = PlaPuncDamper, .expr_type = PlaExprLogicAnd, .preced = 3 },
	{ .punc = PlaPuncDpipe, .expr_type = PlaExprLogicOr, .preced = 2 },
};
static const size_t opr_bin_infos_count = _countof(opr_bin_infos);


static bool is_emp_ch(wchar_t ch) {
	return ch == L' ' || ch == '\t' || ch == L'\n';
}

static bool is_punc_ch(wchar_t ch) {
	return (L'!' <= ch && ch <= L'/' && ch != L'\"' && ch != '\'') || (L':' <= ch && ch <= L'>') || (L'[' <= ch && ch <= L'^') || (L'{' <= ch && ch <= L'~');
}

static bool is_first_ident_ch(wchar_t ch) {
	return (L'A' <= ch && ch <= L'Z') || (L'a' <= ch && ch <= L'z') || ch == L'_';
}
static bool is_ident_ch(wchar_t ch) {
	return is_first_ident_ch(ch) || (L'0' <= ch && ch <= L'9');
}

static bool is_tag_ch(wchar_t ch) {
	return is_ident_ch(ch);
}

static bool is_dqoute_ch(wchar_t ch) {
	return ch == L'\"';
}
static bool is_ch_str_ch(wchar_t ch) {
	return !is_dqoute_ch(ch);
}

static bool is_first_num_str_ch(wchar_t ch) {
	return (L'0' <= ch && ch <= L'9');
}
static bool is_num_str_ch(wchar_t ch) {
	return is_first_num_str_ch(ch) || (L'A' <= ch && ch <= L'Z') || (L'a' <= ch && ch <= L'z') || ch == L'_';
}
static bool is_num_str_tag_intro_ch(wchar_t ch) {
	return ch == L'\'';
}


static void report(ctx_t * ctx, const wchar_t * format, ...) {
	fwprintf(stderr, L"@%4zi:%4zi\n", ctx->tok_start.line_num, ctx->tok_start.line_ch);

	long long backup_cur = _ftelli64(ctx->file);
	u_assert(backup_cur != -1L);

	{
		int res = _fseeki64(ctx->file, ctx->line_start, SEEK_SET);
		u_assert(res == 0);
	}

	wint_t ch;

	while ((ch = fgetwc(ctx->file)) != WEOF) {
		u_assert(ferror(ctx->file) == 0);

		if (ch == L'\t') {
			ch = L' ';
		}

		fputwc(ch, stderr);

		if (ch == L'\n') {
			break;
		}
	}

	{
		int res = _fseeki64(ctx->file, backup_cur, SEEK_SET);
		u_assert(res == 0);
	}

	long long diff = ctx->tok_start.file_cur - ctx->line_start;
	for (long long ch = 0; ch < diff; ++ch) {
		fputwc(L' ', stderr);
	}

	diff = ctx->ch_pos - ctx->tok_start.file_cur;
	for (long long ch = 0; ch < diff; ++ch) {
		fputwc(L'^', stderr);
	}
	fputwc(L'\n', stderr);

	{
		va_list args;

		va_start(args, format);

		vfwprintf(stderr, format, args);

		va_end(args);

		fputwc(L'\n', stderr);
	}
}


static bool next_ch(ctx_t * ctx) {
	ctx->ch_succ = false;

	ctx->ch_pos = _ftelli64(ctx->file);
	u_assert(ctx->ch_pos != -1);

	wint_t new_ch = fgetwc(ctx->file);

	if (new_ch == WEOF) {
		u_assert(ferror(ctx->file) == 0);
		return false;
	}

	ctx->ch = (wchar_t)new_ch;
	ctx->ch_succ = true;

	++ctx->line_ch;

	if (ctx->line_switch) {
		ctx->line_switch = false;
		++ctx->line_num;
		ctx->line_ch = 0;
		ctx->line_start = ctx->ch_pos;
	}

	if (ctx->ch == L'\n') {
		ctx->line_switch = true;
	}

	return true;
}
static file_pos_t capture_file_pos(ctx_t * ctx) {
	return (file_pos_t) {
		.file_cur = ctx->ch_pos, .line_num = ctx->line_num, .line_ch = ctx->line_ch
	};
}
static void push_str_ch(ctx_t * ctx, wchar_t ch) {
	if (ctx->str_size + 1 > ctx->str_cap) {
		u_grow_arr(&ctx->str_cap, &ctx->str, sizeof(*ctx->str), 1);
	}

	ctx->str[ctx->str_size++] = ch;
	ctx->str_hash = u_hs_hash_ch(ctx->str_hash, ch);
}
static void fetch_str(ctx_t * ctx, ch_pred_t * pred) {
	ctx->str_size = 0;
	ctx->str_hash = 0;

	do {
		push_str_ch(ctx, ctx->ch);
	} while (next_ch(ctx) && pred(ctx->ch));
}
static u_hs_t * hadd_str(ctx_t * ctx) {
	return u_hst_add(ctx->hst, ctx->str_size, ctx->str, ctx->str_hash);
}
static u_hs_t * fadd_str(ctx_t * ctx, ch_pred_t * pred) {
	fetch_str(ctx, pred);
	return hadd_str(ctx);
}
static bool next_tok(ctx_t * ctx) {
	ctx->tok.type = TokNone;

	if (ctx->punc_start != 0) {
		size_t punc_len;
		pla_punc_t punc = pla_punc_fetch_best(ctx->str_size, ctx->str + ctx->punc_start, &punc_len);

		if (punc == PlaPuncNone) {
			report(ctx, L"invalid punctuator string");
			return false;
		}

		if (ctx->punc_start + punc_len < ctx->str_size) {
			ctx->punc_start += punc_len;
		}
		else {
			ctx->punc_start = 0;
		}

		ctx->tok.type = TokPunc;
		ctx->tok.punc = punc;

		return true;
	}

	if (!ctx->ch_succ) {
		return false;
	}

	while (is_emp_ch(ctx->ch)) {
		if (!next_ch(ctx)) {
			return false;
		}
	}

	ctx->tok_start = capture_file_pos(ctx);

	if (is_punc_ch(ctx->ch)) {
		fetch_str(ctx, is_punc_ch);

		size_t punc_len;
		pla_punc_t punc = pla_punc_fetch_best(ctx->str_size, ctx->str, &punc_len);

		if (punc == PlaPuncNone) {
			report(ctx, L"invalid punctuator string");
			return false;
		}

		if (punc_len < ctx->str_size) {
			ctx->punc_start = punc_len;
		}

		ctx->tok.type = TokPunc;
		ctx->tok.punc = punc;

		return true;
	}
	else if (is_first_ident_ch(ctx->ch)) {
		fetch_str(ctx, is_ident_ch);

		pla_keyw_t keyw = pla_keyw_fetch_exact(ctx->str_size, ctx->str);

		if (keyw != PlaKeywNone) {
			ctx->tok.type = TokKeyw;
			ctx->tok.keyw = keyw;

			return true;
		}

		u_hs_t * str = hadd_str(ctx);

		ctx->tok.type = TokIdent;
		ctx->tok.ident = str;

		return true;
	}
	else if (is_dqoute_ch(ctx->ch)) {
		if (!next_ch(ctx)) {
			return false;
		}

		u_hs_t * data = fadd_str(ctx, is_ch_str_ch);

		if (!is_dqoute_ch(ctx->ch) || !next_ch(ctx) || !is_tag_ch(ctx->ch)) {
			report(ctx, L"invalid character string");
			return false;
		}

		u_hs_t * tag = fadd_str(ctx, is_tag_ch);

		ctx->tok.type = TokChStr;
		ctx->tok.ch_str.data = data;
		ctx->tok.ch_str.tag = tag;

		return true;
	}
	else if (is_first_num_str_ch(ctx->ch)) {
		u_hs_t * data = fadd_str(ctx, is_num_str_ch);

		if (!is_num_str_tag_intro_ch(ctx->ch) || !next_ch(ctx) || !is_tag_ch(ctx->ch)) {
			report(ctx, L"invalid number string");
			return false;
		}

		u_hs_t * tag = fadd_str(ctx, is_tag_ch);

		ctx->tok.type = TokNumStr;
		ctx->tok.num_str.data = data;
		ctx->tok.num_str.tag = tag;

		return true;
	}
	else {
		report(ctx, L"unknown character");
		return false;
	}
}


static pla_punc_t get_punc(ctx_t * ctx) {
	if (ctx->tok.type == TokPunc) {
		return ctx->tok.punc;
	}

	return PlaPuncNone;
}
static bool consume_punc_exact(ctx_t * ctx, pla_punc_t punc) {
	if (get_punc(ctx) == punc) {
		next_tok(ctx);
		return true;
	}

	return false;
}
static bool consume_punc_exact_crit(ctx_t * ctx, pla_punc_t punc) {
	u_assert(punc < PlaPunc_Count);

	if (consume_punc_exact(ctx, punc)) {
		return true;
	}

	report(ctx, L"failed to consume \'%s\'", pla_punc_strs[punc].str);
	return false;
}
static pla_keyw_t get_keyw(ctx_t * ctx) {
	if (ctx->tok.type == TokKeyw) {
		return ctx->tok.keyw;
	}

	return PlaKeywNone;
}
static bool consume_keyw_exact(ctx_t * ctx, pla_keyw_t keyw) {
	if (get_keyw(ctx) == keyw) {
		next_tok(ctx);
		return true;
	}

	return false;
}
static bool consume_keyw_exact_crit(ctx_t * ctx, pla_keyw_t keyw) {
	u_assert(keyw < PlaKeyw_Count);

	if (consume_keyw_exact(ctx, keyw)) {
		return true;
	}

	report(ctx, L"failed to consume \'%s\'", pla_keyw_strs[keyw].str);
	return false;
}
static bool consume_ident(ctx_t * ctx, u_hs_t ** out) {
	if (ctx->tok.type == TokIdent) {
		*out = ctx->tok.ident;
		next_tok(ctx);
		return true;
	}

	return false;
}
static bool consume_ident_crit(ctx_t * ctx, u_hs_t ** out) {
	if (consume_ident(ctx, out)) {
		return true;
	}

	report(ctx, L"failed to consume an identifier");
	return false;
}
static bool consume_ch_str_crit(ctx_t * ctx, u_hs_t ** out_data, u_hs_t ** out_tag) {
	if (ctx->tok.type == TokChStr) {
		*out_data = ctx->tok.ch_str.data;
		if (out_tag != NULL) {
			*out_tag = ctx->tok.ch_str.tag;
		}
		next_tok(ctx);
		return true;
	}

	report(ctx, L"failed to consume a character string");
	return false;
}

static const post_oper_info_t * get_post_oper_info(pla_punc_t punc) {
	for (size_t i = 0; i < opr_post_infos_count; ++i) {
		if (opr_post_infos[i].punc == punc) {
			return &opr_post_infos[i];
		}
	}

	return NULL;
}
static const unr_oper_info_t * get_unr_oper_info(pla_punc_t punc) {
	for (size_t i = 0; i < opr_unr_infos_count; ++i) {
		if (opr_unr_infos[i].punc == punc) {
			return &opr_unr_infos[i];
		}
	}

	return NULL;
}
static const bin_oper_info_t * get_bin_oper_info(pla_punc_t punc) {
	for (size_t i = 0; i < opr_bin_infos_count; ++i) {
		if (opr_bin_infos[i].punc == punc) {
			return &opr_bin_infos[i];
		}
	}

	return NULL;
}

static ira_int_type_t get_int_type(pla_keyw_t keyw) {
	switch (keyw) {
		case PlaKeywU8:
			return IraIntU8;
		case PlaKeywU16:
			return IraIntU16;
		case PlaKeywU32:
			return IraIntU32;
		case PlaKeywU64:
			return IraIntU64;
		case PlaKeywS8:
			return IraIntS8;
		case PlaKeywS16:
			return IraIntS16;
		case PlaKeywS32:
			return IraIntS32;
		case PlaKeywS64:
			return IraIntS64;
		default:
			u_assert_switch(keyw);
	}
}


static bool parse_expr(ctx_t * ctx, pla_expr_t ** out);

static bool parse_expr_unit(ctx_t * ctx, pla_expr_t ** out) {
	while (true) {
		const unr_oper_info_t * info = get_unr_oper_info(get_punc(ctx));

		if (info == NULL) {
			break;
		}

		next_tok(ctx);

		*out = pla_expr_create(info->expr_type);

		switch (info->expr_type) {
			case PlaExprCast:
				if (!consume_punc_exact_crit(ctx, PlaPuncLeBrack)) {
					return false;
				}

				if (!parse_expr(ctx, &(*out)->opd1.expr)) {
					return false;
				}

				if (!consume_punc_exact_crit(ctx, PlaPuncRiBrack)) {
					return false;
				}
				break;
		}

		out = &(*out)->opd0.expr;
	}


	bool success = true;

	switch (ctx->tok.type) {
		case TokPunc:
			switch (ctx->tok.punc) {
				case PlaPuncLeParen:
					next_tok(ctx);

					if (!parse_expr(ctx, out)) {
						return false;
					}

					if (!consume_punc_exact_crit(ctx, PlaPuncRiParen)) {
						return false;
					}
					break;
				case PlaPuncDolrSign:
					next_tok(ctx);

					if (!consume_punc_exact_crit(ctx, PlaPuncLeParen)) {
						return false;
					}

					*out = pla_expr_create(PlaExprDtFunc);

					if (!consume_punc_exact(ctx, PlaPuncRiParen)) {
						pla_expr_t ** arg = &(*out)->opd1.expr;

						while (true) {
							*arg = pla_expr_create(PlaExprDtFuncArg);

							if (!consume_ident_crit(ctx, &(*arg)->opd2.hs)) {
								return false;
							}

							if (!consume_punc_exact_crit(ctx, PlaPuncColon)) {
								return false;
							}

							if (!parse_expr(ctx, &(*arg)->opd0.expr)) {
								return false;
							}

							if (consume_punc_exact(ctx, PlaPuncRiParen)) {
								break;
							}
							else {
								if (!consume_punc_exact_crit(ctx, PlaPuncComma)) {
									return false;
								}
							}

							arg = &(*arg)->opd1.expr;
						}
					}

					if (consume_punc_exact_crit(ctx, PlaPuncRiArrow)) {
						if (!parse_expr(ctx, &(*out)->opd0.expr)) {
							return false;
						}
					}
					else {
						(*out)->opd0.expr = pla_expr_create(PlaExprDtVoid);
					}

					break;
				default:
					success = false;
			}
			break;
		case TokKeyw:
			switch (ctx->tok.keyw) {
				case PlaKeywVoid:
					*out = pla_expr_create(PlaExprDtVoid);
					next_tok(ctx);
					break;
				case PlaKeywBool:
					*out = pla_expr_create(PlaExprDtBool);
					next_tok(ctx);
					break;
				case PlaKeywU8:
				case PlaKeywU16:
				case PlaKeywU32:
				case PlaKeywU64:
				case PlaKeywS8:
				case PlaKeywS16:
				case PlaKeywS32:
				case PlaKeywS64:
					*out = pla_expr_create(PlaExprDtInt);
					(*out)->opd0.int_type = get_int_type(ctx->tok.keyw);
					next_tok(ctx);
					break;
				case PlaKeywTupl:
					next_tok(ctx);

					if (!consume_punc_exact_crit(ctx, PlaPuncLeBrace)) {
						return false;
					}

					*out = pla_expr_create(PlaExprDtTpl);

					pla_expr_t ** elem = &(*out)->opd1.expr;

					while (true) {
						*elem = pla_expr_create(PlaExprDtFuncArg);

						if (!consume_ident_crit(ctx, &(*elem)->opd2.hs)) {
							return false;
						}

						if (!consume_punc_exact_crit(ctx, PlaPuncColon)) {
							return false;
						}

						if (!parse_expr(ctx, &(*elem)->opd0.expr)) {
							return false;
						}

						if (consume_punc_exact(ctx, PlaPuncRiBrace)) {
							break;
						}
						else {
							if (!consume_punc_exact_crit(ctx, PlaPuncComma)) {
								return false;
							}
						}

						elem = &(*elem)->opd1.expr;
					}
					break;

				case PlaKeywVoidval:
					*out = pla_expr_create(PlaExprValVoid);
					next_tok(ctx);
					break;
				case PlaKeywTrue:
					*out = pla_expr_create(PlaExprValBool);
					(*out)->opd0.boolean = true;
					next_tok(ctx);
					break;
				case PlaKeywFalse:
					*out = pla_expr_create(PlaExprValBool);
					(*out)->opd0.boolean = false;
					next_tok(ctx);
					break;
				case PlaKeywNullof:
					*out = pla_expr_create(PlaExprNullofDt);

					next_tok(ctx);

					if (!consume_punc_exact_crit(ctx, PlaPuncLeBrack)) {
						return false;
					}

					if (!parse_expr(ctx, &(*out)->opd0.expr)) {
						return false;
					}

					if (!consume_punc_exact_crit(ctx, PlaPuncRiBrack)) {
						return false;
					}
					break;

				default:
					success = false;
			}
			break;
		case TokIdent:
		{
			*out = pla_expr_create(PlaExprIdent);
			
			pla_irid_t ** ins = &(*out)->opd0.irid;

			do {
				*ins = pla_irid_create(ctx->tok.ident);

				next_tok(ctx);

				ins = &(*ins)->sub_name;
			} while (consume_punc_exact(ctx, PlaPuncDcolon));

			break;
		}
		case TokChStr:
			*out = pla_expr_create(PlaExprChStr);
			(*out)->opd0.hs = ctx->tok.ch_str.data;
			(*out)->opd1.hs = ctx->tok.ch_str.tag;
			next_tok(ctx);
			break;
		case TokNumStr:
			*out = pla_expr_create(PlaExprNumStr);
			(*out)->opd0.hs = ctx->tok.num_str.data;
			(*out)->opd1.hs = ctx->tok.num_str.tag;
			next_tok(ctx);
			break;
		default:
			success = false;
	}

	if (!success) {
		report(ctx, L"failed to parse an unit-expression");
		return false;
	}


	while (true) {
		const post_oper_info_t * info = get_post_oper_info(get_punc(ctx));

		if (info == NULL) {
			break;
		}

		next_tok(ctx);

		pla_expr_t * expr = pla_expr_create(info->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		switch (info->expr_type) {
			case PlaExprDtArr:
				if (!consume_punc_exact_crit(ctx, PlaPuncRiBrack)) {
					return false;
				}
				break;
			case PlaExprMmbrAcc:
				if (!consume_ident_crit(ctx, &(*out)->opd1.hs)) {
					return false;
				}
				break;
			case PlaExprCall:
				if (!consume_punc_exact(ctx, PlaPuncRiParen)) {
					pla_expr_t ** arg = &(*out)->opd1.expr;

					while (true) {
						*arg = pla_expr_create(PlaExprCallArg);

						if (!parse_expr(ctx, &(*arg)->opd0.expr)) {
							return false;
						}

						if (consume_punc_exact(ctx, PlaPuncRiParen)) {
							break;
						}
						else {
							if (!consume_punc_exact_crit(ctx, PlaPuncComma)) {
								return false;
							}
						}

						arg = &(*arg)->opd1.expr;
					}
				}
				break;
			case PlaExprSubscr:
				if (!parse_expr(ctx, &(*out)->opd1.expr)) {
					return false;
				}

				if (consume_punc_exact_crit(ctx, PlaPuncRiBrack)) {
					return false;
				}
				break;
			case PlaExprDeref:
			case PlaExprAddrOf:
			{
				u_hs_t * ident;

				if (consume_ident(ctx, &ident)) {
					expr = pla_expr_create(PlaExprMmbrAcc);

					expr->opd0.expr = *out;
					*out = expr;

					expr->opd1.hs = ident;
				}
				break;
			}
		}
	}

	return true;
}
static bool parse_expr_bin_core(ctx_t * ctx, pla_expr_t ** out, const bin_oper_info_t ** info) {
	bin_oper_preced_t preced = (*info)->preced;

	do {
		pla_expr_t * expr = pla_expr_create((*info)->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		if (!parse_expr_unit(ctx, &(*out)->opd1.expr)) {
			return false;
		}

		*info = get_bin_oper_info(get_punc(ctx));

		if (*info != NULL) {
			next_tok(ctx);
		}

		while (*info != NULL && preced < (*info)->preced) {
			if (!parse_expr_bin_core(ctx, &(*out)->opd1.expr, info)) {
				return false;
			}
		}
	} while (*info != NULL && preced == (*info)->preced);

	return true;
}
static bool parse_expr_bin(ctx_t * ctx, pla_expr_t ** out) {
	const bin_oper_info_t * info = get_bin_oper_info(get_punc(ctx));

	if (info != NULL) {
		if (!next_tok(ctx)) {
			return false;
		}

		do {
			if (!parse_expr_bin_core(ctx, out, &info)) {
				return false;
			}
		} while (info != NULL);
	}

	return true;
}
static bool parse_expr_asgn(ctx_t * ctx, pla_expr_t ** out) {
	while (true) {
		if (!parse_expr_unit(ctx, out)) {
			return false;
		}

		pla_expr_type_t expr_type = PlaExprNone;

		switch (get_punc(ctx)) {
			case PlaPuncAsgn:
				expr_type = PlaExprAsgn;
				break;
		}

		if (expr_type == PlaExprNone) {
			if (!parse_expr_bin(ctx, out)) {
				return false;
			}
			break;
		}

		next_tok(ctx);

		pla_expr_t * expr = pla_expr_create(expr_type);

		expr->opd0.expr = *out;
		*out = expr;
		out = &expr->opd1.expr;
	}

	return true;
}
static bool parse_expr(ctx_t * ctx, pla_expr_t ** out) {
	return parse_expr_asgn(ctx, out);
}


static bool parse_stmt(ctx_t * ctx, pla_stmt_t ** out);

static bool parse_stmt_blk(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_punc_exact_crit(ctx, PlaPuncLeBrace)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtBlk);

	for (pla_stmt_t ** ins = &(*out)->block.body; !consume_punc_exact(ctx, PlaPuncRiBrace); ) {
		if (!parse_stmt(ctx, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}

static bool parse_stmt_expr(ctx_t * ctx, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtExpr);

	if (!parse_expr(ctx, &(*out)->expr.expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_var(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywVrbl)) {
		return false;
	}

	while (true) {
		u_hs_t * var_name;

		if (!consume_ident_crit(ctx, &var_name)) {
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncColon)) {
			*out = pla_stmt_create(PlaStmtVarDt);
		
			(*out)->var_dt.name = var_name;

			if (!parse_expr(ctx, &(*out)->var_dt.dt_expr)) {
				return false;
			}
		}
		else if (consume_punc_exact(ctx, PlaPuncScAsgn)) {
			*out = pla_stmt_create(PlaStmtVarVal);

			(*out)->var_val.name = var_name;

			if (!parse_expr(ctx, &(*out)->var_val.val_expr)) {
				return false;
			}
		}
		else {
			report(ctx, L"failed to consume a \':\' or \':=\'");
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncComma)) {
			/* nothing */
		}
		else {
			if (consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
				break;
			}
			else {
				return false;
			}
		}

		out = &(*out)->next;
	}

	return true;
}
static bool parse_stmt_cond(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywIf)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtCond);

	if (!parse_expr(ctx, &(*out)->cond.cond_expr)) {
		return false;
	}

	if (!parse_stmt_blk(ctx, &(*out)->cond.true_br)) {
		return false;
	}

	if (consume_keyw_exact(ctx, PlaKeywElse)) {
		if (get_punc(ctx) == PlaPuncLeBrace) {
			if (!parse_stmt_blk(ctx, &(*out)->cond.false_br)) {
				return false;
			}
		}
		else if (get_keyw(ctx) == PlaKeywIf) {
			if (!parse_stmt_cond(ctx, &(*out)->cond.false_br)) {
				return false;
			}
		}
		else {
			report(ctx, L"expected an else or else-if clause");
			return false;
		}
	}

	return true;
}
static bool parse_stmt_pre_loop(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywWhile)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtPreLoop);

	if (!parse_expr(ctx, &(*out)->pre_loop.cond_expr)) {
		return false;
	}

	if (!parse_stmt_blk(ctx, &(*out)->pre_loop.body)) {
		return false;
	}

	return true;
}
static bool parse_stmt_ret(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywRet)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtRet);

	if (!parse_expr(ctx, &(*out)->ret.expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt(ctx_t * ctx, pla_stmt_t ** out) {
	switch (ctx->tok.type) {
		case TokPunc:
			switch (ctx->tok.punc) {
				case PlaPuncSemicolon:
					next_tok(ctx);
					return true;
			}
			break;
		case TokKeyw:
			switch (ctx->tok.keyw) {
				case PlaKeywVrbl:
					return parse_stmt_var(ctx, out);
				case PlaKeywIf:
					return parse_stmt_cond(ctx, out);
				case PlaKeywWhile:
					return parse_stmt_pre_loop(ctx, out);
				case PlaKeywRet:
					return parse_stmt_ret(ctx, out);
			}
			break;
	}

	return parse_stmt_expr(ctx, out);
}


static bool parse_dclr(ctx_t * ctx, pla_dclr_t ** out);

static bool parse_dclr_nspc(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywNmspc)) {
		return false;
	}

	*out = pla_dclr_create(PlaDclrNspc);

	if (!consume_ident_crit(ctx, &(*out)->name)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncLeBrace)) {
		return false;
	}

	pla_dclr_t ** ins = &(*out)->nspc.body;

	while (!consume_punc_exact(ctx, PlaPuncRiBrace)) {
		if (!parse_dclr(ctx, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}
static bool parse_dclr_func(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywFnct)) {
		return false;
	}

	*out = pla_dclr_create(PlaDclrFunc);

	if (!consume_ident_crit(ctx, &(*out)->name)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncColon)) {
		return false;
	}

	if (!parse_expr(ctx, &(*out)->func.dt_expr)) {
		return false;
	}

	if (!parse_stmt_blk(ctx, &(*out)->func.block)) {
		return false;
	}

	return true;
}
static bool parse_dclr_impt(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywImprt)) {
		return false;
	}

	*out = pla_dclr_create(PlaDclrImpt);

	if (!consume_ident_crit(ctx, &(*out)->name)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncColon)) {
		return false;
	}

	if (!parse_expr(ctx, &(*out)->impt.dt_expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncColon)) {
		return false;
	}

	if (!consume_ch_str_crit(ctx, &(*out)->impt.lib_name, NULL)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncComma)) {
		return false;
	}

	if (!consume_ch_str_crit(ctx, &(*out)->impt.sym_name, NULL)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_dclr_var(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywVrbl)) {
		return false;
	}

	while (true) {
		u_hs_t * var_name;

		if (!consume_ident_crit(ctx, &var_name)) {
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncColon)) {
			*out = pla_dclr_create(PlaDclrVarDt);

			(*out)->name = var_name;

			if (!parse_expr(ctx, &(*out)->var_dt.dt_expr)) {
				return false;
			}
		}
		else if (consume_punc_exact(ctx, PlaPuncScAsgn)) {
			*out = pla_dclr_create(PlaDclrVarVal);

			(*out)->name = var_name;

			if (!parse_expr(ctx, &(*out)->var_val.val_expr)) {
				return false;
			}
		}
		else {
			report(ctx, L"failed to consume a \':\' or \':=\'");
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncComma)) {
			/* nothing */
		}
		else {
			if (consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
				break;
			}
			else {
				return false;
			}
		}

		out = &(*out)->next;
	}

	return true;
}
static bool parse_dclr(ctx_t * ctx, pla_dclr_t ** out) {
	switch (ctx->tok.type) {
		case TokNone:
			break;
		case TokKeyw:
			switch (ctx->tok.keyw) {
				case PlaKeywNmspc:
					if (!parse_dclr_nspc(ctx, out)) {
						return false;
					}
					break;
				case PlaKeywFnct:
					if (!parse_dclr_func(ctx, out)) {
						return false;
					}
					break;
				case PlaKeywImprt:
					if (!parse_dclr_impt(ctx, out)) {
						return false;
					}
					break;
				case PlaKeywVrbl:
					if (!parse_dclr_var(ctx, out)) {
						return false;
					}
					break;
				default:
					return false;
			}
			break;
		default:
			return false;
	}

	return true;
}

static bool parse_root_item(ctx_t * ctx, pla_dclr_t ** ins) {
	switch (ctx->tok.type) {
		case TokNone:
			break;
	}

	return parse_dclr(ctx, ins);
}
static bool parse_root(ctx_t * ctx, pla_dclr_t ** out) {
	*out = pla_dclr_create(PlaDclrNspc);

	(*out)->name = NULL;

	pla_dclr_t ** ins = &(*out)->nspc.body;

	while (ctx->tok.type != TokNone) {
		if (!parse_root_item(ctx, ins)) {
			report(ctx, L"failed to parse a root-declarator");
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}

static bool parse_core(ctx_t * ctx) {
	pla_ast_init(ctx->out, ctx->hst);

	if (_wfopen_s(&ctx->file, ctx->file_name, L"r") != 0) {
		return false;
	}

	if (!next_ch(ctx)) {
		return false;
	}

	if (!next_tok(ctx)) {
		return false;
	}

	if (!parse_root(ctx, &ctx->out->root)) {
		return false;
	}

	return true;
}
bool pla_ast_p_parse(u_hst_t * hst, const wchar_t * file_name, pla_ast_t * out) {
	ctx_t ctx = { .hst = hst, .file_name = file_name, .out = out };

	bool result = parse_core(&ctx);

	free(ctx.str);

	if (ctx.file != NULL) {
		fclose(ctx.file);
	}

	return result;
}
