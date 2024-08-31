# Tutorial: Debloating Wget

## Prerequisites

You need TaBaCCo to be installed.
If you haven't finished the [Installing TaBaCCo from a registry](./installing-from-registry.md) or [Installing TaBaCCo from a tarball](./installing-from-tarball.md) tutorials, do so now.

## Instructions

In this tutorial, we'll debloat GNU Wget, a command-line tool for transferring data over the network.

All the commands in this tutorial are meant to be run in the LMCAS Docker container.
The container already contains all the files we'll need.

### Building zlib

Wget has a library dependency we'll need to build before we get on with building Wget itself: zlib.
If you've already built and installed it as part of another tutorial, you can skip this section.

We've included a source release of zlib 1.2.12 in /usr/src; we can extract that tarball to get started.

```shell
lmcas ~ # cd ~
lmcas ~ # tar xzf /usr/src/zlib-1.2.12.tar.gz
lmcas ~ # cd zlib-1.2.12
```

As with many programs written in C, there are 3 stages in getting zlib: configuring, building, and installing.
First, we configure with the `configure` script.

```shell
lmcas ~/zlib-1.2.12 # ./configure
...
```

Next, we build zlib.
The `-j $(nproc) -l $(nproc)` flags specify that the build should spawn up to as many parallel copies of the compiler as there are cores, without making the load average exceed the number of available cores.
These flags are technically not necessary, but on modern machines often significantly speed up the build.

```shell
lmcas ~/zlib-1.2.12 # make -j $(nproc) -l $(nproc)
...
```

Finally, we install zlib, copying the files we've just built to the correct places to get a working installation.

```shell
lmcas ~/zlib-1.2.12 # make install -j $(nproc) -l $(nproc)
...
```

After this, the libz library should be present.

```shell
lmcas ~/zlib-1.2.12 # file /usr/local/lib/libz.a
/usr/local/lib/libz.a: current ar archive
```

### Building Wget

Wget's sources are in `/usr/src`.
To get started, we can extract them into our home folder inside the container.

```shell
lmcas ~ # cd ~
lmcas ~ # tar xaf /usr/src/wget-1.21.3.tar.lz
lmcas ~ # cd wget-1.21.3
```

By default, Wget attempts to link against OpenSSL.
Currently, TaBaCCo only supports shared libraries that are written in C.
OpenSSL, however, has large parts written directly in assembly, to avoid certain cryptographic side-channel attacks.
Support for libraries containing a mix of C and assembly is ongoing work.
Therefore, we configure Wget to build without requiring OpenSSL.

```shell
lmcas ~/wget-1.21.3 # ./configure --without-ssl
...
```

Next, we build Wget.

```shell
lmcas ~/wget-1.21.3 # make -j $(nproc) -l $(nproc)
...
```

### Debloating

To debloat, we need to write a debloating specification to pass to TaBaCCo.
This is a file that instructs TaBaCCo how to debloat the program.
The debloating specification we'll be using is in `spec.json`.

```shell
lmcas ~/wget-1.21.3 # cd ~/wget
lmcas ~/wget # less spec.json
...
```

The file is reproduced here in a separate code block for syntax highlighting.

```json
{
  "binary": "/root/wget-1.21.3/src/wget",
  "configs": [
    {
      "args": ["wget", "--method=GET", "-O-"],
      "env": {
        "CWD": "/",
        "PATH": "/bin"
      },
      "cwd": "/",
      "syscall_mocks": {
        "sys_open": {
          "mapping": {
            "/etc/passwd": "passwd",
            "/home/debloatuser/.wgetrc": "wgetrc"
          }
        },
        "sys_stat": {
          "mapping": {
            "/etc/passwd": "passwd",
            "/home/debloatuser/.wgetrc": "wgetrc"
          }
        }
      }
    }
  ]
}
```

The `mapping` blocks in this `spec.json` cause calls to those syscalls with the paths on the left to be redirected to those on the right.
Normally, `wget` would read from these files, but this allows us to specify files that are in more convenient locations we wish to override them with.
The `wgetrc` file configures `wget`, while `passwd` is used to locate it.

The `mapping` blocks are contained within the `syscall_mocks` section, which are described in more detail in the [Syscall Mocks](../topic-guides/syscall-mocks.md) guide.

```shell
lmcas ~/wget # less wgetrc
...
```

The `wgetrc` file is reproduced below as well.

```plaintext
header = Content-Type: application/json
quiet = on
user-agent = TaBaCCo Example
```

Combining the configuration specified with the command-line arguments (the `args` field of the debloating specification) and the configuration file results in specializing `wget` to the following behavior:

- Always send GET requests.
- Output the response to stdout.
- Send the request header `Content-Type: application/json`
- Send the request header `User-Agent: TaBaCCo Example`
- Be less verbose in logging.

We can run `tabacco`, passing it the debloating specification, to produce a debloated binary that specializes to that behavior.
Since the debloating specification contains the path to the binary, we don't have to provide it again.

As part of debloating, `tabacco` also runs a robustness checker, which uses the syscall mocks to vary elements like uid,pid and gid, among others.
This is done in order to check for unexpected nondeterministic behavior.
More information on the robustness checker is available in the [Robustness Checker](../topic-guides/robustness-checker.md) guide.
In the case of wget, we will see that variation in UID causes it to differ because it calls `sys_getuid` during execution.


```shell
lmcas ~/wget # tabacco -o wget-debloated spec.json
Step 1: Extract the bitcode
Bitcode file extracted to: /tmp/tmpkk0pednp/input/wget.bc.
get-bc		[0:00:00.183921]
Bitcode file extracted to: /tmp/tmpkk0pednp/input/libz.so.1.2.12.bc.
get-bc		[0:00:00.046909]
llvm-link		[0:00:00.194250]
Step 2: Link in noop signal handler
llvm-link		[0:00:00.199945]
Step 3: Annotate the bitcode
opt		[0:00:00.271585]
Step 4: Mark the neck
[{"function": "main", "basic_block_name": "%383", "basic_block_annotation_id": "1073758725", "insn_index": 0}]
neck-miner		[0:00:19.961026]
Step 5: Instrument the annotated bitcode
opt		[0:00:00.353441]
Step 6: Compile the instrumented bitcode
llc		[0:00:04.129800]
Step 7: Assemble the instrumented program
as		[0:00:00.189926]
Step 8: Link the instrumented program
ld		[0:00:00.043613]
Step 9: Run tape robustness checker.
Step 10: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.174354]
Step 11: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.175486]
Step 12: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.174109]
tape-determinism-checker		[0:00:00.065022]
When we varied uid and ran the program, we found a difference in execution behavior
tape elements are different at index 5678, the first has the following element
{'return_code': '1589', 'syscall': 'sys_getuid', 'type': 'syscall_start'}
and the second has the following element
{'return_code': '0', 'syscall': 'sys_getuid', 'type': 'syscall_start'}
It appears that this difference is due to differing behavior when the syscall sys_getuid is called.
For this syscall, you can mock the syscall in the following way to freeze it to a particular value (1000, in this case, but other values are acceptable), and remove this potential source of variation in execution.
"syscall_mocks": {
  "sys_getuid": {
    "uid": 1000
  }
}
Step 13: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.174825]
...
Step 24: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.184246]
tape-determinism-checker		[0:00:00.065071]
When we varied uid we saw a difference in program behavior, and thus are stopping tabacco until this is resolved.
Above this should be information about the variation and how it can be handled (if it can be).
```

In this case, we can make the program behave deterministically before the neck by mocking the UID returned by the `sys_getuid` syscall.
We do this by modifying the debloating specification to add the suggested syscall mock to `spec.json` in the `syscall_mocks` section.
This can be done with the `vim` or `nano` editors inside the container.

```shell
lmcas ~/wget # vim spec.json
```

```
...
  "syscall_mocks": {
    "sys_getuid": {
      "uid": 1000
    },
    "sys_open": {
      "mapping": {
        "/etc/passwd": "passwd",
        "/home/debloatuser/.wgetrc": "wgetrc"
      }
    },
...
```

Now we run TaBaCCo again with the fixed debloating specification.

```
lmcas ~/wget # tabacco -o wget-debloated spec.json
Step 1: Extract the bitcode
Bitcode file extracted to: /tmp/tmp_qsddews/input/wget.bc.
get-bc		[0:00:00.180169]
Bitcode file extracted to: /tmp/tmp_qsddews/input/libz.so.1.2.12.bc.
get-bc		[0:00:00.045860]
llvm-link		[0:00:00.193514]
Step 2: Link in noop signal handler
llvm-link		[0:00:00.197300]
Step 3: Annotate the bitcode
opt		[0:00:00.258795]
Step 4: Mark the neck
[{"function": "main", "basic_block_name": "%383", "basic_block_annotation_id": "1073758725", "insn_index": 0}]
neck-miner		[0:00:18.877302]
Step 5: Instrument the annotated bitcode
opt		[0:00:00.359580]
Step 6: Compile the instrumented bitcode
llc		[0:00:04.167459]
Step 7: Assemble the instrumented program
as		[0:00:00.188550]
Step 8: Link the instrumented program
ld		[0:00:00.044079]
Step 9: Run tape robustness checker.
Step 10: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.171879]
...
Step 21: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.171854]
tape-determinism-checker		[0:00:00.066160]
Robustness checker did not find a difference.
Step 22: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.238146]
Step 23: Specialize the annotated bitcode using the tape
llvm-link		[0:00:00.807904]
opt		[0:00:04.161794]
Step 24: Optimize the specialized bitcode
opt		[0:00:09.472064]
Step 25: Compile the final bitcode
llc		[0:00:07.366702]
Step 26: Assemble the final program
as		[0:00:00.486811]
Step 27: Link the final program
ld		[0:00:00.045139]
strip		[0:00:00.005533]
Step 28: Optimize the specialized bitcode
opt		[0:00:03.242638]
llvm-link		[0:00:00.808075]
Step 29: Compile the final bitcode
llc		[0:00:07.096364]
Step 30: Assemble the final program
as		[0:00:00.443382]
Step 31: Link the final program
ld		[0:00:00.044123]
strip		[0:00:00.005271]
     VM SIZE    
 -------------- 
  +2.1% +5.91Ki    LOAD #2 [R]
  -1.0%    -936    LOAD #3 [RW]
  -2.0% -8.82Ki    LOAD #1 [RX]
  -0.5% -3.83Ki    TOTAL
bloaty		[0:00:00.002473]
```

This creates the binary as `wget-debloated`.

### Testing

To test the debloated binary we've created, the container contains a tiny example web server.
It can be started in the background via:

```shell
lmcas ~/wget # ~/test_http.py &
[1] 23420
```

Now we run the debloated binary, which makes a request to the web server.

When a client connects to the web server, the web server prints information about the request.
The block below contains mixed output from the client and server.
All the lines but the last (`Got a GET request; hello!`) are from the web server, which describe what it received.

```
lmcas ~/wget # ./wget-debloated --method=GET -O- http://localhost:8000/
Got a GET request!
Host: localhost:8000
User-Agent: TaBaCCo Example
Accept: */*
Accept-Encoding: identity
Connection: Keep-Alive
Content-Type: application/json
127.0.0.1 - - [10/Apr/2023 23:17:18] "GET / HTTP/1.1" 200 -
Got a GET request; hello!
```

We can also see that additional arguments passed to the debloated binary are treated as URLs, not as options.

```
lmcas ~/wget # strace -e sendto ./wget-debloated --method=GET -O- --no-check-certificate http://localhost:8000/                                                                           
sendto(3, "\5\227\1\0\0\1\0\0\0\0\0\0\26--no-check-certific"..., 57, MSG_NOSIGNAL, {sa_family=AF_INET, sin_port=htons(53), sin_addr=inet_addr("10.0.2.3")}, 16) = 57
...
+++ exited with 4 +++
```

The exit code 4 corresponds to a network error, and is used when a domain could not be resolved by any DNS server.
As we can see from the output of `strace`, the domain name that `wget` tried to look up was `--no-check-certificate`.

To stop the `~/test_http.py` server we started earlier, we can foreground it and hit <kbd>Ctrl-C</kbd>.

```
lmcas ~/wget # fg
~/test_http.py
^CTraceback (most recent call last):
  File "/root/test_http.py", line 35, in <module>
    HTTPServer(("", 8000), Handler).serve_forever()
  File "/nix/store/m9950wwkbrwb00zw9i4q0snw85rlnf40-python3-3.9.13/lib/python3.9/socketserver.py", line 232, in serve_forever
    ready = selector.select(poll_interval)
  File "/nix/store/m9950wwkbrwb00zw9i4q0snw85rlnf40-python3-3.9.13/lib/python3.9/selectors.py", line 416, in select
    fd_event_list = self._selector.poll(timeout)
KeyboardInterrupt
```

## Comparison

We can build a non-debloated binary with the same compilation flags to compare the functionality of the two programs.

For a variety of reasons, the input binaries to TaBaCCo are not good targets for comparison with the debloated binaries TaBaCCo produces.
Because of this, we have the `-N` (or `--no-debloat`) flag, which instructs TaBaCCo to not actually debloat the binary, but compile it in a similar way to a binary it would debloat.

Running TaBaCCo with this flag produces similar, but shorter output.

```
lmcas ~/wget # tabacco -N -o wget-nondebloated spec.json
Step 1: Extract the bitcode
Bitcode file extracted to: /tmp/tmp984x0j65/input/libz.so.1.2.12.bc.
get-bc          [0:00:00.049534]
Bitcode file extracted to: /tmp/tmp984x0j65/input/wget.bc.
get-bc          [0:00:00.151916]
llvm-link               [0:00:00.157193]
Step 2: Optimize the specialized bitcode
opt             [0:00:02.234269]
llvm-link               [0:00:00.581650]
Step 3: Compile the final bitcode
llc             [0:00:04.997378]
Step 4: Assemble the final program
as              [0:00:00.348587]
Step 5: Link the final program
ld              [0:00:00.066581]
```

To test, we'll need to restart `test_http.py` if it was shut down at the end of the last seciton.

```shell
lmcas ~/wget # ~/test_http.py &
[1] 61411
```

When we run the non-debloated binary, we get a lot more output from `wget` itself, but we also see different headers being sent to the server.

```
lmcas ~/wget # ./wget-nondebloated --method=GET -O- http://localhost:8000/
--2023-04-10 23:19:35--  http://localhost:8000/
Resolving localhost... ::1, 127.0.0.1
Connecting to localhost|::1|:8000... failed: Connection refused.
Connecting to localhost|127.0.0.1|:8000... connected.
HTTP request sent, awaiting response... Got a GET request!
Host: localhost:8000
User-Agent: Wget/1.21.3
Accept: */*
Accept-Encoding: identity
Connection: Keep-Alive
127.0.0.1 - - [10/Apr/2023 23:19:35] "GET / HTTP/1.1" 200 -
200 OK
Length: unspecified [text/plain]
Saving to: ‘STDOUT’

-                                                  [<=>                                                                                                 ]       0  --.-KB/s               Got a GET request; hello!
-                                                  [ <=>                                                                                                ]      26  --.-KB/s    in 0s      

2023-04-10 23:19:35 (345 KB/s) - written to stdout [26]
```

This shows that the options we configured were not present, because they're not the defaults.
