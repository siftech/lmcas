# Supported system calls

Programs often have complicated ways of specifying their configuration, which creates potential problems for TaBaCCo.
For example, if the run time configuration depended on the time of day, TaBaCCo cannot choose the correct configuration at compile time.
However, if the program checks the time after the neck, TaBaCCo will not have frozen that syscall, so no conflict occurs.

As a result, TaBaCCo only supports a subset of system calls before the neck.
After the neck any system call is allowed.
Below we list the set of system calls currently supported by TaBaCCo.
Note not all features of a system call may be supported.

<!-- toc -->

## `sys_bind`

**Manual:** [bind(2)](https://man.archlinux.org/man/bind.2.en)

**Mock:** [`/sys_bind`](./syscall-mocks.md#sys_bind)

**Limitations:**

- The call must succeed or return `-EINVAL`.
- The call must use the `AF_INET` or `AF_INET6` address families.
- The call using the `AF_INET` or `AF_INET6` address families must not specify port 0.

## `sys_clock_getres`

**Manual:** [clock_getres(2)](https://man.archlinux.org/man/clock_getres.2.en)

**Limitations:**

- The call must succeed.

## `sys_clock_gettime`

**Manual:** [clock_getres(2)](https://man.archlinux.org/man/clock_getres.2.en)

**Limitations:**

- The call must succeed.

## `sys_close`

**Manual:** [close(2)](https://man.archlinux.org/man/close.2.en)

**Limitations:**

- The call must succeed.

## `sys_connect`

**Manual:** [connect(2)](https://man.archlinux.org/man/connect.2.en)

`sys_connect` gets a special emulation.
It always returns `ENOENT` when used with the `AF_UNIX` address family.
No other address family is supported.

## `sys_epoll_create1`

**Manual:** [epoll_create(2)](https://man.archlinux.org/man/epoll_create.2.en)

**Limitations:** None known.

## `sys_fcntl`

**Manual:** [fcntl(2)](https://man.archlinux.org/man/fcntl.2.en)

This system call has subcommands.
Only the ones listed below are currently supported.

### `F_SETFD`

**Manual:** [fcntl(2)#F_SETFD](https://man.archlinux.org/man/fcntl.2.en#F_SETFD)

**Limitations:**

- The call must succeed.

### `F_GETFD`

**Manual:** [fcntl(2)#F_GETFD](https://man.archlinux.org/man/fcntl.2.en#F_GETFD)

**Limitations:**

- The call must suceeed.

## `sys_fstat`

**Manual:** [stat(2)](https://man.archlinux.org/man/stat.2.en)

**Limitations:**

- The call must succeed.

## `sys_geteuid`

**Manual:** [getuid(2)](https://man.archlinux.org/man/getuid.2.en)

**Mock:** [`/sys_geteuid`](./syscall-mocks.md#sys_geteuid)

**Limitations:** None known.

## `sys_getgid`

**Manual:** [getgid(2)](https://man.archlinux.org/man/getgid.2.en)

**Limitations:** None known.

## `sys_getgroups`

**Manual:** [getgroups(2)](https://man.archlinux.org/man/getgroups.2.en)

**Limitations:**

- The call must succeed.

## `sys_getpid`

**Manual:** [getpid(2)](https://man.archlinux.org/man/getpid.2.en)

**Limitations:** None known.

## `sys_getppid`

**Manual:** [getpid(2)](https://man.archlinux.org/man/getpid.2.en)

**Limitations:** None known.

## `sys_getuid`

**Manual:** [getuid(2)](https://man.archlinux.org/man/getuid.2.en)

**Mock:** [`/sys_getuid`](./syscall-mocks.md#sys_getuid)

**Limitations:** None known.

## `sys_ioctl`

**Manual:** [ioctl(2)](https://man.archlinux.org/man/ioctl.2.en)

**Mock:** [`/sys_ioctl`](./syscall-mocks.md#sys_ioctl)

This system call has subcommands. Only the ones listed below are currently supported.

### `FIONBIO`

**Limitations:** None known.

### `TIOCGWINSZ`

**Manual:** [ioctl_tty(2)#TIOCGWINSZ](https://man.archlinux.org/man/ioctl_tty.2.en#TIOCGWINSZ)

**Limitations:** See the [`/sys_ioctl/unsafe_allow_tiocgwinsz`](./syscall-mocks.md#sys_ioctlunsafe_allow_tiocgwinsz) mock.

## `sys_listen`

**Manual:** [listen(2)](https://man.archlinux.org/man/listen.2.en)

**Limitations:**

- The call must succeed.

## `sys_lseek`

**Manual:** [lseek(2)](https://man.archlinux.org/man/lseek.2.en)

**Limitations:**

- The call must succeed.

## `sys_mkdir`

**Manual:** [mkdir(2)](https://man.archlinux.org/man/mkdir.2.en)

`sys_mkdir` gets a special emulation.
The typical reason to call it before the neck is to create directories that will be used either on if they do not already exist.
To ensure that the pre-neck code is deterministic, we play off of this pattern:

- If a `sys_mkdir` call would create a directory, it is performed, but `-EEXIST` is returned to the program.
- If the directory already existed, `-EEXIST` is returned to the program.

This behavior ensures that the specialized program doesn't depend on a directory's non-existence to work properly.

This is implemented as an always-on syscall mock, but is not currently user-configurable.

## `sys_mmap`

**Manual:** [mmap(2)](https://man.archlinux.org/man/mmap.2.en)

**Limitations:**

- The call must succeed.
- The `flags` argument must be `MAP_PRIVATE | MAP_ANONYMOUS`.

## `sys_open`

**Manual:** [open(2)](https://man.archlinux.org/man/open.2.en)

**Mock:** [`/sys_open`](./syscall-mocks.md#sys_open)

**Limitations:**

- The call must succeed or return `-ENOENT`.
- The `flags` argument must not contain `O_DIRECTORY`.
- The `flags` argument must not contain `O_PATH`.
- The `flags` argument must not contain `O_WRONLY` unless the `/sys_open/make_wronly_files_devnull` syscall mock setting is used.

## `sys_pipe`

**Manual:** [pipe(2)](https://man.archlinux.org/man/pipe.2.en)

**Limitations:**

- The call must succeed.

## `sys_pread`

**Manual:** [pread(2)](https://man.archlinux.org/man/pread.2.en)

**Limitations:**

- The call must succeed.

## `sys_prlimit`

**Manual:** [getrlimit(2)](https://man.archlinux.org/man/getrlimit.2.en)

**Limitations:**

- The call must succeed.
- The `pid` argument must be 0.

## `sys_read`

**Manual:** [read(2)](https://man.archlinux.org/man/read.2.en)

**Limitations:**

- The call must succeed.

## `sys_readv`

**Manual:** [readv(2)](https://man.archlinux.org/man/readv.2.en)

**Limitations:**

- The call must succeed.

## `sys_rt_sigaction`

**Manual:** [sigaction(2)](https://man.archlinux.org/man/core/man-pages/sigaction.2.en)

**Mock:** [`/sys_rt_sigaction`](./syscall-mocks.md#sys_rt_sigaction)

**Limitations:** None known, although manual configuration of the [`/sys_rt_sigaction/replace_sighup`](./syscall-mocks.md#sys_rt_sigactionreplace_sighup) mock is necessary to set a `SIGHUP` handler.

## `sys_sched_getaffinity`

**Manual:** [sched_setaffinity(2)](https://man.archlinux.org/man/sched_setaffinity.2.en)

**Limitations:**

- The call must succeed.

## `sys_setsockopt`

**Manual:** [getsockopt(2)](https://man.archlinux.org/man/getsockopt.2.en)

**Limitations:**

- The call must succeed.

## `sys_socket`

**Manual:** [socket(2)](https://man.archlinux.org/man/socket.2.en)

**Mock:** [`/sys_socket`](./syscall-mocks.md#sys_socket)

**Limitations:**

- The call must succeed or return `-EAFNOSUPPORT`.
- The call must use the `AF_INET`, `AF_INET6`, or `AF_UNIX` address families.

## `sys_stat`

**Manual:** [stat(2)](https://man.archlinux.org/man/stat.2.en)

**Mock:** [`/sys_stat`](./syscall-mocks.md#sys_stat)

**Limitations:** None known.

## `sys_uname`

**Manual:** [uname(2)](https://man.archlinux.org/man/uname.2.en)

**Limitations:**

- The call must succeed.

## `sys_write`

**Manual:** [write(2)](https://man.archlinux.org/man/write.2.en)

**Limitations:**

- The call must succeed.
