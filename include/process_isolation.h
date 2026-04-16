#ifndef PROCESS_ISOLATION_H
#define PROCESS_ISOLATION_H

#include <sys/types.h>

pid_t spawn_child_process(const char *root_path, char **program_argv);

void graceful_exit(int rc);

#endif
