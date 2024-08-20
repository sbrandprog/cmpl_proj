#include "ul_lib/ul_lib.h"

static bool int_cmp_proc(const int * first_ptr, const int * second_ptr) {
	return *first_ptr < *second_ptr;
}

int main() {
	int arr[] = { -5, -4, -4, -4, -3, -3, -3, -1, 0, 0, 0, 2, 2, 2, 3, 3, 4, 4, 4, 6, 6 };

	for (size_t i = 0; i < _countof(arr); ++i) {
		printf("%3i ", arr[i]);
	}

	putchar('\n');

	for (size_t i = 0; i < _countof(arr); ++i) {
		printf("%3zi ", i);
	}

	putchar('\n');

	int trgs[] = { -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8 };

	for (size_t i = 0; i < _countof(trgs); ++i) {
		int trg = trgs[i];

		size_t lower_i = ul_bs_lower_bound(sizeof(*arr), _countof(arr), arr, int_cmp_proc, &trg),
			upper_i = ul_bs_upper_bound(sizeof(*arr), _countof(arr), arr, int_cmp_proc, &trg);

		if (lower_i > 0 && arr[lower_i - 1] > trg) {
			return -1;
		}
		else if (upper_i < _countof(arr) && arr[upper_i] <= trg) {
			return -1;
		}

		for (int * first = arr + lower_i, *last = arr + upper_i; first != last; ++first) {
			if (*first != trg) {
				return -1;
			}
		}

		wprintf(L"%3i : %3zi %3zi\n", trg, lower_i, upper_i);
	}

	return 0;
}
