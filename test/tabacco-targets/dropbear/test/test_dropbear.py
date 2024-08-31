#!/usr/bin/env python3

import os
from os import chmod, mkdir, listdir
from time import sleep
import logging
from pathlib import Path
import pytest
import requests
import re
from make_tests import make_tests
import threading
from paramiko import SSHClient, SSHException, Ed25519Key, AutoAddPolicy
import subprocess
from utils import retry
from shutil import copy, copytree, rmtree
import stat
import toml

logging.basicConfig()
logging.getLogger("paramiko").setLevel(logging.DEBUG)  # for example

make_tests("dropbear", toml.loads("[dropbear]"))


class RunServerThread(threading.Thread):
    def __init__(self, binary, is_bloated, port):
        # Identical to thread initialization except we pass in the binary being run
        threading.Thread.__init__(self)
        self.binary = binary
        self.is_bloated = is_bloated
        self.port = port

    def run(self):
        """
        Runs the binary with a 30-second timeout, storing stdout into the
        `outs` attribute, and stderr into the `errs` attribute. Stores whether
        the execution timed out in the `timed_out` attribute.
        """

        args = [self.binary]
        if not self.is_bloated:
            args.append("-w")
        args += [
            # Note that we are missing -w here, the option that the specialized
            # debloated version has
            "-j",
            "-E",
            "-F",
            "-R",
            "-I",
            "60",
            "-p",
            str(self.port),
        ]

        self.proc = subprocess.Popen(
            args, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        self.timed_out = False
        try:
            self.outs, self.errs = self.proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.outs, self.errs = self.proc.communicate()


@retry()
def ssh_can_connect(dropbear_test_path, port, server_thread):
    dropbear_test_path = Path(dropbear_test_path)
    client = SSHClient()
    client.load_system_host_keys()
    client.set_missing_host_key_policy(AutoAddPolicy)
    key = Ed25519Key.from_private_key_file(
        str(dropbear_test_path / "ssh" / "id_ed25519")
    )

    assert (dropbear_test_path / "ssh" / "id_ed25519").exists()

    try:
        client.connect(
            "::1",
            username="root",
            pkey=key,
            port=port,
            timeout=60,
            auth_timeout=60,
        )
        client.close()
        server_thread.proc.kill()
        return True
    except SSHException as e:
        print("SSHException: {}".format(str(e)))

        client.close()
        return False


@pytest.mark.tabacco()
@with_use_binary("using_binary", "dropbear")
def test_dropbear_simple(using_binary, is_bloated):
    binary = using_binary(is_bloated)
    if is_bloated:
        port = 12349
    else:
        port = 12348

    # Copy files from test to /etc and /root in running docker container
    dropbear_path = Path("/etc/dropbear")
    if not dropbear_path.exists():
        mkdir(dropbear_path)

    copytree("./conf/ssh", "/root/.ssh")
    root_path = "/root/.ssh/"

    # Ensure we have appropriate permissions for ssh files
    chmod(root_path, stat.S_IRWXU)
    for file in listdir(root_path):
        f = os.path.join(root_path, file)
        chmod(f, stat.S_IEXEC)

    copy(
        "./conf/etc/dropbear_ed25519_host_key",
        "/etc/dropbear/dropbear_ed25519_host_key",
    )

    server_thread = RunServerThread(binary, is_bloated, port)
    server_thread.start()

    connection_succeeds = ssh_can_connect(Path("./conf"), port, server_thread)

    server_thread.join()
    print("stdout: {}".format(server_thread.outs))
    print("stderr: {}".format(server_thread.errs))
    outs = server_thread.outs.decode("utf-8")
    errs = server_thread.errs.decode("utf-8")

    if is_bloated:
        assert connection_succeeds
    else:
        assert not connection_succeeds
    # The server should time out, since it runs until stopped.
    # assert server_thread.timed_out

    assert server_thread.outs == b""
    assert re.search(r"Not backgrounding", errs)
    assert re.search(r"Child connection from ::1", errs)
    if is_bloated:
        assert re.search(
            r"Pubkey auth succeeded for 'root' with key sha1!! d4:04:1f:f8:08:bc:03:c4:79:7b:38:bb:f2:e9:ff:f6:bb:39:bf:d9 from ::1",
            errs,
        )
        assert re.search(r"Exit \(root\) from <::1", errs)
    else:
        assert re.search(r"root login rejected", errs)
        assert re.search(r"Exit before auth from <::1", errs)
    assert re.search(r"Exited normally", errs)
