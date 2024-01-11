#include "pch.h"
#include "ira_val.h"
#include "u_assert.h"

ira_val_t * ira_val_create(ira_val_type_t type, ira_dt_t * dt) {
	ira_val_t * val = malloc(sizeof(*val));

	u_assert(val != NULL);

	memset(val, 0, sizeof(*val));

	*val = (ira_val_t){ .type = type, .dt = dt };

	return val;
}
void ira_val_destroy(ira_val_t * val) {
	if (val == NULL) {
		return;
	}

	switch (val->type) {
		case IraValImmDt:
		case IraValImmBool:
		case IraValImmInt:
			break;
		case IraValLoPtr:
		case IraValNullPtr:
			break;
		case IraValImmArr:
			for (ira_val_t ** elem = val->arr.data, **elem_end = elem + val->arr.size; elem != elem_end; ++elem) {
				ira_val_destroy(*elem);
			}
			free(val->arr.data);
			break;
		default:
			u_assert_switch(val->type);
	}

	free(val);
}

ira_val_t * ira_val_copy(ira_val_t * val) {
	u_assert(val != NULL);

	ira_val_t * new_val = ira_val_create(val->type, val->dt);

	switch (val->type) {
		case IraValImmDt:
			new_val->dt_val = val->dt_val;
			break;
		case IraValImmBool:
			new_val->bool_val = val->bool_val;
			break;
		case IraValImmInt:
			new_val->int_val = val->int_val;
			break;
		case IraValLoPtr:
			new_val->lo_val = val->lo_val;
			break;
		case IraValNullPtr:
			break;
		case IraValImmArr:
			new_val->arr.size = val->arr.size;

			new_val->arr.data = malloc(new_val->arr.size * sizeof(*new_val->arr.data));

			u_assert(new_val->arr.data != NULL);

			for (ira_val_t ** elem = val->arr.data, **elem_end = elem + val->arr.size, **ins = new_val->arr.data; elem != elem_end; ++elem, ++ins) {
				*ins = ira_val_copy(*elem);
			}
			break;
		default:
			u_assert_switch(val->type);
	}

	return new_val;
}
