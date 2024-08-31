from os import chdir, getcwd
from pathlib import Path
import pytest
from shutil import copy
import subprocess
from tempfile import TemporaryDirectory

from make_tests import make_tests
from utils import FileLock

# All paths determined from mount points in Docker container
out_dir = Path("/test/uwisc/test-results")
make_tests("uwisc")


@with_use_binary("using_binary", "kill")
def test_kill_simple(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    target = subprocess.Popen(["sleep", "infinity"])
    try:
        args = [binary, "-9", str(target.pid)]

        proc = subprocess.run(
            args=args,
            stdin=subprocess.DEVNULL,
            capture_output=True,
            encoding="utf-8",
        )

        assert proc.returncode == 0
        assert proc.stdout == ""
        assert proc.stderr == ""

        # "A negative value -N indicates that the child was terminated by
        # signal N (POSIX only)."
        assert target.wait() == -9
    finally:
        if target.returncode is None:
            target.kill()


@pytest.fixture
def du_5MB_dir():
    tmpdir = out_dir / "test_uwisc" / "du" / "5MB_dir"
    if not tmpdir.exists():
        tmpdir.mkdir(parents=True)

    with FileLock(tmpdir):
        MiB = 1024 * 1024
        with (tmpdir / "file1").open("w") as f:
            f.write("1" * (2 * MiB))
        with (tmpdir / "file2").open("w") as f:
            f.write("2" * (2 * MiB))
        with (tmpdir / "file3").open("w") as f:
            f.write("3" * (1 * MiB))

        old_cwd = getcwd()
        try:
            chdir(tmpdir)
            yield
        finally:
            chdir(old_cwd)


@pytest.fixture
def misc_text_file_dir():
    tmpdir = out_dir / "test_uwisc" / "dracula"
    # This depends on tests running serially!
    # - Seems like the pytest parallelization thing solves this by having
    #   separate processes which can all have separate cwd's.
    if not tmpdir.exists():
        tmpdir.mkdir(parents=True)
        for file in Path("/test/tabacco-targets/misc_text_files").iterdir():
            copy(file, tmpdir / file.name)
        for name in ["dracula", "dracula-para"]:
            with (tmpdir / f"{name}.txt").open() as f:
                with (tmpdir / f"{name}-sorted.txt").open("w") as out:
                    for line in sorted(f):
                        out.write(line)

    old_cwd = getcwd()
    try:
        chdir(tmpdir)
        yield
    finally:
        chdir(old_cwd)


@with_use_binary("using_binary", "rm")
def test_rm_dir_fail(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    tmpdir = out_dir / "test_uwisc" / "rmdir"
    newdir = out_dir / "test_uwisc" / "rmdir" / "testdir"
    if not tmpdir.exists():
        tmpdir.mkdir(parents=True)
    if not newdir.exists():
        newdir.mkdir(parents=True)
        (newdir / "foo").touch()

    proc = subprocess.run(args=[binary, str(newdir)], capture_output=True)

    if is_bloated:
        assert proc.returncode == 1
    else:
        assert proc.returncode == 100
    assert newdir.exists()


@with_use_binary("using_binary", "rm")
def test_rm_dir(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    tmpdir = out_dir / "test_uwisc" / "rmdir"
    newdir = out_dir / "test_uwisc" / "rmdir" / "testdir"
    if not tmpdir.exists():
        tmpdir.mkdir(parents=True)
    if not newdir.exists():
        newdir.mkdir(parents=True)
        (newdir / "foo").touch()

    proc = subprocess.run(args=[binary, "-r", str(newdir)], capture_output=True)

    assert proc.returncode == 0
    assert not newdir.exists()


@with_use_binary("using_binary", "chown")
def test_chown(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    if is_bloated:
        dir_name = "/test/uwisc/test-results/test_uwisc/bloated-chown"
    else:
        dir_name = "/test/uwisc/test-results/test_uwisc/debloated-chown"

    tmpdir = Path(dir_name)
    if not tmpdir.exists():
        tmpdir.mkdir(parents=True, exist_ok=True)

    d_owner = tmpdir.owner()
    d_group = tmpdir.group()

    (tmpdir / "foo").touch()

    f_owner = (tmpdir / "foo").owner()
    f_group = (tmpdir / "foo").group()

    assert d_group == "root"
    assert d_owner == "root"
    assert f_owner == "root"
    assert f_group == "root"

    args = [binary, "-R", "nobody:nobody", str(tmpdir)]

    proc = subprocess.run(args=args, capture_output=True)
    assert proc.returncode == 0
    d_owner = (tmpdir).owner()
    d_group = (tmpdir).group()
    f_owner = (tmpdir / "foo").owner()
    f_group = (tmpdir / "foo").owner()

    assert d_group == "nobody"
    assert d_owner == "nobody"
    assert f_owner == "nobody"
    assert f_group == "nobody"

    # Restore the permissions so we can clean up the test easier.
    subprocess.check_call(args=[binary, "-R", "root:root", str(tmpdir)])


@with_use_binary("using_binary", "chown")
def test_chown_fail(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    if is_bloated:
        dir_name = "/test/uwisc/test-results/test_uwisc/bloated-chown"
    else:
        dir_name = "/test/uwisc/test-results/test_uwisc/debloated-chown"

    tmpdir = Path(dir_name)
    if not tmpdir.exists():
        tmpdir.mkdir(parents=True, exist_ok=True)

    d_owner = tmpdir.owner()
    d_group = tmpdir.group()

    (tmpdir / "foo").touch()

    f_owner = (tmpdir / "foo").owner()
    f_group = (tmpdir / "foo").group()

    assert d_group == "root"
    assert d_owner == "root"
    assert f_owner == "root"
    assert f_group == "root"

    args = [binary, "nobody:nobody", str(tmpdir)]

    proc = subprocess.run(args=args, capture_output=True)
    if is_bloated:
        assert proc.returncode == 0
    else:
        assert proc.returncode == 100
    d_owner = (tmpdir).owner()
    d_group = (tmpdir).group()
    f_owner = (tmpdir / "foo").owner()
    f_group = (tmpdir / "foo").owner()

    if is_bloated:
        assert d_group == "nobody"
        assert d_owner == "nobody"
    else:
        assert d_group == "root"
        assert d_owner == "root"
    assert f_owner == "root"
    assert f_group == "root"

    # Restore the permissions so we can clean up the test easier.
    subprocess.check_call(args=[binary, "-R", "root:root", str(tmpdir)])


@with_use_binary("using_binary", "realpath-P")
def test_realpath_P_objcopy_and_nm(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    with TemporaryDirectory() as tmpdir_s:
        tmpdir = Path(tmpdir_s)

        args = [binary, "-P", "objcopy", "nm"]
        proc = subprocess.run(args=args, cwd=tmpdir, capture_output=True)

        assert proc.returncode == 0

        expected_objcopy = str(tmpdir / "objcopy")
        expected_nm = str(tmpdir / "nm")
        assert proc.stdout == f"{expected_objcopy}\n{expected_nm}\n".encode("utf-8")


@with_use_binary("using_binary", "realpath-P")
def test_realpath_P_wc_and_nm(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    with TemporaryDirectory() as tmpdir_s:
        tmpdir = Path(tmpdir_s)

        args = [binary, "-P", "wc", "nm"]
        proc = subprocess.run(args=args, cwd=tmpdir, capture_output=True)

        assert proc.returncode == 0

        expected_wc = str(tmpdir / "wc")
        expected_nm = str(tmpdir / "nm")
        assert proc.stdout == f"{expected_wc}\n{expected_nm}\n".encode("utf-8")


@with_use_binary("using_binary", "realpath-z")
def test_realpath_z_wc_and_nm(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    with TemporaryDirectory() as tmpdir_s:
        tmpdir = Path(tmpdir_s)

        args = [binary, "-z", "wc", "nm"]
        proc = subprocess.run(args=args, cwd=tmpdir, capture_output=True)

        assert proc.returncode == 0

        expected_wc = str(tmpdir / "wc").encode("utf-8")
        expected_nm = str(tmpdir / "nm").encode("utf-8")
        assert proc.stdout == expected_wc + b"\x00" + expected_nm + b"\x00"
