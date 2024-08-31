# Tutorial: Debloating cURL

## Prerequisites

You need TaBaCCo to be installed.
If you haven't finished the [Installing TaBaCCo](./installing.md) tutorial, do so now.

## Screencast

{{#asciinema curl}}

## Instructions

In this tutorial, we'll debloat cURL, a command-line tool for transferring data over the network.

All the commands in this tutorial are meant to be run in the LMCAS Docker container.
The container already contains all the files we'll need.

### Building cURL

cURL's sources are in `/usr/src`.
To get started, we can extract them into our home folder inside the container.

```shell
lmcas ~ # cd ~
lmcas ~ # tar xjf /usr/src/curl-7.83.1.tar.bz2
lmcas ~ # cd curl-7.83.1
```

Currently, TaBaCCo only supports shared libraries that are written in C.
OpenSSL, however, has large parts written directly in assembly, to avoid certain cryptographic side-channel attacks.
Support for libraries containing a mix of C and assembly is ongoing work.
Therefore, we configure cURL to build without OpenSSL.

In addition, due to cURL's use of libtool, we also need to install it somewhere before it will be runnable.
Since we don't want to install a version that hasn't been debloated globally, we instead install it into a directory in our home folder, which we can safely delete after the debloating process completes.

```shell
lmcas ~/curl-7.83.1 # ./configure --disable-shared --prefix=$HOME/curl-fake-install --without-ssl
...
```

Next, we build cURL.

The `-j $(nproc) -l $(nproc)` flags specify that the build should spawn up to as many parallel copies of the compiler as there are cores, without making the load average exceed the number of available cores.
This is technically not necessary, but on modern machines often significantly speeds up the build.


```shell
lmcas ~/curl-7.83.1 # make -j $(nproc) -l $(nproc)
...
```

This isn't the final build of cURL though, since we need to patch it to add the neck marker.
The patch for this is provided in `/root/curl/neck.patch`.

```shell
lmcas ~/curl-7.83.1 # patch -p1 < /root/curl/neck.patch
patching file src/tool_operate.c
```

Then, we can run `make` again to rebuild cURL with the neck marker inserted.
We don't bother with the `-j` and `-l` flags, since this should only rebuild 2 files.

In principle, we could have patched before building the first time, but this makes any build issues easier to debug, since it's clear which are caused by the patch and which are not.

```
lmcas ~/curl-7.83.1 # make
...
```

Finally, we install cURL into the fake directory we configured earlier.

```
lmcas ~/curl-7.83.1 # make install -j $(nproc) -l $(nproc)
...
lmcas ~/curl-7.83.1 # file ~/curl-fake-install/bin/curl
/root/curl-fake-install/bin/curl: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /nix/store/nhhr7xl7m3y3fd5i22n6iqwq1brlw5ka-musl-1.2.3/lib/ld-musl-x86_64.so.1, with debug_info, not stripped
```

### Debloating

To debloat, we need to write a debloating specification to pass to TaBaCCo.
This is a file that instructs TaBaCCo how to debloat the program.
The debloating specification we'll be using is in `spec.json`.

```shell
lmcas ~/curl-7.83.1 # cd ~/curl
lmcas ~/curl # less spec.json
...
```

The file is reproduced here in a separate code block for syntax highlighting.
```json
{
  "binary": "/root/curl-fake-install/bin/curl",
  "args": [
    "curl",
    "-X",
    "POST",
    "-d",
    "{\"title\": \"Curl POST\",\"body\": \"Curl POST example body\",\"userId\":1}",
    "http://127.0.0.1:8000/example"
  ],
  "env": {
    "CWD": "/",
    "CURL_HOME": "/fake-curl-home",
    "PATH": ""
  },
  "cwd": "/",
  "syscall_mocks": {
    "sys_open": {
      "mapping": {
        "/fake-curl-home/.curlrc": "curlrc"
      }
    }
  }
}
```


```shell
lmcas ~/curl # less curlrc 
...
```

The `curlrc` file is reproduced below.

```plaintext
header = "Content-Type: application/json"
silent
user-agent = "TaBaCCo Example"
```

Combining the configuration specified with the command-line arguments (the `args` field of the debloating specification) and the configuration file results in specializing cURL to the following behavior:

- Always request `http://127.0.0.1:8000/example`
- Always send POST requests
- Send the string `{"title":"Curl POST","body":"Curl POST example body","userId":1}` as the body of the request
- Send the request header `Content-Type: application/json`
- Send the request header `User-Agent: TaBaCCo Example`

We can run `tabacco`, passing it the debloating specification, to produce a debloated binary that specializes to that behavior.

```shell
lmcas ~/curl # tabacco -o debloated spec.json
Bitcode file extracted to: /tmp/tmpnp5cinxj/input/curl.bc.
Step 1: Extract the bitcode
get-bc		[0:00:00.352394]
llvm-link		[0:00:00.320174]
Step 2: Annotate the bitcode
opt		[0:00:00.428866]
Step 3: Instrument the annotated bitcode
opt		[0:00:00.544547]
Step 4: Compile the instrumented bitcode
llc		[0:00:05.338468]
Step 5: Assemble the instrumented program
as		[0:00:00.220872]
Step 6: Link the instrumented program
ld		[0:00:00.043065]
Step 7: Run the instrumented program to produce a tape
instrumentation-parent		[0:00:00.435910]
Step 8: Specialize the annotated bitcode using the tape
llvm-link		[0:00:00.950759]
opt		[0:00:14.549299]
Step 9: Optimize the specialized bitcode
opt		[0:00:10.440603]
Step 10: Compile the final bitcode
llc		[0:00:07.234578]
Step 11: Assemble the final program
as		[0:00:00.482209]
Step 12: Link the final program
ld		[0:00:00.041797]
```

This creates the binary as `debloated`.

To test it, the container contains a tiny example web server. It can be started in the background via:


```shell
lmcas ~/curl # ~/test_http.py &
[1] 69163
```

Now we run the debloated curl binary, which makes a request to the web server.

When it connects to the web server, the web server prints information about the request; all the lines but the last (`Hiya!`) are from the web server, and describe what it received.

```


lmcas ~/curl # ~/curl/debloated 
Got a POST request!
Host: 127.0.0.1:8000
User-Agent: TaBaCCo Example
Accept: */*
Content-Type: application/json
Content-Length: 66
Body: {"title": "Curl POST", "body": "Curl POST example body", "userId": 1}
127.0.0.1 - - [24/Jan/2023 05:06:39] "POST /example HTTP/1.1" 200 -
Hiya!
```


