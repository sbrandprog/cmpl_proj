#include "ul_assert.h"
#include "ul_json.h"

ul_json_t * ul_json_create(ul_json_type_t type) {
	ul_json_t * json = malloc(sizeof(*json));

	ul_assert(json != NULL);

	*json = (ul_json_t){ .type = type };

	return json;
}
void ul_json_destroy(ul_json_t * json) {
	if (json == NULL) {
		return;
	}

	switch (json->type) {
		case UlJsonNull:
		case UlJsonBool:
		case UlJsonInt:
		case UlJsonDbl:
		case UlJsonStr:
			break;
		case UlJsonArr:
		case UlJsonObj:
			ul_json_destroy_chain(json->val_json);
			break;
		default:
			ul_assert_unreachable();
	}

	free(json);
}

void ul_json_destroy_chain(ul_json_t * json) {
	while (json != NULL) {
		ul_json_t * next = json->next;

		ul_json_destroy(json);

		json = next;
	}
}
