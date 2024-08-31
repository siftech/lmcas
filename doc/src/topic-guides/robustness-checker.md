# Robustness Checker

As part of debloating, TaBaCCo runs a robustness checker, which uses the syscall mocks to vary elements like uid,pid and gid, among others.

It then checks that the variation does not produce differing behavior up to the program neck, as this variation might cause unexpected results when you run specialized binaries in a different context. 

When the robustness checker detects a difference, this causes debloating to fail, but you can resolve this in a few different ways.

- If the difference is due to a syscall that can be mocked, the program will suggest that you mock the syscall to a consistent value, and doing so will remove a source of variation in the program.
- If the difference is due to the binary calling syscalls `sys_getpid` or `sys_getppid`, this is a sign that the binary and neck position chosen are not supported by TaBaCCo. See the section "Programs that use PID" in the [Known issues](../references/known-issues.md) reference for more details. 
- If you do not want to use the robustness checker, you can pass in `--no-robustness-checks`, and the checks will not run. Note that any problems the robustness checker would catch will not be found as a result.

## Limitations

The robustness checker is currently limited to differences found when we vary `uid`, `euid`, `pid`, `ppid`, and `gid`.

The robustness checker is conservative in what it considers to be unallowed variation, so it might catch variation that is actually acceptable. For instance, if the program calls a syscall like `sys_uname`, which provides information on the system, and variation occurs in a field that is never actually used in the program, the program would still be rejected by the robustness checker. 
