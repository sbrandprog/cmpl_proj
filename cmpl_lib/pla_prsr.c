#include "pla_ec.h"
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


void pla_prsr_init(pla_prsr_t * prsr, ul_ec_fmtr_t * ec_fmtr) {
	*prsr = (pla_prsr_t){ .ec_fmtr = ec_fmtr };
}
void pla_prsr_cleanup(pla_prsr_t * prsr) {
	memset(prsr, 0, sizeof(*prsr));
}


static void push_rse(pla_prsr_t * prsr, pla_prsr_rse_t * rse) {
	rse->prev = prsr->rse;

	if (rse->prev != NULL && rse->prev->set_next) {
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

	pla_ec_formatpost_va(prsr->ec_fmtr, PLA_PRSR_MOD_NAME, (prsr->src != NULL ? prsr->src->name->str : NULL), prsr->tok.pos_start, prsr->tok.pos_end, fmt, args);

	va_end(args);
}


static bool next_tok(pla_prsr_t * prsr) {
	if (prsr->tok.type != PlaTokNone) {
		prsr->prev_tok_pos_start = prsr->tok.pos_start;
		prsr->prev_tok_pos_end = prsr->tok.pos_end;
	}

	prsr->tok.type = PlaTokNone;

	if (prsr->src == NULL || !prsr->src->get_tok_proc(prsr->src->data, &prsr->tok)) {
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


void pla_prsr_set_src(pla_prsr_t * prsr, const pla_prsr_src_t * src) {
	prsr->src = src;

	next_tok(prsr);

	prsr->prev_tok_pos_start = (pla_ec_pos_t){ .line_num = 0, .line_ch = 0 };
	prsr->prev_tok_pos_end = (pla_ec_pos_t){ .line_num = 0, .line_ch = 0 };
	prsr->tok_ind = 0;
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
	*out = pla_cn_create();

	(*out)->pos_start = prsr->tok.pos_start;

	consume_ident_crit(prsr, &(*out)->name);

	if (consume_punc_exact(prsr, delim)) {
		parse_cn_rse(prsr, delim, &(*out)->sub_name);
	}

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_cn(pla_prsr_t * prsr, pla_punc_t delim, pla_cn_t ** out) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	parse_cn_rse(prsr, delim, out);

	pop_rse(prsr);
}


static void parse_expr(pla_prsr_t * prsr, pla_expr_t ** out);

static void parse_expr_tpl_body(pla_prsr_t * prsr, pla_expr_t ** elem_out) {
	consume_punc_exact_crit(prsr, PlaPuncLeBrack);

	while (true) {
		*elem_out = pla_expr_create(PlaExprDtTplElem);

		(*elem_out)->pos_start = prsr->tok.pos_start;

		consume_ident_crit(prsr, &(*elem_out)->opd2.hs);

		consume_punc_exact_crit(prsr, PlaPuncColon);

		parse_expr(prsr, &(*elem_out)->opd0.expr);

		(*elem_out)->pos_end = prsr->prev_tok_pos_end;

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncRiBrack);

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

		pla_expr_t * expr = pla_expr_create(info->expr_type);

		expr->opd0.expr = *out;
		expr->pos_start = prsr->tok.pos_start;
		*out = expr;

		next_tok(prsr);

		switch (info->expr_type) {
			case PlaExprMmbrAcc:
				consume_ident_crit(prsr, &(*out)->opd1.hs);
				break;
			case PlaExprCall:
				if (!consume_punc_exact(prsr, PlaPuncRiParen)) {
					pla_expr_t ** arg = &(*out)->opd1.expr;

					while (true) {
						*arg = pla_expr_create(PlaExprCallArg);

						(*arg)->pos_start = prsr->tok.pos_start;

						parse_expr(prsr, &(*arg)->opd0.expr);

						(*arg)->pos_end = prsr->tok.pos_end;

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

				(*out)->pos_end = prsr->prev_tok_pos_end;

				if (consume_ident(prsr, &ident)) {
					expr = pla_expr_create(PlaExprMmbrAcc);

					expr->opd0.expr = *out;
					expr->pos_start = (*out)->pos_start;
					*out = expr;

					expr->opd1.hs = ident;
				}
				break;
			}
		}

		(*out)->pos_end = prsr->prev_tok_pos_end;
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
					*out = pla_expr_create(PlaExprDtFunc);

					(*out)->pos_start = prsr->tok.pos_start;

					next_tok(prsr);

					consume_punc_exact_crit(prsr, PlaPuncLeParen);

					if (!consume_punc_exact(prsr, PlaPuncRiParen)) {
						pla_expr_t ** arg = &(*out)->opd1.expr;

						while (true) {
							if (get_keyw(prsr) == PlaKeywVararg) {
								*arg = pla_expr_create(PlaExprDtFuncVa);

								(*arg)->pos_start = prsr->tok.pos_start;

								next_tok(prsr);

								consume_punc_exact_crit(prsr, PlaPuncDot);

								consume_ident(prsr, &(*arg)->opd0.hs);

								consume_punc_exact_crit(prsr, PlaPuncRiParen);

								(*arg)->pos_end = prsr->prev_tok_pos_end;

								break;
							}

							*arg = pla_expr_create(PlaExprDtFuncArg);

							(*arg)->pos_start = prsr->tok.pos_start;

							consume_ident_crit(prsr, &(*arg)->opd2.hs);

							consume_punc_exact_crit(prsr, PlaPuncColon);

							parse_expr(prsr, &(*arg)->opd0.expr);

							(*arg)->pos_end = prsr->prev_tok_pos_end;

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

					(*out)->pos_end = prsr->prev_tok_pos_end;

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
					(*out)->pos_start = prsr->tok.pos_start;
					(*out)->pos_end = prsr->tok.pos_end;
					next_tok(prsr);
					break;
				case PlaKeywBool:
					*out = pla_expr_create(PlaExprDtBool);
					(*out)->pos_start = prsr->tok.pos_start;
					(*out)->pos_end = prsr->tok.pos_end;
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
					(*out)->pos_start = prsr->tok.pos_start;
					(*out)->pos_end = prsr->tok.pos_end;
					next_tok(prsr);
					break;
				case PlaKeywTuple:
					*out = pla_expr_create(PlaExprDtTpl);

					(*out)->pos_start = prsr->tok.pos_start;

					next_tok(prsr);

					parse_expr_tpl_body(prsr, &(*out)->opd1.expr);

					(*out)->pos_end = prsr->prev_tok_pos_end;
					
					break;

				case PlaKeywVararg:
				{
					pla_ec_pos_t pos_start = prsr->tok.pos_start;

					next_tok(prsr);

					if (consume_punc_exact(prsr, PlaPuncDot)) {
						*out = pla_expr_create(PlaExprVaStart);

						(*out)->pos_start = pos_start;

						next_tok(prsr);

						consume_punc_exact_crit(prsr, PlaPuncLeParen);

						consume_punc_exact_crit(prsr, PlaPuncRiParen);

						(*out)->pos_end = prsr->prev_tok_pos_end;
					}
					else {
						*out = pla_expr_create(PlaExprVaArg);

						(*out)->pos_start = pos_start;

						consume_punc_exact_crit(prsr, PlaPuncLeBrack);

						parse_expr(prsr, &(*out)->opd0.expr);

						consume_punc_exact_crit(prsr, PlaPuncRiBrack);

						consume_punc_exact_crit(prsr, PlaPuncLeParen);

						parse_expr(prsr, &(*out)->opd1.expr);

						consume_punc_exact_crit(prsr, PlaPuncRiParen);

						(*out)->pos_end = prsr->tok.pos_end;
					}

					break;
				}

				case PlaKeywNull:
					*out = pla_expr_create(PlaExprValVoid);
					(*out)->pos_start = prsr->tok.pos_start;
					(*out)->pos_end = prsr->tok.pos_end;
					next_tok(prsr);
					break;
				case PlaKeywTrue:
				case PlaKeywFalse:
					*out = pla_expr_create(PlaExprValBool);
					(*out)->opd0.boolean = prsr->tok.keyw == PlaKeywTrue ? true : false;
					(*out)->pos_start = prsr->tok.pos_start;
					(*out)->pos_end = prsr->tok.pos_end;
					next_tok(prsr);
					break;

				default:
					success = false;
					break;
			}
			break;
		case PlaTokIdent:
			*out = pla_expr_create(PlaExprIdent);
			(*out)->pos_start = prsr->tok.pos_start;
			parse_cn(prsr, PlaPuncDcolon, &(*out)->opd0.cn);
			(*out)->pos_end = prsr->prev_tok_pos_end;
			break;
		case PlaTokChStr:
			*out = pla_expr_create(PlaExprChStr);
			(*out)->opd0.hs = prsr->tok.ch_str.data;
			(*out)->opd1.hs = prsr->tok.ch_str.tag;
			(*out)->pos_start = prsr->tok.pos_start;
			(*out)->pos_end = prsr->tok.pos_end;
			next_tok(prsr);
			break;
		case PlaTokNumStr:
			*out = pla_expr_create(PlaExprNumStr);
			(*out)->opd0.hs = prsr->tok.num_str.data;
			(*out)->opd1.hs = prsr->tok.num_str.tag;
			(*out)->pos_start = prsr->tok.pos_start;
			(*out)->pos_end = prsr->tok.pos_end;
			next_tok(prsr);
			break;
		default:
			success = false;
			break;
	}

	if (!success) {
		*out = pla_expr_create(PlaExprValVoid);
		(*out)->pos_start = prsr->tok.pos_start;
		(*out)->pos_end = prsr->tok.pos_end;
		report(prsr, L"expected an expression");
	}

	parse_expr_post(prsr, out);
}
static void parse_expr_unr(pla_prsr_t * prsr, pla_expr_t ** out) {
	pla_expr_t ** next_out = NULL;

	switch (prsr->tok.type) {
		case PlaTokPunc:
		{
			const unr_optr_info_t * info = get_unr_optr_info(prsr->tok.punc);

			if (info == NULL) {
				break;
			}

			*out = pla_expr_create(info->expr_type);

			(*out)->pos_start = prsr->tok.pos_start;

			next_out = &(*out)->opd0.expr;

			next_tok(prsr);

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

			break;
		}
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywConst:
					*out = pla_expr_create(PlaExprDtConst);

					(*out)->pos_start = prsr->tok.pos_start;

					next_out = &(*out)->opd0.expr;

					next_tok(prsr);

					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	if (next_out != NULL) {
		parse_expr_unr(prsr, next_out);

		(*out)->pos_end = prsr->prev_tok_pos_end;
	}
	else {
		parse_expr_unit(prsr, out);
	}
}
static void parse_expr_bin_core(pla_prsr_t * prsr, pla_expr_t ** out, const bin_optr_info_t ** info) {
	bin_optr_preced_t preced = (*info)->preced;

	do {
		pla_expr_t * expr = pla_expr_create((*info)->expr_type);

		expr->pos_start = (*out)->pos_start;
		expr->opd0.expr = *out;
		*out = expr;

		parse_expr_unr(prsr, &(*out)->opd1.expr);

		(*out)->pos_end = prsr->prev_tok_pos_end;

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
	pla_ec_pos_t pos_start = prsr->tok.pos_start;

	parse_expr_unr(prsr, out);

	pla_expr_type_t expr_type = PlaExprNone;

	switch (get_punc(prsr)) {
		case PlaPuncAsgn:
			expr_type = PlaExprAsgn;
			break;
	}

	if (expr_type == PlaExprNone) {
		parse_expr_bin(prsr, out);
		return;
	}

	next_tok(prsr);

	pla_expr_t * expr = pla_expr_create(expr_type);

	expr->pos_start = pos_start;

	expr->opd0.expr = *out;
	*out = expr;
	
	parse_expr_asgn(prsr, &expr->opd1.expr);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_expr_rse(pla_prsr_t * prsr, pla_expr_t ** out) {
	parse_expr_asgn(prsr, out);
}
static void parse_expr(pla_prsr_t * prsr, pla_expr_t ** out) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	parse_expr_rse(prsr, out);

	pop_rse(prsr);
}


static void parse_stmt(pla_prsr_t * prsr, pla_stmt_t ** out);

static void parse_stmt_blk(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtBlk);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_punc_exact_crit(prsr, PlaPuncLeBrace);

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

	(*out)->pos_end = prsr->prev_tok_pos_end;
}

static void parse_stmt_expr(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtExpr);

	(*out)->pos_start = prsr->tok.pos_start;

	parse_expr(prsr, &(*out)->expr.expr);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_stmt_var(pla_prsr_t * prsr, pla_stmt_t ** out) {
	pla_ec_pos_t pos_start = prsr->tok.pos_start;
	pla_stmt_t ** pos_cur = out;

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

		(*out)->pos_start = pos_start;
		(*out)->pos_end = prsr->prev_tok_pos_end;

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncSemicolon);

			break;
		}

		out = &(*out)->next;
	}

	(*pos_cur)->pos_end = prsr->prev_tok_pos_end;

	while (pos_cur != out) {
		pos_cur = &(*pos_cur)->next;

		(*pos_cur)->pos_end = prsr->prev_tok_pos_end;
	}
}
static void parse_stmt_cond(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtCond);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywIf);

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

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_stmt_pre_loop(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtPreLoop);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywWhile);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		consume_ident_crit(prsr, &(*out)->pre_loop.name);
	}

	parse_expr(prsr, &(*out)->pre_loop.cond_expr);

	parse_stmt_blk(prsr, &(*out)->pre_loop.body);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_stmt_post_loop(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtPostLoop);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywDo);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		consume_ident_crit(prsr, &(*out)->post_loop.name);
	}

	parse_stmt_blk(prsr, &(*out)->post_loop.body);

	consume_keyw_exact_crit(prsr, PlaKeywWhile);

	parse_expr(prsr, &(*out)->post_loop.cond_expr);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_stmt_brk(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtBrk);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywBreak);

	consume_ident(prsr, &(*out)->brk.name);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_stmt_cnt(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtCnt);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywContinue);

	consume_ident(prsr, &(*out)->cnt.name);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_stmt_ret(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtRet);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywRet);

	if (get_punc(prsr) == PlaPuncSemicolon) {
		(*out)->ret.expr = pla_expr_create(PlaExprValVoid);

		(*out)->ret.expr->pos_start = prsr->prev_tok_pos_end;
		(*out)->ret.expr->pos_end = prsr->tok.pos_start;

		next_tok(prsr);
	}
	else {
		parse_expr(prsr, &(*out)->ret.expr);

		consume_punc_exact_crit(prsr, PlaPuncSemicolon);
	}

	(*out)->pos_end = prsr->prev_tok_pos_end;
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
	pla_prsr_rse_t rse = { .set_next = true };

	push_rse(prsr, &rse);

	parse_stmt_rse(prsr, out);

	pop_rse(prsr);
}


static void parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out);

static void parse_dclr_nspc(pla_prsr_t * prsr, pla_dclr_t ** out) {
	*out = pla_dclr_create(PlaDclrNspc);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywNamespace);

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

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_dclr_func(pla_prsr_t * prsr, pla_dclr_t ** out) {
	*out = pla_dclr_create(PlaDclrFunc);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywFunction);

	consume_ident_crit(prsr, &(*out)->name);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	parse_expr(prsr, &(*out)->func.dt_expr);

	parse_stmt_blk(prsr, &(*out)->func.block);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_dclr_impt(pla_prsr_t * prsr, pla_dclr_t ** out) {
	*out = pla_dclr_create(PlaDclrImpt);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywImport);

	consume_ident_crit(prsr, &(*out)->name);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	parse_expr(prsr, &(*out)->impt.dt_expr);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	consume_ch_str_crit(prsr, &(*out)->impt.lib_name, NULL);

	consume_punc_exact_crit(prsr, PlaPuncComma);

	consume_ch_str_crit(prsr, &(*out)->impt.sym_name, NULL);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_dclr_var(pla_prsr_t * prsr, pla_dclr_t ** out) {
	pla_ec_pos_t pos_start = prsr->tok.pos_start;
	
	consume_keyw_exact_crit(prsr, PlaKeywVariable);

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(prsr, &qual);

		ul_hs_t * name;

		consume_ident_crit(prsr, &name);

		if (consume_punc_exact(prsr, PlaPuncColonAsgn)) {
			*out = pla_dclr_create(PlaDclrVarVal);

			(*out)->var_val.dt_qual = qual;

			parse_expr(prsr, &(*out)->var_val.val_expr);
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncColon);

			*out = pla_dclr_create(PlaDclrVarDt);

			(*out)->var_dt.dt_qual = qual;

			parse_expr(prsr, &(*out)->var_dt.dt_expr);
		}

		(*out)->name = name;
		(*out)->pos_start = pos_start;
		(*out)->pos_end = prsr->prev_tok_pos_end;

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			(void)0;
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncSemicolon);

			break;
		}

		out = &(*out)->next;

		pos_start = prsr->tok.pos_start;
	}
}
static void parse_dclr_stct(pla_prsr_t * prsr, pla_dclr_t ** out) {
	pla_ec_pos_t pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywStruct);

	ul_hs_t * name;

	consume_ident_crit(prsr, &name);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		*out = pla_dclr_create(PlaDclrDtStct);

		(*out)->name = name;

		(*out)->dt_stct.dt_stct_expr = pla_expr_create(PlaExprDtTpl);

		pla_expr_t * dt_stct_expr = (*out)->dt_stct.dt_stct_expr;

		dt_stct_expr->pos_start = prsr->tok.pos_start;

		parse_expr_tpl_body(prsr, &(*out)->dt_stct.dt_stct_expr->opd1.expr);

		dt_stct_expr->pos_end = prsr->prev_tok_pos_end;
	}
	else {
		*out = pla_dclr_create(PlaDclrDtStctDecl);
	}

	(*out)->pos_start = pos_start;
	(*out)->name = name;

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
}
static void parse_dclr_enmn(pla_prsr_t * prsr, pla_dclr_t ** out) {
	*out = pla_dclr_create(PlaDclrEnmn);

	(*out)->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywEnumeration);

	consume_ident_crit(prsr, &(*out)->name);

	consume_punc_exact_crit(prsr, PlaPuncColon);

	parse_expr(prsr, &(*out)->enmn.dt_expr);

	consume_punc_exact_crit(prsr, PlaPuncLeBrace);

	pla_dclr_t ** ins = &(*out)->enmn.elem;

	while (true) {
		*ins = pla_dclr_create(PlaDclrEnmnElem);

		(*ins)->pos_start = prsr->tok.pos_start;

		consume_ident_crit(prsr, &(*ins)->name);

		if (consume_punc_exact(prsr, PlaPuncAsgn)) {
			parse_expr(prsr, &(*ins)->enmn_elem.val);
		}
		else {
			(*ins)->enmn_elem.val = pla_expr_create(PlaExprNone);
		}

		(*ins)->pos_end = prsr->prev_tok_pos_end;

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			if (consume_punc_exact(prsr, PlaPuncRiBrace)) {
				break;
			}
		}
		else {
			consume_punc_exact_crit(prsr, PlaPuncRiBrace);

			break;
		}

		ins = &(*ins)->next;
	}

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	(*out)->pos_end = prsr->prev_tok_pos_end;
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
				case PlaKeywEnumeration:
					parse_dclr_enmn(prsr, out);
					return;
			}
			break;
	}

	report(prsr, L"expected a declarator");
	next_tok(prsr);
}
static void parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out) {
	pla_prsr_rse_t rse = { 0 };

	push_rse(prsr, &rse);

	parse_dclr_rse(prsr, out);
	
	pop_rse(prsr);
}


static void parse_tu_ref(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	pla_tu_ref_t * ref = pla_tu_push_ref(tu);

	ref->pos_start = prsr->tok.pos_start;

	consume_keyw_exact_crit(prsr, PlaKeywTlatuRef);

	if (consume_punc_exact(prsr, PlaPuncDot)) {
		ref->is_rel = true;
	}

	parse_cn(prsr, PlaPuncDot, &ref->cn);

	consume_punc_exact_crit(prsr, PlaPuncSemicolon);

	ref->pos_end = prsr->prev_tok_pos_end;
}
static void parse_tu_item(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
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
static void parse_tu_rse(pla_prsr_t * prsr, pla_tu_t * tu) {
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
static void parse_tu(pla_prsr_t * prsr, pla_tu_t * tu) {
	pla_prsr_rse_t rse = { .get_next = true, .set_next = true };

	push_rse(prsr, &rse);

	parse_tu_rse(prsr, tu);

	pop_rse(prsr);
}

bool pla_prsr_parse_tu(pla_prsr_t * prsr, pla_tu_t * tu) {
	prsr->is_rptd = false;

	parse_tu(prsr, tu);

	return !prsr->is_rptd;
}
