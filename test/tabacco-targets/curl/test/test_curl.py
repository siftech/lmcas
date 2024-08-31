from http.server import BaseHTTPRequestHandler, HTTPServer
from os import getenv
from pathlib import Path
import pytest
import requests
from make_tests import make_tests
from threading import Thread
from utils import retry


make_tests("curl")


class RunServerThread(Thread):
    def __init__(self):
        # Identical to thread initialization except we pass in the binary being run
        Thread.__init__(self)
        thread = self
        self.request = None

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                thread.request = (
                    self.command,
                    self.path,
                    dict(self.headers.items()),
                    self.rfile.read(int(self.headers.get("Content-Length"))),
                )

                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(b"Hello from the curl test\n")

        self.server = HTTPServer(("", 12346), Handler)

    def run(self):
        self.server.handle_request()
        self.server.handle_request()
        self.server.server_close()


@retry()
def ensure_server_up():
    requests.post("http://127.0.0.1:12346/ensure_server_up")


@pytest.fixture
def curl_server(is_bloated):
    if is_bloated:
        pytest.skip("No bloated test for curl")

    expected_data = (
        b'{"title": "Curl POST","body": "Curl POST example body","userId":1}'
    )

    server_thread = RunServerThread()
    server_thread.start()

    ensure_server_up()

    yield

    server_thread.join(timeout=1.0)
    assert not server_thread.is_alive()
    assert server_thread.request is not None
    assert server_thread.request[0] == "POST"
    assert server_thread.request[1] == "/foo/bar"
    assert server_thread.request[2] == {
        "Accept": "*/*",
        "Content-Length": str(len(expected_data)),
        "Content-Type": "application/json",
        "Host": "127.0.0.1:12346",
        "User-Agent": "config-test",
    }
    assert server_thread.request[3] == expected_data
