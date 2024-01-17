#include "pch.h"
#include "ira_inst.h"
#include "ira_val.h"
#include "u_assert.h"

void ira_inst_cleanup(ira_inst_t * inst) {
	u_assert(inst->type < IraInst_Count);

	const ira_inst_info_t * info = &ira_inst_infos[inst->type];

	for (size_t opd = 0; opd < IRA_INST_OPDS_SIZE; ++opd) {
		switch (info->opds[opd]) {
			case IraInstOpdNone:
			case IraInstOpdIntCmp:
			case IraInstOpdDt:
			case IraInstOpdLabel:
			case IraInstOpdLo:
			case IraInstOpdVarDef:
			case IraInstOpdVar:
			case IraInstOpdMmbr:
			case IraInstOpdVarsSize:
				break;
			case IraInstOpdVal:
				ira_val_destroy(inst->opds[opd].val);
				break;
			case IraInstOpdVars2:
			case IraInstOpdIds2:
				free(inst->opds[opd].hss);
				break;
			default:
				u_assert_switch(info->opds[opd]);
		}
	}

	memset(inst, 0, sizeof(*inst));
}

const ira_inst_info_t ira_inst_infos[IraInst_Count] = {
	[IraInstNone] = { .type_str = U_MAKE_ROS(L"InstNone"), .intrp_comp = true, .compl_comp = true, .opds = { IraInstOpdNone, IraInstOpdNone, IraInstOpdNone, IraInstOpdNone, IraInstOpdNone } },
#define IRA_INST(name, compl_, intrp_, opd0, opd1, opd2, opd3, opd4) [IraInst##name] = { .type_str = U_MAKE_ROS(L"Inst" L ## #name), .intrp_comp = intrp_, .compl_comp = compl_, .opds = { IraInstOpd##opd0, IraInstOpd##opd1, IraInstOpd##opd2, IraInstOpd##opd3, IraInstOpd##opd4 } },
#include "ira_inst.def"
#undef IRA_INST
};
