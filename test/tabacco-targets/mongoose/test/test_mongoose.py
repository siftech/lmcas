import pytest
import requests
from make_tests import make_tests
import threading
import subprocess
from utils import retry
import toml

make_tests("mongoose", toml.loads("[switched-server]"))


class RunServerThread(threading.Thread):
    def __init__(self, binary, is_bloated):
        # Identical to thread initialization except we pass in the binary being run
        threading.Thread.__init__(self)
        self.binary = binary
        self.is_bloated = is_bloated

    def run(self):
        """
        Runs the binary with a 30-second timeout, storing stdout into the
        `outs` attribute, and stderr into the `errs` attribute. Stores whether
        the execution timed out in the `timed_out` attribute.
        """
        args = [self.binary, "--no-websocket"]

        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.timed_out = False
        try:
            self.outs, self.errs = proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()
            self.timed_out = True
            self.outs, self.errs = proc.communicate()


@retry()
def run_client() -> tuple[str, str]:
    """
    Tries to connect to the server, returning both the result of a
    non-WebSocket and WebSocket call.

    This is factored out to its own function to allow using the retry
    decorator; since it's a pain to figure out exactly when the server is up,
    we retry a couple times until it works.
    """

    return (
        requests.get("http://localhost:12080/rest").text,
        requests.get("http://localhost:12080/websocket").text,
    )


@pytest.mark.tabacco()
@with_use_binary("using_binary", "switched-server")
def test_mongoose_simple(using_binary, is_bloated):
    binary = using_binary(is_bloated)

    server_thread = RunServerThread(binary, is_bloated)
    server_thread.start()

    rest_response, websocket_response = run_client()
    assert rest_response == '{"result": 123}\n'
    assert websocket_response == "Websocket request rejected!\n"

    server_thread.join()

    # The server should time out, since it runs until stopped.
    assert server_thread.timed_out

    assert (
        server_thread.outs
        == b"Starting WS listener on ws://localhost:12080/websocket\nServing REST response.\nUpgrading to websocket.\nIgnoring request to upgrade to websocket.\n"
    )
    assert server_thread.errs == b""
