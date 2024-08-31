
 _      __  __  _____           _____   _______    ____         _____ _____
| |    |  \/  |/ ____|   /\    / ____| |__   __|  |  _ \       / ____/ ____|
| |    | \  / | |       /  \  | (___      | | __ _| |_) | __ _| |   | |     ___
| |    | |\/| | |      / /\ \  \___ \     | |/ _` |  _ < / _` | |   | |    / _ \
| |____| |  | | |____ / ____ \ ____) |    | | (_| | |_) | (_| | |___| |___| (_) |
|______|_|  |_|\_____/_/    \_\_____/     |_|\__,_|____/ \__,_|\_____\_____\___/

=================================================================================

The LMCAS TaBaCCo (Tape-Based Control-flow Concretization) tool functions both
as a debloating tool and as a way to strip functionality out of a program.
LMCAS operates on applications in LLVM bitcode. Ordinary C programs that build
with a compiler that supports LLVM bitcode, such as Clang, are easily compiled
into LLVM bitcode. In essence, LMCAS fixes the command line options and
configuration files to a specific configuration. The debloated program will not
accept any more command-line _options_, but may accept other arguments.

This is an alpha release. This Docker container contains the current version of
LMCAS TaBaCCo, as well as several popular open-source projects with example
specifications that define how to debloat some functionality.





TaBaCCo relies on the presence of a "neck." The broad structure of a typical
command-line program is often similar to:

    int main(int argc, char** argv) {
        parse_args_and_config(argc, argv);
        do_the_work();
        return 0;
    }

While parse_args_and_config and do_the_work might internally have a lot of
complicated control flow and do lots of interesting and hard-to-analyze things,
the point between them is passed through in every execution in the desired
configuration, and represents a "narrowing" of the control-flow graph. We refer
to this point as the neck.

The TaBaCCo user must insert an annotation at this point, so that the TaBaCCo
compiler passes can identify it. An automatic neck finder is in development,
but is not included in this release.

The original LMCAS tool only supported stripping out functionality based on the
command line options. The current LMCAS TaBaCCo also supports program
configuration files, environment variables, and system configuration files. The
debloating specification file (named spec.json in the examples below) specifies
command line options, environment variables and configuration files to use when
debloating the application.





To see TaBaCCo in action, we can look at a small example program, saved in
/root/example/main.c in the container:

    #include <assert.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>

    __attribute__((noinline)) void _lmcas_neck(void) {}

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

      _lmcas_neck(); // This marks the neck

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

This program follows the above structure; there is some configuration logic at
the top, and the actual effect of the program below. In the middle, we have a
call to a function, _lmcas_neck, that serves to mark the neck. The
function has this name for historical reasons; KLEE is not used by TaBaCCo.

The example program accepts either -h or -l as options, and either prints
"Hello, world!" or runs the ls program via system(), respectively. As an 
example, assume an application runs in an environment where users will 
never run the -l option. By removing the ability to execute the option, 
the program will not call system() and close attacks that use environment 
variables to the behavior of calling ls via system(), for example by
manipulating the PATH environment variable.

To do this, we compile it with a special compiler (based on Clang, with wrapper
scripts to save bitcode produced during compilation and to link against musl,
an implementation of the C standard library).

    $ cd /root/example
    $ musl-gclang -o main main.c
    $ ./main
    Error: invalid mode

We expect this error, since running the program without arguments does not set
a mode, so it remains invalid.

    $ ./main -h
    Hello, world!
    $ ./main -l
    main  main.c  spec.json

We can then create a debloating specification (sometimes shortened as spec file
or debloating spec), to tell TaBaCCo what optional functionality we want to be
available in the final binary. An example debloating spec for this program is
provided as /root/example/spec.json:

    {
      "binary": "/root/example/main",
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

This file gives the path to the binary, plus information about what from the
environment should be "baked into" the final binary. Note that the debloating 
arguments here are effectively treated as a prefix of the arguments the final
program will be run with, so if the program were to process additional
non-option arguments after the neck, they would still be processed in the final
program.

With the binary produced by our special compiler, plus the spec file that
references it, we can run TaBaCCo to get a debloated binary:

    $ cd /root/example
    $ tabacco -o debloated spec.json
    [...]
    $ ./debloated
    Hello, world!
    $ ./debloated -l
    Hello, world!

One can also run the TaBaCCo driver without running the debloating passes, to
get a binary that is linked similarly (to avoid a dependence on the container
this release of TaBaCCo is shipped in), but with full functionality.

    $ cd /root/example
    $ tabacco -o nondebloated -N spec.json
    [...]
    $ ./nondebloated
    Error: invalid mode

This error is also expected; ./nondebloated should have the same behavior as
./main above.

    $ ./nondebloated -h
    Hello, world!
    $ ./nondebloated -l
    debloated  main  main.c  nondebloated  spec.json

One can also note that the system() call was entirely removed by the debloating
process:

    $ cd /root/example
    $ nm debloated | grep system
    $ nm nondebloated | grep system
    0000000000405baa t system

TaBaCCo focuses on removing functionality, but it also removes unused code as
part of compiler optimizations performed by LLVM (the same ones flags like -O2
use with Clang), potentially reducing the size of the binary.





This container includes several open-source projects to demonstrate TaBaCCo's
capabilities and provide realistic examples of its use. Next we walk through
the steps to debloat curl as an example, since it accepts a configuration file,
and requires a non-trivial debloating specification to work.

The steps for using TaBaCCo are generally:

1. Build the target program with our compiler wrapper, musl-gclang. (It's best
   to get it building first, to be sure that any compile-time issues aren't
   caused by adding the neck marker.)

2. Patch the program to add the neck marker.

3. Build the program with the neck marker added.

4. Write a debloating specification.

5. Run the tabacco command-line tool, passing it the debloating specification.

For convenience, the source code of curl is already provided in this container
in /usr/src, and we have a script, /root/curl/build.sh, that goes through these
steps. It is commented, and we suggest reading it now.

Running this script will produce a debloated version of curl at
/root/curl/debloated, and a copy that is not debloated but is compiled with the
same optimizations as the debloated binary at /root/curl/nondebloated.

    $ /root/curl/build.sh
    ... [ lots of build info will scroll by ] ...
    $ ls /root/curl
    build.sh  curlrc  debloated  neck.patch  nondebloated  spec.json





The debloating specification file we provide in the container
specializes to only allow the following behavior:

- Always request http://127.0.0.1:8000/example
- Always send POST requests
- Send the JSON object {"title":"Curl POST","body":"Curl POST example body","userId":1}
  as the body of the request
- Send the header Content-Type: application/json (which curl would not do on
  its own, because of the flags we're using to specify the body)
- Set the User-Agent header to be "TaBaCCo Example"

We can see that this configuration was actually fixed into the binary by
testing it. The container also contains a tiny example web server (just enough
to test curl and wget with). It can be started in the background via:

    $ /root/test_http.py &
    [1] 12345
    $ /root/curl/debloated
    Got a POST request!
    Host: 127.0.0.1:8000
    User-Agent: TaBaCCo Example
    Accept: */*
    Content-Type: application/json
    Content-Length: 66
    Body: {"title": "Curl POST", "body": "Curl POST example body", "userId": 1}
    127.0.0.1 - - [02/Sep/2022 19:34:53] "POST /example HTTP/1.1" 200 -
    Got a POST request; hiya!

When curl connects to the web server, the web server prints information about
the request; all the lines but the last ("Got a POST request; hiya!") are from
the web server, and describe what it received.





Similar testing can be performed for wget, also included in this
container.  The main differences between the debloating specification
for it and curl are that:

- wget is specialized to only make GET requests
- wget does not have a URL to make a request to "baked in," and still accepts
  URLs (but not other parameters about the request) on the command-line

Assuming that the web server started in the previous example is still running:

    $ /root/wget/build.sh
    ... [ lots of build info will scroll by ] ...
    $ /root/wget/debloated http://localhost:8000/hello-world
    Got a GET request!
    Host: localhost:8000
    User-Agent: TaBaCCo Example
    Accept: */*
    Accept-Encoding: identity
    Connection: Keep-Alive
    Content-Type: application/json
    127.0.0.1 - - [02/Sep/2022 19:42:06] "GET /hello-world HTTP/1.1" 200 -
    Got a GET request; hello!

If we try to instead send a POST request, we will find that wget now interprets
the command-line argument as an additional URL, despite it matching a valid
flag.

    $ /root/wget/debloated --method=POST http://localhost:8000/not-a-post
    Got a GET request!
    Host: localhost:8000
    User-Agent: TaBaCCo Example
    Accept: */*
    Accept-Encoding: identity
    Connection: Keep-Alive
    Content-Type: application/json
    127.0.0.1 - - [03/Sep/2022 18:07:05] "GET /not-a-post HTTP/1.1" 200 -
    Got a GET request; hello!
    $ echo $?
    4

wget doesn't print an error from not being able to resolve --method=POST as a
domain name, since one of the options used for debloating asks it to keep
logging to a minimum. However, we can check the exit code and find that it's 4,
which corresponds to a network error. Furthermore, we can see that wget is
doing this:

    $ strace -e sendto /root/wget/debloated --method=POST localhost:8000
    sendto(3, "\230~\1\0\0\1\0\0\0\0\0\0\r--method=post\0\0\1\0\1", 31, MSG_NOSIGNAL, {sa_family=AF_INET, sin_port=htons(53), sin_addr=inet_addr("10.0.2.3")}, 16) = 31
    sendto(3, "&\317\1\0\0\1\0\0\0\0\0\0\r--method=post\0\0\34\0\1", 31, MSG_NOSIGNAL, {sa_family=AF_INET, sin_port=htons(53), sin_addr=inet_addr("10.0.2.3")}, 16) = 31
    Got a GET request!
    Host: localhost:8000
    User-Agent: TaBaCCo Example
    Accept: */*
    Accept-Encoding: identity
    Connection: Keep-Alive
    Content-Type: application/json
    127.0.0.1 - - [03/Sep/2022 18:14:17] "GET / HTTP/1.1" 200 -
    Got a GET request; hello!
    +++ exited with 4 +++

The two sendto() calls are DNS requests made to the nameserver configured in
/etc/resolv.conf. No DNS request gets made to resolve localhost, since an entry
for localhost appears in /etc/hosts, which takes priority.





Another target provided in this container is Dropbear, an SSH server program
designed to run on embedded devices. The provided debloating specification (in
/root/dropbear/spec.json) gives, among others, the -w flag, which is used by
Dropbear to disallow logins from the root username. The container also has
the OpenSSH SSH client installed to allow testing the server.

Running the build script provided for Dropbear produces both debloated and
compiled-without-debloating versions of Dropbear, similar to curl and wget.

    $ /root/dropbear/build.sh
    ... [ lots of build info will scroll by ] ...

To test that the binary works as expected, we start non-debloated version of
Dropbear in the background.

Note that when we run Dropbear, we pass in an argument that configures the
program to not daemonize, but we use the & feature of the shell to run it in
the background so that we can run the client to test it. Dropbear will print
"Not backgrounding" because it's not putting itself into the background, but it
is indeed in the background of the shell.

    $ /root/dropbear/nondebloated -F -E -R &
    [1] 10684
    [10684] Sep 02 22:57:08 Not backgrounding

Because the "Not backgrounding" message prints from the background, it may
print over your shell prompt, so the prompt may appear more similar to:

    6f173edae536 ~ # /root/dropbear/nondebloated -F -E -R &
    [2] 57187
    6f173edae536 ~ # [57187] Sep 03 18:17:54 Not backgrounding

with the cursor appearing on the next line. This depends on the scheduling of
the processes, so nothing is wrong if this does not occur. If this has
occurred, you are still in bash and can still enter commands as usual; hitting
the Enter or Return key, or pressing Control-L, will have their normal effects
and cause a new prompt to be printed.

To demonstrate that we can connect, we'll run a simple command that will
produce different output once we've connected to the server.

    $ echo $$
    1

This command prints the PID of the current shell. Because the shell being used
is the init process of the Docker container, it should be 1. If you are running
under tmux, or in a subshell, it may be different, but should be consistent.

If we SSH into the server and run the same command, we will see a different
PID, indicating that the command is being run in a different shell, one that is
a child process of Dropbear.

    $ ssh root@localhost 'echo $$'
    [10705] Sep 02 23:08:43 Child connection from 127.0.0.1:48064
    [10705] Sep 02 23:08:45 Pubkey auth succeeded for 'root' with key sha1!! d4:04:1f:f8:08:bc:03:c4:79:7b:38:bb:f2:e9:ff:f6:bb:39:bf:d9 from 127.0.0.1:48064
    10706
    [10705] Sep 02 23:08:45 Exit (root) from <127.0.0.1:48064>: Disconnect received

After running the command, it will return to the bash shell we started with. We
shutdown the server by running the fg command to move Dropbear to the
foreground, then hitting Ctrl-C to interrupt it.

    $ fg
    /root/dropbear/nondebloated -F -E -R
    ^C[10684] Sep 02 23:17:01 Early exit: Terminated by signal

If we try the same set of commands on the debloated Dropbear, we will notice
that it _rejects_ our attempt to connect, as this version of Dropbear has been
specialized to reject logins as the root user.

    $ /root/dropbear/debloated  &
    [1] 10667
    [10667] Oct 13 00:10:30 Not backgrounding
    $ ssh -o PasswordAuthentication=no root@localhost 'echo $$'
    [10709] Sep 02 23:54:32 Child connection from 127.0.0.1:32978
    [10709] Sep 02 23:54:32 root login rejected
    root@localhost: Permission denied (publickey,password).
    [10709] Sep 02 23:54:32 Exit before auth from <127.0.0.1:32978>: (user 'root', 0 fails): Exited normally

We use the -o PasswordAuthentication=no flags, since Dropbear defaults to
prompting for a password when root login is allowed; despite this, no password
is ever accepted.





Now that we've seen a couple examples of usage, we can do a deeper dive into
the contents of a debloating specification file. The debloating specification
for curl is stored in /root/curl/spec.json -- we'll look at it in short chunks.

    {

The first option in the debloating specification, binary, gives the path to the
binary that should be used as input. This option is always a string. This
binary must have been compiled with musl-gclang, so that it contains the
bitcode necessary for TaBaCCo to process it.

      "binary": "/root/curl-7.83.1/src/curl",

TaBaCCo uses the the array of strings given by the args property as the list of
arguments to debloat with. Depending on the semantics of the program, this may
end up being the only arguments the program can use, or the prefix of the
arguments the program can use.

      "args": [

In typical Unix style, the program itself is the first argument.

        "curl",

The remainder of the arguments are present as usual. Keep bash's and JSON's
quoting and escaping rules in mind; TaBaCCo directly passes these arguments as
execve() would pass them rather than further processing them as bash would do.

        "-X",
        "POST",
        "-d",
        "{\"title\": \"Curl POST\",\"body\": \"Curl POST example body\",\"userId\":1}",
        "http://127.0.0.1:8000/example"
      ],

The environment variables that TaBaCCo debloats the program with are specified
next. Many programs rely on CWD and PATH to be set, so it's a safe choice to
always specify them. HOME and USER are also commonly dependend-on variables.

Curl looks for a configuration file at $CURL_HOME/.curlrc, so we override
CURL_HOME to a directory that doesn't actually exist, but we'll recognize.

      "env": {
        "CWD": "/",
        "CURL_HOME": "/fake-curl-home",
        "PATH": ""
      },

The binary's working directory should also be specified; it should match the
CWD environment variable, if it is provided.

      "cwd": "/",

Optionally, the debloating specification file can contain a syscall_mocks
property, which describes how TaBaCCo should change the behavior of syscalls
that occur before the neck.

      "syscall_mocks": {

Since we want to specialize the configuration file that would be found in
/fake-curl-home/.curlrc, we remap the call to open(). This causes any uses of
the open() syscall with a path of /fake-curl-home/.curlrc to instead open the
file curlrc, relative to the debloating specification file (not the cwd
specified above!).

        "sys_open": {
          "mapping": {
            "/fake-curl-home/.curlrc": "curlrc"
          }
        }
      }
    }
