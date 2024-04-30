#include "pch.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_tok.h"
#include "pla_lex.h"
#include "pla_cn.h"
#include "pla_expr.h"
#include "pla_stmt.h"
#include "pla_dclr.h"
#include "pla_tu_p.h"
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

typedef struct pla_tu_p_ctx {
	pla_tu_t * tu;
	pla_tu_p_get_tok_proc_t * get_tok_proc;
	void * src_data;

	tok_t tok;
} ctx_t;


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


static void report(ctx_t * ctx, const wchar_t * format, ...) {
	{
		va_list args;

		va_start(args, format);

		vfwprintf(stderr, format, args);

		va_end(args);

		fputwc(L'\n', stderr);
	}
}


static bool next_tok(ctx_t * ctx) {
	ctx->tok.type = PlaTokNone;

	if (!ctx->get_tok_proc(ctx->src_data, &ctx->tok)) {
		return false;
	}

	return true;
}


static pla_punc_t get_punc(ctx_t * ctx) {
	if (ctx->tok.type == PlaTokPunc) {
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
	ul_assert(punc < PlaPunc_Count);

	if (consume_punc_exact(ctx, punc)) {
		return true;
	}

	report(ctx, L"failed to consume \'%s\'", pla_punc_strs[punc].str);
	return false;
}
static pla_keyw_t get_keyw(ctx_t * ctx) {
	if (ctx->tok.type == PlaTokKeyw) {
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
	ul_assert(keyw < PlaKeyw_Count);

	if (consume_keyw_exact(ctx, keyw)) {
		return true;
	}

	report(ctx, L"failed to consume \'%s\'", pla_keyw_strs[keyw].str);
	return false;
}
static bool consume_ident(ctx_t * ctx, ul_hs_t ** out) {
	if (ctx->tok.type == PlaTokIdent) {
		*out = ctx->tok.ident;
		next_tok(ctx);
		return true;
	}

	return false;
}
static bool consume_ident_crit(ctx_t * ctx, ul_hs_t ** out) {
	if (consume_ident(ctx, out)) {
		return true;
	}

	report(ctx, L"failed to consume an identifier");
	return false;
}
static bool consume_ch_str_crit(ctx_t * ctx, ul_hs_t ** out_data, ul_hs_t ** out_tag) {
	if (ctx->tok.type == PlaTokChStr) {
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


static void parse_quals(ctx_t * ctx, ira_dt_qual_t * out) {
	*out = ira_dt_qual_none;

	while (true) {
		if (consume_keyw_exact(ctx, PlaKeywConst)) {
			out->const_q = true;
			continue;
		}

		break;
	}
}


static bool parse_cn(ctx_t * ctx, pla_punc_t delim, pla_cn_t ** out) {
	do {
		*out = pla_cn_create();

		if (!consume_ident_crit(ctx, &(*out)->name)) {
			return false;
		}

		out = &(*out)->sub_name;
	} while (consume_punc_exact(ctx, delim));

	return true;
}


static bool parse_expr(ctx_t * ctx, pla_expr_t ** out);

static bool parse_expr_stct_body(ctx_t * ctx, pla_expr_t ** elem_out) {
	if (!consume_punc_exact_crit(ctx, PlaPuncLeBrace)) {
		return false;
	}

	while (true) {
		*elem_out = pla_expr_create(PlaExprDtFuncArg);

		if (!consume_ident_crit(ctx, &(*elem_out)->opd2.hs)) {
			return false;
		}

		if (!consume_punc_exact_crit(ctx, PlaPuncColon)) {
			return false;
		}

		if (!parse_expr(ctx, &(*elem_out)->opd0.expr)) {
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

		elem_out = &(*elem_out)->opd1.expr;
	}

	return true;
}

static bool parse_expr_unit(ctx_t * ctx, pla_expr_t ** out) {
	while (true) {
		bool success = true;

		switch (ctx->tok.type) {
			case PlaTokPunc:
			{
				const unr_optr_info_t * info = get_unr_optr_info(ctx->tok.punc);

				if (info == NULL) {
					success = false;
					break;
				}

				next_tok(ctx);

				*out = pla_expr_create(info->expr_type);

				switch (info->expr_type) {
					case PlaExprDtArr:
						if (!consume_punc_exact(ctx, PlaPuncRiBrack)) {
							if (!parse_expr(ctx, &(*out)->opd1.expr)) {
								return false;
							}

							if (!consume_punc_exact_crit(ctx, PlaPuncRiBrack)) {
								return false;
							}
						}
						else {
							(*out)->opd1.expr = pla_expr_create(PlaExprValVoid);
						}

						break;
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

				break;
			}
			case PlaTokKeyw:
				switch (ctx->tok.keyw) {
					case PlaKeywConst:
						next_tok(ctx);

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

	switch (ctx->tok.type) {
		case PlaTokPunc:
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

					if (consume_punc_exact(ctx, PlaPuncRiArrow)) {
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
		case PlaTokKeyw:
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
				case PlaKeywStruct:
					next_tok(ctx);

					*out = pla_expr_create(PlaExprDtStct);

					if (!parse_expr_stct_body(ctx, &(*out)->opd1.expr)) {
						return false;
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
		case PlaTokIdent:
		{
			*out = pla_expr_create(PlaExprIdent);

			if (!parse_cn(ctx, PlaPuncDcolon, &(*out)->opd0.cn)) {
				return false;
			}

			break;
		}
		case PlaTokChStr:
			*out = pla_expr_create(PlaExprChStr);
			(*out)->opd0.hs = ctx->tok.ch_str.data;
			(*out)->opd1.hs = ctx->tok.ch_str.tag;
			next_tok(ctx);
			break;
		case PlaTokNumStr:
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
		const post_optr_info_t * info = get_post_optr_info(get_punc(ctx));

		if (info == NULL) {
			break;
		}

		next_tok(ctx);

		pla_expr_t * expr = pla_expr_create(info->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		switch (info->expr_type) {
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

				if (!consume_punc_exact_crit(ctx, PlaPuncRiBrack)) {
					return false;
				}
				break;
			case PlaExprDeref:
			{
				ul_hs_t * ident;

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
static bool parse_expr_bin_core(ctx_t * ctx, pla_expr_t ** out, const bin_optr_info_t ** info) {
	bin_optr_preced_t preced = (*info)->preced;

	do {
		pla_expr_t * expr = pla_expr_create((*info)->expr_type);

		expr->opd0.expr = *out;
		*out = expr;

		if (!parse_expr_unit(ctx, &(*out)->opd1.expr)) {
			return false;
		}

		*info = get_bin_optr_info(get_punc(ctx));

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
	const bin_optr_info_t * info = get_bin_optr_info(get_punc(ctx));

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

	for (pla_stmt_t ** ins = &(*out)->blk.body; !consume_punc_exact(ctx, PlaPuncRiBrace); ) {
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
	if (!consume_keyw_exact_crit(ctx, PlaKeywVariable)) {
		return false;
	}

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(ctx, &qual);

		ul_hs_t * var_name;

		if (!consume_ident_crit(ctx, &var_name)) {
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncColon)) {
			*out = pla_stmt_create(PlaStmtVarDt);

			(*out)->var_dt.name = var_name;
			(*out)->var_dt.dt_qual = qual;

			if (!parse_expr(ctx, &(*out)->var_dt.dt_expr)) {
				return false;
			}
		}
		else if (consume_punc_exact(ctx, PlaPuncColonAsgn)) {
			*out = pla_stmt_create(PlaStmtVarVal);

			(*out)->var_val.name = var_name;
			(*out)->var_val.dt_qual = qual;

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
			if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
				return false;
			}

			break;
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

	if (consume_punc_exact(ctx, PlaPuncColon)) {
		if (!consume_ident_crit(ctx, &(*out)->pre_loop.name)) {
			return false;
		}
	}

	if (!parse_expr(ctx, &(*out)->pre_loop.cond_expr)) {
		return false;
	}

	if (!parse_stmt_blk(ctx, &(*out)->pre_loop.body)) {
		return false;
	}

	return true;
}
static bool parse_stmt_post_loop(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywDo)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtPostLoop);

	if (consume_punc_exact(ctx, PlaPuncColon)) {
		if (!consume_ident_crit(ctx, &(*out)->post_loop.name)) {
			return false;
		}
	}

	if (!parse_stmt_blk(ctx, &(*out)->post_loop.body)) {
		return false;
	}

	if (!consume_keyw_exact_crit(ctx, PlaKeywWhile)) {
		return false;
	}

	if (!parse_expr(ctx, &(*out)->post_loop.cond_expr)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_brk(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywBreak)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtBrk);

	consume_ident(ctx, &(*out)->brk.name);

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_cnt(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywContinue)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtCnt);

	consume_ident(ctx, &(*out)->cnt.name);

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_stmt_ret(ctx_t * ctx, pla_stmt_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywRet)) {
		return false;
	}

	*out = pla_stmt_create(PlaStmtRet);

	if (!consume_punc_exact(ctx, PlaPuncSemicolon)) {
		if (!parse_expr(ctx, &(*out)->ret.expr)) {
			return false;
		}

		if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
			return false;
		}
	}
	else {
		(*out)->ret.expr = pla_expr_create(PlaExprValVoid);
	}

	return true;
}
static bool parse_stmt(ctx_t * ctx, pla_stmt_t ** out) {
	switch (ctx->tok.type) {
		case PlaTokPunc:
			switch (ctx->tok.punc) {
				case PlaPuncSemicolon:
					next_tok(ctx);
					return true;
			}
			break;
		case PlaTokKeyw:
			switch (ctx->tok.keyw) {
				case PlaKeywVariable:
					return parse_stmt_var(ctx, out);
				case PlaKeywIf:
					return parse_stmt_cond(ctx, out);
				case PlaKeywWhile:
					return parse_stmt_pre_loop(ctx, out);
				case PlaKeywDo:
					return parse_stmt_post_loop(ctx, out);
				case PlaKeywBreak:
					return parse_stmt_brk(ctx, out);
				case PlaKeywContinue:
					return parse_stmt_cnt(ctx, out);
				case PlaKeywRet:
					return parse_stmt_ret(ctx, out);
			}
			break;
	}

	return parse_stmt_expr(ctx, out);
}


static bool parse_dclr(ctx_t * ctx, pla_dclr_t ** out);

static bool parse_dclr_nspc(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywNamespace)) {
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
	if (!consume_keyw_exact_crit(ctx, PlaKeywFunction)) {
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
	if (!consume_keyw_exact_crit(ctx, PlaKeywImport)) {
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
	if (!consume_keyw_exact_crit(ctx, PlaKeywVariable)) {
		return false;
	}

	while (true) {
		ira_dt_qual_t qual;

		parse_quals(ctx, &qual);

		ul_hs_t * var_name;

		if (!consume_ident_crit(ctx, &var_name)) {
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncColon)) {
			*out = pla_dclr_create(PlaDclrVarDt);

			(*out)->name = var_name;
			(*out)->var_dt.dt_qual = qual;

			if (!parse_expr(ctx, &(*out)->var_dt.dt_expr)) {
				return false;
			}
		}
		else if (consume_punc_exact(ctx, PlaPuncColonAsgn)) {
			*out = pla_dclr_create(PlaDclrVarVal);

			(*out)->name = var_name;
			(*out)->var_val.dt_qual = qual;

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
			if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
				return false;
			}

			break;
		}

		out = &(*out)->next;
	}

	return true;
}
static bool parse_dclr_stct(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywStruct)) {
		return false;
	}

	ul_hs_t * name;

	if (!consume_ident_crit(ctx, &name)) {
		return false;
	}

	if (get_punc(ctx) == PlaPuncLeBrace) {
		*out = pla_dclr_create(PlaDclrDtStct);

		(*out)->name = name;

		(*out)->dt_stct.dt_stct_expr = pla_expr_create(PlaExprDtStct);

		if (!parse_expr_stct_body(ctx, &(*out)->dt_stct.dt_stct_expr->opd1.expr)) {
			return false;
		}
	}
	else {
		*out = pla_dclr_create(PlaDclrDtStctDecl);

		(*out)->name = name;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_dclr_ro_val(ctx_t * ctx, pla_dclr_t ** out) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywReadonly)) {
		return false;
	}

	while (true) {
		*out = pla_dclr_create(PlaDclrRoVal);

		if (!consume_ident_crit(ctx, &(*out)->name)) {
			return false;
		}

		if (!consume_punc_exact_crit(ctx, PlaPuncColonAsgn)) {
			return false;
		}

		if (!parse_expr(ctx, &(*out)->ro_val.val_expr)) {
			return false;
		}

		if (consume_punc_exact(ctx, PlaPuncComma)) {
			/* nothing */
		}
		else {
			if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
				return false;
			}

			break;
		}

		out = &(*out)->next;
	}

	return true;
}
static bool parse_dclr(ctx_t * ctx, pla_dclr_t ** out) {
	switch (ctx->tok.type) {
		case PlaTokNone:
			break;
		case PlaTokKeyw:
			switch (ctx->tok.keyw) {
				case PlaKeywNamespace:
					return parse_dclr_nspc(ctx, out);
				case PlaKeywFunction:
					return parse_dclr_func(ctx, out);
				case PlaKeywImport:
					return parse_dclr_impt(ctx, out);
				case PlaKeywVariable:
					return parse_dclr_var(ctx, out);
				case PlaKeywStruct:
					return parse_dclr_stct(ctx, out);
				case PlaKeywReadonly:
					return parse_dclr_ro_val(ctx, out);
			}
			break;
	}

	report(ctx, L"expected a declarator");
	return false;
}


static bool parse_tu_ref(ctx_t * ctx, pla_dclr_t ** ins) {
	if (!consume_keyw_exact_crit(ctx, PlaKeywTlatuRef)) {
		return false;
	}

	pla_tu_ref_t * ref = pla_tu_push_ref(ctx->tu);

	if (consume_punc_exact(ctx, PlaPuncDot)) {
		ref->is_rel = true;
	}

	if (!parse_cn(ctx, PlaPuncDot, &ref->cn)) {
		return false;
	}

	if (!consume_punc_exact_crit(ctx, PlaPuncSemicolon)) {
		return false;
	}

	return true;
}
static bool parse_tu_item(ctx_t * ctx, pla_dclr_t ** ins) {
	switch (ctx->tok.type) {
		case PlaTokNone:
			break;
		case PlaTokKeyw:
			switch (ctx->tok.keyw) {
				case PlaKeywTlatuRef:
					return parse_tu_ref(ctx, ins);
			}
			break;
	}

	return parse_dclr(ctx, ins);
}
static bool parse_tu(ctx_t * ctx, pla_dclr_t ** ins) {
	if (!next_tok(ctx)) {
		return false;
	}

	while (ctx->tok.type != PlaTokNone) {
		if (!parse_tu_item(ctx, ins)) {
			return false;
		}

		while (*ins != NULL) {
			ins = &(*ins)->next;
		}
	}

	return true;
}

static bool parse_core(ctx_t * ctx) {
	pla_dclr_t ** root = &ctx->tu->root;

	if (*root == NULL) {
		*root = pla_dclr_create(PlaDclrNspc);

		(*root)->name = NULL;
	}

	pla_dclr_t ** ins = &(*root)->nspc.body;

	while (*ins != NULL) {
		ins = &(*ins)->next;
	}

	if (!parse_tu(ctx, ins)) {
		return false;
	}

	return true;
}
bool pla_tu_p_parse(pla_tu_t * tu, pla_tu_p_get_tok_proc_t * get_tok_proc, void * src_data) {
	ctx_t ctx = { .tu = tu, .get_tok_proc = get_tok_proc, .src_data = src_data };

	bool res;

	__try {
		res = parse_core(&ctx);
	}
	__finally {

	}

	return res;
}
