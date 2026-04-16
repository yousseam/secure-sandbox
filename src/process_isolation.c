#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <seccomp.h>
#include <errno.h>
#include <sys/resource.h>

#include "config.h"
#include "process_isolation.h"

scmp_filter_ctx *ctx;

void graceful_exit(int rc) {
    seccomp_release(ctx);
    exit(rc);
}

static void setup_seccomp_filter() {
    int rc;

    // Initialize seccomp filter with the default action to allow
    if ((ctx = seccomp_init(SCMP_ACT_ALLOW)) == NULL) graceful_exit(1);

    // Block mount
    if ((rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(mount), 0)) != 0) graceful_exit(rc);

    // Block ptrace
    if ((rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(ptrace), 0)) != 0) graceful_exit(rc);

    // Block raw sockets without blocking all sockets
   /* struct scmp_arg_cmp socket_args[2] = {
	SCMP_A0(SCMP_CMP_EQ, 17),	// AF_PACKET
	SCMP_A1(SCMP_CMP_EQ, 3)		// RAW_SOCK
    };
    if ((rc = seccomp_rule_add_array(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(socket), 2, socket_args)) != 0) graceful_exit(rc);*/
    if ((rc = seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(socket), 0)) != 0) graceful_exit(rc);

    // Load the filter into kernel
    if ((rc = seccomp_load(ctx)) != 0) graceful_exit(rc);
}

static void setup_resource_limits() {
    struct rlimit r1;

    // CPU limit (seconds)
    r1.rlim_cur = 5;
    r1.rlim_max = 5;
    if (setrlimit(RLIMIT_CPU, &r1)) perror("setrlimit CPU");

    // Memory limit (100 MB)
    r1.rlim_cur = 100 * 1024 * 1024;
    r1.rlim_max = 100 * 1024 * 1024;
    if (setrlimit(RLIMIT_AS, &r1)) perror("setrlimit AS");

    // Process limit
    r1.rlim_cur = 20;
    r1.rlim_max = 20;
    if (setrlimit(RLIMIT_NPROC, &r1)) perror("setrlimit NPROC");
}

// Function used for cloning and executing child process
static int spawn_child_main(void *arg) {
    SandboxLaunchConfig *cfg = (SandboxLaunchConfig *)arg;

    // Mount "/" root directory
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        perror("mount(private)");
        return 1;
    }

    // Assign root path
    if (chroot(cfg->root_dir) != 0) {
        perror("chroot");
        return 1;
    }

    // Assign root directory
    if (chdir("/") != 0) {
        perror("chdir");
        return 1;
    }

    // Mount "/proc"
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, "hidepid=2,subset=pid") != 0) {
        perror("mount(proc)");
        return 1;
    }

    (void)sethostname("sandbox", 7);

    setup_resource_limits();

    setup_seccomp_filter();

    // Replace process with command
    execvp(cfg->argv[0], cfg->argv);
    perror("execvp");
    return 127;
}

pid_t spawn_child_process(const char *root_path, char **program_argv) {
   static char child_stack[CHILD_STACK_SIZE];
   pid_t child_pid;
   SandboxLaunchConfig *cfg = malloc(sizeof(*cfg));

   if (!cfg) {
	perror("malloc");
	return -1;
   }

   memset(cfg, 0, sizeof(*cfg));
   snprintf(cfg->root_dir, sizeof(cfg->root_dir), "%s", root_path);
   cfg->argv = program_argv;

   child_pid = clone(spawn_child_main, child_stack + CHILD_STACK_SIZE, CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | SIGCHLD, cfg);

   if (child_pid < 0) {
       perror("clone");
       return -1;
       free(cfg);
   }

   return child_pid;
}


