from pathlib import Path
from pyroute2 import DiagSocket
from pyroute2.netlink.diag import SS_LISTEN
from random import random
from os import chdir, getcwd, getpid
from socket import AF_INET, AF_INET6
from sys import stderr
from time import perf_counter, sleep
from contextlib import contextmanager


@contextmanager
def FileLock(filename):
    path = Path(filename)

    lock_path = path
    if path.is_dir():
        lock_path = path / ".lmcas_lock"

    pid = getpid()
    print(f"Locking {lock_path} as {pid}", file=stderr)
    before = perf_counter()
    while True:
        try:
            with open(lock_path, "x") as f:
                print(pid, file=f)
            after = perf_counter()
            break
        except FileExistsError:
            # Add some jitter to avoid a thundering herd problem.
            sleep(1 + random() * 0.1)
    print(f"took {after - before}s to acquire {lock_path}", file=stderr)

    ok = False
    try:
        yield
        ok = True

    finally:
        print(f"Unlocking {lock_path} as {pid} (ok={ok})", file=stderr)
        lock_path.unlink()


@contextmanager
def chdiring(path: Path):
    old_cwd = getcwd()
    chdir(path)
    try:
        yield
    finally:
        chdir(old_cwd)


def retry(*, retries=3, initial_period=1, next_period=lambda x: x * 2 + random() * 0.1):
    """
    A decorator that makes a function retry execution on exception. If an
    exception occurs, the function will be called up to `retries` additional
    times, waiting for `period` between retries. `period` is updated between
    retries by `next_period`, so a non-constant `period` can be defined.
    """

    def decorator(func):
        def inner(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                i = 0
                final_exception = e
                period = initial_period
                print(f"retry {i}: {e}")
                while i < retries:
                    i += 1
                    sleep(period)
                    period = next_period(period)

                    try:
                        return func(*args, **kwargs)
                    except Exception as e:
                        final_exception = e

                raise final_exception

        return inner

    return decorator


@retry()
def wait_for_tcp_socket(addr: str, port: int):
    """
    Waits for the TCP socket with the given address and port to be listened to.
    """

    assert check_tcp_socket(addr, port)


def check_tcp_socket(addr: str, port: int):
    """
    Checks if the TCP socket with the given address and port can be listened to.
    """

    with DiagSocket() as ds:
        ds.bind()
        states_bitmask = 1 << SS_LISTEN
        ipv4_stats = ds.get_sock_stats(family=AF_INET, states=states_bitmask)
        ipv6_stats = ds.get_sock_stats(family=AF_INET6, states=states_bitmask)

        stats = ipv4_stats + ipv6_stats

    for stat in stats:
        if stat["idiag_src"] == addr and stat["idiag_sport"] == port:
            return True
    return False


def choose_nonconflicting_name(gs: dict[str, object], base_name: str) -> str:
    """
    Returns a name derived from base_name that is not present as a key of gs.
    """

    if base_name not in gs:
        return base_name

    i = 0
    while f"{base_name}_{i}" in gs:
        i += 1
    return f"{base_name}_{i}"
