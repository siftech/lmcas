# Supported syscall mocks

<!-- toc -->

Note that syscall mocks are likely to be the subject of changes to improve usability in the future.

## Example

An example of a type-correct `syscall_mocks` setting in the [debloating specification schema] is as follows.
All possible fields are present, but not all have coherent values.
All fields are optional.

[debloating specification schema]: ./debloating-spec.md

```json
{
	"relative_path_dir": "/tmp/foo-debloat-dir",
	"sys_open": {
		"mapping": {
			"/example-config-file.txt": "config.txt"
		},
		"make_wronly_files_devnull": true
	},
	"sys_stat": {
		"mapping": {
			"/example-config-file.txt": "config.txt"
		}
	},
	"sys_rt_sigaction": {
		"replace_sighup": true
	},
	"sys_ioctl": {
		"terminal_dimensions": {
			"row": 25,
			"col": 80,
			"xpixel": 0,
			"ypixel": 0
		},
		unsafe_allow_tiocgwinsz: false
	},
	"sys_socket": {
		"allow": {
			"af_inet" : false,
			"af_inet6" : false,
			"af_unix" : false
		}
	},
	"sys_bind": {
		"allow": {
			"af_inet" : false,
			"af_inet6" : false
		}
	},
	"sys_getuid": {
		"uid": 1234
	},
	"sys_geteuid": {
		"euid": 1234
	}
	"sys_getgid": {
		"gid": 1234
	}
}
```

## Settings

### `/relative_path_dir`

**Type:** string

**Default:** `"."`

A directory that relative paths in the configuration are interpreted relative to.
If this string is a relative path, it is interpreted relative to the directory `tabacco` is run in.

### `/sys_open`

**Manual:** [open(2)](https://man.archlinux.org/man/open.2.en)

#### `/sys_open/mapping`

**Type:** object of strings

**Default:** `{}`

A mapping between file paths.
If one of the object's values is a relative path, it is converted to an absolute path relative to the value given by the `/relative_path_dir` setting.

When a `sys_open` syscall is issued whose pathname matches one of the object's keys, it is replaced with the corresponding value.

Note that an exact match is performed on the pathname; there are a variety of scenarios where unexpected behavior may result:

- The program opens a relative path, and the mapping specifies an absolute path.
- The program opens an absolute path, and the mapping specifies a relative path.
- The program opens a hard link or symlink to a path specified in the mapping.

#### `/sys_open/make_wronly_files_devnull`

**Type:** boolean

**Default:** `false`

If this setting is `true` and a file is opened with `O_WRONLY` included in the mode, the file path is replaced with `/dev/null`.

This is used in order to prevent .pid files from being created before the neck.
Since I/O behavior and control flow before the neck are frozen, a .pid file created before the neck would almost certainly be wrong for one reason or another (the wrong PID is written, a truncated PID is written, a PID followed by uninitialized memory is written, ...).

Note that .pid files are an obsolete and inherently race-condition-causing mechanism for process supervision, and can be emulated if required for compatibility with pre-modern process supervision systems.

### `/sys_stat`

**Manual:** [stat(2)](https://man.archlinux.org/man/stat.2.en)

#### `/sys_stat/mapping`

**Type:** object of strings

**Default:** `{}`

See [`/sys_open/mapping`](#sys_openmapping); this setting functions identically to it, but for `sys_stat`.

### `/sys_rt_sigaction`

**Manual:** [sigaction(2)](https://man.archlinux.org/man/core/man-pages/sigaction.2.en)

#### `/sys_rt_sigaction/replace_sighup`

**Type:** boolean

**Default:** none

Setting this mock is necessary to set a signal handler for the `SIGHUP` signal, due to its customary use as a notification to the program requesting it to reload its configuration.

When set to `true`, calls to `rt_sigaction` before the neck that would establish a `SIGHUP` handler instead become no-ops.

When set to `false`, those calls work as usual.

### `/sys_ioctl`

**Manual:** [ioctl(2)](https://man.archlinux.org/man/ioctl.2.en)

#### `/sys_ioctl/terminal_dimensions`

**Type:** object of numbers

**Default:** `{ "row": 0, "col": 0, "xpixel": 0, "ypixel": 0 }`

Sets the terminal dimensions given by the `TIOCGWINSZ` ioctl at instrumentation time if used before the neck.

See [`/sys_ioctl/unsafe_allow_tiocgwinsz`](#sys_ioctlunsafe_allow_tiocgwinsz) for caveats, however.

#### `/sys_ioctl/unsafe_allow_tiocgwinsz`

**Type:** boolean

**Default:** `false`

Allows the use of the `TIOCGWINSZ` ioctl before the neck, *without* making provisions to ensure that the same kind of file descriptor is present when the final specialized binary runs.
Depending on how the program uses the results, it's possible that memory unsafety could result.

Using it only to check if stdin is a TTY, and caching it for the lifetime of the program, is a use that should be safe.
An example of an unsound use is calling it to size a buffer without caching it, and calling it again before iterating over the buffer, relying on `SIGWINCH` to notify the program that a resize is necessary.

### `/sys_socket`

**Manual:** [socket(2)](https://man.archlinux.org/man/socket.2.en)

#### `/sys_socket/allow`

**Type:** object of booleans

**Default:** `{ "af_inet": false, "af_inet6": false, "af_unix": false }`

This allows for explicitly allowing various address families of sockets.
By default, all address families are disallowed, and the syscall will return `-EAFNOSUPPORT`.

Some programs will opportunistically use the network in pre-neck code, but have a reasonable fallback if they cannot do so.
However, others require being able to create the socket pre-neck, so this setting allows permitting that.

Allowing arbitrary network operations before the neck is a precarious position, since we don't have any control over the other side of the network connection, nor of the strange things networks do that may be observable to a process but aren't actually reproducible.

The supported address families are the ones given in the default value.

### `/sys_bind`

**Manual:** [bind(2)](https://man.archlinux.org/man/bind.2.en)

#### `/sys_bind/allow`

**Type:** object of booleans

**Default:** `{ "af_inet": false, "af_inet6": false }`

This allows for explicitly allowing various address families of sockets.
By default, all address families are disallowed, and the syscall will return `-EINVAL`.

The supported address families are the ones given in the default value.

### `/sys_getuid`

**Manual:** [getuid(2)](https://man.archlinux.org/man/getuid.2.en)

#### `/sys_getuid/uid`

**Type:** integer

**Default:** UID of the TaBaCCo process

Replaces the UID returned by `getuid` calls running pre-neck.

Useful for programs that check if they're running as root (or not as root) and have different behavior depending on this.
Also useful (alongside using `/sys_open/mapping` to replace `/etc/passwd` and `/etc/group`) to make a debloating specification portable between users.

### `/sys_geteuid`

**Manual:** [getuid(2)](https://man.archlinux.org/man/getuid.2.en)

#### `/sys_geteuid/euid`

**Type:** integer

**Default:** EUID of the TaBaCCo process

See [`/sys_getuid/uid`](#sys_getuiduid); this setting functions identically to it, but for `sys_geteuid`.


### `/sys_getgid`

**Manual:** [getgid(2)](https://man.archlinux.org/man/getgid.2.en)

#### `/sys_getgid`

**Type:** integer

**Default:** GID of the TaBaCCo process

Replaces the GID returned by `getgid` calls running pre-neck.

