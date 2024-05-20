#include "pch.h"
#include "pla_ec_fmtr.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_tok.h"
#include "pla_lex.h"
#include "pla_cn.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "pla_dclr.h"
#include "pla_tu.h"
#include "pla_prsr.h"
#include "ira_int.h"

typedef enum pla_tok_type tok_type_t;
typedef struct pla_tok tok_t;

typedef struct post_optr_info {
	pla_punc_t punc;
	pla_expr_type_t expr_type;
} post_optr_info_t;
typedef struct unr_optr_info {
	pla_punc_t punc;
	pla_expr_type_t expr_type;
} unr_optr_info_t;
typedef size_t bin_optr_preced_t;
typedef struct bin_optr_info {
	pla_punc_t punc;
	pla_expr_type_t expr_type;
	bin_optr_preced_t preced;
} bin_optr_info_t;


static const post_optr_info_t post_optr_infos[] = {
	{ .punc = PlaPuncDot, .expr_type = PlaExprMmbrAcc },
	{ .punc = PlaPuncLeParen, .expr_type = PlaExprCall },
	{ .punc = PlaPuncLeBrack, .expr_type = PlaExprSubscr },
	{ .punc = PlaPuncDplus, .expr_type = PlaExprPostInc },
	{ .punc = PlaPuncDminus, .expr_type = PlaExprPostDec },
	{ .punc = PlaPuncExclMark, .expr_type = PlaExprDeref },
};
static const size_t post_optr_infos_size = _countof(post_optr_infos);
static const unr_optr_info_t unr_optr_infos[] = {
	{ .punc = PlaPuncAster, .expr_type = PlaExprDtPtr },
	{ .punc = PlaPuncLeBrack, .expr_type = PlaExprDtArr },
	{ .punc = PlaPuncDplus, .expr_type = PlaExprPostInc },
	{ .punc = PlaPuncDminus, .expr_type = PlaExprPostDec },
	{ .punc = PlaPuncAmper, .expr_type = PlaExprAddrOf },
	{ .punc = PlaPuncPerc, .expr_type = PlaExprCast },
	{ .punc = PlaPuncExclMark, .expr_type = PlaExprLogicNeg },
	{ .punc = PlaPuncTilde, .expr_type = PlaExprBitNeg },
	{ .punc = PlaPuncMinus, .expr_type = PlaExprArithNeg }
};
static const size_t unr_optr_infos_size = _countof(unr_optr_infos);
static const bin_optr_info_t bin_optr_infos[] = {
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
static const size_t bin_optr_infos_size = _countof(bin_optr_infos);

static bool get_src_tok_proc_dflt(void * src_data, pla_tok_t * out) {
	return false;
}

void pla_prsr_init(pla_prsr_t * prsr, pla_ec_fmtr_t * ec_fmtr) {
	*prsr = (pla_prsr_t){ .ec_fmtr = ec_fmtr, .get_tok_proc = get_src_tok_proc_dflt };
}
void pla_prsr_cleanup(pla_prsr_t * prsr) {
	memset(prsr, 0, sizeof(*prsr));
}


static void push_rse(pla_prsr_t * prsr, pla_prsr_rse_t * rse) {
	rse->prev = prsr->rse;

	if (rse->prev != NULL) {
		rse->is_rptd = rse->prev->is_rptd;
	}

	prsr->rse = rse;
}
static void pop_rse(pla_prsr_t * prsr) {
	bool is_rptd = prsr->rse->is_rptd;

	prsr->rse = prsr->rse->prev;

	if (is_rptd && prsr->rse != NULL && prsr->rse->get_next) {
		prsr->rse->is_rptd = true;
	}
}


static void report(pla_prsr_t * prsr, const wchar_t * fmt, ...) {
	prsr->is_rptd = true;

	ul_assert(prsr->rse != NULL);

	if (prsr->rse->is_rptd) {
		return;
	}

	prsr->rse->is_rptd = true;

	va_list args;

	va_start(args, fmt);

	pla_ec_pos_t pos_end = prsr->prev_tok_pos_end;
	pos_end.line_ch += 1;

	pla_ec_fmtr_formatpost_va(prsr->ec_fmtr, PLA_PRSR_EC_GROUP, prsr->prev_tok_pos_end, pos_end, fmt, args);

	va_end(args);
}


static bool next_tok(pla_prsr_t * prsr) {
	if (prsr->tok.type != PlaTokNone) {
		prsr->prev_tok_pos_end = prsr->tok.pos_end;
	}

	prsr->tok.type = PlaTokNone;

	if (!prsr->get_tok_proc(prsr->src_data, &prsr->tok)) {
		return false;
	}

	++prsr->tok_ind;

	return true;
}


static pla_punc_t get_punc(pla_prsr_t * prsr) {
	if (prsr->tok.type == PlaTokPunc) {
		return prsr->tok.punc;
	}

	return PlaPuncNone;
}
static bool consume_punc_exact(pla_prsr_t * prsr, pla_punc_t punc) {
	if (get_punc(prsr) == punc) {
		next_tok(prsr);
		return true;
	}

	return false;
}
static void consume_punc_exact_crit(pla_prsr_t * prsr, pla_punc_t punc) {
	ul_assert(punc < PlaPunc_Count);

	if (!consume_punc_exact(prsr, punc)) {
		report(prsr, L"expected a(n) \'%s\'", pla_punc_strs[punc].str);
	}
}
static pla_keyw_t get_keyw(pla_prsr_t * prsr) {
	if (prsr->tok.type == PlaTokKeyw) {
		return prsr->tok.keyw;
	}

	return PlaKeywNone;
}
static bool consume_keyw_exact(pla_prsr_t * prsr, pla_keyw_t keyw) {
	if (get_keyw(prsr) == keyw) {
		next_tok(prsr);
		return true;
	}

	return false;
}
static void consume_keyw_exact_crit(pla_prsr_t * prsr, pla_keyw_t keyw) {
	ul_assert(keyw < PlaKeyw_Count);

	if (!consume_keyw_exact(prsr, keyw)) {
		report(prsr, L"expected a(n) \'%s\'", pla_keyw_strs[keyw].str);
	}
}
static bool consume_ident(pla_prsr_t * prsr, ul_hs_t ** out) {
	if (prsr->tok.type == PlaTokIdent) {
		*out = prsr->tok.ident;
		next_tok(prsr);
		return true;
	}

	return false;
}
static void consume_ident_crit(pla_prsr_t * prsr, ul_hs_t ** out) {
	if (!consume_ident(prsr, out)) {
		report(prsr, L"expected an identifier");
	}
}
static void consume_ch_str_crit(pla_prsr_t * prsr, ul_hs_t ** out_data, ul_hs_t ** out_tag) {
	if (prsr->tok.type == PlaTokChStr) {
		*out_data = prsr->tok.ch_str.data;
		if (out_tag != NULL) {
			*out_tag = prsr->tok.ch_str.tag;
		}
		next_tok(prsr);
	}
	else {
		report(prsr, L"expected a character string");
	}
}

static const post_optr_info_t * get_post_optr_info(pla_punc_t punc) {
	for (size_t i = 0; i < post_optr_infos_size; ++i) {
		if (post_optr_infos[i].punc == punc) {
			return &post_optr_infos[i];
		}
	}

	return NULL;
}
static const unr_optr_info_t * get_unr_optr_info(pla_punc_t punc) {
	for (size_t i = 0; i < unr_optr_infos_size; ++i) {
		if (unr_optr_infos[i].punc == punc) {
			return &unr_optr_infos[i];
		}
	}

	return NULL;
}
static const bin_optr_info_t * get_bin_optr_info(pla_punc_t punc) {
	for (size_t i = 0; i < bin_optr_infos_size; ++i) {
		if (bin_optr_infos[i].punc == punc) {
			return &bin_optr_infos[i];
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
			ul_assert_unreachable();
	}
}


void pla_prsr_set_src(pla_prsr_t * prsr, void * src_data, pla_prsr_get_tok_proc_t * get_tok_proc) {
	prsr->src_data = src_data;
	prsr->get_tok_proc = get_tok_proc;

	next_tok(prsr);

	prsr->prev_tok_pos_end = (pla_ec_pos_t){ .line_num = 0, .line_ch = 0 };
	prsr->tok_ind = 0;
}
void pla_prsr_reset_src(pla_prsr_t * prsr) {
	pla_prsr_set_src(prsr, NULL, get_src_tok_proc_dflt);
}


static void parse_quals(pla_prsr_t * prsr, ira_dt_qual_t * out) {
	*out = ira_dt_qual_none;

	while (true) {
		if (consume_keyw_exact(prsr, PlaKeywConst)) {
			out->const_q = true;
			continue;
		}

		break;
	}
}


static void parse_cn_rse(pla_prsr_t * prsr, pla_punc_t delim, pla_cn_t ** out) {
	do {
		*out = pla_cn_create();

		consume_ident_crit(prsr, &(*out)->name);

		out = &(*out)->sub_name;
	} while (consume_punc_exact(prsr, delim));
}
static void parse_cn(pla_prsr_t * prsr, pla_punc_t delim, pla_cn_t ** out) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	__try {
		parse_cn_rse(prsr, delim, out);
	}
	__finally {
		pop_rse(prsr);
	}
}
bool pla_prsr_parse_cn(pla_prsr_t * prsr, pla_punc_t delim, pla_cn_t ** out) {
	prsr->is_rptd = false;

	parse_cn(prsr, delim, out);

	return !prsr->is_rptd;
}


static void parse_expr(pla_prsr_t * prsr, pla_expr_t ** out);

static void parse_expr_stct_body(pla_prsr_t * prsr, pla_expr_t ** elem_out) {
	consume_punc_exact_crit(prsr, PlaPuncLeBrace);

	while (true) {
		*elem_out = pla_expr_create(PlaExprDtStctElem);

		consume_ident_crit(prsr, &(*elem_out)->opd2.hs);

		consume_punc_exact_crit(prsr, PlaPuncColon);

		parse_expr(prsr, &(*elem_out)->opd0.expr);

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncRiBrace);

			break;
		}

		elem_out = &(*elem_out)->opd1.expr;
	}
}

static void parse_expr_post(pla_prsr_t * prsr, pla_expr_t ** out) {
	while (true) {
		const post_optr_info_t * info = get_post_optr_info(get_punc(prsr));

		if (info == NULL) {
			break;
		}

		next_tok(prsr);

		pla_expr_t * expr = pla_expr_create(info->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		switch (info->expr_type) {
			case PlaExprMmbrAcc:
				consume_ident_crit(prsr, &(*out)->opd1.hs);
				break;
			case PlaExprCall:
				if (!consume_punc_exact(prsr, PlaPuncRiParen)) {
					pla_expr_t ** arg = &(*out)->opd1.expr;

					while (true) {
						*arg = pla_expr_create(PlaExprCallArg);

						parse_expr(prsr, &(*arg)->opd0.expr);

						if (consume_punc_exact(prsr, PlaPuncComma)) {
							(void)0;
						}
						else {
							consume_punc_exact_crit(prsr, PlaPuncRiParen);

							break;
						}

						arg = &(*arg)->opd1.expr;
					}
				}
				break;
			case PlaExprSubscr:
				parse_expr(prsr, &(*out)->opd1.expr);

				consume_punc_exact_crit(prsr, PlaPuncRiBrack);
				break;
			case PlaExprDeref:
			{
				ul_hs_t * ident;

				if (consume_ident(prsr, &ident)) {
					expr = pla_expr_create(PlaExprMmbrAcc);

					expr->opd0.expr = *out;
					*out = expr;

					expr->opd1.hs = ident;
				}
				break;
			}
		}
	}
}
static void parse_expr_unit(pla_prsr_t * prsr, pla_expr_t ** out) {
	bool success = true;

	switch (prsr->tok.type) {
		case PlaTokPunc:
			switch (prsr->tok.punc) {
				case PlaPuncLeParen:
					next_tok(prsr);

					parse_expr(prsr, out);

					consume_punc_exact_crit(prsr, PlaPuncRiParen);
					break;
				case PlaPuncDolrSign:
					next_tok(prsr);

					consume_punc_exact_crit(prsr, PlaPuncLeParen);

					*out = pla_expr_create(PlaExprDtFunc);

					if (!consume_punc_exact(prsr, PlaPuncRiParen)) {
						pla_expr_t ** arg = &(*out)->opd1.expr;

						while (true) {
							*arg = pla_expr_create(PlaExprDtFuncArg);

							consume_ident_crit(prsr, &(*arg)->opd2.hs);

							consume_punc_exact_crit(prsr, PlaPuncColon);

							parse_expr(prsr, &(*arg)->opd0.expr);

							if (consume_punc_exact(prsr, PlaPuncComma)) {
								(void)0;
							}
							else {
								consume_punc_exact_crit(prsr, PlaPuncRiParen);

								break;
							}

							arg = &(*arg)->opd1.expr;
						}
					}

					if (consume_punc_exact(prsr, PlaPuncRiArrow)) {
						parse_expr(prsr, &(*out)->opd0.expr);
					}
					else {
						(*out)->opd0.expr = pla_expr_create(PlaExprDtVoid);
					}

					break;
				default:
					success = false;
					break;
			}
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywVoid:
					*out = pla_expr_create(PlaExprDtVoid);
					next_tok(prsr);
					break;
				case PlaKeywBool:
					*out = pla_expr_create(PlaExprDtBool);
					next_tok(prsr);
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
					(*out)->opd0.int_type = get_int_type(prsr->tok.keyw);
					next_tok(prsr);
					break;
				case PlaKeywStruct:
					next_tok(prsr);

					*out = pla_expr_create(PlaExprDtStct);

					parse_expr_stct_body(prsr, &(*out)->opd1.expr);

					break;

				case PlaKeywVoidval:
					*out = pla_expr_create(PlaExprValVoid);
					next_tok(prsr);
					break;
				case PlaKeywTrue:
					*out = pla_expr_create(PlaExprValBool);
					(*out)->opd0.boolean = true;
					next_tok(prsr);
					break;
				case PlaKeywFalse:
					*out = pla_expr_create(PlaExprValBool);
					(*out)->opd0.boolean = false;
					next_tok(prsr);
					break;
				case PlaKeywNullof:
					*out = pla_expr_create(PlaExprNullofDt);

					next_tok(prsr);

					consume_punc_exact_crit(prsr, PlaPuncLeBrack);

					parse_expr(prsr, &(*out)->opd0.expr);

					consume_punc_exact_crit(prsr, PlaPuncRiBrack);
					break;

				default:
					success = false;
					break;
			}
			break;
		case PlaTokIdent:
			*out = pla_expr_create(PlaExprIdent);
			parse_cn(prsr, PlaPuncDcolon, &(*out)->opd0.cn);
			break;
		case PlaTokChStr:
			*out = pla_expr_create(PlaExprChStr);
			(*out)->opd0.hs = prsr->tok.ch_str.data;
			(*out)->opd1.hs = prsr->tok.ch_str.tag;
			next_tok(prsr);
			break;
		case PlaTokNumStr:
			*out = pla_expr_create(PlaExprNumStr);
			(*out)->opd0.hs = prsr->tok.num_str.data;
			(*out)->opd1.hs = prsr->tok.num_str.tag;
			next_tok(prsr);
			break;
		default:
			success = false;
			break;
	}

	if (!success) {
		report(prsr, L"expected an unit-expression");
	}

	parse_expr_post(prsr, out);
}
static void parse_expr_unr(pla_prsr_t * prsr, pla_expr_t ** out) {
	while (true) {
		bool success = true;

		switch (prsr->tok.type) {
			case PlaTokPunc:
			{
				const unr_optr_info_t * info = get_unr_optr_info(prsr->tok.punc);

				if (info == NULL) {
					success = false;
					break;
				}

				next_tok(prsr);

				*out = pla_expr_create(info->expr_type);

				switch (info->expr_type) {
					case PlaExprDtArr:
						if (!consume_punc_exact(prsr, PlaPuncRiBrack)) {
							parse_expr(prsr, &(*out)->opd1.expr);

							consume_punc_exact_crit(prsr, PlaPuncRiBrack);
						}
						else {
							(*out)->opd1.expr = pla_expr_create(PlaExprValVoid);
						}

						break;
					case PlaExprCast:
						consume_punc_exact_crit(prsr, PlaPuncLeBrack);

						parse_expr(prsr, &(*out)->opd1.expr);

						consume_punc_exact_crit(prsr, PlaPuncRiBrack);
						break;
				}

				out = &(*out)->opd0.expr;

				break;
			}
			case PlaTokKeyw:
				switch (prsr->tok.keyw) {
					case PlaKeywConst:
						next_tok(prsr);

						*out = pla_expr_create(PlaExprDtConst);

						out = &(*out)->opd0.expr;

						break;
					default:
						success = false;
						break;
				}
				break;
			default:
				success = false;
				break;
		}

		if (success == false) {
			break;
		}
	}

	parse_expr_unit(prsr, out);
}
static void parse_expr_bin_core(pla_prsr_t * prsr, pla_expr_t ** out, const bin_optr_info_t ** info) {
	bin_optr_preced_t preced = (*info)->preced;

	do {
		pla_expr_t * expr = pla_expr_create((*info)->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		parse_expr_unr(prsr, &(*out)->opd1.expr);

		*info = get_bin_optr_info(get_punc(prsr));

		if (*info != NULL) {
			next_tok(prsr);
		}

		while (*info != NULL && preced < (*info)->preced) {
			parse_expr_bin_core(prsr, &(*out)->opd1.expr, info);
		}
	} while (*info != NULL && preced == (*info)->preced);
}
static void parse_expr_bin(pla_prsr_t * prsr, pla_expr_t ** out) {
	const bin_optr_info_t * info = get_bin_optr_info(get_punc(prsr));

	if (info != NULL) {
		next_tok(prsr);

		do {
			parse_expr_bin_core(prsr, out, &info);
		} while (info != NULL);
	}
}
static void parse_expr_asgn(pla_prsr_t * prsr, pla_expr_t ** out) {
	while (true) {
		parse_expr_unr(prsr, out);

		pla_expr_type_t expr_type = PlaExprNone;

		switch (get_punc(prsr)) {
			case PlaPuncAsgn:
				expr_type = PlaExprAsgn;
				break;
		}

		if (expr_type == PlaExprNone) {
			parse_expr_bin(prsr, out);
			break;
		}

		next_tok(prsr);

		pla_expr_t * expr = pla_expr_create(expr_type);

		expr->opd0.expr = *out;
		*out = expr;
		out = &expr->opd1.expr;
	}
}
static void parse_expr_rse(pla_prsr_t * prsr, pla_expr_t ** out) {
	parse_expr_asgn(prsr, out);
}
static void parse_expr(pla_prsr_t * prsr, pla_expr_t ** out) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	__try {
		parse_expr_rse(prsr, out);
	}
	__finally {
		pop_rse(prsr);
	}
}
bool pla_prsr_parse_expr(pla_prsr_t * prsr, pla_expr_t ** out) {
	prsr->is_rptd = false;

	parse_expr(prsr, out);

	return !prsr->is_rptd;
}


static void parse_stmt(pla_prsr_t * prsr, pla_stmt_t ** out);

static void parse_stmt_blk(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_punc_exact_crit(prsr, PlaPuncLeBrace);

	*out = pla_stmt_create(PlaStmtBlk);

	size_t tok_ind = prsr->tok_ind;

	for (pla_stmt_t ** ins = &(*out)->blk.body; !consume_punc_exact(prsr, PlaPuncRiBrace); ) {
		parse_stmt(prsr, ins);

		if (tok_ind == prsr->tok_ind) {
			break;
		}

		tok_ind = prsr->tok_ind;

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}
}

static void parse_stmt_expr(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtExpr);

	parse_expr(prsr, &(*out)->expr.expr);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_stmt_var(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywVariable);

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(prsr, &qual);

		ul_hs_t * var_name;

		consume_ident_crit(prsr, &var_name);

		if (consume_punc_exact(prsr, PlaPuncColonAsgn)) {
			*out = pla_stmt_create(PlaStmtVarVal);

			(*out)->var_val.name = var_name;
			(*out)->var_val.dt_qual = qual;

			parse_expr(prsr, &(*out)->var_val.val_expr);
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncColon);

			*out = pla_stmt_create(PlaStmtVarDt);

			(*out)->var_dt.name = var_name;
			(*out)->var_dt.dt_qual = qual;

			parse_expr(prsr, &(*out)->var_dt.dt_expr);
		}

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncSemicolon);

			break;
		}

		out = &(*out)->next;
	}
}
static void parse_stmt_cond(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywIf);

	*out = pla_stmt_create(PlaStmtCond);

	parse_expr(prsr, &(*out)->cond.cond_expr);

	parse_stmt_blk(prsr, &(*out)->cond.true_br);

	if (consume_keyw_exact(prsr, PlaKeywElse)) {
		if (get_keyw(prsr) == PlaKeywIf) {
			parse_stmt_cond(prsr, &(*out)->cond.false_br);
		}
		else {
			parse_stmt_blk(prsr, &(*out)->cond.false_br);
		}
	}
}
static void parse_stmt_pre_loop(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywWhile);

	*out = pla_stmt_create(PlaStmtPreLoop);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		consume_ident_crit(prsr, &(*out)->pre_loop.name);
	}

	parse_expr(prsr, &(*out)->pre_loop.cond_expr);

	parse_stmt_blk(prsr, &(*out)->pre_loop.body);
}
static void parse_stmt_post_loop(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywDo);

	*out = pla_stmt_create(PlaStmtPostLoop);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		consume_ident_crit(prsr, &(*out)->post_loop.name);
	}

	parse_stmt_blk(prsr, &(*out)->post_loop.body);

	consume_keyw_exact_crit(prsr, PlaKeywWhile);

	parse_expr(prsr, &(*out)->post_loop.cond_expr);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_stmt_brk(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywBreak);

	*out = pla_stmt_create(PlaStmtBrk);

	consume_ident(prsr, &(*out)->brk.name);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_stmt_cnt(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywContinue);

	*out = pla_stmt_create(PlaStmtCnt);

	consume_ident(prsr, &(*out)->cnt.name);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_stmt_ret(pla_prsr_t * prsr, pla_stmt_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywRet);

	*out = pla_stmt_create(PlaStmtRet);

	if (!consume_punc_exact(prsr, PlaPuncSemicolon)) {
		parse_expr(prsr, &(*out)->ret.expr);

		consume_punc_exact_crit(prsr, PlaPuncSemicolon);
	}
	else {
		(*out)->ret.expr = pla_expr_create(PlaExprValVoid);
	}
}
static void parse_stmt_rse(pla_prsr_t * prsr, pla_stmt_t ** out) {
	switch (prsr->tok.type) {
		case PlaTokPunc:
			switch (prsr->tok.punc) {
				case PlaPuncSemicolon:
					next_tok(prsr);
					return;
			}
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywVariable:
					parse_stmt_var(prsr, out);
					return;
				case PlaKeywIf:
					parse_stmt_cond(prsr, out);
					return;
				case PlaKeywWhile:
					parse_stmt_pre_loop(prsr, out);
					return;
				case PlaKeywDo:
					parse_stmt_post_loop(prsr, out);
					return;
				case PlaKeywBreak:
					parse_stmt_brk(prsr, out);
					return;
				case PlaKeywContinue:
					parse_stmt_cnt(prsr, out);
					return;
				case PlaKeywRet:
					parse_stmt_ret(prsr, out);
					return;
			}
			break;
	}

	if (prsr->tok.type == PlaTokNone) {
		report(prsr, L"expected a statement");
	}
	else {
		parse_stmt_expr(prsr, out);
	}
}
static void parse_stmt(pla_prsr_t * prsr, pla_stmt_t ** out) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	__try {
		parse_stmt_rse(prsr, out);
	}
	__finally {
		pop_rse(prsr);
	}
}
bool pla_prsr_parse_stmt(pla_prsr_t * prsr, pla_stmt_t ** out) {
	prsr->is_rptd = false;

	parse_stmt(prsr, out);

	return !prsr->is_rptd;
}


static void parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out);

static void parse_dclr_nspc(pla_prsr_t * prsr, pla_dclr_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywNamespace);

	*out = pla_dclr_create(PlaDclrNspc);

	consume_ident_crit(prsr, &(*out)->name);

	consume_punc_exact_crit(prsr, PlaPuncLeBrace);

	size_t tok_ind = prsr->tok_ind;

	for (pla_dclr_t ** ins = &(*out)->nspc.body; !consume_punc_exact(prsr, PlaPuncRiBrace);) {
		parse_dclr(prsr, ins);

		if (tok_ind == prsr->tok_ind) {
			break;
		}

		tok_ind = prsr->tok_ind;

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}
}
static void parse_dclr_func(pla_prsr_t * prsr, pla_dclr_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywFunction);

	*out = pla_dclr_create(PlaDclrFunc);

	consume_ident_crit(prsr, &(*out)->name);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	parse_expr(prsr, &(*out)->func.dt_expr);

	parse_stmt_blk(prsr, &(*out)->func.block);
}
static void parse_dclr_impt(pla_prsr_t * prsr, pla_dclr_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywImport);

	*out = pla_dclr_create(PlaDclrImpt);

	consume_ident_crit(prsr, &(*out)->name);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	parse_expr(prsr, &(*out)->impt.dt_expr);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	consume_ch_str_crit(prsr, &(*out)->impt.lib_name, NULL);

	consume_punc_exact_crit(prsr, PlaPuncComma);

	consume_ch_str_crit(prsr, &(*out)->impt.sym_name, NULL);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_dclr_var(pla_prsr_t * prsr, pla_dclr_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywVariable);

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(prsr, &qual);

		ul_hs_t * var_name;

		consume_ident_crit(prsr, &var_name);

		if (consume_punc_exact(prsr, PlaPuncColonAsgn)) {
			*out = pla_dclr_create(PlaDclrVarVal);

			(*out)->name = var_name;
			(*out)->var_val.dt_qual = qual;

			parse_expr(prsr, &(*out)->var_val.val_expr);
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncColon);

			*out = pla_dclr_create(PlaDclrVarDt);

			(*out)->name = var_name;
			(*out)->var_dt.dt_qual = qual;

			parse_expr(prsr, &(*out)->var_dt.dt_expr);
		}

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncSemicolon);

			break;
		}

		out = &(*out)->next;
	}
}
static void parse_dclr_stct(pla_prsr_t * prsr, pla_dclr_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywStruct);

	ul_hs_t * name;

	consume_ident_crit(prsr, &name);

	if (get_punc(prsr) == PlaPuncLeBrace) {
		*out = pla_dclr_create(PlaDclrDtStct);

		(*out)->name = name;

		(*out)->dt_stct.dt_stct_expr = pla_expr_create(PlaExprDtStct);

		parse_expr_stct_body(prsr, &(*out)->dt_stct.dt_stct_expr->opd1.expr);
	}
	else {
		*out = pla_dclr_create(PlaDclrDtStctDecl);

		(*out)->name = name;
	}

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_dclr_ro_val(pla_prsr_t * prsr, pla_dclr_t ** out) {
	consume_keyw_exact_crit(prsr, PlaKeywReadonly);

	while (true) {
		*out = pla_dclr_create(PlaDclrRoVal);

		consume_ident_crit(prsr, &(*out)->name);

		consume_punc_exact_crit(prsr, PlaPuncColonAsgn);

		parse_expr(prsr, &(*out)->ro_val.val_expr);

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncSemicolon);

			break;
		}

		out = &(*out)->next;
	}
}
static void parse_dclr_rse(pla_prsr_t * prsr, pla_dclr_t ** out) {
	switch (prsr->tok.type) {
		case PlaTokNone:
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywNamespace:
					parse_dclr_nspc(prsr, out);
					return;
				case PlaKeywFunction:
					parse_dclr_func(prsr, out);
					return;
				case PlaKeywImport:
					parse_dclr_impt(prsr, out);
					return;
				case PlaKeywVariable:
					parse_dclr_var(prsr, out);
					return;
				case PlaKeywStruct:
					parse_dclr_stct(prsr, out);
					return;
				case PlaKeywReadonly:
					parse_dclr_ro_val(prsr, out);
					return;
			}
			break;
	}

	report(prsr, L"expected a declarator");
}
static void parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out) {
	pla_prsr_rse_t rse = { .get_next = true };

	push_rse(prsr, &rse);

	__try {
		parse_dclr_rse(prsr, out);
	}
	__finally {
		pop_rse(prsr);
	}
}

bool pla_prsr_parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out) {
	prsr->is_rptd = false;

	parse_dclr(prsr, out);

	return !prsr->is_rptd;
}


static void parse_tu_ref(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	consume_keyw_exact_crit(prsr, PlaKeywTlatuRef);

	pla_tu_ref_t * ref = pla_tu_push_ref(tu);

	if (consume_punc_exact(prsr, PlaPuncDot)) {
		ref->is_rel = true;
	}

	parse_cn(prsr, PlaPuncDot, &ref->cn);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);
}
static void parse_tu_item_rse(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	switch (prsr->tok.type) {
		case PlaTokNone:
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywTlatuRef:
					parse_tu_ref(prsr, tu, ins);
					return;
			}
			break;
	}

	parse_dclr(prsr, ins);
}
static void parse_tu_item(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	__try {
		parse_tu_item_rse(prsr, tu, ins);
	}
	__finally {
		pop_rse(prsr);
	}
}
static void parse_tu_body(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	size_t tok_ind = prsr->tok_ind;
	
	while (prsr->tok.type != PlaTokNone) {
		parse_tu_item(prsr, tu, ins);

		if (tok_ind == prsr->tok_ind) {
			break;
		}

		tok_ind = prsr->tok_ind;

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}
}
static void parse_tu(pla_prsr_t * prsr, pla_tu_t * tu) {
	pla_dclr_t ** root = &tu->root;

	if (*root == NULL) {
		*root = pla_dclr_create(PlaDclrNspc);

		(*root)->name = NULL;
	}

	pla_dclr_t ** ins = &(*root)->nspc.body;

	while (*ins != NULL) {
		ins = &(*ins)->next;
	}

	parse_tu_body(prsr, tu, ins);
}
bool pla_prsr_parse_tu(pla_prsr_t * prsr, pla_tu_t * tu) {
	prsr->is_rptd = false;

	parse_tu(prsr, tu);

	return !prsr->is_rptd;
}
