from os import chdir, getcwd
from pathlib import Path
import pytest
from make_tests import make_tests
from utils import retry

import tempfile
from subprocess import DEVNULL, Popen, TimeoutExpired
import time

import signal
import socket

# All paths determined from mount points in Docker container
out_dir = Path("/test/tabacco-cps/test-results")
spec_dir = Path("/test/tabacco-targets/tabacco-cps/specs")

make_tests("tabacco-cps")


@pytest.fixture
def ls_dir():
    tmpdir = out_dir / "test_tabacco_cps/ls_dir"

    if not tmpdir.exists():
        tmpdir.mkdir(parents=True, exist_ok=True)

    (tmpdir / "foo").touch()
    (tmpdir / "bar").touch()

    old_cwd = getcwd()
    try:
        chdir(tmpdir)
        yield
    finally:
        chdir(old_cwd)


@retry()
def run_client():
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect(("127.0.0.1", 12345))
    return client.makefile().read()


@pytest.mark.tabacco()
@with_use_binary("using_binary", "cp8")
def test_cp8(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    args = [binary]
    if is_bloated:
        args.append(spec_dir / "cp8/cp8.conf")
    proc = Popen(args)

    msg = run_client()

    try:
        proc.communicate(timeout=5)
        assert False and "server should not have exited"
    except TimeoutExpired:
        proc.kill()
        proc.communicate()

    assert (
        msg == "# Good morning!\n\nThis is a good morning message to anyone reading it!"
    )


@pytest.mark.tabacco()
@pytest.mark.debug
@with_use_binary("using_binary", "cp_sighup_before_neck")
def test_cp_sighup_before_neck(is_bloated, using_binary):
    bloated_expectation = "Load config\nReload config\nLoad config\nInterrupt\n"
    debloated_expectation = "Load config\nInterrupt\n"

    binary = using_binary(is_bloated)
    outs = None
    errs = None
    returncode = None
    encoding = "utf-8"

    f_outs = tempfile.NamedTemporaryFile()
    f_errs = tempfile.NamedTemporaryFile()
    try:

        def collect_output():
            f_outs.seek(0)
            f_errs.seek(0)
            return f_outs.read().decode(encoding), f_errs.read().decode(encoding)

        with Popen(
            [binary],
            stdin=DEVNULL,
            stdout=f_outs,
            stderr=f_errs,
            encoding=encoding,
        ) as proc:
            try:
                time.sleep(1)
                proc.send_signal(signal.SIGHUP)

                time.sleep(1)
                proc.send_signal(signal.SIGINT)

                proc.communicate(timeout=5)
                outs, errs = collect_output()
            except TimeoutExpired:
                proc.kill()
                proc.communicate()
                outs, errs = collect_output()
                assert False, f"Process timed out:\nstdout:\n{outs}\nstderr:\n{errs}"
            finally:
                if proc.returncode is None:
                    proc.kill()

            returncode = proc.returncode
    finally:
        f_outs.close()
        f_errs.close()

    if is_bloated:
        print("returncode = {}, errs = {}, outs = {}".format(returncode, errs, outs))
        assert returncode == 0
        assert errs == ""
        assert outs == bloated_expectation
    else:
        print("returncode = {}, errs = {}, outs = {}".format(returncode, errs, outs))
        assert returncode == 0
        assert errs == ""
        assert outs == debloated_expectation


# test is broken
# See this in output
# [2023-01-27 01:00:05.333] [error] Found a basic block without an ID in main (was the annotation pass run correctly?): ._crit_edge
# [2023-01-27 01:00:05.333] [error] Found a basic block without an ID in read_file (was the annotation pass run correctly?): ._crit_
@pytest.mark.tabacco()
@with_use_binary("using_binary", "cp_sighup_after_neck")
def test_cp_sighup_after_neck(is_bloated, using_binary):
    bloated_expectation = "Load config\nReload config\nLoad config\nInterrupt\n"
    debloated_expectation = ""
    # TODO Update to the below once SIGHUP is substituted instead of removed
    # debloated_expectation = "Load config\nInterrupt\n"
    binary = using_binary(is_bloated)
    outs = None
    errs = None
    returncode = None
    encoding = "utf-8"
    f_outs = tempfile.NamedTemporaryFile()
    f_errs = tempfile.NamedTemporaryFile()
    try:

        def collect_output():
            f_outs.seek(0)
            f_errs.seek(0)
            return f_outs.read().decode(encoding), f_errs.read().decode(encoding)

        with Popen(
            [binary],
            stdin=DEVNULL,
            stdout=f_outs,
            stderr=f_errs,
            encoding=encoding,
        ) as proc:
            try:
                time.sleep(1)
                proc.send_signal(signal.SIGHUP)

                time.sleep(1)
                proc.send_signal(signal.SIGINT)

                proc.communicate(timeout=5)
                outs, errs = collect_output()
            except TimeoutExpired:
                proc.kill()
                proc.communicate()
                outs, errs = collect_output()
                assert False, f"Process timed out:\nstdout:\n{outs}\nstderr:\n{errs}"
            finally:
                if proc.returncode is None:
                    proc.kill()

            returncode = proc.returncode
    finally:
        f_outs.close()
        f_errs.close()

    if is_bloated:
        assert returncode == 0
        assert errs == ""
        assert outs == bloated_expectation
    else:
        assert returncode == -1
        assert errs == ""
        assert outs == debloated_expectation
