#pragma once
#include "ira_dt.h"

#define IRA_INST_OPDS_SIZE 5

enum ira_inst_opd_type {
	IraInstOpdNone,
	IraInstOpdIntCmp,
	IraInstOpdDtQual,
	IraInstOpdDt,
	IraInstOpdLabel,
	IraInstOpdVal,
	IraInstOpdVarDef,
	IraInstOpdVar,
	IraInstOpdMmbr,
	IraInstOpdVarsSize,
	IraInstOpdVars2,
	IraInstOpdIds2,
	IraInstOpds_Count
};
union ira_inst_opd {
	ira_int_cmp_t int_cmp;
	ira_dt_qual_t dt_qual;
	size_t size;
	ul_hs_t * hs;
	ul_hs_t ** hss;
	ira_dt_t * dt;
	ira_val_t * val;
};

enum ira_inst_type {
	IraInstNone,
#define IRA_INST(name, ...) IraInst##name,
#include "ira_inst.def"
#undef IRA_INST
	IraInst_Count
};
struct ira_inst {
	ira_inst_type_t type;
	union {
		ira_inst_opd_t opds[IRA_INST_OPDS_SIZE];
		struct {
			ira_inst_opd_t opd0;
			ira_inst_opd_t opd1;
			ira_inst_opd_t opd2;
			ira_inst_opd_t opd3;
			ira_inst_opd_t opd4;
		};
	};
};

struct ira_inst_info {
	ul_ros_t type_str;
	bool intrp_comp, compl_comp;
	bool mods_ctl_flow;
	union {
		ira_inst_opd_type_t opds[IRA_INST_OPDS_SIZE];
		struct {
			ira_inst_opd_type_t opd0, opd1, opd2, opd3, opd4;
		};
	};
};

void ira_inst_cleanup(ira_inst_t * inst);

const ira_inst_info_t ira_inst_infos[IraInst_Count];
