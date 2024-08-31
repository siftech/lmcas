# How to choose a neck

The neck of a program marks the divison between the *head*, where configuration occurs, and the *body*, where the majority of the program's actual work is performed. 
The placement of the neck is crucial to the TaBaCCo approach: it indicates to our tool how much of the target program's execution we should "lock in" by freezing control flow, the values of command-line arguments provided in the debloating specification file, and some I/O operations.

TaBaCCo includes a component, Neck Miner, which automatically and heuristically identifies necks.
In this release, this component is in a relatively immature state.
There is currently no user interaction available for this tool and it cannot yet be run independently of TaBaCCo. 
However, it already correctly identifies a neck in the majority of the programs we've tested it with.

Manually selecting a neck is only necessary in cases where the automatically identified neck is incorrect or suboptimal.
This manual selection is not always a straightforward task for complex programs because there can be many technically valid necks (see "Mathematical definition of a neck" below). 
This said, there are common code patterns to look out for during the selection process that can help you choose the best neck for your program's needs.

## Manual neck selection

There are a few things to consider when inspecting source code to manually select a neck location. 
It is essential that the neck placement is a correct and valid neck by definition, but it is also very important that the neck placement results in a meaningfully specialized program. 
In fact, there may be many definitionally valid neck candidates in a program, but not all of these candidates will result in a binary specialized in the way the user wants once debloated with TaBaCCo. 

As an example, a technically valid neck placement for most programs is right at the start of the program's `main`, before both configuration and program work is performed. 
However, using this neck placement is likely to only freeze these input arguments as a prefix and will not prevent additional arguments from acting like options. 
A neck placed later in that same program could result in much more of the program configuration being fixed to your desired settings.

In contrast, placing the neck too late in the program runs the risk of freezing more values than you would like. 
For example, a neck placed after a file is read will freeze the binary's view of its contents as encountered during the specialization process.
While this might be desired if that file is a configuration file the user has no intention of changing, if this file is expected to be edited between separate runs of the specialized binary the "old" values that were fixed during specialization will be used instead of any other updated data. 
Thus, an ideal neck placement follows a Goldilocks principle: it should not be too early or too late, but just right.

## Steps

There are a few steps you can take to identify the right placement for a program's neck.

1. The best place to begin searching for the neck is by locating the `main` function of the program you would like to specialize. 
While not all necks are in a program's `main` function (for example, `curl` has a non-`main` neck), it is still very common to find them here. 
In fact, for many tools, the entire configuration step often lives in the `main` function and selecting a neck placement can be as easy as identifying some of the variables saved during the configuration process and then inserting the neck function call before any of them are used for actual program work. 
Even in more complicated examples, beginning at the `main` function of the program and examining the code here is a good first step.

2. After you have found `main`, the next step is to find where the command line arguments are parsed. 
   In simple programs, this is often a `while` loop calling `getopt`. This tends to look something like:

   ```c
         int opt;
         while ((opt = getopt(argc, argv, "ex")) != -1) {
           switch (opt) {
           case 'e':
             eflag = 1;
             break;
           case 'x':
             xflag = 1;
             break;
             ...
   ```

   The neck should be placed after these argument-processing loops.

3. More complex programs might handle this in their own separate function written to parse and set configuration values. 
If configuration is performed outside of `main` in a separate function, you should locate the definition of this function and ensure that it does not also read files and set values that should change between separate runs of the specialized program.

4. If you would like to specialize the target program against a specific configuration file (that you don't intend to change), you should place the neck after this configuration file is read and values in the program determined from the configuration settings have been set. 
If you intend to change this (or any other files), the neck should be placed before this configuration file is read.

5. If you begin to see real program work being performed within the source code, like a loop on `epoll_wait` to listen for events, then you have likely gone too far and should place the neck earlier in the source code.

6. Once you have chosen your neck placement, you should identify the neck to TaBaCCo with a function call to `_lmcas_neck()`, keeping in mind that the definition of this function should also be supplied somewhere in source code prior to its use.
This must take the following form, where the definition and function call should be added to the source code using exactly this syntax:

```c
__attribute__((noinline)) void _lmcas_neck(void) {}

int main() {
    ...

    _lmcas_neck();

    ...
}
```

## If there's no neck

The novelty of TaBaCCo's approach is the concept that having a neck divides the program into configuration, which is a smaller portion of the program that's responsible for initializing and setting program values, and the rest of the program, whose behavior solely depends on the values given to the configuration. 
If the two parts are more intertwined, such as in a program able to modify its own configuration during runtime, then TabaCCo cannot specialize these binaries in most cases.

## Mathematical definition of a neck

The following criteria must be true of a neck:

- Every basic block [reachable] from the neck is [strictly dominated] by the neck...
  - ...after tail duplication (see below)
- Does not dominate an instance of the [`unreachable`] instruction
- For every stack frame "above" the point the neck is at, the point the current stack frame would return to meets all these criteria.

More criteria may be necessary in the future.

[reachable]: https://en.wikipedia.org/wiki/Reachability
[strictly dominated]: https://en.wikipedia.org/wiki/Dominator_(graph_theory)
[`unreachable`]: https://releases.llvm.org/12.0.1/docs/LangRef.html#unreachable-instruction
