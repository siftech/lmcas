from http.server import BaseHTTPRequestHandler, HTTPServer
import pytest
import requests
from make_tests import make_tests
from threading import Thread
from utils import retry

make_tests("wget")


class RunServerThread(Thread):
    def __init__(self):
        # Identical to thread initialization except we pass in the binary being run
        Thread.__init__(self)
        thread = self
        self.request = None

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                thread.request = (
                    self.command,
                    self.path,
                    dict(self.headers.items()),
                )

                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.end_headers()
                self.wfile.write(b"Hello from the wget GET test\n")

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
                self.wfile.write(b"Hello from the wget POST test\n")

        self.server = HTTPServer(("", 12347), Handler)

    def run(self):
        self.server.handle_request()
        self.server.handle_request()
        self.server.server_close()


@retry()
def ensure_server_up():
    requests.post("http://127.0.0.1:12347/ensure_server_up")


@pytest.fixture
def wget_server_get(is_bloated):
    if is_bloated:
        pytest.skip("No bloated test for wget")

    server_thread = RunServerThread()
    server_thread.start()

    ensure_server_up()

    yield

    server_thread.join(timeout=1.0)
    assert not server_thread.is_alive()
    assert server_thread.request is not None
    assert server_thread.request[0] == "GET"
    assert server_thread.request[1] == "/foo/bar/get"
    assert server_thread.request[2] == {
        "Accept": "*/*",
        "Accept-Encoding": "identity",
        "Connection": "Keep-Alive",
        "Content-Type": "application/json",
        "Host": "127.0.0.1:12347",
        "User-Agent": "Wget/1.21.3",
    }


@pytest.fixture
def wget_server_post(is_bloated):
    if is_bloated:
        pytest.skip("No bloated test for wget")

    expected_data = (
        b'{"title": "Wget POST","body": "Wget POST example body","userId":1}'
    )

    server_thread = RunServerThread()
    server_thread.start()

    ensure_server_up()

    yield

    server_thread.join(timeout=1.0)
    assert not server_thread.is_alive()
    assert server_thread.request is not None
    assert server_thread.request[0] == "POST"
    assert server_thread.request[1] == "/foo/bar/post"
    assert server_thread.request[2] == {
        "Accept": "*/*",
        "Accept-Encoding": "identity",
        "Connection": "Keep-Alive",
        "Content-Length": str(len(expected_data)),
        "Content-Type": "application/json",
        "Host": "127.0.0.1:12347",
        "User-Agent": "Wget/1.21.3",
    }
    assert server_thread.request[3] == expected_data
