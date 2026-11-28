#!/usr/bin/env python3
"""Zero-dependency RFC 6455 verifier for examples/websocket-showcase."""

import argparse
import base64
import gzip
import hashlib
import json
import os
import socket
import struct
import sys
import threading
import urllib.error
import urllib.parse
import urllib.request

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def ws_accept(key: str) -> str:
    digest = hashlib.sha1((key + WS_GUID).encode("ascii")).digest()
    return base64.b64encode(digest).decode("ascii")


def recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise EOFError("socket closed unexpectedly")
        buf.extend(chunk)
    return bytes(buf)


def send_frame(sock: socket.socket, opcode: int, payload: bytes, masked: bool) -> None:
    header = bytearray([0x80 | (opcode & 0x0F)])
    mask_bit = 0x80 if masked else 0x00
    length = len(payload)
    if length < 126:
        header.append(mask_bit | length)
    elif length < 65536:
        header.append(mask_bit | 126)
        header.extend(struct.pack("!H", length))
    else:
        header.append(mask_bit | 127)
        header.extend(struct.pack("!Q", length))
    if masked:
        mask = os.urandom(4)
        header.extend(mask)
        body = bytes(b ^ mask[i & 3] for i, b in enumerate(payload))
        sock.sendall(bytes(header) + body)
    else:
        sock.sendall(bytes(header) + payload)


def recv_frame(sock: socket.socket):
    first_two = recv_exact(sock, 2)
    fin = bool(first_two[0] & 0x80)
    opcode = first_two[0] & 0x0F
    masked = bool(first_two[1] & 0x80)
    length = first_two[1] & 0x7F
    if length == 126:
        length = struct.unpack("!H", recv_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", recv_exact(sock, 8))[0]
    mask = recv_exact(sock, 4) if masked else b""
    payload = recv_exact(sock, length)
    if masked:
        payload = bytes(b ^ mask[i & 3] for i, b in enumerate(payload))
    return fin, opcode, payload


def recv_message(sock: socket.socket):
    """Receive one complete RFC 6455 message (handling fragmentation + Ping)."""
    msg_opcode = None
    parts = bytearray()
    while True:
        fin, opcode, payload = recv_frame(sock)
        if opcode == 0x9:  # Ping -> auto Pong
            send_frame(sock, 0xA, payload, masked=True)
            continue
        if opcode == 0xA:  # Pong
            continue
        if opcode == 0x8:  # Close
            code = struct.unpack("!H", payload[:2])[0] if len(payload) >= 2 else None
            reason = payload[2:].decode("utf-8", errors="replace") if len(payload) > 2 else ""
            return opcode, (code, reason)
        if opcode != 0x0:
            msg_opcode = opcode
            parts = bytearray(payload)
        else:
            parts.extend(payload)
        if fin:
            return msg_opcode, bytes(parts)


def ws_connect(host: str, port: int, path: str, protocols=None):
    sock = socket.create_connection((host, port), timeout=10)
    key = base64.b64encode(os.urandom(16)).decode("ascii")
    lines = [
        f"GET {path} HTTP/1.1",
        f"Host: {host}:{port}",
        "Upgrade: websocket",
        "Connection: Upgrade",
        f"Sec-WebSocket-Key: {key}",
        "Sec-WebSocket-Version: 13",
    ]
    if protocols:
        lines.append(f"Sec-WebSocket-Protocol: {', '.join(protocols)}")
    req = ("\r\n".join(lines) + "\r\n\r\n").encode("ascii")
    sock.sendall(req)

    raw = bytearray()
    while b"\r\n\r\n" not in raw:
        chunk = sock.recv(4096)
        if not chunk:
            raise EOFError("handshake closed before headers")
        raw.extend(chunk)
    head, leftover = bytes(raw).split(b"\r\n\r\n", 1)
    assert not leftover, "unexpected coalesced bytes"
    head_lines = head.decode("iso-8859-1").split("\r\n")
    status_line = head_lines[0]
    assert " 101 " in status_line, f"expected 101, got {status_line}"
    headers = {}
    for line in head_lines[1:]:
        if ":" in line:
            k, v = line.split(":", 1)
            headers[k.strip().lower()] = v.strip()
    assert headers.get("sec-websocket-accept") == ws_accept(key), headers
    return sock, headers


class LocalUpstreamPeer:
    """Loopback server on 127.0.0.1:18099 serving both RFC 6455 echo and gzip fallback."""

    def __init__(self, port=18099):
        self.port = port
        self._stop = threading.Event()
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("127.0.0.1", port))
        self._sock.listen(16)
        self._sock.settimeout(0.2)
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def start(self):
        self._thread.start()

    def stop(self):
        self._stop.set()
        self._sock.close()
        self._thread.join(timeout=2)

    def _serve(self):
        while not self._stop.is_set():
            try:
                conn, _ = self._sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            threading.Thread(target=self._handle_conn, args=(conn,), daemon=True).start()

    def _handle_conn(self, conn: socket.socket):
        conn.settimeout(5)
        try:
            raw = bytearray()
            while b"\r\n\r\n" not in raw:
                chunk = conn.recv(4096)
                if not chunk:
                    return
                raw.extend(chunk)
            head = bytes(raw).split(b"\r\n\r\n", 1)[0].decode("iso-8859-1")
            lines = head.split("\r\n")
            path = lines[0].split(" ")[1]
            headers = {}
            for line in lines[1:]:
                if ":" in line:
                    k, v = line.split(":", 1)
                    headers[k.strip().lower()] = v.strip()

            if path.startswith("/fallback"):
                plain = b"ordinary-http-fallback-decoded-from-gzip"
                gz = gzip.compress(plain)
                resp = (
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Encoding: gzip\r\n"
                    "X-Upgrade-Fallback: gzip-ok\r\n"
                    f"Content-Length: {len(gz)}\r\n"
                    "Connection: close\r\n\r\n"
                ).encode("ascii") + gz
                conn.sendall(resp)
                return

            # RFC 6455 WebSocket echo
            key = headers["sec-websocket-key"]
            proto = headers.get("sec-websocket-protocol", "").split(",")[0].strip()
            resp_lines = [
                "HTTP/1.1 101 Switching Protocols",
                "Upgrade: websocket",
                "Connection: Upgrade",
                f"Sec-WebSocket-Accept: {ws_accept(key)}",
            ]
            if proto:
                resp_lines.append(f"Sec-WebSocket-Protocol: {proto}")
            conn.sendall(("\r\n".join(resp_lines) + "\r\n\r\n").encode("ascii"))

            while True:
                _, opcode, payload = recv_frame(conn)
                if opcode == 0x8:
                    send_frame(conn, 0x8, payload, masked=False)
                    return
                if opcode == 0x9:
                    send_frame(conn, 0xA, payload, masked=False)
                elif opcode in (0x1, 0x2):
                    send_frame(conn, opcode, payload, masked=False)
        except Exception:
            pass
        finally:
            conn.close()


def http_get_json(url: str):
    try:
        with urllib.request.urlopen(url, timeout=10) as resp:
            return resp.status, json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as err:
        return err.code, json.loads(err.read().decode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--url",
        default="http://127.0.0.1:8080",
        help="Base URL of the running capsid-host (default: http://127.0.0.1:8080)",
    )
    args = parser.parse_args()
    parsed = urllib.parse.urlparse(args.url)
    host = parsed.hostname or "127.0.0.1"
    port = parsed.port or 8080
    base = f"http://{host}:{port}"

    peer = LocalUpstreamPeer(18099)
    peer.start()
    try:
        # 1. Index + API error cases
        status, idx = http_get_json(f"{base}/")
        assert status == 200 and len(idx["routes"]) == 8, idx
        status, err_cases = http_get_json(f"{base}/ws/error-cases")
        assert status == 200 and err_cases.get("ok") is True, err_cases
        print("[PASS] GET / and /ws/error-cases")

        # 2. Inbound echo: text, 32 KiB binary, Ping/Pong, and custom close 4001
        sock, _ = ws_connect(host, port, "/ws/echo")
        try:
            send_frame(sock, 0x1, b"info", masked=True)
            op, data = recv_message(sock)
            info = json.loads(data.decode("utf-8"))
            assert (
                op == 0x1
                and info["defaultBinaryType"] == "blob"
                and info["activeBinaryType"] == "arraybuffer"
            ), info

            send_frame(sock, 0x1, b"hello-ws", masked=True)
            op, data = recv_message(sock)
            assert op == 0x1 and data == b"echo:hello-ws", (op, data)

            # Host-handled Ping/Pong interleaved with 32 KiB binary payload
            send_frame(sock, 0x9, b"ping-token", masked=True)
            blob = bytes((i * 13 + 7) & 0xFF for i in range(32768))
            send_frame(sock, 0x2, blob, masked=True)
            op, data = recv_message(sock)
            assert op == 0x2 and data == blob, (op, len(data))

            send_frame(sock, 0x1, b"close:4001:custom-goodbye", masked=True)
            op, close_info = recv_message(sock)
            assert op == 0x8 and close_info == (4001, "custom-goodbye"), (op, close_info)
            send_frame(sock, 0x8, struct.pack("!H", 4001), masked=True)
        finally:
            sock.close()
        print("[PASS] WS /ws/echo (text, 32 KiB binary, ping/pong, custom close 4001)")

        # 3. Subprotocol negotiation
        sock, hdrs = ws_connect(
            host, port, "/ws/subprotocol", protocols=["unknown.v1", "capsid.chat.v1"]
        )
        try:
            assert hdrs.get("sec-websocket-protocol") == "capsid.chat.v1", hdrs
            send_frame(sock, 0x1, b"hi", masked=True)
            op, data = recv_message(sock)
            payload = json.loads(data.decode("utf-8"))
            assert (
                op == 0x1
                and payload["selected"] == "capsid.chat.v1"
                and payload["received"] == "hi"
            ), payload
            send_frame(sock, 0x8, struct.pack("!H", 1000), masked=True)
        finally:
            sock.close()
        print("[PASS] WS /ws/subprotocol (Sec-WebSocket-Protocol negotiation)")

        # 4. Graceful half-open drain (allowHalfOpen: true)
        sock, _ = ws_connect(host, port, "/ws/half-open")
        try:
            send_frame(sock, 0x1, b"first", masked=True)
            op, data = recv_message(sock)
            assert op == 0x1 and data == b"ack:1:first", (op, data)
            # Client sends clean Close first; server must still send final message before reciprocating Close
            send_frame(sock, 0x8, struct.pack("!H", 1000) + b"client-done", masked=True)
            op, final_msg = recv_message(sock)
            assert op == 0x1 and final_msg == b"final-after-close:1:1000:client-done", (
                op,
                final_msg,
            )
            op, close_info = recv_message(sock)
            assert op == 0x8 and close_info == (1000, "half-open-complete"), (op, close_info)
        finally:
            sock.close()
        print("[PASS] WS /ws/half-open (allowHalfOpen final message + reciprocal close)")

        # 5. Delayed server.accept() preserves early frames in order
        sock, _ = ws_connect(host, port, "/ws/delayed-accept")
        try:
            send_frame(sock, 0x1, b"early-1", masked=True)
            send_frame(sock, 0x1, b"early-2", masked=True)
            op1, d1 = recv_message(sock)
            op2, d2 = recv_message(sock)
            assert op1 == 0x1 and d1 == b"buffered:1:early-1", (op1, d1)
            assert op2 == 0x1 and d2 == b"buffered:2:early-2", (op2, d2)
            send_frame(sock, 0x8, struct.pack("!H", 1000), masked=True)
        finally:
            sock.close()
        print("[PASS] WS /ws/delayed-accept (early frames buffered before accept)")

        # 6. Default binaryType === "blob"
        sock, _ = ws_connect(host, port, "/ws/blob-mode")
        try:
            sample = b"capsid-blob-binary-frame-payload"
            send_frame(sock, 0x2, sample, masked=True)
            op, data = recv_message(sock)
            info = json.loads(data.decode("utf-8"))
            expected_sha = hashlib.sha256(sample).hexdigest()
            assert (
                op == 0x1
                and info["isBlob"] is True
                and info["binaryType"] == "blob"
                and info["size"] == len(sample)
                and info["sha256"] == expected_sha
            ), info
            send_frame(sock, 0x8, struct.pack("!H", 1000), masked=True)
        finally:
            sock.close()
        print("[PASS] WS /ws/blob-mode (default Blob binary frames + WebCrypto SHA-256)")

        # 7. Outbound WebSocket (allowed target + denied ungranted target)
        q = urllib.parse.urlencode(
            {"target": "ws://127.0.0.1:18099/echo", "protocol": "capsid.demo.v1"}
        )
        status, out = http_get_json(f"{base}/ws/outbound?{q}")
        assert (
            status == 200
            and out.get("ok") is True
            and out.get("protocol") == "capsid.demo.v1"
            and out["replies"][0] == {"kind": "text", "value": "hello-outbound-text"}
            and out["replies"][1] == {"kind": "binary", "bytes": [1, 2, 3, 4, 5]}
            and out["closeCode"] == 1000
        ), out

        denied_q = urllib.parse.urlencode({"target": "ws://127.0.0.1:19999/blocked"})
        denied_status, denied_out = http_get_json(f"{base}/ws/outbound?{denied_q}")
        assert denied_status == 502 and denied_out.get("ok") is False, (denied_status, denied_out)
        print("[PASS] GET /ws/outbound (allowed ws:// echo + fail-closed egress denial)")

        # 8. HTTP Upgrade non-101 fallback (with gzip decoding)
        fb_q = urllib.parse.urlencode({"target": "http://127.0.0.1:18099/fallback"})
        status, fb = http_get_json(f"{base}/ws/upgrade-fallback?{fb_q}")
        assert (
            status == 200
            and fb.get("ok") is True
            and fb.get("fallbackHeader") == "gzip-ok"
            and fb.get("body") == "ordinary-http-fallback-decoded-from-gzip"
        ), fb
        print("[PASS] GET /ws/upgrade-fallback (non-101 gzip Upgrade fallback)")

        print("ALL WEBSOCKET SHOWCASE CHECKS PASSED (8/8)")
    finally:
        peer.stop()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
