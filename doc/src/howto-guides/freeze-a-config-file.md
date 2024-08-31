# How to freeze a config file

Configuration files are accessed using file I/O, so freezing them in currently requires configuring syscall mocks appropriately.
We'll start with a simple debloating specification, and see different ways a config file might be frozen in.

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {},
		"cwd": "/"
	}]
}
```

## By absolute path

In the simplest case, a config file is accessed by absolute path, such as `/etc/example.conf`.
In this case, we can map the config file to one of our choosing:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/etc/example.conf": "debloat-example.conf"
				}
			}
		}
	}]
}
```

If the program checks for the existence of the config file ahead of time using the `sys_stat` syscall, it may be necessary to add a similar mock for it:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/etc/example.conf": "debloat-example.conf"
				}
			},
			"sys_stat": {
				"mapping": {
					"/etc/example.conf": "debloat-example.conf"
				}
			}
		}
	}]
}
```

## By argument or environment variable

Some programs take the path to the config file as an argument (e.g. `--config` for GNU Wget) or an environment variable (e.g. `CURL_HOME` for cURL).
In this case, we generally recommend that these options be used to load the config file from a nonexistent path, and then a syscall mock be used to map a file to this path.
This typically results in a more portable debloating specification (in the sense of not accidentally depending on details of the machine running TaBaCCo).

An example of this might look as follows for a config file specified with a command-line argument:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example", "--config", "/does-not-exist/example.conf"],
		"env": {},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/does-not-exist/example.conf": "debloat-example.conf"
				}
			}
		}
	}]
}
```

Or like this for a config file specified with an environment variable:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {
			"EXAMPLE_HOME": "/does-not-exist"
		},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/does-not-exist/example.conf": "debloat-example.conf"
				}
			}
		}
	}]
}
```

## In a user's `.config` directory

Programs that attempt to comply with the [XDG Base Directory Specification] will, by default, look for configuration in `~/.config` of the user running them.
Thankfully, the specification gives an environment variable to control what directory is actually searched, `XDG_CONFIG_HOME`.
A configuration like the one above can be used for this case:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {
			"XDG_CONFIG_HOME": "/does-not-exist"
		},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/does-not-exist/example.conf": "debloat-example.conf"
				}
			}
		}
	}]
}
```

## In a user's home folder

Some programs additionally load a config file from the home folder of the user running them.
This is especially prone to leading to surprising differences between the environment where TaBaCCo was run and where the binary will be deployed, since the user typically differs between the two.

Often, the program will use the `HOME` environment variable to find the user's home directory.
In that case, a debloating specification that simply provides the environment variable can be used:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {
			"HOME": "/does-not-exist"
		},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/does-not-exist/example.conf": "debloat-example.conf"
				}
			}
		}
	}]
}
```

Sometimes, however, the program only looks at the current user's home directory, and not any more convenient environment variables.
In this case, we can take the relatively heavy-handed step of providing a replacement [`/etc/passwd`] file to use.
This file provides the source of truth for what a user's home directory is.

Often, a simple `passwd` file like the following will suffice:

```
root:x:0:0::/root:/bin/nologin
debloatuser:x:1000:100::/does-not-exist:/bin/nologin
```

This defines a user named `debloatuser` with a UID of 1000, a GID of 100, and a home directory of `/does-not-exist`.

We can then mock the `sys_getuid` syscall to return the UID 1000:

```json
{
	"binary": "example",
	"configs": [{
		"args": ["example"],
		"env": {},
		"cwd": "/",
		"syscall_mocks": {
			"sys_open": {
				"mapping": {
					"/does-not-exist/example.conf": "debloat-example.conf",
					"/etc/passwd": "passwd"
				}
			},
			"sys_getuid": {
				"uid": 1000
			}
		}
	}]
}
```

[`/etc/passwd`]: https://en.wikipedia.org/wiki/Passwd#Password_file
[XDG Base Directory Specification]: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
