#include "pch.h"
#include "ira_val.h"
#include "ira_dt.h"
#include "ira_lo.h"

static void destroy_val_arr(size_t arr_size, ira_val_t ** arr) {
	for (ira_val_t ** elem = arr, **elem_end = elem + arr_size; elem != elem_end; ++elem) {
		ira_val_destroy(*elem);
	}
}
static ira_val_t ** copy_val_arr(size_t arr_size, ira_val_t ** arr) {
	ira_val_t ** new_arr = malloc(arr_size * sizeof(*new_arr));

	ul_assert(new_arr != NULL);

	for (ira_val_t ** elem = arr, **elem_end = elem + arr_size, **ins = new_arr; elem != elem_end; ++elem, ++ins) {
		*ins = ira_val_copy(*elem);
	}

	return new_arr;
}

ira_val_t * ira_val_create(ira_val_type_t type, ira_dt_t * dt) {
	ira_val_t * val = malloc(sizeof(*val));

	ul_assert(val != NULL);

	memset(val, 0, sizeof(*val));

	*val = (ira_val_t){ .type = type, .dt = dt };

	return val;
}
void ira_val_destroy(ira_val_t * val) {
	if (val == NULL) {
		return;
	}

	switch (val->type) {
		case IraValImmVoid:
		case IraValImmDt:
		case IraValImmBool:
		case IraValImmInt:
			break;
		case IraValImmVec:
			destroy_val_arr(val->dt->vec.size, val->arr_val.data);
			free(val->arr_val.data);
			break;
		case IraValImmPtr:
		case IraValLoPtr:
			break;
		case IraValImmTpl:
			destroy_val_arr(val->dt->tpl.elems_size, val->arr_val.data);
			free(val->arr_val.data);
			break;
		case IraValImmStct:
			ira_val_destroy(val->val_val);
			break;
		case IraValImmArr:
			destroy_val_arr(val->arr_val.size, val->arr_val.data);
			free(val->arr_val.data);
			break;
		default:
			ul_assert_unreachable();
	}

	free(val);
}

ira_val_t * ira_val_copy(ira_val_t * val) {
	ira_val_t * new_val = ira_val_create(val->type, val->dt);

	switch (val->type) {
		case IraValImmVoid:
			break;
		case IraValImmDt:
			new_val->dt_val = val->dt_val;
			break;
		case IraValImmBool:
			new_val->bool_val = val->bool_val;
			break;
		case IraValImmInt:
			new_val->int_val = val->int_val;
			break;
		case IraValImmVec:
			new_val->arr_val.data = copy_val_arr(val->dt->vec.size, val->arr_val.data);
			break;
		case IraValImmPtr:
			new_val->int_val = val->int_val;
			break;
		case IraValLoPtr:
			new_val->lo_val = val->lo_val;
			break;
		case IraValImmTpl:
			new_val->arr_val.data = copy_val_arr(val->dt->tpl.elems_size, val->arr_val.data);
			break;
		case IraValImmStct:
			new_val->val_val = ira_val_copy(val->val_val);
			break;
		case IraValImmArr:
			new_val->arr_val.size = val->arr_val.size;
			new_val->arr_val.data = copy_val_arr(val->arr_val.size, val->arr_val.data);
			break;
		default:
			ul_assert_unreachable();
	}

	return new_val;
}
