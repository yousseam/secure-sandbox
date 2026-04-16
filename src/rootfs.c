#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rootfs.h"
#include "config.h"

static int ensure_dir(const char *dir, mode_t mode) {
    if (mkdir(dir, mode) == 0) return 0; // attempt to make new directory
    
    if (access(dir, F_OK) == 0) return 0; // check if directory exists
    
    return -1;
}

static void join_path(char *out, size_t out_size, const char *base, const char *suffix) {
    snprintf(out, out_size, "%s/%s", base, suffix);
}

static int write_text_file(const char *root, const char *relative_path, const char *content) {
    char full_path[PATH_BUF_SIZE];
    int fd;

    snprintf(full_path, sizeof(full_path), "%s%s", root, relative_path);

    fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
	perror(full_path);
	return -1;
    }

    if (write(fd, content, strlen(content)) < 0) {
	perror("write");
	close(fd);
	return -1;
    }

    close(fd);
    return 0;
}

static int copy_file(const char *src_path, const char *dst_path) {
    int src_fd, dst_fd;
    char buffer[512];
    ssize_t bytes_read;

    src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
	perror(src_path);
	return -1;
    }

    dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
	perror(dst_path);
	return -1;
    }

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
	if (write(dst_fd, buffer, sizeof(buffer)) < 0) {
	    perror("write");
	    close(src_fd);
	    close(dst_fd);
	    return -1;
	}
    }

    if (bytes_read < 0) {
	perror("read");
	close(src_fd);
	close(dst_fd);
	return -1;
    }

    close(src_fd);
    close(dst_fd);
    return 0;

}

static int bind_mount_readonly(const char *src, const char *dst) {
    if (mount(src, dst, NULL, MS_BIND | MS_REC, NULL) != 0) {
	perror("mount[MS_BIND]");
	return -1;
    }

    if (mount(NULL, dst, NULL, MS_BIND | MS_REC | MS_REMOUNT | MS_RDONLY, NULL) != 0) {
	perror("mount[MS_REMOUNT | MS_RDONLY]");
	return -1;
    }

    return 0;
}

static int bind_mount_device(const char *src, const char *dst) {
    int fd = open(dst, O_CREAT | O_WRONLY, 0666);
    if (fd < 0) {
	perror(dst);
	return -1;
    }

    if (mount(src, dst, NULL, MS_BIND, NULL) != 0) {
	perror("mount device");
	return -1;
    }

    return 0;
}

int build_rootfs(const char *root_path) {
    static const char *required_dirs[] = {
	"bin", "sbin", "lib", "lib64", "usr", "etc", "proc", "dev", "tmp"
    };
    static const char *readonly_bind_dirs[] = {
	"bin", "sbin", "lib", "lib64", "usr"
    };
    static const char *safe_devices[] = {
	"null", "zero", "random", "urandom"
    };

    char path[PATH_BUF_SIZE];
    char src[PATH_BUF_SIZE];
    char dst[PATH_BUF_SIZE];

    // if dir does not exist after "ensure_dir()" return -1
    if (ensure_dir(root_path, 0755) != 0) return -1;

    for (int i = 0; i < (int)(sizeof(required_dirs) / sizeof(required_dirs[0])); i++) {
	join_path(path, sizeof(path), root_path, required_dirs[i]);
	if (ensure_dir(path, 0755) != 0) return -1;
    }

    join_path(path, sizeof(path), root_path, "tmp");
    if (chmod(path, 01777) != 0) {
	perror("chmod");
	return -1;
    }

    if (write_text_file(root_path, "/etc/passwd", "root:x:0:0:root:/root:/bin/sh\n") != 0) {
	return -1;
    }

    if (write_text_file(root_path, "/etc/hostname", "sandbox\n") != 0) {
	return -1;
    }

    join_path(dst, sizeof(dst), root_path, "etc/resolv.conf");
    if (copy_file("/etc/resolv.conf", dst) != 0) {
	return -1;
    }

    join_path(dst, sizeof(dst), root_path, "etc/os-release");
    if (copy_file("/etc/os-release", dst) != 0) {
	return -1;
    }

    for (int i = 0; i < (int)(sizeof(readonly_bind_dirs) / sizeof(readonly_bind_dirs[0])); i++) {
	snprintf(src, sizeof(src), "/%s", readonly_bind_dirs[i]);
	join_path(dst, sizeof(dst), root_path, readonly_bind_dirs[i]);

	if (access(src, F_OK) != 0) {
	    continue;
	}

	if (bind_mount_readonly(src, dst) != 0) {
	    return -1;
	}
    }

    for (int i = 0; i < (int)(sizeof(safe_devices) / sizeof(safe_devices[0])); i++) {
	snprintf(src, sizeof(src), "/dev/%s", safe_devices[i]);
	snprintf(dst, sizeof(dst), "%s/dev/%s", root_path, safe_devices[i]);

	if (bind_mount_device(src, dst) != 0) {
	    return -1;
	}
    }

    return 0;
}

void destroy_rootfs(const char *root_path) {
    static const char *mounted_paths[] = {
	"bin", "sbin", "lib", "lib64", "usr", "dev/null", "dev/zero", "dev/random", "dev/urandom", "proc"
    };
    char path[PATH_BUF_SIZE];
    char command[PATH_BUF_SIZE + 32];

    for (int i = 0; i < (int)(sizeof(mounted_paths) / sizeof(mounted_paths[0])); i++) {
	join_path(path, sizeof(path), root_path, mounted_paths[i]);
	(void)umount2(path, MNT_DETACH);
    }

    snprintf(command, sizeof(command), "rm -rf %s", root_path);
    (void)system(command);
}

