#include "any_to_exe.h"

static bool test_proc(test_ctx_t * ctx) {
	ctx->from = TestFromIra;
	
	ira_pec_init(&ctx->pec, &ctx->hst, &ctx->ec_fmtr);

	ctx->pec.root = ira_pec_push_unq_lo(&ctx->pec, IraLoNspc, NULL);

	ul_hs_t * main_name = UL_HST_HASHADD_WS(&ctx->hst, L"main");

	ctx->pec.root->nspc.body = ira_lo_create_nspc_node(main_name);

	ira_lo_t * main_lo = ira_pec_push_unq_lo(&ctx->pec, IraLoFunc, main_name);

	ctx->pec.root->nspc.body->lo = main_lo;

	ctx->pec.ep_name = main_name;

	ira_dt_t * s32_dt = &ctx->pec.dt_ints[IraIntS32];
	ira_dt_t * main_dt;

	if (!ira_pec_get_dt_func(&ctx->pec, s32_dt, 0, NULL, ctx->pec.dt_func_vass.none, &main_dt)) {
		return false;
	}

	main_lo->func = ira_func_create(main_dt);

	ul_hs_t * var_name = UL_HST_HASHADD_WS(&ctx->hst, L"%1");

	{
		ira_inst_t def_var = { .type = IraInstDefVar, .opd0.hs = var_name, .opd1.dt = s32_dt, .opd2.dt_qual = ira_dt_qual_none };

		ira_func_push_inst(main_lo->func, &def_var);
	}

	{
		ira_int_t val_imm = { .si32 = 0 };
		ira_val_t * val;

		if (!ira_pec_make_val_imm_int(&ctx->pec, IraIntS32, val_imm, & val)) {
			return false;
		}

		ira_inst_t load = { .type = IraInstLoadVal, .opd0.hs = var_name, .opd1.val = val };

		ira_func_push_inst(main_lo->func, &load);
	}

	{
		ira_inst_t ret = { .type = IraInstRet, .opd0.hs = var_name };

		ira_func_push_inst(main_lo->func, &ret);
	}

	return true;
}
