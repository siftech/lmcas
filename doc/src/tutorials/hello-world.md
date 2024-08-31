# Tutorial: Debloating a simple program

## Prerequisites

You need TaBaCCo to be installed.
If you haven't finished the [Installing TaBaCCo from a registry](./installing-from-registry.md) or [Installing TaBaCCo from a tarball](./installing-from-tarball.md) tutorials, do so now.

## Instructions

In this tutorial, we'll debloat a simple C program, to see how to use TaBaCCo.
All the commands in this tutorial are meant to be run in the LMCAS Docker container.

The container already contains all the files we'll need in the `~/example` directory.

```shell
lmcas ~ # cd ~/example/
lmcas ~/example # ls
main.c  spec.json
```

If we look at the `main.c` file, we can see the source code of the program.

```shell
lmcas ~/example # less main.c
...
```

Press <kbd>q</kbd> to exit `less`.
The file is reproduced here with syntax highlighting.

```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  enum { MODE_INVALID, MODE_HELLO, MODE_LS } mode = MODE_INVALID;

  int opt;
  while ((opt = getopt(argc, argv, "hl")) != -1) {
    switch (opt) {
    case 'h':
      mode = MODE_HELLO;
      break;
    case 'l':
      mode = MODE_LS;
      break;
    default:
      fprintf(stderr, "Usage: %s <-h|-l>\n", argv[0]);
      return -1;
    }
  }

  switch (mode) {
  case MODE_HELLO:
    puts("Hello, world!");
    break;
  case MODE_LS:
    return system("ls");
  default:
    fprintf(stderr, "Error: invalid mode\n");
    return 1;
  }

  return 0;
}
```

This program accepts either the `-h` flag or `-l` flag.
If the last flag it received was `-h`, it prints `Hello, world!`.
If the last flag it received was `-l`, it runs `ls` using the `system()` function.

We'll be debloating this program to lock in the `-h` flag, removing the `-l` functionality.

Before we can do that, we need to compile the program.
TaBaCCo ships with a wrapper for the Clang compiler named `musl-gclang`, which should be used for any program or library that is used as input to TaBaCCo.

```shell
lmcas ~/example # musl-gclang main.c -o main
```

We can then try it out, to verify that it works as we expect.

```shell
lmcas ~/example # ./main -h
Hello, world!
lmcas ~/example # ./main -l
main  main.c  spec.json
```

TaBaCCo takes a debloating specification as input.
This is a file that instructs TaBaCCo how to debloat the program.
The debloating specification we'll be using is in `spec.json`.

```shell
lmcas ~/example # less spec.json
...
```

The file is again reproduced in a separate code block for syntax highlighting.

```json
{
  "binary": "/root/example/main",
  "configs": [
    {
      "args": [
        "main",
        "-h"
      ],
      "env": {
        "CWD": "/",
        "PATH": "/bin"
      },
      "cwd": "/"
    }
  ]
}
```

We can run `tabacco`, passing it the debloating specification, to produce a debloated binary.

```shell
lmcas ~/example # tabacco -o debloated spec.json
Step 1: Extract the bitcode
Bitcode file extracted to: /tmp/tmp7t73gqk8/input/main.bc.
get-bc          [0:00:00.036664]
llvm-link               [0:00:00.025079]
Step 2: Link in noop signal handler
llvm-link               [0:00:00.025951]
Step 3: Annotate the bitcode
opt             [0:00:00.031559]
Step 4: Mark the neck
[{"function": "main", "basic_block_name": "%24", "basic_block_annotation_id": "1073741832", "insn_index": 0}]
neck-miner              [0:00:00.048721]
Step 5: Instrument the annotated bitcode
opt             [0:00:00.036659]
Step 6: Compile the instrumented bitcode
llc             [0:00:00.056744]
Step 7: Assemble the instrumented program
as              [0:00:00.010989]
Step 8: Link the instrumented program
ld              [0:00:00.072494]
Step 9: Run the instrumented program to produce a tape
instrumentation-parent          [0:00:00.011607]
Step 10: Specialize the annotated bitcode using the tape
llvm-link               [0:00:00.933959]
opt             [0:00:01.251383]
Step 11: Optimize the specialized bitcode
opt             [0:00:06.846649]
Step 12: Compile the final bitcode
llc             [0:00:06.952134]
Step 13: Assemble the final program
as              [0:00:00.606130]
Step 14: Link the final program
ld              [0:00:00.193259]
strip           [0:00:00.012408]
Step 15: Optimize the specialized bitcode
opt             [0:00:00.038013]
llvm-link               [0:00:00.866927]
Step 16: Compile the final bitcode
llc             [0:00:06.101373]
Step 17: Assemble the final program
as              [0:00:00.496381]
Step 18: Link the final program
ld              [0:00:00.056891]
strip           [0:00:00.009701]
     VM SIZE    
 -------------- 
   +50% +4.37Ki    LOAD #2 [R]
 -14.5% -1.39Ki    LOAD #3 [RW]
 -32.8% -11.5Ki    LOAD #1 [RX]
 -15.8% -8.51Ki    TOTAL
bloaty          [0:00:00.008979]
```

This creates the binary as `debloated`.
We can try running it, and see that it works the same way `./main -h` did, without accepting more options.

```shell
lmcas ~/example # ./debloated 
USAGE: main -h [ARGS...]
lmcas ~/example # ./debloated -h
Hello, world!
lmcas ~/example # ./debloated -l 
USAGE: main -h [ARGS...]
lmcas ~/example # ./debloated -h -l
Hello, world!
lmcas ~/example # ./debloated -h -l -l -l
Hello, world!
lmcas ~/example # ./debloated -h --this-is-an-invalid-option
Hello, world!
lmcas ~/example # ./debloated -h -t
Hello, world!
```
