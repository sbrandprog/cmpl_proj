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


static void report(pla_prsr_t * prsr, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	pla_ec_fmtr_formatpost_va(prsr->ec_fmtr, PLA_PRSR_EC_GROUP, prsr->tok.pos_start, prsr->tok.pos_end, fmt, args);

	va_end(args);
}


static bool next_tok(pla_prsr_t * prsr) {
	prsr->tok.type = PlaTokNone;

	if (!prsr->get_tok_proc(prsr->src_data, &prsr->tok)) {
		return false;
	}

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
static bool consume_punc_exact_crit(pla_prsr_t * prsr, pla_punc_t punc) {
	ul_assert(punc < PlaPunc_Count);

	if (consume_punc_exact(prsr, punc)) {
		return true;
	}

	report(prsr, L"failed to consume \'%s\'", pla_punc_strs[punc].str);
	return false;
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
static bool consume_keyw_exact_crit(pla_prsr_t * prsr, pla_keyw_t keyw) {
	ul_assert(keyw < PlaKeyw_Count);

	if (consume_keyw_exact(prsr, keyw)) {
		return true;
	}

	report(prsr, L"failed to consume \'%s\'", pla_keyw_strs[keyw].str);
	return false;
}
static bool consume_ident(pla_prsr_t * prsr, ul_hs_t ** out) {
	if (prsr->tok.type == PlaTokIdent) {
		*out = prsr->tok.ident;
		next_tok(prsr);
		return true;
	}

	return false;
}
static bool consume_ident_crit(pla_prsr_t * prsr, ul_hs_t ** out) {
	if (consume_ident(prsr, out)) {
		return true;
	}

	report(prsr, L"failed to consume an identifier");
	return false;
}
static bool consume_ch_str_crit(pla_prsr_t * prsr, ul_hs_t ** out_data, ul_hs_t ** out_tag) {
	if (prsr->tok.type == PlaTokChStr) {
		*out_data = prsr->tok.ch_str.data;
		if (out_tag != NULL) {
			*out_tag = prsr->tok.ch_str.tag;
		}
		next_tok(prsr);
		return true;
	}

	report(prsr, L"failed to consume a character string");
	return false;
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


bool pla_prsr_parse_cn(pla_prsr_t * prsr, pla_punc_t delim, pla_cn_t ** out) {
	do {
		*out = pla_cn_create();

		if (!consume_ident_crit(prsr, &(*out)->name)) {
			return false;
		}

		out = &(*out)->sub_name;
	} while (consume_punc_exact(prsr, delim));

	return true;
}


static bool parse_expr_stct_body(pla_prsr_t * prsr, pla_expr_t ** elem_out) {
	if (!consume_punc_exact_crit(prsr, PlaPuncLeBrace)) {
		return false;
	}

	while (true) {
		*elem_out = pla_expr_create(PlaExprDtStctElem);

		if (!consume_ident_crit(prsr, &(*elem_out)->opd2.hs)) {
			return false;
		}

		if (!consume_punc_exact_crit(prsr, PlaPuncColon)) {
			return false;
		}

		if (!pla_prsr_parse_expr(prsr, &(*elem_out)->opd0.expr)) {
			return false;
		}

		if (consume_punc_exact(prsr, PlaPuncRiBrace)) {
			break;
		}
		else {
			if (!consume_punc_exact_crit(prsr, PlaPuncComma)) {
				return false;
			}
		}

		elem_out = &(*elem_out)->opd1.expr;
	}

	return true;
}

static bool parse_expr_unit(pla_prsr_t * prsr, pla_expr_t ** out) {
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
							if (!pla_prsr_parse_expr(prsr, &(*out)->opd1.expr)) {
								return false;
							}

							if (!consume_punc_exact_crit(prsr, PlaPuncRiBrack)) {
								return false;
							}
						}
						else {
							(*out)->opd1.expr = pla_expr_create(PlaExprValVoid);
						}

						break;
					case PlaExprCast:
						if (!consume_punc_exact_crit(prsr, PlaPuncLeBrack)) {
							return false;
						}

						if (!pla_prsr_parse_expr(prsr, &(*out)->opd1.expr)) {
							return false;
						}

						if (!consume_punc_exact_crit(prsr, PlaPuncRiBrack)) {
							return false;
						}
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


	bool success = true;

	switch (prsr->tok.type) {
		case PlaTokPunc:
			switch (prsr->tok.punc) {
				case PlaPuncLeParen:
					next_tok(prsr);

					if (!pla_prsr_parse_expr(prsr, out)) {
						return false;
					}

					if (!consume_punc_exact_crit(prsr, PlaPuncRiParen)) {
						return false;
					}
					break;
				case PlaPuncDolrSign:
					next_tok(prsr);

					if (!consume_punc_exact_crit(prsr, PlaPuncLeParen)) {
						return false;
					}

					*out = pla_expr_create(PlaExprDtFunc);

					if (!consume_punc_exact(prsr, PlaPuncRiParen)) {
						pla_expr_t ** arg = &(*out)->opd1.expr;

						while (true) {
							*arg = pla_expr_create(PlaExprDtFuncArg);

							if (!consume_ident_crit(prsr, &(*arg)->opd2.hs)) {
								return false;
							}

							if (!consume_punc_exact_crit(prsr, PlaPuncColon)) {
								return false;
							}

							if (!pla_prsr_parse_expr(prsr, &(*arg)->opd0.expr)) {
								return false;
							}

							if (consume_punc_exact(prsr, PlaPuncRiParen)) {
								break;
							}
							else {
								if (!consume_punc_exact_crit(prsr, PlaPuncComma)) {
									return false;
								}
							}

							arg = &(*arg)->opd1.expr;
						}
					}

					if (consume_punc_exact(prsr, PlaPuncRiArrow)) {
						if (!pla_prsr_parse_expr(prsr, &(*out)->opd0.expr)) {
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

					if (!parse_expr_stct_body(prsr, &(*out)->opd1.expr)) {
						return false;
					}
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

					if (!consume_punc_exact_crit(prsr, PlaPuncLeBrack)) {
						return false;
					}

					if (!pla_prsr_parse_expr(prsr, &(*out)->opd0.expr)) {
						return false;
					}

					if (!consume_punc_exact_crit(prsr, PlaPuncRiBrack)) {
						return false;
					}
					break;

				default:
					success = false;
			}
			break;
		case PlaTokIdent:
		{
			*out = pla_expr_create(PlaExprIdent);

			if (!pla_prsr_parse_cn(prsr, PlaPuncDcolon, &(*out)->opd0.cn)) {
				return false;
			}

			break;
		}
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
	}

	if (!success) {
		report(prsr, L"failed to parse an unit-expression");
		return false;
	}


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
				if (!consume_ident_crit(prsr, &(*out)->opd1.hs)) {
					return false;
				}
				break;
			case PlaExprCall:
				if (!consume_punc_exact(prsr, PlaPuncRiParen)) {
					pla_expr_t ** arg = &(*out)->opd1.expr;

					while (true) {
						*arg = pla_expr_create(PlaExprCallArg);

						if (!pla_prsr_parse_expr(prsr, &(*arg)->opd0.expr)) {
							return false;
						}

						if (consume_punc_exact(prsr, PlaPuncRiParen)) {
							break;
						}
						else {
							if (!consume_punc_exact_crit(prsr, PlaPuncComma)) {
								return false;
							}
						}

						arg = &(*arg)->opd1.expr;
					}
				}
				break;
			case PlaExprSubscr:
				if (!pla_prsr_parse_expr(prsr, &(*out)->opd1.expr)) {
					return false;
				}

				if (!consume_punc_exact_crit(prsr, PlaPuncRiBrack)) {
					return false;
				}
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

	return true;
}
static bool parse_expr_bin_core(pla_prsr_t * prsr, pla_expr_t ** out, const bin_optr_info_t ** info) {
	bin_optr_preced_t preced = (*info)->preced;

	do {
		pla_expr_t * expr = pla_expr_create((*info)->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		if (!parse_expr_unit(prsr, &(*out)->opd1.expr)) {
			return false;
		}

		*info = get_bin_optr_info(get_punc(prsr));

		if (*info != NULL) {
			next_tok(prsr);
		}

		while (*info != NULL && preced < (*info)->preced) {
			if (!parse_expr_bin_core(prsr, &(*out)->opd1.expr, info)) {
				return false;
			}
		}
	} while (*info != NULL && preced == (*info)->preced);

	return true;
}
static bool parse_expr_bin(pla_prsr_t * prsr, pla_expr_t ** out) {
	const bin_optr_info_t * info = get_bin_optr_info(get_punc(prsr));

	if (info != NULL) {
		if (!next_tok(prsr)) {
			return false;
		}

		do {
			if (!parse_expr_bin_core(prsr, out, &info)) {
				return false;
			}
		} while (info != NULL);
	}

	return true;
}
static bool parse_expr_asgn(pla_prsr_t * prsr, pla_expr_t ** out) {
	while (true) {
		if (!parse_expr_unit(prsr, out)) {
			return false;
		}

		pla_expr_type_t expr_type = PlaExprNone;

		switch (get_punc(prsr)) {
			case PlaPuncAsgn:
				expr_type = PlaExprAsgn;
				break;
		}

		if (expr_type == PlaExprNone) {
			if (!parse_expr_bin(prsr, out)) {
				return false;
			}
			break;
		}

		next_tok(prsr);

		pla_expr_t * expr = pla_expr_create(expr_type);

		expr->opd0.expr = *out;
		*out = expr;
		out = &expr->opd1.expr;
	}

	return true;
}
bool pla_prsr_parse_expr(pla_prsr_t * prsr, pla_expr_t ** out) {
	return parse_expr_asgn(prsr, out);
}


static bool parse_stmt_blk(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_punc_exact_crit(prsr, PlaPuncLeBrace)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtBlk);

	for (pla_stmt_t ** ins = &(*out)->blk.body; !consume_punc_exact(prsr, PlaPuncRiBrace); ) {
		if (!pla_prsr_parse_stmt(prsr, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}

static bool parse_stmt_expr(pla_prsr_t * prsr, pla_stmt_t ** out) {
	*out = pla_stmt_create(PlaStmtExpr);

	if (!pla_prsr_parse_expr(prsr, &(*out)->expr.expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_var(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywVariable)) {
		return false;
	}

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(prsr, &qual);

		ul_hs_t * var_name;

		if (!consume_ident_crit(prsr, &var_name)) {
			return false;
		}

		if (consume_punc_exact(prsr, PlaPuncColon)) {
			*out = pla_stmt_create(PlaStmtVarDt);

			(*out)->var_dt.name = var_name;
			(*out)->var_dt.dt_qual = qual;

			if (!pla_prsr_parse_expr(prsr, &(*out)->var_dt.dt_expr)) {
				return false;
			}
		}
		else if (consume_punc_exact(prsr, PlaPuncColonAsgn)) {
			*out = pla_stmt_create(PlaStmtVarVal);

			(*out)->var_val.name = var_name;
			(*out)->var_val.dt_qual = qual;

			if (!pla_prsr_parse_expr(prsr, &(*out)->var_val.val_expr)) {
				return false;
			}
		}
		else {
			report(prsr, L"failed to consume a \':\' or \':=\'");
			return false;
		}

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			/* nothing */
		}
		else {
			if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
				return false;
			}

			break;
		}

		out = &(*out)->next;
	}

	return true;
}
static bool parse_stmt_cond(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywIf)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtCond);

	if (!pla_prsr_parse_expr(prsr, &(*out)->cond.cond_expr)) {
		return false;
	}

	if (!parse_stmt_blk(prsr, &(*out)->cond.true_br)) {
		return false;
	}

	if (consume_keyw_exact(prsr, PlaKeywElse)) {
		if (get_punc(prsr) == PlaPuncLeBrace) {
			if (!parse_stmt_blk(prsr, &(*out)->cond.false_br)) {
				return false;
			}
		}
		else if (get_keyw(prsr) == PlaKeywIf) {
			if (!parse_stmt_cond(prsr, &(*out)->cond.false_br)) {
				return false;
			}
		}
		else {
			report(prsr, L"expected an else or else-if clause");
			return false;
		}
	}

	return true;
}
static bool parse_stmt_pre_loop(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywWhile)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtPreLoop);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		if (!consume_ident_crit(prsr, &(*out)->pre_loop.name)) {
			return false;
		}
	}

	if (!pla_prsr_parse_expr(prsr, &(*out)->pre_loop.cond_expr)) {
		return false;
	}

	if (!parse_stmt_blk(prsr, &(*out)->pre_loop.body)) {
		return false;
	}

	return true;
}
static bool parse_stmt_post_loop(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywDo)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtPostLoop);

	if (consume_punc_exact(prsr, PlaPuncColon)) {
		if (!consume_ident_crit(prsr, &(*out)->post_loop.name)) {
			return false;
		}
	}

	if (!parse_stmt_blk(prsr, &(*out)->post_loop.body)) {
		return false;
	}

	if (!consume_keyw_exact_crit(prsr, PlaKeywWhile)) {
		return false;
	}

	if (!pla_prsr_parse_expr(prsr, &(*out)->post_loop.cond_expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_brk(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywBreak)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtBrk);

	consume_ident(prsr, &(*out)->brk.name);

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_cnt(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywContinue)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtCnt);

	consume_ident(prsr, &(*out)->cnt.name);

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_ret(pla_prsr_t * prsr, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywRet)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtRet);

	if (!consume_punc_exact(prsr, PlaPuncSemicolon)) {
		if (!pla_prsr_parse_expr(prsr, &(*out)->ret.expr)) {
			return false;
		}

		if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
			return false;
		}
	}
	else {
		(*out)->ret.expr = pla_expr_create(PlaExprValVoid);
	}

	return true;
}
bool pla_prsr_parse_stmt(pla_prsr_t * prsr, pla_stmt_t ** out) {
	switch (prsr->tok.type) {
		case PlaTokPunc:
			switch (prsr->tok.punc) {
				case PlaPuncSemicolon:
					next_tok(prsr);
					return true;
			}
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywVariable:
					return parse_stmt_var(prsr, out);
				case PlaKeywIf:
					return parse_stmt_cond(prsr, out);
				case PlaKeywWhile:
					return parse_stmt_pre_loop(prsr, out);
				case PlaKeywDo:
					return parse_stmt_post_loop(prsr, out);
				case PlaKeywBreak:
					return parse_stmt_brk(prsr, out);
				case PlaKeywContinue:
					return parse_stmt_cnt(prsr, out);
				case PlaKeywRet:
					return parse_stmt_ret(prsr, out);
			}
			break;
	}

	return parse_stmt_expr(prsr, out);
}


static bool parse_dclr_nspc(pla_prsr_t * prsr, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywNamespace)) {
		return false;
	}

	*out = pla_dclr_create(PlaDclrNspc);

	if (!consume_ident_crit(prsr, &(*out)->name)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncLeBrace)) {
		return false;
	}

	pla_dclr_t ** ins = &(*out)->nspc.body;

	while (!consume_punc_exact(prsr, PlaPuncRiBrace)) {
		if (!pla_prsr_parse_dclr(prsr, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}
static bool parse_dclr_func(pla_prsr_t * prsr, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywFunction)) {
		return false;
	}

	*out = pla_dclr_create(PlaDclrFunc);

	if (!consume_ident_crit(prsr, &(*out)->name)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncColon)) {
		return false;
	}

	if (!pla_prsr_parse_expr(prsr, &(*out)->func.dt_expr)) {
		return false;
	}

	if (!parse_stmt_blk(prsr, &(*out)->func.block)) {
		return false;
	}

	return true;
}
static bool parse_dclr_impt(pla_prsr_t * prsr, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywImport)) {
		return false;
	}

	*out = pla_dclr_create(PlaDclrImpt);

	if (!consume_ident_crit(prsr, &(*out)->name)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncColon)) {
		return false;
	}

	if (!pla_prsr_parse_expr(prsr, &(*out)->impt.dt_expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncColon)) {
		return false;
	}

	if (!consume_ch_str_crit(prsr, &(*out)->impt.lib_name, NULL)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncComma)) {
		return false;
	}

	if (!consume_ch_str_crit(prsr, &(*out)->impt.sym_name, NULL)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_dclr_var(pla_prsr_t * prsr, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywVariable)) {
		return false;
	}

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(prsr, &qual);

		ul_hs_t * var_name;

		if (!consume_ident_crit(prsr, &var_name)) {
			return false;
		}

		if (consume_punc_exact(prsr, PlaPuncColon)) {
			*out = pla_dclr_create(PlaDclrVarDt);

			(*out)->name = var_name;
			(*out)->var_dt.dt_qual = qual;

			if (!pla_prsr_parse_expr(prsr, &(*out)->var_dt.dt_expr)) {
				return false;
			}
		}
		else if (consume_punc_exact(prsr, PlaPuncColonAsgn)) {
			*out = pla_dclr_create(PlaDclrVarVal);

			(*out)->name = var_name;
			(*out)->var_val.dt_qual = qual;

			if (!pla_prsr_parse_expr(prsr, &(*out)->var_val.val_expr)) {
				return false;
			}
		}
		else {
			report(prsr, L"failed to consume a \':\' or \':=\'");
			return false;
		}

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			/* nothing */
		}
		else {
			if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
				return false;
			}

			break;
		}

		out = &(*out)->next;
	}

	return true;
}
static bool parse_dclr_stct(pla_prsr_t * prsr, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywStruct)) {
		return false;
	}

	ul_hs_t * name;

	if (!consume_ident_crit(prsr, &name)) {
		return false;
	}

	if (get_punc(prsr) == PlaPuncLeBrace) {
		*out = pla_dclr_create(PlaDclrDtStct);

		(*out)->name = name;

		(*out)->dt_stct.dt_stct_expr = pla_expr_create(PlaExprDtStct);

		if (!parse_expr_stct_body(prsr, &(*out)->dt_stct.dt_stct_expr->opd1.expr)) {
			return false;
		}
	}
	else {
		*out = pla_dclr_create(PlaDclrDtStctDecl);

		(*out)->name = name;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_dclr_ro_val(pla_prsr_t * prsr, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywReadonly)) {
		return false;
	}

	while (true) {
		*out = pla_dclr_create(PlaDclrRoVal);

		if (!consume_ident_crit(prsr, &(*out)->name)) {
			return false;
		}

		if (!consume_punc_exact_crit(prsr, PlaPuncColonAsgn)) {
			return false;
		}

		if (!pla_prsr_parse_expr(prsr, &(*out)->ro_val.val_expr)) {
			return false;
		}

		if (consume_punc_exact(prsr, PlaPuncComma)) {
			/* nothing */
		}
		else {
			if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
				return false;
			}

			break;
		}

		out = &(*out)->next;
	}

	return true;
}
bool pla_prsr_parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out) {
	switch (prsr->tok.type) {
		case PlaTokNone:
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywNamespace:
					return parse_dclr_nspc(prsr, out);
				case PlaKeywFunction:
					return parse_dclr_func(prsr, out);
				case PlaKeywImport:
					return parse_dclr_impt(prsr, out);
				case PlaKeywVariable:
					return parse_dclr_var(prsr, out);
				case PlaKeywStruct:
					return parse_dclr_stct(prsr, out);
				case PlaKeywReadonly:
					return parse_dclr_ro_val(prsr, out);
			}
			break;
	}

	report(prsr, L"expected a declarator");
	return false;
}


static bool parse_tu_ref(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	if (!consume_keyw_exact_crit(prsr, PlaKeywTlatuRef)) {
		return false;
	}

	pla_tu_ref_t * ref = pla_tu_push_ref(tu);

	if (consume_punc_exact(prsr, PlaPuncDot)) {
		ref->is_rel = true;
	}

	if (!pla_prsr_parse_cn(prsr, PlaPuncDot, &ref->cn)) {
		return false;
	}

	if (!consume_punc_exact_crit(prsr, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_tu_item(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	switch (prsr->tok.type) {
		case PlaTokNone:
			break;
		case PlaTokKeyw:
			switch (prsr->tok.keyw) {
				case PlaKeywTlatuRef:
					return parse_tu_ref(prsr, tu, ins);
			}
			break;
	}

	return pla_prsr_parse_dclr(prsr, ins);
}
static bool parse_tu_body(pla_prsr_t * prsr, pla_tu_t * tu, pla_dclr_t ** ins) {
	while (prsr->tok.type != PlaTokNone) {
		if (!parse_tu_item(prsr, tu, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}
bool pla_prsr_parse_tu(pla_prsr_t * prsr, pla_tu_t * tu) {
	pla_dclr_t ** root = &tu->root;

	if (*root == NULL) {
		*root = pla_dclr_create(PlaDclrNspc);

		(*root)->name = NULL;
	}

	pla_dclr_t ** ins = &(*root)->nspc.body;

	while (*ins != NULL) {
		ins = &(*ins)->next;
	}

	if (!parse_tu_body(prsr, tu, ins)) {
		return false;
	}

	return true;
}
