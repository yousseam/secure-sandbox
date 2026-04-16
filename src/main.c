#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "rootfs.h"
#include "process_isolation.h"

static void print_usage(char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s create			- setup up sandbox rootfs\n", prog);
    fprintf(stderr, "  %s run [args]		- run command in sandbox\n", prog);
    fprintf(stderr, "  %s destroy		- remove sandbox rootfs\n", prog);
}

int main(int argc, char *argv[]) {
    char *root_path = SANDBOX_ROOTFS;

    if (argc < 2) {
	print_usage(argv[0]);
	return -1;
    }

    if (strcmp(argv[1], "create") == 0) {
	build_rootfs(root_path);
	printf("Created sandbox at %s\n", root_path);
    } else if (strcmp(argv[1], "run") == 0) {
	if (argc < 3) {
	    print_usage(argv[0]);
	    return -1;
	}
	if (access(root_path, F_OK) != 0) {
	    fprintf(stderr, "No Sandbox directory found. Run 'create' first.\n");
	    return -1;
	}
	pid_t child_pid = spawn_child_process(root_path, &argv[2]);
	int status;

	printf("Sandboxed PID: %d\n", child_pid);

	if (waitpid(child_pid, &status, 0) < 0) {
	    perror("waitpid");
	    return -1;
	}

	if (WIFEXITED(status)) {
	    return WEXITSTATUS(status);
	}

	graceful_exit(0);

    } else if (strcmp(argv[1], "destroy") == 0) {
	destroy_rootfs(root_path);
	printf("Destroyed sandbox at %s\n", root_path);
    } else { 
	print_usage(argv[0]);
	return -1;
    }

    return 0;
}

