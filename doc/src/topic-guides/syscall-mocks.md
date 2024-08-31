# Syscall mocks

Programs are often deployed to a very different environment from their build environment.
TaBaCCo, however, requires being able to run the program in an environment sufficiently similar to the final environment to accurately record the configuation the program will use.
For example, a program might behave differently depending on whether it is run as the root user.

Syscall mocks (as in [mock objects] in software testing) allows for emulation of parts of the final environment by modifying the behavior of syscalls.
Users configure mocks via the debloating specification.

[mock objects]: https://en.wikipedia.org/wiki/Mock_object

Currently, syscall mocks are fairly *ad hoc* and tightly coupled to the actual Linux system call interface, rather than to the generic Unixy abstractions (the filesystem, users, etc.).
A higher-level user interface is an area of future work.

Syscall mocks also apply only to the syscalls performed before the neck.
For some syscalls, for example `sys_open`, this is perfectly acceptable -- there is no guarantee that the filesystem remains unchanged between any two moments in time.
For others, for example `sys_getpid`, there is no mechanism for the system call's behavior to change without the program itself taking action to change it.

In the future, a heavier-weight sandboxing approach that persists past the neck may be added as an optional feature, preventing this problem.
