# Debloating specification schema

<!-- toc -->

## Example

An example schema is as follows.
All possible fields are present, but not all have coherent values.

```json
{
	"binary": "/home/example/foo",
	"configs": [
		{
			"args": ["foo", "--bar"],
			"env": {
				"FOO_CONFIG": "/example-config-file.txt"
			},
			"cwd": "/",
			"syscall_mocks": {
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
					},
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
			}
		},
		{
			"args": ["foo", "--baz"]
		}
	]
}
```

## Settings

### `/binary`

**Type:** string

The path to the input binary, which must have been compiled and linked using `musl-gclang`.

### `/configs`

**Type:** non-empty array of option configurations

The list of configurations that should be debloated with.
The output program will be runnable with all of the lists of arguments provided.

For example, if a program should be runnable as either `foo --bar some-input` or `foo --baz some-other-input`, there might be two configs, one with an `args` of `["foo", "--bar"]` and one with an `args` of `["foo", "--baz"]`.

### `/configs/*/args`

**Type:** non-empty array of strings

The arguments provided to the program, including `argv[0]`.

Although `execve()` supports an empty `argv`, TaBaCCo does not.

### `/configs/*/env`

**Type:** object of strings

The environment variables provided to the program.

### `/configs/*/cwd`

**Type:** string

The working directory in which to run the program when recording the tape.

### `/configs/*/syscall_mocks`

**Type:** optional object

Configuration for mocking syscalls.
See [the reference page on syscall mocks](./syscall-mocks.md) for more information.

