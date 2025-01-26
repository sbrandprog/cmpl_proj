#include "ul_spawn.h"
#include "ul_assert.h"

#ifdef _WIN32

#include <process.h>

int ul_spawn_wait(const char * file_name, const char * const * args)
{
    return (int)_spawnv(_P_WAIT, file_name, args);
}

#endif
