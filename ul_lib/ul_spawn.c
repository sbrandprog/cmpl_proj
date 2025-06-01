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
        size_t args_size = 2;

        for (const char * const * arg = args; *arg != NULL; ++arg)
        {
            ++args_size;
        }

        char ** new_args = malloc(args_size * sizeof(*new_args));

        ul_assert(new_args != NULL);

        {
            const char * const * arg = args;
            char ** new_arg = new_args;

            {
                size_t file_name_len = strlen(file_name);

                *new_arg = malloc((file_name_len + 1) * sizeof(**new_arg));

                ul_assert(*new_arg != NULL);

                strncpy(*new_arg, file_name, file_name_len);
            }

            for (; *arg != NULL; ++arg, ++new_arg)
            {
                size_t arg_len = strlen(*arg);

                *new_arg = malloc((arg_len + 1) * sizeof(**new_arg));

                ul_assert(*new_arg != NULL);

                strncpy(*new_arg, *arg, arg_len);
            }

            *new_arg++ = NULL;
        }

        execv(file_name, new_args);

        exit(-1);
    }
}

#endif
