# Secure Sandbox Environment

A lightweight, Linux-native sandbox for executing untrusted binaries in an isolated environment.

This project is implemented in **C** using native Linux kernel primitives rather than external container engines or frameworks. It combines filesystem isolation, Linux namespaces, Seccomp syscall filtering, and process/resource limits to reduce the attack surface presented by untrusted programs.

> **Security notice:** This project is intended as a lightweight sandboxing and security research implementation. It should **not** be considered a complete replacement for production-grade container or VM isolation. Running untrusted code always carries residual risk, particularly when the sandbox itself runs with elevated privileges.

---

## Features

### Filesystem Isolation

The sandbox creates a dedicated root filesystem at:

```text
/tmp/sandbox_rootfs
```

It uses `chroot()` to change the visible filesystem root and provides required system directories through **read-only bind mounts**

### Process & Namespace Isolation

The sandbox uses Linux `clone()` to create an isolated execution environment with:

* **PID namespace** — sandbox processes receive their own process ID space.
* **Mount namespace** — filesystem mount operations are isolated from the host.
* **UTS namespace** — hostname and domain-name state can be isolated.

A dedicated `/proc` filesystem is mounted inside the sandbox so that process information reflects the sandbox's PID namespace rather than the host.

### Seccomp Syscall Filtering

The sandbox applies a **Seccomp allowlist** to restrict access to potentially dangerous system calls.

Examples of restricted operations include:

* `mount`
* `ptrace`
* Raw socket operations
* Other privileged or unnecessary kernel interfaces

At the same time, normal program execution requires common operations such as:

* File I/O
* Memory management
* Process creation
* Program execution
* Signal handling

The exact syscall policy is defined by the implementation and should be reviewed whenever the sandbox is modified.

### Resource Limits

The sandbox uses `setrlimit()` to constrain resource consumption.

Limits can be applied to resources such as:

* CPU execution time
* Address-space/memory consumption
* Maximum number of processes

These restrictions are intended to mitigate common denial-of-service techniques such as:

* Infinite CPU loops
* Excessive memory allocation
* Fork bombs
* Unbounded process creation

### Information Leakage Reduction

The sandbox attempts to minimize exposure of host information by:

* Providing an isolated `/proc`
* Preventing access to sensitive `/proc` entries such as `/proc/kcore`
* Omitting `/sys` from the sandbox filesystem
* Restricting visibility through Linux namespaces

---

## Threat Model

The sandbox is designed to execute **untrusted binaries** while mitigating several common attack vectors.

### 1. Host Filesystem Access

An untrusted program should not be able to freely access or modify files outside the sandbox root.

The sandbox addresses this through:

* `chroot()`
* A dedicated root filesystem
* Mount namespace isolation
* Read-only bind mounts for host-provided system directories

### 2. Process Probing

The PID namespace prevents sandboxed processes from directly seeing the host's process hierarchy.

This reduces the ability of an untrusted process to:

* Enumerate host processes
* Signal arbitrary host processes
* Inspect host process state
* Trace unrelated host processes

### 3. Privileged Kernel Operations

Seccomp filtering prevents the sandboxed program from invoking selected dangerous system calls.

For example, attempts to perform operations such as:

```text
mount()
ptrace()
raw socket operations
```

may be rejected by the kernel.

### 4. Resource Exhaustion

Resource limits provide basic protection against resource exhaustion.

Examples include:

```text
Infinite CPU loop
       ↓
CPU time limit
       ↓
Process terminated
```

and:

```text
Fork bomb
   ↓
Process limit
   ↓
Additional processes rejected
```

These controls are defense-in-depth measures rather than complete DoS protection.

---

## Requirements

### Operating System

A Linux system with support for:

* Linux namespaces
* `clone()`
* `chroot()`
* Mount namespaces
* Seccomp
* `setrlimit()`

### Build Tools

The project requires:

* GCC
* GNU Make
* Standard Linux development headers

### Privileges

The sandbox currently requires **root or sudo privileges** for operations such as:

* Creating namespaces
* Mounting filesystems
* Creating bind mounts
* Performing `chroot()`

Run sandbox management commands with:

```bash
sudo
```

where required.

---

## Installation

Clone the repository:

```bash
git clone https://github.com/yousseam/secure-sandbox.git
cd secure-sandbox
```

Build the project:

```bash
make
```

After a successful build, the sandbox executable should be available in the project directory:

```text
./bin/sandbox
```

---

## Usage

The command-line interface provides three primary operations:

```text
create
run
destroy
```

---

### 1. Create the Sandbox

Initialize the sandbox root filesystem and configure the required bind mounts:

```bash
sudo ./sandbox create
```

This prepares:

```text
/tmp/sandbox_rootfs
```

and makes required system libraries and binaries available to programs running inside the sandbox.

The resulting filesystem is conceptually structured as:

```text
/tmp/sandbox_rootfs/
├── bin/
├── lib/
├── lib64/
├── usr/
└── proc/
```

---

### 2. Run a Program

Execute a program inside the sandbox:

```bash
sudo ./sandbox run <program> [arguments]
```

For example:

```bash
sudo ./sandbox run /bin/sh
```

Or:

```bash
sudo ./sandbox run /bin/ls -la
```

The target process is started inside the configured namespace, filesystem, Seccomp, and resource-limit environment.

---

### 3. Destroy the Sandbox

Clean up the sandbox filesystem and unmount configured directories:

```bash
sudo ./sandbox destroy
```

This removes the sandbox environment from:

```text
/tmp/sandbox_rootfs
```

> Make sure no processes are still using the sandbox mounts before attempting cleanup.

---

## Testing

The project can be tested using custom binaries placed inside the sandbox filesystem.

### Step 1 — Create the Sandbox

```bash
sudo ./sandbox create
```

### Step 2 — Create a Test Directory

```bash
sudo mkdir -p /tmp/sandbox_rootfs/tests
```

### Step 3 — Copy Test Binaries

```bash
sudo cp path/to/your/test_binaries/* /tmp/sandbox_rootfs/tests/
```

### Step 4 — Execute a Test

```bash
sudo ./sandbox run /tests/test_binary
```

### Example Security Tests

Useful tests include programs that attempt to:

* Read files outside the sandbox
* Write to read-only system directories
* Access `/proc/kcore`
* Access `/sys`
* Call `ptrace()`
* Call `mount()`
* Create excessive processes
* Allocate excessive memory
* Consume CPU indefinitely
* Create raw sockets
* Inspect host processes

Expected behavior is that restricted operations fail or the process is terminated when configured resource limits are exceeded.

---

## Architecture

The sandbox combines several Linux security primitives into a layered isolation model:

```text
                           Host Linux Kernel
                                  │
              ┌───────────────────┴───────────────────┐
              │                                       │
        Host Environment                         Sandbox
              │                                       │
      ┌───────┴────────┐                ┌─────────────┴─────────────┐
      │                │                │                           │
 Host Processes   Host Filesystem   Namespaces                Resource Limits
                                      │                           │
                              ┌───────┼────────┐            ┌─────┼─────┐
                              │       │        │            │     │     │
                             PID    Mount     UTS          CPU   RAM  Processes
                              │       │        │
                              └───────┼────────┘
                                      │
                                chroot()
                                      │
                           /tmp/sandbox_rootfs
                                      │
                    ┌─────────────────┴─────────────────┐
                    │                                   │
             Read-only system files                Isolated /proc
                    │                                   │
                    └─────────────────┬─────────────────┘
                                      │
                              Target Process
                                      │
                               ┌──────┴──────┐
                               │             │
                            Seccomp       Resource
                            Filter          Limits
                               │             │
                               └──────┬──────┘
                                      │
                              Untrusted Binary
```

---

## Isolation Layers

The sandbox follows a defense-in-depth approach.

| Layer                      | Linux Primitive       | Purpose                                      |
| -------------------------- | --------------------- | -------------------------------------------- |
| Filesystem                 | `chroot()`            | Restrict filesystem root                     |
| Mount isolation            | Mount namespace       | Isolate filesystem mounts                    |
| Process isolation          | PID namespace         | Hide host process hierarchy                  |
| Hostname isolation         | UTS namespace         | Isolate hostname/domain state                |
| Syscall filtering          | Seccomp               | Restrict dangerous kernel interfaces         |
| CPU limits                 | `setrlimit()`         | Limit CPU consumption                        |
| Memory limits              | `setrlimit()`         | Limit resource consumption                   |
| Process limits             | `setrlimit()`         | Mitigate fork bombs                          |
| Process information        | Isolated `/proc`      | Reduce host information exposure             |
| Host filesystem protection | Read-only bind mounts | Prevent modification of mounted system files |

---

## Security Considerations

This project intentionally uses low-level Linux primitives and should be treated as a **security-sensitive system**.

### `chroot()` Is Not a Complete Security Boundary

`chroot()` changes the apparent filesystem root but should not be treated as a standalone security mechanism.

The security model therefore relies on additional controls such as:

* Mount namespaces
* PID namespaces
* Seccomp
* Resource limits
* Reduced filesystem visibility

### Root Privileges Increase Risk

The sandbox management process requires elevated privileges.

A vulnerability in the sandbox implementation could therefore potentially have severe consequences.

In particular, code responsible for:

* Namespace creation
* Mount configuration
* Path handling
* File descriptor management
* Seccomp configuration
* Privilege transitions

should be reviewed carefully.

### Seccomp Policies Must Be Maintained

A syscall allowlist is only effective if it accurately reflects the intended execution environment.

Adding new functionality may require additional syscalls. Every new syscall should be evaluated before being added to the allowlist.

A permissive Seccomp policy can significantly weaken the sandbox.

### Resource Limits Are Not Cgroups

This implementation uses `setrlimit()` rather than Linux cgroups.

`rlimit` provides useful per-process resource controls, but cgroups provide stronger and more comprehensive resource management for many production workloads.

### Kernel Vulnerabilities

No userspace sandbox can completely protect against a vulnerability in the Linux kernel itself.

A successful kernel exploit could potentially escape namespace and Seccomp restrictions.

For high-risk workloads, consider stronger isolation such as:

* Virtual machines
* MicroVMs
* Dedicated worker hosts
* Production container runtimes with appropriate hardening

---

## Limitations

The current implementation is intentionally lightweight and does not attempt to provide every isolation mechanism available on Linux.

Potential areas for future improvement include:

* Linux cgroup-based resource management
* Dropping Linux capabilities
* Dedicated unprivileged user/group IDs
* `no_new_privs`
* User namespaces
* Network namespace isolation
* More restrictive mount options
* Seccomp argument filtering
* File descriptor sanitization
* `pivot_root()` instead of relying solely on `chroot()`
* Landlock filesystem restrictions
* Read-only root filesystem
* Dedicated sandbox users
* Audit logging
* Automated security regression tests
