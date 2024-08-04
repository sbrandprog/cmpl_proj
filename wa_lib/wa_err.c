#include "wa_err.h"

#define BUF_SIZE 256

static void print_msg(const wchar_t * func_name, DWORD err) {
	wchar_t buf[BUF_SIZE];

	DWORD msg_size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buf, _countof(buf), NULL);

	fwprintf(stderr, L"unexpected error code from WIN32 API function [0x%08lX | %9lu] in [%s].\n", (uint32_t)err, (uint32_t)err, func_name);

	if (msg_size == 0) {
		fwprintf(stderr, L"failed to generate message.\n");
	}
	else {
		fwprintf(stderr, L"message:\n%.*s", (int)msg_size, buf);
	}
}

void wa_err_check_fatal_func(const wchar_t * func_name) {
	DWORD err = GetLastError();

	if (err != ERROR_SUCCESS) {
		print_msg(func_name, err);

		fwprintf(stderr, L"error considered fatal. exiting.\n");

		exit(-1);
	}
}

void wa_err_check_func(const wchar_t * func_name) {
	DWORD err = GetLastError();

	if (err != ERROR_SUCCESS) {
		print_msg(func_name, err);
	}
}
