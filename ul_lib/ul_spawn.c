#include "ul_assert.h"
#include "ul_spawn.h"

#ifdef _WIN32

#include <process.h>

int ul_spawn_wait(const wchar_t * file_name, const wchar_t * const * args) {
	return (int)_wspawnv(_P_WAIT, file_name, args);
}

#endif