#pragma once
#include "ira.h"

struct ira_func
{
    ira_dt_t * dt;

    size_t insts_size;
    ira_inst_t * insts;
    size_t insts_cap;
};

IRA_API ira_func_t * ira_func_create(ira_dt_t * dt);
IRA_API void ira_func_destroy(ira_func_t * func);

IRA_API ira_inst_t * ira_func_push_inst(ira_func_t * func, ira_inst_t * inst);
