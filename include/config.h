#ifndef CONFIG_H
#define CONFIG_H

// Root directory for the sandbox
#define SANDBOX_ROOTFS "/tmp/sandbox_rootfs"

// Buffer size
#define PATH_BUF_SIZE 1024

// Child stack size
#define CHILD_STACK_SIZE 1024 * 1024

// Struct for controlling child process
typedef struct {
    char root_dir[PATH_BUF_SIZE];
    char **argv;
} SandboxLaunchConfig;

#endif
