#include "ul_spawn.h"
#include "ul_assert.h"

#if defined _WIN32

#include <process.h>

int ul_spawn_wait(const char * file_name, const char * const * args)
{
    return (int)_spawnv(_P_WAIT, file_name, args);
}

#elif defined __linux__

#include <sys/wait.h>
#include <unistd.h>

int ul_spawn_wait(const char * file_name, const char * const * args)
{
    pid_t pid = fork();

    ul_assert(pid != -1);

    if (pid != 0)
    {
        int wstatus;

        pid_t res = waitpid(pid, &wstatus, 0);

        ul_assert(res == pid);

        return WEXITSTATUS(wstatus);
    }
    else
    {
        size_t args_size = 0;

        for (const char * const * arg = args; *arg != NULL; ++arg)
        {
            ++args_size;
        }

        char ** new_args = malloc(args_size * sizeof(*new_args));

        ul_assert(new_args != NULL);

        {
            const char * const * arg = args;
            char ** new_arg = new_args;

            for (; *arg != NULL; ++arg, ++new_arg)
            {
                *new_arg = malloc(strlen(*arg));

                ul_assert(*new_arg != NULL);

                strcpy(*new_arg, *arg);
            }
        }

        execv(file_name, new_args);

        exit(-1);
    }
}

#endif
