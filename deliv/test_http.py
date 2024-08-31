#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import sys


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        print("Got a GET request!")
        for header, value in self.headers.items():
            print(f"{header}: {value}")

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"Got a GET request; hello!\n")

    def do_POST(self):
        print("Got a POST request!")
        for header, value in self.headers.items():
            print(f"{header}: {value}")
        sys.stdout.write("Body: ")
        sys.stdout.flush()
        body = self.rfile.read(int(self.headers.get("Content-Length")))
        json.dump(json.loads(body), sys.stdout)
        print()

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"Got a POST request; hiya!\n")


HTTPServer(("", 8000), Handler).serve_forever()
