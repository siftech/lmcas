# Tutorial: Debloating Dropbear

## Prerequisites

You need TaBaCCo to be installed.
If you haven't finished the [Installing TaBaCCo from a registry](./installing-from-registry.md) or [Installing TaBaCCo from a tarball](./installing-from-tarball.md) tutorials, do so now.

## Instructions

In this tutorial, we'll debloat Dropbear, an SSH server program designed to run on embedded devices.

All the commands in this tutorial are meant to be run in the LMCAS Docker container.
The container already contains all the files we'll need.

### Building zlib

Dropbear has a library dependency we'll need to build before we get on with building Dropbear itself: zlib.
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

### Building Dropbear

Dropbear's sources are in `/usr/src`.
To get started, we can extract them into our home folder inside the container.

```shell
lmcas ~/zlib-1.2.12 # cd ~
lmcas ~ # tar xf /usr/src/dropbear-2020.81.tar.bz2 -C /root
lmcas ~ # cd dropbear-2020.81
```

We configure it, and by default zlib is enabled.

```shell
lmcas ~/dropbear-2020.81 # ./configure --enable-static
...
```

Now, we can build Dropbear.

```shell
lmcas ~/dropbear-2020.81 # make -j $(nproc) -l $(nproc)
...
```

### Debloating and testing the resulting binary

To debloat, we need to write a debloating specification to pass to TaBaCCo.
This is a file that instructs TaBaCCo how to debloat the program.
The debloating specification we'll be using is in `/root/dropbear/spec.json`.

```shell
lmcas ~/dropbear-2020.81 # cd ~/dropbear
lmcas ~/dropbear # less spec.json
...
```

The file is reproduced here in a separate code block for syntax highlighting.
```json
{
  "binary": "/root/dropbear-2020.81/dropbear",
  "configs": [
    {
      "args": [
        "dropbear",
        "-w",
        "-j",
        "-E",
        "-F",
        "-R",
        "-I",
        "60"
      ],
      "env": {
        "CWD": "/",
        "PATH": "/bin"
      },
      "cwd": "/"
    },
    {
      "args": [
        "dropbear",
        "-j",
        "-E",
        "-F",
        "-R",
        "-I",
        "60",
        "-c",
        "uptime"
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

The first debloating configuration gives, among others, the -w flag, which is used by Dropbear to disallow logins from the root username.
The second one gives `-c uptime`, which forces clients to get the result of the uptime command rather than arbitrary shell access.

The container also has the OpenSSH SSH client installed to allow testing the server.

To test that the binary works as expected, we must copy the Dropbear ssh host key from `/root/.ssh/dropbear_ed25519_host_key` into `/etc/dropbear`.

```shell
lmcas ~/dropbear # mkdir -p /etc/dropbear
lmcas ~/dropbear # cp /root/.ssh/dropbear_ed25519_host_key /etc/dropbear
```

#### Testing the non-debloated Dropbear

Now, we can first run `tabacco` to create a non-debloated binary to understand the behavior of the original binary.
Whether or not we are debloating during a `tabacco` run, we must specify both the name of the output file with `-o` and the `spec.json` file TaBaCCo should use. Additionally, on this run, `-N` indicates to TaBaCCo that we should not specialize this binary.

```shell
lmcas ~/dropbear # tabacco -o nondebloated -N spec.json
...
Step 5: Link the final program
ld              [0:00:00.043482]
strip           [0:00:00.006348]
```

Now, we can run the non-debloated binary which should now exist in `~/dropbear/nondebloated`.
Note that when we run Dropbear, we pass in an argument that configures the program to not daemonize, but we use the & feature of the shell to run it in the background so that we can run the client to test it.
Dropbear will print "Not backgrounding" because it's not putting itself into the background, but it is indeed in the background of the shell.

```shell
lmcas ~/dropbear # /root/dropbear/nondebloated -F -E -R &
[1] 24420
[24420] Jan 26 20:59:38 Not backgrounding
lmcas ~/dropbear #
```

Because the "Not backgrounding" message prints from the background, it may print over your shell prompt, so the prompt may appear more similar to:

```shell
lmcas ~/dropbear # /root/dropbear/nondebloated -F -E -R &
[2] 57187
lmcas ~/dropbear # [57187] Jan 26 20:59:38 Not backgrounding
```

with the cursor appearing on the next line.
This depends on the scheduling of the processes, so nothing is wrong if this does not occur. If this has occurred, you are still in bash and can still enter commands as usual; hitting the Enter or Return key, or pressing Control-L, will have their normal effects and cause a new prompt to be printed.

To demonstrate that we can connect, we'll run a simple command that will produce different output once we've connected to the server.

```shell
lmcas ~/dropbear # echo $$
1
```

This command prints the PID of the current shell. Because the shell being used is the init process of the Docker container, it should be 1. If you are running under tmux, or in a subshell, it may be different, but should be consistent.

If we SSH into the server and run the same command, we will see a different PID, indicating that the command is being run in a different shell, one that is a child process of Dropbear.

```shell
lmcas ~/dropbear # ssh root@localhost 'echo $$'
[24428] Jan 26 21:13:38 Child connection from 127.0.0.1:59432
[24428] Jan 26 21:13:38 Pubkey auth succeeded for 'root' with key sha1!! d4:04:1f:f8:08:bc:03:c4:79:7b:38:bb:f2:e9:ff:f6:bb:39:bf:d9 from 127.0.0.1:59432 24429
[24428] Jan 26 21:13:38 Exit (root) from <127.0.0.1:59432>: Disconnect received
```

After running the command, it will return to the bash shell we started with.
We shutdown the server by running the `fg` command to move Dropbear to the foreground, then hitting `Ctrl-C` to interrupt it.

```shell
lmcas ~/dropbear # fg
/root/dropbear/nondebloated -F -E -R
^C[24426] Jan 26 21:15:11 Early exit: Terminated by signal
```

#### Testing the debloated Dropbear

To compare this behavior to that of a debloated Dropbear, we now run `tabacco` to create a debloated binary, again passing it the debloating specification, and this time producing a debloated binary that specializes to this behavior.


```shell
lmcas ~/dropbear # tabacco -o debloated spec.json
...
Step 19: Link the final program
ld              [0:00:00.045963]
strip           [0:00:00.005343]
     VM SIZE
 --------------
  +1.8% +1.25Ki    LOAD #2 [R]
  -0.7%    -400    LOAD #3 [RW]
  -7.2% -27.8Ki    LOAD #1 [RX]
  -5.3% -26.9Ki    TOTAL
bloaty          [0:00:00.002524]
```

This creates the Dropbear binary as `debloated`.

Now, if we try the same set of commands on the debloated Dropbear, we will notice that it _rejects_ our attempt to connect, as this version of Dropbear has been specialized to reject logins as the root user.

```shell
lmcas ~/dropbear # /root/dropbear/debloated -w -j -E -F -R -I 60 &
[1] 24556
lmcas ~/dropbear # [24556] Jan 26 21:24:31 Not backgrounding
lmcas ~/dropbear # ssh -o PasswordAuthentication=no root@localhost 'echo $$'
[24558] Jan 26 21:25:37 Child connection from 127.0.0.1:48822
[24558] Jan 26 21:25:38 root login rejected
root@localhost: Permission denied (publickey,password).
[24558] Jan 26 21:25:38 Exit before auth from <127.0.0.1:48822>: (user 'root', 0 fails): Exited normally
```

We use the `-o PasswordAuthentication=no` flags, since Dropbear defaults to prompting for a password when root login is allowed; despite this, no password is ever accepted.

We can try the other configuration as well:

```shell
lmcas ~/dropbear # /root/dropbear/debloated -j -E -F -R -I 60 -c uptime &
[1] 210
lmcas ~/dropbear # [210] Apr 10 20:25:30 Forced command set to 'uptime'
[210] Apr 10 20:25:30 Not backgrounding
lmcas ~/dropbear # ssh -o PasswordAuthentication=no root@localhost
[212] Apr 10 20:25:51 Child connection from 127.0.0.1:60868
[212] Apr 10 20:25:51 Pubkey auth succeeded for 'root' with key sha1!! d4:04:1f:f8:08:bc:03:c4:79:7b:38:bb:f2:e9:ff:f6:bb:39:bf:d9 from 127.0.0.1:60868
[213] Apr 10 20:25:51 lastlog_perform_login: Couldn't stat /var/log/lastlog: No such file or directory
[213] Apr 10 20:25:51 lastlog_openseek: /var/log/lastlog is not a file or directory!
[213] Apr 10 20:25:51 wtmp_write: problem writing /dev/null/wtmp: Not a directory
 20:25:51  up   6:55,  0 users,  load average: 0.48, 0.78, 0.79
Connection to localhost closed.
[212] Apr 10 20:25:51 wtmp_write: problem writing /dev/null/wtmp: Not a directory
[212] Apr 10 20:25:51 Exit (root) from <127.0.0.1:60868>: Disconnect received
lmcas ~/dropbear # ssh -o PasswordAuthentication=no root@localhost 'pwd'
[215] Apr 10 20:25:55 Child connection from 127.0.0.1:60874
[215] Apr 10 20:25:56 Pubkey auth succeeded for 'root' with key sha1!! d4:04:1f:f8:08:bc:03:c4:79:7b:38:bb:f2:e9:ff:f6:bb:39:bf:d9 from 127.0.0.1:60874
 20:25:56  up   6:55,  0 users,  load average: 0.44, 0.76, 0.78
[215] Apr 10 20:25:56 Exit (root) from <127.0.0.1:60874>: Disconnect received
```

As we can see, the command is now locked in.
