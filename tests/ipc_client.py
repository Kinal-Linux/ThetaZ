#!/usr/bin/env python3
"""IPC round-trip client for the ThetaZ θserver socket.

Sends a framed GetTickCount message and asserts on the response frame.
Exits non-zero on any failure; prints OK on success.
"""
import socket
import struct
import sys


def main() -> int:
    host, port = "127.0.0.1", 12345
    deadline = float(sys.argv[1]) if len(sys.argv) > 1 else 15.0

    import time
    end = time.monotonic() + deadline
    sock = None
    while time.monotonic() < end:
        try:
            sock = socket.create_connection((host, port), timeout=2)
            break
        except OSError:
            time.sleep(0.1)
    if sock is None:
        print("FAIL: could not connect to IPC server")
        return 1

    with sock:
        # GetTickCount message: type=9, length=0, request_id=42
        frame = struct.pack("<III", 9, 0, 42)
        sock.sendall(frame)

        header = b""
        while len(header) < 8:
            part = sock.recv(8 - len(header))
            if not part:
                print("FAIL: connection closed mid-response")
                return 1
            header += part

        status, length = struct.unpack("<II", header)
        body = b""
        while len(body) < length:
            part = sock.recv(length - len(body))
            if not part:
                print("FAIL: connection closed mid-body")
                return 1
            body += part

        if status != 0 or length != 4:
            print(f"FAIL: unexpected response status={status} length={length}")
            return 1

    print("IPC: GetTickCount round-trip OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
